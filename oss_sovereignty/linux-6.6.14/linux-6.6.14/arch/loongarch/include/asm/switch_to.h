#ifndef _ASM_SWITCH_TO_H
#define _ASM_SWITCH_TO_H
#include <asm/cpu-features.h>
#include <asm/fpu.h>
#include <asm/lbt.h>
struct task_struct;
extern asmlinkage struct task_struct *__switch_to(struct task_struct *prev,
			struct task_struct *next, struct thread_info *next_ti,
			void *sched_ra, void *sched_cfa);
#define switch_to(prev, next, last)						\
do {										\
	lose_fpu_inatomic(1, prev);						\
	lose_lbt_inatomic(1, prev);						\
	hw_breakpoint_thread_switch(next);					\
	(last) = __switch_to(prev, next, task_thread_info(next),		\
		 __builtin_return_address(0), __builtin_frame_address(0));	\
} while (0)
#endif  
