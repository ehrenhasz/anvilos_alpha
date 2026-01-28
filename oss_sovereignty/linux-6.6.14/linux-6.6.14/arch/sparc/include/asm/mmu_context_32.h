#ifndef __SPARC_MMU_CONTEXT_H
#define __SPARC_MMU_CONTEXT_H
#ifndef __ASSEMBLY__
#include <asm-generic/mm_hooks.h>
#define init_new_context init_new_context
int init_new_context(struct task_struct *tsk, struct mm_struct *mm);
#define destroy_context destroy_context
void destroy_context(struct mm_struct *mm);
void switch_mm(struct mm_struct *old_mm, struct mm_struct *mm,
	       struct task_struct *tsk);
#define activate_mm(active_mm, mm) switch_mm((active_mm), (mm), NULL)
#include <asm-generic/mmu_context.h>
#endif  
#endif  
