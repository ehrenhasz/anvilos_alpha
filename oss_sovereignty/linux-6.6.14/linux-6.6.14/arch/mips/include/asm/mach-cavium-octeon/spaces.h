#ifndef _ASM_MACH_CAVIUM_OCTEON_SPACES_H
#define _ASM_MACH_CAVIUM_OCTEON_SPACES_H
#include <linux/const.h>
#ifdef CONFIG_64BIT
#define CAC_BASE		_AC(0x8000000000000000, UL)
#define UNCAC_BASE		_AC(0x8000000000000000, UL)
#define IO_BASE			_AC(0x8000000000000000, UL)
#endif  
#include <asm/mach-generic/spaces.h>
#endif  
