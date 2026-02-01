 

#include <stdarg.h>
#include <stdio.h>

#include "py/emit.h"
#include "py/runtime.h"

#if MICROPY_WARNINGS

void mp_warning(const char *category, const char *msg, ...) {
    if (category == NULL) {
        category = "Warning";
    }
    mp_print_str(MICROPY_ERROR_PRINTER, category);
    mp_print_str(MICROPY_ERROR_PRINTER, ": ");

    va_list args;
    va_start(args, msg);
    mp_vprintf(MICROPY_ERROR_PRINTER, msg, args);
    mp_print_str(MICROPY_ERROR_PRINTER, "\n");
    va_end(args);
}

void mp_emitter_warning(pass_kind_t pass, const char *msg) {
    if (pass == MP_PASS_CODE_SIZE) {
        mp_warning(NULL, msg);
    }
}

#endif 
