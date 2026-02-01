 

#include "py/obj.h"

#if MICROPY_ERROR_REPORTING == MICROPY_ERROR_REPORTING_DETAILED
static void cell_print(const mp_print_t *print, mp_obj_t o_in, mp_print_kind_t kind) {
    (void)kind;
    mp_obj_cell_t *o = MP_OBJ_TO_PTR(o_in);
    mp_printf(print, "<cell %p ", o->obj);
    if (o->obj == MP_OBJ_NULL) {
        mp_print_str(print, "(nil)");
    } else {
        mp_obj_print_helper(print, o->obj, PRINT_REPR);
    }
    mp_print_str(print, ">");
}
#endif

#if MICROPY_ERROR_REPORTING == MICROPY_ERROR_REPORTING_DETAILED
#define CELL_TYPE_PRINT , print, cell_print
#else
#define CELL_TYPE_PRINT
#endif

static MP_DEFINE_CONST_OBJ_TYPE(
    
    mp_type_cell, MP_QSTR_, MP_TYPE_FLAG_NONE
    CELL_TYPE_PRINT
    );

mp_obj_t mp_obj_new_cell(mp_obj_t obj) {
    mp_obj_cell_t *o = mp_obj_malloc(mp_obj_cell_t, &mp_type_cell);
    o->obj = obj;
    return MP_OBJ_FROM_PTR(o);
}
