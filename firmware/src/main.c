#include "gg_app.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <math.h>
#include <string.h>
LOG_MODULE_REGISTER(gridguard,LOG_LEVEL_INF);

gg_app_state_t gg_app;
K_SEM_DEFINE(dsp_tick,0,1);K_SEM_DEFINE(relay_tick,0,1);
K_MSGQ_DEFINE(metrics_q,sizeof(gg_metrics_t),8,4);
typedef struct {gg_fault_code_t fault;gg_relay_state_t result;float value,threshold,source,load;uint32_t timestamp;} fault_event_t;
K_MSGQ_DEFINE(fault_q,sizeof(fault_event_t),8,4);
static struct k_timer cycle_timer;
static K_THREAD_STACK_DEFINE(dsp_stack,4096);static struct k_thread dsp_thread;
static K_THREAD_STACK_DEFINE(relay_stack,4096);static struct k_thread relay_thread;
static K_THREAD_STACK_DEFINE(nilm_stack,4096);static struct k_thread nilm_thread;
static K_THREAD_STACK_DEFINE(comms_stack,4096);static struct k_thread comms_thread;
static K_THREAD_STACK_DEFINE(nvs_stack,4096);static struct k_thread nvs_thread;
static void timer_expiry(struct k_timer *timer){ARG_UNUSED(timer);k_sem_give(&dsp_tick);k_sem_give(&relay_tick);}
static void dsp_entry(void *a,void *b,void *c){ARG_UNUSED(a);ARG_UNUSED(b);ARG_UNUSED(c);gg_adc_sample_t window[64];gg_metrics_t m;unsigned n;int rc;uint32_t start,elapsed;static unsigned stale_windows;
 while(1){k_sem_take(&dsp_tick,K_FOREVER);start=k_cycle_get_32();for(n=0;n<64;n++)window[n]=gg_plant_step(&gg_app.plant,NULL,NULL);memcpy(gg_app.waveform,window,sizeof(window));
  rc=gg_metrology_compute(window,&m);if(rc)continue;gg_app.metrics=m;gg_app.timestamp_ms=k_uptime_get_32();
  {unsigned clipped=0;int stale=1;for(n=0;n<64;n++){if(window[n].voltage_code==0||window[n].voltage_code==4095||window[n].current_code==0||window[n].current_code==4095)clipped++;if(n&&(window[n].voltage_code!=window[0].voltage_code||window[n].current_code!=window[0].current_code))stale=0;}stale_windows=stale?stale_windows+1:0;gg_app.fault=gg_protection_step(&gg_app.protection,&m,clipped<=8&&stale_windows<5);}
  if(gg_app.fault!=FAULT_NONE&&gg_app.relay.state==NORMAL_CLOSED){gg_app.trip_started_cycle=k_cycle_get_32();gg_relay_request_trip(&gg_app.relay,1);fault_event_t e={gg_app.fault,gg_app.relay.state,m.i_rms,12,0,0,gg_app.timestamp_ms};if(k_msgq_put(&fault_q,&e,K_NO_WAIT)!=0)gg_app.dropped_frames++;}
  if(k_msgq_put(&metrics_q,&m,K_NO_WAIT)!=0)gg_app.dropped_frames++;
  if(gg_app.relay.state==TRIP_FAILURE||gg_app.relay.state==CLOSE_FAILURE)gg_hal_status_led(1);
  elapsed=k_cycle_get_32()-start;gg_app.dsp_exec_us=k_cyc_to_us_floor32(elapsed);if(gg_app.dsp_exec_us>gg_app.max_dsp_us)gg_app.max_dsp_us=gg_app.dsp_exec_us;k_msleep(5);
 }}
static void relay_entry(void *a,void *b,void *c){ARG_UNUSED(a);ARG_UNUSED(b);ARG_UNUSED(c);float src=0,load=0;int aux=1,rc;gg_relay_state_t old;
 while(1){k_sem_take(&relay_tick,K_FOREVER);old=gg_app.relay.state;
  if(gg_app.feedback_mode<=FEEDBACK_HARDWARE_LOWSIDE){rc=gg_hal_feedback(&src,&load,&aux);if(rc){src=0;load=0;}}
  else {src=gg_app.feedback_mode==FEEDBACK_VIRTUAL_NO_SRC?0:12;
   if(gg_app.feedback_mode==FEEDBACK_VIRTUAL_WELDED)load=12;else load=gg_app.relay.coil_cmd?12:0;aux=1;}
  gg_relay_step(&gg_app.relay,20,src,load,aux);
  gg_hal_set_coil(gg_app.relay.coil_cmd);
  if(old!=gg_app.relay.state){if(gg_app.trip_started_cycle&&(gg_app.relay.state==PHYSICAL_DISCONNECT_CONFIRMED||gg_app.relay.state==TRIP_FAILURE||gg_app.relay.state==DISCONNECT_UNVERIFIED)){gg_app.trip_latency_us=k_cyc_to_us_floor32(k_cycle_get_32()-gg_app.trip_started_cycle);gg_app.trip_started_cycle=0;}if(gg_app.fault!=FAULT_NONE||gg_app.relay.state==PHYSICAL_DISCONNECT_CONFIRMED||gg_app.relay.state==TRIP_FAILURE){fault_event_t e={gg_app.fault,gg_app.relay.state,0,0,src,load,k_uptime_get_32()};(void)k_msgq_put(&fault_q,&e,K_NO_WAIT);}gg_comms_send_protection();}
 }}
