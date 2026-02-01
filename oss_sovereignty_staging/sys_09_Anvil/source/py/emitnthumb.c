

#include "py/mpconfig.h"

#if MICROPY_EMIT_THUMB


#define GENERIC_ASM_API (1)
#include "py/asmthumb.h"


#define NLR_BUF_IDX_LOCAL_1 (3) 

#define N_THUMB (1)
#define EXPORT_FUN(name) emit_native_thumb_##name
#include "py/emitnative.c"

#endif
