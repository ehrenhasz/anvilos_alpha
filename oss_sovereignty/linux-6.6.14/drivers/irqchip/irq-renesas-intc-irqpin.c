
 

#include <linux/init.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>

#define INTC_IRQPIN_MAX 8  

#define INTC_IRQPIN_REG_SENSE 0  
#define INTC_IRQPIN_REG_PRIO 1  
#define INTC_IRQPIN_REG_SOURCE 2  
#define INTC_IRQPIN_REG_MASK 3  
#define INTC_IRQPIN_REG_CLEAR 4  
#define INTC_IRQPIN_REG_NR_MANDATORY 5
#define INTC_IRQPIN_REG_IRLM 5  
#define INTC_IRQPIN_REG_NR 6

 

struct intc_irqpin_iomem {
	void __iomem *iomem;
	unsigned long (*read)(void __iomem *iomem);
	void (*write)(void __iomem *iomem, unsigned long data);
	int width;
};

struct intc_irqpin_irq {
	int hw_irq;
	int requested_irq;
	int domain_irq;
	struct intc_irqpin_priv *p;
};

struct intc_irqpin_priv {
	struct intc_irqpin_iomem iomem[INTC_IRQPIN_REG_NR];
	struct intc_irqpin_irq irq[INTC_IRQPIN_MAX];
	unsigned int sense_bitfield_width;
	struct platform_device *pdev;
	struct irq_chip irq_chip;
	struct irq_domain *irq_domain;
	atomic_t wakeup_path;
	unsigned shared_irqs:1;
	u8 shared_irq_mask;
};

struct intc_irqpin_config {
	int irlm_bit;		 
};

static unsigned long intc_irqpin_read32(void __iomem *iomem)
{
	return ioread32(iomem);
}

static unsigned long intc_irqpin_read8(void __iomem *iomem)
{
	return ioread8(iomem);
}

static void intc_irqpin_write32(void __iomem *iomem, unsigned long data)
{
	iowrite32(data, iomem);
}

static void intc_irqpin_write8(void __iomem *iomem, unsigned long data)
{
	iowrite8(data, iomem);
}

static inline unsigned long intc_irqpin_read(struct intc_irqpin_priv *p,
					     int reg)
{
	struct intc_irqpin_iomem *i = &p->iomem[reg];

	return i->read(i->iomem);
}

static inline void intc_irqpin_write(struct intc_irqpin_priv *p,
				     int reg, unsigned long data)
{
	struct intc_irqpin_iomem *i = &p->iomem[reg];

	i->write(i->iomem, data);
}

static inline unsigned long intc_irqpin_hwirq_mask(struct intc_irqpin_priv *p,
						   int reg, int hw_irq)
{
	return BIT((p->iomem[reg].width - 1) - hw_irq);
}

static inline void intc_irqpin_irq_write_hwirq(struct intc_irqpin_priv *p,
					       int reg, int hw_irq)
{
	intc_irqpin_write(p, reg, intc_irqpin_hwirq_mask(p, reg, hw_irq));
}

static DEFINE_RAW_SPINLOCK(intc_irqpin_lock);  

static void intc_irqpin_read_modify_write(struct intc_irqpin_priv *p,
					  int reg, int shift,
					  int width, int value)
{
	unsigned long flags;
	unsigned long tmp;

	raw_spin_lock_irqsave(&intc_irqpin_lock, flags);

	tmp = intc_irqpin_read(p, reg);
	tmp &= ~(((1 << width) - 1) << shift);
	tmp |= value << shift;
	intc_irqpin_write(p, reg, tmp);

	raw_spin_unlock_irqrestore(&intc_irqpin_lock, flags);
}

static void intc_irqpin_mask_unmask_prio(struct intc_irqpin_priv *p,
					 int irq, int do_mask)
{
	 
	int bitfield_width = 4;
	int shift = 32 - (irq + 1) * bitfield_width;

	intc_irqpin_read_modify_write(p, INTC_IRQPIN_REG_PRIO,
				      shift, bitfield_width,
				      do_mask ? 0 : (1 << bitfield_width) - 1);
}

