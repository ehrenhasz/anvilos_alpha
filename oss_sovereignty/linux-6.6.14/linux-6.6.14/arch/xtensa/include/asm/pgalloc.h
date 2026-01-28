#ifndef _XTENSA_PGALLOC_H
#define _XTENSA_PGALLOC_H
#ifdef CONFIG_MMU
#include <linux/highmem.h>
#include <linux/slab.h>
#define __HAVE_ARCH_PTE_ALLOC_ONE_KERNEL
#define __HAVE_ARCH_PTE_ALLOC_ONE
#include <asm-generic/pgalloc.h>
#define pmd_populate_kernel(mm, pmdp, ptep)				     \
	(pmd_val(*(pmdp)) = ((unsigned long)ptep))
#define pmd_populate(mm, pmdp, page)					     \
	(pmd_val(*(pmdp)) = ((unsigned long)page_to_virt(page)))
static inline pgd_t*
pgd_alloc(struct mm_struct *mm)
{
	return (pgd_t*) __get_free_page(GFP_KERNEL | __GFP_ZERO);
}
static inline void ptes_clear(pte_t *ptep)
{
	int i;
	for (i = 0; i < PTRS_PER_PTE; i++)
		pte_clear(NULL, 0, ptep + i);
}
static inline pte_t *pte_alloc_one_kernel(struct mm_struct *mm)
{
	pte_t *ptep;
	ptep = (pte_t *)__pte_alloc_one_kernel(mm);
	if (!ptep)
		return NULL;
	ptes_clear(ptep);
	return ptep;
}
static inline pgtable_t pte_alloc_one(struct mm_struct *mm)
{
	struct page *page;
	page = __pte_alloc_one(mm, GFP_PGTABLE_USER);
	if (!page)
		return NULL;
	ptes_clear(page_address(page));
	return page;
}
#endif  
#endif  
