#ifndef _PARISC_PGTABLE_H
#define _PARISC_PGTABLE_H
#include <asm/page.h>
#if CONFIG_PGTABLE_LEVELS == 3
#include <asm-generic/pgtable-nopud.h>
#elif CONFIG_PGTABLE_LEVELS == 2
#include <asm-generic/pgtable-nopmd.h>
#endif
#include <asm/fixmap.h>
#ifndef __ASSEMBLY__
#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/mm_types.h>
#include <asm/processor.h>
#include <asm/cache.h>
extern spinlock_t pa_tlb_flush_lock;
#if defined(CONFIG_64BIT) && defined(CONFIG_SMP)
extern int pa_serialize_tlb_flushes;
#else
#define pa_serialize_tlb_flushes        (0)
#endif
#define purge_tlb_start(flags)  do { \
	if (pa_serialize_tlb_flushes)	\
		spin_lock_irqsave(&pa_tlb_flush_lock, flags); \
	else \
		local_irq_save(flags);	\
	} while (0)
#define purge_tlb_end(flags)	do { \
	if (pa_serialize_tlb_flushes)	\
		spin_unlock_irqrestore(&pa_tlb_flush_lock, flags); \
	else \
		local_irq_restore(flags); \
	} while (0)
static inline void purge_tlb_entries(struct mm_struct *mm, unsigned long addr)
{
	unsigned long flags;
	purge_tlb_start(flags);
	mtsp(mm->context.space_id, SR_TEMP1);
	pdtlb(SR_TEMP1, addr);
	pitlb(SR_TEMP1, addr);
	purge_tlb_end(flags);
}
extern void __update_cache(pte_t pte);
#define set_pte(pteptr, pteval)			\
	do {					\
		*(pteptr) = (pteval);		\
		mb();				\
	} while(0)
