
#ifndef MICROPY_INCLUDED_PY_PERSISTENTCODE_H
#define MICROPY_INCLUDED_PY_PERSISTENTCODE_H

#include "py/mpprint.h"
#include "py/reader.h"
#include "py/emitglue.h"





#define MPY_VERSION 6
#define MPY_SUB_VERSION 3




#define MPY_FEATURE_ENCODE_SUB_VERSION(version) (version)
#define MPY_FEATURE_DECODE_SUB_VERSION(feat) ((feat) & 3)


#define MPY_FEATURE_ENCODE_ARCH(arch) ((arch) << 2)
#define MPY_FEATURE_DECODE_ARCH(feat) ((feat) >> 2)


#if MICROPY_EMIT_X86
    #define MPY_FEATURE_ARCH (MP_NATIVE_ARCH_X86)
#elif MICROPY_EMIT_X64
    #define MPY_FEATURE_ARCH (MP_NATIVE_ARCH_X64)
#elif MICROPY_EMIT_THUMB
    #if defined(__thumb2__)
        #if defined(__ARM_FP) && (__ARM_FP & 8) == 8
            #define MPY_FEATURE_ARCH (MP_NATIVE_ARCH_ARMV7EMDP)
        #elif defined(__ARM_FP) && (__ARM_FP & 4) == 4
            #define MPY_FEATURE_ARCH (MP_NATIVE_ARCH_ARMV7EMSP)
        #else
            #define MPY_FEATURE_ARCH (MP_NATIVE_ARCH_ARMV7EM)
        #endif
    #else
        #define MPY_FEATURE_ARCH (MP_NATIVE_ARCH_ARMV6M)
    #endif
    #define MPY_FEATURE_ARCH_TEST(x) (MP_NATIVE_ARCH_ARMV6M <= (x) && (x) <= MPY_FEATURE_ARCH)
#elif MICROPY_EMIT_ARM
    #define MPY_FEATURE_ARCH (MP_NATIVE_ARCH_ARMV6)
#elif MICROPY_EMIT_XTENSA
    #define MPY_FEATURE_ARCH (MP_NATIVE_ARCH_XTENSA)
#elif MICROPY_EMIT_XTENSAWIN
    #define MPY_FEATURE_ARCH (MP_NATIVE_ARCH_XTENSAWIN)
#else
    #define MPY_FEATURE_ARCH (MP_NATIVE_ARCH_NONE)
#endif

#ifndef MPY_FEATURE_ARCH_TEST
#define MPY_FEATURE_ARCH_TEST(x) ((x) == MPY_FEATURE_ARCH)
#endif


#define MPY_FILE_HEADER_INT (MPY_VERSION \
    | (MPY_FEATURE_ENCODE_SUB_VERSION(MPY_SUB_VERSION) | MPY_FEATURE_ENCODE_ARCH(MPY_FEATURE_ARCH)) << 8)

enum {
    MP_NATIVE_ARCH_NONE = 0,
    MP_NATIVE_ARCH_X86,
    MP_NATIVE_ARCH_X64,
    MP_NATIVE_ARCH_ARMV6,
    MP_NATIVE_ARCH_ARMV6M,
    MP_NATIVE_ARCH_ARMV7M,
    MP_NATIVE_ARCH_ARMV7EM,
    MP_NATIVE_ARCH_ARMV7EMSP,
    MP_NATIVE_ARCH_ARMV7EMDP,
    MP_NATIVE_ARCH_XTENSA,
    MP_NATIVE_ARCH_XTENSAWIN,
};

enum {
    MP_PERSISTENT_OBJ_FUN_TABLE = 0,
    MP_PERSISTENT_OBJ_NONE,
    MP_PERSISTENT_OBJ_FALSE,
    MP_PERSISTENT_OBJ_TRUE,
    MP_PERSISTENT_OBJ_ELLIPSIS,
    MP_PERSISTENT_OBJ_STR,
    MP_PERSISTENT_OBJ_BYTES,
    MP_PERSISTENT_OBJ_INT,
    MP_PERSISTENT_OBJ_FLOAT,
    MP_PERSISTENT_OBJ_COMPLEX,
    MP_PERSISTENT_OBJ_TUPLE,
};

void mp_raw_code_load(mp_reader_t *reader, mp_compiled_module_t *ctx);
void mp_raw_code_load_mem(const byte *buf, size_t len, mp_compiled_module_t *ctx);
void mp_raw_code_load_file(qstr filename, mp_compiled_module_t *ctx);

void mp_raw_code_save(mp_compiled_module_t *cm, mp_print_t *print);
void mp_raw_code_save_file(mp_compiled_module_t *cm, qstr filename);

void mp_native_relocate(void *reloc, uint8_t *text, uintptr_t reloc_text);

#endif 
