#ifndef _ASM_POWERPC_NOHASH_32_PGTABLE_H
#define _ASM_POWERPC_NOHASH_32_PGTABLE_H
#include <asm-generic/pgtable-nopmd.h>
#ifndef __ASSEMBLY__
#include <linux/sched.h>
#include <linux/threads.h>
#include <asm/mmu.h>			 
#ifdef CONFIG_44x
extern int icache_44x_need_flush;
#endif
#endif  
#define PTE_INDEX_SIZE	PTE_SHIFT
#define PMD_INDEX_SIZE	0
#define PUD_INDEX_SIZE	0
#define PGD_INDEX_SIZE	(32 - PGDIR_SHIFT)
#define PMD_CACHE_INDEX	PMD_INDEX_SIZE
#define PUD_CACHE_INDEX	PUD_INDEX_SIZE
#ifndef __ASSEMBLY__
#define PTE_TABLE_SIZE	(sizeof(pte_t) << PTE_INDEX_SIZE)
#define PMD_TABLE_SIZE	0
#define PUD_TABLE_SIZE	0
#define PGD_TABLE_SIZE	(sizeof(pgd_t) << PGD_INDEX_SIZE)
#define PMD_MASKED_BITS (PTE_TABLE_SIZE - 1)
#endif	 
#define PTRS_PER_PTE	(1 << PTE_INDEX_SIZE)
#define PTRS_PER_PGD	(1 << PGD_INDEX_SIZE)
#define PGDIR_SHIFT	(PAGE_SHIFT + PTE_INDEX_SIZE)
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))
#define PGD_MASKED_BITS		0
#define USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#define pte_ERROR(e) \
	pr_err("%s:%d: bad pte %llx.\n", __FILE__, __LINE__, \
		(unsigned long long)pte_val(e))
#define pgd_ERROR(e) \
	pr_err("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))
#ifndef __ASSEMBLY__
int map_kernel_page(unsigned long va, phys_addr_t pa, pgprot_t prot);
void unmap_kernel_page(unsigned long va);
#endif  
#include <asm/fixmap.h>
#ifdef CONFIG_HIGHMEM
#define IOREMAP_TOP	PKMAP_BASE
#else
#define IOREMAP_TOP	FIXADDR_START
#endif
#define IOREMAP_START	VMALLOC_START
#define IOREMAP_END	VMALLOC_END
#define VMALLOC_OFFSET (0x1000000)  
#ifdef PPC_PIN_SIZE
#define VMALLOC_START (((ALIGN((long)high_memory, PPC_PIN_SIZE) + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1)))
#else
#define VMALLOC_START ((((long)high_memory + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1)))
#endif
#ifdef CONFIG_KASAN_VMALLOC
#define VMALLOC_END	ALIGN_DOWN(ioremap_bot, PAGE_SIZE << KASAN_SHADOW_SCALE_SHIFT)
#else
#define VMALLOC_END	ioremap_bot
#endif
#if defined(CONFIG_40x)
#include <asm/nohash/32/pte-40x.h>
#elif defined(CONFIG_44x)
#include <asm/nohash/32/pte-44x.h>
#elif defined(CONFIG_PPC_85xx) && defined(CONFIG_PTE_64BIT)
#include <asm/nohash/pte-e500.h>
#elif defined(CONFIG_PPC_85xx)
#include <asm/nohash/32/pte-85xx.h>
#elif defined(CONFIG_PPC_8xx)
#include <asm/nohash/32/pte-8xx.h>
#endif
#ifndef PTE_RPN_SHIFT
#define PTE_RPN_SHIFT	(PAGE_SHIFT)
#endif
#if defined(CONFIG_PPC32) && defined(CONFIG_PTE_64BIT)
#define PTE_RPN_MASK	(~((1ULL << PTE_RPN_SHIFT) - 1))
#define MAX_POSSIBLE_PHYSMEM_BITS 36
#else
#define PTE_RPN_MASK	(~((1UL << PTE_RPN_SHIFT) - 1))
#define MAX_POSSIBLE_PHYSMEM_BITS 32
#endif
#define _PAGE_CHG_MASK	(PTE_RPN_MASK | _PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_SPECIAL)
#ifndef __ASSEMBLY__
#define pte_clear(mm, addr, ptep) \
	do { pte_update(mm, addr, ptep, ~0, 0, 0); } while (0)
#ifndef pte_mkwrite_novma
static inline pte_t pte_mkwrite_novma(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_RW);
}
#endif
static inline pte_t pte_mkdirty(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_DIRTY);
}
static inline pte_t pte_mkyoung(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_ACCESSED);
}
#ifndef pte_wrprotect
static inline pte_t pte_wrprotect(pte_t pte)
{
	return __pte(pte_val(pte) & ~_PAGE_RW);
}
#endif
#ifndef pte_mkexec
static inline pte_t pte_mkexec(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_EXEC);
}
#endif
#define pmd_none(pmd)		(!pmd_val(pmd))
#define	pmd_bad(pmd)		(pmd_val(pmd) & _PMD_BAD)
#define	pmd_present(pmd)	(pmd_val(pmd) & _PMD_PRESENT_MASK)
static inline void pmd_clear(pmd_t *pmdp)
{
	*pmdp = __pmd(0);
}
#ifdef CONFIG_PPC_8xx
static pmd_t *pmd_off(struct mm_struct *mm, unsigned long addr);
static int hugepd_ok(hugepd_t hpd);
static int number_of_cells_per_pte(pmd_t *pmd, pte_basic_t val, int huge)
{
	if (!huge)
		return PAGE_SIZE / SZ_4K;
	else if (hugepd_ok(*((hugepd_t *)pmd)))
		return 1;
	else if (IS_ENABLED(CONFIG_PPC_4K_PAGES) && !(val & _PAGE_HUGE))
		return SZ_16K / SZ_4K;
	else
		return SZ_512K / SZ_4K;
}
static inline pte_basic_t pte_update(struct mm_struct *mm, unsigned long addr, pte_t *p,
				     unsigned long clr, unsigned long set, int huge)
{
	pte_basic_t *entry = (pte_basic_t *)p;
	pte_basic_t old = pte_val(*p);
	pte_basic_t new = (old & ~(pte_basic_t)clr) | set;
	int num, i;
	pmd_t *pmd = pmd_off(mm, addr);
	num = number_of_cells_per_pte(pmd, new, huge);
	for (i = 0; i < num; i += PAGE_SIZE / SZ_4K, new += PAGE_SIZE) {
		*entry++ = new;
		if (IS_ENABLED(CONFIG_PPC_16K_PAGES) && num != 1) {
			*entry++ = new;
			*entry++ = new;
			*entry++ = new;
		}
	}
	return old;
}
#ifdef CONFIG_PPC_16K_PAGES
#define ptep_get ptep_get
static inline pte_t ptep_get(pte_t *ptep)
{
	pte_basic_t val = READ_ONCE(ptep->pte);
	pte_t pte = {val, val, val, val};
	return pte;
}
#endif  
#else
static inline pte_basic_t pte_update(struct mm_struct *mm, unsigned long addr, pte_t *p,
				     unsigned long clr, unsigned long set, int huge)
{
	pte_basic_t old = pte_val(*p);
	pte_basic_t new = (old & ~(pte_basic_t)clr) | set;
	*p = __pte(new);
#ifdef CONFIG_44x
	if ((old & _PAGE_USER) && (old & _PAGE_EXEC))
		icache_44x_need_flush = 1;
#endif
	return old;
}
#endif
#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
static inline int __ptep_test_and_clear_young(struct mm_struct *mm,
					      unsigned long addr, pte_t *ptep)
{
	unsigned long old;
	old = pte_update(mm, addr, ptep, _PAGE_ACCESSED, 0, 0);
	return (old & _PAGE_ACCESSED) != 0;
}
#define ptep_test_and_clear_young(__vma, __addr, __ptep) \
	__ptep_test_and_clear_young((__vma)->vm_mm, __addr, __ptep)
