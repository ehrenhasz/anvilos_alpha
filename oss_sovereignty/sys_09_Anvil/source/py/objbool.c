 

#include <stdlib.h>

#include "py/runtime.h"

#if MICROPY_OBJ_IMMEDIATE_OBJS

#define BOOL_VALUE(o) ((o) == mp_const_false ? 0 : 1)

#else

#define BOOL_VALUE(o) (((mp_obj_bool_t *)MP_OBJ_TO_PTR(o))->value)

typedef struct _mp_obj_bool_t {
    mp_obj_base_t base;
    bool value;
} mp_obj_bool_t;

#endif

static void bool_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    bool value = BOOL_VALUE(self_in);
    if (MICROPY_PY_JSON && kind == PRINT_JSON) {
        if (value) {
            mp_print_str(print, "true");
        } else {
            mp_print_str(print, "false");
        }
    } else {
        if (value) {
            mp_print_str(print, "True");
        } else {
            mp_print_str(print, "False");
        }
    }
}

static mp_obj_t bool_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type_in;
    mp_arg_check_num(n_args, n_kw, 0, 1, false);

    if (n_args == 0) {
        return mp_const_false;
    } else {
        return mp_obj_new_bool(mp_obj_is_true(args[0]));
    }
}

static mp_obj_t bool_unary_op(mp_unary_op_t op, mp_obj_t o_in) {
    if (op == MP_UNARY_OP_LEN) {
        return MP_OBJ_NULL;
    }
    bool value = BOOL_VALUE(o_in);
    return mp_unary_op(op, MP_OBJ_NEW_SMALL_INT(value));
}

static mp_obj_t bool_binary_op(mp_binary_op_t op, mp_obj_t lhs_in, mp_obj_t rhs_in) {
    bool value = BOOL_VALUE(lhs_in);
    return mp_binary_op(op, MP_OBJ_NEW_SMALL_INT(value), rhs_in);
}

MP_DEFINE_CONST_OBJ_TYPE(
    
    mp_type_bool,
    MP_QSTR_bool,
    MP_TYPE_FLAG_EQ_CHECKS_OTHER_TYPE,
    make_new, bool_make_new,
    print, bool_print,
    unary_op, bool_unary_op,
    binary_op, bool_binary_op
    );

#if !MICROPY_OBJ_IMMEDIATE_OBJS
const mp_obj_bool_t mp_const_false_obj = {{&mp_type_bool}, false};
const mp_obj_bool_t mp_const_true_obj = {{&mp_type_bool}, true};
#endif
