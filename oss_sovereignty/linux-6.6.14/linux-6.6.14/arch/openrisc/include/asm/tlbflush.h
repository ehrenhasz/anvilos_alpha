#ifndef __ASM_OPENRISC_TLBFLUSH_H
#define __ASM_OPENRISC_TLBFLUSH_H
#include <linux/mm.h>
#include <asm/processor.h>
#include <asm/current.h>
#include <linux/sched.h>
extern void local_flush_tlb_all(void);
extern void local_flush_tlb_mm(struct mm_struct *mm);
extern void local_flush_tlb_page(struct vm_area_struct *vma,
				 unsigned long addr);
extern void local_flush_tlb_range(struct vm_area_struct *vma,
				  unsigned long start,
				  unsigned long end);
#ifndef CONFIG_SMP
#define flush_tlb_all	local_flush_tlb_all
#define flush_tlb_mm	local_flush_tlb_mm
#define flush_tlb_page	local_flush_tlb_page
#define flush_tlb_range	local_flush_tlb_range
#else
extern void flush_tlb_all(void);
extern void flush_tlb_mm(struct mm_struct *mm);
extern void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr);
extern void flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
			    unsigned long end);
#endif
static inline void flush_tlb(void)
{
	flush_tlb_mm(current->mm);
}
static inline void flush_tlb_kernel_range(unsigned long start,
					  unsigned long end)
{
	flush_tlb_range(NULL, start, end);
}
#endif  
