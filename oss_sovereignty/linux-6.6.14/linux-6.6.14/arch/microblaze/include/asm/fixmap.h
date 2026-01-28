#ifndef _ASM_FIXMAP_H
#define _ASM_FIXMAP_H
#ifndef __ASSEMBLY__
#include <linux/kernel.h>
#include <asm/page.h>
#ifdef CONFIG_HIGHMEM
#include <linux/threads.h>
#include <asm/kmap_size.h>
#endif
#define FIXADDR_TOP	((unsigned long)(-PAGE_SIZE))
enum fixed_addresses {
	FIX_HOLE,
#ifdef CONFIG_HIGHMEM
	FIX_KMAP_BEGIN,	 
	FIX_KMAP_END = FIX_KMAP_BEGIN + (KM_MAX_IDX * num_possible_cpus()) - 1,
#endif
	__end_of_fixed_addresses
};
extern void __set_fixmap(enum fixed_addresses idx,
					phys_addr_t phys, pgprot_t flags);
#define __FIXADDR_SIZE	(__end_of_fixed_addresses << PAGE_SHIFT)
#define FIXADDR_START		(FIXADDR_TOP - __FIXADDR_SIZE)
#define FIXMAP_PAGE_NOCACHE PAGE_KERNEL_CI
#include <asm-generic/fixmap.h>
#endif  
#endif
