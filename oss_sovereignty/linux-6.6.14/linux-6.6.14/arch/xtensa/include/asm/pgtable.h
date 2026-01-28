#ifndef _XTENSA_PGTABLE_H
#define _XTENSA_PGTABLE_H
#include <asm/page.h>
#include <asm/kmem_layout.h>
#include <asm-generic/pgtable-nopmd.h>
#ifdef CONFIG_MMU
#define USER_RING		1	 
#else
#define USER_RING		0
#endif
#define KERNEL_RING		0	 
#define PGDIR_SHIFT	22
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))
#define PTRS_PER_PTE		1024
#define PTRS_PER_PTE_SHIFT	10
#define PTRS_PER_PGD		1024
#define USER_PTRS_PER_PGD	(TASK_SIZE/PGDIR_SIZE)
#define FIRST_USER_PGD_NR	(FIRST_USER_ADDRESS >> PGDIR_SHIFT)
#ifdef CONFIG_MMU
#define VMALLOC_START		(XCHAL_KSEG_CACHED_VADDR - 0x10000000)
#define VMALLOC_END		(VMALLOC_START + 0x07FEFFFF)
#define TLBTEMP_BASE_1		(VMALLOC_START + 0x08000000)
#define TLBTEMP_BASE_2		(TLBTEMP_BASE_1 + DCACHE_WAY_SIZE)
#if 2 * DCACHE_WAY_SIZE > ICACHE_WAY_SIZE
#define TLBTEMP_SIZE		(2 * DCACHE_WAY_SIZE)
#else
#define TLBTEMP_SIZE		ICACHE_WAY_SIZE
#endif
#else
#define VMALLOC_START		__XTENSA_UL_CONST(0)
#define VMALLOC_END		__XTENSA_UL_CONST(0xffffffff)
#endif
#define _PAGE_ATTRIB_MASK	0xf
#define _PAGE_HW_EXEC		(1<<0)	 
#define _PAGE_HW_WRITE		(1<<1)	 
#define _PAGE_CA_BYPASS		(0<<2)	 
#define _PAGE_CA_WB		(1<<2)	 
#define _PAGE_CA_WT		(2<<2)	 
#define _PAGE_CA_MASK		(3<<2)
#define _PAGE_CA_INVALID	(3<<2)
#if XCHAL_HW_VERSION_MAJOR < 2000
#define _PAGE_HW_VALID		0x01	 
#define _PAGE_NONE		0x04
#else
#define _PAGE_HW_VALID		0x00
#define _PAGE_NONE		0x0f
#endif
#define _PAGE_USER		(1<<4)	 
#define _PAGE_WRITABLE_BIT	6
#define _PAGE_WRITABLE		(1<<6)	 
#define _PAGE_DIRTY		(1<<7)	 
#define _PAGE_ACCESSED		(1<<8)	 
#define _PAGE_SWP_EXCLUSIVE	(1<<1)
#ifdef CONFIG_MMU
#define _PAGE_CHG_MASK	   (PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)
#define _PAGE_PRESENT	   (_PAGE_HW_VALID | _PAGE_CA_WB | _PAGE_ACCESSED)
#define PAGE_NONE	   __pgprot(_PAGE_NONE | _PAGE_USER)
#define PAGE_COPY	   __pgprot(_PAGE_PRESENT | _PAGE_USER)
#define PAGE_COPY_EXEC	   __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_HW_EXEC)
#define PAGE_READONLY	   __pgprot(_PAGE_PRESENT | _PAGE_USER)
#define PAGE_READONLY_EXEC __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_HW_EXEC)
#define PAGE_SHARED	   __pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_WRITABLE)
#define PAGE_SHARED_EXEC \
	__pgprot(_PAGE_PRESENT | _PAGE_USER | _PAGE_WRITABLE | _PAGE_HW_EXEC)
