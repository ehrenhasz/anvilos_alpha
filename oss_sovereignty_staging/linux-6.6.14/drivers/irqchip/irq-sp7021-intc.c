
 
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/io.h>
#include <linux/irqchip.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#define SP_INTC_HWIRQ_MIN	0
#define SP_INTC_HWIRQ_MAX	223

#define SP_INTC_NR_IRQS		(SP_INTC_HWIRQ_MAX - SP_INTC_HWIRQ_MIN + 1)
#define SP_INTC_NR_GROUPS	DIV_ROUND_UP(SP_INTC_NR_IRQS, 32)
#define SP_INTC_REG_SIZE	(SP_INTC_NR_GROUPS * 4)

 
#define REG_INTR_TYPE		(sp_intc.g0)
#define REG_INTR_POLARITY	(REG_INTR_TYPE     + SP_INTC_REG_SIZE)
#define REG_INTR_PRIORITY	(REG_INTR_POLARITY + SP_INTC_REG_SIZE)
#define REG_INTR_MASK		(REG_INTR_PRIORITY + SP_INTC_REG_SIZE)

 
#define REG_INTR_CLEAR		(sp_intc.g1)
#define REG_MASKED_EXT1		(REG_INTR_CLEAR    + SP_INTC_REG_SIZE)
#define REG_MASKED_EXT0		(REG_MASKED_EXT1   + SP_INTC_REG_SIZE)
#define REG_INTR_GROUP		(REG_INTR_CLEAR    + 31 * 4)

#define GROUP_MASK		(BIT(SP_INTC_NR_GROUPS) - 1)
#define GROUP_SHIFT_EXT1	(0)
#define GROUP_SHIFT_EXT0	(8)

 
#define GPIO_INT0_HWIRQ		120
#define GPIO_INT7_HWIRQ		127
#define IS_GPIO_INT(irq)					\
({								\
	u32 i = irq;						\
	(i >= GPIO_INT0_HWIRQ) && (i <= GPIO_INT7_HWIRQ);	\
})

 
enum {
	_IS_EDGE = 0,
	_IS_LOW,
	_IS_ACTIVE
};

#define STATE_BIT(irq, idx)		(((irq) - GPIO_INT0_HWIRQ) * 3 + (idx))
#define ASSIGN_STATE(irq, idx, v)	assign_bit(STATE_BIT(irq, idx), sp_intc.states, v)
#define TEST_STATE(irq, idx)		test_bit(STATE_BIT(irq, idx), sp_intc.states)

static struct sp_intctl {
	 
	void __iomem *g0; 
	void __iomem *g1; 

	struct irq_domain *domain;
	raw_spinlock_t lock;

	 
	DECLARE_BITMAP(states, (GPIO_INT7_HWIRQ - GPIO_INT0_HWIRQ + 1) * 3);
} sp_intc;

static struct irq_chip sp_intc_chip;

static void sp_intc_assign_bit(u32 hwirq, void __iomem *base, bool value)
{
	u32 offset, mask;
	unsigned long flags;
	void __iomem *reg;

	offset = (hwirq / 32) * 4;
	reg = base + offset;

	raw_spin_lock_irqsave(&sp_intc.lock, flags);
	mask = readl_relaxed(reg);
	if (value)
		mask |= BIT(hwirq % 32);
	else
		mask &= ~BIT(hwirq % 32);
	writel_relaxed(mask, reg);
	raw_spin_unlock_irqrestore(&sp_intc.lock, flags);
}

static void sp_intc_ack_irq(struct irq_data *d)
{
	u32 hwirq = d->hwirq;

	if (unlikely(IS_GPIO_INT(hwirq) && TEST_STATE(hwirq, _IS_EDGE))) { 
		sp_intc_assign_bit(hwirq, REG_INTR_POLARITY, !TEST_STATE(hwirq, _IS_LOW));
		ASSIGN_STATE(hwirq, _IS_ACTIVE, true);
	}

	sp_intc_assign_bit(hwirq, REG_INTR_CLEAR, 1);
}

static void sp_intc_mask_irq(struct irq_data *d)
{
	sp_intc_assign_bit(d->hwirq, REG_INTR_MASK, 0);
}

static void sp_intc_unmask_irq(struct irq_data *d)
{
	sp_intc_assign_bit(d->hwirq, REG_INTR_MASK, 1);
}

