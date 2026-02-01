


#include <linux/irqchip.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/printk.h>

#include <asm/exception.h>

#define AIC_SCR(x)	((x)*4)	 
#define AIC_GEN		0x84	 
#define AIC_GRSR	0x88	 
#define AIC_IRSR	0x100	 
#define AIC_IASR	0x104	 
#define AIC_ISR		0x108	 
#define AIC_IPER	0x10c	 
#define AIC_ISNR	0x110	 
#define AIC_IMR		0x114	 
#define AIC_OISR	0x118	 
#define AIC_MECR	0x120	 
#define AIC_MDCR	0x124	 
#define AIC_SSCR	0x128	 
#define AIC_SCCR	0x12c	 
#define AIC_EOSCR	0x130	 

#define AIC_SCR_SRCTYPE_LOW_LEVEL	(0 << 6)
#define AIC_SCR_SRCTYPE_HIGH_LEVEL	(1 << 6)
#define AIC_SCR_SRCTYPE_NEG_EDGE	(2 << 6)
#define AIC_SCR_SRCTYPE_POS_EDGE	(3 << 6)
#define AIC_SCR_PRIORITY(x)		(x)
#define AIC_SCR_PRIORITY_MASK		0x7

#define AIC_NUM_IRQS		32

struct wpcm450_aic {
	void __iomem *regs;
	struct irq_domain *domain;
};

static struct wpcm450_aic *aic;

static void wpcm450_aic_init_hw(void)
{
	int i;

	 
	writel(0xffffffff, aic->regs + AIC_MDCR);

	 
	readl(aic->regs + AIC_IPER);
	writel(0, aic->regs + AIC_EOSCR);

	 
	for (i = 0; i < AIC_NUM_IRQS; i++)
		writel(AIC_SCR_SRCTYPE_HIGH_LEVEL | AIC_SCR_PRIORITY(7),
		       aic->regs + AIC_SCR(i));
}

static void __exception_irq_entry wpcm450_aic_handle_irq(struct pt_regs *regs)
{
	int hwirq;

	 
	 
	hwirq = readl(aic->regs + AIC_IPER) / 4;

	generic_handle_domain_irq(aic->domain, hwirq);
}

static void wpcm450_aic_eoi(struct irq_data *d)
{
	 
	writel(0, aic->regs + AIC_EOSCR);
}

static void wpcm450_aic_mask(struct irq_data *d)
{
	unsigned int mask = BIT(d->hwirq);

	 
	writel(mask, aic->regs + AIC_MDCR);
}

static void wpcm450_aic_unmask(struct irq_data *d)
{
	unsigned int mask = BIT(d->hwirq);

	 
	writel(mask, aic->regs + AIC_MECR);
}

static int wpcm450_aic_set_type(struct irq_data *d, unsigned int flow_type)
{
	 
	if ((flow_type & IRQ_TYPE_SENSE_MASK) != IRQ_TYPE_LEVEL_HIGH)
		return -EINVAL;

	return 0;
}

static struct irq_chip wpcm450_aic_chip = {
	.name = "wpcm450-aic",
	.irq_eoi = wpcm450_aic_eoi,
	.irq_mask = wpcm450_aic_mask,
	.irq_unmask = wpcm450_aic_unmask,
	.irq_set_type = wpcm450_aic_set_type,
};

static int wpcm450_aic_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hwirq)
{
	if (hwirq >= AIC_NUM_IRQS)
		return -EPERM;

	irq_set_chip_and_handler(irq, &wpcm450_aic_chip, handle_fasteoi_irq);
	irq_set_chip_data(irq, aic);
	irq_set_probe(irq);

	return 0;
}

static const struct irq_domain_ops wpcm450_aic_ops = {
	.map = wpcm450_aic_map,
	.xlate = irq_domain_xlate_twocell,
};

static int __init wpcm450_aic_of_init(struct device_node *node,
				      struct device_node *parent)
{
	if (parent)
		return -EINVAL;

	aic = kzalloc(sizeof(*aic), GFP_KERNEL);
	if (!aic)
		return -ENOMEM;

	aic->regs = of_iomap(node, 0);
	if (!aic->regs) {
		pr_err("Failed to map WPCM450 AIC registers\n");
		kfree(aic);
		return -ENOMEM;
	}

	wpcm450_aic_init_hw();

	set_handle_irq(wpcm450_aic_handle_irq);

	aic->domain = irq_domain_add_linear(node, AIC_NUM_IRQS, &wpcm450_aic_ops, aic);

	return 0;
}

IRQCHIP_DECLARE(wpcm450_aic, "nuvoton,wpcm450-aic", wpcm450_aic_of_init);
