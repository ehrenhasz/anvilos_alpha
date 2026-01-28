#ifndef _ASM_TIMEX_H
#define _ASM_TIMEX_H
#include <asm-generic/timex.h>
#include <asm/hexagon_vm.h>
#define CLOCK_TICK_RATE              19200
#define ARCH_HAS_READ_CURRENT_TIMER
static inline int read_current_timer(unsigned long *timer_val)
{
	*timer_val = __vmgettime();
	return 0;
}
#endif
