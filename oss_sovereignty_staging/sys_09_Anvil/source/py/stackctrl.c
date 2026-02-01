 

#include "py/runtime.h"
#include "py/stackctrl.h"

void mp_stack_ctrl_init(void) {
    #if __GNUC__ >= 13
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdangling-pointer"
    #endif
    volatile int stack_dummy;
    MP_STATE_THREAD(stack_top) = (char *)&stack_dummy;
    #if __GNUC__ >= 13
    #pragma GCC diagnostic pop
    #endif
}

void mp_stack_set_top(void *top) {
    MP_STATE_THREAD(stack_top) = top;
}

mp_uint_t mp_stack_usage(void) {
    
    volatile int stack_dummy;
    return MP_STATE_THREAD(stack_top) - (char *)&stack_dummy;
}

#if MICROPY_STACK_CHECK

void mp_stack_set_limit(mp_uint_t limit) {
    MP_STATE_THREAD(stack_limit) = limit;
}

void mp_stack_check(void) {
    if (mp_stack_usage() >= MP_STATE_THREAD(stack_limit)) {
        mp_raise_recursion_depth();
    }
}

#endif 
