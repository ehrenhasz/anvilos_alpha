
 

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/msi.h>
#include <linux/sched.h>

#include <linux/irqchip/arm-gic-v4.h>

 

static struct irq_domain *gic_domain;
static const struct irq_domain_ops *vpe_domain_ops;
static const struct irq_domain_ops *sgi_domain_ops;

#ifdef CONFIG_ARM64
#include <asm/cpufeature.h>

bool gic_cpuif_has_vsgi(void)
{
	unsigned long fld, reg = read_sanitised_ftr_reg(SYS_ID_AA64PFR0_EL1);

	fld = cpuid_feature_extract_unsigned_field(reg, ID_AA64PFR0_EL1_GIC_SHIFT);

	return fld >= 0x3;
}
#else
bool gic_cpuif_has_vsgi(void)
{
	return false;
}
#endif

static bool has_v4_1(void)
{
	return !!sgi_domain_ops;
}

static bool has_v4_1_sgi(void)
{
	return has_v4_1() && gic_cpuif_has_vsgi();
}

static int its_alloc_vcpu_sgis(struct its_vpe *vpe, int idx)
{
	char *name;
	int sgi_base;

	if (!has_v4_1_sgi())
		return 0;

	name = kasprintf(GFP_KERNEL, "GICv4-sgi-%d", task_pid_nr(current));
	if (!name)
		goto err;

	vpe->fwnode = irq_domain_alloc_named_id_fwnode(name, idx);
	if (!vpe->fwnode)
		goto err;

	kfree(name);
	name = NULL;

	vpe->sgi_domain = irq_domain_create_linear(vpe->fwnode, 16,
						   sgi_domain_ops, vpe);
	if (!vpe->sgi_domain)
		goto err;

	sgi_base = irq_domain_alloc_irqs(vpe->sgi_domain, 16, NUMA_NO_NODE, vpe);
	if (sgi_base <= 0)
		goto err;

	return 0;

err:
	if (vpe->sgi_domain)
		irq_domain_remove(vpe->sgi_domain);
	if (vpe->fwnode)
		irq_domain_free_fwnode(vpe->fwnode);
	kfree(name);
	return -ENOMEM;
}

int its_alloc_vcpu_irqs(struct its_vm *vm)
{
	int vpe_base_irq, i;

	vm->fwnode = irq_domain_alloc_named_id_fwnode("GICv4-vpe",
						      task_pid_nr(current));
	if (!vm->fwnode)
		goto err;

	vm->domain = irq_domain_create_hierarchy(gic_domain, 0, vm->nr_vpes,
						 vm->fwnode, vpe_domain_ops,
						 vm);
	if (!vm->domain)
		goto err;

	for (i = 0; i < vm->nr_vpes; i++) {
		vm->vpes[i]->its_vm = vm;
		vm->vpes[i]->idai = true;
	}

	vpe_base_irq = irq_domain_alloc_irqs(vm->domain, vm->nr_vpes,
					     NUMA_NO_NODE, vm);
	if (vpe_base_irq <= 0)
		goto err;

	for (i = 0; i < vm->nr_vpes; i++) {
		int ret;
		vm->vpes[i]->irq = vpe_base_irq + i;
		ret = its_alloc_vcpu_sgis(vm->vpes[i], i);
		if (ret)
			goto err;
	}

	return 0;

err:
	if (vm->domain)
		irq_domain_remove(vm->domain);
	if (vm->fwnode)
		irq_domain_free_fwnode(vm->fwnode);

	return -ENOMEM;
}

static void its_free_sgi_irqs(struct its_vm *vm)
{
	int i;

	if (!has_v4_1_sgi())
		return;

	for (i = 0; i < vm->nr_vpes; i++) {
		unsigned int irq = irq_find_mapping(vm->vpes[i]->sgi_domain, 0);

		if (WARN_ON(!irq))
			continue;

		irq_domain_free_irqs(irq, 16);
		irq_domain_remove(vm->vpes[i]->sgi_domain);
		irq_domain_free_fwnode(vm->vpes[i]->fwnode);
	}
}

