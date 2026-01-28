#ifndef __VIRT_CONVERT__
#define __VIRT_CONVERT__
#ifdef __KERNEL__
#include <linux/compiler.h>
#include <linux/mmzone.h>
#include <asm/setup.h>
#include <asm/page.h>
#define virt_to_phys virt_to_phys
static inline unsigned long virt_to_phys(void *address)
{
	return __pa(address);
}
#define phys_to_virt phys_to_virt
static inline void *phys_to_virt(unsigned long address)
{
	return __va(address);
}
#define page_to_phys(page)	(page_to_pfn(page) << PAGE_SHIFT)
#if defined(CONFIG_AMIGA) || defined(CONFIG_VME)
#define virt_to_bus virt_to_phys
#endif
#endif
#endif