#endif  
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, pte_val(e))
#if CONFIG_PGTABLE_LEVELS == 3
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08lx.\n", __FILE__, __LINE__, (unsigned long)pmd_val(e))
#endif
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, (unsigned long)pgd_val(e))
#if defined(CONFIG_64BIT)
#define KERNEL_INITIAL_ORDER	26	 
#else
#define KERNEL_INITIAL_ORDER	25	 
#endif
#define KERNEL_INITIAL_SIZE	(1 << KERNEL_INITIAL_ORDER)
#if CONFIG_PGTABLE_LEVELS == 3
#define PMD_TABLE_ORDER	1
#define PGD_TABLE_ORDER	0
#else
#define PGD_TABLE_ORDER	1
#endif
#define PLD_SHIFT	PAGE_SHIFT
#define PLD_SIZE	PAGE_SIZE
#define BITS_PER_PTE	(PAGE_SHIFT - BITS_PER_PTE_ENTRY)
#define PTRS_PER_PTE    (1UL << BITS_PER_PTE)
#if CONFIG_PGTABLE_LEVELS == 3
#define PMD_SHIFT       (PLD_SHIFT + BITS_PER_PTE)
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
#define BITS_PER_PMD	(PAGE_SHIFT + PMD_TABLE_ORDER - BITS_PER_PMD_ENTRY)
#define PTRS_PER_PMD    (1UL << BITS_PER_PMD)
#else
#define BITS_PER_PMD	0
#endif
#define PGDIR_SHIFT	(PLD_SHIFT + BITS_PER_PTE + BITS_PER_PMD)
#if (PGDIR_SHIFT + PAGE_SHIFT + PGD_TABLE_ORDER - BITS_PER_PGD_ENTRY) > BITS_PER_LONG
#define BITS_PER_PGD	(BITS_PER_LONG - PGDIR_SHIFT)
#else
#define BITS_PER_PGD	(PAGE_SHIFT + PGD_TABLE_ORDER - BITS_PER_PGD_ENTRY)
#endif
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))
#define PTRS_PER_PGD    (1UL << BITS_PER_PGD)
#define USER_PTRS_PER_PGD       PTRS_PER_PGD
#ifdef CONFIG_64BIT
#define MAX_ADDRBITS	(PGDIR_SHIFT + BITS_PER_PGD)
#define MAX_ADDRESS	(1UL << MAX_ADDRBITS)
#define SPACEID_SHIFT	(MAX_ADDRBITS - 32)
#else
#define MAX_ADDRBITS	(BITS_PER_LONG)
#define MAX_ADDRESS	(1ULL << MAX_ADDRBITS)
#define SPACEID_SHIFT	0
#endif
#if (KERNEL_INITIAL_ORDER) >= (PLD_SHIFT + BITS_PER_PTE)
# define PT_INITIAL	(1 << (KERNEL_INITIAL_ORDER - PLD_SHIFT - BITS_PER_PTE))
#else
# define PT_INITIAL	(1)   
#endif
#define _PAGE_READ_BIT     31    
#define _PAGE_WRITE_BIT    30    
#define _PAGE_EXEC_BIT     29    
#define _PAGE_GATEWAY_BIT  28    
#define _PAGE_DMB_BIT      27    
#define _PAGE_DIRTY_BIT    26    
#define _PAGE_REFTRAP_BIT  25    
#define _PAGE_NO_CACHE_BIT 24    
#define _PAGE_ACCESSED_BIT 23    
#define _PAGE_PRESENT_BIT  22    
#define _PAGE_HPAGE_BIT    21    
#define _PAGE_USER_BIT     20    
#ifdef CONFIG_HUGETLB_PAGE
#define _PAGE_SPECIAL_BIT  _PAGE_DMB_BIT   
#else
#define _PAGE_SPECIAL_BIT  _PAGE_HPAGE_BIT  
#endif
#define xlate_pabit(x) (31 - x)
#define PTE_SHIFT	   	xlate_pabit(_PAGE_USER_BIT)
#define PFN_PTE_SHIFT		12
#define _PAGE_READ     (1 << xlate_pabit(_PAGE_READ_BIT))
#define _PAGE_WRITE    (1 << xlate_pabit(_PAGE_WRITE_BIT))
#define _PAGE_RW       (_PAGE_READ | _PAGE_WRITE)
#define _PAGE_EXEC     (1 << xlate_pabit(_PAGE_EXEC_BIT))
#define _PAGE_GATEWAY  (1 << xlate_pabit(_PAGE_GATEWAY_BIT))
#define _PAGE_DMB      (1 << xlate_pabit(_PAGE_DMB_BIT))
#define _PAGE_DIRTY    (1 << xlate_pabit(_PAGE_DIRTY_BIT))
#define _PAGE_REFTRAP  (1 << xlate_pabit(_PAGE_REFTRAP_BIT))
#define _PAGE_NO_CACHE (1 << xlate_pabit(_PAGE_NO_CACHE_BIT))
#define _PAGE_ACCESSED (1 << xlate_pabit(_PAGE_ACCESSED_BIT))
#define _PAGE_PRESENT  (1 << xlate_pabit(_PAGE_PRESENT_BIT))
#define _PAGE_HUGE     (1 << xlate_pabit(_PAGE_HPAGE_BIT))
#define _PAGE_USER     (1 << xlate_pabit(_PAGE_USER_BIT))
#define _PAGE_SPECIAL  (1 << xlate_pabit(_PAGE_SPECIAL_BIT))
#define _PAGE_TABLE	(_PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_DIRTY | _PAGE_ACCESSED)
#define _PAGE_CHG_MASK	(PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_SPECIAL)
#define _PAGE_KERNEL_RO	(_PAGE_PRESENT | _PAGE_READ | _PAGE_DIRTY | _PAGE_ACCESSED)
#define _PAGE_KERNEL_EXEC	(_PAGE_KERNEL_RO | _PAGE_EXEC)
#define _PAGE_KERNEL_RWX	(_PAGE_KERNEL_EXEC | _PAGE_WRITE)
#define _PAGE_KERNEL		(_PAGE_KERNEL_RO | _PAGE_WRITE)
#define _PAGE_SWP_EXCLUSIVE	_PAGE_ACCESSED
#define _PxD_PRESENT_BIT   31
#define _PxD_VALID_BIT     30
#define PxD_FLAG_PRESENT  (1 << xlate_pabit(_PxD_PRESENT_BIT))
#define PxD_FLAG_VALID    (1 << xlate_pabit(_PxD_VALID_BIT))
#define PxD_FLAG_MASK     (0xf)
#define PxD_FLAG_SHIFT    (4)
#define PxD_VALUE_SHIFT   (PFN_PTE_SHIFT-PxD_FLAG_SHIFT)
#ifndef __ASSEMBLY__
#define PAGE_NONE	__pgprot(_PAGE_PRESENT | _PAGE_USER)
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | _PAGE_WRITE)
#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ)
#define PAGE_WRITEONLY  __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_WRITE)
#define PAGE_EXECREAD   __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | _PAGE_EXEC)
#define PAGE_COPY       PAGE_EXECREAD
#define PAGE_RWX        __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)
#define PAGE_KERNEL	__pgprot(_PAGE_KERNEL)
#define PAGE_KERNEL_EXEC	__pgprot(_PAGE_KERNEL_EXEC)
#define PAGE_KERNEL_RWX	__pgprot(_PAGE_KERNEL_RWX)
#define PAGE_KERNEL_RO	__pgprot(_PAGE_KERNEL_RO)
#define PAGE_KERNEL_UNC	__pgprot(_PAGE_KERNEL | _PAGE_NO_CACHE)
#define PAGE_GATEWAY    __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_GATEWAY| _PAGE_READ)
extern pgd_t swapper_pg_dir[];  
extern pte_t pg0[];
extern unsigned long *empty_zero_page;
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))
#define pte_none(x)     (pte_val(x) == 0)
#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)
#define pte_user(x)	(pte_val(x) & _PAGE_USER)
#define pte_clear(mm, addr, xp)  set_pte(xp, __pte(0))
#define pmd_flag(x)	(pmd_val(x) & PxD_FLAG_MASK)
#define pmd_address(x)	((unsigned long)(pmd_val(x) &~ PxD_FLAG_MASK) << PxD_VALUE_SHIFT)
#define pud_flag(x)	(pud_val(x) & PxD_FLAG_MASK)
#define pud_address(x)	((unsigned long)(pud_val(x) &~ PxD_FLAG_MASK) << PxD_VALUE_SHIFT)
#define pgd_flag(x)	(pgd_val(x) & PxD_FLAG_MASK)
#define pgd_address(x)	((unsigned long)(pgd_val(x) &~ PxD_FLAG_MASK) << PxD_VALUE_SHIFT)
#define pmd_none(x)	(!pmd_val(x))
#define pmd_bad(x)	(!(pmd_flag(x) & PxD_FLAG_VALID))
#define pmd_present(x)	(pmd_flag(x) & PxD_FLAG_PRESENT)
static inline void pmd_clear(pmd_t *pmd) {
		set_pmd(pmd,  __pmd(0));
}
#if CONFIG_PGTABLE_LEVELS == 3
#define pud_pgtable(pud) ((pmd_t *) __va(pud_address(pud)))
#define pud_page(pud)	virt_to_page((void *)pud_pgtable(pud))
#define pud_none(x)     (!pud_val(x))
#define pud_bad(x)      (!(pud_flag(x) & PxD_FLAG_VALID))
#define pud_present(x)  (pud_flag(x) & PxD_FLAG_PRESENT)
static inline void pud_clear(pud_t *pud) {
	set_pud(pud, __pud(0));
}
#endif
static inline int pte_dirty(pte_t pte)		{ return pte_val(pte) & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte)		{ return pte_val(pte) & _PAGE_ACCESSED; }
static inline int pte_write(pte_t pte)		{ return pte_val(pte) & _PAGE_WRITE; }
static inline int pte_special(pte_t pte)	{ return pte_val(pte) & _PAGE_SPECIAL; }
static inline pte_t pte_mkclean(pte_t pte)	{ pte_val(pte) &= ~_PAGE_DIRTY; return pte; }
static inline pte_t pte_mkold(pte_t pte)	{ pte_val(pte) &= ~_PAGE_ACCESSED; return pte; }
static inline pte_t pte_wrprotect(pte_t pte)	{ pte_val(pte) &= ~_PAGE_WRITE; return pte; }
static inline pte_t pte_mkdirty(pte_t pte)	{ pte_val(pte) |= _PAGE_DIRTY; return pte; }
static inline pte_t pte_mkyoung(pte_t pte)	{ pte_val(pte) |= _PAGE_ACCESSED; return pte; }
static inline pte_t pte_mkwrite_novma(pte_t pte)	{ pte_val(pte) |= _PAGE_WRITE; return pte; }
static inline pte_t pte_mkspecial(pte_t pte)	{ pte_val(pte) |= _PAGE_SPECIAL; return pte; }
#ifdef CONFIG_HUGETLB_PAGE
#define pte_huge(pte)           (pte_val(pte) & _PAGE_HUGE)
#define pte_mkhuge(pte)         (__pte(pte_val(pte) | \
				 (parisc_requires_coherency() ? 0 : _PAGE_HUGE)))
