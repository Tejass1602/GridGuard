#ifndef GG_PLANT_H
#define GG_PLANT_H
#include <stdint.h>
#define GG_FS_HZ 3200.0f
#define GG_WINDOW_SAMPLES 64
#define GG_APPLIANCE_COUNT 4
typedef enum { GG_FAULT_CLEAR=0, GG_FAULT_SAG=1, GG_FAULT_SWELL=2, GG_FAULT_FREQ_LOW=3, GG_FAULT_FREQ_HIGH=4, GG_FAULT_SHORT=5, GG_FAULT_OVERLOAD=6, GG_FAULT_ADC_CLIP=7 } gg_fault_scenario_t;
typedef struct {
    float phase, grid_hz, voltage_rms, elapsed_s[GG_APPLIANCE_COUNT];
    float inrush_peak_a[GG_APPLIANCE_COUNT];
    uint8_t appliance_on[GG_APPLIANCE_COUNT];
    gg_fault_scenario_t fault;
    float fault_magnitude;
    uint32_t rng;
} gg_plant_t;
typedef struct { uint16_t voltage_code, current_code; } gg_adc_sample_t;
void gg_plant_init(gg_plant_t *plant, uint32_t seed);
void gg_plant_set_appliance(gg_plant_t *plant, unsigned id, int on);
void gg_plant_set_fault(gg_plant_t *plant, gg_fault_scenario_t fault, float magnitude);
gg_adc_sample_t gg_plant_step(gg_plant_t *plant, float *volts, float *amps);
#endif
