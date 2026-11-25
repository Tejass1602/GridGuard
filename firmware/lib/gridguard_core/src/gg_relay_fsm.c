#include "gg_relay_fsm.h"
#include <string.h>
void gg_relay_init(gg_relay_fsm_t *f,gg_feedback_mode_t mode){memset(f,0,sizeof(*f));f->state=BOOT_SAFE_OPEN;f->mode=mode;}
void gg_relay_self_test_complete(gg_relay_fsm_t *f,int ok){if(f&&f->state==BOOT_SAFE_OPEN)f->state=ok?SELF_TEST:TRIP_FAILURE;}
void gg_relay_request_trip(gg_relay_fsm_t *f,int fault){if(!f)return;f->fault_active=fault;f->state=TRIP_REQUESTED;f->elapsed_ms=0;f->samples=0;f->source_lost_samples=0;}
void gg_relay_step(gg_relay_fsm_t *f,uint32_t dt,float src,float load,int aux_high){
 float threshold;
 if(!f)return;
 if(f->state==SELF_TEST){f->state=NORMAL_CLOSED;f->coil_cmd=1;}
 if(f->state==TRIP_REQUESTED){f->coil_cmd=0;f->state=COIL_DEENERGIZED;f->elapsed_ms=0;f->samples=0;f->source_lost_samples=0;}
 if(f->state==COIL_DEENERGIZED)f->state=VERIFY_OPEN;
 if(f->state==VERIFY_OPEN){f->elapsed_ms+=dt;if(f->elapsed_ms<=100)return;
  if(src<3.0f)f->source_lost_samples++;else f->source_lost_samples=0;
  threshold=src*.10f;if(threshold>.5f)threshold=.5f;
  if(src>=3.0f&&load<threshold&&(!f->aux_enabled||aux_high))f->samples++;else if(src>=3.0f)f->samples=0;
  if(f->samples>=5){f->state=PHYSICAL_DISCONNECT_CONFIRMED;return;}
  if(f->source_lost_samples>=5){f->state=DISCONNECT_UNVERIFIED;return;}
  if(f->elapsed_ms>=600){f->state=src>=3.0f?TRIP_FAILURE:DISCONNECT_UNVERIFIED;return;}}
 if(f->state==RESET_CHECK){if(f->fault_active||src<3){f->state=TRIP_FAILURE;f->coil_cmd=0;return;}f->coil_cmd=1;f->elapsed_ms=0;f->samples=0;f->state=VERIFY_CLOSED;}
 if(f->state==VERIFY_CLOSED){f->elapsed_ms+=dt;if(f->elapsed_ms<=100)return;
  if(src>=3&&load>=.8f*src)f->samples++;else f->samples=0;
  if(f->samples>=5){f->state=NORMAL_CLOSED;return;}
  if(f->elapsed_ms>=600){f->coil_cmd=0;f->state=CLOSE_FAILURE;}}
}
int gg_relay_request_reset(gg_relay_fsm_t *f,int clear,float src){if(!f||!clear||src<3||(f->state!=PHYSICAL_DISCONNECT_CONFIRMED&&f->state!=BOOT_SAFE_OPEN&&f->state!=TRIP_FAILURE&&f->state!=DISCONNECT_UNVERIFIED&&f->state!=CLOSE_FAILURE))return 0;f->fault_active=0;f->state=RESET_CHECK;return 1;}
