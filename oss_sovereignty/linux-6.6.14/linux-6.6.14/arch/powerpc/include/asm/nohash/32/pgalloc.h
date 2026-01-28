#ifndef _ASM_POWERPC_PGALLOC_32_H
#define _ASM_POWERPC_PGALLOC_32_H
#include <linux/threads.h>
#include <linux/slab.h>
#define pmd_free(mm, x) 		do { } while (0)
#define __pmd_free_tlb(tlb,x,a)		do { } while (0)
static inline void pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmdp,
				       pte_t *pte)
{
	if (IS_ENABLED(CONFIG_BOOKE))
		*pmdp = __pmd((unsigned long)pte | _PMD_PRESENT);
	else
		*pmdp = __pmd(__pa(pte) | _PMD_PRESENT);
}
static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmdp,
				pgtable_t pte_page)
{
	if (IS_ENABLED(CONFIG_BOOKE))
		*pmdp = __pmd((unsigned long)pte_page | _PMD_PRESENT);
	else
		*pmdp = __pmd(__pa(pte_page) | _PMD_USER | _PMD_PRESENT);
}
#endif  
