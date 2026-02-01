 

#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/irqchip.h>

#define OMPIC_CPUBYTES		8
#define OMPIC_CTRL(cpu)		(0x0 + (cpu * OMPIC_CPUBYTES))
#define OMPIC_STAT(cpu)		(0x4 + (cpu * OMPIC_CPUBYTES))

#define OMPIC_CTRL_IRQ_ACK	(1 << 31)
#define OMPIC_CTRL_IRQ_GEN	(1 << 30)
#define OMPIC_CTRL_DST(cpu)	(((cpu) & 0x3fff) << 16)

#define OMPIC_STAT_IRQ_PENDING	(1 << 30)

#define OMPIC_DATA(x)		((x) & 0xffff)

DEFINE_PER_CPU(unsigned long, ops);

static void __iomem *ompic_base;

static inline u32 ompic_readreg(void __iomem *base, loff_t offset)
{
	return ioread32be(base + offset);
}

static void ompic_writereg(void __iomem *base, loff_t offset, u32 data)
{
	iowrite32be(data, base + offset);
}

static void ompic_raise_softirq(const struct cpumask *mask,
				unsigned int ipi_msg)
{
	unsigned int dst_cpu;
	unsigned int src_cpu = smp_processor_id();

	for_each_cpu(dst_cpu, mask) {
		set_bit(ipi_msg, &per_cpu(ops, dst_cpu));

		 

		ompic_writereg(ompic_base, OMPIC_CTRL(src_cpu),
			       OMPIC_CTRL_IRQ_GEN |
			       OMPIC_CTRL_DST(dst_cpu) |
			       OMPIC_DATA(1));
	}
}

static irqreturn_t ompic_ipi_handler(int irq, void *dev_id)
{
	unsigned int cpu = smp_processor_id();
	unsigned long *pending_ops = &per_cpu(ops, cpu);
	unsigned long ops;

	ompic_writereg(ompic_base, OMPIC_CTRL(cpu), OMPIC_CTRL_IRQ_ACK);
	while ((ops = xchg(pending_ops, 0)) != 0) {

		 

		do {
			unsigned long ipi_msg;

			ipi_msg = __ffs(ops);
			ops &= ~(1UL << ipi_msg);

			handle_IPI(ipi_msg);
		} while (ops);
	}

	return IRQ_HANDLED;
}

static int __init ompic_of_init(struct device_node *node,
				struct device_node *parent)
{
	struct resource res;
	int irq;
	int ret;

	 
	if (ompic_base) {
		pr_err("ompic: duplicate ompic's are not supported");
		return -EEXIST;
	}

	if (of_address_to_resource(node, 0, &res)) {
		pr_err("ompic: reg property requires an address and size");
		return -EINVAL;
	}

	if (resource_size(&res) < (num_possible_cpus() * OMPIC_CPUBYTES)) {
		pr_err("ompic: reg size, currently %d must be at least %d",
			resource_size(&res),
			(num_possible_cpus() * OMPIC_CPUBYTES));
		return -EINVAL;
	}

	 
	ompic_base = ioremap(res.start, resource_size(&res));
	if (!ompic_base) {
		pr_err("ompic: unable to map registers");
		return -ENOMEM;
	}

	irq = irq_of_parse_and_map(node, 0);
	if (irq <= 0) {
		pr_err("ompic: unable to parse device irq");
		ret = -EINVAL;
		goto out_unmap;
	}

	ret = request_irq(irq, ompic_ipi_handler, IRQF_PERCPU,
				"ompic_ipi", NULL);
	if (ret)
		goto out_irq_disp;

	set_smp_cross_call(ompic_raise_softirq);

	return 0;

out_irq_disp:
	irq_dispose_mapping(irq);
out_unmap:
	iounmap(ompic_base);
	ompic_base = NULL;
	return ret;
}
IRQCHIP_DECLARE(ompic, "openrisc,ompic", ompic_of_init);
