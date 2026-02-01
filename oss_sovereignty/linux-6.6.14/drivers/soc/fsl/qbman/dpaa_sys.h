 

#ifndef __DPAA_SYS_H
#define __DPAA_SYS_H

#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/prefetch.h>
#include <linux/genalloc.h>
#include <asm/cacheflush.h>
#include <linux/io.h>
#include <linux/delay.h>

 
#define DPAA_PORTAL_CE 0
#define DPAA_PORTAL_CI 1

static inline void dpaa_flush(void *p)
{
	 
#ifdef CONFIG_PPC
	flush_dcache_range((unsigned long)p, (unsigned long)p+64);
#endif
}

#define dpaa_invalidate(p) dpaa_flush(p)

#define dpaa_zero(p) memset(p, 0, 64)

static inline void dpaa_touch_ro(void *p)
{
#if (L1_CACHE_BYTES == 32)
	prefetch(p+32);
#endif
	prefetch(p);
}

 
static inline void dpaa_invalidate_touch_ro(void *p)
{
	dpaa_invalidate(p);
	dpaa_touch_ro(p);
}


#ifdef CONFIG_FSL_DPAA_CHECKING
#define DPAA_ASSERT(x) WARN_ON(!(x))
#else
#define DPAA_ASSERT(x)
#endif

 
static inline u8 dpaa_cyc_diff(u8 ringsize, u8 first, u8 last)
{
	 
	if (first <= last)
		return last - first;
	return ringsize + last - first;
}

 
#define DPAA_GENALLOC_OFF	0x80000000

 
int qbman_init_private_mem(struct device *dev, int idx, dma_addr_t *addr,
				size_t *size);

 
#ifdef CONFIG_PPC
#define QBMAN_MEMREMAP_ATTR	MEMREMAP_WB
#else
#define QBMAN_MEMREMAP_ATTR	MEMREMAP_WC
#endif

static inline int dpaa_set_portal_irq_affinity(struct device *dev,
					       int irq, int cpu)
{
	int ret = 0;

	if (!irq_can_set_affinity(irq)) {
		dev_err(dev, "unable to set IRQ affinity\n");
		return -EINVAL;
	}

	if (cpu == -1 || !cpu_online(cpu))
		cpu = cpumask_any(cpu_online_mask);

	ret = irq_set_affinity(irq, cpumask_of(cpu));
	if (ret)
		dev_err(dev, "irq_set_affinity() on CPU %d failed\n", cpu);

	return ret;
}

#endif	 
