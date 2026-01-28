#ifndef _ASM_POWERPC_PAGE_H
#define _ASM_POWERPC_PAGE_H
#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#else
#include <asm/types.h>
#endif
#include <asm/asm-const.h>
#define PAGE_SHIFT		CONFIG_PPC_PAGE_SHIFT
#define PAGE_SIZE		(ASM_CONST(1) << PAGE_SHIFT)
#ifndef __ASSEMBLY__
#ifndef CONFIG_HUGETLB_PAGE
#define HPAGE_SHIFT PAGE_SHIFT
#elif defined(CONFIG_PPC_BOOK3S_64)
extern unsigned int hpage_shift;
#define HPAGE_SHIFT hpage_shift
#elif defined(CONFIG_PPC_8xx)
#define HPAGE_SHIFT		19	 
#elif defined(CONFIG_PPC_E500)
#define HPAGE_SHIFT		22	 
#endif
#define HPAGE_SIZE		((1UL) << HPAGE_SHIFT)
#define HPAGE_MASK		(~(HPAGE_SIZE - 1))
#define HUGETLB_PAGE_ORDER	(HPAGE_SHIFT - PAGE_SHIFT)
#define HUGE_MAX_HSTATE		(MMU_PAGE_COUNT-1)
#endif
#define PAGE_MASK      (~((1 << PAGE_SHIFT) - 1))
#define KERNELBASE      ASM_CONST(CONFIG_KERNEL_START)
#define PAGE_OFFSET	ASM_CONST(CONFIG_PAGE_OFFSET)
#define LOAD_OFFSET	ASM_CONST((CONFIG_KERNEL_START-CONFIG_PHYSICAL_START))
#if defined(CONFIG_NONSTATIC_KERNEL)
#ifndef __ASSEMBLY__
extern phys_addr_t memstart_addr;
extern phys_addr_t kernstart_addr;
#if defined(CONFIG_RELOCATABLE) && defined(CONFIG_PPC32)
extern long long virt_phys_offset;
#endif
#endif  
#define PHYSICAL_START	kernstart_addr
#else	 
#define PHYSICAL_START	ASM_CONST(CONFIG_PHYSICAL_START)
#endif
#if defined(CONFIG_PPC32) && defined(CONFIG_BOOKE)
#ifdef CONFIG_RELOCATABLE
#define VIRT_PHYS_OFFSET virt_phys_offset
#else
#define VIRT_PHYS_OFFSET (KERNELBASE - PHYSICAL_START)
#endif
#endif
#ifdef CONFIG_PPC64
#define MEMORY_START	0UL
#elif defined(CONFIG_NONSTATIC_KERNEL)
#define MEMORY_START	memstart_addr
#else
#define MEMORY_START	(PHYSICAL_START + PAGE_OFFSET - KERNELBASE)
#endif
#ifdef CONFIG_FLATMEM
#define ARCH_PFN_OFFSET		((unsigned long)(MEMORY_START >> PAGE_SHIFT))
#endif
#if defined(CONFIG_PPC32) && defined(CONFIG_BOOKE)
#define __va(x) ((void *)(unsigned long)((phys_addr_t)(x) + VIRT_PHYS_OFFSET))
#define __pa(x) ((phys_addr_t)(unsigned long)(x) - VIRT_PHYS_OFFSET)
#else
#ifdef CONFIG_PPC64
#define VIRTUAL_WARN_ON(x)	WARN_ON(IS_ENABLED(CONFIG_DEBUG_VIRTUAL) && (x))
#define __va(x)								\
({									\
	VIRTUAL_WARN_ON((unsigned long)(x) >= PAGE_OFFSET);		\
	(void *)(unsigned long)((phys_addr_t)(x) | PAGE_OFFSET);	\
})
#define __pa(x)								\
({									\
	VIRTUAL_WARN_ON((unsigned long)(x) < PAGE_OFFSET);		\
	(unsigned long)(x) & 0x0fffffffffffffffUL;			\
})
#else  
#define __va(x) ((void *)(unsigned long)((phys_addr_t)(x) + PAGE_OFFSET - MEMORY_START))
#define __pa(x) ((unsigned long)(x) - PAGE_OFFSET + MEMORY_START)
#endif
#endif
#ifndef __ASSEMBLY__
static inline unsigned long virt_to_pfn(const void *kaddr)
{
	return __pa(kaddr) >> PAGE_SHIFT;
}
static inline const void *pfn_to_kaddr(unsigned long pfn)
{
	return __va(pfn << PAGE_SHIFT);
}
#endif
#define virt_to_page(kaddr)	pfn_to_page(virt_to_pfn(kaddr))
#define virt_addr_valid(vaddr)	({					\
	unsigned long _addr = (unsigned long)vaddr;			\
	_addr >= PAGE_OFFSET && _addr < (unsigned long)high_memory &&	\
	pfn_valid(virt_to_pfn((void *)_addr));				\
})
#define VM_DATA_DEFAULT_FLAGS32	VM_DATA_FLAGS_TSK_EXEC
#define VM_DATA_DEFAULT_FLAGS64	VM_DATA_FLAGS_NON_EXEC
#ifdef __powerpc64__
#include <asm/page_64.h>
#else
#include <asm/page_32.h>
#endif
#ifdef CONFIG_PPC_BOOK3E_64
#define is_kernel_addr(x)	((x) >= 0x8000000000000000ul)
#elif defined(CONFIG_PPC_BOOK3S_64)
#define is_kernel_addr(x)	((x) >= PAGE_OFFSET)
#else
#define is_kernel_addr(x)	((x) >= TASK_SIZE)
#endif
#ifndef CONFIG_PPC_BOOK3S_64
#ifdef CONFIG_PPC64
#define PD_HUGE 0x8000000000000000UL
#else
#define PD_HUGE 0x80000000
#endif
#else	 
#define HUGEPD_ADDR_MASK	(0x0ffffffffffffffful & ~HUGEPD_SHIFT_MASK)
#endif  
#ifdef CONFIG_PPC_8xx
#define HUGEPD_SHIFT_MASK     0xfff
#else
#define HUGEPD_SHIFT_MASK     0x3f
#endif
#ifndef __ASSEMBLY__
#ifdef CONFIG_PPC_BOOK3S_64
#include <asm/pgtable-be-types.h>
#else
#include <asm/pgtable-types.h>
#endif
struct page;
extern void clear_user_page(void *page, unsigned long vaddr, struct page *pg);
extern void copy_user_page(void *to, void *from, unsigned long vaddr,
		struct page *p);
extern int devmem_is_allowed(unsigned long pfn);
#ifdef CONFIG_PPC_SMLPAR
void arch_free_page(struct page *page, int order);
#define HAVE_ARCH_FREE_PAGE
#endif
struct vm_area_struct;
extern unsigned long kernstart_virt_addr;
static inline unsigned long kaslr_offset(void)
{
	return kernstart_virt_addr - KERNELBASE;
}
#include <asm-generic/memory_model.h>
#endif  
#endif  
