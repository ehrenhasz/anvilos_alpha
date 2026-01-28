#ifndef __ASM_TIMEX_H
#define __ASM_TIMEX_H
#include <asm/arch_timer.h>
#define get_cycles()	arch_timer_read_counter()
#include <asm-generic/timex.h>
#endif
