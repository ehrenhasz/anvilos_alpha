#ifndef __ASM_MACH_PXA_REGS_H
#define __ASM_MACH_PXA_REGS_H
#define UNCACHED_PHYS_0		0xfe000000
#define UNCACHED_PHYS_0_SIZE	0x00100000
#define io_v2p(x) (0x3c000000 + ((x) & 0x01ffffff) + (((x) & 0x0e000000) << 1))
#define io_p2v(x) IOMEM(0xf2000000 + ((x) & 0x01ffffff) + (((x) & 0x1c000000) >> 1))
#ifndef __ASSEMBLY__
# define __REG(x)	(*((volatile u32 __iomem *)io_p2v(x)))
# define __REG2(x,y)	\
	(*(volatile u32 __iomem*)((u32)&__REG(x) + (y)))
# define __PREG(x)	(io_v2p((u32)&(x)))
#else
# define __REG(x)	io_p2v(x)
# define __PREG(x)	io_v2p(x)
#endif
#endif
