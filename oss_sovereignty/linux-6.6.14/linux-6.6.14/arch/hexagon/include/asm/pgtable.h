#ifndef _ASM_PGTABLE_H
#define _ASM_PGTABLE_H
#include <asm/page.h>
#include <asm-generic/pgtable-nopmd.h>
extern unsigned long empty_zero_page;
#include <asm/vm_mmu.h>
#define _PAGE_READ	__HVM_PTE_R
#define _PAGE_WRITE	__HVM_PTE_W
#define _PAGE_EXECUTE	__HVM_PTE_X
#define _PAGE_USER	__HVM_PTE_U
#define _PAGE_PRESENT	(1<<0)
#define _PAGE_DIRTY	(1<<1)
#define _PAGE_ACCESSED	(1<<2)
#define _PAGE_VALID	_PAGE_PRESENT
#define _PAGE_SWP_EXCLUSIVE	(1<<6)
#define PGDIR_SHIFT 22
#define PTRS_PER_PGD 1024
#define PGDIR_SIZE (1UL << PGDIR_SHIFT)
#define PGDIR_MASK (~(PGDIR_SIZE-1))
#ifdef CONFIG_PAGE_SIZE_4KB
#define PTRS_PER_PTE 1024
#endif
#ifdef CONFIG_PAGE_SIZE_16KB
#define PTRS_PER_PTE 256
#endif
#ifdef CONFIG_PAGE_SIZE_64KB
#define PTRS_PER_PTE 64
#endif
#ifdef CONFIG_PAGE_SIZE_256KB
#define PTRS_PER_PTE 16
#endif
#ifdef CONFIG_PAGE_SIZE_1MB
#define PTRS_PER_PTE 4
#endif
#define pgd_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__,\
		pgd_val(e))
extern unsigned long _dflt_cache_att;
#define PAGE_NONE	__pgprot(_PAGE_PRESENT | _PAGE_USER | \
				_dflt_cache_att)
#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_USER | \
				_PAGE_READ | _PAGE_EXECUTE | _dflt_cache_att)
#define PAGE_COPY	PAGE_READONLY
#define PAGE_EXEC	__pgprot(_PAGE_PRESENT | _PAGE_USER | \
				_PAGE_READ | _PAGE_EXECUTE | _dflt_cache_att)
#define PAGE_COPY_EXEC	PAGE_EXEC
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | \
				_PAGE_EXECUTE | _PAGE_WRITE | _dflt_cache_att)
#define PAGE_KERNEL	__pgprot(_PAGE_PRESENT | _PAGE_READ | \
				_PAGE_WRITE | _PAGE_EXECUTE | _dflt_cache_att)
#define CACHEDEF	(CACHE_DEFAULT << 6)
extern pgd_t swapper_pg_dir[PTRS_PER_PGD];   
#ifdef CONFIG_HUGETLB_PAGE
#define pte_mkhuge(pte) __pte((pte_val(pte) & ~0x3) | HVM_HUGEPAGE_SIZE)
#endif
extern void sync_icache_dcache(pte_t pte);
#define pte_present_exec_user(pte) \
	((pte_val(pte) & (_PAGE_EXECUTE | _PAGE_USER)) == \
	(_PAGE_EXECUTE | _PAGE_USER))
