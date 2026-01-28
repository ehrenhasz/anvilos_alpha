#ifndef _ASM_POWERPC_TIMEX_H
#define _ASM_POWERPC_TIMEX_H
#ifdef __KERNEL__
#include <asm/cputable.h>
#include <asm/vdso/timebase.h>
#define CLOCK_TICK_RATE	1024000  
typedef unsigned long cycles_t;
static inline cycles_t get_cycles(void)
{
	return mftb();
}
#define get_cycles get_cycles
#endif	 
#endif	 