static int intc_irqpin_set_sense(struct intc_irqpin_priv *p, int irq, int value)
{
	 
	int bitfield_width = p->sense_bitfield_width;
	int shift = 32 - (irq + 1) * bitfield_width;

	dev_dbg(&p->pdev->dev, "sense irq = %d, mode = %d\n", irq, value);

	if (value >= (1 << bitfield_width))
		return -EINVAL;

	intc_irqpin_read_modify_write(p, INTC_IRQPIN_REG_SENSE, shift,
				      bitfield_width, value);
	return 0;
}

static void intc_irqpin_dbg(struct intc_irqpin_irq *i, char *str)
{
	dev_dbg(&i->p->pdev->dev, "%s (%d:%d:%d)\n",
		str, i->requested_irq, i->hw_irq, i->domain_irq);
}

static void intc_irqpin_irq_enable(struct irq_data *d)
{
	struct intc_irqpin_priv *p = irq_data_get_irq_chip_data(d);
	int hw_irq = irqd_to_hwirq(d);

	intc_irqpin_dbg(&p->irq[hw_irq], "enable");
	intc_irqpin_irq_write_hwirq(p, INTC_IRQPIN_REG_CLEAR, hw_irq);
}

static void intc_irqpin_irq_disable(struct irq_data *d)
{
	struct intc_irqpin_priv *p = irq_data_get_irq_chip_data(d);
	int hw_irq = irqd_to_hwirq(d);

	intc_irqpin_dbg(&p->irq[hw_irq], "disable");
	intc_irqpin_irq_write_hwirq(p, INTC_IRQPIN_REG_MASK, hw_irq);
}

static void intc_irqpin_shared_irq_enable(struct irq_data *d)
{
	struct intc_irqpin_priv *p = irq_data_get_irq_chip_data(d);
	int hw_irq = irqd_to_hwirq(d);

	intc_irqpin_dbg(&p->irq[hw_irq], "shared enable");
	intc_irqpin_irq_write_hwirq(p, INTC_IRQPIN_REG_CLEAR, hw_irq);

	p->shared_irq_mask &= ~BIT(hw_irq);
}

static void intc_irqpin_shared_irq_disable(struct irq_data *d)
{
	struct intc_irqpin_priv *p = irq_data_get_irq_chip_data(d);
	int hw_irq = irqd_to_hwirq(d);

	intc_irqpin_dbg(&p->irq[hw_irq], "shared disable");
	intc_irqpin_irq_write_hwirq(p, INTC_IRQPIN_REG_MASK, hw_irq);

	p->shared_irq_mask |= BIT(hw_irq);
}

static void intc_irqpin_irq_enable_force(struct irq_data *d)
{
	struct intc_irqpin_priv *p = irq_data_get_irq_chip_data(d);
	int irq = p->irq[irqd_to_hwirq(d)].requested_irq;

	intc_irqpin_irq_enable(d);

	 
	irq_get_chip(irq)->irq_unmask(irq_get_irq_data(irq));
}

static void intc_irqpin_irq_disable_force(struct irq_data *d)
{
	struct intc_irqpin_priv *p = irq_data_get_irq_chip_data(d);
	int irq = p->irq[irqd_to_hwirq(d)].requested_irq;

	 
	irq_get_chip(irq)->irq_mask(irq_get_irq_data(irq));
	intc_irqpin_irq_disable(d);
}

#define INTC_IRQ_SENSE_VALID 0x10
#define INTC_IRQ_SENSE(x) (x + INTC_IRQ_SENSE_VALID)

