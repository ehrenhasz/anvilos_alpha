#ifndef _ASM_MACH_IP27_SPACES_H
#define _ASM_MACH_IP27_SPACES_H
#include <linux/const.h>
#ifdef CONFIG_64BIT
#define HSPEC_BASE		_AC(0x9000000000000000, UL)
#define IO_BASE			_AC(0x9200000000000000, UL)
#define MSPEC_BASE		_AC(0x9400000000000000, UL)
#define UNCAC_BASE		_AC(0x9600000000000000, UL)
#define CAC_BASE		_AC(0xa800000000000000, UL)
#endif
#define TO_MSPEC(x)		(MSPEC_BASE | ((x) & TO_PHYS_MASK))
#define TO_HSPEC(x)		(HSPEC_BASE | ((x) & TO_PHYS_MASK))
#define HIGHMEM_START		(~0UL)
#include <asm/mach-generic/spaces.h>
#endif  
