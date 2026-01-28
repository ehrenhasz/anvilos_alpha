#ifndef _ASM_MMU_CONTEXT_H
#define _ASM_MMU_CONTEXT_H
#include <linux/mm_types.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/mem-layout.h>
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
				struct task_struct *tsk)
{
	int l1;
	if (next->context.generation < prev->context.generation) {
		for (l1 = MIN_KERNEL_SEG; l1 <= max_kernel_seg; l1++)
			next->pgd[l1] = init_mm.pgd[l1];
		next->context.generation = prev->context.generation;
	}
	__vmnewmap((void *)next->context.ptbase);
}
#define activate_mm activate_mm
static inline void activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	unsigned long flags;
	local_irq_save(flags);
	switch_mm(prev, next, current_thread_info()->task);
	local_irq_restore(flags);
}
#include <asm-generic/mm_hooks.h>
#include <asm-generic/mmu_context.h>
#endif
