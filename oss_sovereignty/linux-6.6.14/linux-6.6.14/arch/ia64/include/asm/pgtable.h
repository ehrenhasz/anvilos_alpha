#ifndef _ASM_IA64_PGTABLE_H
#define _ASM_IA64_PGTABLE_H
#include <asm/mman.h>
#include <asm/page.h>
#include <asm/processor.h>
#include <asm/types.h>
#define IA64_MAX_PHYS_BITS	50	 
#define _PAGE_P_BIT		0
#define _PAGE_A_BIT		5
#define _PAGE_D_BIT		6
#define _PAGE_P			(1 << _PAGE_P_BIT)	 
#define _PAGE_MA_WB		(0x0 <<  2)	 
#define _PAGE_MA_UC		(0x4 <<  2)	 
#define _PAGE_MA_UCE		(0x5 <<  2)	 
#define _PAGE_MA_WC		(0x6 <<  2)	 
#define _PAGE_MA_NAT		(0x7 <<  2)	 
#define _PAGE_MA_MASK		(0x7 <<  2)
#define _PAGE_PL_0		(0 <<  7)	 
#define _PAGE_PL_1		(1 <<  7)	 
#define _PAGE_PL_2		(2 <<  7)	 
#define _PAGE_PL_3		(3 <<  7)	 
#define _PAGE_PL_MASK		(3 <<  7)
#define _PAGE_AR_R		(0 <<  9)	 
#define _PAGE_AR_RX		(1 <<  9)	 
#define _PAGE_AR_RW		(2 <<  9)	 
#define _PAGE_AR_RWX		(3 <<  9)	 
#define _PAGE_AR_R_RW		(4 <<  9)	 
#define _PAGE_AR_RX_RWX		(5 <<  9)	 
#define _PAGE_AR_RWX_RW		(6 <<  9)	 
#define _PAGE_AR_X_RX		(7 <<  9)	 
#define _PAGE_AR_MASK		(7 <<  9)
#define _PAGE_AR_SHIFT		9
#define _PAGE_A			(1 << _PAGE_A_BIT)	 
#define _PAGE_D			(1 << _PAGE_D_BIT)	 
#define _PAGE_PPN_MASK		(((__IA64_UL(1) << IA64_MAX_PHYS_BITS) - 1) & ~0xfffUL)
#define _PAGE_ED		(__IA64_UL(1) << 52)	 
#define _PAGE_PROTNONE		(__IA64_UL(1) << 63)
#define _PAGE_SWP_EXCLUSIVE	(1 << 7)
#define _PFN_MASK		_PAGE_PPN_MASK
#define _PAGE_CHG_MASK	(_PAGE_P | _PAGE_PROTNONE | _PAGE_PL_MASK | _PAGE_AR_MASK | _PAGE_ED)
#define _PAGE_SIZE_4K	12
#define _PAGE_SIZE_8K	13
#define _PAGE_SIZE_16K	14
#define _PAGE_SIZE_64K	16
#define _PAGE_SIZE_256K	18
#define _PAGE_SIZE_1M	20
#define _PAGE_SIZE_4M	22
#define _PAGE_SIZE_16M	24
#define _PAGE_SIZE_64M	26
#define _PAGE_SIZE_256M	28
#define _PAGE_SIZE_1G	30
#define _PAGE_SIZE_4G	32
#define __ACCESS_BITS		_PAGE_ED | _PAGE_A | _PAGE_P | _PAGE_MA_WB
#define __DIRTY_BITS_NO_ED	_PAGE_A | _PAGE_P | _PAGE_D | _PAGE_MA_WB
#define __DIRTY_BITS		_PAGE_ED | __DIRTY_BITS_NO_ED
#define PTRS_PER_PTD_SHIFT	(PAGE_SHIFT-3)
#define PTRS_PER_PTE	(__IA64_UL(1) << (PTRS_PER_PTD_SHIFT))
#define PMD_SHIFT	(PAGE_SHIFT + (PTRS_PER_PTD_SHIFT))
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
#define PTRS_PER_PMD	(1UL << (PTRS_PER_PTD_SHIFT))
#if CONFIG_PGTABLE_LEVELS == 4
#define PUD_SHIFT	(PMD_SHIFT + (PTRS_PER_PTD_SHIFT))
#define PUD_SIZE	(1UL << PUD_SHIFT)
#define PUD_MASK	(~(PUD_SIZE-1))
#define PTRS_PER_PUD	(1UL << (PTRS_PER_PTD_SHIFT))
#endif
#if CONFIG_PGTABLE_LEVELS == 4
#define PGDIR_SHIFT		(PUD_SHIFT + (PTRS_PER_PTD_SHIFT))
#else
#define PGDIR_SHIFT		(PMD_SHIFT + (PTRS_PER_PTD_SHIFT))
#endif
#define PGDIR_SIZE		(__IA64_UL(1) << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE-1))
#define PTRS_PER_PGD_SHIFT	PTRS_PER_PTD_SHIFT
#define PTRS_PER_PGD		(1UL << PTRS_PER_PGD_SHIFT)
#define USER_PTRS_PER_PGD	(5*PTRS_PER_PGD/8)	 
#define PAGE_NONE	__pgprot(_PAGE_PROTNONE | _PAGE_A)
#define PAGE_SHARED	__pgprot(__ACCESS_BITS | _PAGE_PL_3 | _PAGE_AR_RW)
#define PAGE_READONLY	__pgprot(__ACCESS_BITS | _PAGE_PL_3 | _PAGE_AR_R)
#define PAGE_COPY	__pgprot(__ACCESS_BITS | _PAGE_PL_3 | _PAGE_AR_R)
#define PAGE_COPY_EXEC	__pgprot(__ACCESS_BITS | _PAGE_PL_3 | _PAGE_AR_RX)
#define PAGE_GATE	__pgprot(__ACCESS_BITS | _PAGE_PL_0 | _PAGE_AR_X_RX)
#define PAGE_KERNEL	__pgprot(__DIRTY_BITS  | _PAGE_PL_0 | _PAGE_AR_RWX)
#define PAGE_KERNELRX	__pgprot(__ACCESS_BITS | _PAGE_PL_0 | _PAGE_AR_RX)
#define PAGE_KERNEL_UC	__pgprot(__DIRTY_BITS  | _PAGE_PL_0 | _PAGE_AR_RWX | \
				 _PAGE_MA_UC)
