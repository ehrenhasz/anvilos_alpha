#ifndef _ASM_ARC_PGALLOC_H
#define _ASM_ARC_PGALLOC_H
#include <linux/mm.h>
#include <linux/log2.h>
#include <asm-generic/pgalloc.h>
static inline void
pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd, pte_t *pte)
{
	set_pmd(pmd, __pmd((unsigned long)pte));
}
static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmd, pgtable_t pte_page)
{
	set_pmd(pmd, __pmd((unsigned long)page_address(pte_page)));
}
static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *ret = (pgd_t *) __get_free_page(GFP_KERNEL);
	if (ret) {
		int num, num2;
		num = USER_PTRS_PER_PGD + USER_KERNEL_GUTTER / PGDIR_SIZE;
		memzero(ret, num * sizeof(pgd_t));
		num2 = VMALLOC_SIZE / PGDIR_SIZE;
		memcpy(ret + num, swapper_pg_dir + num, num2 * sizeof(pgd_t));
		memzero(ret + num + num2,
			       (PTRS_PER_PGD - num - num2) * sizeof(pgd_t));
	}
	return ret;
}
#if CONFIG_PGTABLE_LEVELS > 3
static inline void p4d_populate(struct mm_struct *mm, p4d_t *p4dp, pud_t *pudp)
{
	set_p4d(p4dp, __p4d((unsigned long)pudp));
}
#define __pud_free_tlb(tlb, pmd, addr)  pud_free((tlb)->mm, pmd)
#endif
#if CONFIG_PGTABLE_LEVELS > 2
static inline void pud_populate(struct mm_struct *mm, pud_t *pudp, pmd_t *pmdp)
{
	set_pud(pudp, __pud((unsigned long)pmdp));
}
#define __pmd_free_tlb(tlb, pmd, addr)  pmd_free((tlb)->mm, pmd)
#endif
#define __pte_free_tlb(tlb, pte, addr)  pte_free((tlb)->mm, pte)
#endif  
