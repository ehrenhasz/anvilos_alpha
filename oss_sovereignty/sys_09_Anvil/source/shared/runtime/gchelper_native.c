 

#include <stdio.h>

#include "py/mpstate.h"
#include "py/gc.h"
#include "shared/runtime/gchelper.h"

#if MICROPY_ENABLE_GC


uintptr_t gc_helper_get_regs_and_sp(uintptr_t *regs);

MP_NOINLINE void gc_helper_collect_regs_and_stack(void) {
    
    gc_helper_regs_t regs;
    uintptr_t sp = gc_helper_get_regs_and_sp(regs);

    
    gc_collect_root((void **)sp, ((uintptr_t)MP_STATE_THREAD(stack_top) - sp) / sizeof(uintptr_t));
}

#endif
