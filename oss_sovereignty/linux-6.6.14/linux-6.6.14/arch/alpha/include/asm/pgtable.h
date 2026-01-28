#ifndef _ALPHA_PGTABLE_H
#define _ALPHA_PGTABLE_H
#include <asm-generic/pgtable-nopud.h>
#include <linux/mmzone.h>
#include <asm/page.h>
#include <asm/processor.h>	 
#include <asm/machvec.h>
#include <asm/setup.h>
struct mm_struct;
struct vm_area_struct;
#define set_pte(pteptr, pteval) ((*(pteptr)) = (pteval))
#define PMD_SHIFT	(PAGE_SHIFT + (PAGE_SHIFT-3))
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
#define PGDIR_SHIFT	(PAGE_SHIFT + 2*(PAGE_SHIFT-3))
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))
#define PTRS_PER_PTE	(1UL << (PAGE_SHIFT-3))
#define PTRS_PER_PMD	(1UL << (PAGE_SHIFT-3))
#define PTRS_PER_PGD	(1UL << (PAGE_SHIFT-3))
#define USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#define PTRS_PER_PAGE	(1UL << (PAGE_SHIFT-3))
#ifdef CONFIG_ALPHA_LARGE_VMALLOC
#define VMALLOC_START		0xfffffe0000000000
#else
#define VMALLOC_START		(-2*PGDIR_SIZE)
#endif
#define VMALLOC_END		(-PGDIR_SIZE)
#define _PAGE_VALID	0x0001
#define _PAGE_FOR	0x0002	 
#define _PAGE_FOW	0x0004	 
#define _PAGE_FOE	0x0008	 
#define _PAGE_ASM	0x0010
#define _PAGE_KRE	0x0100	 
#define _PAGE_URE	0x0200	 
#define _PAGE_KWE	0x1000	 
#define _PAGE_UWE	0x2000	 
#define _PAGE_DIRTY	0x20000
#define _PAGE_ACCESSED	0x40000
#define _PAGE_SWP_EXCLUSIVE	0x8000000000UL
#define __DIRTY_BITS	(_PAGE_DIRTY | _PAGE_KWE | _PAGE_UWE)
#define __ACCESS_BITS	(_PAGE_ACCESSED | _PAGE_KRE | _PAGE_URE)
#define _PFN_MASK	0xFFFFFFFF00000000UL
#define _PAGE_TABLE	(_PAGE_VALID | __DIRTY_BITS | __ACCESS_BITS)
#define _PAGE_CHG_MASK	(_PFN_MASK | __DIRTY_BITS | __ACCESS_BITS)
#define PAGE_NONE	__pgprot(_PAGE_VALID | __ACCESS_BITS | _PAGE_FOR | _PAGE_FOW | _PAGE_FOE)
#define PAGE_SHARED	__pgprot(_PAGE_VALID | __ACCESS_BITS)
#define PAGE_COPY	__pgprot(_PAGE_VALID | __ACCESS_BITS | _PAGE_FOW)
#define PAGE_READONLY	__pgprot(_PAGE_VALID | __ACCESS_BITS | _PAGE_FOW)
#define PAGE_KERNEL	__pgprot(_PAGE_VALID | _PAGE_ASM | _PAGE_KRE | _PAGE_KWE)
#define _PAGE_NORMAL(x) __pgprot(_PAGE_VALID | __ACCESS_BITS | (x))
#define _PAGE_P(x) _PAGE_NORMAL((x) | (((x) & _PAGE_FOW)?0:_PAGE_FOW))
#define _PAGE_S(x) _PAGE_NORMAL(x)
#define pgprot_noncached(prot)	(prot)
extern pte_t __bad_page(void);
extern pmd_t * __bad_pagetable(void);
extern unsigned long __zero_page(void);
#define BAD_PAGETABLE	__bad_pagetable()
#define BAD_PAGE	__bad_page()
#define ZERO_PAGE(vaddr)	(virt_to_page(ZERO_PGE))
#define BITS_PER_PTR			(8*sizeof(unsigned long))
#define PTR_MASK			(~(sizeof(void*)-1))
#define SIZEOF_PTR_LOG2			3
#define PAGE_PTR(address)		\
  ((unsigned long)(address)>>(PAGE_SHIFT-SIZEOF_PTR_LOG2)&PTR_MASK&~PAGE_MASK)
#if defined(CONFIG_ALPHA_GENERIC) && defined(USE_48_BIT_KSEG)
#error "EV6-only feature in a generic kernel"
#endif
#if defined(CONFIG_ALPHA_GENERIC) || \
    (defined(CONFIG_ALPHA_EV6) && !defined(USE_48_BIT_KSEG))
#define KSEG_PFN	(0xc0000000000UL >> PAGE_SHIFT)
#define PHYS_TWIDDLE(pfn) \
  ((((pfn) & KSEG_PFN) == (0x40000000000UL >> PAGE_SHIFT)) \
  ? ((pfn) ^= KSEG_PFN) : (pfn))
