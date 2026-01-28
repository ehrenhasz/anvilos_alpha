
#ifndef MICROPY_INCLUDED_PY_STACKCTRL_H
#define MICROPY_INCLUDED_PY_STACKCTRL_H

#include "py/mpconfig.h"

void mp_stack_ctrl_init(void);
void mp_stack_set_top(void *top);
mp_uint_t mp_stack_usage(void);

#if MICROPY_STACK_CHECK

void mp_stack_set_limit(mp_uint_t limit);
void mp_stack_check(void);
#define MP_STACK_CHECK() mp_stack_check()

#else

#define mp_stack_set_limit(limit) (void)(limit)
#define MP_STACK_CHECK()

#endif

#endif 
