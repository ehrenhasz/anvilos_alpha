 

#include "extmod/virtpin.h"

int mp_virtual_pin_read(mp_obj_t pin) {
    mp_obj_base_t *s = (mp_obj_base_t *)MP_OBJ_TO_PTR(pin);
    mp_pin_p_t *pin_p = (mp_pin_p_t *)MP_OBJ_TYPE_GET_SLOT(s->type, protocol);
    return pin_p->ioctl(pin, MP_PIN_READ, 0, NULL);
}

void mp_virtual_pin_write(mp_obj_t pin, int value) {
    mp_obj_base_t *s = (mp_obj_base_t *)MP_OBJ_TO_PTR(pin);
    mp_pin_p_t *pin_p = (mp_pin_p_t *)MP_OBJ_TYPE_GET_SLOT(s->type, protocol);
    pin_p->ioctl(pin, MP_PIN_WRITE, value, NULL);
}
