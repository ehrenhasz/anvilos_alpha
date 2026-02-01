 

#include "py/mphal.h"
#include "py/runtime.h"
#include "py/smallint.h"
#include "extmod/modtime.h"

#if MICROPY_PY_TIME

#ifdef MICROPY_PY_TIME_INCLUDEFILE
#include MICROPY_PY_TIME_INCLUDEFILE
#endif

#if MICROPY_PY_TIME_GMTIME_LOCALTIME_MKTIME

#include "shared/timeutils/timeutils.h"













static mp_obj_t time_localtime(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0 || args[0] == mp_const_none) {
        
        return mp_time_localtime_get();
    } else {
        
        mp_int_t seconds = mp_obj_get_int(args[0]);
        timeutils_struct_time_t tm;
        timeutils_seconds_since_epoch_to_struct_time(seconds, &tm);
        mp_obj_t tuple[8] = {
            tuple[0] = mp_obj_new_int(tm.tm_year),
            tuple[1] = mp_obj_new_int(tm.tm_mon),
            tuple[2] = mp_obj_new_int(tm.tm_mday),
            tuple[3] = mp_obj_new_int(tm.tm_hour),
            tuple[4] = mp_obj_new_int(tm.tm_min),
            tuple[5] = mp_obj_new_int(tm.tm_sec),
            tuple[6] = mp_obj_new_int(tm.tm_wday),
            tuple[7] = mp_obj_new_int(tm.tm_yday),
        };
        return mp_obj_new_tuple(8, tuple);
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_time_localtime_obj, 0, 1, time_localtime);





static mp_obj_t time_mktime(mp_obj_t tuple) {
    size_t len;
    mp_obj_t *elem;
    mp_obj_get_array(tuple, &len, &elem);

    
    if (len < 8 || len > 9) {
        mp_raise_TypeError(MP_ERROR_TEXT("mktime needs a tuple of length 8 or 9"));
    }

    return mp_obj_new_int_from_uint(timeutils_mktime(mp_obj_get_int(elem[0]),
        mp_obj_get_int(elem[1]), mp_obj_get_int(elem[2]), mp_obj_get_int(elem[3]),
        mp_obj_get_int(elem[4]), mp_obj_get_int(elem[5])));
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_time_mktime_obj, time_mktime);

#endif 

#if MICROPY_PY_TIME_TIME_TIME_NS



static mp_obj_t time_time(void) {
    return mp_time_time_get();
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_time_time_obj, time_time);



static mp_obj_t time_time_ns(void) {
    return mp_obj_new_int_from_ull(mp_hal_time_ns());
}
MP_DEFINE_CONST_FUN_OBJ_0(mp_time_time_ns_obj, time_time_ns);

#endif 

static mp_obj_t time_sleep(mp_obj_t seconds_o) {
    #ifdef MICROPY_PY_TIME_CUSTOM_SLEEP
    mp_time_sleep(seconds_o);
    #else
    #if MICROPY_PY_BUILTINS_FLOAT
    mp_hal_delay_ms((mp_uint_t)(1000 * mp_obj_get_float(seconds_o)));
    #else
    mp_hal_delay_ms(1000 * mp_obj_get_int(seconds_o));
    #endif
    #endif
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_time_sleep_obj, time_sleep);

