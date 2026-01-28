

#ifndef MICROPY_INCLUDED_RA_PIN_H
#define MICROPY_INCLUDED_RA_PIN_H

#include "shared/runtime/mpirq.h"
#include "py/obj.h"

typedef struct {
    mp_obj_base_t base;
    qstr name;
    uint8_t pin;
    uint8_t bit;
    uint8_t channel;
} machine_pin_adc_obj_t;

typedef struct {
    mp_obj_base_t base;
    qstr name;
    uint8_t pin;
    const machine_pin_adc_obj_t *ad;
} machine_pin_obj_t;


#include "genhdr/pins.h"

extern const mp_obj_type_t machine_pin_board_pins_obj_type;
extern const mp_obj_type_t machine_pin_cpu_pins_obj_type;

extern const mp_obj_dict_t machine_pin_cpu_pins_locals_dict;
extern const mp_obj_dict_t machine_pin_board_pins_locals_dict;

void machine_pin_init(void);
uint32_t pin_get_mode(const machine_pin_obj_t *pin);
uint32_t pin_get_pull(const machine_pin_obj_t *pin);
uint32_t pin_get_drive(const machine_pin_obj_t *pin);
uint32_t pin_get_af(const machine_pin_obj_t *pin);
const machine_pin_obj_t *machine_pin_find(mp_obj_t user_obj);
const machine_pin_obj_t *pin_find_named_pin(const mp_obj_dict_t *named_pins, mp_obj_t name);

#endif 
