#ifndef _XTENSA_IO_H
#define _XTENSA_IO_H
#include <asm/byteorder.h>
#include <asm/page.h>
#include <asm/vectors.h>
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/pgtable.h>
#include <linux/types.h>
#define IOADDR(x)		(XCHAL_KIO_BYPASS_VADDR + (x))
#define IO_SPACE_LIMIT ~0
#define PCI_IOBASE		((void __iomem *)XCHAL_KIO_BYPASS_VADDR)
#ifdef CONFIG_MMU
void __iomem *ioremap_prot(phys_addr_t phys_addr, size_t size,
			   unsigned long prot);
#define ioremap_prot ioremap_prot
#define iounmap iounmap
static inline void __iomem *ioremap(unsigned long offset, unsigned long size)
{
	if (offset >= XCHAL_KIO_PADDR
	    && offset - XCHAL_KIO_PADDR < XCHAL_KIO_SIZE)
		return (void*)(offset-XCHAL_KIO_PADDR+XCHAL_KIO_BYPASS_VADDR);
	else
		return ioremap_prot(offset, size,
			pgprot_val(pgprot_noncached(PAGE_KERNEL)));
}
#define ioremap ioremap
static inline void __iomem *ioremap_cache(unsigned long offset,
		unsigned long size)
{
	if (offset >= XCHAL_KIO_PADDR
	    && offset - XCHAL_KIO_PADDR < XCHAL_KIO_SIZE)
		return (void*)(offset-XCHAL_KIO_PADDR+XCHAL_KIO_CACHED_VADDR);
	else
		return ioremap_prot(offset, size, pgprot_val(PAGE_KERNEL));
}
#define ioremap_cache ioremap_cache
#endif  
#include <asm-generic/io.h>
#endif	 
