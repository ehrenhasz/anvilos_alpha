#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H
#define UNCACHEABLE_ADDR	0xfa050000	 
#define VIO_BASE        0xf8000000	 
#define VIO_SHIFT       3		 
#define PIO_START       0x80000000	 
#define io_p2v( x )             \
   IOMEM( (((x)&0x00ffffff) | (((x)&0x30000000)>>VIO_SHIFT)) + VIO_BASE )
#define io_v2p( x )             \
   ( (((x)&0x00ffffff) | (((x)&(0x30000000>>VIO_SHIFT))<<VIO_SHIFT)) + PIO_START )
#define __MREG(x)	IOMEM(io_p2v(x))
#ifndef __ASSEMBLY__
# define __REG(x)	(*((volatile unsigned long __iomem *)io_p2v(x)))
# define __PREG(x)	(io_v2p((unsigned long)&(x)))
#else
# define __REG(x)	io_p2v(x)
# define __PREG(x)	io_v2p(x)
#endif
#include "SA-1100.h"
#endif   
