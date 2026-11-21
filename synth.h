/* Reduced physical piano model. See MODEL.txt for equations and limitations. */
#define STRING_PARTIALS 32
#define STRING_MODES (3*STRING_PARTIALS)
#define BOARD_MODES 64
#define DELAY_SIZE 8192
#define CONTACT_OS 8

typedef struct { double q,v; } Motion;
typedef struct { double c,d,sv,sw,iq,iv; } Step;
typedef struct {
 double omega,mass,shape,bridge,loss,damper;
 Step normal,contact;
} StringMode;
typedef struct { double omega,mass,shape,radiation,gamma; Step step; } BoardMode;
typedef struct {
 Settings settings;
 StringMode strings[STRING_MODES]; BoardMode board[BOARD_MODES];
 Step bridgeStep,caseStep,cavityStep;
 double fundamental,inharmonicity,linearDensity,boardMass,boardFirst,caseHz,cavityHz,rt60;
 double bridgeK,bridgeC,hammerMass,hammerVelocity,feltForce,feltExponent;
 double damperRate,damperLift,micGain,roomGain,airCoeff;
 int directDelay,delays[6]; double reflection[6];
} Model;
typedef struct {
 Motion strings[STRING_MODES],board[BOARD_MODES],bridge,panel,cavity;
 double direct[DELAY_SIZE],room[DELAY_SIZE],roomAir,dcIn,dcOut,damper;
 double hammerQ,hammerV,contactTime,peakForce;
 unsigned int pos; LONG strike;
 int touching,hadContact;
} Voice;
typedef struct { Model model; Voice voice; } Instrument;
typedef struct { Instrument active,old; int transition; double gain; unsigned long overloads,nonfinite; } Engine;
static double sinc(double x) { return fabs(x)<1e-7?1:sin(x)/x; }
static Step oscillator(double w,double gamma,double mass,double dt) {
 Step a; double r=exp(-gamma*dt),c,s,wd;
 if(gamma<w){wd=sqrt(w*w-gamma*gamma);c=cos(wd*dt);s=sin(wd*dt)/wd;}
 else if(gamma>w){wd=sqrt(gamma*gamma-w*w);c=cosh(wd*dt);s=sinh(wd*dt)/wd;}
 else {c=1;s=dt;}
 a.c=r*(c+gamma*s);a.d=r*(c-gamma*s);a.sv=r*s;a.sw=-w*w*r*s;
 a.iq=(1-a.c)/(mass*w*w);a.iv=-a.sw/(mass*w*w);
 return a;
}
static void move_oscillator(Motion *v,const Step *a,double force) {
 double q=a->c*v->q+a->sv*v->v+a->iq*force;
 v->v=a->sw*v->q+a->d*v->v+a->iv*force; v->q=q;
}
static void build_model(Model *m,const Settings *s) {
 const double *p=s->p; int i,j,k,n,string;
 double L=p[P_SLEN],d=p[P_SDIAM]*.001,area=PI*d*d/4,I=PI*d*d*d*d/64;
 double mu=p[P_SRHO]*area,T=p[P_STENSION],EI=p[P_SE]*1e9*I;
 double h=p[P_PTHICK]*.001,a=p[P_PLEN],b=p[P_PWIDTH],rho=p[P_PRHO];
 double Ex=p[P_PE]*1e9,Ey=Ex*p[P_PRATIO],nu=.3,den=1-nu*nu*p[P_PRATIO];
 double Dxx=Ex*h*h*h/(12*den),Dyy=Ey*h*h*h/(12*den);
 double Dxy=nu*Ey*h*h*h/(12*den),Dshear=sqrt(Ex*Ey)/5*h*h*h/12;
 double boardMass=rho*h*a*b,bridgeWidth=p[P_BWIDTH]*.001;
 double aperture=p[P_APERTURE]*.0001,neck=.06+1.7*sqrt(aperture/PI);
 double cavityW=343*sqrt(aperture/(p[P_CVOL]*neck));
 double roomArea=2*(p[P_RLEN]*p[P_RWID]+p[P_RLEN]*p[P_RHEIGHT]+p[P_RWID]*p[P_RHEIGHT]);
 memset(m,0,sizeof(*m)); m->settings=*s;
 m->linearDensity=mu; m->boardMass=boardMass;
 m->inharmonicity=PI*PI*EI/(T*L*L);
 m->fundamental=sqrt(T/mu)/(2*L)*sqrt(1+m->inharmonicity);
 m->hammerMass=p[P_HMASS]*.001; m->hammerVelocity=p[P_HVEL];
 m->feltForce=p[P_HFELT]; m->feltExponent=p[P_HEXP];
 m->bridgeK=p[P_BK]*1e6; m->bridgeC=p[P_BC];
 m->bridgeStep=oscillator(sqrt(m->bridgeK/(p[P_BMASS]*.001)),p[P_BC]/(2*p[P_BMASS]*.001),p[P_BMASS]*.001,1.0/RATE);
 m->caseHz=sqrt(p[P_CK]*1e6/p[P_CMASS])/(2*PI);
 m->caseStep=oscillator(2*PI*m->caseHz,PI*m->caseHz*p[P_CLOSS],p[P_CMASS],1.0/RATE);
 m->cavityHz=cavityW/(2*PI);
 /* Acoustic mode is expressed as a normalized pressure-transfer oscillator. */
 m->cavityStep=oscillator(cavityW,cavityW*(.07+.1*sqrt(aperture)),1,1.0/RATE);
 m->damperRate=1-exp(-1.0/(RATE*p[P_DTIME]*.001));
 m->damperLift=p[P_PEDAL]/100;
 m->micGain=1.204/(4*PI*p[P_DIST]);
 m->rt60=.161*p[P_RLEN]*p[P_RWID]*p[P_RHEIGHT]/(-roomArea*log(1-p[P_ABSORB]));
 m->directDelay=(int)(p[P_DIST]/343*RATE);
 m->airCoeff=1-exp(-2*PI*3500/RATE);
 for(i=0;i<6;++i) {
  double distance;
  if(i<2) distance=p[P_RLEN]+(i?p[P_DIST]:-p[P_DIST]);
  else if(i<4) distance=sqrt(p[P_RWID]*p[P_RWID]+p[P_DIST]*p[P_DIST]);
  else {double z=i==4?2.4:2*(p[P_RHEIGHT]-1.2); distance=sqrt(z*z+p[P_DIST]*p[P_DIST]);}
  m->delays[i]=(int)(distance/343*RATE);
  m->reflection[i]=sqrt(1-p[P_ABSORB])*p[P_DIST]/distance;
 }
 m->roomGain=pow(.001,(p[P_RLEN]+p[P_RWID]+p[P_RHEIGHT])/3/343/m->rt60);
 k=0;
 for(i=1;i<=8;++i)for(j=1;j<=8;++j) {
  BoardMode *v=&m->board[k++]; double kx=i*PI/a,ky=j*PI/b;
  double w=sqrt((Dxx*pow(kx,4)+2*(Dxy+2*Dshear)*kx*kx*ky*ky+Dyy*pow(ky,4))/(rho*h));
  v->omega=w; v->mass=boardMass/4;
  v->shape=sin(i*PI*.37)*sin(j*PI*.43)*sinc(kx*bridgeWidth/2);
  v->gamma=p[P_PLOSS]*w/2;
  /* Modal displaced volume for a simply supported rectangular plate. Even
     modes cancel on-axis; oblique/local radiation is approximated by a small
     off-axis term. This is not a calibrated acoustic radiation solver. */
  v->radiation=a*b*((i%2&&j%2)?4.0/(PI*PI*i*j):.025/(i*j));
  v->radiation*=cos(p[P_ANGLE]*PI/180)*sinc(w/343*.2*sin(p[P_ANGLE]*PI/180));
  if(w>2*PI*15000) v->shape=v->radiation=0;
  v->step=oscillator(w,v->gamma,v->mass,1.0/RATE);
 }
 m->boardFirst=m->board[0].omega/(2*PI);
 k=0;
 for(string=0;string<3;++string)for(n=1;n<=STRING_PARTIALS;++n) {
  StringMode *v=&m->strings[k++];
  double cents=string==0?p[P_DETL]:(string==2?p[P_DETR]:0);
  double tension=(T+EI*PI*PI/(L*L))*pow(2,cents/600)-EI*PI*PI/(L*L);
  double kn=n*PI/L,w=sqrt((tension*kn*kn+EI*pow(kn,4))/mu);
  double yr=0,yi=0,zr,zi,reY,contactMean;
  v->omega=w; v->mass=mu*L/2;
  v->shape=sin(n*PI*p[P_HPOS]/100)*sinc(kn*p[P_HWIDTH]*.001/2);
  v->bridge=tension*kn*(n%2?1:-1);
  /* Estimate passive loading from plate driving-point mobility and the
     series bridge impedance. Feed-forward radiation below is approximate. */
  for(i=0;i<BOARD_MODES;++i) {
   BoardMode *bmode=&m->board[i]; double A=bmode->omega*bmode->omega-w*w,B=2*bmode->gamma*w;
   double factor=bmode->shape*bmode->shape/bmode->mass/(A*A+B*B);
   yr+=factor*w*B; yi+=factor*w*A;
  }
  if(yr*yr+yi*yi>1e-24) { zr=yr/(yr*yr+yi*yi)+p[P_BC]; zi=-yi/(yr*yr+yi*yi)+w*p[P_BMASS]*.001-m->bridgeK/w; reY=zr/(zr*zr+zi*zi); }
  else reY=0;
  v->loss=p[P_SLOSS]*(1+.045*(n-1)*(n-1))+bounded(v->bridge*v->bridge*reY/(2*v->mass*w*w),0,20);
  contactMean=.5-.5*cos(2*n*PI*p[P_DPOS]/100)*sinc(kn*p[P_DWIDTH]*.001);
  v->damper=p[P_DAMP]*contactMean/(2*v->mass)*sqrt(p[P_DCOMP]/.5);
  if(w>2*PI*16000) {v->shape=v->bridge=0;v->damper=0;}
  v->normal=oscillator(w,v->loss,v->mass,1.0/RATE);
  v->contact=oscillator(w,v->loss,v->mass,1.0/(RATE*CONTACT_OS));
 }
}
static void strike_voice(Instrument *instrument,LONG serial) {
 Voice *v=&instrument->voice; Model *m=&instrument->model; int i;
 v->strike=serial; v->hammerQ=0;
 for(i=0;i<STRING_MODES;++i)v->hammerQ+=v->strings[i].q*m->strings[i].shape/3;
 v->hammerV=m->hammerVelocity; v->contactTime=0;v->touching=1;v->hadContact=0;v->peakForce=0;v->damper=0;
}
static double instrument_sample(Instrument *instrument,LONG serial,int held) {
 Model *m=&instrument->model; Voice *v=&instrument->voice;
 double bridgeForce=0,boardPressure=0,casePressure,cavityPressure,dry,wet=0;
 int i,sub,count,pos=v->pos&(DELAY_SIZE-1);
 if(serial!=v->strike)strike_voice(instrument,serial);
 v->damper+=(((held?0:1)*(1-m->damperLift))-v->damper)*m->damperRate;
 count=v->touching?CONTACT_OS:1;
 for(sub=0;sub<count;++sub) {
  double force=0;
  if(v->touching) {
   double displacement=0,compression;
   for(i=0;i<STRING_MODES;++i)displacement+=v->strings[i].q*m->strings[i].shape/3;
   compression=v->hammerQ-displacement;
   if(compression>0) { force=m->feltForce*pow(compression/.001,m->feltExponent);v->hadContact=1; }
   if(force>v->peakForce)v->peakForce=force;
   v->hammerV-=force/m->hammerMass/(RATE*CONTACT_OS);
   v->hammerQ+=v->hammerV/(RATE*CONTACT_OS);
   v->contactTime+=1.0/(RATE*CONTACT_OS);
   if((v->hadContact&&compression<=0&&v->hammerV<0)||v->contactTime>.03)v->touching=0;
  }
  for(i=0;i<STRING_MODES;++i) {
   StringMode *s=&m->strings[i];
   move_oscillator(&v->strings[i],count==1?&s->normal:&s->contact,force*s->shape/3);
  }
 }
 for(i=0;i<STRING_MODES;++i) {
  StringMode *s=&m->strings[i];
  double damping=1.0/(1+s->damper*v->damper/RATE);
  v->strings[i].q*=damping;v->strings[i].v*=damping;
  bridgeForce+=s->bridge*v->strings[i].q;
 }
 move_oscillator(&v->bridge,&m->bridgeStep,bridgeForce);
 bridgeForce=m->bridgeK*v->bridge.q+m->bridgeC*v->bridge.v;
 for(i=0;i<BOARD_MODES;++i) {
  double previous=v->board[i].v;
  move_oscillator(&v->board[i],&m->board[i].step,bridgeForce*m->board[i].shape);
  boardPressure+=m->board[i].radiation*(v->board[i].v-previous)*RATE;
 }
 {
  double previous=v->panel.v;
  move_oscillator(&v->panel,&m->caseStep,bridgeForce*.16);
  casePressure=.12*(v->panel.v-previous)*RATE;
 }
 move_oscillator(&v->cavity,&m->cavityStep,boardPressure*pow(2*PI*m->cavityHz,2));
 cavityPressure=.14*v->cavity.q;
 dry=m->micGain*(boardPressure+casePressure+cavityPressure);
 v->direct[pos]=dry;
 for(i=0;i<6;++i)wet+=m->reflection[i]*v->direct[(pos-m->delays[i])&(DELAY_SIZE-1)];
 {
  double feedback=0;
  for(i=0;i<6;++i)feedback+=v->room[(pos-m->delays[i])&(DELAY_SIZE-1)]/6;
  v->roomAir+=m->airCoeff*(feedback-v->roomAir);
  v->room[pos]=dry*.15+m->roomGain*v->roomAir;
 }
 dry=v->direct[(pos-m->directDelay)&(DELAY_SIZE-1)]+wet+.22*v->roomAir;
 v->dcOut=dry-v->dcIn+.998*v->dcOut;v->dcIn=dry;
 ++v->pos;
 return v->dcOut;
}
static void init_engine(Engine *e,const Settings *s) {memset(e,0,sizeof(*e));build_model(&e->active.model,s);e->gain=1;}
static void change_engine(Engine *e,const Settings *s) {
 e->old=e->active; build_model(&e->active.model,s);e->transition=2048;
}
static void render_engine(Engine *e,short *out,int count,LONG serial,int held,double volume) {
 int i;
 for(i=0;i<count;++i) {
  double x=instrument_sample(&e->active,serial,held),a;
  if(e->transition) {
   double old=instrument_sample(&e->old,serial,held),mix=1-e->transition/2048.0;
   x=old*(1-mix)+x*mix; --e->transition;
  }
  e->gain+=(volume-e->gain)*.001;
  x*=e->gain;
  if(!(x>=-1e12&&x<=1e12)) { ++e->nonfinite;memset(&e->active.voice,0,sizeof(Voice));memset(&e->old.voice,0,sizeof(Voice));e->transition=0;x=0; }
  /* Transparent below -3 dBFS; soft safety ceiling for intentionally extreme
     material combinations. Output level is separate from the physical model. */
  a=fabs(x);
  if(a>.7) {++e->overloads;x=(x<0?-1:1)*(.7+.28*(1-exp(-(a-.7)/.28)));}
  out[i]=(short)(bounded(x,-.98,.98)*32767);
 }
}
