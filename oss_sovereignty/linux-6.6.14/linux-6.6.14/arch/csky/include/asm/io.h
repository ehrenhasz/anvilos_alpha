#ifndef __ASM_CSKY_IO_H
#define __ASM_CSKY_IO_H
#include <linux/pgtable.h>
#include <linux/types.h>
#define readb(c)		({ u8  __v = readb_relaxed(c); rmb(); __v; })
#define readw(c)		({ u16 __v = readw_relaxed(c); rmb(); __v; })
#define readl(c)		({ u32 __v = readl_relaxed(c); rmb(); __v; })
#ifdef CONFIG_CPU_HAS_CACHEV2
#define writeb(v,c)		({ wmb(); writeb_relaxed((v),(c)); })
#define writew(v,c)		({ wmb(); writew_relaxed((v),(c)); })
#define writel(v,c)		({ wmb(); writel_relaxed((v),(c)); })
#else
#define writeb(v,c)		({ wmb(); writeb_relaxed((v),(c)); mb(); })
#define writew(v,c)		({ wmb(); writew_relaxed((v),(c)); mb(); })
#define writel(v,c)		({ wmb(); writel_relaxed((v),(c)); mb(); })
#endif
extern void __memcpy_fromio(void *, const volatile void __iomem *, size_t);
extern void __memcpy_toio(volatile void __iomem *, const void *, size_t);
extern void __memset_io(volatile void __iomem *, int, size_t);
#define memset_io(c,v,l)        __memset_io((c),(v),(l))
#define memcpy_fromio(a,c,l)    __memcpy_fromio((a),(c),(l))
#define memcpy_toio(c,a,l)      __memcpy_toio((c),(a),(l))
#define ioremap_wc(addr, size) \
	ioremap_prot((addr), (size), \
		(_PAGE_IOREMAP & ~_CACHE_MASK) | _CACHE_UNCACHED)
#include <asm-generic/io.h>
#endif  
