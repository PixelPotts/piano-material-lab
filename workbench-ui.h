/* Native GDI controls; each knob maps to one documented physical parameter. */
static HFONT fontTitle,fontSection,fontLabel,fontValue,fontSmall,fontBig;
static int selected=-1,dragging=-1,dragStartY,autoStrike;
static double dragStartValue;
static DWORD autoStart;
static HWND valueEdit;
static int editing=-1;
static WNDPROC originalEditProc;
static RECT keyRect={24,744,152,874};
static COLORREF accents[7]={0x008CB7EF,0x00B4D49A,0x0097C6DB,0x00E1BE87,0x00CCA5C8,0x00B7B7EF,0x00C6C291};
static RECT buttons[6]={{654,28,762,66},{774,28,882,66},{894,28,1002,66},{1014,28,1122,66},{1134,28,1270,66},{1282,28,1416,66}};
static const char *buttonLabels[6]={"TUNE C4","RESET ALL","SAVE PRESET","LOAD PRESET","EXPORT WAV","AUTO STRIKE"};
static void fill(HDC dc,int x,int y,int w,int h,COLORREF color){RECT r={x,y,x+w,y+h};HBRUSH b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);}
static void label(HDC dc,int x,int y,int w,int h,const char *text,HFONT font,COLORREF color,UINT align){RECT r={x,y,x+w,y+h};SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextA(dc,text,-1,&r,align|DT_NOPREFIX);}
static void line(HDC dc,int x1,int y1,int x2,int y2,COLORREF color,int width){HPEN pen=CreatePen(PS_SOLID,width,color);HGDIOBJ old=SelectObject(dc,pen);MoveToEx(dc,x1,y1,NULL);LineTo(dc,x2,y2);SelectObject(dc,old);DeleteObject(pen);}
static void knob_position(int i,int *cx,int *cy){int group=i/6,col=group%4,row=group/4;*cx=24+col*352+56+(i%3)*112;*cy=94+row*320+96+((i%6)/3)*130;}
static int hit_knob(int x,int y){int i;for(i=0;i<PARAM_COUNT;++i){int cx,cy;knob_position(i,&cx,&cy);if(x>=cx-50&&x<=cx+50&&y>=cy-34&&y<=cy+65)return i;}return -1;}
static void format_value(int i,double value,char *out,int size){
 double span=parameters[i].max-parameters[i].min;
 if(i==P_STENSION)snprintf(out,size,"%.2f",value);
 else if(span>=1000)snprintf(out,size,"%.0f",value);
 else if(span>=100)snprintf(out,size,"%.1f",value);
 else if(span>=10)snprintf(out,size,"%.2f",value);
 else if(span>=1)snprintf(out,size,"%.3f",value);
 else snprintf(out,size,"%.4f",value);
}
static void set_parameter(int i,double value){
 if(i<0||i>=PARAM_COUNT)return;
 EnterCriticalSection(&controlLock);controls.p[i]=bounded(value,parameters[i].min,parameters[i].max);LeaveCriticalSection(&controlLock);
 publish_settings();selected=i;
 snprintf(statusText,sizeof(statusText),"%s",parameters[i].help);
 InvalidateRect(mainWindow,NULL,FALSE);
}
static void draw_knob(HDC dc,int i){
 int cx,cy;char value[64];double t=normalized(i,controls.p[i]),angle=(135+270*t)*PI/180;
 COLORREF accent=accents[i/6];HPEN pen;HBRUSH brush;HGDIOBJ oldPen,oldBrush;
 int seg;
 knob_position(i,&cx,&cy);
 for(seg=0;seg<36;++seg){double a=(135+270*seg/36.0)*PI/180,b=(135+270*(seg+1)/36.0)*PI/180;line(dc,cx+(int)(32*cos(a)),cy+(int)(32*sin(a)),cx+(int)(32*cos(b)),cy+(int)(32*sin(b)),seg/36.0<=t?accent:RGB(49,54,61),3);}
 pen=CreatePen(PS_SOLID,1,i==selected?accent:RGB(73,78,88));brush=CreateSolidBrush(RGB(38,42,49));oldPen=SelectObject(dc,pen);oldBrush=SelectObject(dc,brush);
 Ellipse(dc,cx-25,cy-25,cx+25,cy+25);SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);DeleteObject(brush);
 line(dc,cx+(int)(11*cos(angle)),cy+(int)(11*sin(angle)),cx+(int)(22*cos(angle)),cy+(int)(22*sin(angle)),accent,3);
 label(dc,cx-55,cy+39,110,18,parameters[i].name,fontLabel,RGB(214,218,225),DT_CENTER|DT_SINGLELINE);
 format_value(i,controls.p[i],value,sizeof(value));label(dc,cx-54,cy+59,108,19,value,fontValue,accent,DT_CENTER|DT_SINGLELINE);
 label(dc,cx-54,cy-57,108,16,parameters[i].unit,fontSmall,RGB(135,144,158),DT_CENTER|DT_SINGLELINE);
}
static void paint_workbench(HDC dc){
 int i;char text[240];short samples[BLOCK];double peak;unsigned long limiting,nonfinite;int empty;
 fill(dc,0,0,1440,960,RGB(12,15,20));
 label(dc,24,24,600,35,"PIANO / MATERIAL LAB",fontTitle,RGB(237,239,243),DT_SINGLELINE);
 label(dc,25,63,620,20,"42 PHYSICAL CONTROLS   /   HAMMER  >  STRINGS  >  BODY  >  AIR",fontSmall,RGB(136,148,163),DT_SINGLELINE);
 for(i=0;i<6;++i){RECT r=buttons[i];fill(dc,r.left,r.top,r.right-r.left,r.bottom-r.top,i==5&&autoStrike?RGB(59,81,68):RGB(32,38,47));label(dc,r.left,r.top+11,r.right-r.left,20,buttonLabels[i],fontSmall,RGB(220,226,232),DT_CENTER|DT_SINGLELINE);}
 for(i=0;i<8;++i){int x=24+(i%4)*352,y=94+(i/4)*320;fill(dc,x,y,336,304,RGB(23,27,34));line(dc,x,y,x+336,y,i<7?accents[i]:RGB(102,113,130),2);label(dc,x+16,y+13,308,23,i<7?groupNames[i]:"08  MODEL READOUT",fontSection,RGB(205,213,224),DT_SINGLELINE);}
 for(i=0;i<PARAM_COUNT;++i)draw_knob(dc,i);
 {
  int x=1098,y=463;double cents=1200*log(displayModel.fundamental/261.625565)/log(2);
  snprintf(text,sizeof(text),"%.2f Hz",displayModel.fundamental);label(dc,x,y,300,44,text,fontBig,RGB(232,213,159),DT_SINGLELINE);
  snprintf(text,sizeof(text),"Center string  /  C4 %+.1f cents",cents);label(dc,x,y+47,306,20,text,fontSmall,RGB(151,163,177),DT_SINGLELINE);
  snprintf(text,sizeof(text),"WIRE MASS       %.3f g/m",displayModel.linearDensity*1000);label(dc,x,y+84,305,20,text,fontValue,RGB(196,205,218),DT_SINGLELINE);
  snprintf(text,sizeof(text),"INHARMONICITY   %.6f",displayModel.inharmonicity);label(dc,x,y+110,305,20,text,fontValue,RGB(196,205,218),DT_SINGLELINE);
  snprintf(text,sizeof(text),"BOARD           %.2f kg / %.1f Hz",displayModel.boardMass,displayModel.boardFirst);label(dc,x,y+136,305,20,text,fontValue,RGB(196,205,218),DT_SINGLELINE);
  snprintf(text,sizeof(text),"CASE / CAVITY   %.0f / %.0f Hz",displayModel.caseHz,displayModel.cavityHz);label(dc,x,y+162,305,20,text,fontValue,RGB(196,205,218),DT_SINGLELINE);
  snprintf(text,sizeof(text),"ROOM DECAY      %.2f s (estimate)",displayModel.rt60);label(dc,x,y+188,305,20,text,fontValue,RGB(196,205,218),DT_SINGLELINE);
  label(dc,x,y+220,302,25,"Reduced physical model. Not a measured piano.",fontSmall,RGB(127,139,154),DT_SINGLELINE);
 }
 fill(dc,keyRect.left,keyRect.top,keyRect.right-keyRect.left,keyRect.bottom-keyRect.top,gate?RGB(200,210,215):RGB(239,239,230));
 label(dc,24,766,128,40,"C",fontBig,RGB(30,36,43),DT_CENTER|DT_SINGLELINE);
 label(dc,24,830,128,28,"HOLD / SPACE",fontSmall,RGB(47,55,64),DT_CENTER|DT_SINGLELINE);
 EnterCriticalSection(&controlLock);memcpy(samples,scopeSamples,sizeof(samples));peak=meterPeak;limiting=displayedOverloads;nonfinite=displayedNonfinite;empty=queueEmpty;LeaveCriticalSection(&controlLock);
 fill(dc,176,744,888,130,RGB(17,21,27));
 label(dc,190,751,600,20,"LIVE PRESSURE SIGNAL  /  FULL-SCALE VIEW",fontSmall,RGB(120,134,151),DT_SINGLELINE);
 line(dc,190,815,1048,815,RGB(43,51,62),1);
 for(i=1;i<BLOCK;++i)line(dc,190+(i-1)*858/(BLOCK-1),815-(int)(samples[i-1]/32768.0*45),190+i*858/(BLOCK-1),815-(int)(samples[i]/32768.0*45),RGB(146,191,170),1);
 label(dc,1098,744,300,24,"MONITOR LEVEL  (NOT INSTRUMENT PHYSICS)",fontSmall,RGB(155,168,184),DT_SINGLELINE);
 fill(dc,1100,790,300,5,RGB(46,55,67));fill(dc,1100,790,(int)(300*outputVolume/1.5),5,RGB(190,204,216));fill(dc,1095+(int)(300*outputVolume/1.5),780,10,25,RGB(220,224,229));
 snprintf(text,sizeof(text),"%.1f dB     Peak %.1f dBFS",outputVolume>0?20*log10(outputVolume):-96,peak>0?20*log10(peak):-96);label(dc,1098,819,310,22,text,fontValue,RGB(189,201,214),DT_SINGLELINE);
 snprintf(text,sizeof(text),"Queue gaps: %d   Safety: %s",empty,nonfinite?"FAULT":(limiting?"LIMITED":"CLEAR"));label(dc,1098,849,310,22,text,fontSmall,empty||nonfinite?RGB(238,155,133):RGB(139,173,153),DT_SINGLELINE);
 line(dc,24,898,1416,898,RGB(49,56,66),1);
 if(selected>=0){snprintf(text,sizeof(text),"%s  |  %s",parameters[selected].name,parameters[selected].help);label(dc,24,910,1392,42,text,fontLabel,RGB(184,196,210),DT_WORDBREAK);}
 else label(dc,24,910,1392,42,statusText,fontLabel,RGB(184,196,210),DT_WORDBREAK);
}
static void finish_edit(int commit){
 HWND edit=valueEdit;int index=editing;char text[96],*end;double value;
 if(!edit)return;GetWindowTextA(edit,text,sizeof(text));valueEdit=NULL;editing=-1;
 if(commit){value=strtod(text,&end);while(*end==' ')++end;if(end!=text&&!*end&&value>=parameters[index].min&&value<=parameters[index].max)set_parameter(index,value);else{selected=-1;snprintf(statusText,sizeof(statusText),"Value not applied. Enter a number from %.6g to %.6g %s.",parameters[index].min,parameters[index].max,parameters[index].unit);}}
 DestroyWindow(edit);SetFocus(mainWindow);InvalidateRect(mainWindow,NULL,FALSE);
}
static LRESULT CALLBACK edit_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_KEYDOWN&&wp==VK_RETURN){finish_edit(1);return 0;}
 if(msg==WM_KEYDOWN&&wp==VK_ESCAPE){finish_edit(0);return 0;}
 return CallWindowProcA(originalEditProc,hwnd,msg,wp,lp);
}
static void start_edit(HWND hwnd,int i){int cx,cy;char value[64];if(valueEdit)finish_edit(0);knob_position(i,&cx,&cy);snprintf(value,sizeof(value),"%.12g",controls.p[i]);editing=i;valueEdit=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT",value,WS_CHILD|WS_VISIBLE|ES_CENTER|ES_AUTOHSCROLL,cx-53,cy+56,106,25,hwnd,(HMENU)100,GetModuleHandle(NULL),NULL);SendMessage(valueEdit,WM_SETFONT,(WPARAM)fontValue,TRUE);originalEditProc=(WNDPROC)SetWindowLongPtrA(valueEdit,GWLP_WNDPROC,(LONG_PTR)edit_proc);SetFocus(valueEdit);SendMessage(valueEdit,EM_SETSEL,0,-1);}
static void do_button(int i){
 Settings s;
 if(i==0){double tension=string_tension_c4(&controls);if(tension>=parameters[P_STENSION].min&&tension<=parameters[P_STENSION].max){set_parameter(P_STENSION,tension);snprintf(statusText,sizeof(statusText),"Tension solved for C4 at the current wire length, diameter, density and stiffness.");}else snprintf(statusText,sizeof(statusText),"C4 requires %.1f N, outside this instrument's tension range. Change wire dimensions first.",tension);}
 if(i==1){default_settings(&s);EnterCriticalSection(&controlLock);controls=s;LeaveCriticalSection(&controlLock);publish_settings();snprintf(statusText,sizeof(statusText),"All 42 physical properties restored to the reference instrument.");}
 if(i==2)snprintf(statusText,sizeof(statusText),write_settings("user-preset.piano",&controls)?"Preset saved to user-preset.piano.":"Could not save the preset.");
 if(i==3){if(read_settings("user-preset.piano",&s)){EnterCriticalSection(&controlLock);controls=s;LeaveCriticalSection(&controlLock);publish_settings();snprintf(statusText,sizeof(statusText),"Saved preset loaded.");}else snprintf(statusText,sizeof(statusText),"No valid saved preset. Use SAVE PRESET first.");}
 if(i==4){snprintf(statusText,sizeof(statusText),write_wave("test-note.wav",&controls,6,0)?"Current instrument exported to test-note.wav (one-second hold, then release).":"Could not export the WAV.");}
 if(i==5){autoStrike=!autoStrike;autoStart=GetTickCount();if(!autoStrike){autoDown=0;update_key(mainWindow);}snprintf(statusText,sizeof(statusText),autoStrike?"Automatic strike every 2.5 seconds. Adjust knobs to compare the response.":"Automatic striking stopped.");}
 selected=-1;InvalidateRect(mainWindow,NULL,FALSE);
}
static LRESULT CALLBACK window_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:
  mainWindow=hwnd;fontTitle=CreateFontA(-28,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");fontSection=CreateFontA(-15,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");fontLabel=CreateFontA(-14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");fontValue=CreateFontA(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Consolas");fontSmall=CreateFontA(-12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");fontBig=CreateFontA(-34,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");SetTimer(hwnd,1,33,NULL);return 0;
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps),mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,1440,960);HGDIOBJ old=SelectObject(mem,bmp);paint_workbench(mem);BitBlt(dc,0,0,1440,960,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(hwnd,&ps);return 0;}
 case WM_TIMER:
  if(autoStrike){int down=(GetTickCount()-autoStart)%2500<1000;if(down!=autoDown){autoDown=down;update_key(hwnd);}}
  InvalidateRect(hwnd,NULL,FALSE);return 0;
 case WM_LBUTTONDOWN:{int x=(short)LOWORD(lp),y=(short)HIWORD(lp),i;POINT pt={x,y};
  if(valueEdit)finish_edit(1);SetFocus(hwnd);
  for(i=0;i<6;++i)if(PtInRect(&buttons[i],pt)){do_button(i);return 0;}
  if(PtInRect(&keyRect,pt)){mouseDown=1;SetCapture(hwnd);update_key(hwnd);return 0;}
  if(x>=1090&&x<=1410&&y>=772&&y<=810){dragging=-2;SetCapture(hwnd);EnterCriticalSection(&controlLock);outputVolume=bounded((x-1100)/300.0*1.5,0,1.5);LeaveCriticalSection(&controlLock);return 0;}
  i=hit_knob(x,y);if(i>=0){selected=dragging=i;dragStartY=y;dragStartValue=normalized(i,controls.p[i]);SetCapture(hwnd);}return 0;}
 case WM_MOUSEMOVE:{int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
  if(dragging>=0){double step=(dragStartY-y)/180.0;if(GetKeyState(VK_SHIFT)<0)step*=.1;set_parameter(dragging,denormalized(dragging,dragStartValue+step));}
  else if(dragging==-2){EnterCriticalSection(&controlLock);outputVolume=bounded((x-1100)/300.0*1.5,0,1.5);LeaveCriticalSection(&controlLock);}
  else {int i=hit_knob(x,y);if(i>=0)selected=i;}return 0;}
 case WM_LBUTTONUP:dragging=-1;mouseDown=0;ReleaseCapture();update_key(hwnd);return 0;
 case WM_CAPTURECHANGED:dragging=-1;mouseDown=0;update_key(hwnd);return 0;
 case WM_LBUTTONDBLCLK:{int i=hit_knob((short)LOWORD(lp),(short)HIWORD(lp));if(i>=0){dragging=-1;ReleaseCapture();start_edit(hwnd,i);}return 0;}
 case WM_RBUTTONDOWN:{int i=hit_knob((short)LOWORD(lp),(short)HIWORD(lp));if(i>=0){Settings s;default_settings(&s);set_parameter(i,s.p[i]);}return 0;}
 case WM_MOUSEWHEEL:{POINT pt={(short)LOWORD(lp),(short)HIWORD(lp)};int i;ScreenToClient(hwnd,&pt);i=hit_knob(pt.x,pt.y);if(i>=0)set_parameter(i,denormalized(i,normalized(i,controls.p[i])+(short)HIWORD(wp)/120.0*(GetKeyState(VK_SHIFT)<0?.001:.01)));return 0;}
 case WM_KEYDOWN:
  if(wp==VK_SPACE||wp=='C'){keyboardDown|=wp==VK_SPACE?1:2;update_key(hwnd);}
  else if(wp=='S'&&!(lp&(1<<30)))set_parameter(P_PEDAL,controls.p[P_PEDAL]<50?100:0);
  else if(wp==VK_ESCAPE){autoStrike=autoDown=keyboardDown=mouseDown=0;set_parameter(P_PEDAL,0);update_key(hwnd);}
  else if(wp==VK_TAB){selected=(selected+1)%PARAM_COUNT;InvalidateRect(hwnd,NULL,FALSE);}
  else if(wp==VK_RETURN&&selected>=0)start_edit(hwnd,selected);
  else if((wp==VK_UP||wp==VK_DOWN||wp==VK_LEFT||wp==VK_RIGHT)&&selected>=0)set_parameter(selected,denormalized(selected,normalized(selected,controls.p[selected])+(wp==VK_UP||wp==VK_RIGHT?1:-1)*(GetKeyState(VK_SHIFT)<0?.001:.01)));
  return 0;
 case WM_KEYUP:if(wp==VK_SPACE||wp=='C'){keyboardDown&=~(wp==VK_SPACE?1:2);update_key(hwnd);}return 0;
 case WM_KILLFOCUS:keyboardDown=mouseDown=0;ReleaseCapture();update_key(hwnd);return 0;
 case WM_APP+1:MessageBoxA(hwnd,"Audio playback failed. Check your output device and reopen the app.","Piano Material Lab",MB_ICONERROR);return 0;
 case WM_APP+2:if(valueEdit&&GetFocus()!=valueEdit)finish_edit(1);return 0;
 case WM_CLOSE:if(valueEdit)finish_edit(1);write_settings("current-settings.piano",&controls);DestroyWindow(hwnd);return 0;
 case WM_DESTROY:KillTimer(hwnd,1);InterlockedExchange(&quitting,1);SetEvent(audioEvent);DeleteObject(fontTitle);DeleteObject(fontSection);DeleteObject(fontLabel);DeleteObject(fontValue);DeleteObject(fontSmall);DeleteObject(fontBig);PostQuitMessage(0);return 0;
 }
 return DefWindowProcA(hwnd,msg,wp,lp);
}
