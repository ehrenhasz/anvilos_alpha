#include <stdint.h>

// Minimal configuration for AnvilOS Bare Metal
#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_MINIMUM)
#define MICROPY_ENABLE_COMPILER     (1)
#define MICROPY_ENABLE_GC           (1)
#define MICROPY_HELPER_REPL         (1)
#define MICROPY_MODULE_FROZEN_MPY   (0)
// Use a named pool (defined in bridge.c)
#define MICROPY_QSTR_EXTRA_POOL     mp_qstr_null_pool

// Disable Readline to avoid history complexity
#define MICROPY_USE_READLINE        (0)
#define MICROPY_USE_READLINE_HISTORY (0)

// Type definitions for x86 (32-bit for now to match substrate)
typedef intptr_t mp_int_t;
typedef uintptr_t mp_uint_t;
typedef long mp_off_t;

// We need alloca
#ifndef alloca
#define alloca(x) __builtin_alloca(x)
#endif

// Hardware Name
#define MICROPY_HW_BOARD_NAME "AnvilOS-Zero-C"
#define MICROPY_HW_MCU_NAME   "x86-BareMetal"

// Heap Size (Static)
#define MICROPY_HEAP_SIZE      (64 * 1024) 

#define MP_STATE_PORT MP_STATE_VM