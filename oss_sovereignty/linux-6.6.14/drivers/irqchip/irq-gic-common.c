
 

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqchip/arm-gic.h>

#include "irq-gic-common.h"

static DEFINE_RAW_SPINLOCK(irq_controller_lock);

void gic_enable_of_quirks(const struct device_node *np,
			  const struct gic_quirk *quirks, void *data)
{
	for (; quirks->desc; quirks++) {
		if (!quirks->compatible && !quirks->property)
			continue;
		if (quirks->compatible &&
		    !of_device_is_compatible(np, quirks->compatible))
			continue;
		if (quirks->property &&
		    !of_property_read_bool(np, quirks->property))
			continue;
		if (quirks->init(data))
			pr_info("GIC: enabling workaround for %s\n",
				quirks->desc);
	}
}

void gic_enable_quirks(u32 iidr, const struct gic_quirk *quirks,
		void *data)
{
	for (; quirks->desc; quirks++) {
		if (quirks->compatible || quirks->property)
			continue;
		if (quirks->iidr != (quirks->mask & iidr))
			continue;
		if (quirks->init(data))
			pr_info("GIC: enabling workaround for %s\n",
				quirks->desc);
	}
}

int gic_configure_irq(unsigned int irq, unsigned int type,
		       void __iomem *base, void (*sync_access)(void))
{
	u32 confmask = 0x2 << ((irq % 16) * 2);
	u32 confoff = (irq / 16) * 4;
	u32 val, oldval;
	int ret = 0;
	unsigned long flags;

	 
	raw_spin_lock_irqsave(&irq_controller_lock, flags);
	val = oldval = readl_relaxed(base + confoff);
	if (type & IRQ_TYPE_LEVEL_MASK)
		val &= ~confmask;
	else if (type & IRQ_TYPE_EDGE_BOTH)
		val |= confmask;

	 
	if (val == oldval) {
		raw_spin_unlock_irqrestore(&irq_controller_lock, flags);
		return 0;
	}

	 
	writel_relaxed(val, base + confoff);
	if (readl_relaxed(base + confoff) != val)
		ret = -EINVAL;

	raw_spin_unlock_irqrestore(&irq_controller_lock, flags);

	if (sync_access)
		sync_access();

	return ret;
}

void gic_dist_config(void __iomem *base, int gic_irqs,
		     void (*sync_access)(void))
{
	unsigned int i;

	 
	for (i = 32; i < gic_irqs; i += 16)
		writel_relaxed(GICD_INT_ACTLOW_LVLTRIG,
					base + GIC_DIST_CONFIG + i / 4);

	 
	for (i = 32; i < gic_irqs; i += 4)
		writel_relaxed(GICD_INT_DEF_PRI_X4, base + GIC_DIST_PRI + i);

	 
	for (i = 32; i < gic_irqs; i += 32) {
		writel_relaxed(GICD_INT_EN_CLR_X32,
			       base + GIC_DIST_ACTIVE_CLEAR + i / 8);
		writel_relaxed(GICD_INT_EN_CLR_X32,
			       base + GIC_DIST_ENABLE_CLEAR + i / 8);
	}

	if (sync_access)
		sync_access();
}

void gic_cpu_config(void __iomem *base, int nr, void (*sync_access)(void))
{
	int i;

	 
	for (i = 0; i < nr; i += 32) {
		writel_relaxed(GICD_INT_EN_CLR_X32,
			       base + GIC_DIST_ACTIVE_CLEAR + i / 8);
		writel_relaxed(GICD_INT_EN_CLR_X32,
			       base + GIC_DIST_ENABLE_CLEAR + i / 8);
	}

	 
	for (i = 0; i < nr; i += 4)
		writel_relaxed(GICD_INT_DEF_PRI_X4,
					base + GIC_DIST_PRI + i * 4 / 4);

	if (sync_access)
		sync_access();
}
