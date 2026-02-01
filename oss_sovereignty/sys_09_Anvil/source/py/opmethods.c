 

#include "py/obj.h"
#include "py/builtin.h"

static mp_obj_t op_getitem(mp_obj_t self_in, mp_obj_t key_in) {
    const mp_obj_type_t *type = mp_obj_get_type(self_in);
    
    return MP_OBJ_TYPE_GET_SLOT(type, subscr)(self_in, key_in, MP_OBJ_SENTINEL);
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_op_getitem_obj, op_getitem);

static mp_obj_t op_setitem(mp_obj_t self_in, mp_obj_t key_in, mp_obj_t value_in) {
    const mp_obj_type_t *type = mp_obj_get_type(self_in);
    
    return MP_OBJ_TYPE_GET_SLOT(type, subscr)(self_in, key_in, value_in);
}
MP_DEFINE_CONST_FUN_OBJ_3(mp_op_setitem_obj, op_setitem);

static mp_obj_t op_delitem(mp_obj_t self_in, mp_obj_t key_in) {
    const mp_obj_type_t *type = mp_obj_get_type(self_in);
    
    return MP_OBJ_TYPE_GET_SLOT(type, subscr)(self_in, key_in, MP_OBJ_NULL);
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_op_delitem_obj, op_delitem);

static mp_obj_t op_contains(mp_obj_t lhs_in, mp_obj_t rhs_in) {
    const mp_obj_type_t *type = mp_obj_get_type(lhs_in);
    
    return MP_OBJ_TYPE_GET_SLOT(type, binary_op)(MP_BINARY_OP_CONTAINS, lhs_in, rhs_in);
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_op_contains_obj, op_contains);
