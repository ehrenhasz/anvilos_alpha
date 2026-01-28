#ifndef _ASM_NIOS2_PGTABLE_H
#define _ASM_NIOS2_PGTABLE_H
#include <linux/io.h>
#include <linux/bug.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/pgtable-bits.h>
#include <asm-generic/pgtable-nopmd.h>
#define VMALLOC_START		CONFIG_NIOS2_KERNEL_MMU_REGION_BASE
#define VMALLOC_END		(CONFIG_NIOS2_KERNEL_REGION_BASE - 1)
struct mm_struct;
#define MKP(x, w, r) __pgprot(_PAGE_PRESENT | _PAGE_CACHED |		\
				((x) ? _PAGE_EXEC : 0) |		\
				((r) ? _PAGE_READ : 0) |		\
				((w) ? _PAGE_WRITE : 0))
#define PAGE_KERNEL __pgprot(_PAGE_PRESENT | _PAGE_CACHED | _PAGE_READ | \
			     _PAGE_WRITE | _PAGE_EXEC | _PAGE_GLOBAL)
#define PAGE_SHARED __pgprot(_PAGE_PRESENT | _PAGE_CACHED | _PAGE_READ | \
			     _PAGE_WRITE | _PAGE_ACCESSED)
#define PAGE_COPY MKP(0, 0, 1)
#define PTRS_PER_PGD	(PAGE_SIZE / sizeof(pgd_t))
#define PTRS_PER_PTE	(PAGE_SIZE / sizeof(pte_t))
#define USER_PTRS_PER_PGD	\
	(CONFIG_NIOS2_KERNEL_MMU_REGION_BASE / PGDIR_SIZE)
#define PGDIR_SHIFT	22
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))
extern unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)];
#define ZERO_PAGE(vaddr)	(virt_to_page(empty_zero_page))
extern pgd_t swapper_pg_dir[PTRS_PER_PGD];
extern pte_t invalid_pte_table[PAGE_SIZE/sizeof(pte_t)];
static inline void set_pmd(pmd_t *pmdptr, pmd_t pmdval)
{
	*pmdptr = pmdval;
}
static inline int pte_write(pte_t pte)		\
	{ return pte_val(pte) & _PAGE_WRITE; }
static inline int pte_dirty(pte_t pte)		\
	{ return pte_val(pte) & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte)		\
	{ return pte_val(pte) & _PAGE_ACCESSED; }
#define pgprot_noncached pgprot_noncached
static inline pgprot_t pgprot_noncached(pgprot_t _prot)
{
	unsigned long prot = pgprot_val(_prot);
	prot &= ~_PAGE_CACHED;
	return __pgprot(prot);
}
static inline int pte_none(pte_t pte)
{
	return !(pte_val(pte) & ~(_PAGE_GLOBAL|0xf));
}
static inline int pte_present(pte_t pte)	\
	{ return pte_val(pte) & _PAGE_PRESENT; }
static inline pte_t pte_wrprotect(pte_t pte)
{
	pte_val(pte) &= ~_PAGE_WRITE;
	return pte;
}
static inline pte_t pte_mkclean(pte_t pte)
{
	pte_val(pte) &= ~_PAGE_DIRTY;
	return pte;
}
static inline pte_t pte_mkold(pte_t pte)
{
	pte_val(pte) &= ~_PAGE_ACCESSED;
	return pte;
}
static inline pte_t pte_mkwrite_novma(pte_t pte)
{
	pte_val(pte) |= _PAGE_WRITE;
	return pte;
}
static inline pte_t pte_mkdirty(pte_t pte)
{
	pte_val(pte) |= _PAGE_DIRTY;
	return pte;
}
static inline pte_t pte_mkyoung(pte_t pte)
{
	pte_val(pte) |= _PAGE_ACCESSED;
	return pte;
}
static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	const unsigned long mask = _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC;
	pte_val(pte) = (pte_val(pte) & ~mask) | (pgprot_val(newprot) & mask);
	return pte;
}
static inline int pmd_present(pmd_t pmd)
{
	return (pmd_val(pmd) != (unsigned long) invalid_pte_table)
			&& (pmd_val(pmd) != 0UL);
}
static inline void pmd_clear(pmd_t *pmdp)
{
	pmd_val(*pmdp) = (unsigned long) invalid_pte_table;
}
#define pte_pfn(pte)		(pte_val(pte) & 0xfffff)
#define pfn_pte(pfn, prot)	(__pte(pfn | pgprot_val(prot)))
#define pte_page(pte)		(pfn_to_page(pte_pfn(pte)))
static inline void set_pte(pte_t *ptep, pte_t pteval)
{
	*ptep = pteval;
}
static inline void set_ptes(struct mm_struct *mm, unsigned long addr,
		pte_t *ptep, pte_t pte, unsigned int nr)
{
	unsigned long paddr = (unsigned long)page_to_virt(pte_page(pte));
	flush_dcache_range(paddr, paddr + nr * PAGE_SIZE);
	for (;;) {
		set_pte(ptep, pte);
		if (--nr == 0)
			break;
		ptep++;
		pte_val(pte) += 1;
	}
}
#define set_ptes set_ptes
static inline int pmd_none(pmd_t pmd)
{
	return (pmd_val(pmd) ==
		(unsigned long) invalid_pte_table) || (pmd_val(pmd) == 0UL);
}
#define pmd_bad(pmd)	(pmd_val(pmd) & ~PAGE_MASK)
static inline void pte_clear(struct mm_struct *mm,
				unsigned long addr, pte_t *ptep)
{
	pte_t null;
	pte_val(null) = (addr >> PAGE_SHIFT) & 0xf;
	set_pte(ptep, null);
}
#define mk_pte(page, prot)	(pfn_pte(page_to_pfn(page), prot))
#define pmd_phys(pmd)		virt_to_phys((void *)pmd_val(pmd))
#define pmd_pfn(pmd)		(pmd_phys(pmd) >> PAGE_SHIFT)
#define pmd_page(pmd)		(pfn_to_page(pmd_phys(pmd) >> PAGE_SHIFT))
static inline unsigned long pmd_page_vaddr(pmd_t pmd)
{
	return pmd_val(pmd);
}
#define pte_ERROR(e) \
	pr_err("%s:%d: bad pte %08lx.\n", \
		__FILE__, __LINE__, pte_val(e))
#define pgd_ERROR(e) \
	pr_err("%s:%d: bad pgd %08lx.\n", \
		__FILE__, __LINE__, pgd_val(e))
#define __swp_type(swp)		(((swp).val >> 26) & 0x1f)
#define __swp_offset(swp)	((swp).val & 0xfffff)
#define __swp_entry(type, off)	((swp_entry_t) { (((type) & 0x1f) << 26) \
						 | ((off) & 0xfffff) })
#define __swp_entry_to_pte(swp)	((pte_t) { (swp).val })
#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
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
extern void __init paging_init(void);
extern void __init mmu_init(void);
void update_mmu_cache_range(struct vm_fault *vmf, struct vm_area_struct *vma,
		unsigned long address, pte_t *ptep, unsigned int nr);
#define update_mmu_cache(vma, addr, ptep) \
	update_mmu_cache_range(NULL, vma, addr, ptep, 1)
#endif  
