 

#include "py/runtime.h"
#include "py/mphal.h"
#include "extmod/modmachine.h"

#if MICROPY_PY_MACHINE_BITSTREAM



#define MICROPY_MACHINE_BITSTREAM_TYPE_HIGH_LOW (0)


static mp_obj_t machine_bitstream_(size_t n_args, const mp_obj_t *args) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    int encoding = mp_obj_get_int(args[1]);

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_READ);

    switch (encoding) {
        case MICROPY_MACHINE_BITSTREAM_TYPE_HIGH_LOW: {
            uint32_t timing_ns[4];
            mp_obj_t *timing;
            mp_obj_get_array_fixed_n(args[2], 4, &timing);
            for (size_t i = 0; i < 4; ++i) {
                timing_ns[i] = mp_obj_get_int(timing[i]);
            }
            machine_bitstream_high_low(pin, timing_ns, bufinfo.buf, bufinfo.len);
            break;
        }
        default:
            mp_raise_ValueError(MP_ERROR_TEXT("encoding"));
    }

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_bitstream_obj, 4, 4, machine_bitstream_);

#endif 
