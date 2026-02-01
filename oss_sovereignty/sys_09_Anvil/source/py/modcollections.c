 

#include "py/builtin.h"

#if MICROPY_PY_COLLECTIONS

static const mp_rom_map_elem_t mp_module_collections_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_collections) },
    #if MICROPY_PY_COLLECTIONS_DEQUE
    { MP_ROM_QSTR(MP_QSTR_deque), MP_ROM_PTR(&mp_type_deque) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_namedtuple), MP_ROM_PTR(&mp_namedtuple_obj) },
    #if MICROPY_PY_COLLECTIONS_ORDEREDDICT
    { MP_ROM_QSTR(MP_QSTR_OrderedDict), MP_ROM_PTR(&mp_type_ordereddict) },
    #endif
};

static MP_DEFINE_CONST_DICT(mp_module_collections_globals, mp_module_collections_globals_table);

const mp_obj_module_t mp_module_collections = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_collections_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_collections, mp_module_collections);

#endif 
