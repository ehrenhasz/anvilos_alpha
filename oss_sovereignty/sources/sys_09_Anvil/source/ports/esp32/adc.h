
#ifndef MICROPY_INCLUDED_ESP32_ADC_H
#define MICROPY_INCLUDED_ESP32_ADC_H

#include "py/runtime.h"
#include "esp_adc_cal.h"

#define ADC_ATTEN_MAX SOC_ADC_ATTEN_NUM

typedef struct _machine_adc_block_obj_t {
    mp_obj_base_t base;
    adc_unit_t unit_id;
    mp_int_t bits;
    adc_bits_width_t width;
    esp_adc_cal_characteristics_t *characteristics[ADC_ATTEN_MAX];
} machine_adc_block_obj_t;

typedef struct _machine_adc_obj_t {
    mp_obj_base_t base;
    machine_adc_block_obj_t *block;
    adc_channel_t channel_id;
    gpio_num_t gpio_id;
} machine_adc_obj_t;

extern machine_adc_block_obj_t madcblock_obj[];

void madcblock_bits_helper(machine_adc_block_obj_t *self, mp_int_t bits);
mp_int_t madcblock_read_helper(machine_adc_block_obj_t *self, adc_channel_t channel_id);
mp_int_t madcblock_read_uv_helper(machine_adc_block_obj_t *self, adc_channel_t channel_id, adc_atten_t atten);

const machine_adc_obj_t *madc_search_helper(machine_adc_block_obj_t *block, adc_channel_t channel_id, gpio_num_t gpio_id);
void madc_init_helper(const machine_adc_obj_t *self, size_t n_pos_args, const mp_obj_t *pos_args, mp_map_t *kw_args);

#endif 
