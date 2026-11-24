#ifndef GG_PROTECTION_H
#define GG_PROTECTION_H
#include "gg_metrology.h"
typedef enum { FAULT_NONE=0, FAULT_SHORT_CIRCUIT=1, FAULT_OVERCURRENT=2, FAULT_IDMT_OVERLOAD=3, FAULT_UNDERVOLTAGE=4, FAULT_OVERVOLTAGE=5, FAULT_FREQ_DRIFT=6, FAULT_HIGH_THD_WARN=7, FAULT_ADC_HEALTH=8, FAULT_RELAY_VERIFY=9 } gg_fault_code_t;
typedef enum { GG_CURVE_SI, GG_CURVE_VI, GG_CURVE_EI, GG_CURVE_LTI } gg_curve_t;
typedef struct { float thermal; unsigned overcurrent_count, undervoltage_count, overvoltage_count, freq_count, thd_count, adc_count; gg_fault_code_t warning; } gg_protection_t;
void gg_protection_init(gg_protection_t *p);
float gg_protection_operating_time(float multiple, gg_curve_t curve, float tms);
gg_fault_code_t gg_protection_idmt_step(gg_protection_t *p, float current_rms);
gg_fault_code_t gg_protection_step(gg_protection_t *p, const gg_metrics_t *m, int adc_healthy);
#endif
