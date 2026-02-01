 
#ifndef MICROPY_INCLUDED_PY_REPL_H
#define MICROPY_INCLUDED_PY_REPL_H

#include "py/mpconfig.h"
#include "py/misc.h"
#include "py/mpprint.h"

#if MICROPY_HELPER_REPL

#if MICROPY_PY_SYS_PS1_PS2

const char *mp_repl_get_psx(unsigned int entry);

static inline const char *mp_repl_get_ps1(void) {
    return mp_repl_get_psx(MP_SYS_MUTABLE_PS1);
}

static inline const char *mp_repl_get_ps2(void) {
    return mp_repl_get_psx(MP_SYS_MUTABLE_PS2);
}

#else

static inline const char *mp_repl_get_ps1(void) {
    return ">>> ";
}

static inline const char *mp_repl_get_ps2(void) {
    return "... ";
}

#endif

bool mp_repl_continue_with_input(const char *input);
size_t mp_repl_autocomplete(const char *str, size_t len, const mp_print_t *print, const char **compl_str);

#endif

#endif 
