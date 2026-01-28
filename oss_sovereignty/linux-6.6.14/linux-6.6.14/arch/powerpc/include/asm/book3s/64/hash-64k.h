#ifndef _ASM_POWERPC_BOOK3S_64_HASH_64K_H
#define _ASM_POWERPC_BOOK3S_64_HASH_64K_H
#define H_PTE_INDEX_SIZE   8   
#define H_PMD_INDEX_SIZE  10   
#define H_PUD_INDEX_SIZE  10   
#define H_PGD_INDEX_SIZE   8   
#if defined(CONFIG_SPARSEMEM_VMEMMAP) && defined(CONFIG_SPARSEMEM_EXTREME)
#define H_MAX_PHYSMEM_BITS	51
#else
#define H_MAX_PHYSMEM_BITS	46
#endif
#define MAX_EA_BITS_PER_CONTEXT		49
#define REGION_SHIFT		MAX_EA_BITS_PER_CONTEXT
#define H_KERN_MAP_SIZE		(1UL << MAX_EA_BITS_PER_CONTEXT)
#define H_KERN_VIRT_START	ASM_CONST(0xc008000000000000)
#define H_PAGE_COMBO	_RPAGE_RPN0  
#define H_PAGE_4K_PFN	_RPAGE_RPN1  
#define H_PAGE_BUSY	_RPAGE_RSV1      
#define H_PAGE_HASHPTE	_RPAGE_RPN43	 
#define H_PTE_PKEY_BIT4		_RPAGE_PKEY_BIT4
#define H_PTE_PKEY_BIT3		_RPAGE_PKEY_BIT3
#define H_PTE_PKEY_BIT2		_RPAGE_PKEY_BIT2
#define H_PTE_PKEY_BIT1		_RPAGE_PKEY_BIT1
#define H_PTE_PKEY_BIT0		_RPAGE_PKEY_BIT0
#define H_PAGE_THP_HUGE  H_PAGE_4K_PFN
#define _PAGE_HPTEFLAGS (H_PAGE_BUSY | H_PAGE_HASHPTE | H_PAGE_COMBO)
#define H_PTE_FRAG_SIZE_SHIFT  (H_PTE_INDEX_SIZE + 3 + 1)
#define H_PTE_FRAG_NR	(PAGE_SIZE >> H_PTE_FRAG_SIZE_SHIFT)
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) || defined(CONFIG_HUGETLB_PAGE)
#define H_PMD_FRAG_SIZE_SHIFT  (H_PMD_INDEX_SIZE + 3 + 1)
#else
#define H_PMD_FRAG_SIZE_SHIFT  (H_PMD_INDEX_SIZE + 3)
#endif
#define H_PMD_FRAG_NR	(PAGE_SIZE >> H_PMD_FRAG_SIZE_SHIFT)
#ifndef __ASSEMBLY__
#include <asm/errno.h>
#define __real_pte __real_pte
static inline real_pte_t __real_pte(pte_t pte, pte_t *ptep, int offset)
{
	real_pte_t rpte;
	unsigned long *hidxp;
	rpte.pte = pte;
	smp_rmb();
	hidxp = (unsigned long *)(ptep + offset);
	rpte.hidx = *hidxp;
	return rpte;
}
#define HIDX_UNSHIFT_BY_ONE(x) ((x + 0xfUL) & 0xfUL)  
#define HIDX_SHIFT_BY_ONE(x) ((x + 0x1UL) & 0xfUL)    
#define HIDX_BITS(x, index)  (x << (index << 2))
#define BITS_TO_HIDX(x, index)  ((x >> (index << 2)) & 0xfUL)
#define INVALID_RPTE_HIDX  0x0UL
static inline unsigned long __rpte_to_hidx(real_pte_t rpte, unsigned long index)
{
	return HIDX_UNSHIFT_BY_ONE(BITS_TO_HIDX(rpte.hidx, index));
}
static inline unsigned long pte_set_hidx(pte_t *ptep, real_pte_t rpte,
					 unsigned int subpg_index,
					 unsigned long hidx, int offset)
{
	unsigned long *hidxp = (unsigned long *)(ptep + offset);
	rpte.hidx &= ~HIDX_BITS(0xfUL, subpg_index);
	*hidxp = rpte.hidx  | HIDX_BITS(HIDX_SHIFT_BY_ONE(hidx), subpg_index);
	smp_wmb();
	return 0x0UL;
}
#define __rpte_to_pte(r)	((r).pte)
extern bool __rpte_sub_valid(real_pte_t rpte, unsigned long index);
#define pte_iterate_hashed_subpages(rpte, psize, vpn, index, shift)	\
	do {								\
		unsigned long __end = vpn + (1UL << (PAGE_SHIFT - VPN_SHIFT));	\
		unsigned __split = (psize == MMU_PAGE_4K ||		\
				    psize == MMU_PAGE_64K_AP);		\
		shift = mmu_psize_defs[psize].shift;			\
		for (index = 0; vpn < __end; index++,			\
			     vpn += (1L << (shift - VPN_SHIFT))) {	\
		if (!__split || __rpte_sub_valid(rpte, index))
#define pte_iterate_hashed_end()  } } while(0)
#define pte_pagesize_index(mm, addr, pte)	\
	(((pte) & H_PAGE_COMBO)? MMU_PAGE_4K: MMU_PAGE_64K)
extern int remap_pfn_range(struct vm_area_struct *, unsigned long addr,
			   unsigned long pfn, unsigned long size, pgprot_t);
static inline int hash__remap_4k_pfn(struct vm_area_struct *vma, unsigned long addr,
				 unsigned long pfn, pgprot_t prot)
{
	if (pfn > (PTE_RPN_MASK >> PAGE_SHIFT)) {
		WARN(1, "remap_4k_pfn called with wrong pfn value\n");
		return -EINVAL;
	}
	return remap_pfn_range(vma, addr, pfn, PAGE_SIZE,
			       __pgprot(pgprot_val(prot) | H_PAGE_4K_PFN));
}
#define H_PTE_TABLE_SIZE	PTE_FRAG_SIZE
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) || defined (CONFIG_HUGETLB_PAGE)
#define H_PMD_TABLE_SIZE	((sizeof(pmd_t) << PMD_INDEX_SIZE) + \
				 (sizeof(unsigned long) << PMD_INDEX_SIZE))
