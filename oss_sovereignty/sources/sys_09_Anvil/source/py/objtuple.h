
#ifndef MICROPY_INCLUDED_PY_OBJTUPLE_H
#define MICROPY_INCLUDED_PY_OBJTUPLE_H

#include "py/obj.h"

typedef struct _mp_obj_tuple_t {
    mp_obj_base_t base;
    size_t len;
    mp_obj_t items[];
} mp_obj_tuple_t;

typedef struct _mp_rom_obj_tuple_t {
    mp_obj_base_t base;
    size_t len;
    mp_rom_obj_t items[];
} mp_rom_obj_tuple_t;

void mp_obj_tuple_print(const mp_print_t *print, mp_obj_t o_in, mp_print_kind_t kind);
mp_obj_t mp_obj_tuple_unary_op(mp_unary_op_t op, mp_obj_t self_in);
mp_obj_t mp_obj_tuple_binary_op(mp_binary_op_t op, mp_obj_t lhs, mp_obj_t rhs);
mp_obj_t mp_obj_tuple_subscr(mp_obj_t base, mp_obj_t index, mp_obj_t value);
mp_obj_t mp_obj_tuple_getiter(mp_obj_t o_in, mp_obj_iter_buf_t *iter_buf);

extern const mp_obj_type_t mp_type_attrtuple;

#define MP_DEFINE_ATTRTUPLE(tuple_obj_name, fields, nitems, ...) \
    const mp_rom_obj_tuple_t tuple_obj_name = { \
        .base = {&mp_type_attrtuple}, \
        .len = nitems, \
        .items = { __VA_ARGS__, MP_ROM_PTR((void *)fields) } \
    }

#if MICROPY_PY_COLLECTIONS
void mp_obj_attrtuple_print_helper(const mp_print_t *print, const qstr *fields, mp_obj_tuple_t *o);
#endif

mp_obj_t mp_obj_new_attrtuple(const qstr *fields, size_t n, const mp_obj_t *items);

#endif 
