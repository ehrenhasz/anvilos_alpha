#ifndef _ASM_ARC_PGTABLE_BITS_ARCV2_H
#define _ASM_ARC_PGTABLE_BITS_ARCV2_H
#ifdef CONFIG_ARC_CACHE_PAGES
#define _PAGE_CACHEABLE		(1 << 0)   
#else
#define _PAGE_CACHEABLE		0
#endif
#define _PAGE_EXECUTE		(1 << 1)   
#define _PAGE_WRITE		(1 << 2)   
#define _PAGE_READ		(1 << 3)   
#define _PAGE_ACCESSED		(1 << 4)   
#define _PAGE_DIRTY		(1 << 5)   
#define _PAGE_SPECIAL		(1 << 6)
#define _PAGE_GLOBAL		(1 << 8)   
#define _PAGE_PRESENT		(1 << 9)   
#define _PAGE_SWP_EXCLUSIVE	_PAGE_DIRTY
#ifdef CONFIG_ARC_MMU_V4
#define _PAGE_HW_SZ		(1 << 10)   
#else
#define _PAGE_HW_SZ		0
#endif
#define ___DEF		(_PAGE_PRESENT | _PAGE_CACHEABLE)
#define _PAGE_CHG_MASK	(PAGE_MASK_PHYS | _PAGE_ACCESSED | _PAGE_DIRTY | \
							   _PAGE_SPECIAL)
#define PAGE_U_NONE     __pgprot(___DEF)
#define PAGE_U_R        __pgprot(___DEF | _PAGE_READ)
#define PAGE_U_W_R      __pgprot(___DEF | _PAGE_READ | _PAGE_WRITE)
#define PAGE_U_X_R      __pgprot(___DEF | _PAGE_READ | _PAGE_EXECUTE)
#define PAGE_U_X_W_R    __pgprot(___DEF \
				| _PAGE_READ | _PAGE_WRITE | _PAGE_EXECUTE)
#define PAGE_KERNEL     __pgprot(___DEF | _PAGE_GLOBAL \
				| _PAGE_READ | _PAGE_WRITE | _PAGE_EXECUTE)
#define PAGE_SHARED	PAGE_U_W_R
#define pgprot_noncached(prot)	(__pgprot(pgprot_val(prot) & ~_PAGE_CACHEABLE))
#ifndef __ASSEMBLY__
#define pte_write(pte)		(pte_val(pte) & _PAGE_WRITE)
#define pte_dirty(pte)		(pte_val(pte) & _PAGE_DIRTY)
#define pte_young(pte)		(pte_val(pte) & _PAGE_ACCESSED)
#define pte_special(pte)	(pte_val(pte) & _PAGE_SPECIAL)
#define PTE_BIT_FUNC(fn, op) \
	static inline pte_t pte_##fn(pte_t pte) { pte_val(pte) op; return pte; }
PTE_BIT_FUNC(mknotpresent,     &= ~(_PAGE_PRESENT));
PTE_BIT_FUNC(wrprotect,	&= ~(_PAGE_WRITE));
PTE_BIT_FUNC(mkwrite_novma,	|= (_PAGE_WRITE));
PTE_BIT_FUNC(mkclean,	&= ~(_PAGE_DIRTY));
PTE_BIT_FUNC(mkdirty,	|= (_PAGE_DIRTY));
PTE_BIT_FUNC(mkold,	&= ~(_PAGE_ACCESSED));
PTE_BIT_FUNC(mkyoung,	|= (_PAGE_ACCESSED));
PTE_BIT_FUNC(mkspecial,	|= (_PAGE_SPECIAL));
PTE_BIT_FUNC(mkhuge,	|= (_PAGE_HW_SZ));
static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot));
}
struct vm_fault;
void update_mmu_cache_range(struct vm_fault *vmf, struct vm_area_struct *vma,
		unsigned long address, pte_t *ptep, unsigned int nr);
#define update_mmu_cache(vma, addr, ptep) \
	update_mmu_cache_range(NULL, vma, addr, ptep, 1)
#define __swp_entry(type, off)		((swp_entry_t) \
					{ ((type) & 0x1f) | ((off) << 13) })
#define __swp_type(pte_lookalike)	(((pte_lookalike).val) & 0x1f)
#define __swp_offset(pte_lookalike)	((pte_lookalike).val >> 13)
#define __pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)		((pte_t) { (x).val })
static inline int pte_swp_exclusive(pte_t pte)
{
	return pte_val(pte) & _PAGE_SWP_EXCLUSIVE;
}
PTE_BIT_FUNC(swp_mkexclusive, |= (_PAGE_SWP_EXCLUSIVE));
PTE_BIT_FUNC(swp_clear_exclusive, &= ~(_PAGE_SWP_EXCLUSIVE));
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
#include <asm/hugepage.h>
#endif
#endif  
#endif
