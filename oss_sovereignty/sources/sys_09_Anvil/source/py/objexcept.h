
#ifndef MICROPY_INCLUDED_PY_OBJEXCEPT_H
#define MICROPY_INCLUDED_PY_OBJEXCEPT_H

#include "py/obj.h"
#include "py/objtuple.h"

typedef struct _mp_obj_exception_t {
    mp_obj_base_t base;
    size_t traceback_alloc : (8 * sizeof(size_t) / 2);
    size_t traceback_len : (8 * sizeof(size_t) / 2);
    size_t *traceback_data;
    mp_obj_tuple_t *args;
} mp_obj_exception_t;

void mp_obj_exception_print(const mp_print_t *print, mp_obj_t o_in, mp_print_kind_t kind);
void mp_obj_exception_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);

#define MP_DEFINE_EXCEPTION(exc_name, base_name) \
    MP_DEFINE_CONST_OBJ_TYPE(mp_type_##exc_name, MP_QSTR_##exc_name, MP_TYPE_FLAG_NONE, \
    make_new, mp_obj_exception_make_new, \
    print, mp_obj_exception_print, \
    attr, mp_obj_exception_attr, \
    parent, &mp_type_##base_name \
    );

#endif 
