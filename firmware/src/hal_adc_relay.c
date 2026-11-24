#include "gg_app.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>

#define ADC_SOURCE_SPEC ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0)
#define ADC_LOAD_SPEC ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1)
static const struct gpio_dt_spec relay_gpio=GPIO_DT_SPEC_GET(DT_NODELABEL(relay_drv),gpios);
static const struct gpio_dt_spec status_gpio=GPIO_DT_SPEC_GET(DT_NODELABEL(status_led),gpios);
static const struct gpio_dt_spec aux_gpio=GPIO_DT_SPEC_GET(DT_NODELABEL(aux_contact),gpios);
static const struct adc_dt_spec source_adc=ADC_SOURCE_SPEC;
static const struct adc_dt_spec load_adc=ADC_LOAD_SPEC;
static int read_pin(const struct adc_dt_spec *spec,int32_t *mv){
 int16_t raw=0;int32_t converted;struct adc_sequence seq={.buffer=&raw,.buffer_size=sizeof(raw),.resolution=12};int rc;
 rc=adc_sequence_init_dt(spec,&seq);if(rc)return rc;rc=adc_read(spec->dev,&seq);if(rc)return rc;
 converted=raw;rc=adc_raw_to_millivolts_dt(spec,&converted);if(rc<0)return rc;*mv=converted;return 0;
}
int gg_hal_init(void){int rc;
 if(!gpio_is_ready_dt(&relay_gpio)||!gpio_is_ready_dt(&status_gpio)||!gpio_is_ready_dt(&aux_gpio))return -ENODEV;
 rc=gpio_pin_configure_dt(&relay_gpio,GPIO_OUTPUT_INACTIVE);if(rc)return rc;
 rc=gpio_pin_configure_dt(&status_gpio,GPIO_OUTPUT_INACTIVE);if(rc)return rc;
 rc=gpio_pin_configure_dt(&aux_gpio,GPIO_INPUT);if(rc)return rc;
 if(!adc_is_ready_dt(&source_adc)||!adc_is_ready_dt(&load_adc))return -ENODEV;
 rc=adc_channel_setup_dt(&source_adc);if(rc)return rc;return adc_channel_setup_dt(&load_adc);
}
int gg_hal_feedback(float *source,float *load,int *aux_high){int32_t s,l,aux,rc;
 rc=read_pin(&source_adc,&s);if(rc)return rc;rc=read_pin(&load_adc,&l);if(rc)return rc;
 *source=(float)s*4.3f/1000.0f;*load=(float)l*4.3f/1000.0f;
 if(gg_app.feedback_mode==FEEDBACK_HARDWARE_LOWSIDE)*load=*source-*load;
 aux=gpio_pin_get_raw(aux_gpio.port,aux_gpio.pin);if(aux<0)return aux;*aux_high=aux!=0;return 0;
}
void gg_hal_set_coil(int enabled){gpio_pin_set_dt(&relay_gpio,enabled?1:0);}
void gg_hal_status_led(int enabled){gpio_pin_set_dt(&status_gpio,enabled?1:0);}