#define PAGE_KERNEL	   __pgprot(_PAGE_PRESENT | _PAGE_HW_WRITE)
#define PAGE_KERNEL_RO	   __pgprot(_PAGE_PRESENT)
#define PAGE_KERNEL_EXEC   __pgprot(_PAGE_PRESENT|_PAGE_HW_WRITE|_PAGE_HW_EXEC)
#if (DCACHE_WAY_SIZE > PAGE_SIZE)
# define _PAGE_DIRECTORY   (_PAGE_HW_VALID | _PAGE_ACCESSED | _PAGE_CA_BYPASS)
#else
# define _PAGE_DIRECTORY   (_PAGE_HW_VALID | _PAGE_ACCESSED | _PAGE_CA_WB)
#endif
#else  
# define _PAGE_CHG_MASK  (PAGE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)
# define PAGE_NONE       __pgprot(0)
# define PAGE_SHARED     __pgprot(0)
# define PAGE_COPY       __pgprot(0)
# define PAGE_READONLY   __pgprot(0)
# define PAGE_KERNEL     __pgprot(0)
#endif
#ifndef __ASSEMBLY__
#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, pte_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd entry %08lx.\n", __FILE__, __LINE__, pgd_val(e))
extern unsigned long empty_zero_page[1024];
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))
#ifdef CONFIG_MMU
extern pgd_t swapper_pg_dir[PAGE_SIZE/sizeof(pgd_t)];
extern void paging_init(void);
#else
# define swapper_pg_dir NULL
static inline void paging_init(void) { }
#endif
#define pmd_page_vaddr(pmd) ((unsigned long)(pmd_val(pmd) & PAGE_MASK))
#define pmd_pfn(pmd) (__pa(pmd_val(pmd)) >> PAGE_SHIFT)
#define pmd_page(pmd) virt_to_page(pmd_val(pmd))
# define pte_none(pte)	 (pte_val(pte) == (_PAGE_CA_INVALID | _PAGE_USER))
#if XCHAL_HW_VERSION_MAJOR < 2000
# define pte_present(pte) ((pte_val(pte) & _PAGE_CA_MASK) != _PAGE_CA_INVALID)
#else
# define pte_present(pte)						\
	(((pte_val(pte) & _PAGE_CA_MASK) != _PAGE_CA_INVALID)		\
	 || ((pte_val(pte) & _PAGE_ATTRIB_MASK) == _PAGE_NONE))
#endif
#define pte_clear(mm,addr,ptep)						\
	do { update_pte(ptep, __pte(_PAGE_CA_INVALID | _PAGE_USER)); } while (0)
#define pmd_none(pmd)	 (!pmd_val(pmd))
#define pmd_present(pmd) (pmd_val(pmd) & PAGE_MASK)
#define pmd_bad(pmd)	 (pmd_val(pmd) & ~PAGE_MASK)
#define pmd_clear(pmdp)	 do { set_pmd(pmdp, __pmd(0)); } while (0)
static inline int pte_write(pte_t pte) { return pte_val(pte) & _PAGE_WRITABLE; }
static inline int pte_dirty(pte_t pte) { return pte_val(pte) & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte) { return pte_val(pte) & _PAGE_ACCESSED; }
static inline pte_t pte_wrprotect(pte_t pte)
	{ pte_val(pte) &= ~(_PAGE_WRITABLE | _PAGE_HW_WRITE); return pte; }
static inline pte_t pte_mkclean(pte_t pte)
	{ pte_val(pte) &= ~(_PAGE_DIRTY | _PAGE_HW_WRITE); return pte; }
static inline pte_t pte_mkold(pte_t pte)
	{ pte_val(pte) &= ~_PAGE_ACCESSED; return pte; }
static inline pte_t pte_mkdirty(pte_t pte)
	{ pte_val(pte) |= _PAGE_DIRTY; return pte; }
static inline pte_t pte_mkyoung(pte_t pte)
	{ pte_val(pte) |= _PAGE_ACCESSED; return pte; }
static inline pte_t pte_mkwrite_novma(pte_t pte)
	{ pte_val(pte) |= _PAGE_WRITABLE; return pte; }
