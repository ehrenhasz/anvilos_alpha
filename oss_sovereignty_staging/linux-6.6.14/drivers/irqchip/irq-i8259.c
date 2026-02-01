 
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/irqchip.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/irq.h>

#include <asm/i8259.h>
#include <asm/io.h>

 

static int i8259A_auto_eoi = -1;
DEFINE_RAW_SPINLOCK(i8259A_lock);
static void disable_8259A_irq(struct irq_data *d);
static void enable_8259A_irq(struct irq_data *d);
static void mask_and_ack_8259A(struct irq_data *d);
static void init_8259A(int auto_eoi);
static int (*i8259_poll)(void) = i8259_irq;

static struct irq_chip i8259A_chip = {
	.name			= "XT-PIC",
	.irq_mask		= disable_8259A_irq,
	.irq_disable		= disable_8259A_irq,
	.irq_unmask		= enable_8259A_irq,
	.irq_mask_ack		= mask_and_ack_8259A,
};

 

void i8259_set_poll(int (*poll)(void))
{
	i8259_poll = poll;
}

 
static unsigned int cached_irq_mask = 0xffff;

#define cached_master_mask	(cached_irq_mask)
#define cached_slave_mask	(cached_irq_mask >> 8)

static void disable_8259A_irq(struct irq_data *d)
{
	unsigned int mask, irq = d->irq - I8259A_IRQ_BASE;
	unsigned long flags;

	mask = 1 << irq;
	raw_spin_lock_irqsave(&i8259A_lock, flags);
	cached_irq_mask |= mask;
	if (irq & 8)
		outb(cached_slave_mask, PIC_SLAVE_IMR);
	else
		outb(cached_master_mask, PIC_MASTER_IMR);
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}

static void enable_8259A_irq(struct irq_data *d)
{
	unsigned int mask, irq = d->irq - I8259A_IRQ_BASE;
	unsigned long flags;

	mask = ~(1 << irq);
	raw_spin_lock_irqsave(&i8259A_lock, flags);
	cached_irq_mask &= mask;
	if (irq & 8)
		outb(cached_slave_mask, PIC_SLAVE_IMR);
	else
		outb(cached_master_mask, PIC_MASTER_IMR);
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}

void make_8259A_irq(unsigned int irq)
{
	disable_irq_nosync(irq);
	irq_set_chip_and_handler(irq, &i8259A_chip, handle_level_irq);
	enable_irq(irq);
}

 
static inline int i8259A_irq_real(unsigned int irq)
{
	int value;
	int irqmask = 1 << irq;

	if (irq < 8) {
		outb(0x0B, PIC_MASTER_CMD);	 
		value = inb(PIC_MASTER_CMD) & irqmask;
		outb(0x0A, PIC_MASTER_CMD);	 
		return value;
	}
	outb(0x0B, PIC_SLAVE_CMD);	 
	value = inb(PIC_SLAVE_CMD) & (irqmask >> 8);
	outb(0x0A, PIC_SLAVE_CMD);	 
	return value;
}

 
static void mask_and_ack_8259A(struct irq_data *d)
{
	unsigned int irqmask, irq = d->irq - I8259A_IRQ_BASE;
	unsigned long flags;

	irqmask = 1 << irq;
	raw_spin_lock_irqsave(&i8259A_lock, flags);
	 
	if (cached_irq_mask & irqmask)
		goto spurious_8259A_irq;
	cached_irq_mask |= irqmask;

handle_real_irq:
	if (irq & 8) {
		inb(PIC_SLAVE_IMR);	 
		outb(cached_slave_mask, PIC_SLAVE_IMR);
		outb(0x60+(irq&7), PIC_SLAVE_CMD); 
		outb(0x60+PIC_CASCADE_IR, PIC_MASTER_CMD);  
	} else {
		inb(PIC_MASTER_IMR);	 
		outb(cached_master_mask, PIC_MASTER_IMR);
		outb(0x60+irq, PIC_MASTER_CMD);  
	}
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
	return;

spurious_8259A_irq:
	 
	if (i8259A_irq_real(irq))
		 
		goto handle_real_irq;

	{
		static int spurious_irq_mask;
		 
		if (!(spurious_irq_mask & irqmask)) {
			printk(KERN_DEBUG "spurious 8259A interrupt: IRQ%d.\n", irq);
			spurious_irq_mask |= irqmask;
		}
		atomic_inc(&irq_err_count);
		 
		goto handle_real_irq;
	}
}

static void i8259A_resume(void)
{
	if (i8259A_auto_eoi >= 0)
		init_8259A(i8259A_auto_eoi);
}

