
#ifndef MICROPY_INCLUDED_EXTMOD_VIRTPIN_H
#define MICROPY_INCLUDED_EXTMOD_VIRTPIN_H

#include "py/obj.h"

#define MP_PIN_READ   (1)
#define MP_PIN_WRITE  (2)
#define MP_PIN_INPUT  (3)
#define MP_PIN_OUTPUT (4)


typedef struct _mp_pin_p_t {
    mp_uint_t (*ioctl)(mp_obj_t obj, mp_uint_t request, uintptr_t arg, int *errcode);
} mp_pin_p_t;

int mp_virtual_pin_read(mp_obj_t pin);
void mp_virtual_pin_write(mp_obj_t pin, int value);


mp_obj_t mp_pin_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args);

#endif 