#else
#define pte_huge(pte)           (0)
#define pte_mkhuge(pte)         (pte)
#endif
#define __mk_pte(addr,pgprot) \
({									\
	pte_t __pte;							\
									\
	pte_val(__pte) = ((((addr)>>PAGE_SHIFT)<<PFN_PTE_SHIFT) + pgprot_val(pgprot));	\
									\
	__pte;								\
})
#define mk_pte(page, pgprot)	pfn_pte(page_to_pfn(page), (pgprot))
static inline pte_t pfn_pte(unsigned long pfn, pgprot_t pgprot)
{
	pte_t pte;
	pte_val(pte) = (pfn << PFN_PTE_SHIFT) | pgprot_val(pgprot);
	return pte;
}
static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{ pte_val(pte) = (pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot); return pte; }
#define pte_pfn(x)		(pte_val(x) >> PFN_PTE_SHIFT)
#define pte_page(pte)		(pfn_to_page(pte_pfn(pte)))
static inline unsigned long pmd_page_vaddr(pmd_t pmd)
{
	return ((unsigned long) __va(pmd_address(pmd)));
}
#define pmd_pfn(pmd)	(pmd_address(pmd) >> PAGE_SHIFT)
#define __pmd_page(pmd) ((unsigned long) __va(pmd_address(pmd)))
#define pmd_page(pmd)	virt_to_page((void *)__pmd_page(pmd))
extern void paging_init (void);
static inline void set_ptes(struct mm_struct *mm, unsigned long addr,
		pte_t *ptep, pte_t pte, unsigned int nr)
{
	if (pte_present(pte) && pte_user(pte))
		__update_cache(pte);
	for (;;) {
		*ptep = pte;
		purge_tlb_entries(mm, addr);
		if (--nr == 0)
			break;
		ptep++;
		pte_val(pte) += 1 << PFN_PTE_SHIFT;
		addr += PAGE_SIZE;
	}
}
#define set_ptes set_ptes
#define PG_dcache_dirty         PG_arch_1
#define update_mmu_cache_range(vmf, vma, addr, ptep, nr) __update_cache(*ptep)
#define update_mmu_cache(vma, addr, ptep) __update_cache(*ptep)
#define __swp_type(x)                     ((x).val & 0x1f)
#define __swp_offset(x)                   ( (((x).val >> 5) & 0x7) | \
					  (((x).val >> 10) << 3) )
