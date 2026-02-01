 

#include "py/mpstate.h"

#if MICROPY_DYNAMIC_COMPILER
mp_dynamic_compiler_t mp_dynamic_compiler = {0};
#endif

mp_state_ctx_t mp_state_ctx;
