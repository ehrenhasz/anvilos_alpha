#ifndef _ASM_MACH_GENERIC_SPACES_H
#define _ASM_MACH_GENERIC_SPACES_H
#include <linux/const.h>
#include <asm/mipsregs.h>
#ifndef IO_SPACE_LIMIT
#define IO_SPACE_LIMIT 0xffff
#endif
#ifndef __ASSEMBLY__
# if defined(CONFIG_MIPS_AUTO_PFN_OFFSET)
#  define PHYS_OFFSET		((unsigned long)PFN_PHYS(ARCH_PFN_OFFSET))
# elif !defined(PHYS_OFFSET)
#  define PHYS_OFFSET		_AC(0, UL)
# endif
#endif  
#ifdef CONFIG_32BIT
#define CAC_BASE		_AC(0x80000000, UL)
#ifndef IO_BASE
#define IO_BASE			_AC(0xa0000000, UL)
#endif
#ifndef UNCAC_BASE
#define UNCAC_BASE		_AC(0xa0000000, UL)
#endif
#ifndef MAP_BASE
#define MAP_BASE		_AC(0xc0000000, UL)
#endif
#ifndef HIGHMEM_START
#define HIGHMEM_START		_AC(0x20000000, UL)
#endif
#endif  
#ifdef CONFIG_64BIT
#ifndef CAC_BASE
#define CAC_BASE	PHYS_TO_XKPHYS(read_c0_config() & CONF_CM_CMASK, 0)
#endif
#ifndef IO_BASE
#define IO_BASE			_AC(0x9000000000000000, UL)
#endif
#ifndef UNCAC_BASE
#define UNCAC_BASE		_AC(0x9000000000000000, UL)
#endif
#ifndef MAP_BASE
#define MAP_BASE		_AC(0xc000000000000000, UL)
#endif
#ifndef HIGHMEM_START
#define HIGHMEM_START		(_AC(1, UL) << _AC(59, UL))
#endif
#define TO_PHYS(x)		(	      ((x) & TO_PHYS_MASK))
#define TO_CAC(x)		(CAC_BASE   | ((x) & TO_PHYS_MASK))
#define TO_UNCAC(x)		(UNCAC_BASE | ((x) & TO_PHYS_MASK))
#endif  
#ifndef PAGE_OFFSET
#define PAGE_OFFSET		(CAC_BASE + PHYS_OFFSET)
#endif
#ifndef FIXADDR_TOP
#define FIXADDR_TOP		((unsigned long)(long)(int)0xfffe0000)
#endif
#endif  
