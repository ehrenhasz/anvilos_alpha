

#include "py/mpconfig.h"

#if MICROPY_EMIT_XTENSA


#define GENERIC_ASM_API (1)
#include "py/asmxtensa.h"


#define NLR_BUF_IDX_LOCAL_1 (8) 

#define N_XTENSA (1)
#define EXPORT_FUN(name) emit_native_xtensa_##name
#include "py/emitnative.c"

#endif