static void nilm_entry(void *a,void *b,void *c){ARG_UNUSED(a);ARG_UNUSED(b);ARG_UNUSED(c);gg_cusum_t cusum;gg_metrics_t m; static gg_metrics_t history[10],capture[25];unsigned history_count=0,history_pos=0,capture_count=0,j;int event,was_capture;gg_cusum_init(&cusum);gg_nilm_tracker_init(&gg_app.nilm);memset(history,0,sizeof(history));
 while(1){if(k_msgq_get(&metrics_q,&m,K_FOREVER)!=0)continue;was_capture=cusum.capture>0;if(was_capture&&capture_count<25)capture[capture_count++]=m;event=gg_cusum_step(&cusum,m.p_w,m.q1_var);
  if(cusum.capture==25&&!was_capture)capture_count=0;
  if(event){gg_metrics_t pre={0},post={0};float features[10],peak=0,di1,dp,dq,ds;unsigned count=history_count<10?history_count:10;
   for(j=0;j<count;j++){const gg_metrics_t *x=&history[(history_pos+10-count+j)%10];pre.p_w+=x->p_w;pre.q1_var+=x->q1_var;pre.s_va+=x->s_va;pre.i_rms+=x->i_rms;pre.thd_i_pct+=x->thd_i_pct;for(unsigned h=0;h<6;h++)pre.harm_i_rms[h]+=x->harm_i_rms[h];}
   if(count){pre.p_w/=count;pre.q1_var/=count;pre.s_va/=count;pre.i_rms/=count;pre.thd_i_pct/=count;for(unsigned h=0;h<6;h++)pre.harm_i_rms[h]/=count;}
   {unsigned begin=event>0&&capture_count>8?capture_count-8:0;unsigned used=capture_count-begin;if(!used)used=1;for(j=0;j<capture_count&&j<15;j++){if(capture[j].i_rms>peak)peak=capture[j].i_rms;}for(j=begin;j<capture_count;j++){const gg_metrics_t *x=&capture[j];post.p_w+=x->p_w;post.q1_var+=x->q1_var;post.s_va+=x->s_va;post.i_rms+=x->i_rms;post.thd_i_pct+=x->thd_i_pct;for(unsigned h=0;h<6;h++)post.harm_i_rms[h]+=x->harm_i_rms[h];}post.p_w/=used;post.q1_var/=used;post.s_va/=used;post.i_rms/=used;post.thd_i_pct/=used;for(unsigned h=0;h<6;h++)post.harm_i_rms[h]/=used;}
   dp=post.p_w-pre.p_w;dq=post.q1_var-pre.q1_var;ds=post.s_va-pre.s_va;di1=post.harm_i_rms[0]-pre.harm_i_rms[0];features[0]=fabsf(dp);features[1]=dq;features[2]=fabsf(ds);features[3]=dp/fmaxf(hypotf(dp,dq),1);features[4]=(post.harm_i_rms[1]-pre.harm_i_rms[1])/fmaxf(di1,.05f);features[5]=(post.harm_i_rms[2]-pre.harm_i_rms[2])/fmaxf(di1,.05f);features[6]=(post.harm_i_rms[3]-pre.harm_i_rms[3])/fmaxf(di1,.05f);features[7]=((post.i_rms<0.05f?0.0f:post.thd_i_pct)-(pre.i_rms<0.05f?0.0f:pre.thd_i_pct))/100.0f;features[8]=fmaxf(peak-pre.i_rms,0)/fmaxf(fabsf(di1),.05f);features[9]=copysignf(fabsf(dq)/fmaxf(fabsf(dp),1),dq);
   if(event>0){gg_nilm_result_t r=gg_nilm_infer(features);gg_nilm_tracker_event(&gg_app.nilm,&r,1,fabsf(dp),.02f);}
   else {static const float pn[4]={1000.0f,75.0f,85.0f,250.0f},qn[4]={0.0f,57.0f,-18.0f,188.0f},i3n[4]={0.0f,0.016f,0.27f,0.11f};float di3=pre.harm_i_rms[1]-post.harm_i_rms[1];unsigned best=4;float error=INFINITY;for(j=0;j<4;j++)if(gg_app.nilm.state[j]){float d=fabsf((-dp)-pn[j])/40.0f+fabsf((-dq)-qn[j])/25.0f+fabsf(di3-i3n[j])/0.06f;if(d<error&&d<6.0f){error=d;best=j;}}if(best<4){gg_nilm_result_t r={0};r.class_id=best;r.distance=0;r.probability[best]=1;gg_nilm_tracker_event(&gg_app.nilm,&r,0,0,.02f);}else gg_app.nilm.unknown_power_w=fmaxf(0,post.p_w);}
   capture_count=0;
  }
  if(!cusum.capture){history[history_pos]=m;history_pos=(history_pos+1)%10;if(history_count<10)history_count++;}
 }}
