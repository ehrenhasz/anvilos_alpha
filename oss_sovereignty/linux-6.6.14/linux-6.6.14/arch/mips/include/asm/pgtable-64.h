#ifndef _ASM_PGTABLE_64_H
#define _ASM_PGTABLE_64_H
#include <linux/compiler.h>
#include <linux/linkage.h>
#include <asm/addrspace.h>
#include <asm/page.h>
#include <asm/cachectl.h>
#include <asm/fixmap.h>
#if CONFIG_PGTABLE_LEVELS == 2
#include <asm-generic/pgtable-nopmd.h>
#elif CONFIG_PGTABLE_LEVELS == 3
#include <asm-generic/pgtable-nopud.h>
#else
#include <asm-generic/pgtable-nop4d.h>
#endif
#ifdef __PAGETABLE_PMD_FOLDED
#define PGDIR_SHIFT	(PAGE_SHIFT + PAGE_SHIFT - 3)
#else
#define PMD_SHIFT	(PAGE_SHIFT + (PAGE_SHIFT - 3))
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
# ifdef __PAGETABLE_PUD_FOLDED
# define PGDIR_SHIFT	(PMD_SHIFT + (PAGE_SHIFT + PMD_TABLE_ORDER - 3))
# endif
#endif
#ifndef __PAGETABLE_PUD_FOLDED
#define PUD_SHIFT	(PMD_SHIFT + (PAGE_SHIFT + PMD_TABLE_ORDER - 3))
#define PUD_SIZE	(1UL << PUD_SHIFT)
#define PUD_MASK	(~(PUD_SIZE-1))
#define PGDIR_SHIFT	(PUD_SHIFT + (PAGE_SHIFT + PUD_TABLE_ORDER - 3))
#endif
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))
#ifdef CONFIG_PAGE_SIZE_4KB
# ifdef CONFIG_MIPS_VA_BITS_48
#  define PGD_TABLE_ORDER	0
#  define PUD_TABLE_ORDER	0
# else
#  define PGD_TABLE_ORDER	1
#  define PUD_TABLE_ORDER	aieeee_attempt_to_allocate_pud
# endif
#define PMD_TABLE_ORDER		0
#endif
#ifdef CONFIG_PAGE_SIZE_8KB
#define PGD_TABLE_ORDER		0
#define PUD_TABLE_ORDER		aieeee_attempt_to_allocate_pud
#define PMD_TABLE_ORDER		0
#endif
#ifdef CONFIG_PAGE_SIZE_16KB
#ifdef CONFIG_MIPS_VA_BITS_48
#define PGD_TABLE_ORDER		1
#else
#define PGD_TABLE_ORDER		0
#endif
#define PUD_TABLE_ORDER		aieeee_attempt_to_allocate_pud
#define PMD_TABLE_ORDER		0
#endif
#ifdef CONFIG_PAGE_SIZE_32KB
#define PGD_TABLE_ORDER		0
#define PUD_TABLE_ORDER		aieeee_attempt_to_allocate_pud
#define PMD_TABLE_ORDER		0
#endif
#ifdef CONFIG_PAGE_SIZE_64KB
#define PGD_TABLE_ORDER		0
#define PUD_TABLE_ORDER		aieeee_attempt_to_allocate_pud
#ifdef CONFIG_MIPS_VA_BITS_48
#define PMD_TABLE_ORDER		0
#else
#define PMD_TABLE_ORDER		aieeee_attempt_to_allocate_pmd
#endif
#endif
#define PTRS_PER_PGD	((PAGE_SIZE << PGD_TABLE_ORDER) / sizeof(pgd_t))
#ifndef __PAGETABLE_PUD_FOLDED
#define PTRS_PER_PUD	((PAGE_SIZE << PUD_TABLE_ORDER) / sizeof(pud_t))
#endif
#ifndef __PAGETABLE_PMD_FOLDED
#define PTRS_PER_PMD	((PAGE_SIZE << PMD_TABLE_ORDER) / sizeof(pmd_t))
#endif
#define PTRS_PER_PTE	(PAGE_SIZE / sizeof(pte_t))
#define USER_PTRS_PER_PGD       ((TASK_SIZE64 / PGDIR_SIZE)?(TASK_SIZE64 / PGDIR_SIZE):1)
#define VMALLOC_START		(MAP_BASE + (2 * PAGE_SIZE))
#define VMALLOC_END	\
	(MAP_BASE + \
	 min(PTRS_PER_PGD * PTRS_PER_PUD * PTRS_PER_PMD * PTRS_PER_PTE * PAGE_SIZE, \
	     (1UL << cpu_vmbits)) - (1UL << 32))
#if defined(CONFIG_MODULES) && defined(KBUILD_64BIT_SYM32) && \
	VMALLOC_START != CKSSEG
#define MODULE_START	CKSSEG
#define MODULE_END	(FIXADDR_START-2*PAGE_SIZE)
#endif
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %016lx.\n", __FILE__, __LINE__, pte_val(e))
#ifndef __PAGETABLE_PMD_FOLDED
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %016lx.\n", __FILE__, __LINE__, pmd_val(e))
#endif
#ifndef __PAGETABLE_PUD_FOLDED
#define pud_ERROR(e) \
	printk("%s:%d: bad pud %016lx.\n", __FILE__, __LINE__, pud_val(e))
