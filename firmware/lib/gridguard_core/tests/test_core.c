#include "gg_metrology.h"
#include "gg_nilm.h"
#include "gg_protection.h"
#include "gg_relay_fsm.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(c,msg) do { if(!(c)){fprintf(stderr,"FAIL: %s (line %d)\n",msg,__LINE__);return 1;} } while(0)
static uint16_t code(float v,float fs){float x=floorf(2048+v/fs*2047+.5f);if(x<0)x=0;if(x>4095)x=4095;return (uint16_t)x;}
static void make_wave(gg_adc_sample_t *s,int smps){unsigned n,h;for(n=0;n<64;n++){float th=6.28318530718f*n/64, v=325.269f*(sinf(th)+.015f*sinf(3*th+.2f)+.010f*sinf(5*th-.1f)),i=0;
 if(smps){const float r[5]={1,.72f,.48f,.26f,.12f};for(h=0;h<5;h++){unsigned harmonic=1+2*h;i+=1.41421356237f*.38f*r[h]*sinf(harmonic*th+.2094395f);}}
 else i=1.41421356237f*4.35f*sinf(th);
 s[n].voltage_code=code(v,400);s[n].current_code=code(i,40);}}
int main(void){gg_adc_sample_t s[64];gg_metrics_t m;unsigned n;gg_protection_t prot;gg_relay_fsm_t relay;float dt=0;
 /* T1: quantized pure 230 V / 4.35 A resistive waveform. */
 gg_metrology_init();make_wave(s,0);for(n=0;n<5;n++)CHECK(gg_metrology_compute(s,&m)==0,"metrology computes");
 CHECK(fabsf(m.v_rms-230)<.5f,"T1 voltage RMS");CHECK(fabsf(m.p_w-1000)<3,"T1 active power");CHECK(m.pf>.995f,"T1 power factor");CHECK(fabsf(m.freq_hz-50)<.05f,"T1 frequency");CHECK(m.thd_i_pct<1,"T1 current THD");
 /* T2: SMPS odd harmonics and total harmonic distortion. */
 gg_metrology_init();make_wave(s,1);CHECK(gg_metrology_compute(s,&m)==0,"SMPS metrology");
 CHECK(fabsf(m.harm_i_rms[1]/m.harm_i_rms[0]-.72f)<.03f,"T2 third harmonic ratio");CHECK(fabsf(m.harm_i_rms[2]/m.harm_i_rms[0]-.48f)<.03f,"T2 fifth harmonic ratio");CHECK(fabsf(m.thd_i_pct-91.1f)<2,"T2 THD");
 /* T3: EI inverse-time integrator at exactly M=2. */
 gg_protection_init(&prot);CHECK(fabsf(gg_protection_operating_time(2,GG_CURVE_EI,.15f)-4)<.001f,"T3 theoretical time");
 for(n=0;n<199;n++)CHECK(gg_protection_idmt_step(&prot,16)==FAULT_NONE,"T3 early trip");
 CHECK(gg_protection_idmt_step(&prot,16)==FAULT_IDMT_OVERLOAD,"T3 trip at 4 seconds");
 /* T4: unknown capacitive event is rejected by the class distance gate. */
 {float capacitive[10]={600,600,850,0,0,0,0,0,1,1};gg_nilm_result_t r=gg_nilm_infer(capacitive);CHECK(r.class_id==GG_NILM_UNKNOWN,"T4 unknown-load Mahalanobis gate");}
 /* T5: verify only after settling and five consecutive independent samples. */
 gg_relay_init(&relay,FEEDBACK_VIRTUAL_OK);gg_relay_self_test_complete(&relay,1);gg_relay_step(&relay,20,12,12,1);CHECK(relay.state==NORMAL_CLOSED,"relay self test");gg_relay_request_trip(&relay,1);
 for(n=0;n<4;n++)gg_relay_step(&relay,20,12,0,1);CHECK(relay.state==VERIFY_OPEN,"T5 cannot confirm before settling/five samples");
 gg_relay_step(&relay,20,12,0,1);CHECK(relay.state==VERIFY_OPEN,"T5 settling delay is enforced");
 for(n=0;n<5;n++)gg_relay_step(&relay,20,12,0,1);CHECK(relay.state==PHYSICAL_DISCONNECT_CONFIRMED,"T5 healthy open after five samples");
 gg_relay_init(&relay,FEEDBACK_VIRTUAL_WELDED);gg_relay_self_test_complete(&relay,1);gg_relay_step(&relay,20,12,12,1);gg_relay_request_trip(&relay,1);for(n=0;n<30&&(relay.state==VERIFY_OPEN||relay.state==COIL_DEENERGIZED||relay.state==TRIP_REQUESTED);n++)gg_relay_step(&relay,20,12,12,1);
 CHECK(relay.state==TRIP_FAILURE,"T5 welded contact failure");
 gg_relay_init(&relay,FEEDBACK_VIRTUAL_NO_SRC);gg_relay_self_test_complete(&relay,1);gg_relay_step(&relay,20,12,12,1);gg_relay_request_trip(&relay,1);for(n=0;n<12&&relay.state!=DISCONNECT_UNVERIFIED;n++)gg_relay_step(&relay,20,0,0,1);
 CHECK(relay.state==DISCONNECT_UNVERIFIED,"T5 lost source cannot confirm disconnect");
 (void)dt;puts("T1-T5 core acceptance passed");return 0;}
