#ifndef _ASM_FIXMAP_H
#define _ASM_FIXMAP_H
#include <linux/kernel.h>
#include <linux/threads.h>
#include <asm/page.h>
enum fixed_addresses {
#define FIX_N_COLOURS 8
	FIX_CMAP_BEGIN,
	FIX_CMAP_END = FIX_CMAP_BEGIN + (FIX_N_COLOURS * NR_CPUS) - 1,
#ifdef CONFIG_IOREMAP_FIXED
#define FIX_N_IOREMAPS	32
	FIX_IOREMAP_BEGIN,
	FIX_IOREMAP_END = FIX_IOREMAP_BEGIN + FIX_N_IOREMAPS - 1,
#endif
	__end_of_fixed_addresses
};
extern void __set_fixmap(enum fixed_addresses idx,
			 unsigned long phys, pgprot_t flags);
extern void __clear_fixmap(enum fixed_addresses idx, pgprot_t flags);
#define FIXADDR_TOP	(P4SEG - PAGE_SIZE)
#define FIXADDR_SIZE	(__end_of_fixed_addresses << PAGE_SHIFT)
#define FIXADDR_START	(FIXADDR_TOP - FIXADDR_SIZE)
#define FIXMAP_PAGE_NOCACHE PAGE_KERNEL_NOCACHE
#include <asm-generic/fixmap.h>
#endif
