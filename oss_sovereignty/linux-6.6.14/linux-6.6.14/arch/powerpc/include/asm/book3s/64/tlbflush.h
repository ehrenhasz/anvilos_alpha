#ifndef _ASM_POWERPC_BOOK3S_64_TLBFLUSH_H
#define _ASM_POWERPC_BOOK3S_64_TLBFLUSH_H
#define MMU_NO_CONTEXT	~0UL
#include <linux/mm_types.h>
#include <linux/mmu_notifier.h>
#include <asm/book3s/64/tlbflush-hash.h>
#include <asm/book3s/64/tlbflush-radix.h>
enum {
	TLB_INVAL_SCOPE_GLOBAL = 0,	 
	TLB_INVAL_SCOPE_LPID = 1,	 
};
static inline void tlbiel_all(void)
{
	if (early_radix_enabled())
		radix__tlbiel_all(TLB_INVAL_SCOPE_GLOBAL);
	else
		hash__tlbiel_all(TLB_INVAL_SCOPE_GLOBAL);
}
static inline void tlbiel_all_lpid(bool radix)
{
	if (radix)
		radix__tlbiel_all(TLB_INVAL_SCOPE_LPID);
	else
		hash__tlbiel_all(TLB_INVAL_SCOPE_LPID);
}
#define __HAVE_ARCH_FLUSH_PMD_TLB_RANGE
static inline void flush_pmd_tlb_range(struct vm_area_struct *vma,
				       unsigned long start, unsigned long end)
{
	if (radix_enabled())
		radix__flush_pmd_tlb_range(vma, start, end);
}
#define __HAVE_ARCH_FLUSH_PUD_TLB_RANGE
static inline void flush_pud_tlb_range(struct vm_area_struct *vma,
				       unsigned long start, unsigned long end)
{
	if (radix_enabled())
		radix__flush_pud_tlb_range(vma, start, end);
}
#define __HAVE_ARCH_FLUSH_HUGETLB_TLB_RANGE
static inline void flush_hugetlb_tlb_range(struct vm_area_struct *vma,
					   unsigned long start,
					   unsigned long end)
{
	if (radix_enabled())
		radix__flush_hugetlb_tlb_range(vma, start, end);
}
static inline void flush_tlb_range(struct vm_area_struct *vma,
				   unsigned long start, unsigned long end)
{
	if (radix_enabled())
		radix__flush_tlb_range(vma, start, end);
}
static inline void flush_tlb_kernel_range(unsigned long start,
					  unsigned long end)
{
	if (radix_enabled())
		radix__flush_tlb_kernel_range(start, end);
}
static inline void local_flush_tlb_mm(struct mm_struct *mm)
{
	if (radix_enabled())
		radix__local_flush_tlb_mm(mm);
}
static inline void local_flush_tlb_page(struct vm_area_struct *vma,
					unsigned long vmaddr)
{
	if (radix_enabled())
		radix__local_flush_tlb_page(vma, vmaddr);
}
static inline void local_flush_tlb_page_psize(struct mm_struct *mm,
					      unsigned long vmaddr, int psize)
{
	if (radix_enabled())
		radix__local_flush_tlb_page_psize(mm, vmaddr, psize);
}
static inline void tlb_flush(struct mmu_gather *tlb)
{
	if (radix_enabled())
		radix__tlb_flush(tlb);
	else
		hash__tlb_flush(tlb);
}
#ifdef CONFIG_SMP
static inline void flush_tlb_mm(struct mm_struct *mm)
{
	if (radix_enabled())
		radix__flush_tlb_mm(mm);
}
static inline void flush_tlb_page(struct vm_area_struct *vma,
				  unsigned long vmaddr)
{
	if (radix_enabled())
		radix__flush_tlb_page(vma, vmaddr);
}
#else
#define flush_tlb_mm(mm)		local_flush_tlb_mm(mm)
#define flush_tlb_page(vma, addr)	local_flush_tlb_page(vma, addr)
#endif  
#define flush_tlb_fix_spurious_fault flush_tlb_fix_spurious_fault
static inline void flush_tlb_fix_spurious_fault(struct vm_area_struct *vma,
						unsigned long address,
						pte_t *ptep)
{
}
static inline bool __pte_protnone(unsigned long pte)
{
	return (pte & (pgprot_val(PAGE_NONE) | _PAGE_RWX)) == pgprot_val(PAGE_NONE);
}
static inline bool __pte_flags_need_flush(unsigned long oldval,
					  unsigned long newval)
{
	unsigned long delta = oldval ^ newval;
	if (!radix_enabled())
		return true;
	VM_WARN_ON_ONCE(!__pte_protnone(oldval) && oldval & _PAGE_PRIVILEGED);
	VM_WARN_ON_ONCE(!__pte_protnone(newval) && newval & _PAGE_PRIVILEGED);
	VM_WARN_ON_ONCE(!(oldval & _PAGE_PTE));
	VM_WARN_ON_ONCE(!(newval & _PAGE_PTE));
	VM_WARN_ON_ONCE(!(oldval & _PAGE_PRESENT));
	VM_WARN_ON_ONCE(!(newval & _PAGE_PRESENT));
	if (delta & ~(_PAGE_RWX | _PAGE_DIRTY | _PAGE_ACCESSED))
		return true;
	if ((delta & ~_PAGE_ACCESSED) & oldval)
		return true;
	return false;
}
static inline bool pte_needs_flush(pte_t oldpte, pte_t newpte)
{
	return __pte_flags_need_flush(pte_val(oldpte), pte_val(newpte));
}
#define pte_needs_flush pte_needs_flush
static inline bool huge_pmd_needs_flush(pmd_t oldpmd, pmd_t newpmd)
{
	return __pte_flags_need_flush(pmd_val(oldpmd), pmd_val(newpmd));
}
#define huge_pmd_needs_flush huge_pmd_needs_flush
extern bool tlbie_capable;
extern bool tlbie_enabled;
static inline bool cputlb_use_tlbie(void)
{
	return tlbie_enabled;
}
#endif  
