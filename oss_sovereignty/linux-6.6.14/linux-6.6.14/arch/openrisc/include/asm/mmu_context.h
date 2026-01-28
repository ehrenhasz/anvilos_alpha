#ifndef __ASM_OPENRISC_MMU_CONTEXT_H
#define __ASM_OPENRISC_MMU_CONTEXT_H
#include <asm-generic/mm_hooks.h>
#define init_new_context init_new_context
extern int init_new_context(struct task_struct *tsk, struct mm_struct *mm);
#define destroy_context destroy_context
extern void destroy_context(struct mm_struct *mm);
extern void switch_mm(struct mm_struct *prev, struct mm_struct *next,
		      struct task_struct *tsk);
#define activate_mm(prev, next) switch_mm((prev), (next), NULL)
extern volatile pgd_t *current_pgd[];  
#include <asm-generic/mmu_context.h>
#endif
