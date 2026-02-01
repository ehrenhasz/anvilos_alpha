
 
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/of_pci.h>

#define MSI_IR0			0x000000
#define MSI_INT0		0x800000
#define IDX_PER_GROUP		8
#define IRQS_PER_IDX		16
#define NR_HW_IRQS		16
#define NR_MSI_VEC		(IDX_PER_GROUP * IRQS_PER_IDX * NR_HW_IRQS)

struct xgene_msi_group {
	struct xgene_msi	*msi;
	int			gic_irq;
	u32			msi_grp;
};

struct xgene_msi {
	struct device_node	*node;
	struct irq_domain	*inner_domain;
	struct irq_domain	*msi_domain;
	u64			msi_addr;
	void __iomem		*msi_regs;
	unsigned long		*bitmap;
	struct mutex		bitmap_lock;
	struct xgene_msi_group	*msi_groups;
	int			num_cpus;
};

 
static struct xgene_msi xgene_msi_ctrl;

static struct irq_chip xgene_msi_top_irq_chip = {
	.name		= "X-Gene1 MSI",
	.irq_enable	= pci_msi_unmask_irq,
	.irq_disable	= pci_msi_mask_irq,
	.irq_mask	= pci_msi_mask_irq,
	.irq_unmask	= pci_msi_unmask_irq,
};

static struct  msi_domain_info xgene_msi_domain_info = {
	.flags	= (MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS |
		  MSI_FLAG_PCI_MSIX),
	.chip	= &xgene_msi_top_irq_chip,
};

 

 
static u32 xgene_msi_ir_read(struct xgene_msi *msi,
				    u32 msi_grp, u32 msir_idx)
{
	return readl_relaxed(msi->msi_regs + MSI_IR0 +
			      (msi_grp << 19) + (msir_idx << 16));
}

 
static u32 xgene_msi_int_read(struct xgene_msi *msi, u32 msi_grp)
{
	return readl_relaxed(msi->msi_regs + MSI_INT0 + (msi_grp << 16));
}

 
static u32 hwirq_to_reg_set(unsigned long hwirq)
{
	return (hwirq / (NR_HW_IRQS * IRQS_PER_IDX));
}

static u32 hwirq_to_group(unsigned long hwirq)
{
	return (hwirq % NR_HW_IRQS);
}

static u32 hwirq_to_msi_data(unsigned long hwirq)
{
	return ((hwirq / NR_HW_IRQS) % IRQS_PER_IDX);
}

static void xgene_compose_msi_msg(struct irq_data *data, struct msi_msg *msg)
{
	struct xgene_msi *msi = irq_data_get_irq_chip_data(data);
	u32 reg_set = hwirq_to_reg_set(data->hwirq);
	u32 group = hwirq_to_group(data->hwirq);
	u64 target_addr = msi->msi_addr + (((8 * group) + reg_set) << 16);

	msg->address_hi = upper_32_bits(target_addr);
	msg->address_lo = lower_32_bits(target_addr);
	msg->data = hwirq_to_msi_data(data->hwirq);
}

 
static int hwirq_to_cpu(unsigned long hwirq)
{
	return (hwirq % xgene_msi_ctrl.num_cpus);
}

static unsigned long hwirq_to_canonical_hwirq(unsigned long hwirq)
{
	return (hwirq - hwirq_to_cpu(hwirq));
}

static int xgene_msi_set_affinity(struct irq_data *irqdata,
				  const struct cpumask *mask, bool force)
{
	int target_cpu = cpumask_first(mask);
	int curr_cpu;

	curr_cpu = hwirq_to_cpu(irqdata->hwirq);
	if (curr_cpu == target_cpu)
		return IRQ_SET_MASK_OK_DONE;

	 
	irqdata->hwirq = hwirq_to_canonical_hwirq(irqdata->hwirq) + target_cpu;

	return IRQ_SET_MASK_OK;
}

static struct irq_chip xgene_msi_bottom_irq_chip = {
	.name			= "MSI",
	.irq_set_affinity       = xgene_msi_set_affinity,
	.irq_compose_msi_msg	= xgene_compose_msi_msg,
};

static int xgene_irq_domain_alloc(struct irq_domain *domain, unsigned int virq,
				  unsigned int nr_irqs, void *args)
{
	struct xgene_msi *msi = domain->host_data;
	int msi_irq;

	mutex_lock(&msi->bitmap_lock);

	msi_irq = bitmap_find_next_zero_area(msi->bitmap, NR_MSI_VEC, 0,
					     msi->num_cpus, 0);
	if (msi_irq < NR_MSI_VEC)
		bitmap_set(msi->bitmap, msi_irq, msi->num_cpus);
	else
		msi_irq = -ENOSPC;

	mutex_unlock(&msi->bitmap_lock);

	if (msi_irq < 0)
		return msi_irq;

	irq_domain_set_info(domain, virq, msi_irq,
			    &xgene_msi_bottom_irq_chip, domain->host_data,
			    handle_simple_irq, NULL, NULL);

	return 0;
}

