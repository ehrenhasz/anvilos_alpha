 

#include <stdio.h>

#include "py/runtime.h"

#if MICROPY_ENABLE_PYSTACK

void mp_pystack_init(void *start, void *end) {
    MP_STATE_THREAD(pystack_start) = start;
    MP_STATE_THREAD(pystack_end) = end;
    MP_STATE_THREAD(pystack_cur) = start;
}

void *mp_pystack_alloc(size_t n_bytes) {
    n_bytes = (n_bytes + (MICROPY_PYSTACK_ALIGN - 1)) & ~(MICROPY_PYSTACK_ALIGN - 1);
    #if MP_PYSTACK_DEBUG
    n_bytes += MICROPY_PYSTACK_ALIGN;
    #endif
    if (MP_STATE_THREAD(pystack_cur) + n_bytes > MP_STATE_THREAD(pystack_end)) {
        
        mp_raise_type_arg(&mp_type_RuntimeError, MP_OBJ_NEW_QSTR(MP_QSTR_pystack_space_exhausted));
    }
    void *ptr = MP_STATE_THREAD(pystack_cur);
    MP_STATE_THREAD(pystack_cur) += n_bytes;
    #if MP_PYSTACK_DEBUG
    *(size_t *)(MP_STATE_THREAD(pystack_cur) - MICROPY_PYSTACK_ALIGN) = n_bytes;
    #endif
    return ptr;
}

#endif
