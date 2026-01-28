
#ifndef MICROPY_INCLUDED_PY_OBJNAMEDTUPLE_H
#define MICROPY_INCLUDED_PY_OBJNAMEDTUPLE_H

#include "py/objtuple.h"

typedef struct _mp_obj_namedtuple_type_t {
    
    mp_obj_empty_type_t base;
    void *slots[8];
    size_t n_fields;
    qstr fields[];
} mp_obj_namedtuple_type_t;

typedef struct _mp_obj_namedtuple_t {
    mp_obj_tuple_t tuple;
} mp_obj_namedtuple_t;

size_t mp_obj_namedtuple_find_field(const mp_obj_namedtuple_type_t *type, qstr name);
mp_obj_namedtuple_type_t *mp_obj_new_namedtuple_base(size_t n_fields, mp_obj_t *fields);

#endif 