#define pgprot_noncached(prot) \
		((__pgprot((pgprot_val(prot) & ~_PAGE_CA_MASK) | \
			   _PAGE_CA_BYPASS)))
#define PFN_PTE_SHIFT		PAGE_SHIFT
#define pte_pfn(pte)		(pte_val(pte) >> PAGE_SHIFT)
#define pte_same(a,b)		(pte_val(a) == pte_val(b))
#define pte_page(x)		pfn_to_page(pte_pfn(x))
#define pfn_pte(pfn, prot)	__pte(((pfn) << PAGE_SHIFT) | pgprot_val(prot))
#define mk_pte(page, prot)	pfn_pte(page_to_pfn(page), prot)
static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot));
}
static inline void update_pte(pte_t *ptep, pte_t pteval)
{
	*ptep = pteval;
#if (DCACHE_WAY_SIZE > PAGE_SIZE) && XCHAL_DCACHE_IS_WRITEBACK
	__asm__ __volatile__ ("dhwb %0, 0" :: "a" (ptep));
#endif
}
struct mm_struct;
static inline void set_pte(pte_t *ptep, pte_t pte)
{
	update_pte(ptep, pte);
}
static inline void
set_pmd(pmd_t *pmdp, pmd_t pmdval)
{
	*pmdp = pmdval;
}
struct vm_area_struct;
static inline int
ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr,
			  pte_t *ptep)
{
	pte_t pte = *ptep;
	if (!pte_young(pte))
		return 0;
	update_pte(ptep, pte_mkold(pte));
	return 1;
}
static inline pte_t
ptep_get_and_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	pte_t pte = *ptep;
	pte_clear(mm, addr, ptep);
	return pte;
}
static inline void
ptep_set_wrprotect(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	pte_t pte = *ptep;
	update_pte(ptep, pte_wrprotect(pte));
}
#define MAX_SWAPFILES_CHECK() BUILD_BUG_ON(MAX_SWAPFILES_SHIFT > 5)
#define __swp_type(entry)	(((entry).val >> 6) & 0x1f)
#define __swp_offset(entry)	((entry).val >> 11)
#define __swp_entry(type,offs)	\
	((swp_entry_t){(((type) & 0x1f) << 6) | ((offs) << 11) | \
	 _PAGE_CA_INVALID | _PAGE_USER})
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
#endif  
#ifdef __ASSEMBLY__
#define _PGD_INDEX(rt,rs)	extui	rt, rs, PGDIR_SHIFT, 32-PGDIR_SHIFT
#define _PTE_INDEX(rt,rs)	extui	rt, rs, PAGE_SHIFT, PTRS_PER_PTE_SHIFT
#define _PGD_OFFSET(mm,adr,tmp)		l32i	mm, mm, MM_PGD;		\
					_PGD_INDEX(tmp, adr);		\
					addx4	mm, tmp, mm
#define _PTE_OFFSET(pmd,adr,tmp)	_PTE_INDEX(tmp, adr);		\
					srli	pmd, pmd, PAGE_SHIFT;	\
					slli	pmd, pmd, PAGE_SHIFT;	\
					addx4	pmd, tmp, pmd
#else
struct vm_fault;
void update_mmu_cache_range(struct vm_fault *vmf, struct vm_area_struct *vma,
		unsigned long address, pte_t *ptep, unsigned int nr);
#define update_mmu_cache(vma, address, ptep) \
	update_mmu_cache_range(NULL, vma, address, ptep, 1)
typedef pte_t *pte_addr_t;
void update_mmu_tlb(struct vm_area_struct *vma,
		    unsigned long address, pte_t *ptep);
#define __HAVE_ARCH_UPDATE_MMU_TLB
#endif  
#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG
#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
#define __HAVE_ARCH_PTEP_SET_WRPROTECT
#define __HAVE_ARCH_PTEP_MKDIRTY
#define __HAVE_ARCH_PTE_SAME
#define HAVE_ARCH_UNMAPPED_AREA
#endif  
