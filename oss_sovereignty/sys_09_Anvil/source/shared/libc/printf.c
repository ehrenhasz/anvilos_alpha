 

#include "py/mpconfig.h"

#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "py/obj.h"
#include "py/mphal.h"

#if MICROPY_PY_BUILTINS_FLOAT
#include "py/formatfloat.h"
#endif

#if MICROPY_DEBUG_PRINTERS
int DEBUG_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = mp_vprintf(MICROPY_DEBUG_PRINTER, fmt, ap);
    va_end(ap);
    return ret;
}
#endif

#if MICROPY_USE_INTERNAL_PRINTF

#undef putchar  
int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int putchar(int c);
int puts(const char *s);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
int snprintf(char *str, size_t size, const char *fmt, ...);

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = mp_vprintf(MICROPY_INTERNAL_PRINTF_PRINTER, fmt, ap);
    va_end(ap);
    return ret;
}

int vprintf(const char *fmt, va_list ap) {
    return mp_vprintf(MICROPY_INTERNAL_PRINTF_PRINTER, fmt, ap);
}


int putchar(int c) {
    char chr = c;
    MICROPY_INTERNAL_PRINTF_PRINTER->print_strn(MICROPY_INTERNAL_PRINTF_PRINTER->data, &chr, 1);
    return chr;
}


int puts(const char *s) {
    MICROPY_INTERNAL_PRINTF_PRINTER->print_strn(MICROPY_INTERNAL_PRINTF_PRINTER->data, s, strlen(s));
    return putchar('\n'); 
}

typedef struct _strn_print_env_t {
    char *cur;
    size_t remain;
} strn_print_env_t;

static void strn_print_strn(void *data, const char *str, size_t len) {
    strn_print_env_t *strn_print_env = data;
    if (len > strn_print_env->remain) {
        len = strn_print_env->remain;
    }
    memcpy(strn_print_env->cur, str, len);
    strn_print_env->cur += len;
    strn_print_env->remain -= len;
}

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 9




int __GI_vsnprintf(char *str, size_t size, const char *fmt, va_list ap) __attribute__((weak, alias("vsnprintf")));
#endif

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    strn_print_env_t strn_print_env = {str, size};
    mp_print_t print = {&strn_print_env, strn_print_strn};
    int len = mp_vprintf(&print, fmt, ap);
    
    if (size > 0) {
        if (strn_print_env.remain == 0) {
            strn_print_env.cur[-1] = 0;
        } else {
            strn_print_env.cur[0] = 0;
        }
    }
    return len;
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return ret;
}

#endif 
