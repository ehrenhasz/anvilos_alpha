 

#include "bman_priv.h"

static struct bman_portal *affine_bportals[NR_CPUS];
static struct cpumask portal_cpus;
static int __bman_portals_probed;
 
static DEFINE_SPINLOCK(bman_lock);

static struct bman_portal *init_pcfg(struct bm_portal_config *pcfg)
{
	struct bman_portal *p = bman_create_affine_portal(pcfg);

	if (!p) {
		dev_crit(pcfg->dev, "%s: Portal failure on cpu %d\n",
			 __func__, pcfg->cpu);
		return NULL;
	}

	bman_p_irqsource_add(p, BM_PIRQ_RCRI);
	affine_bportals[pcfg->cpu] = p;

	dev_info(pcfg->dev, "Portal initialised, cpu %d\n", pcfg->cpu);

	return p;
}

static int bman_offline_cpu(unsigned int cpu)
{
	struct bman_portal *p = affine_bportals[cpu];
	const struct bm_portal_config *pcfg;

	if (!p)
		return 0;

	pcfg = bman_get_bm_portal_config(p);
	if (!pcfg)
		return 0;

	 
	cpu = cpumask_any_but(cpu_online_mask, cpu);
	irq_set_affinity(pcfg->irq, cpumask_of(cpu));
	return 0;
}

static int bman_online_cpu(unsigned int cpu)
{
	struct bman_portal *p = affine_bportals[cpu];
	const struct bm_portal_config *pcfg;

	if (!p)
		return 0;

	pcfg = bman_get_bm_portal_config(p);
	if (!pcfg)
		return 0;

	irq_set_affinity(pcfg->irq, cpumask_of(cpu));
	return 0;
}

int bman_portals_probed(void)
{
	return __bman_portals_probed;
}
EXPORT_SYMBOL_GPL(bman_portals_probed);

static int bman_portal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct bm_portal_config *pcfg;
	struct resource *addr_phys[2];
	int irq, cpu, err, i;

	err = bman_is_probed();
	if (!err)
		return -EPROBE_DEFER;
	if (err < 0) {
		dev_err(&pdev->dev, "failing probe due to bman probe error\n");
		return -ENODEV;
	}

	pcfg = devm_kmalloc(dev, sizeof(*pcfg), GFP_KERNEL);
	if (!pcfg) {
		__bman_portals_probed = -1;
		return -ENOMEM;
	}

	pcfg->dev = dev;

	addr_phys[0] = platform_get_resource(pdev, IORESOURCE_MEM,
					     DPAA_PORTAL_CE);
	if (!addr_phys[0]) {
		dev_err(dev, "Can't get %pOF property 'reg::CE'\n", node);
		goto err_ioremap1;
	}

	addr_phys[1] = platform_get_resource(pdev, IORESOURCE_MEM,
					     DPAA_PORTAL_CI);
	if (!addr_phys[1]) {
		dev_err(dev, "Can't get %pOF property 'reg::CI'\n", node);
		goto err_ioremap1;
	}

	pcfg->cpu = -1;

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0)
		goto err_ioremap1;
	pcfg->irq = irq;

	pcfg->addr_virt_ce = memremap(addr_phys[0]->start,
					resource_size(addr_phys[0]),
					QBMAN_MEMREMAP_ATTR);
	if (!pcfg->addr_virt_ce) {
		dev_err(dev, "memremap::CE failed\n");
		goto err_ioremap1;
	}

	pcfg->addr_virt_ci = ioremap(addr_phys[1]->start,
					resource_size(addr_phys[1]));
	if (!pcfg->addr_virt_ci) {
		dev_err(dev, "ioremap::CI failed\n");
		goto err_ioremap2;
	}

	spin_lock(&bman_lock);
	cpu = cpumask_first_zero(&portal_cpus);
	if (cpu >= nr_cpu_ids) {
		__bman_portals_probed = 1;
		 
		spin_unlock(&bman_lock);
		goto check_cleanup;
	}

	cpumask_set_cpu(cpu, &portal_cpus);
	spin_unlock(&bman_lock);
	pcfg->cpu = cpu;

	if (!init_pcfg(pcfg)) {
		dev_err(dev, "portal init failed\n");
		goto err_portal_init;
	}

	 
	if (!cpu_online(cpu))
		bman_offline_cpu(cpu);

check_cleanup:
	if (__bman_portals_probed == 1 && bman_requires_cleanup()) {
		 
		for (i = 0; i < BM_POOL_MAX; i++) {
			err =  bm_shutdown_pool(i);
			if (err) {
				dev_err(dev, "Failed to shutdown bpool %d\n",
					i);
				goto err_portal_init;
			}
		}
		bman_done_cleanup();
	}

	return 0;

err_portal_init:
	iounmap(pcfg->addr_virt_ci);
err_ioremap2:
	memunmap(pcfg->addr_virt_ce);
err_ioremap1:
	 __bman_portals_probed = -1;

	return -ENXIO;
}

static const struct of_device_id bman_portal_ids[] = {
	{
		.compatible = "fsl,bman-portal",
	},
	{}
};
MODULE_DEVICE_TABLE(of, bman_portal_ids);

static struct platform_driver bman_portal_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = bman_portal_ids,
	},
	.probe = bman_portal_probe,
};

static int __init bman_portal_driver_register(struct platform_driver *drv)
{
	int ret;

	ret = platform_driver_register(drv);
	if (ret < 0)
		return ret;

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
					"soc/qbman_portal:online",
					bman_online_cpu, bman_offline_cpu);
	if (ret < 0) {
		pr_err("bman: failed to register hotplug callbacks.\n");
		platform_driver_unregister(drv);
		return ret;
	}
	return 0;
}

module_driver(bman_portal_driver,
	      bman_portal_driver_register, platform_driver_unregister);