#else
#define PHYS_TWIDDLE(pfn) (pfn)
#endif
#define page_to_pa(page)	(page_to_pfn(page) << PAGE_SHIFT)
#define PFN_PTE_SHIFT		32
#define pte_pfn(pte)		(pte_val(pte) >> PFN_PTE_SHIFT)
#define pte_page(pte)	pfn_to_page(pte_pfn(pte))
#define mk_pte(page, pgprot)						\
({									\
	pte_t pte;							\
									\
	pte_val(pte) = (page_to_pfn(page) << 32) | pgprot_val(pgprot);	\
	pte;								\
})
extern inline pte_t pfn_pte(unsigned long physpfn, pgprot_t pgprot)
{ pte_t pte; pte_val(pte) = (PHYS_TWIDDLE(physpfn) << 32) | pgprot_val(pgprot); return pte; }
extern inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{ pte_val(pte) = (pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot); return pte; }
extern inline void pmd_set(pmd_t * pmdp, pte_t * ptep)
{ pmd_val(*pmdp) = _PAGE_TABLE | ((((unsigned long) ptep) - PAGE_OFFSET) << (32-PAGE_SHIFT)); }
extern inline void pud_set(pud_t * pudp, pmd_t * pmdp)
{ pud_val(*pudp) = _PAGE_TABLE | ((((unsigned long) pmdp) - PAGE_OFFSET) << (32-PAGE_SHIFT)); }
extern inline unsigned long
pmd_page_vaddr(pmd_t pmd)
{
	return ((pmd_val(pmd) & _PFN_MASK) >> (32-PAGE_SHIFT)) + PAGE_OFFSET;
}
#define pmd_pfn(pmd)	(pmd_val(pmd) >> 32)
#define pmd_page(pmd)	(pfn_to_page(pmd_val(pmd) >> 32))
#define pud_page(pud)	(pfn_to_page(pud_val(pud) >> 32))
extern inline pmd_t *pud_pgtable(pud_t pgd)
{
	return (pmd_t *)(PAGE_OFFSET + ((pud_val(pgd) & _PFN_MASK) >> (32-PAGE_SHIFT)));
}
extern inline int pte_none(pte_t pte)		{ return !pte_val(pte); }
extern inline int pte_present(pte_t pte)	{ return pte_val(pte) & _PAGE_VALID; }
extern inline void pte_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	pte_val(*ptep) = 0;
}
extern inline int pmd_none(pmd_t pmd)		{ return !pmd_val(pmd); }
extern inline int pmd_bad(pmd_t pmd)		{ return (pmd_val(pmd) & ~_PFN_MASK) != _PAGE_TABLE; }
extern inline int pmd_present(pmd_t pmd)	{ return pmd_val(pmd) & _PAGE_VALID; }
extern inline void pmd_clear(pmd_t * pmdp)	{ pmd_val(*pmdp) = 0; }
extern inline int pud_none(pud_t pud)		{ return !pud_val(pud); }
extern inline int pud_bad(pud_t pud)		{ return (pud_val(pud) & ~_PFN_MASK) != _PAGE_TABLE; }
extern inline int pud_present(pud_t pud)	{ return pud_val(pud) & _PAGE_VALID; }
extern inline void pud_clear(pud_t * pudp)	{ pud_val(*pudp) = 0; }
extern inline int pte_write(pte_t pte)		{ return !(pte_val(pte) & _PAGE_FOW); }
extern inline int pte_dirty(pte_t pte)		{ return pte_val(pte) & _PAGE_DIRTY; }
extern inline int pte_young(pte_t pte)		{ return pte_val(pte) & _PAGE_ACCESSED; }
extern inline pte_t pte_wrprotect(pte_t pte)	{ pte_val(pte) |= _PAGE_FOW; return pte; }
extern inline pte_t pte_mkclean(pte_t pte)	{ pte_val(pte) &= ~(__DIRTY_BITS); return pte; }
extern inline pte_t pte_mkold(pte_t pte)	{ pte_val(pte) &= ~(__ACCESS_BITS); return pte; }
extern inline pte_t pte_mkwrite_novma(pte_t pte){ pte_val(pte) &= ~_PAGE_FOW; return pte; }
extern inline pte_t pte_mkdirty(pte_t pte)	{ pte_val(pte) |= __DIRTY_BITS; return pte; }
extern inline pte_t pte_mkyoung(pte_t pte)	{ pte_val(pte) |= __ACCESS_BITS; return pte; }
extern inline pmd_t * pmd_offset(pud_t * dir, unsigned long address)
{
	pmd_t *ret = pud_pgtable(*dir) + ((address >> PMD_SHIFT) & (PTRS_PER_PAGE - 1));
	smp_rmb();  
	return ret;
}
#define pmd_offset pmd_offset
extern inline pte_t * pte_offset_kernel(pmd_t * dir, unsigned long address)
{
	pte_t *ret = (pte_t *) pmd_page_vaddr(*dir)
		+ ((address >> PAGE_SHIFT) & (PTRS_PER_PAGE - 1));
	smp_rmb();  
	return ret;
}
#define pte_offset_kernel pte_offset_kernel
extern pgd_t swapper_pg_dir[1024];
extern inline void update_mmu_cache(struct vm_area_struct * vma,
	unsigned long address, pte_t *ptep)
{
}
static inline void update_mmu_cache_range(struct vm_fault *vmf,
		struct vm_area_struct *vma, unsigned long address,
		pte_t *ptep, unsigned int nr)
{
}
extern inline pte_t mk_swap_pte(unsigned long type, unsigned long offset)
{ pte_t pte; pte_val(pte) = ((type & 0x7f) << 32) | (offset << 40); return pte; }
#define __swp_type(x)		(((x).val >> 32) & 0x7f)
#define __swp_offset(x)		((x).val >> 40)
#define __swp_entry(type, off)	((swp_entry_t) { pte_val(mk_swap_pte((type), (off))) })
#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val })
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
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %016lx.\n", __FILE__, __LINE__, pte_val(e))
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %016lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %016lx.\n", __FILE__, __LINE__, pgd_val(e))
extern void paging_init(void);
#define HAVE_ARCH_UNMAPPED_AREA
#endif  
