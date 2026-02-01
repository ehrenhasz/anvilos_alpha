 

#include <stdlib.h>

#include "py/runtime.h"







typedef struct _mp_obj_polymorph_iter_t {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
} mp_obj_polymorph_iter_t;

static mp_obj_t polymorph_it_iternext(mp_obj_t self_in) {
    mp_obj_polymorph_iter_t *self = MP_OBJ_TO_PTR(self_in);
    
    return self->iternext(self_in);
}

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_polymorph_iter,
    MP_QSTR_iterator,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    iter, polymorph_it_iternext
    );

#if MICROPY_ENABLE_FINALISER





typedef struct _mp_obj_polymorph_iter_with_finaliser_t {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
    mp_fun_1_t finaliser;
} mp_obj_polymorph_with_finaliser_iter_t;

static mp_obj_t mp_obj_polymorph_iter_del(mp_obj_t self_in) {
    mp_obj_polymorph_with_finaliser_iter_t *self = MP_OBJ_TO_PTR(self_in);
    
    return self->finaliser(self_in);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_obj_polymorph_iter_del_obj, mp_obj_polymorph_iter_del);

static const mp_rom_map_elem_t mp_obj_polymorph_iter_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mp_obj_polymorph_iter_del_obj) },
};
static MP_DEFINE_CONST_DICT(mp_obj_polymorph_iter_locals_dict, mp_obj_polymorph_iter_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_polymorph_iter_with_finaliser,
    MP_QSTR_iterator,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    iter, polymorph_it_iternext,
    locals_dict, &mp_obj_polymorph_iter_locals_dict
    );
#endif
