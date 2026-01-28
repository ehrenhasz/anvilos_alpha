#ifndef _ASM_NIOS2_TLBFLUSH_H
#define _ASM_NIOS2_TLBFLUSH_H
struct mm_struct;
extern void flush_tlb_all(void);
extern void flush_tlb_mm(struct mm_struct *mm);
extern void flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
			    unsigned long end);
extern void flush_tlb_kernel_range(unsigned long start, unsigned long end);
static inline void flush_tlb_page(struct vm_area_struct *vma,
				  unsigned long address)
{
	flush_tlb_range(vma, address, address + PAGE_SIZE);
}
static inline void flush_tlb_kernel_page(unsigned long address)
{
	flush_tlb_kernel_range(address, address + PAGE_SIZE);
}
extern void reload_tlb_page(struct vm_area_struct *vma, unsigned long addr,
			    pte_t pte);
#endif  
