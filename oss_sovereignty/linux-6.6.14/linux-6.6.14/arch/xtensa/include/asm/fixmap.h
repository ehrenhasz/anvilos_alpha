#ifndef _ASM_FIXMAP_H
#define _ASM_FIXMAP_H
#ifdef CONFIG_HIGHMEM
#include <linux/threads.h>
#include <linux/pgtable.h>
#include <asm/kmap_size.h>
enum fixed_addresses {
	FIX_KMAP_BEGIN,
	FIX_KMAP_END = FIX_KMAP_BEGIN +
		(KM_MAX_IDX * NR_CPUS * DCACHE_N_COLORS) - 1,
	__end_of_fixed_addresses
};
#define FIXADDR_END     (XCHAL_KSEG_CACHED_VADDR - PAGE_SIZE)
#define FIXADDR_SIZE	(__end_of_fixed_addresses << PAGE_SHIFT)
#define FIXADDR_START	((FIXADDR_END - FIXADDR_SIZE) & PMD_MASK)
#define FIXADDR_TOP	(FIXADDR_START + FIXADDR_SIZE - PAGE_SIZE)
#include <asm-generic/fixmap.h>
#endif  
#endif
