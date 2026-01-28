

#ifndef ADC_H__
#define ADC_H__

typedef struct _machine_adc_obj_t machine_adc_obj_t;

void adc_init0(void);

int16_t machine_adc_value_read(machine_adc_obj_t *adc_obj);

#endif 
