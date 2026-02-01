

#include "py/mpconfig.h"

#if MICROPY_EMIT_X64


#define GENERIC_ASM_API (1)
#include "py/asmx64.h"


#define NLR_BUF_IDX_LOCAL_1 (5) 

#define N_X64 (1)
#define EXPORT_FUN(name) emit_native_x64_##name
#include "py/emitnative.c"

#endif
