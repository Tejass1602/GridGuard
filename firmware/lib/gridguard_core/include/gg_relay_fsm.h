#ifndef GG_RELAY_FSM_H
#define GG_RELAY_FSM_H
#include <stdint.h>
typedef enum { BOOT_SAFE_OPEN, SELF_TEST, NORMAL_CLOSED, TRIP_REQUESTED, COIL_DEENERGIZED, VERIFY_OPEN, PHYSICAL_DISCONNECT_CONFIRMED, TRIP_FAILURE, DISCONNECT_UNVERIFIED, RESET_CHECK, VERIFY_CLOSED, CLOSE_FAILURE } gg_relay_state_t;
typedef enum { FEEDBACK_HARDWARE_HIGHSIDE=0, FEEDBACK_HARDWARE_LOWSIDE=1, FEEDBACK_VIRTUAL_OK=2, FEEDBACK_VIRTUAL_WELDED=3, FEEDBACK_VIRTUAL_NO_SRC=4 } gg_feedback_mode_t;
typedef struct { gg_relay_state_t state; gg_feedback_mode_t mode; uint32_t elapsed_ms; unsigned samples, source_lost_samples; int coil_cmd, fault_active, aux_enabled; } gg_relay_fsm_t;
void gg_relay_init(gg_relay_fsm_t *f, gg_feedback_mode_t mode);
void gg_relay_self_test_complete(gg_relay_fsm_t *f, int success);
void gg_relay_request_trip(gg_relay_fsm_t *f, int fault_active);
void gg_relay_step(gg_relay_fsm_t *f, uint32_t dt_ms, float v_src, float v_load, int aux_high);
int gg_relay_request_reset(gg_relay_fsm_t *f, int fault_clear, float v_src);
#endif
