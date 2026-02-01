
 

#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

 

struct or1k_pic_dev {
	struct irq_chip chip;
	irq_flow_handler_t handle;
	unsigned long flags;
};

 

static void or1k_pic_mask(struct irq_data *data)
{
	mtspr(SPR_PICMR, mfspr(SPR_PICMR) & ~(1UL << data->hwirq));
}

static void or1k_pic_unmask(struct irq_data *data)
{
	mtspr(SPR_PICMR, mfspr(SPR_PICMR) | (1UL << data->hwirq));
}

static void or1k_pic_ack(struct irq_data *data)
{
	mtspr(SPR_PICSR, (1UL << data->hwirq));
}

static void or1k_pic_mask_ack(struct irq_data *data)
{
	mtspr(SPR_PICMR, mfspr(SPR_PICMR) & ~(1UL << data->hwirq));
	mtspr(SPR_PICSR, (1UL << data->hwirq));
}

 
static void or1k_pic_or1200_ack(struct irq_data *data)
{
	mtspr(SPR_PICSR, mfspr(SPR_PICSR) & ~(1UL << data->hwirq));
}

static void or1k_pic_or1200_mask_ack(struct irq_data *data)
{
	mtspr(SPR_PICMR, mfspr(SPR_PICMR) & ~(1UL << data->hwirq));
	mtspr(SPR_PICSR, mfspr(SPR_PICSR) & ~(1UL << data->hwirq));
}

static struct or1k_pic_dev or1k_pic_level = {
	.chip = {
		.name = "or1k-PIC-level",
		.irq_unmask = or1k_pic_unmask,
		.irq_mask = or1k_pic_mask,
	},
	.handle = handle_level_irq,
	.flags = IRQ_LEVEL | IRQ_NOPROBE,
};

static struct or1k_pic_dev or1k_pic_edge = {
	.chip = {
		.name = "or1k-PIC-edge",
		.irq_unmask = or1k_pic_unmask,
		.irq_mask = or1k_pic_mask,
		.irq_ack = or1k_pic_ack,
		.irq_mask_ack = or1k_pic_mask_ack,
	},
	.handle = handle_edge_irq,
	.flags = IRQ_LEVEL | IRQ_NOPROBE,
};

static struct or1k_pic_dev or1k_pic_or1200 = {
	.chip = {
		.name = "or1200-PIC",
		.irq_unmask = or1k_pic_unmask,
		.irq_mask = or1k_pic_mask,
		.irq_ack = or1k_pic_or1200_ack,
		.irq_mask_ack = or1k_pic_or1200_mask_ack,
	},
	.handle = handle_level_irq,
	.flags = IRQ_LEVEL | IRQ_NOPROBE,
};

static struct irq_domain *root_domain;

static inline int pic_get_irq(int first)
{
	int hwirq;

	hwirq = ffs(mfspr(SPR_PICSR) >> first);
	if (!hwirq)
		return NO_IRQ;
	else
		hwirq = hwirq + first - 1;

	return hwirq;
}

static void or1k_pic_handle_irq(struct pt_regs *regs)
{
	int irq = -1;

	while ((irq = pic_get_irq(irq + 1)) != NO_IRQ)
		generic_handle_domain_irq(root_domain, irq);
}

static int or1k_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hw)
{
	struct or1k_pic_dev *pic = d->host_data;

	irq_set_chip_and_handler(irq, &pic->chip, pic->handle);
	irq_set_status_flags(irq, pic->flags);

	return 0;
}

static const struct irq_domain_ops or1k_irq_domain_ops = {
	.xlate = irq_domain_xlate_onecell,
	.map = or1k_map,
};

 
static int __init or1k_pic_init(struct device_node *node,
				 struct or1k_pic_dev *pic)
{
	 
	mtspr(SPR_PICMR, (0UL));

	root_domain = irq_domain_add_linear(node, 32, &or1k_irq_domain_ops,
					    pic);

	set_handle_irq(or1k_pic_handle_irq);

	return 0;
}

static int __init or1k_pic_or1200_init(struct device_node *node,
				       struct device_node *parent)
{
	return or1k_pic_init(node, &or1k_pic_or1200);
}
IRQCHIP_DECLARE(or1k_pic_or1200, "opencores,or1200-pic", or1k_pic_or1200_init);
IRQCHIP_DECLARE(or1k_pic, "opencores,or1k-pic", or1k_pic_or1200_init);

static int __init or1k_pic_level_init(struct device_node *node,
				      struct device_node *parent)
{
	return or1k_pic_init(node, &or1k_pic_level);
}
IRQCHIP_DECLARE(or1k_pic_level, "opencores,or1k-pic-level",
		or1k_pic_level_init);

static int __init or1k_pic_edge_init(struct device_node *node,
				     struct device_node *parent)
{
	return or1k_pic_init(node, &or1k_pic_edge);
}
IRQCHIP_DECLARE(or1k_pic_edge, "opencores,or1k-pic-edge", or1k_pic_edge_init);
