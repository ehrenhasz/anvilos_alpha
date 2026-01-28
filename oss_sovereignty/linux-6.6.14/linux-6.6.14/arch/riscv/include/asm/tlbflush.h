#ifndef _ASM_RISCV_TLBFLUSH_H
#define _ASM_RISCV_TLBFLUSH_H
#include <linux/mm_types.h>
#include <asm/smp.h>
#include <asm/errata_list.h>
#ifdef CONFIG_MMU
extern unsigned long asid_mask;
static inline void local_flush_tlb_all(void)
{
	__asm__ __volatile__ ("sfence.vma" : : : "memory");
}
static inline void local_flush_tlb_page(unsigned long addr)
{
	ALT_FLUSH_TLB_PAGE(__asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory"));
}
#else  
#define local_flush_tlb_all()			do { } while (0)
#define local_flush_tlb_page(addr)		do { } while (0)
#endif  
#if defined(CONFIG_SMP) && defined(CONFIG_MMU)
void flush_tlb_all(void);
void flush_tlb_mm(struct mm_struct *mm);
void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr);
void flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
		     unsigned long end);
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
#define __HAVE_ARCH_FLUSH_PMD_TLB_RANGE
void flush_pmd_tlb_range(struct vm_area_struct *vma, unsigned long start,
			unsigned long end);
#endif
#else  
#define flush_tlb_all() local_flush_tlb_all()
#define flush_tlb_page(vma, addr) local_flush_tlb_page(addr)
static inline void flush_tlb_range(struct vm_area_struct *vma,
		unsigned long start, unsigned long end)
{
	local_flush_tlb_all();
}
#define flush_tlb_mm(mm) flush_tlb_all()
#endif  
static inline void flush_tlb_kernel_range(unsigned long start,
	unsigned long end)
{
	flush_tlb_all();
}
#endif  
