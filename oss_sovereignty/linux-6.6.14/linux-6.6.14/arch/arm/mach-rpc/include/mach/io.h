#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H
#include <mach/hardware.h>
#define IO_SPACE_LIMIT 0xffff
#define __io(a)		(PCIO_BASE + ((a) << 2))
#endif
