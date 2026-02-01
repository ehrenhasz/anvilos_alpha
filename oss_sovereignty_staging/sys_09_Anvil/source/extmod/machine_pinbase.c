 

#include "py/runtime.h"

#if MICROPY_PY_MACHINE_PIN_BASE

#include "extmod/modmachine.h"
#include "extmod/virtpin.h"






typedef struct _mp_pinbase_t {
    mp_obj_base_t base;
} mp_pinbase_t;

static const mp_pinbase_t pinbase_singleton = {
    .base = { &machine_pinbase_type },
};

static mp_obj_t pinbase_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type;
    (void)n_args;
    (void)n_kw;
    (void)args;
    return MP_OBJ_FROM_PTR(&pinbase_singleton);
}

mp_uint_t pinbase_ioctl(mp_obj_t obj, mp_uint_t request, uintptr_t arg, int *errcode);
mp_uint_t pinbase_ioctl(mp_obj_t obj, mp_uint_t request, uintptr_t arg, int *errcode) {
    (void)errcode;
    switch (request) {
        case MP_PIN_READ: {
            mp_obj_t dest[2];
            mp_load_method(obj, MP_QSTR_value, dest);
            return mp_obj_get_int(mp_call_method_n_kw(0, 0, dest));
        }
        case MP_PIN_WRITE: {
            mp_obj_t dest[3];
            mp_load_method(obj, MP_QSTR_value, dest);
            dest[2] = (arg == 0 ? mp_const_false : mp_const_true);
            mp_call_method_n_kw(1, 0, dest);
            return 0;
        }
    }
    return -1;
}

static const mp_pin_p_t pinbase_pin_p = {
    .ioctl = pinbase_ioctl,
};

MP_DEFINE_CONST_OBJ_TYPE(
    machine_pinbase_type,
    MP_QSTR_PinBase,
    MP_TYPE_FLAG_NONE,
    make_new, pinbase_make_new,
    protocol, &pinbase_pin_p
    );

#endif 
