 

#include <stdlib.h>
#include <assert.h>

#include "py/runtime.h"

#if MICROPY_PY_BUILTINS_REVERSED

typedef struct _mp_obj_reversed_t {
    mp_obj_base_t base;
    mp_obj_t seq;           
    mp_uint_t cur_index;    
} mp_obj_reversed_t;

static mp_obj_t reversed_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    
    mp_obj_t dest[2];
    mp_load_method_maybe(args[0], MP_QSTR___reversed__, dest);
    if (dest[0] != MP_OBJ_NULL) {
        return mp_call_method_n_kw(0, 0, dest);
    }

    mp_obj_reversed_t *o = mp_obj_malloc(mp_obj_reversed_t, type);
    o->seq = args[0];
    o->cur_index = mp_obj_get_int(mp_obj_len(args[0])); 

    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t reversed_iternext(mp_obj_t self_in) {
    mp_check_self(mp_obj_is_type(self_in, &mp_type_reversed));
    mp_obj_reversed_t *self = MP_OBJ_TO_PTR(self_in);

    
    if (self->cur_index == 0) {
        return MP_OBJ_STOP_ITERATION;
    }

    
    self->cur_index -= 1;
    return mp_obj_subscr(self->seq, MP_OBJ_NEW_SMALL_INT(self->cur_index), MP_OBJ_SENTINEL);
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_reversed,
    MP_QSTR_reversed,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    make_new, reversed_make_new,
    iter, reversed_iternext
    );

#endif 
