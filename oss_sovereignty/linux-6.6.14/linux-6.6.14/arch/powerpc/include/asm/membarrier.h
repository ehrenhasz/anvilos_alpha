#ifndef _ASM_POWERPC_MEMBARRIER_H
#define _ASM_POWERPC_MEMBARRIER_H
static inline void membarrier_arch_switch_mm(struct mm_struct *prev,
					     struct mm_struct *next,
					     struct task_struct *tsk)
{
	if (IS_ENABLED(CONFIG_SMP) &&
	    likely(!(atomic_read(&next->membarrier_state) &
		     (MEMBARRIER_STATE_PRIVATE_EXPEDITED |
		      MEMBARRIER_STATE_GLOBAL_EXPEDITED)) || !prev))
		return;
	smp_mb();
}
#endif  
