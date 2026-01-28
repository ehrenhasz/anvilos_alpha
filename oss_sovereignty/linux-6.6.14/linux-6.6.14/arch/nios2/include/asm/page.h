#ifndef _ASM_NIOS2_PAGE_H
#define _ASM_NIOS2_PAGE_H
#include <linux/pfn.h>
#include <linux/const.h>
#define PAGE_SHIFT	12
#define PAGE_SIZE	(_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE - 1))
#define PAGE_OFFSET	\
	(CONFIG_NIOS2_MEM_BASE + CONFIG_NIOS2_KERNEL_REGION_BASE)
#ifndef __ASSEMBLY__
#define PHYS_OFFSET		CONFIG_NIOS2_MEM_BASE
#define ARCH_PFN_OFFSET		PFN_UP(PHYS_OFFSET)
#define clear_page(page)	memset((page), 0, PAGE_SIZE)
#define copy_page(to, from)	memcpy((to), (from), PAGE_SIZE)
struct page;
extern void clear_user_page(void *addr, unsigned long vaddr, struct page *page);
extern void copy_user_page(void *vto, void *vfrom, unsigned long vaddr,
				struct page *to);
typedef struct page *pgtable_t;
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pgd; } pgd_t;
typedef struct { unsigned long pgprot; } pgprot_t;
#define pte_val(x)	((x).pte)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)
#define __pte(x)	((pte_t) { (x) })
#define __pgd(x)	((pgd_t) { (x) })
#define __pgprot(x)	((pgprot_t) { (x) })
extern unsigned long memory_start;
extern unsigned long memory_end;
extern unsigned long memory_size;
extern struct page *mem_map;
# define __pa(x)		\
	((unsigned long)(x) - PAGE_OFFSET + PHYS_OFFSET)
# define __va(x)		\
	((void *)((unsigned long)(x) + PAGE_OFFSET - PHYS_OFFSET))
#define page_to_virt(page)	\
	((void *)(((page) - mem_map) << PAGE_SHIFT) + PAGE_OFFSET)
# define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)
# define virt_to_page(vaddr)	pfn_to_page(PFN_DOWN(virt_to_phys(vaddr)))
# define virt_addr_valid(vaddr)	pfn_valid(PFN_DOWN(virt_to_phys(vaddr)))
# define VM_DATA_DEFAULT_FLAGS	VM_DATA_FLAGS_NON_EXEC
#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>
#endif  
#endif  