# ifndef __ASSEMBLY__
#include <linux/sched/mm.h>	 
#include <linux/bitops.h>
#include <asm/cacheflush.h>
#include <asm/mmu_context.h>
#define pgd_ERROR(e)	printk("%s:%d: bad pgd %016lx.\n", __FILE__, __LINE__, pgd_val(e))
#if CONFIG_PGTABLE_LEVELS == 4
#define pud_ERROR(e)	printk("%s:%d: bad pud %016lx.\n", __FILE__, __LINE__, pud_val(e))
#endif
#define pmd_ERROR(e)	printk("%s:%d: bad pmd %016lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pte_ERROR(e)	printk("%s:%d: bad pte %016lx.\n", __FILE__, __LINE__, pte_val(e))
static inline long
ia64_phys_addr_valid (unsigned long addr)
{
	return (addr & (local_cpu_data->unimpl_pa_mask)) == 0;
}
#define VMALLOC_START		(RGN_BASE(RGN_GATE) + 0x200000000UL)
#if defined(CONFIG_SPARSEMEM) && defined(CONFIG_SPARSEMEM_VMEMMAP)
# define VMALLOC_END		(RGN_BASE(RGN_GATE) + (1UL << (4*PAGE_SHIFT - 10)))
# define vmemmap		((struct page *)VMALLOC_END)
#else
# define VMALLOC_END		(RGN_BASE(RGN_GATE) + (1UL << (4*PAGE_SHIFT - 9)))
#endif
#define	kc_vaddr_to_offset(v) ((v) - RGN_BASE(RGN_GATE))
#define	kc_offset_to_vaddr(o) ((o) + RGN_BASE(RGN_GATE))
#define RGN_MAP_SHIFT (PGDIR_SHIFT + PTRS_PER_PGD_SHIFT - 3)
#define RGN_MAP_LIMIT	((1UL << RGN_MAP_SHIFT) - PAGE_SIZE)	 
#define PFN_PTE_SHIFT	PAGE_SHIFT
#define pfn_pte(pfn, pgprot) \
({ pte_t __pte; pte_val(__pte) = ((pfn) << PAGE_SHIFT) | pgprot_val(pgprot); __pte; })
#define pte_pfn(_pte)		((pte_val(_pte) & _PFN_MASK) >> PAGE_SHIFT)
#define mk_pte(page, pgprot)	pfn_pte(page_to_pfn(page), (pgprot))
#define mk_pte_phys(physpage, pgprot) \
({ pte_t __pte; pte_val(__pte) = physpage + pgprot_val(pgprot); __pte; })
#define pte_modify(_pte, newprot) \
	(__pte((pte_val(_pte) & ~_PAGE_CHG_MASK) | (pgprot_val(newprot) & _PAGE_CHG_MASK)))
