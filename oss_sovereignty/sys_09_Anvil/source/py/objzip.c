 

#include <stdlib.h>
#include <assert.h>

#include "py/objtuple.h"
#include "py/runtime.h"

typedef struct _mp_obj_zip_t {
    mp_obj_base_t base;
    size_t n_iters;
    mp_obj_t iters[];
} mp_obj_zip_t;

static mp_obj_t zip_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, MP_OBJ_FUN_ARGS_MAX, false);

    mp_obj_zip_t *o = mp_obj_malloc_var(mp_obj_zip_t, iters, mp_obj_t, n_args, type);
    o->n_iters = n_args;
    for (size_t i = 0; i < n_args; i++) {
        o->iters[i] = mp_getiter(args[i], NULL);
    }
    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t zip_iternext(mp_obj_t self_in) {
    mp_check_self(mp_obj_is_type(self_in, &mp_type_zip));
    mp_obj_zip_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->n_iters == 0) {
        return MP_OBJ_STOP_ITERATION;
    }
    mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(mp_obj_new_tuple(self->n_iters, NULL));

    for (size_t i = 0; i < self->n_iters; i++) {
        mp_obj_t next = mp_iternext(self->iters[i]);
        if (next == MP_OBJ_STOP_ITERATION) {
            mp_obj_tuple_del(MP_OBJ_FROM_PTR(tuple));
            return MP_OBJ_STOP_ITERATION;
        }
        tuple->items[i] = next;
    }
    return MP_OBJ_FROM_PTR(tuple);
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_zip,
    MP_QSTR_zip,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    make_new, zip_make_new,
    iter, zip_iternext
    );
