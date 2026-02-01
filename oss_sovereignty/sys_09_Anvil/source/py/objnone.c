 

#include <stdlib.h>

#include "py/obj.h"

#if !MICROPY_OBJ_IMMEDIATE_OBJS
typedef struct _mp_obj_none_t {
    mp_obj_base_t base;
} mp_obj_none_t;
#endif

static void none_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)self_in;
    if (MICROPY_PY_JSON && kind == PRINT_JSON) {
        mp_print_str(print, "null");
    } else {
        mp_print_str(print, "None");
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_NoneType,
    MP_QSTR_NoneType,
    MP_TYPE_FLAG_NONE,
    print, none_print
    );

#if !MICROPY_OBJ_IMMEDIATE_OBJS
const mp_obj_none_t mp_const_none_obj = {{&mp_type_NoneType}};
#endif
