 
#ifndef MICROPY_INCLUDED_PY_FROZENMOD_H
#define MICROPY_INCLUDED_PY_FROZENMOD_H

#include "py/builtin.h"

enum {
    MP_FROZEN_NONE,
    MP_FROZEN_STR,
    MP_FROZEN_MPY,
};

mp_import_stat_t mp_find_frozen_module(const char *str, int *frozen_type, void **data);

#endif 
