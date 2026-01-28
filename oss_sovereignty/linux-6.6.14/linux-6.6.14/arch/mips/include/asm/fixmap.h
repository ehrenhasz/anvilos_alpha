#ifndef _ASM_FIXMAP_H
#define _ASM_FIXMAP_H
#include <asm/page.h>
#include <spaces.h>
#ifdef CONFIG_HIGHMEM
#include <linux/threads.h>
#include <asm/kmap_size.h>
#endif
enum fixed_addresses {
#define FIX_N_COLOURS 8
	FIX_CMAP_BEGIN,
	FIX_CMAP_END = FIX_CMAP_BEGIN + (FIX_N_COLOURS * 2),
#ifdef CONFIG_HIGHMEM
	FIX_KMAP_BEGIN = FIX_CMAP_END + 1,
	FIX_KMAP_END = FIX_KMAP_BEGIN + (KM_MAX_IDX * NR_CPUS) - 1,
#endif
	__end_of_fixed_addresses
};
#define FIXADDR_SIZE	(__end_of_fixed_addresses << PAGE_SHIFT)
#define FIXADDR_START	(FIXADDR_TOP - FIXADDR_SIZE)
#include <asm-generic/fixmap.h>
extern void fixrange_init(unsigned long start, unsigned long end,
	pgd_t *pgd_base);
#endif