#else
#define H_PMD_TABLE_SIZE	(sizeof(pmd_t) << PMD_INDEX_SIZE)
#endif
#ifdef CONFIG_HUGETLB_PAGE
#define H_PUD_TABLE_SIZE	((sizeof(pud_t) << PUD_INDEX_SIZE) +	\
				 (sizeof(unsigned long) << PUD_INDEX_SIZE))
#else
#define H_PUD_TABLE_SIZE	(sizeof(pud_t) << PUD_INDEX_SIZE)
#endif
#define H_PGD_TABLE_SIZE	(sizeof(pgd_t) << PGD_INDEX_SIZE)
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static inline char *get_hpte_slot_array(pmd_t *pmdp)
{
	smp_rmb();
	return *(char **)(pmdp + PTRS_PER_PMD);
}
static inline unsigned int hpte_valid(unsigned char *hpte_slot_array, int index)
{
	return hpte_slot_array[index] & 0x1;
}
static inline unsigned int hpte_hash_index(unsigned char *hpte_slot_array,
					   int index)
{
	return hpte_slot_array[index] >> 1;
}
static inline void mark_hpte_slot_valid(unsigned char *hpte_slot_array,
					unsigned int index, unsigned int hidx)
{
	hpte_slot_array[index] = (hidx << 1) | 0x1;
}
static inline int hash__pmd_trans_huge(pmd_t pmd)
{
	return !!((pmd_val(pmd) & (_PAGE_PTE | H_PAGE_THP_HUGE | _PAGE_DEVMAP)) ==
		  (_PAGE_PTE | H_PAGE_THP_HUGE));
}
static inline pmd_t hash__pmd_mkhuge(pmd_t pmd)
{
	return __pmd(pmd_val(pmd) | (_PAGE_PTE | H_PAGE_THP_HUGE));
}
extern unsigned long hash__pmd_hugepage_update(struct mm_struct *mm,
					   unsigned long addr, pmd_t *pmdp,
					   unsigned long clr, unsigned long set);
extern pmd_t hash__pmdp_collapse_flush(struct vm_area_struct *vma,
				   unsigned long address, pmd_t *pmdp);
extern void hash__pgtable_trans_huge_deposit(struct mm_struct *mm, pmd_t *pmdp,
					 pgtable_t pgtable);
extern pgtable_t hash__pgtable_trans_huge_withdraw(struct mm_struct *mm, pmd_t *pmdp);
extern pmd_t hash__pmdp_huge_get_and_clear(struct mm_struct *mm,
				       unsigned long addr, pmd_t *pmdp);
extern int hash__has_transparent_hugepage(void);
#endif  
static inline pmd_t hash__pmd_mkdevmap(pmd_t pmd)
{
	return __pmd(pmd_val(pmd) | (_PAGE_PTE | H_PAGE_THP_HUGE | _PAGE_DEVMAP));
}
#endif	 
#endif  
