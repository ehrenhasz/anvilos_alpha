
#ifndef MICROPY_INCLUDED_MODPLATFORM_H
#define MICROPY_INCLUDED_MODPLATFORM_H

#include "py/misc.h"  
#include "py/mpconfig.h"







#if defined(__ARM_ARCH)
#define MICROPY_PLATFORM_ARCH   "arm"
#elif defined(__x86_64__) || defined(_M_X64)
#define MICROPY_PLATFORM_ARCH   "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
#define MICROPY_PLATFORM_ARCH   "x86"
#elif defined(__xtensa__)
#define MICROPY_PLATFORM_ARCH   "xtensa"
#elif defined(__riscv)
#define MICROPY_PLATFORM_ARCH   "riscv"
#else
#define MICROPY_PLATFORM_ARCH   ""
#endif

#if defined(__GNUC__)
#define MICROPY_PLATFORM_COMPILER \
    "GCC " \
    MP_STRINGIFY(__GNUC__) "." \
    MP_STRINGIFY(__GNUC_MINOR__) "." \
    MP_STRINGIFY(__GNUC_PATCHLEVEL__)
#elif defined(__ARMCC_VERSION)
#define MICROPY_PLATFORM_COMPILER \
    "ARMCC " \
    MP_STRINGIFY((__ARMCC_VERSION / 1000000)) "." \
    MP_STRINGIFY((__ARMCC_VERSION / 10000 % 100)) "." \
    MP_STRINGIFY((__ARMCC_VERSION % 10000))
#elif defined(_MSC_VER)
#if defined(_WIN64)
#define MICROPY_PLATFORM_COMPILER_BITS  "64 bit"
#elif defined(_M_IX86)
#define MICROPY_PLATFORM_COMPILER_BITS  "32 bit"
#else
#define MICROPY_PLATFORM_COMPILER_BITS  ""
#endif
#define MICROPY_PLATFORM_COMPILER \
    "MSC v." MP_STRINGIFY(_MSC_VER) " " MICROPY_PLATFORM_COMPILER_BITS
#else
#define MICROPY_PLATFORM_COMPILER       ""
#endif

#if defined(__GLIBC__)
#define MICROPY_PLATFORM_LIBC_LIB       "glibc"
#define MICROPY_PLATFORM_LIBC_VER \
    MP_STRINGIFY(__GLIBC__) "." \
    MP_STRINGIFY(__GLIBC_MINOR__)
#elif defined(__NEWLIB__)
#define MICROPY_PLATFORM_LIBC_LIB       "newlib"
#define MICROPY_PLATFORM_LIBC_VER       _NEWLIB_VERSION
#else
#define MICROPY_PLATFORM_LIBC_LIB       ""
#define MICROPY_PLATFORM_LIBC_VER       ""
#endif

#if defined(__linux)
#define MICROPY_PLATFORM_SYSTEM         "Linux"
#elif defined(__unix__)
#define MICROPY_PLATFORM_SYSTEM         "Unix"
#elif defined(__CYGWIN__)
#define MICROPY_PLATFORM_SYSTEM         "Cygwin"
#elif defined(_WIN32)
#define MICROPY_PLATFORM_SYSTEM         "Windows"
#else
#define MICROPY_PLATFORM_SYSTEM         "MicroPython"
#endif

#ifndef MICROPY_PLATFORM_VERSION
#define MICROPY_PLATFORM_VERSION ""
#endif

#endif 
