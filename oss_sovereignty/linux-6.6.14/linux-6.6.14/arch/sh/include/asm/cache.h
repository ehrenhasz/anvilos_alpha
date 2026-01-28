#ifndef __ASM_SH_CACHE_H
#define __ASM_SH_CACHE_H
#include <linux/init.h>
#include <cpu/cache.h>
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)
#define ARCH_DMA_MINALIGN	L1_CACHE_BYTES
#define __read_mostly __section(".data..read_mostly")
#ifndef __ASSEMBLY__
struct cache_info {
	unsigned int ways;		 
	unsigned int sets;		 
	unsigned int linesz;		 
	unsigned int way_size;		 
	unsigned int way_incr;
	unsigned int entry_shift;
	unsigned int entry_mask;
	unsigned int alias_mask;
	unsigned int n_aliases;		 
	unsigned long flags;
};
#endif  
#endif  