static unsigned char intc_irqpin_sense[IRQ_TYPE_SENSE_MASK + 1] = {
	[IRQ_TYPE_EDGE_FALLING] = INTC_IRQ_SENSE(0x00),
	[IRQ_TYPE_EDGE_RISING] = INTC_IRQ_SENSE(0x01),
	[IRQ_TYPE_LEVEL_LOW] = INTC_IRQ_SENSE(0x02),
	[IRQ_TYPE_LEVEL_HIGH] = INTC_IRQ_SENSE(0x03),
	[IRQ_TYPE_EDGE_BOTH] = INTC_IRQ_SENSE(0x04),
};

static int intc_irqpin_irq_set_type(struct irq_data *d, unsigned int type)
{
	unsigned char value = intc_irqpin_sense[type & IRQ_TYPE_SENSE_MASK];
	struct intc_irqpin_priv *p = irq_data_get_irq_chip_data(d);

	if (!(value & INTC_IRQ_SENSE_VALID))
		return -EINVAL;

	return intc_irqpin_set_sense(p, irqd_to_hwirq(d),
				     value ^ INTC_IRQ_SENSE_VALID);
}

static int intc_irqpin_irq_set_wake(struct irq_data *d, unsigned int on)
{
	struct intc_irqpin_priv *p = irq_data_get_irq_chip_data(d);
	int hw_irq = irqd_to_hwirq(d);

	irq_set_irq_wake(p->irq[hw_irq].requested_irq, on);
	if (on)
		atomic_inc(&p->wakeup_path);
	else
		atomic_dec(&p->wakeup_path);

	return 0;
}