#define pte_none(pte) 			(!pte_val(pte))
#define pte_present(pte)		(pte_val(pte) & (_PAGE_P | _PAGE_PROTNONE))
#define pte_clear(mm,addr,pte)		(pte_val(*(pte)) = 0UL)
#define pte_page(pte)			virt_to_page(((pte_val(pte) & _PFN_MASK) + PAGE_OFFSET))
#define pmd_none(pmd)			(!pmd_val(pmd))
#define pmd_bad(pmd)			(!ia64_phys_addr_valid(pmd_val(pmd)))
#define pmd_present(pmd)		(pmd_val(pmd) != 0UL)
#define pmd_clear(pmdp)			(pmd_val(*(pmdp)) = 0UL)
#define pmd_page_vaddr(pmd)		((unsigned long) __va(pmd_val(pmd) & _PFN_MASK))
#define pmd_pfn(pmd)			((pmd_val(pmd) & _PFN_MASK) >> PAGE_SHIFT)
#define pmd_page(pmd)			virt_to_page((pmd_val(pmd) + PAGE_OFFSET))
#define pud_none(pud)			(!pud_val(pud))
#define pud_bad(pud)			(!ia64_phys_addr_valid(pud_val(pud)))
#define pud_present(pud)		(pud_val(pud) != 0UL)
#define pud_clear(pudp)			(pud_val(*(pudp)) = 0UL)
#define pud_pgtable(pud)		((pmd_t *) __va(pud_val(pud) & _PFN_MASK))
#define pud_page(pud)			virt_to_page((pud_val(pud) + PAGE_OFFSET))
#if CONFIG_PGTABLE_LEVELS == 4
#define p4d_none(p4d)			(!p4d_val(p4d))
#define p4d_bad(p4d)			(!ia64_phys_addr_valid(p4d_val(p4d)))
#define p4d_present(p4d)		(p4d_val(p4d) != 0UL)
#define p4d_clear(p4dp)			(p4d_val(*(p4dp)) = 0UL)
#define p4d_pgtable(p4d)		((pud_t *) __va(p4d_val(p4d) & _PFN_MASK))
#define p4d_page(p4d)			virt_to_page((p4d_val(p4d) + PAGE_OFFSET))
#endif
#define pte_write(pte)	((unsigned) (((pte_val(pte) & _PAGE_AR_MASK) >> _PAGE_AR_SHIFT) - 2) <= 4)
#define pte_exec(pte)		((pte_val(pte) & _PAGE_AR_RX) != 0)
#define pte_dirty(pte)		((pte_val(pte) & _PAGE_D) != 0)
#define pte_young(pte)		((pte_val(pte) & _PAGE_A) != 0)
#define pte_wrprotect(pte)	(__pte(pte_val(pte) & ~_PAGE_AR_RW))
#define pte_mkwrite_novma(pte)	(__pte(pte_val(pte) | _PAGE_AR_RW))
#define pte_mkold(pte)		(__pte(pte_val(pte) & ~_PAGE_A))
#define pte_mkyoung(pte)	(__pte(pte_val(pte) | _PAGE_A))
#define pte_mkclean(pte)	(__pte(pte_val(pte) & ~_PAGE_D))
#define pte_mkdirty(pte)	(__pte(pte_val(pte) | _PAGE_D))
#define pte_mkhuge(pte)		(__pte(pte_val(pte)))
#define pte_present_exec_user(pte)\
	((pte_val(pte) & (_PAGE_P | _PAGE_PL_MASK | _PAGE_AR_RX)) == \
		(_PAGE_P | _PAGE_PL_3 | _PAGE_AR_RX))
extern void __ia64_sync_icache_dcache(pte_t pteval);
static inline void set_pte(pte_t *ptep, pte_t pteval)
{
	if (pte_present_exec_user(pteval) &&
	    (!pte_present(*ptep) ||
		pte_pfn(*ptep) != pte_pfn(pteval)))
		__ia64_sync_icache_dcache(pteval);
	*ptep = pteval;
}
#define pgprot_cacheable(prot)		__pgprot((pgprot_val(prot) & ~_PAGE_MA_MASK) | _PAGE_MA_WB)
#define pgprot_noncached(prot)		__pgprot((pgprot_val(prot) & ~_PAGE_MA_MASK) | _PAGE_MA_UC)
#define pgprot_writecombine(prot)	__pgprot((pgprot_val(prot) & ~_PAGE_MA_MASK) | _PAGE_MA_WC)
struct file;
extern pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
				     unsigned long size, pgprot_t vma_prot);
#define __HAVE_PHYS_MEM_ACCESS_PROT
static inline unsigned long
pgd_index (unsigned long address)
{
	unsigned long region = address >> 61;
	unsigned long l1index = (address >> PGDIR_SHIFT) & ((PTRS_PER_PGD >> 3) - 1);
	return (region << (PAGE_SHIFT - 6)) | l1index;
}
#define pgd_index pgd_index
#define pgd_offset_k(addr) \
	(init_mm.pgd + (((addr) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1)))
