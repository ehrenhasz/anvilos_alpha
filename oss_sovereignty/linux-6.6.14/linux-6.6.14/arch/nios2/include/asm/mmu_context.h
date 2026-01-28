#ifndef _ASM_NIOS2_MMU_CONTEXT_H
#define _ASM_NIOS2_MMU_CONTEXT_H
#include <linux/mm_types.h>
#include <asm-generic/mm_hooks.h>
extern void mmu_context_init(void);
extern unsigned long get_pid_from_context(mm_context_t *ctx);
extern pgd_t *pgd_current;
#define init_new_context init_new_context
static inline int init_new_context(struct task_struct *tsk,
					struct mm_struct *mm)
{
	mm->context = 0;
	return 0;
}
void switch_mm(struct mm_struct *prev, struct mm_struct *next,
		struct task_struct *tsk);
#define activate_mm activate_mm
void activate_mm(struct mm_struct *prev, struct mm_struct *next);
#include <asm-generic/mmu_context.h>
#endif  
