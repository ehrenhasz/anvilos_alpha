#ifndef _ASM_ARC_MMU_CONTEXT_H
#define _ASM_ARC_MMU_CONTEXT_H
#include <linux/sched/mm.h>
#include <asm/tlb.h>
#include <asm-generic/mm_hooks.h>
#define MM_CTXT_ASID_MASK	0x000000ff  
#define MM_CTXT_CYCLE_MASK	(~MM_CTXT_ASID_MASK)
#define MM_CTXT_FIRST_CYCLE	(MM_CTXT_ASID_MASK + 1)
#define MM_CTXT_NO_ASID		0UL
#define asid_mm(mm, cpu)	mm->context.asid[cpu]
#define hw_pid(mm, cpu)		(asid_mm(mm, cpu) & MM_CTXT_ASID_MASK)
DECLARE_PER_CPU(unsigned int, asid_cache);
#define asid_cpu(cpu)		per_cpu(asid_cache, cpu)
static inline void get_new_mmu_context(struct mm_struct *mm)
{
	const unsigned int cpu = smp_processor_id();
	unsigned long flags;
	local_irq_save(flags);
	if (!((asid_mm(mm, cpu) ^ asid_cpu(cpu)) & MM_CTXT_CYCLE_MASK))
		goto set_hw;
	if (unlikely(!(++asid_cpu(cpu) & MM_CTXT_ASID_MASK))) {
		local_flush_tlb_all();
		if (!asid_cpu(cpu))
			asid_cpu(cpu) = MM_CTXT_FIRST_CYCLE;
	}
	asid_mm(mm, cpu) = asid_cpu(cpu);
set_hw:
	mmu_setup_asid(mm, hw_pid(mm, cpu));
	local_irq_restore(flags);
}
#define init_new_context init_new_context
static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	int i;
	for_each_possible_cpu(i)
		asid_mm(mm, i) = MM_CTXT_NO_ASID;
	return 0;
}
#define destroy_context destroy_context
static inline void destroy_context(struct mm_struct *mm)
{
	unsigned long flags;
	local_irq_save(flags);
	asid_mm(mm, smp_processor_id()) = MM_CTXT_NO_ASID;
	local_irq_restore(flags);
}
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
	const int cpu = smp_processor_id();
	cpumask_set_cpu(cpu, mm_cpumask(next));
	mmu_setup_pgd(next, next->pgd);
	get_new_mmu_context(next);
}
#include <asm-generic/mmu_context.h>
#endif  
