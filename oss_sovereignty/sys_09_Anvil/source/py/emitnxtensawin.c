

#include "py/mpconfig.h"

#if MICROPY_EMIT_XTENSAWIN


#define GENERIC_ASM_API (1)
#define GENERIC_ASM_API_WIN (1)
#include "py/asmxtensa.h"


#define NLR_BUF_IDX_LOCAL_1 (2 + 4) 

#define N_NLR_SETJMP (1)
#define N_XTENSAWIN (1)
#define EXPORT_FUN(name) emit_native_xtensawin_##name
#include "py/emitnative.c"

#endif