void its_free_vcpu_irqs(struct its_vm *vm)
{
	its_free_sgi_irqs(vm);
	irq_domain_free_irqs(vm->vpes[0]->irq, vm->nr_vpes);
	irq_domain_remove(vm->domain);
	irq_domain_free_fwnode(vm->fwnode);
}

static int its_send_vpe_cmd(struct its_vpe *vpe, struct its_cmd_info *info)
{
	return irq_set_vcpu_affinity(vpe->irq, info);
}

int its_make_vpe_non_resident(struct its_vpe *vpe, bool db)
{
	struct irq_desc *desc = irq_to_desc(vpe->irq);
	struct its_cmd_info info = { };
	int ret;

	WARN_ON(preemptible());

	info.cmd_type = DESCHEDULE_VPE;
	if (has_v4_1()) {
		 
		info.req_db = db;
	} else {
		 
		while (db && irqd_irq_disabled(&desc->irq_data))
			enable_irq(vpe->irq);
	}

	ret = its_send_vpe_cmd(vpe, &info);
	if (!ret)
		vpe->resident = false;

	vpe->ready = false;

	return ret;
}

int its_make_vpe_resident(struct its_vpe *vpe, bool g0en, bool g1en)
{
	struct its_cmd_info info = { };
	int ret;

	WARN_ON(preemptible());

	info.cmd_type = SCHEDULE_VPE;
	if (has_v4_1()) {
		info.g0en = g0en;
		info.g1en = g1en;
	} else {
		 
		disable_irq_nosync(vpe->irq);
	}

	ret = its_send_vpe_cmd(vpe, &info);
	if (!ret)
		vpe->resident = true;

	return ret;
}

int its_commit_vpe(struct its_vpe *vpe)
{
	struct its_cmd_info info = {
		.cmd_type = COMMIT_VPE,
	};
	int ret;

	WARN_ON(preemptible());

	ret = its_send_vpe_cmd(vpe, &info);
	if (!ret)
		vpe->ready = true;

	return ret;
}


int its_invall_vpe(struct its_vpe *vpe)
{
	struct its_cmd_info info = {
		.cmd_type = INVALL_VPE,
	};

	return its_send_vpe_cmd(vpe, &info);
}

int its_map_vlpi(int irq, struct its_vlpi_map *map)
{
	struct its_cmd_info info = {
		.cmd_type = MAP_VLPI,
		{
			.map      = map,
		},
	};
	int ret;

	 
	irq_set_status_flags(irq, IRQ_DISABLE_UNLAZY);

	ret = irq_set_vcpu_affinity(irq, &info);
	if (ret)
		irq_clear_status_flags(irq, IRQ_DISABLE_UNLAZY);

	return ret;
}

int its_get_vlpi(int irq, struct its_vlpi_map *map)
{
	struct its_cmd_info info = {
		.cmd_type = GET_VLPI,
		{
			.map      = map,
		},
	};

	return irq_set_vcpu_affinity(irq, &info);
}

int its_unmap_vlpi(int irq)
{
	irq_clear_status_flags(irq, IRQ_DISABLE_UNLAZY);
	return irq_set_vcpu_affinity(irq, NULL);
}

int its_prop_update_vlpi(int irq, u8 config, bool inv)
{
	struct its_cmd_info info = {
		.cmd_type = inv ? PROP_UPDATE_AND_INV_VLPI : PROP_UPDATE_VLPI,
		{
			.config   = config,
		},
	};

	return irq_set_vcpu_affinity(irq, &info);
}

int its_prop_update_vsgi(int irq, u8 priority, bool group)
{
	struct its_cmd_info info = {
		.cmd_type = PROP_UPDATE_VSGI,
		{
			.priority	= priority,
			.group		= group,
		},
	};

	return irq_set_vcpu_affinity(irq, &info);
}

int its_init_v4(struct irq_domain *domain,
		const struct irq_domain_ops *vpe_ops,
		const struct irq_domain_ops *sgi_ops)
{
	if (domain) {
		pr_info("ITS: Enabling GICv4 support\n");
		gic_domain = domain;
		vpe_domain_ops = vpe_ops;
		sgi_domain_ops = sgi_ops;
		return 0;
	}

	pr_err("ITS: No GICv4 VPE domain allocated\n");
	return -ENODEV;
}