static void xgene_irq_domain_free(struct irq_domain *domain,
				  unsigned int virq, unsigned int nr_irqs)
{
	struct irq_data *d = irq_domain_get_irq_data(domain, virq);
	struct xgene_msi *msi = irq_data_get_irq_chip_data(d);
	u32 hwirq;

	mutex_lock(&msi->bitmap_lock);

	hwirq = hwirq_to_canonical_hwirq(d->hwirq);
	bitmap_clear(msi->bitmap, hwirq, msi->num_cpus);

	mutex_unlock(&msi->bitmap_lock);

	irq_domain_free_irqs_parent(domain, virq, nr_irqs);
}

static const struct irq_domain_ops msi_domain_ops = {
	.alloc  = xgene_irq_domain_alloc,
	.free   = xgene_irq_domain_free,
};

static int xgene_allocate_domains(struct xgene_msi *msi)
{
	msi->inner_domain = irq_domain_add_linear(NULL, NR_MSI_VEC,
						  &msi_domain_ops, msi);
	if (!msi->inner_domain)
		return -ENOMEM;

	msi->msi_domain = pci_msi_create_irq_domain(of_node_to_fwnode(msi->node),
						    &xgene_msi_domain_info,
						    msi->inner_domain);

	if (!msi->msi_domain) {
		irq_domain_remove(msi->inner_domain);
		return -ENOMEM;
	}

	return 0;
}

static void xgene_free_domains(struct xgene_msi *msi)
{
	if (msi->msi_domain)
		irq_domain_remove(msi->msi_domain);
	if (msi->inner_domain)
		irq_domain_remove(msi->inner_domain);
}

static int xgene_msi_init_allocator(struct xgene_msi *xgene_msi)
{
	xgene_msi->bitmap = bitmap_zalloc(NR_MSI_VEC, GFP_KERNEL);
	if (!xgene_msi->bitmap)
		return -ENOMEM;

	mutex_init(&xgene_msi->bitmap_lock);

	xgene_msi->msi_groups = kcalloc(NR_HW_IRQS,
					sizeof(struct xgene_msi_group),
					GFP_KERNEL);
	if (!xgene_msi->msi_groups)
		return -ENOMEM;

	return 0;
}

static void xgene_msi_isr(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct xgene_msi_group *msi_groups;
	struct xgene_msi *xgene_msi;
	int msir_index, msir_val, hw_irq, ret;
	u32 intr_index, grp_select, msi_grp;

	chained_irq_enter(chip, desc);

	msi_groups = irq_desc_get_handler_data(desc);
	xgene_msi = msi_groups->msi;
	msi_grp = msi_groups->msi_grp;

	 
	grp_select = xgene_msi_int_read(xgene_msi, msi_grp);
	while (grp_select) {
		msir_index = ffs(grp_select) - 1;
		 
		msir_val = xgene_msi_ir_read(xgene_msi, msi_grp, msir_index);
		while (msir_val) {
			intr_index = ffs(msir_val) - 1;
			 
			hw_irq = (((msir_index * IRQS_PER_IDX) + intr_index) *
				 NR_HW_IRQS) + msi_grp;
			 
			hw_irq = hwirq_to_canonical_hwirq(hw_irq);
			ret = generic_handle_domain_irq(xgene_msi->inner_domain, hw_irq);
			WARN_ON_ONCE(ret);
			msir_val &= ~(1 << intr_index);
		}
		grp_select &= ~(1 << msir_index);

		if (!grp_select) {
			 
			grp_select = xgene_msi_int_read(xgene_msi, msi_grp);
		}
	}

	chained_irq_exit(chip, desc);
}

static enum cpuhp_state pci_xgene_online;

static void xgene_msi_remove(struct platform_device *pdev)
{
	struct xgene_msi *msi = platform_get_drvdata(pdev);

	if (pci_xgene_online)
		cpuhp_remove_state(pci_xgene_online);
	cpuhp_remove_state(CPUHP_PCI_XGENE_DEAD);

	kfree(msi->msi_groups);

	bitmap_free(msi->bitmap);
	msi->bitmap = NULL;

	xgene_free_domains(msi);
}

