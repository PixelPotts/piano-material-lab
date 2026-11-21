#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "waveout.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RATE 44100
#define BLOCK 512
#define BUFFERS 6
#define PI 3.14159265358979323846
#include "parameters.h"
#include "synth.h"

static volatile LONG gate,strikes,quitting;
static HANDLE audioThread,audioEvent;
static HWAVEOUT output;
static CRITICAL_SECTION controlLock;
static Settings controls;
static Model displayModel;
static Engine audioEngine;
static LONG controlVersion=1;
static double outputVolume=.5;
static short scopeSamples[BLOCK];
static double meterPeak;
static unsigned long displayedOverloads,displayedNonfinite;
static int queueEmpty,audioTest,mouseDown,keyboardDown,autoDown;
static HWND mainWindow;
static char statusText[160]="Drag a knob vertically. Shift = fine. Double-click a value to type. Right-click = reset.";

static int write_wave(const char *filename,const Settings *s,int seconds,int scenario) {
 Engine *e=(Engine*)calloc(1,sizeof(Engine)); FILE *f; short pcm[BLOCK];
 unsigned int size=RATE*seconds*2,riff=size+36,fmt=16,rate=RATE,bytes=RATE*2;
 unsigned short format=1,channels=1,align=2,bits=16; int n,count,ok=1;
 if(!e)return 0;init_engine(e,s);f=fopen(filename,"wb");if(!f){free(e);return 0;}
 fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&fmt,4,1,f);
 fwrite(&format,2,1,f);fwrite(&channels,2,1,f);fwrite(&rate,4,1,f);fwrite(&bytes,4,1,f);
 fwrite(&align,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&size,4,1,f);
 for(n=0;n<RATE*seconds;n+=count){
  int held=scenario==1||n<RATE;LONG serial=scenario==3?0:1;
  count=BLOCK;if(n<RATE&&n+count>RATE)count=RATE-n;if(n+count>RATE*seconds)count=RATE*seconds-n;
  if(scenario==2){int boundary=(n/(RATE/8)+1)*(RATE/8);serial=1+n/(RATE/8);held=1;if(n+count>boundary)count=boundary-n;}
  render_engine(e,pcm,count,serial,held,.5);
  if(fwrite(pcm,2,count,f)!=(size_t)count){ok=0;break;}
 }
 if(e->nonfinite)ok=0;if(fclose(f))ok=0;free(e);return ok;
}
static int run_sweep(void) {
 Settings s; Engine *e=(Engine*)calloc(1,sizeof(Engine)); short pcm[BLOCK];
 int parameter,side,n,j; FILE *report=fopen("control-sweep.csv","w");
 if(!e||!report){free(e);if(report)fclose(report);return 1;}
 fprintf(report,"parameter,side,value,fundamental,peak,rms,nonfinite,overloads,signature\n");
 for(parameter=-1;parameter<PARAM_COUNT;++parameter)for(side=0;side<(parameter<0?1:2);++side){
  double peak=0,energy=0;unsigned long long hash=1469598103934665603ULL;
  default_settings(&s);if(parameter>=0)s.p[parameter]=side?parameters[parameter].max:parameters[parameter].min;
  init_engine(e,&s);
  for(n=0;n<RATE*2;n+=BLOCK){
   int count=BLOCK;if(n+count>RATE*2)count=RATE*2-n;
   render_engine(e,pcm,count,1,n<RATE,.5);
   for(j=0;j<count;++j){double a=pcm[j]/32768.0;energy+=a*a;if(fabs(a)>peak)peak=fabs(a);hash^=(unsigned short)pcm[j];hash*=1099511628211ULL;}
  }
  fprintf(report,"%s,%d,%.12g,%.8f,%.8f,%.8f,%lu,%lu,%llu\n",parameter<0?"default":parameters[parameter].id,side,parameter<0?0:s.p[parameter],e->active.model.fundamental,peak,sqrt(energy/(RATE*2)),e->nonfinite,e->overloads,hash);
 }
 fclose(report);
 /* Exercise coefficient transitions while the actual model is sounding. */
 default_settings(&s);init_engine(e,&s);
 for(parameter=0;parameter<PARAM_COUNT;++parameter){
  s.p[parameter]=parameters[parameter].max;change_engine(e,&s);
  for(n=0;n<4096;n+=BLOCK)render_engine(e,pcm,BLOCK,1,1,.5);
  default_settings(&s);change_engine(e,&s);
  for(n=0;n<4096;n+=BLOCK)render_engine(e,pcm,BLOCK,1,1,.5);
 }
 report=fopen("transition-test.txt","w");if(report){fprintf(report,"nonfinite=%lu\noverloads=%lu\n",e->nonfinite,e->overloads);fclose(report);}
 j=e->nonfinite?1:0;free(e);return j;
}
static DWORD WINAPI audio_loop(void *unused) {
 WAVEHDR headers[BUFFERS];short samples[BUFFERS][BLOCK];int i,submitted=0,starvations=0,restarted=0;
 DWORD started=GetTickCount();MMRESULT result=0;LONG version=controlVersion;
 LARGE_INTEGER freq,before,after;double maxRender=0;
 QueryPerformanceFrequency(&freq);memset(headers,0,sizeof(headers));
 SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
 result=waveOutPause(output);
 for(i=0;!result&&i<BUFFERS;++i){headers[i].lpData=(LPSTR)samples[i];headers[i].dwBufferLength=sizeof(samples[i]);result=waveOutPrepareHeader(output,&headers[i],sizeof(WAVEHDR));}
 while(!result&&!InterlockedCompareExchange(&quitting,0,0)){
  int queued=0;
  if(audioTest){DWORD elapsed=GetTickCount()-started;if(elapsed>=8000)break;InterlockedExchange(&gate,elapsed<5000);InterlockedExchange(&strikes,1);}
  for(i=0;i<BUFFERS;++i)if(headers[i].dwFlags&WHDR_INQUEUE)++queued;
  if(submitted&&!queued)++starvations;
  for(i=0;i<BUFFERS;++i)if(!(headers[i].dwFlags&WHDR_INQUEUE)){
   double volume;Settings next;LONG newVersion;double peak=0;int j;
   EnterCriticalSection(&controlLock);next=controls;newVersion=controlVersion;volume=outputVolume;LeaveCriticalSection(&controlLock);
   QueryPerformanceCounter(&before);
   if(newVersion!=version&&!audioEngine.transition){change_engine(&audioEngine,&next);version=newVersion;}
   render_engine(&audioEngine,samples[i],BLOCK,InterlockedCompareExchange(&strikes,0,0),InterlockedCompareExchange(&gate,0,0),volume);
   QueryPerformanceCounter(&after);{double ms=1000.0*(after.QuadPart-before.QuadPart)/freq.QuadPart;if(ms>maxRender)maxRender=ms;}
   for(j=0;j<BLOCK;++j){double a=fabs(samples[i][j]/32768.0);if(a>peak)peak=a;}
   EnterCriticalSection(&controlLock);memcpy(scopeSamples,samples[i],sizeof(scopeSamples));meterPeak=peak;queueEmpty=starvations;displayedOverloads=audioEngine.overloads;displayedNonfinite=audioEngine.nonfinite;LeaveCriticalSection(&controlLock);
   result=waveOutWrite(output,&headers[i],sizeof(WAVEHDR));++submitted;if(result)break;
  }
  if(!restarted&&!result){result=waveOutRestart(output);restarted=1;}
  if(!result)WaitForSingleObject(audioEvent,50);
 }
 waveOutReset(output);for(i=0;i<BUFFERS;++i)if(headers[i].dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(output,&headers[i],sizeof(WAVEHDR));
 {FILE *log=fopen(audioTest?"playback-test.txt":"playback-session.txt","w");if(log){fprintf(log,"blocks_submitted=%d\nqueue_empty_observations=%d\nmax_render_ms=%.4f\nblock_samples=%d\nbuffer_count=%d\nwaveout_result=%u\nnonfinite=%lu\n",submitted,starvations,maxRender,BLOCK,BUFFERS,result,audioEngine.nonfinite);fclose(log);}}
 if(result)PostMessage(mainWindow,WM_APP+1,result,0);
 return result;
}
static void update_key(HWND hwnd){int down=mouseDown||keyboardDown||autoDown;LONG previous=InterlockedExchange(&gate,down);if(down&&!previous)InterlockedIncrement(&strikes);InvalidateRect(hwnd,NULL,FALSE);}
static void publish_settings(void){build_model(&displayModel,&controls);EnterCriticalSection(&controlLock);++controlVersion;LeaveCriticalSection(&controlLock);}
#include "workbench-ui.h"

int WINAPI WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR args,int show){
 WNDCLASSA wc={0};HWND hwnd;MSG msg;RECT rect={0,0,1440,960};
 DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;
 WAVEFORMATEX format={0};char executable[MAX_PATH],*slash;
 GetModuleFileNameA(NULL,executable,sizeof(executable));slash=strrchr(executable,'\\');if(slash){*slash=0;SetCurrentDirectoryA(executable);}
 InitializeCriticalSection(&controlLock);default_settings(&controls);
 if(strstr(args,"--test")){int i;const char *names[]={"test-note.wav","test-held.wav","test-repeat.wav","test-silence.wav"};for(i=0;i<4;++i)if(!write_wave(names[i],&controls,8,i))return 1;return 0;}
 if(strstr(args,"--sweep"))return run_sweep();
 audioTest=strstr(args,"--audio-test")!=NULL;
 if(!audioTest)read_settings("current-settings.piano",&controls);
 build_model(&displayModel,&controls);init_engine(&audioEngine,&controls);
 {typedef BOOL (WINAPI *DpiFn)(void);DpiFn dpi=(DpiFn)GetProcAddress(GetModuleHandleA("user32.dll"),"SetProcessDPIAware");if(dpi)dpi();}
 format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;format.nSamplesPerSec=RATE;format.wBitsPerSample=16;format.nBlockAlign=2;format.nAvgBytesPerSec=RATE*2;
 audioEvent=CreateEvent(NULL,FALSE,FALSE,NULL);if(!audioEvent)return 1;
 if(waveOutOpen(&output,WAVE_MAPPER,&format,(DWORD_PTR)audioEvent,0,CALLBACK_EVENT)!=MMSYSERR_NOERROR){CloseHandle(audioEvent);MessageBoxA(NULL,"Could not open an audio device.","Piano Material Lab",MB_ICONERROR);return 1;}
 if(audioTest){DWORD result=audio_loop(NULL);waveOutClose(output);CloseHandle(audioEvent);return result?1:0;}
 wc.lpfnWndProc=window_proc;wc.hInstance=inst;wc.lpszClassName="PianoMaterialLab";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.style=CS_DBLCLKS;
 if(!RegisterClassA(&wc)){waveOutClose(output);CloseHandle(audioEvent);return 1;}
 AdjustWindowRect(&rect,style,FALSE);
 hwnd=CreateWindowA(wc.lpszClassName,"Piano Material Lab - 42 physical controls",style,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,NULL,NULL,inst,NULL);
 if(!hwnd){waveOutClose(output);CloseHandle(audioEvent);return 1;}
 mainWindow=hwnd;audioThread=CreateThread(NULL,0,audio_loop,NULL,0,NULL);
 if(!audioThread){DestroyWindow(hwnd);waveOutClose(output);CloseHandle(audioEvent);return 1;}
 ShowWindow(hwnd,show);UpdateWindow(hwnd);
 while(GetMessageA(&msg,NULL,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
 InterlockedExchange(&quitting,1);SetEvent(audioEvent);WaitForSingleObject(audioThread,INFINITE);
 CloseHandle(audioThread);waveOutClose(output);CloseHandle(audioEvent);DeleteCriticalSection(&controlLock);return 0;
}
