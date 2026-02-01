 

#include <stdlib.h>

#include "py/runtime.h"



typedef struct _mp_obj_getitem_iter_t {
    mp_obj_base_t base;
    mp_obj_t args[3];
} mp_obj_getitem_iter_t;

static mp_obj_t it_iternext(mp_obj_t self_in) {
    mp_obj_getitem_iter_t *self = MP_OBJ_TO_PTR(self_in);
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        
        mp_obj_t value = mp_call_method_n_kw(1, 0, self->args);
        self->args[2] = MP_OBJ_NEW_SMALL_INT(MP_OBJ_SMALL_INT_VALUE(self->args[2]) + 1);
        nlr_pop();
        return value;
    } else {
        
        mp_obj_type_t *t = (mp_obj_type_t *)((mp_obj_base_t *)nlr.ret_val)->type;
        if (t == &mp_type_StopIteration || t == &mp_type_IndexError) {
            return MP_OBJ_STOP_ITERATION;
        } else {
            
            nlr_jump(nlr.ret_val);
        }
    }
}

static MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_it,
    MP_QSTR_iterator,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    iter, it_iternext
    );


mp_obj_t mp_obj_new_getitem_iter(mp_obj_t *args, mp_obj_iter_buf_t *iter_buf) {
    assert(sizeof(mp_obj_getitem_iter_t) <= sizeof(mp_obj_iter_buf_t));
    mp_obj_getitem_iter_t *o = (mp_obj_getitem_iter_t *)iter_buf;
    o->base.type = &mp_type_it;
    o->args[0] = args[0];
    o->args[1] = args[1];
    o->args[2] = MP_OBJ_NEW_SMALL_INT(0);
    return MP_OBJ_FROM_PTR(o);
}
