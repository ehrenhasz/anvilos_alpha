 

#include "py/builtin.h"

#if MICROPY_PY_BUILTINS_FLOAT && MICROPY_PY_BUILTINS_COMPLEX && MICROPY_PY_CMATH

#include <math.h>


static mp_obj_t mp_cmath_phase(mp_obj_t z_obj) {
    mp_float_t real, imag;
    mp_obj_get_complex(z_obj, &real, &imag);
    return mp_obj_new_float(MICROPY_FLOAT_C_FUN(atan2)(imag, real));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_cmath_phase_obj, mp_cmath_phase);


static mp_obj_t mp_cmath_polar(mp_obj_t z_obj) {
    mp_float_t real, imag;
    mp_obj_get_complex(z_obj, &real, &imag);
    mp_obj_t tuple[2] = {
        mp_obj_new_float(MICROPY_FLOAT_C_FUN(sqrt)(real * real + imag * imag)),
        mp_obj_new_float(MICROPY_FLOAT_C_FUN(atan2)(imag, real)),
    };
    return mp_obj_new_tuple(2, tuple);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_cmath_polar_obj, mp_cmath_polar);


static mp_obj_t mp_cmath_rect(mp_obj_t r_obj, mp_obj_t phi_obj) {
    mp_float_t r = mp_obj_get_float(r_obj);
    mp_float_t phi = mp_obj_get_float(phi_obj);
    return mp_obj_new_complex(r * MICROPY_FLOAT_C_FUN(cos)(phi), r * MICROPY_FLOAT_C_FUN(sin)(phi));
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_cmath_rect_obj, mp_cmath_rect);


static mp_obj_t mp_cmath_exp(mp_obj_t z_obj) {
    mp_float_t real, imag;
    mp_obj_get_complex(z_obj, &real, &imag);
    mp_float_t exp_real = MICROPY_FLOAT_C_FUN(exp)(real);
    return mp_obj_new_complex(exp_real * MICROPY_FLOAT_C_FUN(cos)(imag), exp_real * MICROPY_FLOAT_C_FUN(sin)(imag));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_cmath_exp_obj, mp_cmath_exp);



static mp_obj_t mp_cmath_log(mp_obj_t z_obj) {
    mp_float_t real, imag;
    mp_obj_get_complex(z_obj, &real, &imag);
    return mp_obj_new_complex(MICROPY_FLOAT_CONST(0.5) * MICROPY_FLOAT_C_FUN(log)(real * real + imag * imag), MICROPY_FLOAT_C_FUN(atan2)(imag, real));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_cmath_log_obj, mp_cmath_log);

#if MICROPY_PY_MATH_SPECIAL_FUNCTIONS

static mp_obj_t mp_cmath_log10(mp_obj_t z_obj) {
    mp_float_t real, imag;
    mp_obj_get_complex(z_obj, &real, &imag);
    return mp_obj_new_complex(MICROPY_FLOAT_CONST(0.5) * MICROPY_FLOAT_C_FUN(log10)(real * real + imag * imag), MICROPY_FLOAT_CONST(0.4342944819032518) * MICROPY_FLOAT_C_FUN(atan2)(imag, real));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_cmath_log10_obj, mp_cmath_log10);
#endif


static mp_obj_t mp_cmath_sqrt(mp_obj_t z_obj) {
    mp_float_t real, imag;
    mp_obj_get_complex(z_obj, &real, &imag);
    mp_float_t sqrt_abs = MICROPY_FLOAT_C_FUN(pow)(real * real + imag * imag, MICROPY_FLOAT_CONST(0.25));
    mp_float_t theta = MICROPY_FLOAT_CONST(0.5) * MICROPY_FLOAT_C_FUN(atan2)(imag, real);
    return mp_obj_new_complex(sqrt_abs * MICROPY_FLOAT_C_FUN(cos)(theta), sqrt_abs * MICROPY_FLOAT_C_FUN(sin)(theta));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_cmath_sqrt_obj, mp_cmath_sqrt);


static mp_obj_t mp_cmath_cos(mp_obj_t z_obj) {
    mp_float_t real, imag;
    mp_obj_get_complex(z_obj, &real, &imag);
    return mp_obj_new_complex(MICROPY_FLOAT_C_FUN(cos)(real) * MICROPY_FLOAT_C_FUN(cosh)(imag), -MICROPY_FLOAT_C_FUN(sin)(real) * MICROPY_FLOAT_C_FUN(sinh)(imag));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_cmath_cos_obj, mp_cmath_cos);


static mp_obj_t mp_cmath_sin(mp_obj_t z_obj) {
    mp_float_t real, imag;
    mp_obj_get_complex(z_obj, &real, &imag);
    return mp_obj_new_complex(MICROPY_FLOAT_C_FUN(sin)(real) * MICROPY_FLOAT_C_FUN(cosh)(imag), MICROPY_FLOAT_C_FUN(cos)(real) * MICROPY_FLOAT_C_FUN(sinh)(imag));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_cmath_sin_obj, mp_cmath_sin);

static const mp_rom_map_elem_t mp_module_cmath_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_cmath) },
    { MP_ROM_QSTR(MP_QSTR_e), mp_const_float_e },
    { MP_ROM_QSTR(MP_QSTR_pi), mp_const_float_pi },
    { MP_ROM_QSTR(MP_QSTR_phase), MP_ROM_PTR(&mp_cmath_phase_obj) },
    { MP_ROM_QSTR(MP_QSTR_polar), MP_ROM_PTR(&mp_cmath_polar_obj) },
    { MP_ROM_QSTR(MP_QSTR_rect), MP_ROM_PTR(&mp_cmath_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_exp), MP_ROM_PTR(&mp_cmath_exp_obj) },
    { MP_ROM_QSTR(MP_QSTR_log), MP_ROM_PTR(&mp_cmath_log_obj) },
    #if MICROPY_PY_MATH_SPECIAL_FUNCTIONS
    { MP_ROM_QSTR(MP_QSTR_log10), MP_ROM_PTR(&mp_cmath_log10_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_sqrt), MP_ROM_PTR(&mp_cmath_sqrt_obj) },
    
    
    
    { MP_ROM_QSTR(MP_QSTR_cos), MP_ROM_PTR(&mp_cmath_cos_obj) },
    { MP_ROM_QSTR(MP_QSTR_sin), MP_ROM_PTR(&mp_cmath_sin_obj) },
    
    
    
    
    
    
    
    
    
    
};

static MP_DEFINE_CONST_DICT(mp_module_cmath_globals, mp_module_cmath_globals_table);

const mp_obj_module_t mp_module_cmath = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_cmath_globals,
};

MP_REGISTER_MODULE(MP_QSTR_cmath, mp_module_cmath);

#endif 
