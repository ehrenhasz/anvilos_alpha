#ifndef __UM_FIXMAP_H
#define __UM_FIXMAP_H
#include <asm/processor.h>
#include <asm/archparam.h>
#include <asm/page.h>
#include <linux/threads.h>
enum fixed_addresses {
	__end_of_fixed_addresses
};
extern void __set_fixmap (enum fixed_addresses idx,
			  unsigned long phys, pgprot_t flags);
#define FIXADDR_TOP	(TASK_SIZE - 2 * PAGE_SIZE)
#define FIXADDR_SIZE	(__end_of_fixed_addresses << PAGE_SHIFT)
#define FIXADDR_START	(FIXADDR_TOP - FIXADDR_SIZE)
#include <asm-generic/fixmap.h>
#endif
