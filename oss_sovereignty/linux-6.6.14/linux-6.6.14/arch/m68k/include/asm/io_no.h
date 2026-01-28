#ifndef _M68KNOMMU_IO_H
#define _M68KNOMMU_IO_H
#define iomem(a)	((void __iomem *) (a))
#define __raw_readb(addr) \
    ({ u8 __v = (*(__force volatile u8 *) (addr)); __v; })
#define __raw_readw(addr) \
    ({ u16 __v = (*(__force volatile u16 *) (addr)); __v; })
#define __raw_readl(addr) \
    ({ u32 __v = (*(__force volatile u32 *) (addr)); __v; })
#define __raw_writeb(b, addr) (void)((*(__force volatile u8 *) (addr)) = (b))
#define __raw_writew(b, addr) (void)((*(__force volatile u16 *) (addr)) = (b))
#define __raw_writel(b, addr) (void)((*(__force volatile u32 *) (addr)) = (b))
#if defined(CONFIG_COLDFIRE)
#include <asm/byteorder.h>
#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#endif  
#if defined(IOMEMBASE)
static int __cf_internalio(unsigned long addr)
{
	return (addr >= IOMEMBASE) && (addr <= IOMEMBASE + IOMEMSIZE - 1);
}
static int cf_internalio(const volatile void __iomem *addr)
{
	return __cf_internalio((unsigned long) addr);
}
#define readw readw
static inline u16 readw(const volatile void __iomem *addr)
{
	if (cf_internalio(addr))
		return __raw_readw(addr);
	return swab16(__raw_readw(addr));
}
#define readl readl
static inline u32 readl(const volatile void __iomem *addr)
{
	if (cf_internalio(addr))
		return __raw_readl(addr);
	return swab32(__raw_readl(addr));
}
#define writew writew
static inline void writew(u16 value, volatile void __iomem *addr)
{
	if (cf_internalio(addr))
		__raw_writew(value, addr);
	else
		__raw_writew(swab16(value), addr);
}
#define writel writel
static inline void writel(u32 value, volatile void __iomem *addr)
{
	if (cf_internalio(addr))
		__raw_writel(value, addr);
	else
		__raw_writel(swab32(value), addr);
}
#else
#define readb __raw_readb
#define readw __raw_readw
#define readl __raw_readl
#define writeb __raw_writeb
#define writew __raw_writew
#define writel __raw_writel
#endif  
#if defined(CONFIG_PCI)
#define PCI_MEM_PA	0xf0000000		 
#define PCI_MEM_BA	0xf0000000		 
#define PCI_MEM_SIZE	0x08000000		 
#define PCI_MEM_MASK	(PCI_MEM_SIZE - 1)
#define PCI_IO_PA	0xf8000000		 
#define PCI_IO_BA	0x00000000		 
#define PCI_IO_SIZE	0x00010000		 
#define PCI_IO_MASK	(PCI_IO_SIZE - 1)
#define HAVE_ARCH_PIO_SIZE
#define PIO_OFFSET	0
#define PIO_MASK	0xffff
#define PIO_RESERVED	0x10000
#define PCI_IOBASE	((void __iomem *) PCI_IO_PA)
#define PCI_SPACE_LIMIT	PCI_IO_MASK
#endif  
#include <asm/kmap.h>
#include <asm/virtconvert.h>
#endif  
