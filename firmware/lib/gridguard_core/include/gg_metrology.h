#ifndef GG_METROLOGY_H
#define GG_METROLOGY_H
#include "gg_plant.h"
typedef struct {
 float v_rms, i_rms, p_w, q1_var, s_va, pf, freq_hz, thd_i_pct, thd_v_pct, energy_wh;
 float harm_i_rms[6], harm_v_rms[6];
 float fundamental_v_re, fundamental_v_im, fundamental_i_re, fundamental_i_im, distortion_va;
} gg_metrics_t;
void gg_metrology_init(void);
int gg_metrology_compute(const gg_adc_sample_t samples[GG_WINDOW_SAMPLES], gg_metrics_t *out);
#endif
