 

#include "py/runtime.h"

mp_obj_t mp_call_function_1_protected(mp_obj_t fun, mp_obj_t arg) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t ret = mp_call_function_1(fun, arg);
        nlr_pop();
        return ret;
    } else {
        mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
        return MP_OBJ_NULL;
    }
}

mp_obj_t mp_call_function_2_protected(mp_obj_t fun, mp_obj_t arg1, mp_obj_t arg2) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t ret = mp_call_function_2(fun, arg1, arg2);
        nlr_pop();
        return ret;
    } else {
        mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(nlr.ret_val));
        return MP_OBJ_NULL;
    }
}
