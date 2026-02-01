 

#include <stdlib.h>
#include <assert.h>

#include "py/obj.h"

 
 

typedef struct _mp_obj_singleton_t {
    mp_obj_base_t base;
    qstr name;
} mp_obj_singleton_t;

static void singleton_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_singleton_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "%q", self->name);
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_singleton, MP_QSTR_, MP_TYPE_FLAG_NONE,
    print, singleton_print
    );

const mp_obj_singleton_t mp_const_ellipsis_obj = {{&mp_type_singleton}, MP_QSTR_Ellipsis};
#if MICROPY_PY_BUILTINS_NOTIMPLEMENTED
const mp_obj_singleton_t mp_const_notimplemented_obj = {{&mp_type_singleton}, MP_QSTR_NotImplemented};
#endif
