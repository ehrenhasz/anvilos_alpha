#ifndef _ASM_X86_TLBFLUSH_H
#define _ASM_X86_TLBFLUSH_H
#include <linux/mm_types.h>
#include <linux/mmu_notifier.h>
#include <linux/sched.h>
#include <asm/processor.h>
#include <asm/cpufeature.h>
#include <asm/special_insns.h>
#include <asm/smp.h>
#include <asm/invpcid.h>
#include <asm/pti.h>
#include <asm/processor-flags.h>
#include <asm/pgtable.h>
DECLARE_PER_CPU(u64, tlbstate_untag_mask);
void __flush_tlb_all(void);
#define TLB_FLUSH_ALL	-1UL
#define TLB_GENERATION_INVALID	0
void cr4_update_irqsoff(unsigned long set, unsigned long clear);
unsigned long cr4_read_shadow(void);
static inline void cr4_set_bits_irqsoff(unsigned long mask)
{
	cr4_update_irqsoff(mask, 0);
}
static inline void cr4_clear_bits_irqsoff(unsigned long mask)
{
	cr4_update_irqsoff(0, mask);
}
static inline void cr4_set_bits(unsigned long mask)
{
	unsigned long flags;
	local_irq_save(flags);
	cr4_set_bits_irqsoff(mask);
	local_irq_restore(flags);
}
static inline void cr4_clear_bits(unsigned long mask)
{
	unsigned long flags;
	local_irq_save(flags);
	cr4_clear_bits_irqsoff(mask);
	local_irq_restore(flags);
}
#ifndef MODULE
#define TLB_NR_DYN_ASIDS	6
struct tlb_context {
	u64 ctx_id;
	u64 tlb_gen;
};
struct tlb_state {
	struct mm_struct *loaded_mm;
#define LOADED_MM_SWITCHING ((struct mm_struct *)1UL)
	union {
		struct mm_struct	*last_user_mm;
		unsigned long		last_user_mm_spec;
	};
	u16 loaded_mm_asid;
	u16 next_asid;
	bool invalidate_other;
#ifdef CONFIG_ADDRESS_MASKING
	u8 lam;
#endif
	unsigned short user_pcid_flush_mask;
	unsigned long cr4;
	struct tlb_context ctxs[TLB_NR_DYN_ASIDS];
};
DECLARE_PER_CPU_ALIGNED(struct tlb_state, cpu_tlbstate);
struct tlb_state_shared {
	bool is_lazy;
};
DECLARE_PER_CPU_SHARED_ALIGNED(struct tlb_state_shared, cpu_tlbstate_shared);
bool nmi_uaccess_okay(void);
#define nmi_uaccess_okay nmi_uaccess_okay
static inline void cr4_init_shadow(void)
{
	this_cpu_write(cpu_tlbstate.cr4, __read_cr4());
}
extern unsigned long mmu_cr4_features;
extern u32 *trampoline_cr4_features;
extern void initialize_tlbstate_and_flush(void);
struct flush_tlb_info {
	struct mm_struct	*mm;
	unsigned long		start;
	unsigned long		end;
	u64			new_tlb_gen;
	unsigned int		initiating_cpu;
	u8			stride_shift;
	u8			freed_tables;
};
void flush_tlb_local(void);
void flush_tlb_one_user(unsigned long addr);
void flush_tlb_one_kernel(unsigned long addr);
void flush_tlb_multi(const struct cpumask *cpumask,
		      const struct flush_tlb_info *info);
#ifdef CONFIG_PARAVIRT
#include <asm/paravirt.h>
#endif
#define flush_tlb_mm(mm)						\
		flush_tlb_mm_range(mm, 0UL, TLB_FLUSH_ALL, 0UL, true)
#define flush_tlb_range(vma, start, end)				\
	flush_tlb_mm_range((vma)->vm_mm, start, end,			\
			   ((vma)->vm_flags & VM_HUGETLB)		\
				? huge_page_shift(hstate_vma(vma))	\
				: PAGE_SHIFT, false)
extern void flush_tlb_all(void);
extern void flush_tlb_mm_range(struct mm_struct *mm, unsigned long start,
				unsigned long end, unsigned int stride_shift,
				bool freed_tables);
