#include "gg_plant.h"
#include <math.h>

static const float amp[4]={4.35f,.41f,.38f,1.36f};
static const float phase0[4]={0.0f,.6457718f,-.2094395f,.643681f};
static const float ratios[4][4]={{0,0,0,0},{.04f,.02f,.01f,0},{.72f,.48f,.26f,.12f},{.08f,.05f,.02f,.01f}};
static const float kappa[4]={1.05f,2.2f,1.8f,4.5f};
static const float tau[4]={.05f,.25f,.04f,.35f};
static uint32_t random_u32(gg_plant_t *p) { p->rng=p->rng*1664525u+1013904223u; return p->rng; }
static float uniform(gg_plant_t *p) { return (float)(random_u32(p)>>8)*(1.0f/16777216.0f); }
static float noise(gg_plant_t *p) {
 float u1=fmaxf(uniform(p),1e-7f),u2=uniform(p);
 return sqrtf(-2.0f*logf(u1))*cosf(6.28318530718f*u2);
}
static uint16_t quantize(float value,float full_scale) {
 float code=floorf(2048.0f+value/full_scale*2047.0f+0.5f);
 if(code<0) code=0;
 if(code>4095) code=4095;
 return (uint16_t)code;
}
void gg_plant_init(gg_plant_t *p,uint32_t seed) {
 unsigned a; p->phase=0; p->grid_hz=50; p->voltage_rms=230; p->fault=GG_FAULT_CLEAR;
 p->fault_magnitude=0; p->rng=seed?seed:1;
 for(a=0;a<4;a++){p->elapsed_s[a]=1000; p->inrush_peak_a[a]=0; p->appliance_on[a]=0;}
}
void gg_plant_set_appliance(gg_plant_t *p,unsigned id,int on) {
 if(id>=4)return;
 if(on&&!p->appliance_on[id]){p->elapsed_s[id]=0; p->inrush_peak_a[id]=0;}
 if(!on)p->elapsed_s[id]=1000;
 p->appliance_on[id]=(uint8_t)(on!=0);
}
void gg_plant_set_fault(gg_plant_t *p,gg_fault_scenario_t fault,float magnitude) { p->fault=fault;p->fault_magnitude=magnitude; }
gg_adc_sample_t gg_plant_step(gg_plant_t *p,float *volts,float *amps) {
 const float two_pi=6.28318530718f; float v_rms=p->voltage_rms,freq=p->grid_hz,current=0,theta,v; unsigned a,h;
 if(p->fault==GG_FAULT_SAG)v_rms=p->fault_magnitude>0?p->fault_magnitude:170;
 if(p->fault==GG_FAULT_SWELL)v_rms=p->fault_magnitude>0?p->fault_magnitude:275;
 if(p->fault==GG_FAULT_FREQ_LOW)freq=p->fault_magnitude>0?p->fault_magnitude:47;
 if(p->fault==GG_FAULT_FREQ_HIGH)freq=p->fault_magnitude>0?p->fault_magnitude:53;
 p->phase+=two_pi*freq/GG_FS_HZ; if(p->phase>=two_pi)p->phase-=two_pi;
 theta=p->phase;
 v=1.41421356237f*v_rms*(sinf(theta)+.015f*sinf(3*theta+.2f)+.010f*sinf(5*theta-.1f));
 for(a=0;a<4;a++) if(p->appliance_on[a]) {
  float env=1+(kappa[a]-1)*expf(-p->elapsed_s[a]/tau[a]); float sum=0;
  for(h=1;h<=9;h+=2){float r=h==1?1:ratios[a][h/2-1]; float ph=h==1?phase0[a]:phase0[a]; sum+=1.41421356237f*amp[a]*r*sinf((float)h*theta-ph);}
  current+=env*sum; if(fabsf(env*sum)>p->inrush_peak_a[a])p->inrush_peak_a[a]=fabsf(env*sum);
  p->elapsed_s[a]+=1.0f/GG_FS_HZ;
 }
 if(p->fault==GG_FAULT_SHORT)current+=p->fault_magnitude>0?p->fault_magnitude*sqrtf(2):36.0f*sinf(theta);
 if(p->fault==GG_FAULT_OVERLOAD)current+=(p->fault_magnitude>0?p->fault_magnitude:16.0f)*1.41421356237f*sinf(theta);
 v+=noise(p)*.08f; current+=noise(p)*.015f;
 if(p->fault==GG_FAULT_ADC_CLIP){v=450.0f;}
 if(volts)*volts=v;
 if(amps)*amps=current;
 {gg_adc_sample_t s={quantize(v,400.0f),quantize(current,40.0f)}; return s;}
}
