 

#include "py/builtin.h"

#if MICROPY_PY_ARRAY

static const mp_rom_map_elem_t mp_module_array_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_array) },
    { MP_ROM_QSTR(MP_QSTR_array), MP_ROM_PTR(&mp_type_array) },
};

static MP_DEFINE_CONST_DICT(mp_module_array_globals, mp_module_array_globals_table);

const mp_obj_module_t mp_module_array = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_array_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_array, mp_module_array);

#endif
