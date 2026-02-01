 

#include "py/mpstate.h"

#if MICROPY_NLR_SETJMP

void nlr_jump(void *val) {
    MP_NLR_JUMP_HEAD(val, top);
    longjmp(top->jmpbuf, 1);
}

#endif