extern void flush_tlb_kernel_range(unsigned long start, unsigned long end);
static inline void flush_tlb_page(struct vm_area_struct *vma, unsigned long a)
{
	flush_tlb_mm_range(vma->vm_mm, a, a + PAGE_SIZE, PAGE_SHIFT, false);
}
static inline bool arch_tlbbatch_should_defer(struct mm_struct *mm)
{
	bool should_defer = false;
	if (cpumask_any_but(mm_cpumask(mm), get_cpu()) < nr_cpu_ids)
		should_defer = true;
	put_cpu();
	return should_defer;
}
static inline u64 inc_mm_tlb_gen(struct mm_struct *mm)
{
	return atomic64_inc_return(&mm->context.tlb_gen);
}
static inline void arch_tlbbatch_add_pending(struct arch_tlbflush_unmap_batch *batch,
					     struct mm_struct *mm,
					     unsigned long uaddr)
{
	inc_mm_tlb_gen(mm);
	cpumask_or(&batch->cpumask, &batch->cpumask, mm_cpumask(mm));
	mmu_notifier_arch_invalidate_secondary_tlbs(mm, 0, -1UL);
}
static inline void arch_flush_tlb_batched_pending(struct mm_struct *mm)
{
	flush_tlb_mm(mm);
}
extern void arch_tlbbatch_flush(struct arch_tlbflush_unmap_batch *batch);
static inline bool pte_flags_need_flush(unsigned long oldflags,
					unsigned long newflags,
					bool ignore_access)
{
	const pteval_t flush_on_clear = _PAGE_DIRTY | _PAGE_PRESENT |
					_PAGE_ACCESSED;
	const pteval_t software_flags = _PAGE_SOFTW1 | _PAGE_SOFTW2 |
					_PAGE_SOFTW3 | _PAGE_SOFTW4 |
					_PAGE_SAVED_DIRTY;
	const pteval_t flush_on_change = _PAGE_RW | _PAGE_USER | _PAGE_PWT |
			  _PAGE_PCD | _PAGE_PSE | _PAGE_GLOBAL | _PAGE_PAT |
			  _PAGE_PAT_LARGE | _PAGE_PKEY_BIT0 | _PAGE_PKEY_BIT1 |
			  _PAGE_PKEY_BIT2 | _PAGE_PKEY_BIT3 | _PAGE_NX;
	unsigned long diff = oldflags ^ newflags;
	BUILD_BUG_ON(flush_on_clear & software_flags);
	BUILD_BUG_ON(flush_on_clear & flush_on_change);
	BUILD_BUG_ON(flush_on_change & software_flags);
	diff &= ~software_flags;
	if (ignore_access)
		diff &= ~_PAGE_ACCESSED;
	if (diff & oldflags & flush_on_clear)
		return true;
	if (diff & flush_on_change)
		return true;
	if (IS_ENABLED(CONFIG_DEBUG_VM) &&
	    (diff & ~(flush_on_clear | software_flags | flush_on_change))) {
		VM_WARN_ON_ONCE(1);
		return true;
	}
	return false;
}
static inline bool pte_needs_flush(pte_t oldpte, pte_t newpte)
{
	if (!(pte_flags(oldpte) & _PAGE_PRESENT))
		return false;
	if (pte_pfn(oldpte) != pte_pfn(newpte))
		return true;
	return pte_flags_need_flush(pte_flags(oldpte), pte_flags(newpte),
				    true);
}
#define pte_needs_flush pte_needs_flush
static inline bool huge_pmd_needs_flush(pmd_t oldpmd, pmd_t newpmd)
{
	if (!(pmd_flags(oldpmd) & _PAGE_PRESENT))
		return false;
	if (pmd_pfn(oldpmd) != pmd_pfn(newpmd))
		return true;
	return pte_flags_need_flush(pmd_flags(oldpmd), pmd_flags(newpmd),
				    false);
}
#define huge_pmd_needs_flush huge_pmd_needs_flush
#ifdef CONFIG_ADDRESS_MASKING
static inline  u64 tlbstate_lam_cr3_mask(void)
{
	u64 lam = this_cpu_read(cpu_tlbstate.lam);
	return lam << X86_CR3_LAM_U57_BIT;
}
static inline void set_tlbstate_lam_mode(struct mm_struct *mm)
{
	this_cpu_write(cpu_tlbstate.lam,
		       mm->context.lam_cr3_mask >> X86_CR3_LAM_U57_BIT);
	this_cpu_write(tlbstate_untag_mask, mm->context.untag_mask);
}
#else
static inline u64 tlbstate_lam_cr3_mask(void)
{
	return 0;
}
static inline void set_tlbstate_lam_mode(struct mm_struct *mm)
{
}
#endif
#endif  
static inline void __native_tlb_flush_global(unsigned long cr4)
{
	native_write_cr4(cr4 ^ X86_CR4_PGE);
	native_write_cr4(cr4);
}
#endif  
