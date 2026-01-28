#ifndef _ASM_TLBFLUSH_H
#define _ASM_TLBFLUSH_H
#include <linux/mm.h>
#include <asm/processor.h>
extern void tlb_flush_all(void);
extern void flush_tlb_mm(struct mm_struct *mm);
extern void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr);
extern void flush_tlb_range(struct vm_area_struct *vma,
				unsigned long start, unsigned long end);
extern void flush_tlb_kernel_range(unsigned long start, unsigned long end);
extern void flush_tlb_one(unsigned long);
#define flush_tlb_pgtables(mm, start, end)
#endif
