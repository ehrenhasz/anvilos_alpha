#ifndef _UAPI__ASM_ARC_PAGE_H
#define _UAPI__ASM_ARC_PAGE_H
#include <linux/const.h>
#if defined(CONFIG_ARC_PAGE_SIZE_16K)
#define PAGE_SHIFT 14
#elif defined(CONFIG_ARC_PAGE_SIZE_4K)
#define PAGE_SHIFT 12
#else
#define PAGE_SHIFT 13
#endif
#define PAGE_SIZE	_BITUL(PAGE_SHIFT)	 
#define PAGE_OFFSET	_AC(0x80000000, UL)	 
#define PAGE_MASK	(~(PAGE_SIZE-1))
#endif  
