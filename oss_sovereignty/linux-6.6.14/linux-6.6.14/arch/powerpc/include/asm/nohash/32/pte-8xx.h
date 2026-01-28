#ifndef _ASM_POWERPC_NOHASH_32_PTE_8xx_H
#define _ASM_POWERPC_NOHASH_32_PTE_8xx_H
#ifdef __KERNEL__
#define _PAGE_PRESENT	0x0001	 
#define _PAGE_NO_CACHE	0x0002	 
#define _PAGE_SH	0x0004	 
#define _PAGE_SPS	0x0008	 
#define _PAGE_DIRTY	0x0100	 
#define _PAGE_GUARDED	0x0010	 
#define _PAGE_ACCESSED	0x0020	 
#define _PAGE_EXEC	0x0040	 
#define _PAGE_SPECIAL	0x0080	 
#define _PAGE_NA	0x0200	 
#define _PAGE_RO	0x0600	 
#define _PAGE_HUGE	0x0800	 
#define _PAGE_COHERENT	0
#define _PAGE_WRITETHRU	0
#define _PAGE_KERNEL_RO		(_PAGE_SH | _PAGE_RO)
#define _PAGE_KERNEL_ROX	(_PAGE_SH | _PAGE_RO | _PAGE_EXEC)
#define _PAGE_KERNEL_RW		(_PAGE_SH | _PAGE_DIRTY)
#define _PAGE_KERNEL_RWX	(_PAGE_SH | _PAGE_DIRTY | _PAGE_EXEC)
#define _PMD_PRESENT	0x0001
#define _PMD_PRESENT_MASK	_PMD_PRESENT
#define _PMD_BAD	0x0f90
#define _PMD_PAGE_MASK	0x000c
#define _PMD_PAGE_8M	0x000c
#define _PMD_PAGE_512K	0x0004
#define _PMD_ACCESSED	0x0020	 
#define _PMD_USER	0x0040	 
#define _PTE_NONE_MASK	0
#ifdef CONFIG_PPC_16K_PAGES
#define _PAGE_PSIZE	_PAGE_SPS
#else
#define _PAGE_PSIZE		0
#endif
#define _PAGE_BASE_NC	(_PAGE_PRESENT | _PAGE_ACCESSED | _PAGE_PSIZE)
#define _PAGE_BASE	(_PAGE_BASE_NC)
#define PAGE_NONE	__pgprot(_PAGE_BASE | _PAGE_NA)
#define PAGE_SHARED	__pgprot(_PAGE_BASE)
#define PAGE_SHARED_X	__pgprot(_PAGE_BASE | _PAGE_EXEC)
#define PAGE_COPY	__pgprot(_PAGE_BASE | _PAGE_RO)
#define PAGE_COPY_X	__pgprot(_PAGE_BASE | _PAGE_RO | _PAGE_EXEC)
#define PAGE_READONLY	__pgprot(_PAGE_BASE | _PAGE_RO)
#define PAGE_READONLY_X	__pgprot(_PAGE_BASE | _PAGE_RO | _PAGE_EXEC)
#ifndef __ASSEMBLY__
static inline pte_t pte_wrprotect(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_RO);
}
#define pte_wrprotect pte_wrprotect
static inline int pte_read(pte_t pte)
{
	return (pte_val(pte) & _PAGE_RO) != _PAGE_NA;
}
#define pte_read pte_read
static inline int pte_write(pte_t pte)
{
	return !(pte_val(pte) & _PAGE_RO);
}
#define pte_write pte_write
static inline pte_t pte_mkwrite_novma(pte_t pte)
{
	return __pte(pte_val(pte) & ~_PAGE_RO);
}
#define pte_mkwrite_novma pte_mkwrite_novma
static inline bool pte_user(pte_t pte)
{
	return !(pte_val(pte) & _PAGE_SH);
}
#define pte_user pte_user
static inline pte_t pte_mkprivileged(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_SH);
}
#define pte_mkprivileged pte_mkprivileged
static inline pte_t pte_mkuser(pte_t pte)
{
	return __pte(pte_val(pte) & ~_PAGE_SH);
}
#define pte_mkuser pte_mkuser
static inline pte_t pte_mkhuge(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_SPS | _PAGE_HUGE);
}
#define pte_mkhuge pte_mkhuge
static inline pte_basic_t pte_update(struct mm_struct *mm, unsigned long addr, pte_t *p,
				     unsigned long clr, unsigned long set, int huge);
static inline void ptep_set_wrprotect(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	pte_update(mm, addr, ptep, 0, _PAGE_RO, 0);
}
#define ptep_set_wrprotect ptep_set_wrprotect
static inline void __ptep_set_access_flags(struct vm_area_struct *vma, pte_t *ptep,
					   pte_t entry, unsigned long address, int psize)
{
	unsigned long set = pte_val(entry) & (_PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_EXEC);
	unsigned long clr = ~pte_val(entry) & _PAGE_RO;
	int huge = psize > mmu_virtual_psize ? 1 : 0;
	pte_update(vma->vm_mm, address, ptep, clr, set, huge);
	flush_tlb_page(vma, address);
}
#define __ptep_set_access_flags __ptep_set_access_flags
static inline unsigned long pgd_leaf_size(pgd_t pgd)
{
	if (pgd_val(pgd) & _PMD_PAGE_8M)
		return SZ_8M;
	return SZ_4M;
}
#define pgd_leaf_size pgd_leaf_size
static inline unsigned long pte_leaf_size(pte_t pte)
{
	pte_basic_t val = pte_val(pte);
	if (val & _PAGE_HUGE)
		return SZ_512K;
	if (val & _PAGE_SPS)
		return SZ_16K;
	return SZ_4K;
}
#define pte_leaf_size pte_leaf_size
#endif
#endif  
#endif  
