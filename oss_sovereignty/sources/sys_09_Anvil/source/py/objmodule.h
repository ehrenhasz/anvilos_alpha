
#ifndef MICROPY_INCLUDED_PY_OBJMODULE_H
#define MICROPY_INCLUDED_PY_OBJMODULE_H

#include "py/obj.h"

#ifndef NO_QSTR


#include "genhdr/moduledefs.h"
#endif

extern const mp_map_t mp_builtin_module_map;
extern const mp_map_t mp_builtin_extensible_module_map;

mp_obj_t mp_module_get_builtin(qstr module_name, bool extensible);

void mp_module_generic_attr(qstr attr, mp_obj_t *dest, const uint16_t *keys, mp_obj_t *values);

#endif 
