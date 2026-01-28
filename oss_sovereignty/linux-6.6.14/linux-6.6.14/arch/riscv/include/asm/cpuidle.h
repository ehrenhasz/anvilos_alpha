#ifndef _ASM_RISCV_CPUIDLE_H
#define _ASM_RISCV_CPUIDLE_H
#include <asm/barrier.h>
#include <asm/processor.h>
static inline void cpu_do_idle(void)
{
	mb();
	wait_for_interrupt();
}
#endif
