#ifndef _XTENSA_MMU_CONTEXT_H
#define _XTENSA_MMU_CONTEXT_H
#ifndef CONFIG_MMU
#include <asm/nommu_context.h>
#else
#include <linux/stringify.h>
#include <linux/sched.h>
#include <linux/mm_types.h>
#include <linux/pgtable.h>
#include <asm/vectors.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm-generic/mm_hooks.h>
#include <asm-generic/percpu.h>
#if (XCHAL_HAVE_TLBS != 1)
# error "Linux must have an MMU!"
#endif
DECLARE_PER_CPU(unsigned long, asid_cache);
#define cpu_asid_cache(cpu) per_cpu(asid_cache, cpu)
#define NO_CONTEXT	0
#define ASID_USER_FIRST	4
#define ASID_MASK	((1 << XCHAL_MMU_ASID_BITS) - 1)
#define ASID_INSERT(x)	(0x03020001 | (((x) & ASID_MASK) << 8))
void init_mmu(void);
void init_kio(void);
static inline void set_rasid_register (unsigned long val)
{
	__asm__ __volatile__ (" wsr %0, rasid\n\t"
			      " isync\n" : : "a" (val));
}
static inline unsigned long get_rasid_register (void)
{
	unsigned long tmp;
	__asm__ __volatile__ (" rsr %0, rasid\n\t" : "=a" (tmp));
	return tmp;
}
static inline void get_new_mmu_context(struct mm_struct *mm, unsigned int cpu)
{
	unsigned long asid = cpu_asid_cache(cpu);
	if ((++asid & ASID_MASK) == 0) {
		local_flush_tlb_all();
		asid += ASID_USER_FIRST;
	}
	cpu_asid_cache(cpu) = asid;
	mm->context.asid[cpu] = asid;
	mm->context.cpu = cpu;
}
static inline void get_mmu_context(struct mm_struct *mm, unsigned int cpu)
{
	if (mm) {
		unsigned long asid = mm->context.asid[cpu];
		if (asid == NO_CONTEXT ||
				((asid ^ cpu_asid_cache(cpu)) & ~ASID_MASK))
			get_new_mmu_context(mm, cpu);
	}
}
static inline void activate_context(struct mm_struct *mm, unsigned int cpu)
{
	get_mmu_context(mm, cpu);
	set_rasid_register(ASID_INSERT(mm->context.asid[cpu]));
	invalidate_page_directory();
}
#define init_new_context init_new_context
static inline int init_new_context(struct task_struct *tsk,
		struct mm_struct *mm)
{
	int cpu;
	for_each_possible_cpu(cpu) {
		mm->context.asid[cpu] = NO_CONTEXT;
	}
	mm->context.cpu = -1;
	return 0;
}
static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
	unsigned int cpu = smp_processor_id();
	int migrated = next->context.cpu != cpu;
	if (migrated) {
		__invalidate_icache_all();
		next->context.cpu = cpu;
	}
	if (migrated || prev != next)
		activate_context(next, cpu);
}
#define destroy_context destroy_context
static inline void destroy_context(struct mm_struct *mm)
{
	invalidate_page_directory();
}
#include <asm-generic/mmu_context.h>
#endif  
#endif  
