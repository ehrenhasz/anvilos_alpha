#ifndef _ASM_MICROBLAZE_PAGE_H
#define _ASM_MICROBLAZE_PAGE_H
#include <linux/pfn.h>
#include <asm/setup.h>
#include <asm/asm-compat.h>
#include <linux/const.h>
#ifdef __KERNEL__
#define PAGE_SHIFT		12
#define PAGE_SIZE	(ASM_CONST(1) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))
#define LOAD_OFFSET	ASM_CONST((CONFIG_KERNEL_START-CONFIG_KERNEL_BASE_ADDR))
#define PTE_SHIFT	(PAGE_SHIFT - 2)	 
#ifndef __ASSEMBLY__
#define PAGE_OFFSET	CONFIG_KERNEL_START
typedef unsigned long pte_basic_t;
#define PTE_FMT		"%.8lx"
# define copy_page(to, from)			memcpy((to), (from), PAGE_SIZE)
# define clear_page(pgaddr)			memset((pgaddr), 0, PAGE_SIZE)
# define clear_user_page(pgaddr, vaddr, page)	memset((pgaddr), 0, PAGE_SIZE)
# define copy_user_page(vto, vfrom, vaddr, topg) \
			memcpy((vto), (vfrom), PAGE_SIZE)
typedef struct page *pgtable_t;
typedef struct { unsigned long	pte; }		pte_t;
typedef struct { unsigned long	pgprot; }	pgprot_t;
typedef struct { unsigned long pgd; } pgd_t;
# define pte_val(x)	((x).pte)
# define pgprot_val(x)	((x).pgprot)
#   define pgd_val(x)      ((x).pgd)
# define __pte(x)	((pte_t) { (x) })
# define __pgd(x)	((pgd_t) { (x) })
# define __pgprot(x)	((pgprot_t) { (x) })
extern unsigned long max_low_pfn;
extern unsigned long min_low_pfn;
extern unsigned long max_pfn;
extern unsigned long memory_start;
extern unsigned long memory_size;
extern unsigned long lowmem_size;
extern unsigned long kernel_tlb;
extern int page_is_ram(unsigned long pfn);
# define phys_to_pfn(phys)	(PFN_DOWN(phys))
# define pfn_to_phys(pfn)	(PFN_PHYS(pfn))
#  define virt_to_page(kaddr)	(pfn_to_page(__pa(kaddr) >> PAGE_SHIFT))
#  define page_to_virt(page)   __va(page_to_pfn(page) << PAGE_SHIFT)
#  define page_to_phys(page)     (page_to_pfn(page) << PAGE_SHIFT)
#  define ARCH_PFN_OFFSET	(memory_start >> PAGE_SHIFT)
# endif  
#define __virt_to_phys(addr) \
	((addr) + CONFIG_KERNEL_BASE_ADDR - CONFIG_KERNEL_START)
#define __phys_to_virt(addr) \
	((addr) + CONFIG_KERNEL_START - CONFIG_KERNEL_BASE_ADDR)
#define tophys(rd, rs) \
	addik rd, rs, (CONFIG_KERNEL_BASE_ADDR - CONFIG_KERNEL_START)
#define tovirt(rd, rs) \
	addik rd, rs, (CONFIG_KERNEL_START - CONFIG_KERNEL_BASE_ADDR)
#ifndef __ASSEMBLY__
# define __pa(x)	__virt_to_phys((unsigned long)(x))
# define __va(x)	((void *)__phys_to_virt((unsigned long)(x)))
static inline unsigned long virt_to_pfn(const void *vaddr)
{
	return phys_to_pfn(__pa(vaddr));
}
static inline const void *pfn_to_virt(unsigned long pfn)
{
	return __va(pfn_to_phys((pfn)));
}
#define	virt_addr_valid(vaddr)	(pfn_valid(virt_to_pfn(vaddr)))
#endif  
#define TOPHYS(addr)  __virt_to_phys(addr)
#endif  
#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>
#endif  
