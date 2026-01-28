#ifndef _XTENSA_CURRENT_H
#define _XTENSA_CURRENT_H
#include <asm/thread_info.h>
#ifndef __ASSEMBLY__
#include <linux/thread_info.h>
struct task_struct;
static inline struct task_struct *get_current(void)
{
	return current_thread_info()->task;
}
#define current get_current()
register unsigned long current_stack_pointer __asm__("a1");
#else
#define GET_CURRENT(reg,sp)		\
	GET_THREAD_INFO(reg,sp);	\
	l32i reg, reg, TI_TASK		\
#endif
#endif  
