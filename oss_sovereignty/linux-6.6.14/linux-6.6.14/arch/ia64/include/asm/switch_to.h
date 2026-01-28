#ifndef _ASM_IA64_SWITCH_TO_H
#define _ASM_IA64_SWITCH_TO_H
#include <linux/percpu.h>
struct task_struct;
extern struct task_struct *ia64_switch_to (void *next_task);
extern void ia64_save_extra (struct task_struct *task);
extern void ia64_load_extra (struct task_struct *task);
#define IA64_HAS_EXTRA_STATE(t)							\
	((t)->thread.flags & (IA64_THREAD_DBG_VALID|IA64_THREAD_PM_VALID))
#define __switch_to(prev,next,last) do {							 \
	if (IA64_HAS_EXTRA_STATE(prev))								 \
		ia64_save_extra(prev);								 \
	if (IA64_HAS_EXTRA_STATE(next))								 \
		ia64_load_extra(next);								 \
	ia64_psr(task_pt_regs(next))->dfh = !ia64_is_local_fpu_owner(next);			 \
	(last) = ia64_switch_to((next));							 \
} while (0)
#ifdef CONFIG_SMP
# define switch_to(prev,next,last) do {						\
	if (ia64_psr(task_pt_regs(prev))->mfh && ia64_is_local_fpu_owner(prev)) {				\
		ia64_psr(task_pt_regs(prev))->mfh = 0;			\
		(prev)->thread.flags |= IA64_THREAD_FPH_VALID;			\
		__ia64_save_fpu((prev)->thread.fph);				\
	}									\
	__switch_to(prev, next, last);						\
	 			\
	if (unlikely((current->thread.flags & IA64_THREAD_MIGRATION) &&	       \
		     (task_cpu(current) !=				       \
		      		      task_thread_info(current)->last_cpu))) { \
		task_thread_info(current)->last_cpu = task_cpu(current);       \
	}								       \
} while (0)
#else
# define switch_to(prev,next,last)	__switch_to(prev, next, last)
#endif
#endif  
