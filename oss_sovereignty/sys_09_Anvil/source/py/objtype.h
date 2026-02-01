 
#ifndef MICROPY_INCLUDED_PY_OBJTYPE_H
#define MICROPY_INCLUDED_PY_OBJTYPE_H

#include "py/obj.h"



typedef struct _mp_obj_instance_t {
    mp_obj_base_t base;
    mp_map_t members;
    mp_obj_t subobj[];
    
} mp_obj_instance_t;

#if MICROPY_CPYTHON_COMPAT

mp_obj_instance_t *mp_obj_new_instance(const mp_obj_type_t *cls, const mp_obj_type_t **native_base);
#endif


bool mp_obj_instance_is_callable(mp_obj_t self_in);
mp_obj_t mp_obj_instance_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args);

#define mp_obj_is_instance_type(type) ((type)->flags & MP_TYPE_FLAG_INSTANCE_TYPE)
#define mp_obj_is_native_type(type) (!((type)->flags & MP_TYPE_FLAG_INSTANCE_TYPE))


mp_obj_t mp_obj_instance_getiter(mp_obj_t self_in, mp_obj_iter_buf_t *iter_buf);

#endif 
