
 
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/dmar.h>
#include <linux/hpet.h>
#include <linux/msi.h>
#include <asm/irqdomain.h>
#include <asm/hpet.h>
#include <asm/hw_irq.h>
#include <asm/apic.h>
#include <asm/irq_remapping.h>
#include <asm/xen/hypervisor.h>

struct irq_domain *x86_pci_msi_default_domain __ro_after_init;

static void irq_msi_update_msg(struct irq_data *irqd, struct irq_cfg *cfg)
{
	struct msi_msg msg[2] = { [1] = { }, };

	__irq_msi_compose_msg(cfg, msg, false);
	irq_data_get_irq_chip(irqd)->irq_write_msi_msg(irqd, msg);
}

static int
msi_set_affinity(struct irq_data *irqd, const struct cpumask *mask, bool force)
{
	struct irq_cfg old_cfg, *cfg = irqd_cfg(irqd);
	struct irq_data *parent = irqd->parent_data;
	unsigned int cpu;
	int ret;

	 
	cpu = cpumask_first(irq_data_get_effective_affinity_mask(irqd));
	old_cfg = *cfg;

	 
	ret = parent->chip->irq_set_affinity(parent, mask, force);
	if (ret < 0 || ret == IRQ_SET_MASK_OK_DONE)
		return ret;

	 
	if (!irqd_can_reserve(irqd) ||
	    cfg->vector == old_cfg.vector ||
	    old_cfg.vector == MANAGED_IRQ_SHUTDOWN_VECTOR ||
	    !irqd_is_started(irqd) ||
	    cfg->dest_apicid == old_cfg.dest_apicid) {
		irq_msi_update_msg(irqd, cfg);
		return ret;
	}

	 
	if (WARN_ON_ONCE(cpu != smp_processor_id())) {
		irq_msi_update_msg(irqd, cfg);
		return ret;
	}

	 
	lock_vector_lock();

	 
	if (IS_ERR_OR_NULL(this_cpu_read(vector_irq[cfg->vector])))
		this_cpu_write(vector_irq[cfg->vector], VECTOR_RETRIGGERED);

	 
	old_cfg.vector = cfg->vector;
	irq_msi_update_msg(irqd, &old_cfg);

	 
	irq_msi_update_msg(irqd, cfg);

	 
	unlock_vector_lock();

	 
	if (lapic_vector_set_in_irr(cfg->vector))
		irq_data_get_irq_chip(irqd)->irq_retrigger(irqd);

	return ret;
}

 
bool pci_dev_has_default_msi_parent_domain(struct pci_dev *dev)
{
	struct irq_domain *domain = dev_get_msi_domain(&dev->dev);

	if (!domain)
		domain = dev_get_msi_domain(&dev->bus->dev);
	if (!domain)
		return false;

	return domain == x86_vector_domain;
}

 
static int x86_msi_prepare(struct irq_domain *domain, struct device *dev,
			   int nvec, msi_alloc_info_t *alloc)
{
	struct msi_domain_info *info = domain->host_data;

	init_irq_alloc_info(alloc, NULL);

	switch (info->bus_token) {
	case DOMAIN_BUS_PCI_DEVICE_MSI:
		alloc->type = X86_IRQ_ALLOC_TYPE_PCI_MSI;
		return 0;
	case DOMAIN_BUS_PCI_DEVICE_MSIX:
	case DOMAIN_BUS_PCI_DEVICE_IMS:
		alloc->type = X86_IRQ_ALLOC_TYPE_PCI_MSIX;
		return 0;
	default:
		return -EINVAL;
	}
}

 
static bool x86_init_dev_msi_info(struct device *dev, struct irq_domain *domain,
				  struct irq_domain *real_parent, struct msi_domain_info *info)
{
	const struct msi_parent_ops *pops = real_parent->msi_parent_ops;

	 
	switch (real_parent->bus_token) {
	case DOMAIN_BUS_ANY:
		 
		if (WARN_ON_ONCE(domain != real_parent))
			return false;
		info->chip->irq_set_affinity = msi_set_affinity;
		break;
	case DOMAIN_BUS_DMAR:
	case DOMAIN_BUS_AMDVI:
		break;
	default:
		WARN_ON_ONCE(1);
		return false;
	}

	 
	switch(info->bus_token) {
	case DOMAIN_BUS_PCI_DEVICE_MSI:
	case DOMAIN_BUS_PCI_DEVICE_MSIX:
		break;
	case DOMAIN_BUS_PCI_DEVICE_IMS:
		if (!(pops->supported_flags & MSI_FLAG_PCI_IMS))
			return false;
		break;
	default:
		WARN_ON_ONCE(1);
		return false;
	}

	 
	info->flags			&= pops->supported_flags;
	 
	info->flags			|= X86_VECTOR_MSI_FLAGS_REQUIRED;

	 
	info->ops->msi_prepare		= x86_msi_prepare;

