#ifndef _ASM_CACHE_H
#define _ASM_CACHE_H
#include <kmalloc.h>
#define L1_CACHE_SHIFT		CONFIG_MIPS_L1_CACHE_SHIFT
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)
#define __read_mostly __section(".data..read_mostly")
extern void cache_noop(void);
#endif  
