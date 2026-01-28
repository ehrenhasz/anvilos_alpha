#ifndef _S390_TLB_H
#define _S390_TLB_H
void __tlb_remove_table(void *_table);
static inline void tlb_flush(struct mmu_gather *tlb);
static inline bool __tlb_remove_page_size(struct mmu_gather *tlb,
					  struct encoded_page *page,
					  int page_size);
#define tlb_flush tlb_flush
#define pte_free_tlb pte_free_tlb
#define pmd_free_tlb pmd_free_tlb
#define p4d_free_tlb p4d_free_tlb
#define pud_free_tlb pud_free_tlb
#include <asm/tlbflush.h>
#include <asm-generic/tlb.h>
static inline bool __tlb_remove_page_size(struct mmu_gather *tlb,
					  struct encoded_page *page,
					  int page_size)
{
	free_page_and_swap_cache(encoded_page_ptr(page));
	return false;
}
static inline void tlb_flush(struct mmu_gather *tlb)
{
	__tlb_flush_mm_lazy(tlb->mm);
}
static inline void pte_free_tlb(struct mmu_gather *tlb, pgtable_t pte,
                                unsigned long address)
{
	__tlb_adjust_range(tlb, address, PAGE_SIZE);
	tlb->mm->context.flush_mm = 1;
	tlb->freed_tables = 1;
	tlb->cleared_pmds = 1;
	page_table_free_rcu(tlb, (unsigned long *) pte, address);
}
static inline void pmd_free_tlb(struct mmu_gather *tlb, pmd_t *pmd,
				unsigned long address)
{
	if (mm_pmd_folded(tlb->mm))
		return;
	pagetable_pmd_dtor(virt_to_ptdesc(pmd));
	__tlb_adjust_range(tlb, address, PAGE_SIZE);
	tlb->mm->context.flush_mm = 1;
	tlb->freed_tables = 1;
	tlb->cleared_puds = 1;
	tlb_remove_ptdesc(tlb, pmd);
}
static inline void p4d_free_tlb(struct mmu_gather *tlb, p4d_t *p4d,
				unsigned long address)
{
	if (mm_p4d_folded(tlb->mm))
		return;
	__tlb_adjust_range(tlb, address, PAGE_SIZE);
	tlb->mm->context.flush_mm = 1;
	tlb->freed_tables = 1;
	tlb_remove_table(tlb, p4d);
}
static inline void pud_free_tlb(struct mmu_gather *tlb, pud_t *pud,
				unsigned long address)
{
	if (mm_pud_folded(tlb->mm))
		return;
	tlb->mm->context.flush_mm = 1;
	tlb->freed_tables = 1;
	tlb->cleared_p4ds = 1;
	tlb_remove_table(tlb, pud);
}
#endif  