	info->chip->irq_ack		= irq_chip_ack_parent;
	info->chip->irq_retrigger	= irq_chip_retrigger_hierarchy;
	info->chip->flags		|= IRQCHIP_SKIP_SET_WAKE |
					   IRQCHIP_AFFINITY_PRE_STARTUP;

	info->handler			= handle_edge_irq;
	info->handler_name		= "edge";

	return true;
}

static const struct msi_parent_ops x86_vector_msi_parent_ops = {
	.supported_flags	= X86_VECTOR_MSI_FLAGS_SUPPORTED,
	.init_dev_msi_info	= x86_init_dev_msi_info,
};

struct irq_domain * __init native_create_pci_msi_domain(void)
{
	if (apic_is_disabled)
		return NULL;

	x86_vector_domain->flags |= IRQ_DOMAIN_FLAG_MSI_PARENT;
	x86_vector_domain->msi_parent_ops = &x86_vector_msi_parent_ops;
	return x86_vector_domain;
}

void __init x86_create_pci_msi_domain(void)
{
	x86_pci_msi_default_domain = x86_init.irqs.create_pci_msi_domain();
}

 
int pci_msi_prepare(struct irq_domain *domain, struct device *dev, int nvec,
		    msi_alloc_info_t *arg)
{
	init_irq_alloc_info(arg, NULL);

	if (to_pci_dev(dev)->msix_enabled)
		arg->type = X86_IRQ_ALLOC_TYPE_PCI_MSIX;
	else
		arg->type = X86_IRQ_ALLOC_TYPE_PCI_MSI;
	return 0;
}
EXPORT_SYMBOL_GPL(pci_msi_prepare);

#ifdef CONFIG_DMAR_TABLE
 
static void dmar_msi_compose_msg(struct irq_data *data, struct msi_msg *msg)
{
	__irq_msi_compose_msg(irqd_cfg(data), msg, true);
}

static void dmar_msi_write_msg(struct irq_data *data, struct msi_msg *msg)
{
	dmar_msi_write(data->irq, msg);
}

static struct irq_chip dmar_msi_controller = {
	.name			= "DMAR-MSI",
	.irq_unmask		= dmar_msi_unmask,
	.irq_mask		= dmar_msi_mask,
	.irq_ack		= irq_chip_ack_parent,
	.irq_set_affinity	= msi_domain_set_affinity,
	.irq_retrigger		= irq_chip_retrigger_hierarchy,
	.irq_compose_msi_msg	= dmar_msi_compose_msg,
	.irq_write_msi_msg	= dmar_msi_write_msg,
	.flags			= IRQCHIP_SKIP_SET_WAKE |
				  IRQCHIP_AFFINITY_PRE_STARTUP,
};

static int dmar_msi_init(struct irq_domain *domain,
			 struct msi_domain_info *info, unsigned int virq,
			 irq_hw_number_t hwirq, msi_alloc_info_t *arg)
{
	irq_domain_set_info(domain, virq, arg->devid, info->chip, NULL,
			    handle_edge_irq, arg->data, "edge");

	return 0;
}

static struct msi_domain_ops dmar_msi_domain_ops = {
	.msi_init	= dmar_msi_init,
};

static struct msi_domain_info dmar_msi_domain_info = {
	.ops		= &dmar_msi_domain_ops,
	.chip		= &dmar_msi_controller,
	.flags		= MSI_FLAG_USE_DEF_DOM_OPS,
};

static struct irq_domain *dmar_get_irq_domain(void)
{
	static struct irq_domain *dmar_domain;
	static DEFINE_MUTEX(dmar_lock);
	struct fwnode_handle *fn;

	mutex_lock(&dmar_lock);
	if (dmar_domain)
		goto out;

	fn = irq_domain_alloc_named_fwnode("DMAR-MSI");
	if (fn) {
		dmar_domain = msi_create_irq_domain(fn, &dmar_msi_domain_info,
						    x86_vector_domain);
		if (!dmar_domain)
			irq_domain_free_fwnode(fn);
	}
out:
	mutex_unlock(&dmar_lock);
	return dmar_domain;
}

int dmar_alloc_hwirq(int id, int node, void *arg)
{
	struct irq_domain *domain = dmar_get_irq_domain();
	struct irq_alloc_info info;

	if (!domain)
		return -1;

	init_irq_alloc_info(&info, NULL);
	info.type = X86_IRQ_ALLOC_TYPE_DMAR;
	info.devid = id;
	info.hwirq = id;
	info.data = arg;

	return irq_domain_alloc_irqs(domain, 1, node, &info);
}

void dmar_free_hwirq(int irq)
{
	irq_domain_free_irqs(irq, 1);
}
#endif

bool arch_restore_msi_irqs(struct pci_dev *dev)
{
	return xen_initdom_restore_msi(dev);
}
