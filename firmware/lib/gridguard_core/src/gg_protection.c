#include "gg_protection.h"
#include <math.h>
#include <string.h>
void gg_protection_init(gg_protection_t *p){memset(p,0,sizeof(*p));}
float gg_protection_operating_time(float m,gg_curve_t curve,float tms){
 static const float k[]={.14f,13.5f,80.0f,120.0f},a[]={.02f,1.0f,2.0f,1.0f};
 if(curve>GG_CURVE_LTI||m<=1.05f)return INFINITY;
 return tms*k[curve]/(powf(m,a[curve])-1.0f);
}
gg_fault_code_t gg_protection_idmt_step(gg_protection_t *p,float current){
 float mult,top;if(!p)return FAULT_NONE;mult=current/8.0f;top=gg_protection_operating_time(mult,GG_CURVE_EI,.15f);
 if(mult>1.05f&&isfinite(top))p->thermal=fminf(1,p->thermal+.02f/top);else p->thermal=fmaxf(0,p->thermal-.02f/10.0f);
 if(p->thermal>=.99999f){p->thermal=1;return FAULT_IDMT_OVERLOAD;}return FAULT_NONE;
}
static unsigned persistence(unsigned n,int bad){return bad?n+1:0;}
gg_fault_code_t gg_protection_step(gg_protection_t *p,const gg_metrics_t *m,int healthy){
 if(!p||!m)return FAULT_NONE;
 if(m->i_rms>=25)return FAULT_SHORT_CIRCUIT;
 p->overcurrent_count=persistence(p->overcurrent_count,m->i_rms>=12);
 if(p->overcurrent_count>=3)return FAULT_OVERCURRENT;
 if(gg_protection_idmt_step(p,m->i_rms)==FAULT_IDMT_OVERLOAD)return FAULT_IDMT_OVERLOAD;
 p->undervoltage_count=persistence(p->undervoltage_count,m->v_rms<195);
 p->overvoltage_count=persistence(p->overvoltage_count,m->v_rms>255);
 p->freq_count=persistence(p->freq_count,m->freq_hz<48.5f||m->freq_hz>51.5f);
 p->thd_count=persistence(p->thd_count,m->thd_i_pct>35&&m->i_rms>.3f);
 p->adc_count=persistence(p->adc_count,!healthy);
 if(p->undervoltage_count>=10)return FAULT_UNDERVOLTAGE;
 if(p->overvoltage_count>=10)return FAULT_OVERVOLTAGE;
 if(p->freq_count>=15)return FAULT_FREQ_DRIFT;
 if(p->adc_count>=5)return FAULT_ADC_HEALTH;
 p->warning=p->thd_count>=25?FAULT_HIGH_THD_WARN:FAULT_NONE;
 return FAULT_NONE;
}
