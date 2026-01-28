
#ifndef MICROPY_INCLUDED_EXTMOD_MISC_H
#define MICROPY_INCLUDED_EXTMOD_MISC_H



#include <stddef.h>
#include "py/runtime.h"

MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_dupterm_obj);

#if MICROPY_PY_OS_DUPTERM
bool mp_os_dupterm_is_builtin_stream(mp_const_obj_t stream);
void mp_os_dupterm_stream_detached_attached(mp_obj_t stream_detached, mp_obj_t stream_attached);
uintptr_t mp_os_dupterm_poll(uintptr_t poll_flags);
int mp_os_dupterm_rx_chr(void);
int mp_os_dupterm_tx_strn(const char *str, size_t len);
void mp_os_deactivate(size_t dupterm_idx, const char *msg, mp_obj_t exc);
#else
static inline int mp_os_dupterm_tx_strn(const char *s, size_t l) {
    return -1;
}
#endif

#endif 
