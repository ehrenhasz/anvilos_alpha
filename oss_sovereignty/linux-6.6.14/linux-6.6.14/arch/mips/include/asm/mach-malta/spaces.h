#ifndef _ASM_MALTA_SPACES_H
#define _ASM_MALTA_SPACES_H
#ifdef CONFIG_EVA
#define PAGE_OFFSET	_AC(0x0, UL)
#define PHYS_OFFSET	_AC(0x80000000, UL)
#define HIGHMEM_START	_AC(0xffff0000, UL)
#define __pa_symbol(x)	(RELOC_HIDE((unsigned long)(x), 0))
#endif  
#include <asm/mach-generic/spaces.h>
#endif  
