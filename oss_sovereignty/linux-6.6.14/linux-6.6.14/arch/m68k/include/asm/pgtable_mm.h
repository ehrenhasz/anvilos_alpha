#ifndef _M68K_PGTABLE_H
#define _M68K_PGTABLE_H
#if defined(CONFIG_SUN3) || defined(CONFIG_COLDFIRE)
#include <asm-generic/pgtable-nopmd.h>
#else
#include <asm-generic/pgtable-nopud.h>
#endif
#include <asm/setup.h>
#ifndef __ASSEMBLY__
#include <asm/processor.h>
#include <linux/sched.h>
#include <linux/threads.h>
#include <asm/virtconvert.h>
#define set_pte(pteptr, pteval)					\
	do{							\
		*(pteptr) = (pteval);				\
	} while(0)
#if CONFIG_PGTABLE_LEVELS == 3
#define PMD_SHIFT	18
#endif
#define PMD_SIZE	(1UL << PMD_SHIFT)
#define PMD_MASK	(~(PMD_SIZE-1))
#ifdef CONFIG_SUN3
#define PGDIR_SHIFT     17
#elif defined(CONFIG_COLDFIRE)
#define PGDIR_SHIFT     22
#else
#define PGDIR_SHIFT	25
#endif
#define PGDIR_SIZE	(1UL << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE-1))
#ifdef CONFIG_SUN3
#define PTRS_PER_PTE   16
#define __PAGETABLE_PMD_FOLDED 1
#define PTRS_PER_PMD   1
#define PTRS_PER_PGD   2048
#elif defined(CONFIG_COLDFIRE)
#define PTRS_PER_PTE	512
#define __PAGETABLE_PMD_FOLDED 1
#define PTRS_PER_PMD	1
#define PTRS_PER_PGD	1024
#else
#define PTRS_PER_PTE	64
#define PTRS_PER_PMD	128
#define PTRS_PER_PGD	128
#endif
#define USER_PTRS_PER_PGD	(TASK_SIZE/PGDIR_SIZE)
#ifdef CONFIG_SUN3
#define KMAP_START	0x0dc00000
#define KMAP_END	0x0e000000
#elif defined(CONFIG_COLDFIRE)
#define KMAP_START	0xe0000000
#define KMAP_END	0xf0000000
#elif defined(CONFIG_VIRT)
#define	KMAP_START	0xdf000000
#define	KMAP_END	0xff000000
#else
#define	KMAP_START	0xd0000000
#define	KMAP_END	0xf0000000
#endif
#ifdef CONFIG_SUN3
extern unsigned long m68k_vmalloc_end;
#define VMALLOC_START 0x0f800000
#define VMALLOC_END m68k_vmalloc_end
#elif defined(CONFIG_COLDFIRE)
#define VMALLOC_START	0xd0000000
#define VMALLOC_END	0xe0000000
#elif defined(CONFIG_VIRT)
#define VMALLOC_OFFSET	PAGE_SIZE
#define VMALLOC_START (((unsigned long) high_memory + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1))
#define VMALLOC_END     KMAP_START
#else
#define VMALLOC_OFFSET	(8*1024*1024)
#define VMALLOC_START (((unsigned long) high_memory + VMALLOC_OFFSET) & ~(VMALLOC_OFFSET-1))
#define VMALLOC_END KMAP_START
#endif
extern void *empty_zero_page;
#define ZERO_PAGE(vaddr)	(virt_to_page(empty_zero_page))
#define BITS_PER_PTR			(8*sizeof(unsigned long))
#define PTR_MASK			(~(sizeof(void*)-1))
#define SIZEOF_PTR_LOG2			       2
extern void kernel_set_cachemode(void *addr, unsigned long size, int cmode);
static inline void update_mmu_cache_range(struct vm_fault *vmf,
		struct vm_area_struct *vma, unsigned long address,
		pte_t *ptep, unsigned int nr)
{
}
#define update_mmu_cache(vma, addr, ptep) \
	update_mmu_cache_range(NULL, vma, addr, ptep, 1)
#endif  
#ifdef CONFIG_SUN3
#include <asm/sun3_pgtable.h>
#elif defined(CONFIG_COLDFIRE)
#include <asm/mcf_pgtable.h>
#else
#include <asm/motorola_pgtable.h>
#endif
#ifndef __ASSEMBLY__
#ifdef CONFIG_COLDFIRE
# define pgprot_noncached(prot) (__pgprot(pgprot_val(prot) | CF_PAGE_NOCACHE))
#else
#ifdef SUN3_PAGE_NOCACHE
# define __SUN3_PAGE_NOCACHE	SUN3_PAGE_NOCACHE
#else
# define __SUN3_PAGE_NOCACHE	0
#endif
#define pgprot_noncached(prot)							\
	(MMU_IS_SUN3								\
	 ? (__pgprot(pgprot_val(prot) | __SUN3_PAGE_NOCACHE))			\
	 : ((MMU_IS_851 || MMU_IS_030)						\
	    ? (__pgprot(pgprot_val(prot) | _PAGE_NOCACHE030))			\
	    : (MMU_IS_040 || MMU_IS_060)					\
	    ? (__pgprot((pgprot_val(prot) & _CACHEMASK040) | _PAGE_NOCACHE_S))	\
	    : (prot)))
pgprot_t pgprot_dmacoherent(pgprot_t prot);
#define pgprot_dmacoherent(prot)	pgprot_dmacoherent(prot)
#endif  
#endif  
#endif  
