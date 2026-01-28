#ifndef _ASM_POWERPC_LIVEPATCH_H
#define _ASM_POWERPC_LIVEPATCH_H
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#ifdef CONFIG_LIVEPATCH_64
static inline void klp_init_thread_info(struct task_struct *p)
{
	task_thread_info(p)->livepatch_sp = end_of_stack(p) + 1;
}
#else
static inline void klp_init_thread_info(struct task_struct *p) { }
#endif
#endif  
