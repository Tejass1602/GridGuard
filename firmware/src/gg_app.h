#ifndef GG_APP_H
#define GG_APP_H
#include <stdint.h>
#include "gg_metrology.h"
#include "gg_nilm.h"
#include "gg_protection.h"
#include "gg_relay_fsm.h"
#include "gg_plant.h"
typedef struct { gg_metrics_t metrics; gg_relay_fsm_t relay; gg_protection_t protection; gg_plant_t plant; gg_nilm_tracker_t nilm; gg_adc_sample_t waveform[64]; gg_fault_code_t fault; uint32_t timestamp_ms,dsp_exec_us,max_dsp_us,dropped_frames,trip_started_cycle,trip_latency_us; uint16_t seq; uint8_t feedback_mode; } gg_app_state_t;
extern gg_app_state_t gg_app;
int gg_hal_init(void);
int gg_hal_feedback(float *source_v,float *load_v,int *aux_high);
void gg_hal_set_coil(int enabled);
void gg_hal_status_led(int enabled);
int gg_store_init(void);
int gg_store_has_latched_fault(void);
void gg_store_send_faults(uint16_t start,uint16_t count);
uint16_t gg_store_record_count(void);
int gg_store_fault(gg_fault_code_t fault,gg_relay_state_t result,float trigger,float threshold,float source,float load,uint32_t ts);
int gg_comms_init(void);
void gg_comms_send_hello(void);
void gg_comms_send_telemetry(void);
void gg_comms_send_protection(void);
void gg_comms_send_waveform(void);
void gg_comms_send_health(void);
void gg_comms_send_fault_record(uint32_t id,uint32_t ts,uint8_t fault,uint8_t result,uint8_t coil,uint8_t aux,float trigger,float threshold,float source,float load,uint16_t verify,uint32_t crc);
void gg_comms_send_nilm(void);
void gg_comms_rx_task(void);
#endif
