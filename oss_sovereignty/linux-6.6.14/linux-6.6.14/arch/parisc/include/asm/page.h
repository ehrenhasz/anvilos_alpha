#ifndef _PARISC_PAGE_H
#define _PARISC_PAGE_H
#include <linux/const.h>
#if defined(CONFIG_PARISC_PAGE_SIZE_4KB)
# define PAGE_SHIFT	12
#elif defined(CONFIG_PARISC_PAGE_SIZE_16KB)
# define PAGE_SHIFT	14
#elif defined(CONFIG_PARISC_PAGE_SIZE_64KB)
# define PAGE_SHIFT	16
#else
# error "unknown default kernel page size"
#endif
#define PAGE_SIZE	(_AC(1,UL) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))
#ifndef __ASSEMBLY__
#include <asm/types.h>
#include <asm/cache.h>
#define clear_page(page)	clear_page_asm((void *)(page))
#define copy_page(to, from)	copy_page_asm((void *)(to), (void *)(from))
struct page;
struct vm_area_struct;
void clear_page_asm(void *page);
void copy_page_asm(void *to, void *from);
#define clear_user_page(vto, vaddr, page) clear_page_asm(vto)
void copy_user_highpage(struct page *to, struct page *from, unsigned long vaddr,
		struct vm_area_struct *vma);
#define __HAVE_ARCH_COPY_USER_HIGHPAGE
#define STRICT_MM_TYPECHECKS
#ifdef STRICT_MM_TYPECHECKS
typedef struct { unsigned long pte; } pte_t;  
typedef struct { __u32 pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;
#if CONFIG_PGTABLE_LEVELS == 3
typedef struct { __u32 pmd; } pmd_t;
#define __pmd(x)	((pmd_t) { (x) } )
#define pmd_val(x)	((x).pmd + 0)
#endif
#define pte_val(x)	((x).pte)
#define pgd_val(x)	((x).pgd + 0)
#define pgprot_val(x)	((x).pgprot)
#define __pte(x)	((pte_t) { (x) } )
#define __pgd(x)	((pgd_t) { (x) } )
#define __pgprot(x)	((pgprot_t) { (x) } )
#else
typedef unsigned long pte_t;
#if CONFIG_PGTABLE_LEVELS == 3
typedef         __u32 pmd_t;
#define pmd_val(x)      (x)
#define __pmd(x)	(x)
#endif
typedef         __u32 pgd_t;
typedef unsigned long pgprot_t;
#define pte_val(x)      (x)
#define pgd_val(x)      (x)
#define pgprot_val(x)   (x)
#define __pte(x)        (x)
#define __pgd(x)        (x)
#define __pgprot(x)     (x)
#endif  
#define set_pmd(pmdptr, pmdval) (*(pmdptr) = (pmdval))
#if CONFIG_PGTABLE_LEVELS == 3
#define set_pud(pudptr, pudval) (*(pudptr) = (pudval))
#endif
typedef struct page *pgtable_t;
typedef struct __physmem_range {
	unsigned long start_pfn;
	unsigned long pages;        
} physmem_range_t;
extern physmem_range_t pmem_ranges[];
extern int npmem_ranges;
#endif  
#ifdef CONFIG_64BIT
#define BITS_PER_PTE_ENTRY	3
#define BITS_PER_PMD_ENTRY	2
#define BITS_PER_PGD_ENTRY	2
#else
#define BITS_PER_PTE_ENTRY	2
#define BITS_PER_PMD_ENTRY	2
#define BITS_PER_PGD_ENTRY	2
#endif
#define PGD_ENTRY_SIZE	(1UL << BITS_PER_PGD_ENTRY)
#define PMD_ENTRY_SIZE	(1UL << BITS_PER_PMD_ENTRY)
#define PTE_ENTRY_SIZE	(1UL << BITS_PER_PTE_ENTRY)
#define LINUX_GATEWAY_SPACE     0
#ifdef CONFIG_64BIT
#define __PAGE_OFFSET_DEFAULT	(0x40000000)	 
#else
#define __PAGE_OFFSET_DEFAULT	(0x10000000)	 
#endif
#if defined(BOOTLOADER)
#define __PAGE_OFFSET	(0)	 
#else
#define __PAGE_OFFSET	__PAGE_OFFSET_DEFAULT
#endif  
#define PAGE_OFFSET		((unsigned long)__PAGE_OFFSET)
#define GATEWAY_PAGE_SIZE	0x4000
#define KERNEL_BINARY_TEXT_START	(__PAGE_OFFSET + 0x100000)
#ifdef __ASSEMBLY__
#   define PA(x)	((x)-__PAGE_OFFSET)
#   define VA(x)	((x)+__PAGE_OFFSET)
#endif
#define __pa(x)			((unsigned long)(x)-PAGE_OFFSET)
#define __va(x)			((void *)((unsigned long)(x)+PAGE_OFFSET))
#ifdef CONFIG_HUGETLB_PAGE
#define HPAGE_SHIFT		PMD_SHIFT  
#define HPAGE_SIZE      	((1UL) << HPAGE_SHIFT)
#define HPAGE_MASK		(~(HPAGE_SIZE - 1))
#define HUGETLB_PAGE_ORDER	(HPAGE_SHIFT - PAGE_SHIFT)
#if defined(CONFIG_64BIT) && defined(CONFIG_PARISC_PAGE_SIZE_4KB)
# define REAL_HPAGE_SHIFT	20  
# define _HUGE_PAGE_SIZE_ENCODING_DEFAULT _PAGE_SIZE_ENCODING_1M
#elif !defined(CONFIG_64BIT) && defined(CONFIG_PARISC_PAGE_SIZE_4KB)
# define REAL_HPAGE_SHIFT	22  
# define _HUGE_PAGE_SIZE_ENCODING_DEFAULT _PAGE_SIZE_ENCODING_4M
#else
# define REAL_HPAGE_SHIFT	24  
# define _HUGE_PAGE_SIZE_ENCODING_DEFAULT _PAGE_SIZE_ENCODING_16M
#endif
#endif  
#define virt_addr_valid(kaddr)	pfn_valid(__pa(kaddr) >> PAGE_SHIFT)
#define page_to_phys(page)	(page_to_pfn(page) << PAGE_SHIFT)
#define virt_to_page(kaddr)     pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>
#include <asm/pdc.h>
#define PAGE0   ((struct zeropage *)absolute_pointer(__PAGE_OFFSET))
#endif  