static int xgene_msi_hwirq_alloc(unsigned int cpu)
{
	struct xgene_msi *msi = &xgene_msi_ctrl;
	struct xgene_msi_group *msi_group;
	cpumask_var_t mask;
	int i;
	int err;

	for (i = cpu; i < NR_HW_IRQS; i += msi->num_cpus) {
		msi_group = &msi->msi_groups[i];
		if (!msi_group->gic_irq)
			continue;

		irq_set_chained_handler_and_data(msi_group->gic_irq,
			xgene_msi_isr, msi_group);

		 
		if (alloc_cpumask_var(&mask, GFP_KERNEL)) {
			cpumask_clear(mask);
			cpumask_set_cpu(cpu, mask);
			err = irq_set_affinity(msi_group->gic_irq, mask);
			if (err)
				pr_err("failed to set affinity for GIC IRQ");
			free_cpumask_var(mask);
		} else {
			pr_err("failed to alloc CPU mask for affinity\n");
			err = -EINVAL;
		}

		if (err) {
			irq_set_chained_handler_and_data(msi_group->gic_irq,
							 NULL, NULL);
			return err;
		}
	}

	return 0;
}

static int xgene_msi_hwirq_free(unsigned int cpu)
{
	struct xgene_msi *msi = &xgene_msi_ctrl;
	struct xgene_msi_group *msi_group;
	int i;

	for (i = cpu; i < NR_HW_IRQS; i += msi->num_cpus) {
		msi_group = &msi->msi_groups[i];
		if (!msi_group->gic_irq)
			continue;

		irq_set_chained_handler_and_data(msi_group->gic_irq, NULL,
						 NULL);
	}
	return 0;
}

static const struct of_device_id xgene_msi_match_table[] = {
	{.compatible = "apm,xgene1-msi"},
	{},
};

static int xgene_msi_probe(struct platform_device *pdev)
{
	struct resource *res;
	int rc, irq_index;
	struct xgene_msi *xgene_msi;
	int virt_msir;
	u32 msi_val, msi_idx;

	xgene_msi = &xgene_msi_ctrl;

	platform_set_drvdata(pdev, xgene_msi);

	xgene_msi->msi_regs = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(xgene_msi->msi_regs)) {
		rc = PTR_ERR(xgene_msi->msi_regs);
		goto error;
	}
	xgene_msi->msi_addr = res->start;
	xgene_msi->node = pdev->dev.of_node;
	xgene_msi->num_cpus = num_possible_cpus();

	rc = xgene_msi_init_allocator(xgene_msi);
	if (rc) {
		dev_err(&pdev->dev, "Error allocating MSI bitmap\n");
		goto error;
	}

	rc = xgene_allocate_domains(xgene_msi);
	if (rc) {
		dev_err(&pdev->dev, "Failed to allocate MSI domain\n");
		goto error;
	}

	for (irq_index = 0; irq_index < NR_HW_IRQS; irq_index++) {
		virt_msir = platform_get_irq(pdev, irq_index);
		if (virt_msir < 0) {
			rc = virt_msir;
			goto error;
		}
		xgene_msi->msi_groups[irq_index].gic_irq = virt_msir;
		xgene_msi->msi_groups[irq_index].msi_grp = irq_index;
		xgene_msi->msi_groups[irq_index].msi = xgene_msi;
	}

	 
	for (irq_index = 0; irq_index < NR_HW_IRQS; irq_index++) {
		for (msi_idx = 0; msi_idx < IDX_PER_GROUP; msi_idx++)
			xgene_msi_ir_read(xgene_msi, irq_index, msi_idx);

		 
		msi_val = xgene_msi_int_read(xgene_msi, irq_index);
		if (msi_val) {
			dev_err(&pdev->dev, "Failed to clear spurious IRQ\n");
			rc = -EINVAL;
			goto error;
		}
	}

	rc = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "pci/xgene:online",
			       xgene_msi_hwirq_alloc, NULL);
	if (rc < 0)
		goto err_cpuhp;
	pci_xgene_online = rc;
	rc = cpuhp_setup_state(CPUHP_PCI_XGENE_DEAD, "pci/xgene:dead", NULL,
			       xgene_msi_hwirq_free);
	if (rc)
		goto err_cpuhp;

	dev_info(&pdev->dev, "APM X-Gene PCIe MSI driver loaded\n");

	return 0;

err_cpuhp:
	dev_err(&pdev->dev, "failed to add CPU MSI notifier\n");
error:
	xgene_msi_remove(pdev);
	return rc;
}

static struct platform_driver xgene_msi_driver = {
	.driver = {
		.name = "xgene-msi",
		.of_match_table = xgene_msi_match_table,
	},
	.probe = xgene_msi_probe,
	.remove_new = xgene_msi_remove,
};

static int __init xgene_pcie_msi_init(void)
{
	return platform_driver_register(&xgene_msi_driver);
}
subsys_initcall(xgene_pcie_msi_init);
