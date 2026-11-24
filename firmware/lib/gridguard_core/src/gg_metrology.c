#include "gg_metrology.h"
#include <math.h>
#include <string.h>

static const unsigned harmonics[6]={1,3,5,7,9,11};
static double global_index;
static double last_crossing=-1;
static float filtered_hz=50;
static float energy_wh;
void gg_metrology_init(void){global_index=0;last_crossing=-1;filtered_hz=50;energy_wh=0;}
static void goertzel(const float *x,unsigned k,float *re,float *im,float *rms) {
 float w=6.28318530718f*(float)k/64.0f,c=2*cosf(w),s1=0,s2=0,s;
 unsigned n; for(n=0;n<64;n++){s=x[n]+c*s1-s2;s2=s1;s1=s;}
 *re=s1-cosf(w)*s2; *im=sinf(w)*s2;
 *rms=.70710678118f*sqrtf((*re)*(*re)+(*im)*(*im))*2.0f/64.0f;
}
int gg_metrology_compute(const gg_adc_sample_t samples[64],gg_metrics_t *o) {
 float v[64],i[64],mv=0,mi=0,ssv=0,ssi=0,p=0; unsigned n,k; float vr1=0,vi1=0,ir1=0,ii1=0;
 if(!samples||!o)return -1;
 memset(o,0,sizeof(*o));
 for(n=0;n<64;n++){v[n]=((int)samples[n].voltage_code-2048)*(400.0f/2047.0f);i[n]=((int)samples[n].current_code-2048)*(40.0f/2047.0f);mv+=v[n];mi+=i[n];}
 mv/=64;mi/=64;
 for(n=0;n<64;n++){v[n]-=mv;i[n]-=mi;ssv+=v[n]*v[n];ssi+=i[n]*i[n];p+=v[n]*i[n];}
 o->v_rms=sqrtf(ssv/64);o->i_rms=sqrtf(ssi/64);o->p_w=p/64;o->s_va=o->v_rms*o->i_rms;o->pf=o->p_w/fmaxf(o->s_va,1e-6f);
 for(k=0;k<6;k++){
  float a,b,c,d;goertzel(v,harmonics[k],&a,&b,&o->harm_v_rms[k]);goertzel(i,harmonics[k],&c,&d,&o->harm_i_rms[k]);
  if(k==0){vr1=a*(1.41421356237f/64);vi1=b*(1.41421356237f/64);ir1=c*(1.41421356237f/64);ii1=d*(1.41421356237f/64);}
 }
 o->fundamental_v_re=vr1;o->fundamental_v_im=vi1;o->fundamental_i_re=ir1;o->fundamental_i_im=ii1;
 o->q1_var=vi1*ir1-vr1*ii1;
 o->distortion_va=sqrtf(fmaxf(0,o->s_va*o->s_va-o->p_w*o->p_w-o->q1_var*o->q1_var));
 {float ih=0,vh=0;for(k=1;k<6;k++){ih+=o->harm_i_rms[k]*o->harm_i_rms[k];vh+=o->harm_v_rms[k]*o->harm_v_rms[k];}
 o->thd_i_pct=100*sqrtf(ih)/fmaxf(o->harm_i_rms[0],1e-6f);o->thd_v_pct=100*sqrtf(vh)/fmaxf(o->harm_v_rms[0],1e-6f);}
 for(n=1;n<64;n++) if(v[n-1]<-15 && v[n]>=15){float frac=(float)(-v[n-1])/(v[n]-v[n-1]);double crossing=global_index+(double)n-1+frac;
  if(last_crossing>=0){float f=(float)(GG_FS_HZ/(crossing-last_crossing));if(f>40&&f<70)filtered_hz=.75f*filtered_hz+.25f*f;}last_crossing=crossing;}
 global_index+=64;o->freq_hz=filtered_hz;energy_wh+=o->p_w*.02f/3600.0f;o->energy_wh=energy_wh;
 return 0;
}
