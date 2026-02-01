 

#include <string.h>

#include "py/obj.h"
#include "py/runtime.h"

typedef struct _mp_obj_closure_t {
    mp_obj_base_t base;
    mp_obj_t fun;
    size_t n_closed;
    mp_obj_t closed[];
} mp_obj_closure_t;

static mp_obj_t closure_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_obj_closure_t *self = MP_OBJ_TO_PTR(self_in);

    

    size_t n_total = self->n_closed + n_args + 2 * n_kw;
    if (n_total <= 5) {
        
        mp_obj_t args2[5];
        memcpy(args2, self->closed, self->n_closed * sizeof(mp_obj_t));
        memcpy(args2 + self->n_closed, args, (n_args + 2 * n_kw) * sizeof(mp_obj_t));
        return mp_call_function_n_kw(self->fun, self->n_closed + n_args, n_kw, args2);
    } else {
        
        mp_obj_t *args2 = m_new(mp_obj_t, n_total);
        memcpy(args2, self->closed, self->n_closed * sizeof(mp_obj_t));
        memcpy(args2 + self->n_closed, args, (n_args + 2 * n_kw) * sizeof(mp_obj_t));
        mp_obj_t res = mp_call_function_n_kw(self->fun, self->n_closed + n_args, n_kw, args2);
        m_del(mp_obj_t, args2, n_total);
        return res;
    }
}

#if MICROPY_ERROR_REPORTING == MICROPY_ERROR_REPORTING_DETAILED
static void closure_print(const mp_print_t *print, mp_obj_t o_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_closure_t *o = MP_OBJ_TO_PTR(o_in);
    mp_print_str(print, "<closure ");
    mp_obj_print_helper(print, o->fun, PRINT_REPR);
    mp_printf(print, " at %p, n_closed=%u ", o, (int)o->n_closed);
    for (size_t i = 0; i < o->n_closed; i++) {
        if (o->closed[i] == MP_OBJ_NULL) {
            mp_print_str(print, "(nil)");
        } else {
            mp_obj_print_helper(print, o->closed[i], PRINT_REPR);
        }
        mp_print_str(print, " ");
    }
    mp_print_str(print, ">");
}
#endif

#if MICROPY_PY_FUNCTION_ATTRS
static void mp_obj_closure_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    
    mp_obj_closure_t *o = MP_OBJ_TO_PTR(self_in);
    mp_load_method_maybe(o->fun, attr, dest);
}
#define CLOSURE_TYPE_ATTR attr, mp_obj_closure_attr,
#else
#define CLOSURE_TYPE_ATTR
#endif

#if MICROPY_ERROR_REPORTING == MICROPY_ERROR_REPORTING_DETAILED
#define CLOSURE_TYPE_PRINT print, closure_print,
#else
#define CLOSURE_TYPE_PRINT
#endif

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_closure,
    MP_QSTR_closure,
    MP_TYPE_FLAG_BINDS_SELF,
    CLOSURE_TYPE_ATTR
    CLOSURE_TYPE_PRINT
    call, closure_call
    );

mp_obj_t mp_obj_new_closure(mp_obj_t fun, size_t n_closed_over, const mp_obj_t *closed) {
    mp_obj_closure_t *o = mp_obj_malloc_var(mp_obj_closure_t, closed, mp_obj_t, n_closed_over, &mp_type_closure);
    o->fun = fun;
    o->n_closed = n_closed_over;
    memcpy(o->closed, closed, n_closed_over * sizeof(mp_obj_t));
    return MP_OBJ_FROM_PTR(o);
}
