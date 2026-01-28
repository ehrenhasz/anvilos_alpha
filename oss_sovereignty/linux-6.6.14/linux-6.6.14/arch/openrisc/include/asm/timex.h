#ifndef __ASM_OPENRISC_TIMEX_H
#define __ASM_OPENRISC_TIMEX_H
#define get_cycles get_cycles
#include <asm-generic/timex.h>
#include <asm/spr.h>
#include <asm/spr_defs.h>
static inline cycles_t get_cycles(void)
{
	return mfspr(SPR_TTCR);
}
#define get_cycles get_cycles
#define CLOCK_TICK_RATE 1000
#define ARCH_HAS_READ_CURRENT_TIMER
#endif
