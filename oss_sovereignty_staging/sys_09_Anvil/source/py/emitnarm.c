

#include "py/mpconfig.h"

#if MICROPY_EMIT_ARM


#define GENERIC_ASM_API (1)
#include "py/asmarm.h"


#define NLR_BUF_IDX_LOCAL_1 (3) 

#define N_ARM (1)
#define EXPORT_FUN(name) emit_native_arm_##name
#include "py/emitnative.c"

#endif
