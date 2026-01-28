
#ifndef MICROPY_INCLUDED_PY_OBJLIST_H
#define MICROPY_INCLUDED_PY_OBJLIST_H

#include "py/obj.h"

typedef struct _mp_obj_list_t {
    mp_obj_base_t base;
    size_t alloc;
    size_t len;
    mp_obj_t *items;
} mp_obj_list_t;

void mp_obj_list_init(mp_obj_list_t *o, size_t n);
mp_obj_t mp_obj_list_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *args);

#endif 
