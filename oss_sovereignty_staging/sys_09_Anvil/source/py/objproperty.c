 

#include <stdlib.h>
#include <assert.h>

#include "py/runtime.h"

#if MICROPY_PY_BUILTINS_PROPERTY

typedef struct _mp_obj_property_t {
    mp_obj_base_t base;
    mp_obj_t proxy[3]; 
} mp_obj_property_t;

static mp_obj_t property_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    enum { ARG_fget, ARG_fset, ARG_fdel, ARG_doc };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_doc, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    mp_obj_property_t *o = mp_obj_malloc(mp_obj_property_t, type);
    o->proxy[0] = vals[ARG_fget].u_obj;
    o->proxy[1] = vals[ARG_fset].u_obj;
    o->proxy[2] = vals[ARG_fdel].u_obj;
    
    return MP_OBJ_FROM_PTR(o);
}

static mp_obj_t property_getter(mp_obj_t self_in, mp_obj_t getter) {
    mp_obj_property_t *p2 = m_new_obj(mp_obj_property_t);
    *p2 = *(mp_obj_property_t *)MP_OBJ_TO_PTR(self_in);
    p2->proxy[0] = getter;
    return MP_OBJ_FROM_PTR(p2);
}

static MP_DEFINE_CONST_FUN_OBJ_2(property_getter_obj, property_getter);

static mp_obj_t property_setter(mp_obj_t self_in, mp_obj_t setter) {
    mp_obj_property_t *p2 = m_new_obj(mp_obj_property_t);
    *p2 = *(mp_obj_property_t *)MP_OBJ_TO_PTR(self_in);
    p2->proxy[1] = setter;
    return MP_OBJ_FROM_PTR(p2);
}

static MP_DEFINE_CONST_FUN_OBJ_2(property_setter_obj, property_setter);

static mp_obj_t property_deleter(mp_obj_t self_in, mp_obj_t deleter) {
    mp_obj_property_t *p2 = m_new_obj(mp_obj_property_t);
    *p2 = *(mp_obj_property_t *)MP_OBJ_TO_PTR(self_in);
    p2->proxy[2] = deleter;
    return MP_OBJ_FROM_PTR(p2);
}

static MP_DEFINE_CONST_FUN_OBJ_2(property_deleter_obj, property_deleter);

static const mp_rom_map_elem_t property_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_getter), MP_ROM_PTR(&property_getter_obj) },
    { MP_ROM_QSTR(MP_QSTR_setter), MP_ROM_PTR(&property_setter_obj) },
    { MP_ROM_QSTR(MP_QSTR_deleter), MP_ROM_PTR(&property_deleter_obj) },
};

static MP_DEFINE_CONST_DICT(property_locals_dict, property_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_property,
    MP_QSTR_property,
    MP_TYPE_FLAG_NONE,
    make_new, property_make_new,
    locals_dict, &property_locals_dict
    );

const mp_obj_t *mp_obj_property_get(mp_obj_t self_in) {
    mp_check_self(mp_obj_is_type(self_in, &mp_type_property));
    mp_obj_property_t *self = MP_OBJ_TO_PTR(self_in);
    return self->proxy;
}

#endif 
