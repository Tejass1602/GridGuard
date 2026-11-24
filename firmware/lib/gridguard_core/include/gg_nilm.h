#ifndef GG_NILM_H
#define GG_NILM_H
#include <stdint.h>
#define GG_NILM_FEATURES 10
#define GG_NILM_HIDDEN 16
#define GG_NILM_CLASSES 4
#define GG_NILM_UNKNOWN 255
typedef struct { float baseline_p, baseline_q, positive, negative; unsigned lockout, capture; int pending_event; uint8_t initialized; } gg_cusum_t;
typedef struct { uint8_t state[4],last_event_id,last_event_type; float est_power_w[4], est_energy_wh[4], confidence[4], unknown_power_w, mahalanobis; } gg_nilm_tracker_t;
typedef struct { float probability[4], distance; unsigned class_id; } gg_nilm_result_t;
void gg_cusum_init(gg_cusum_t *c);
int gg_cusum_step(gg_cusum_t *c, float p, float q);
gg_nilm_result_t gg_nilm_infer(const float features[10]);
void gg_nilm_tracker_init(gg_nilm_tracker_t *t);
void gg_nilm_tracker_event(gg_nilm_tracker_t *t, const gg_nilm_result_t *r, int turn_on, float measured_power, float dt_s);
#endif