static int sp_intc_set_type(struct irq_data *d, unsigned int type)
{
	u32 hwirq = d->hwirq;
	bool is_edge = !(type & IRQ_TYPE_LEVEL_MASK);
	bool is_low = (type == IRQ_TYPE_LEVEL_LOW || type == IRQ_TYPE_EDGE_FALLING);

	irq_set_handler_locked(d, is_edge ? handle_edge_irq : handle_level_irq);

	if (unlikely(IS_GPIO_INT(hwirq) && is_edge)) { 
		 
		ASSIGN_STATE(hwirq, _IS_EDGE, is_edge);
		ASSIGN_STATE(hwirq, _IS_LOW, is_low);
		ASSIGN_STATE(hwirq, _IS_ACTIVE, false);
		 
		is_edge = false;
	}

	sp_intc_assign_bit(hwirq, REG_INTR_TYPE, is_edge);
	sp_intc_assign_bit(hwirq, REG_INTR_POLARITY, is_low);

	return 0;
}

static int sp_intc_get_ext_irq(int ext_num)
{
	void __iomem *base = ext_num ? REG_MASKED_EXT1 : REG_MASKED_EXT0;
	u32 shift = ext_num ? GROUP_SHIFT_EXT1 : GROUP_SHIFT_EXT0;
	u32 groups;
	u32 pending_group;
	u32 group;
	u32 pending_irq;

	groups = readl_relaxed(REG_INTR_GROUP);
	pending_group = (groups >> shift) & GROUP_MASK;
	if (!pending_group)
		return -1;

	group = fls(pending_group) - 1;
	pending_irq = readl_relaxed(base + group * 4);
	if (!pending_irq)
		return -1;

	return (group * 32) + fls(pending_irq) - 1;
}

static void sp_intc_handle_ext_cascaded(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	int ext_num = (uintptr_t)irq_desc_get_handler_data(desc);
	int hwirq;

	chained_irq_enter(chip, desc);

	while ((hwirq = sp_intc_get_ext_irq(ext_num)) >= 0) {
		if (unlikely(IS_GPIO_INT(hwirq) && TEST_STATE(hwirq, _IS_ACTIVE))) { 
			ASSIGN_STATE(hwirq, _IS_ACTIVE, false);
			sp_intc_assign_bit(hwirq, REG_INTR_POLARITY, TEST_STATE(hwirq, _IS_LOW));
		} else {
			generic_handle_domain_irq(sp_intc.domain, hwirq);
		}
	}

	chained_irq_exit(chip, desc);
}

static struct irq_chip sp_intc_chip = {
	.name = "sp_intc",
	.irq_ack = sp_intc_ack_irq,
	.irq_mask = sp_intc_mask_irq,
	.irq_unmask = sp_intc_unmask_irq,
	.irq_set_type = sp_intc_set_type,
};

static int sp_intc_irq_domain_map(struct irq_domain *domain,
				  unsigned int irq, irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &sp_intc_chip, handle_level_irq);
	irq_set_chip_data(irq, &sp_intc_chip);
	irq_set_noprobe(irq);

	return 0;
}

static const struct irq_domain_ops sp_intc_dm_ops = {
	.xlate = irq_domain_xlate_twocell,
	.map = sp_intc_irq_domain_map,
};

static int sp_intc_irq_map(struct device_node *node, int i)
{
	unsigned int irq;

	irq = irq_of_parse_and_map(node, i);
	if (!irq)
		return -ENOENT;

	irq_set_chained_handler_and_data(irq, sp_intc_handle_ext_cascaded, (void *)(uintptr_t)i);

	return 0;
}

static int __init sp_intc_init_dt(struct device_node *node, struct device_node *parent)
{
	int i, ret;

	sp_intc.g0 = of_iomap(node, 0);
	if (!sp_intc.g0)
		return -ENXIO;

	sp_intc.g1 = of_iomap(node, 1);
	if (!sp_intc.g1) {
		ret = -ENXIO;
		goto out_unmap0;
	}

	ret = sp_intc_irq_map(node, 0); 
	if (ret)
		goto out_unmap1;

	ret = sp_intc_irq_map(node, 1); 
	if (ret)
		goto out_unmap1;

	 
	for (i = 0; i < SP_INTC_NR_GROUPS; i++) {
		 
		writel_relaxed(0, REG_INTR_MASK + i * 4);
		 
		writel_relaxed(~0, REG_INTR_TYPE + i * 4);
		 
		writel_relaxed(0, REG_INTR_POLARITY + i * 4);
		 
		writel_relaxed(~0, REG_INTR_PRIORITY + i * 4);
		 
		writel_relaxed(~0, REG_INTR_CLEAR + i * 4);
	}

	sp_intc.domain = irq_domain_add_linear(node, SP_INTC_NR_IRQS,
					       &sp_intc_dm_ops, &sp_intc);
	if (!sp_intc.domain) {
		ret = -ENOMEM;
		goto out_unmap1;
	}

	raw_spin_lock_init(&sp_intc.lock);

	return 0;

out_unmap1:
	iounmap(sp_intc.g1);
out_unmap0:
	iounmap(sp_intc.g0);

	return ret;
}

IRQCHIP_DECLARE(sp_intc, "sunplus,sp7021-intc", sp_intc_init_dt);
