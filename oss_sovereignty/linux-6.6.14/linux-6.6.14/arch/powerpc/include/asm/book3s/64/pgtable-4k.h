#ifndef _ASM_POWERPC_BOOK3S_64_PGTABLE_4K_H
#define _ASM_POWERPC_BOOK3S_64_PGTABLE_4K_H
#ifndef __ASSEMBLY__
#ifdef CONFIG_HUGETLB_PAGE
static inline int pmd_huge(pmd_t pmd)
{
	if (radix_enabled())
		return !!(pmd_raw(pmd) & cpu_to_be64(_PAGE_PTE));
	return 0;
}
static inline int pud_huge(pud_t pud)
{
	if (radix_enabled())
		return !!(pud_raw(pud) & cpu_to_be64(_PAGE_PTE));
	return 0;
}
static inline int hugepd_ok(hugepd_t hpd)
{
	if (radix_enabled())
		return 0;
	return hash__hugepd_ok(hpd);
}
#define is_hugepd(hpd)		(hugepd_ok(hpd))
#define H_16M_CACHE_INDEX (PAGE_SHIFT + H_PTE_INDEX_SIZE + H_PMD_INDEX_SIZE - 24)
#define H_16G_CACHE_INDEX                                                      \
	(PAGE_SHIFT + H_PTE_INDEX_SIZE + H_PMD_INDEX_SIZE + H_PUD_INDEX_SIZE - 34)
static inline int get_hugepd_cache_index(int index)
{
	switch (index) {
	case H_16M_CACHE_INDEX:
		return HTLB_16M_INDEX;
	case H_16G_CACHE_INDEX:
		return HTLB_16G_INDEX;
	default:
		BUG();
	}
}
#endif  
#endif  
#endif  
