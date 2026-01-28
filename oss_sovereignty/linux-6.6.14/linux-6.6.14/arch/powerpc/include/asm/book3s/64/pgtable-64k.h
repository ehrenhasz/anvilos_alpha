#ifndef _ASM_POWERPC_BOOK3S_64_PGTABLE_64K_H
#define _ASM_POWERPC_BOOK3S_64_PGTABLE_64K_H
#ifndef __ASSEMBLY__
#ifdef CONFIG_HUGETLB_PAGE
static inline int pmd_huge(pmd_t pmd)
{
	return !!(pmd_raw(pmd) & cpu_to_be64(_PAGE_PTE));
}
static inline int pud_huge(pud_t pud)
{
	return !!(pud_raw(pud) & cpu_to_be64(_PAGE_PTE));
}
static inline int hugepd_ok(hugepd_t hpd)
{
	return 0;
}
#define is_hugepd(pdep)			0
static inline int get_hugepd_cache_index(int index)
{
	BUG();
}
#endif  
static inline int remap_4k_pfn(struct vm_area_struct *vma, unsigned long addr,
			       unsigned long pfn, pgprot_t prot)
{
	if (radix_enabled())
		BUG();
	return hash__remap_4k_pfn(vma, addr, pfn, prot);
}
#endif	 
#endif  
