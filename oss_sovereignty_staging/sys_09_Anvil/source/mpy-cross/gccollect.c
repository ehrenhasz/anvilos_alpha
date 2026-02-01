 

#include <stdio.h>

#include "py/mpstate.h"
#include "py/gc.h"

#include "shared/runtime/gchelper.h"

#if MICROPY_ENABLE_GC

void gc_collect(void) {
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    gc_collect_end();
}

#endif 