static void i8259A_shutdown(void)
{
	 
	if (i8259A_auto_eoi >= 0) {
		outb(0xff, PIC_MASTER_IMR);	 
		outb(0xff, PIC_SLAVE_IMR);	 
	}
}

static struct syscore_ops i8259_syscore_ops = {
	.resume = i8259A_resume,
	.shutdown = i8259A_shutdown,
};

static void init_8259A(int auto_eoi)
{
	unsigned long flags;

	i8259A_auto_eoi = auto_eoi;

	raw_spin_lock_irqsave(&i8259A_lock, flags);

	outb(0xff, PIC_MASTER_IMR);	 
	outb(0xff, PIC_SLAVE_IMR);	 

	 
	outb_p(0x11, PIC_MASTER_CMD);	 
	outb_p(I8259A_IRQ_BASE + 0, PIC_MASTER_IMR);	 
	outb_p(1U << PIC_CASCADE_IR, PIC_MASTER_IMR);	 
	if (auto_eoi)	 
		outb_p(MASTER_ICW4_DEFAULT | PIC_ICW4_AEOI, PIC_MASTER_IMR);
	else		 
		outb_p(MASTER_ICW4_DEFAULT, PIC_MASTER_IMR);

	outb_p(0x11, PIC_SLAVE_CMD);	 
	outb_p(I8259A_IRQ_BASE + 8, PIC_SLAVE_IMR);	 
	outb_p(PIC_CASCADE_IR, PIC_SLAVE_IMR);	 
	outb_p(SLAVE_ICW4_DEFAULT, PIC_SLAVE_IMR);  
	if (auto_eoi)
		 
		i8259A_chip.irq_mask_ack = disable_8259A_irq;
	else
		i8259A_chip.irq_mask_ack = mask_and_ack_8259A;

	udelay(100);		 

	outb(cached_master_mask, PIC_MASTER_IMR);  
	outb(cached_slave_mask, PIC_SLAVE_IMR);	   

	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}

static struct resource pic1_io_resource = {
	.name = "pic1",
	.start = PIC_MASTER_CMD,
	.end = PIC_MASTER_IMR,
	.flags = IORESOURCE_IO | IORESOURCE_BUSY
};

static struct resource pic2_io_resource = {
	.name = "pic2",
	.start = PIC_SLAVE_CMD,
	.end = PIC_SLAVE_IMR,
	.flags = IORESOURCE_IO | IORESOURCE_BUSY
};

static int i8259A_irq_domain_map(struct irq_domain *d, unsigned int virq,
				 irq_hw_number_t hw)
{
	irq_set_chip_and_handler(virq, &i8259A_chip, handle_level_irq);
	irq_set_probe(virq);
	return 0;
}

static const struct irq_domain_ops i8259A_ops = {
	.map = i8259A_irq_domain_map,
	.xlate = irq_domain_xlate_onecell,
};

 
struct irq_domain * __init __init_i8259_irqs(struct device_node *node)
{
	 
	int irq = I8259A_IRQ_BASE + PIC_CASCADE_IR;
	struct irq_domain *domain;

	insert_resource(&ioport_resource, &pic1_io_resource);
	insert_resource(&ioport_resource, &pic2_io_resource);

	init_8259A(0);

	domain = irq_domain_add_legacy(node, 16, I8259A_IRQ_BASE, 0,
				       &i8259A_ops, NULL);
	if (!domain)
		panic("Failed to add i8259 IRQ domain");

	if (request_irq(irq, no_action, IRQF_NO_THREAD, "cascade", NULL))
		pr_err("Failed to register cascade interrupt\n");
	register_syscore_ops(&i8259_syscore_ops);
	return domain;
}

void __init init_i8259_irqs(void)
{
	__init_i8259_irqs(NULL);
}

static void i8259_irq_dispatch(struct irq_desc *desc)
{
	struct irq_domain *domain = irq_desc_get_handler_data(desc);
	int hwirq = i8259_poll();

	if (hwirq < 0)
		return;

	generic_handle_domain_irq(domain, hwirq);
}

static int __init i8259_of_init(struct device_node *node, struct device_node *parent)
{
	struct irq_domain *domain;
	unsigned int parent_irq;

	domain = __init_i8259_irqs(node);

	parent_irq = irq_of_parse_and_map(node, 0);
	if (!parent_irq) {
		pr_err("Failed to map i8259 parent IRQ\n");
		irq_domain_remove(domain);
		return -ENODEV;
	}

	irq_set_chained_handler_and_data(parent_irq, i8259_irq_dispatch,
					 domain);
	return 0;
}
IRQCHIP_DECLARE(i8259, "intel,i8259", i8259_of_init);