#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
static inline pte_t ptep_get_and_clear(struct mm_struct *mm, unsigned long addr,
				       pte_t *ptep)
{
	return __pte(pte_update(mm, addr, ptep, ~0, 0, 0));
}
#define __HAVE_ARCH_PTEP_SET_WRPROTECT
#ifndef ptep_set_wrprotect
static inline void ptep_set_wrprotect(struct mm_struct *mm, unsigned long addr,
				      pte_t *ptep)
{
	pte_update(mm, addr, ptep, _PAGE_RW, 0, 0);
}
#endif
#ifndef __ptep_set_access_flags
static inline void __ptep_set_access_flags(struct vm_area_struct *vma,
					   pte_t *ptep, pte_t entry,
					   unsigned long address,
					   int psize)
{
	unsigned long set = pte_val(entry) &
			    (_PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_RW | _PAGE_EXEC);
	int huge = psize > mmu_virtual_psize ? 1 : 0;
	pte_update(vma->vm_mm, address, ptep, 0, set, huge);
	flush_tlb_page(vma, address);
}
#endif
static inline int pte_young(pte_t pte)
{
	return pte_val(pte) & _PAGE_ACCESSED;
}
#ifndef CONFIG_BOOKE
#define pmd_pfn(pmd)		(pmd_val(pmd) >> PAGE_SHIFT)
#else
#define pmd_page_vaddr(pmd)	\
	((const void *)(pmd_val(pmd) & ~(PTE_TABLE_SIZE - 1)))
#define pmd_pfn(pmd)		(__pa(pmd_val(pmd)) >> PAGE_SHIFT)
#endif
#define pmd_page(pmd)		pfn_to_page(pmd_pfn(pmd))
#define __swp_type(entry)		((entry).val & 0x1f)
#define __swp_offset(entry)		((entry).val >> 5)
#define __swp_entry(type, offset)	((swp_entry_t) { ((type) & 0x1f) | ((offset) << 5) })
#define __pte_to_swp_entry(pte)		((swp_entry_t) { pte_val(pte) >> 3 })
#define __swp_entry_to_pte(x)		((pte_t) { (x).val << 3 })
#define _PAGE_SWP_EXCLUSIVE	0x000004
#endif  
#endif  
