 
 

#ifndef _VMWGFX_MKSSTAT_H_
#define _VMWGFX_MKSSTAT_H_

#include <asm/page.h>
#include <linux/kconfig.h>

 
#define MKSSTAT_PID_RESERVED -1

#if IS_ENABLED(CONFIG_DRM_VMWGFX_MKSSTATS)
 

typedef enum {
	MKSSTAT_KERN_EXECBUF,  
	MKSSTAT_KERN_COTABLE_RESIZE,

	MKSSTAT_KERN_COUNT  
} mksstat_kern_stats_t;

 

static inline void *vmw_mksstat_get_kern_pstat(void *page_addr)
{
	return page_addr + PAGE_SIZE * 1;
}

 

static inline void *vmw_mksstat_get_kern_pinfo(void *page_addr)
{
	return page_addr + PAGE_SIZE * 2;
}

 

static inline void *vmw_mksstat_get_kern_pstrs(void *page_addr)
{
	return page_addr + PAGE_SIZE * 3;
}

 

struct mksstat_timer_t {
  mksstat_kern_stats_t old_top;
	const u64 t0;
	const int slot;
};

#define MKS_STAT_TIME_DECL(kern_cntr)                                     \
	struct mksstat_timer_t _##kern_cntr = {                           \
		.t0 = rdtsc(),                                            \
		.slot = vmw_mksstat_get_kern_slot(current->pid, dev_priv) \
	}

#define MKS_STAT_TIME_PUSH(kern_cntr)                                                               \
	do {                                                                                        \
		if (_##kern_cntr.slot >= 0) {                                                       \
			_##kern_cntr.old_top = dev_priv->mksstat_kern_top_timer[_##kern_cntr.slot]; \
			dev_priv->mksstat_kern_top_timer[_##kern_cntr.slot] = kern_cntr;            \
		}                                                                                   \
	} while (0)

#define MKS_STAT_TIME_POP(kern_cntr)                                                                                                           \
	do {                                                                                                                                   \
		if (_##kern_cntr.slot >= 0) {                                                                                                  \
			const pid_t pid = atomic_cmpxchg(&dev_priv->mksstat_kern_pids[_##kern_cntr.slot], current->pid, MKSSTAT_PID_RESERVED); \
			dev_priv->mksstat_kern_top_timer[_##kern_cntr.slot] = _##kern_cntr.old_top;                                            \
			                                                                                                                       \
			if (pid == current->pid) {                                                                                             \
				const u64 dt = rdtsc() - _##kern_cntr.t0;                                                                      \
				MKSGuestStatCounterTime *pstat;                                                                                \
				                                                                                                               \
				BUG_ON(!dev_priv->mksstat_kern_pages[_##kern_cntr.slot]);                                                      \
				                                                                                                               \
				pstat = vmw_mksstat_get_kern_pstat(page_address(dev_priv->mksstat_kern_pages[_##kern_cntr.slot]));             \
				                                                                                                               \
				atomic64_inc(&pstat[kern_cntr].counter.count);                                                                 \
				atomic64_add(dt, &pstat[kern_cntr].selfCycles);                                                                \
				atomic64_add(dt, &pstat[kern_cntr].totalCycles);                                                               \
				                                                                                                               \
				if (_##kern_cntr.old_top != MKSSTAT_KERN_COUNT)                                                                \
					atomic64_sub(dt, &pstat[_##kern_cntr.old_top].selfCycles);                                             \
					                                                                                                       \
				atomic_set(&dev_priv->mksstat_kern_pids[_##kern_cntr.slot], current->pid);                                     \
			}                                                                                                                      \
		}                                                                                                                              \
	} while (0)

#else
#define MKS_STAT_TIME_DECL(kern_cntr)
#define MKS_STAT_TIME_PUSH(kern_cntr)
#define MKS_STAT_TIME_POP(kern_cntr)

#endif  

#endif
