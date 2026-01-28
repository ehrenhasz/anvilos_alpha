#ifndef __ASM_PAGE_DEF_H
#define __ASM_PAGE_DEF_H
#include <linux/const.h>
#define PAGE_SHIFT		CONFIG_ARM64_PAGE_SHIFT
#define PAGE_SIZE		(_AC(1, UL) << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))
#endif  
