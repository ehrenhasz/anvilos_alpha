#ifndef _ASM_MACH_IP30_SPACES_H
#define _ASM_MACH_IP30_SPACES_H
#define PHYS_OFFSET	_AC(0x20000000, UL)
#ifdef CONFIG_64BIT
#define CAC_BASE	_AC(0xA800000000000000, UL)
#endif
#include <asm/mach-generic/spaces.h>
#endif  
