
 

#define __GENERATING_BOUNDS_H
 
#include <linux/page-flags.h>
#include <linux/mmzone.h>
#include <linux/kbuild.h>
#include <linux/log2.h>
#include <linux/spinlock_types.h>

int main(void)
{
	 
	DEFINE(NR_PAGEFLAGS, __NR_PAGEFLAGS);
	DEFINE(MAX_NR_ZONES, __MAX_NR_ZONES);
#ifdef CONFIG_SMP
	DEFINE(NR_CPUS_BITS, ilog2(CONFIG_NR_CPUS));
#endif
	DEFINE(SPINLOCK_SIZE, sizeof(spinlock_t));
#ifdef CONFIG_LRU_GEN
	DEFINE(LRU_GEN_WIDTH, order_base_2(MAX_NR_GENS + 1));
	DEFINE(__LRU_REFS_WIDTH, MAX_NR_TIERS - 2);
#else
	DEFINE(LRU_GEN_WIDTH, 0);
	DEFINE(__LRU_REFS_WIDTH, 0);
#endif
	 

	return 0;
}
