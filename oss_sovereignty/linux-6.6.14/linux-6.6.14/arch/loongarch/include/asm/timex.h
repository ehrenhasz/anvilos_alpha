#ifndef _ASM_TIMEX_H
#define _ASM_TIMEX_H
#ifdef __KERNEL__
#include <linux/compiler.h>
#include <asm/cpu.h>
#include <asm/cpu-features.h>
typedef unsigned long cycles_t;
#define get_cycles get_cycles
static inline cycles_t get_cycles(void)
{
	return drdtime();
}
#endif  
#endif  
