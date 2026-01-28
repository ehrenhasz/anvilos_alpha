#ifndef _ASM_HEXAGON_MEM_LAYOUT_H
#define _ASM_HEXAGON_MEM_LAYOUT_H
#include <linux/const.h>
#define PAGE_OFFSET			_AC(0xc0000000, UL)
#ifdef CONFIG_HEXAGON_PHYS_OFFSET
#ifndef __ASSEMBLY__
extern unsigned long	__phys_offset;
#endif
#define PHYS_OFFSET	__phys_offset
#endif
#ifndef PHYS_OFFSET
#define PHYS_OFFSET	0
#endif
#define PHYS_PFN_OFFSET	(PHYS_OFFSET >> PAGE_SHIFT)
#define ARCH_PFN_OFFSET	PHYS_PFN_OFFSET
#define TASK_SIZE			(PAGE_OFFSET)
#define STACK_TOP			TASK_SIZE
#define STACK_TOP_MAX			TASK_SIZE
#ifndef __ASSEMBLY__
enum fixed_addresses {
	FIX_KMAP_BEGIN,
	FIX_KMAP_END,   
	__end_of_fixed_addresses
};
#define MIN_KERNEL_SEG (PAGE_OFFSET >> PGDIR_SHIFT)    
extern int max_kernel_seg;
#define VMALLOC_START ((unsigned long) __va(high_memory + VMALLOC_OFFSET))
#define VMALLOC_OFFSET PAGE_SIZE
#define FIXADDR_TOP     0xfe000000
#define FIXADDR_SIZE    (__end_of_fixed_addresses << PAGE_SHIFT)
#define FIXADDR_START   (FIXADDR_TOP - FIXADDR_SIZE)
#define LAST_PKMAP	PTRS_PER_PTE
#define LAST_PKMAP_MASK	(LAST_PKMAP - 1)
#define PKMAP_NR(virt)	((virt - PKMAP_BASE) >> PAGE_SHIFT)
#define PKMAP_ADDR(nr)	(PKMAP_BASE + ((nr) << PAGE_SHIFT))
#define PKMAP_BASE (FIXADDR_START-PAGE_SIZE*LAST_PKMAP)
#define VMALLOC_END (PKMAP_BASE-PAGE_SIZE*2)
#endif  
#endif  
