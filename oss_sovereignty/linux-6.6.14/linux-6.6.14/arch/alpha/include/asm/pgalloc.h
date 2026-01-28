#ifndef _ALPHA_PGALLOC_H
#define _ALPHA_PGALLOC_H
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <asm-generic/pgalloc.h>
static inline void
pmd_populate(struct mm_struct *mm, pmd_t *pmd, pgtable_t pte)
{
	pmd_set(pmd, (pte_t *)(page_to_pa(pte) + PAGE_OFFSET));
}
static inline void
pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
	pmd_set(pmd, pte);
}
static inline void
pud_populate(struct mm_struct *mm, pud_t *pud, pmd_t *pmd)
{
	pud_set(pud, pmd);
}
extern pgd_t *pgd_alloc(struct mm_struct *mm);
#endif  
