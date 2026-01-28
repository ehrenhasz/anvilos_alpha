#ifndef _ASM_RISCV_CURRENT_H
#define _ASM_RISCV_CURRENT_H
#include <linux/bug.h>
#include <linux/compiler.h>
#ifndef __ASSEMBLY__
struct task_struct;
register struct task_struct *riscv_current_is_tp __asm__("tp");
static __always_inline struct task_struct *get_current(void)
{
	return riscv_current_is_tp;
}
#define current get_current()
register unsigned long current_stack_pointer __asm__("sp");
#endif  
#endif  
