#ifndef _ASM_NIOS2_PGALLOC_H
#define _ASM_NIOS2_PGALLOC_H
#include <linux/mm.h>
#include <asm-generic/pgalloc.h>
static inline void pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd,
	pte_t *pte)
{
	set_pmd(pmd, __pmd((unsigned long)pte));
}
static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmd,
	pgtable_t pte)
{
	set_pmd(pmd, __pmd((unsigned long)page_address(pte)));
}
extern pgd_t *pgd_alloc(struct mm_struct *mm);
#define __pte_free_tlb(tlb, pte, addr)					\
	do {								\
		pagetable_pte_dtor(page_ptdesc(pte));			\
		tlb_remove_page_ptdesc((tlb), (page_ptdesc(pte)));	\
	} while (0)
#endif  
