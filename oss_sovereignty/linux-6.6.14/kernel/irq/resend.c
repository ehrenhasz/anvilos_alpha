
 

#include <linux/irq.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/interrupt.h>

#include "internals.h"

#ifdef CONFIG_HARDIRQS_SW_RESEND

 
static HLIST_HEAD(irq_resend_list);
static DEFINE_RAW_SPINLOCK(irq_resend_lock);

 
static void resend_irqs(struct tasklet_struct *unused)
{
	struct irq_desc *desc;

	raw_spin_lock_irq(&irq_resend_lock);
	while (!hlist_empty(&irq_resend_list)) {
		desc = hlist_entry(irq_resend_list.first, struct irq_desc,
				   resend_node);
		hlist_del_init(&desc->resend_node);
		raw_spin_unlock(&irq_resend_lock);
		desc->handle_irq(desc);
		raw_spin_lock(&irq_resend_lock);
	}
	raw_spin_unlock_irq(&irq_resend_lock);
}

 
static DECLARE_TASKLET(resend_tasklet, resend_irqs);

static int irq_sw_resend(struct irq_desc *desc)
{
	 
	if (handle_enforce_irqctx(&desc->irq_data))
		return -EINVAL;

	 
	if (irq_settings_is_nested_thread(desc)) {
		 
		if (!desc->parent_irq)
			return -EINVAL;

		desc = irq_to_desc(desc->parent_irq);
		if (!desc)
			return -EINVAL;
	}

	 
	raw_spin_lock(&irq_resend_lock);
	if (hlist_unhashed(&desc->resend_node))
		hlist_add_head(&desc->resend_node, &irq_resend_list);
	raw_spin_unlock(&irq_resend_lock);
	tasklet_schedule(&resend_tasklet);
	return 0;
}

void clear_irq_resend(struct irq_desc *desc)
{
	raw_spin_lock(&irq_resend_lock);
	hlist_del_init(&desc->resend_node);
	raw_spin_unlock(&irq_resend_lock);
}

void irq_resend_init(struct irq_desc *desc)
{
	INIT_HLIST_NODE(&desc->resend_node);
}
#else
void clear_irq_resend(struct irq_desc *desc) {}
void irq_resend_init(struct irq_desc *desc) {}

static int irq_sw_resend(struct irq_desc *desc)
{
	return -EINVAL;
}
#endif

static int try_retrigger(struct irq_desc *desc)
{
	if (desc->irq_data.chip->irq_retrigger)
		return desc->irq_data.chip->irq_retrigger(&desc->irq_data);

#ifdef CONFIG_IRQ_DOMAIN_HIERARCHY
	return irq_chip_retrigger_hierarchy(&desc->irq_data);
#else
	return 0;
#endif
}

 
int check_irq_resend(struct irq_desc *desc, bool inject)
{
	int err = 0;

	 
	if (irq_settings_is_level(desc)) {
		desc->istate &= ~IRQS_PENDING;
		return -EINVAL;
	}

	if (desc->istate & IRQS_REPLAY)
		return -EBUSY;

	if (!(desc->istate & IRQS_PENDING) && !inject)
		return 0;

	desc->istate &= ~IRQS_PENDING;

	if (!try_retrigger(desc))
		err = irq_sw_resend(desc);

	 
	if (!err)
		desc->istate |= IRQS_REPLAY;
	return err;
}

#ifdef CONFIG_GENERIC_IRQ_INJECTION
 
int irq_inject_interrupt(unsigned int irq)
{
	struct irq_desc *desc;
	unsigned long flags;
	int err;

	 
	if (!irq_set_irqchip_state(irq, IRQCHIP_STATE_PENDING, true))
		return 0;

	 
	desc = irq_get_desc_buslock(irq, &flags, 0);
	if (!desc)
		return -EINVAL;

	 
	if ((desc->istate & IRQS_NMI) || !irqd_is_activated(&desc->irq_data))
		err = -EINVAL;
	else
		err = check_irq_resend(desc, true);

	irq_put_desc_busunlock(desc, flags);
	return err;
}
EXPORT_SYMBOL_GPL(irq_inject_interrupt);
#endif