#define __swp_entry(type, offset)         ((swp_entry_t) { \
					    ((type) & 0x1f) | \
					    ((offset & 0x7) << 5) | \
					    ((offset >> 3) << 10) })
#define __pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)		((pte_t) { (x).val })
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
static inline int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep)
{
	pte_t pte;
	if (!pte_young(*ptep))
		return 0;
	pte = *ptep;
	if (!pte_young(pte)) {
		return 0;
	}
	set_pte(ptep, pte_mkold(pte));
	return 1;
}
struct mm_struct;
static inline pte_t ptep_get_and_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	pte_t old_pte;
	old_pte = *ptep;
	set_pte(ptep, __pte(0));
	return old_pte;
}
static inline void ptep_set_wrprotect(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	set_pte(ptep, pte_wrprotect(*ptep));
}
#define pte_same(A,B)	(pte_val(A) == pte_val(B))
#endif  
#define _PAGE_SIZE_ENCODING_4K		0
#define _PAGE_SIZE_ENCODING_16K		1
#define _PAGE_SIZE_ENCODING_64K		2
#define _PAGE_SIZE_ENCODING_256K	3
#define _PAGE_SIZE_ENCODING_1M		4
#define _PAGE_SIZE_ENCODING_4M		5
#define _PAGE_SIZE_ENCODING_16M		6
#define _PAGE_SIZE_ENCODING_64M		7
#if defined(CONFIG_PARISC_PAGE_SIZE_4KB)
# define _PAGE_SIZE_ENCODING_DEFAULT _PAGE_SIZE_ENCODING_4K
#elif defined(CONFIG_PARISC_PAGE_SIZE_16KB)
# define _PAGE_SIZE_ENCODING_DEFAULT _PAGE_SIZE_ENCODING_16K
#elif defined(CONFIG_PARISC_PAGE_SIZE_64KB)
# define _PAGE_SIZE_ENCODING_DEFAULT _PAGE_SIZE_ENCODING_64K
#endif
#define pgprot_noncached(prot) __pgprot(pgprot_val(prot) | _PAGE_NO_CACHE)
#define HAVE_ARCH_UNMAPPED_AREA
#define HAVE_ARCH_UNMAPPED_AREA_TOPDOWN
#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
#define __HAVE_ARCH_PTEP_SET_WRPROTECT
#define __HAVE_ARCH_PTE_SAME
#endif  