static void comms_entry(void *a,void *b,void *c){ARG_UNUSED(a);ARG_UNUSED(b);ARG_UNUSED(c);while(1){static unsigned led_cnt=0;if(++led_cnt%10==0)gg_hal_status_led((led_cnt/10)&1);gg_comms_rx_task();gg_comms_send_telemetry();k_sleep(K_MSEC(50));}}
static void nvs_entry(void *a,void *b,void *c){ARG_UNUSED(a);ARG_UNUSED(b);ARG_UNUSED(c);fault_event_t e;while(1){if(k_msgq_get(&fault_q,&e,K_FOREVER)==0)(void)gg_store_fault(e.fault,e.result,e.value,e.threshold,e.source,e.load,e.timestamp);}}
int main(void){int rc,hal_rc;
 memset(&gg_app,0,sizeof(gg_app));gg_app.feedback_mode=FEEDBACK_VIRTUAL_OK;gg_relay_init(&gg_app.relay,(gg_feedback_mode_t)gg_app.feedback_mode);gg_app.relay.aux_enabled=0;gg_protection_init(&gg_app.protection);gg_plant_init(&gg_app.plant,42);gg_metrology_init();
 hal_rc=gg_hal_init();if(hal_rc)LOG_ERR("HAL initialization failed: %d",hal_rc);gg_hal_set_coil(0);gg_hal_status_led(1);
 rc=gg_store_init();if(rc)LOG_ERR("NVS initialization failed: %d",rc);
 if(gg_app.relay.state==BOOT_SAFE_OPEN&&!gg_store_has_latched_fault())gg_relay_self_test_complete(&gg_app.relay,rc==0&&hal_rc==0);
 (void)gg_comms_init();
 k_sleep(K_MSEC(500));
 k_thread_create(&dsp_thread,dsp_stack,K_THREAD_STACK_SIZEOF(dsp_stack),dsp_entry,NULL,NULL,NULL,3,K_FP_REGS,K_NO_WAIT);k_thread_name_set(&dsp_thread,"gg_dsp_prot");
 k_thread_create(&relay_thread,relay_stack,K_THREAD_STACK_SIZEOF(relay_stack),relay_entry,NULL,NULL,NULL,3,K_FP_REGS,K_NO_WAIT);k_thread_name_set(&relay_thread,"gg_relay_ver");
 k_thread_create(&nilm_thread,nilm_stack,K_THREAD_STACK_SIZEOF(nilm_stack),nilm_entry,NULL,NULL,NULL,3,K_FP_REGS,K_NO_WAIT);k_thread_name_set(&nilm_thread,"gg_nilm");
 k_thread_create(&comms_thread,comms_stack,K_THREAD_STACK_SIZEOF(comms_stack),comms_entry,NULL,NULL,NULL,3,K_FP_REGS,K_NO_WAIT);k_thread_name_set(&comms_thread,"gg_comms");
 k_thread_create(&nvs_thread,nvs_stack,K_THREAD_STACK_SIZEOF(nvs_stack),nvs_entry,NULL,NULL,NULL,8,0,K_NO_WAIT);k_thread_name_set(&nvs_thread,"gg_nvs_log");
 k_timer_init(&cycle_timer,timer_expiry,NULL);k_timer_start(&cycle_timer,K_MSEC(20),K_MSEC(20));LOG_INF("GridGuard internal-plant mode ready");return 0;
}