static inline void set_pte(pte_t *ptep, pte_t pteval)
{
	if (pte_present_exec_user(pteval))
		sync_icache_dcache(pteval);
	*ptep = pteval;
}
#define _NULL_PMD	0x7
#define _NULL_PTE	0x0
static inline void pmd_clear(pmd_t *pmd_entry_ptr)
{
	 pmd_val(*pmd_entry_ptr) = _NULL_PMD;
}
static inline void pte_clear(struct mm_struct *mm, unsigned long addr,
				pte_t *ptep)
{
	pte_val(*ptep) = _NULL_PTE;
}
static inline int pmd_none(pmd_t pmd)
{
	return pmd_val(pmd) == _NULL_PMD;
}
static inline int pmd_present(pmd_t pmd)
{
	return pmd_val(pmd) != (unsigned long)_NULL_PMD;
}
static inline int pmd_bad(pmd_t pmd)
{
	return 0;
}
#define pmd_pfn(pmd)  (pmd_val(pmd) >> PAGE_SHIFT)
#define pmd_page(pmd)  (pfn_to_page(pmd_val(pmd) >> PAGE_SHIFT))
static inline int pte_none(pte_t pte)
{
	return pte_val(pte) == _NULL_PTE;
};
static inline int pte_present(pte_t pte)
{
	return pte_val(pte) & _PAGE_PRESENT;
}
#define mk_pte(page, pgprot) pfn_pte(page_to_pfn(page), (pgprot))
#define pte_page(x) pfn_to_page(pte_pfn(x))
static inline pte_t pte_mkold(pte_t pte)
{
	pte_val(pte) &= ~_PAGE_ACCESSED;
	return pte;
}
static inline pte_t pte_mkyoung(pte_t pte)
{
	pte_val(pte) |= _PAGE_ACCESSED;
	return pte;
}
static inline pte_t pte_mkclean(pte_t pte)
{
	pte_val(pte) &= ~_PAGE_DIRTY;
	return pte;
}
static inline pte_t pte_mkdirty(pte_t pte)
{
	pte_val(pte) |= _PAGE_DIRTY;
	return pte;
}
static inline int pte_young(pte_t pte)
{
	return pte_val(pte) & _PAGE_ACCESSED;
}
static inline int pte_dirty(pte_t pte)
{
	return pte_val(pte) & _PAGE_DIRTY;
}
static inline pte_t pte_modify(pte_t pte, pgprot_t prot)
{
	pte_val(pte) &= PAGE_MASK;
	pte_val(pte) |= pgprot_val(prot);
	return pte;
}
static inline pte_t pte_wrprotect(pte_t pte)
{
	pte_val(pte) &= ~_PAGE_WRITE;
	return pte;
}
static inline pte_t pte_mkwrite_novma(pte_t pte)
{
	pte_val(pte) |= _PAGE_WRITE;
	return pte;
}
static inline pte_t pte_mkexec(pte_t pte)
{
	pte_val(pte) |= _PAGE_EXECUTE;
	return pte;
}
static inline int pte_read(pte_t pte)
{
	return pte_val(pte) & _PAGE_READ;
}
static inline int pte_write(pte_t pte)
{
	return pte_val(pte) & _PAGE_WRITE;
}
static inline int pte_exec(pte_t pte)
{
	return pte_val(pte) & _PAGE_EXECUTE;
}
#define __pte_to_swp_entry(pte) ((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x) ((pte_t) { (x).val })
#define PFN_PTE_SHIFT	PAGE_SHIFT
#define pfn_pte(pfn, pgprot) __pte((pfn << PAGE_SHIFT) | pgprot_val(pgprot))
#define pte_pfn(pte) (pte_val(pte) >> PAGE_SHIFT)
#define set_pmd(pmdptr, pmdval) (*(pmdptr) = (pmdval))
static inline unsigned long pmd_page_vaddr(pmd_t pmd)
{
	return (unsigned long)__va(pmd_val(pmd) & PAGE_MASK);
}
#define ZERO_PAGE(vaddr) (virt_to_page(&empty_zero_page))
#define __swp_type(swp_pte)		(((swp_pte).val >> 1) & 0x1f)
#define __swp_offset(swp_pte) \
	((((swp_pte).val >> 7) & 0x7) | (((swp_pte).val >> 10) & 0x3ffff8))
#define __swp_entry(type, offset) \
	((swp_entry_t)	{ \
		(((type & 0x1f) << 1) | \
		 ((offset & 0x3ffff8) << 10) | ((offset & 0x7) << 7)) })
static inline int pte_swp_exclusive(pte_t pte)
{
	return pte_val(pte) & _PAGE_SWP_EXCLUSIVE;
}
static inline pte_t pte_swp_mkexclusive(pte_t pte)
{
	pte_val(pte) |= _PAGE_SWP_EXCLUSIVE;
	return pte;
}
static inline pte_t pte_swp_clear_exclusive(pte_t pte)
{
	pte_val(pte) &= ~_PAGE_SWP_EXCLUSIVE;
	return pte;
}
#endif