#define pgd_offset_gate(mm, addr)	pgd_offset_k(addr)
static inline int
ptep_test_and_clear_young (struct vm_area_struct *vma, unsigned long addr, pte_t *ptep)
{
#ifdef CONFIG_SMP
	if (!pte_young(*ptep))
		return 0;
	return test_and_clear_bit(_PAGE_A_BIT, ptep);
#else
	pte_t pte = *ptep;
	if (!pte_young(pte))
		return 0;
	set_pte_at(vma->vm_mm, addr, ptep, pte_mkold(pte));
	return 1;
#endif
}
static inline pte_t
ptep_get_and_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
#ifdef CONFIG_SMP
	return __pte(xchg((long *) ptep, 0));
#else
	pte_t pte = *ptep;
	pte_clear(mm, addr, ptep);
	return pte;
#endif
}
static inline void
ptep_set_wrprotect(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
#ifdef CONFIG_SMP
	unsigned long new, old;
	do {
		old = pte_val(*ptep);
		new = pte_val(pte_wrprotect(__pte (old)));
	} while (cmpxchg((unsigned long *) ptep, old, new) != old);
#else
	pte_t old_pte = *ptep;
	set_pte_at(mm, addr, ptep, pte_wrprotect(old_pte));
#endif
}
static inline int
pte_same (pte_t a, pte_t b)
{
	return pte_val(a) == pte_val(b);
}
#define update_mmu_cache_range(vmf, vma, address, ptep, nr) do { } while (0)
#define update_mmu_cache(vma, address, ptep) do { } while (0)
extern pgd_t swapper_pg_dir[PTRS_PER_PGD];
extern void paging_init (void);
#define __swp_type(entry)		(((entry).val >> 1) & 0x3f)
#define __swp_offset(entry)		(((entry).val << 1) >> 9)
#define __swp_entry(type, offset)	((swp_entry_t) { ((type & 0x3f) << 1) | \
							 ((long) (offset) << 8) })
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
extern unsigned long empty_zero_page[PAGE_SIZE/sizeof(unsigned long)];
extern struct page *zero_page_memmap_ptr;
#define ZERO_PAGE(vaddr) (zero_page_memmap_ptr)
#define HAVE_ARCH_UNMAPPED_AREA
#ifdef CONFIG_HUGETLB_PAGE
#define HUGETLB_PGDIR_SHIFT	(HPAGE_SHIFT + 2*(PAGE_SHIFT-3))
#define HUGETLB_PGDIR_SIZE	(__IA64_UL(1) << HUGETLB_PGDIR_SHIFT)
#define HUGETLB_PGDIR_MASK	(~(HUGETLB_PGDIR_SIZE-1))
#endif
#define __HAVE_ARCH_PTEP_SET_ACCESS_FLAGS
#ifdef CONFIG_SMP
# define ptep_set_access_flags(__vma, __addr, __ptep, __entry, __safely_writable) \
({									\
	int __changed = !pte_same(*(__ptep), __entry);			\
	if (__changed && __safely_writable) {				\
		set_pte(__ptep, __entry);				\
		flush_tlb_page(__vma, __addr);				\
	}								\
	__changed;							\
})
#else
# define ptep_set_access_flags(__vma, __addr, __ptep, __entry, __safely_writable) \
({									\
	int __changed = !pte_same(*(__ptep), __entry);			\
	if (__changed) {						\
		set_pte_at((__vma)->vm_mm, (__addr), __ptep, __entry);	\
		flush_tlb_page(__vma, __addr);				\
	}								\
	__changed;							\
})
#endif
# endif  
#if defined(CONFIG_IA64_GRANULE_64MB)
# define IA64_GRANULE_SHIFT	_PAGE_SIZE_64M
#elif defined(CONFIG_IA64_GRANULE_16MB)
# define IA64_GRANULE_SHIFT	_PAGE_SIZE_16M
#endif
#define IA64_GRANULE_SIZE	(1 << IA64_GRANULE_SHIFT)
#define KERNEL_TR_PAGE_SHIFT	_PAGE_SIZE_64M
#define KERNEL_TR_PAGE_SIZE	(1 << KERNEL_TR_PAGE_SHIFT)
#define FIXADDR_USER_START	GATE_ADDR
#ifdef HAVE_BUGGY_SEGREL
# define FIXADDR_USER_END	(GATE_ADDR + 2*PAGE_SIZE)
#else
# define FIXADDR_USER_END	(GATE_ADDR + 2*PERCPU_PAGE_SIZE)
#endif
#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
#define __HAVE_ARCH_PTEP_SET_WRPROTECT
#define __HAVE_ARCH_PTE_SAME
#define __HAVE_ARCH_PGD_OFFSET_GATE
#if CONFIG_PGTABLE_LEVELS == 3
#include <asm-generic/pgtable-nopud.h>
#endif
#include <asm-generic/pgtable-nop4d.h>
#endif  
