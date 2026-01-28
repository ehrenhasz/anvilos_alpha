#ifndef _XTENSA_PAGE_H
#define _XTENSA_PAGE_H
#include <linux/const.h>
#include <asm/processor.h>
#include <asm/types.h>
#include <asm/cache.h>
#include <asm/kmem_layout.h>
#define PAGE_SHIFT	12
#define PAGE_SIZE	(__XTENSA_UL_CONST(1) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))
#ifdef CONFIG_MMU
#define PAGE_OFFSET	XCHAL_KSEG_CACHED_VADDR
#define PHYS_OFFSET	XCHAL_KSEG_PADDR
#define MAX_LOW_PFN	(PHYS_PFN(XCHAL_KSEG_PADDR) + \
			 PHYS_PFN(XCHAL_KSEG_SIZE))
#else
#define PAGE_OFFSET	_AC(CONFIG_DEFAULT_MEM_START, UL)
#define PHYS_OFFSET	_AC(CONFIG_DEFAULT_MEM_START, UL)
#define MAX_LOW_PFN	PHYS_PFN(0xfffffffful)
#endif
#if DCACHE_WAY_SIZE > PAGE_SIZE
# define DCACHE_ALIAS_ORDER	(DCACHE_WAY_SHIFT - PAGE_SHIFT)
# define DCACHE_ALIAS_MASK	(PAGE_MASK & (DCACHE_WAY_SIZE - 1))
# define DCACHE_ALIAS(a)	(((a) & DCACHE_ALIAS_MASK) >> PAGE_SHIFT)
# define DCACHE_ALIAS_EQ(a,b)	((((a) ^ (b)) & DCACHE_ALIAS_MASK) == 0)
#else
# define DCACHE_ALIAS_ORDER	0
# define DCACHE_ALIAS(a)	((void)(a), 0)
#endif
#define DCACHE_N_COLORS		(1 << DCACHE_ALIAS_ORDER)
#if ICACHE_WAY_SIZE > PAGE_SIZE
# define ICACHE_ALIAS_ORDER	(ICACHE_WAY_SHIFT - PAGE_SHIFT)
# define ICACHE_ALIAS_MASK	(PAGE_MASK & (ICACHE_WAY_SIZE - 1))
# define ICACHE_ALIAS(a)	(((a) & ICACHE_ALIAS_MASK) >> PAGE_SHIFT)
# define ICACHE_ALIAS_EQ(a,b)	((((a) ^ (b)) & ICACHE_ALIAS_MASK) == 0)
#else
# define ICACHE_ALIAS_ORDER	0
#endif
#ifdef __ASSEMBLY__
#define __pgprot(x)	(x)
#else
typedef struct { unsigned long pte; } pte_t;		 
typedef struct { unsigned long pgd; } pgd_t;		 
typedef struct { unsigned long pgprot; } pgprot_t;
typedef struct page *pgtable_t;
#define pte_val(x)	((x).pte)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)
#define __pte(x)	((pte_t) { (x) } )
#define __pgd(x)	((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )
#if XCHAL_HAVE_NSA
static inline __attribute_const__ int get_order(unsigned long size)
{
	int lz;
	asm ("nsau %0, %1" : "=r" (lz) : "r" ((size - 1) >> PAGE_SHIFT));
	return 32 - lz;
}
#else
# include <asm-generic/getorder.h>
#endif
struct page;
struct vm_area_struct;
extern void clear_page(void *page);
extern void copy_page(void *to, void *from);
#if defined(CONFIG_MMU) && DCACHE_WAY_SIZE > PAGE_SIZE
extern void clear_page_alias(void *vaddr, unsigned long paddr);
extern void copy_page_alias(void *to, void *from,
			    unsigned long to_paddr, unsigned long from_paddr);
#define clear_user_highpage clear_user_highpage
void clear_user_highpage(struct page *page, unsigned long vaddr);
#define __HAVE_ARCH_COPY_USER_HIGHPAGE
void copy_user_highpage(struct page *to, struct page *from,
			unsigned long vaddr, struct vm_area_struct *vma);
#else
# define clear_user_page(page, vaddr, pg)	clear_page(page)
# define copy_user_page(to, from, vaddr, pg)	copy_page(to, from)
#endif
#define ARCH_PFN_OFFSET		(PHYS_OFFSET >> PAGE_SHIFT)
#ifdef CONFIG_MMU
static inline unsigned long ___pa(unsigned long va)
{
	unsigned long off = va - PAGE_OFFSET;
	if (off >= XCHAL_KSEG_SIZE)
		off -= XCHAL_KSEG_SIZE;
#ifndef CONFIG_XIP_KERNEL
	return off + PHYS_OFFSET;
#else
	if (off < XCHAL_KSEG_SIZE)
		return off + PHYS_OFFSET;
	off -= XCHAL_KSEG_SIZE;
	if (off >= XCHAL_KIO_SIZE)
		off -= XCHAL_KIO_SIZE;
	return off + XCHAL_KIO_PADDR;
#endif
}
#define __pa(x)	___pa((unsigned long)(x))
#else
#define __pa(x)	\
	((unsigned long) (x) - PAGE_OFFSET + PHYS_OFFSET)
#endif
#define __va(x)	\
	((void *)((unsigned long) (x) - PHYS_OFFSET + PAGE_OFFSET))
#define virt_to_page(kaddr)	pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define page_to_virt(page)	__va(page_to_pfn(page) << PAGE_SHIFT)
#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)
#define page_to_phys(page)	(page_to_pfn(page) << PAGE_SHIFT)
#endif  
#include <asm-generic/memory_model.h>
#endif  