static mp_obj_t time_sleep_ms(mp_obj_t arg) {
    mp_int_t ms = mp_obj_get_int(arg);
    if (ms >= 0) {
        mp_hal_delay_ms(ms);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_time_sleep_ms_obj, time_sleep_ms);

static mp_obj_t time_sleep_us(mp_obj_t arg) {
    mp_int_t us = mp_obj_get_int(arg);
    if (us > 0) {
        mp_hal_delay_us(us);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_time_sleep_us_obj, time_sleep_us);

static mp_obj_t time_ticks_ms(void) {
    return MP_OBJ_NEW_SMALL_INT(mp_hal_ticks_ms() & (MICROPY_PY_TIME_TICKS_PERIOD - 1));
}
MP_DEFINE_CONST_FUN_OBJ_0(mp_time_ticks_ms_obj, time_ticks_ms);

static mp_obj_t time_ticks_us(void) {
    return MP_OBJ_NEW_SMALL_INT(mp_hal_ticks_us() & (MICROPY_PY_TIME_TICKS_PERIOD - 1));
}
MP_DEFINE_CONST_FUN_OBJ_0(mp_time_ticks_us_obj, time_ticks_us);

static mp_obj_t time_ticks_cpu(void) {
    return MP_OBJ_NEW_SMALL_INT(mp_hal_ticks_cpu() & (MICROPY_PY_TIME_TICKS_PERIOD - 1));
}
MP_DEFINE_CONST_FUN_OBJ_0(mp_time_ticks_cpu_obj, time_ticks_cpu);

static mp_obj_t time_ticks_diff(mp_obj_t end_in, mp_obj_t start_in) {
    
    mp_uint_t start = MP_OBJ_SMALL_INT_VALUE(start_in);
    mp_uint_t end = MP_OBJ_SMALL_INT_VALUE(end_in);
    
    
    mp_int_t diff = ((end - start + MICROPY_PY_TIME_TICKS_PERIOD / 2) & (MICROPY_PY_TIME_TICKS_PERIOD - 1))
        - MICROPY_PY_TIME_TICKS_PERIOD / 2;
    return MP_OBJ_NEW_SMALL_INT(diff);
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_time_ticks_diff_obj, time_ticks_diff);

static mp_obj_t time_ticks_add(mp_obj_t ticks_in, mp_obj_t delta_in) {
    
    mp_uint_t ticks = MP_OBJ_SMALL_INT_VALUE(ticks_in);
    mp_uint_t delta = mp_obj_get_int(delta_in);

    
    
    
    
    
    
    
    
    if (delta + MICROPY_PY_TIME_TICKS_PERIOD / 2 - 1 >= MICROPY_PY_TIME_TICKS_PERIOD - 1) {
        mp_raise_msg(&mp_type_OverflowError, MP_ERROR_TEXT("ticks interval overflow"));
    }

    return MP_OBJ_NEW_SMALL_INT((ticks + delta) & (MICROPY_PY_TIME_TICKS_PERIOD - 1));
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_time_ticks_add_obj, time_ticks_add);

static const mp_rom_map_elem_t mp_module_time_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_time) },

    #if MICROPY_PY_TIME_GMTIME_LOCALTIME_MKTIME
    { MP_ROM_QSTR(MP_QSTR_gmtime), MP_ROM_PTR(&mp_time_localtime_obj) },
    { MP_ROM_QSTR(MP_QSTR_localtime), MP_ROM_PTR(&mp_time_localtime_obj) },
    { MP_ROM_QSTR(MP_QSTR_mktime), MP_ROM_PTR(&mp_time_mktime_obj) },
    #endif

    #if MICROPY_PY_TIME_TIME_TIME_NS
    { MP_ROM_QSTR(MP_QSTR_time), MP_ROM_PTR(&mp_time_time_obj) },
    { MP_ROM_QSTR(MP_QSTR_time_ns), MP_ROM_PTR(&mp_time_time_ns_obj) },
    #endif

    { MP_ROM_QSTR(MP_QSTR_sleep), MP_ROM_PTR(&mp_time_sleep_obj) },
    { MP_ROM_QSTR(MP_QSTR_sleep_ms), MP_ROM_PTR(&mp_time_sleep_ms_obj) },
    { MP_ROM_QSTR(MP_QSTR_sleep_us), MP_ROM_PTR(&mp_time_sleep_us_obj) },

    { MP_ROM_QSTR(MP_QSTR_ticks_ms), MP_ROM_PTR(&mp_time_ticks_ms_obj) },
    { MP_ROM_QSTR(MP_QSTR_ticks_us), MP_ROM_PTR(&mp_time_ticks_us_obj) },
    { MP_ROM_QSTR(MP_QSTR_ticks_cpu), MP_ROM_PTR(&mp_time_ticks_cpu_obj) },
    { MP_ROM_QSTR(MP_QSTR_ticks_add), MP_ROM_PTR(&mp_time_ticks_add_obj) },
    { MP_ROM_QSTR(MP_QSTR_ticks_diff), MP_ROM_PTR(&mp_time_ticks_diff_obj) },

    #ifdef MICROPY_PY_TIME_EXTRA_GLOBALS
    MICROPY_PY_TIME_EXTRA_GLOBALS
    #endif
};
static MP_DEFINE_CONST_DICT(mp_module_time_globals, mp_module_time_globals_table);

const mp_obj_module_t mp_module_time = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_time_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_time, mp_module_time);

#endif 
