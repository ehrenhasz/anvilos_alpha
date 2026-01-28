#ifndef _SUN3_PGALLOC_H
#define _SUN3_PGALLOC_H
#include <asm/tlb.h>
#include <asm-generic/pgalloc.h>
extern const char bad_pmd_string[];
#define __pte_free_tlb(tlb, pte, addr)				\
do {								\
	pagetable_pte_dtor(page_ptdesc(pte));			\
	tlb_remove_page_ptdesc((tlb), page_ptdesc(pte));	\
} while (0)
static inline void pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
	pmd_val(*pmd) = __pa((unsigned long)pte);
}
static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmd, pgtable_t page)
{
	pmd_val(*pmd) = __pa((unsigned long)page_address(page));
}
#define pmd_free(mm, x)			do { } while (0)
static inline pgd_t * pgd_alloc(struct mm_struct *mm)
{
     pgd_t *new_pgd;
     new_pgd = (pgd_t *)get_zeroed_page(GFP_KERNEL);
     memcpy(new_pgd, swapper_pg_dir, PAGE_SIZE);
     memset(new_pgd, 0, (PAGE_OFFSET >> PGDIR_SHIFT));
     return new_pgd;
}
#endif  
