#ifndef _ASM_IO_H
#define _ASM_IO_H
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/addrspace.h>
#include <asm/cpu.h>
#include <asm/page.h>
#include <asm/pgtable-bits.h>
#include <asm/string.h>
#define page_to_phys(page)	((phys_addr_t)page_to_pfn(page) << PAGE_SHIFT)
extern void __init __iomem *early_ioremap(u64 phys_addr, unsigned long size);
extern void __init early_iounmap(void __iomem *addr, unsigned long size);
#define early_memremap early_ioremap
#define early_memunmap early_iounmap
#ifdef CONFIG_ARCH_IOREMAP
static inline void __iomem *ioremap_prot(phys_addr_t offset, unsigned long size,
					 unsigned long prot_val)
{
	if (prot_val & _CACHE_CC)
		return (void __iomem *)(unsigned long)(CACHE_BASE + offset);
	else
		return (void __iomem *)(unsigned long)(UNCACHE_BASE + offset);
}
#define ioremap(offset, size)		\
	ioremap_prot((offset), (size), pgprot_val(PAGE_KERNEL_SUC))
#define iounmap(addr) 			((void)(addr))
#endif
#define ioremap_wc(offset, size)	\
	ioremap_prot((offset), (size),	\
		pgprot_val(wc_enabled ? PAGE_KERNEL_WUC : PAGE_KERNEL_SUC))
#define ioremap_cache(offset, size)	\
	ioremap_prot((offset), (size), pgprot_val(PAGE_KERNEL))
#define mmiowb() wmb()
extern void __memset_io(volatile void __iomem *dst, int c, size_t count);
extern void __memcpy_toio(volatile void __iomem *to, const void *from, size_t count);
extern void __memcpy_fromio(void *to, const volatile void __iomem *from, size_t count);
#define memset_io(c, v, l)     __memset_io((c), (v), (l))
#define memcpy_fromio(a, c, l) __memcpy_fromio((a), (c), (l))
#define memcpy_toio(c, a, l)   __memcpy_toio((c), (a), (l))
#include <asm-generic/io.h>
#define ARCH_HAS_VALID_PHYS_ADDR_RANGE
extern int valid_phys_addr_range(phys_addr_t addr, size_t size);
extern int valid_mmap_phys_addr_range(unsigned long pfn, size_t size);
#endif  
