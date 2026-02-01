 

#include <assert.h>
#include <string.h>

#include "py/obj.h"
#include "py/mperrno.h"

#if MICROPY_PY_ERRNO



#ifndef MICROPY_PY_ERRNO_LIST
#define MICROPY_PY_ERRNO_LIST \
    X(EPERM) \
    X(ENOENT) \
    X(EIO) \
    X(EBADF) \
    X(EAGAIN) \
    X(ENOMEM) \
    X(EACCES) \
    X(EEXIST) \
    X(ENODEV) \
    X(EISDIR) \
    X(EINVAL) \
    X(EOPNOTSUPP) \
    X(EADDRINUSE) \
    X(ECONNABORTED) \
    X(ECONNRESET) \
    X(ENOBUFS) \
    X(ENOTCONN) \
    X(ETIMEDOUT) \
    X(ECONNREFUSED) \
    X(EHOSTUNREACH) \
    X(EALREADY) \
    X(EINPROGRESS) \

#endif

#if MICROPY_PY_ERRNO_ERRORCODE
static const mp_rom_map_elem_t errorcode_table[] = {
    #define X(e) { MP_ROM_INT(MP_##e), MP_ROM_QSTR(MP_QSTR_##e) },
    MICROPY_PY_ERRNO_LIST
#undef X
};

static const mp_obj_dict_t errorcode_dict = {
    .base = {&mp_type_dict},
    .map = {
        .all_keys_are_qstrs = 0, 
        .is_fixed = 1,
        .is_ordered = 1,
        .used = MP_ARRAY_SIZE(errorcode_table),
        .alloc = MP_ARRAY_SIZE(errorcode_table),
        .table = (mp_map_elem_t *)(mp_rom_map_elem_t *)errorcode_table,
    },
};
#endif

static const mp_rom_map_elem_t mp_module_errno_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_errno) },
    #if MICROPY_PY_ERRNO_ERRORCODE
    { MP_ROM_QSTR(MP_QSTR_errorcode), MP_ROM_PTR(&errorcode_dict) },
    #endif

    #define X(e) { MP_ROM_QSTR(MP_QSTR_##e), MP_ROM_INT(MP_##e) },
    MICROPY_PY_ERRNO_LIST
#undef X
};

static MP_DEFINE_CONST_DICT(mp_module_errno_globals, mp_module_errno_globals_table);

const mp_obj_module_t mp_module_errno = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_errno_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_errno, mp_module_errno);

qstr mp_errno_to_str(mp_obj_t errno_val) {
    #if MICROPY_PY_ERRNO_ERRORCODE
    
    mp_map_elem_t *elem = mp_map_lookup((mp_map_t *)&errorcode_dict.map, errno_val, MP_MAP_LOOKUP);
    if (elem == NULL) {
        return MP_QSTRnull;
    } else {
        return MP_OBJ_QSTR_VALUE(elem->value);
    }
    #else
    
    for (size_t i = 0; i < MP_ARRAY_SIZE(mp_module_errno_globals_table); ++i) {
        if (errno_val == mp_module_errno_globals_table[i].value) {
            return MP_OBJ_QSTR_VALUE(mp_module_errno_globals_table[i].key);
        }
    }
    return MP_QSTRnull;
    #endif
}

#endif 
