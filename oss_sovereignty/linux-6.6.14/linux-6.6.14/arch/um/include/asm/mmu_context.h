#ifndef __UM_MMU_CONTEXT_H
#define __UM_MMU_CONTEXT_H
#include <linux/sched.h>
#include <linux/mm_types.h>
#include <linux/mmap_lock.h>
#include <asm/mm_hooks.h>
#include <asm/mmu.h>
extern void force_flush_all(void);
#define activate_mm activate_mm
static inline void activate_mm(struct mm_struct *old, struct mm_struct *new)
{
	__switch_mm(&new->context.id);
}
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next, 
			     struct task_struct *tsk)
{
	unsigned cpu = smp_processor_id();
	if(prev != next){
		cpumask_clear_cpu(cpu, mm_cpumask(prev));
		cpumask_set_cpu(cpu, mm_cpumask(next));
		if(next != &init_mm)
			__switch_mm(&next->context.id);
	}
}
#define init_new_context init_new_context
extern int init_new_context(struct task_struct *task, struct mm_struct *mm);
#define destroy_context destroy_context
extern void destroy_context(struct mm_struct *mm);
#include <asm-generic/mmu_context.h>
#endif