#endif
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %016lx.\n", __FILE__, __LINE__, pgd_val(e))
extern pte_t invalid_pte_table[PTRS_PER_PTE];
#ifndef __PAGETABLE_PUD_FOLDED
typedef struct { unsigned long pud; } pud_t;
#define pud_val(x)	((x).pud)
#define __pud(x)	((pud_t) { (x) })
extern pud_t invalid_pud_table[PTRS_PER_PUD];
static inline int p4d_none(p4d_t p4d)
{
	return p4d_val(p4d) == (unsigned long)invalid_pud_table;
}
static inline int p4d_bad(p4d_t p4d)
{
	if (unlikely(p4d_val(p4d) & ~PAGE_MASK))
		return 1;
	return 0;
}
static inline int p4d_present(p4d_t p4d)
{
	return p4d_val(p4d) != (unsigned long)invalid_pud_table;
}
static inline void p4d_clear(p4d_t *p4dp)
{
	p4d_val(*p4dp) = (unsigned long)invalid_pud_table;
}
static inline pud_t *p4d_pgtable(p4d_t p4d)
{
	return (pud_t *)p4d_val(p4d);
}
#define p4d_phys(p4d)		virt_to_phys((void *)p4d_val(p4d))
#define p4d_page(p4d)		(pfn_to_page(p4d_phys(p4d) >> PAGE_SHIFT))
#define p4d_index(address)	(((address) >> P4D_SHIFT) & (PTRS_PER_P4D - 1))
static inline void set_p4d(p4d_t *p4d, p4d_t p4dval)
{
	*p4d = p4dval;
}
#endif
#ifndef __PAGETABLE_PMD_FOLDED
typedef struct { unsigned long pmd; } pmd_t;
#define pmd_val(x)	((x).pmd)
#define __pmd(x)	((pmd_t) { (x) } )
extern pmd_t invalid_pmd_table[PTRS_PER_PMD];
#endif
static inline int pmd_none(pmd_t pmd)
{
	return pmd_val(pmd) == (unsigned long) invalid_pte_table;
}
static inline int pmd_bad(pmd_t pmd)
{
#ifdef CONFIG_MIPS_HUGE_TLB_SUPPORT
	if (unlikely(pmd_val(pmd) & _PAGE_HUGE))
		return 0;
#endif
	if (unlikely(pmd_val(pmd) & ~PAGE_MASK))
		return 1;
	return 0;
}
static inline int pmd_present(pmd_t pmd)
{
#ifdef CONFIG_MIPS_HUGE_TLB_SUPPORT
	if (unlikely(pmd_val(pmd) & _PAGE_HUGE))
		return pmd_val(pmd) & _PAGE_PRESENT;
#endif
	return pmd_val(pmd) != (unsigned long) invalid_pte_table;
}
static inline void pmd_clear(pmd_t *pmdp)
{
	pmd_val(*pmdp) = ((unsigned long) invalid_pte_table);
}
#ifndef __PAGETABLE_PMD_FOLDED
static inline int pud_none(pud_t pud)
{
	return pud_val(pud) == (unsigned long) invalid_pmd_table;
}
static inline int pud_bad(pud_t pud)
{
	return pud_val(pud) & ~PAGE_MASK;
}
static inline int pud_present(pud_t pud)
{
	return pud_val(pud) != (unsigned long) invalid_pmd_table;
}
static inline void pud_clear(pud_t *pudp)
{
	pud_val(*pudp) = ((unsigned long) invalid_pmd_table);
}
#endif
#define pte_page(x)		pfn_to_page(pte_pfn(x))
#define pte_pfn(x)		((unsigned long)((x).pte >> PFN_PTE_SHIFT))
#define pfn_pte(pfn, prot)	__pte(((pfn) << PFN_PTE_SHIFT) | pgprot_val(prot))
#define pfn_pmd(pfn, prot)	__pmd(((pfn) << PFN_PTE_SHIFT) | pgprot_val(prot))
#ifndef __PAGETABLE_PMD_FOLDED
static inline pmd_t *pud_pgtable(pud_t pud)
{
	return (pmd_t *)pud_val(pud);
}
#define pud_phys(pud)		virt_to_phys((void *)pud_val(pud))
#define pud_page(pud)		(pfn_to_page(pud_phys(pud) >> PAGE_SHIFT))
#endif
extern void pgd_init(void *addr);
extern void pud_init(void *addr);
extern void pmd_init(void *addr);
static inline pte_t mk_swap_pte(unsigned long type, unsigned long offset)
{ pte_t pte; pte_val(pte) = ((type & 0x7f) << 16) | (offset << 24); return pte; }
#define __swp_type(x)		(((x).val >> 16) & 0x7f)
#define __swp_offset(x)		((x).val >> 24)
#define __swp_entry(type, offset) ((swp_entry_t) { pte_val(mk_swap_pte((type), (offset))) })
#define __pte_to_swp_entry(pte) ((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val })
#define _PAGE_SWP_EXCLUSIVE	(1 << 23)
#endif  