static irqreturn_t intc_irqpin_irq_handler(int irq, void *dev_id)
{
	struct intc_irqpin_irq *i = dev_id;
	struct intc_irqpin_priv *p = i->p;
	unsigned long bit;

	intc_irqpin_dbg(i, "demux1");
	bit = intc_irqpin_hwirq_mask(p, INTC_IRQPIN_REG_SOURCE, i->hw_irq);

	if (intc_irqpin_read(p, INTC_IRQPIN_REG_SOURCE) & bit) {
		intc_irqpin_write(p, INTC_IRQPIN_REG_SOURCE, ~bit);
		intc_irqpin_dbg(i, "demux2");
		generic_handle_irq(i->domain_irq);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static irqreturn_t intc_irqpin_shared_irq_handler(int irq, void *dev_id)
{
	struct intc_irqpin_priv *p = dev_id;
	unsigned int reg_source = intc_irqpin_read(p, INTC_IRQPIN_REG_SOURCE);
	irqreturn_t status = IRQ_NONE;
	int k;

	for (k = 0; k < 8; k++) {
		if (reg_source & BIT(7 - k)) {
			if (BIT(k) & p->shared_irq_mask)
				continue;

			status |= intc_irqpin_irq_handler(irq, &p->irq[k]);
		}
	}

	return status;
}

 
static struct lock_class_key intc_irqpin_irq_lock_class;

 
static struct lock_class_key intc_irqpin_irq_request_class;

static int intc_irqpin_irq_domain_map(struct irq_domain *h, unsigned int virq,
				      irq_hw_number_t hw)
{
	struct intc_irqpin_priv *p = h->host_data;

	p->irq[hw].domain_irq = virq;
	p->irq[hw].hw_irq = hw;

	intc_irqpin_dbg(&p->irq[hw], "map");
	irq_set_chip_data(virq, h->host_data);
	irq_set_lockdep_class(virq, &intc_irqpin_irq_lock_class,
			      &intc_irqpin_irq_request_class);
	irq_set_chip_and_handler(virq, &p->irq_chip, handle_level_irq);
	return 0;
}

static const struct irq_domain_ops intc_irqpin_irq_domain_ops = {
	.map	= intc_irqpin_irq_domain_map,
	.xlate  = irq_domain_xlate_twocell,
};

static const struct intc_irqpin_config intc_irqpin_irlm_r8a777x = {
	.irlm_bit = 23,  
};

static const struct intc_irqpin_config intc_irqpin_rmobile = {
	.irlm_bit = -1,
};

static const struct of_device_id intc_irqpin_dt_ids[] = {
	{ .compatible = "renesas,intc-irqpin", },
	{ .compatible = "renesas,intc-irqpin-r8a7778",
	  .data = &intc_irqpin_irlm_r8a777x },
	{ .compatible = "renesas,intc-irqpin-r8a7779",
	  .data = &intc_irqpin_irlm_r8a777x },
	{ .compatible = "renesas,intc-irqpin-r8a7740",
	  .data = &intc_irqpin_rmobile },
	{ .compatible = "renesas,intc-irqpin-sh73a0",
	  .data = &intc_irqpin_rmobile },
	{},
};
MODULE_DEVICE_TABLE(of, intc_irqpin_dt_ids);

static int intc_irqpin_probe(struct platform_device *pdev)
{
	const struct intc_irqpin_config *config;
	struct device *dev = &pdev->dev;
	struct intc_irqpin_priv *p;
	struct intc_irqpin_iomem *i;
	struct resource *io[INTC_IRQPIN_REG_NR];
	struct irq_chip *irq_chip;
	void (*enable_fn)(struct irq_data *d);
	void (*disable_fn)(struct irq_data *d);
	const char *name = dev_name(dev);
	bool control_parent;
	unsigned int nirqs;
	int ref_irq;
	int ret;
	int k;

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	 
	of_property_read_u32(dev->of_node, "sense-bitfield-width",
			     &p->sense_bitfield_width);
	control_parent = of_property_read_bool(dev->of_node, "control-parent");
	if (!p->sense_bitfield_width)
		p->sense_bitfield_width = 4;  

	p->pdev = pdev;
	platform_set_drvdata(pdev, p);

	config = of_device_get_match_data(dev);

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	 
	memset(io, 0, sizeof(io));
	for (k = 0; k < INTC_IRQPIN_REG_NR; k++) {
		io[k] = platform_get_resource(pdev, IORESOURCE_MEM, k);
		if (!io[k] && k < INTC_IRQPIN_REG_NR_MANDATORY) {
			dev_err(dev, "not enough IOMEM resources\n");
			ret = -EINVAL;
			goto err0;
		}
	}

	 
	for (k = 0; k < INTC_IRQPIN_MAX; k++) {
		ret = platform_get_irq_optional(pdev, k);
		if (ret == -ENXIO)
			break;
		if (ret < 0)
			goto err0;

		p->irq[k].p = p;
		p->irq[k].requested_irq = ret;
	}

	nirqs = k;
	if (nirqs < 1) {
		dev_err(dev, "not enough IRQ resources\n");
		ret = -EINVAL;
		goto err0;
	}

	 
	for (k = 0; k < INTC_IRQPIN_REG_NR; k++) {
		i = &p->iomem[k];

		 
		if (!io[k])
			continue;

		switch (resource_size(io[k])) {
		case 1:
			i->width = 8;
			i->read = intc_irqpin_read8;
			i->write = intc_irqpin_write8;
			break;
		case 4:
			i->width = 32;
			i->read = intc_irqpin_read32;
			i->write = intc_irqpin_write32;
			break;
		default:
			dev_err(dev, "IOMEM size mismatch\n");
			ret = -EINVAL;
			goto err0;
		}

		i->iomem = devm_ioremap(dev, io[k]->start,
					resource_size(io[k]));
		if (!i->iomem) {
			dev_err(dev, "failed to remap IOMEM\n");
			ret = -ENXIO;
			goto err0;
		}
	}

	 
	if (config && config->irlm_bit >= 0) {
		if (io[INTC_IRQPIN_REG_IRLM])
			intc_irqpin_read_modify_write(p, INTC_IRQPIN_REG_IRLM,
						      config->irlm_bit, 1, 1);
		else
			dev_warn(dev, "unable to select IRLM mode\n");
	}

	 
	for (k = 0; k < nirqs; k++)
		intc_irqpin_mask_unmask_prio(p, k, 1);

	 
	intc_irqpin_write(p, INTC_IRQPIN_REG_SOURCE, 0x0);

	 
	ref_irq = p->irq[0].requested_irq;
	p->shared_irqs = 1;
	for (k = 1; k < nirqs; k++) {
		if (ref_irq != p->irq[k].requested_irq) {
			p->shared_irqs = 0;
			break;
		}
	}

	 
	if (control_parent) {
		enable_fn = intc_irqpin_irq_enable_force;
		disable_fn = intc_irqpin_irq_disable_force;
	} else if (!p->shared_irqs) {
		enable_fn = intc_irqpin_irq_enable;
		disable_fn = intc_irqpin_irq_disable;
	} else {
		enable_fn = intc_irqpin_shared_irq_enable;
		disable_fn = intc_irqpin_shared_irq_disable;
	}

	irq_chip = &p->irq_chip;
	irq_chip->name = "intc-irqpin";
	irq_chip->irq_mask = disable_fn;
	irq_chip->irq_unmask = enable_fn;
	irq_chip->irq_set_type = intc_irqpin_irq_set_type;
	irq_chip->irq_set_wake = intc_irqpin_irq_set_wake;
	irq_chip->flags	= IRQCHIP_MASK_ON_SUSPEND;

	p->irq_domain = irq_domain_add_simple(dev->of_node, nirqs, 0,
					      &intc_irqpin_irq_domain_ops, p);
	if (!p->irq_domain) {
		ret = -ENXIO;
		dev_err(dev, "cannot initialize irq domain\n");
		goto err0;
	}

	irq_domain_set_pm_device(p->irq_domain, dev);

	if (p->shared_irqs) {
		 
		if (devm_request_irq(dev, p->irq[0].requested_irq,
				intc_irqpin_shared_irq_handler,
				IRQF_SHARED, name, p)) {
			dev_err(dev, "failed to request low IRQ\n");
			ret = -ENOENT;
			goto err1;
		}
	} else {
		 
		for (k = 0; k < nirqs; k++) {
			if (devm_request_irq(dev, p->irq[k].requested_irq,
					     intc_irqpin_irq_handler, 0, name,
					     &p->irq[k])) {
				dev_err(dev, "failed to request low IRQ\n");
				ret = -ENOENT;
				goto err1;
			}
		}
	}

	 
	for (k = 0; k < nirqs; k++)
		intc_irqpin_mask_unmask_prio(p, k, 0);

	dev_info(dev, "driving %d irqs\n", nirqs);

	return 0;

err1:
	irq_domain_remove(p->irq_domain);
err0:
	pm_runtime_put(dev);
	pm_runtime_disable(dev);
	return ret;
}

static int intc_irqpin_remove(struct platform_device *pdev)
{
	struct intc_irqpin_priv *p = platform_get_drvdata(pdev);

	irq_domain_remove(p->irq_domain);
	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static int __maybe_unused intc_irqpin_suspend(struct device *dev)
{
	struct intc_irqpin_priv *p = dev_get_drvdata(dev);

	if (atomic_read(&p->wakeup_path))
		device_set_wakeup_path(dev);

	return 0;
}

static SIMPLE_DEV_PM_OPS(intc_irqpin_pm_ops, intc_irqpin_suspend, NULL);

static struct platform_driver intc_irqpin_device_driver = {
	.probe		= intc_irqpin_probe,
	.remove		= intc_irqpin_remove,
	.driver		= {
		.name	= "renesas_intc_irqpin",
		.of_match_table = intc_irqpin_dt_ids,
		.pm	= &intc_irqpin_pm_ops,
	}
};

static int __init intc_irqpin_init(void)
{
	return platform_driver_register(&intc_irqpin_device_driver);
}
postcore_initcall(intc_irqpin_init);

static void __exit intc_irqpin_exit(void)
{
	platform_driver_unregister(&intc_irqpin_device_driver);
}
module_exit(intc_irqpin_exit);

MODULE_AUTHOR("Magnus Damm");
MODULE_DESCRIPTION("Renesas INTC External IRQ Pin Driver");
