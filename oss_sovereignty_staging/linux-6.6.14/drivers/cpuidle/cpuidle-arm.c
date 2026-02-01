
 

#define pr_fmt(fmt) "CPUidle arm: " fmt

#include <linux/cpu_cooling.h>
#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

#include <asm/cpuidle.h>

#include "dt_idle_states.h"

 
static __cpuidle int arm_enter_idle_state(struct cpuidle_device *dev,
					  struct cpuidle_driver *drv, int idx)
{
	 
	return CPU_PM_CPU_IDLE_ENTER(arm_cpuidle_suspend, idx);
}

static struct cpuidle_driver arm_idle_driver __initdata = {
	.name = "arm_idle",
	.owner = THIS_MODULE,
	 
	.states[0] = {
		.enter                  = arm_enter_idle_state,
		.exit_latency           = 1,
		.target_residency       = 1,
		.power_usage		= UINT_MAX,
		.name                   = "WFI",
		.desc                   = "ARM WFI",
	}
};

static const struct of_device_id arm_idle_state_match[] __initconst = {
	{ .compatible = "arm,idle-state",
	  .data = arm_enter_idle_state },
	{ },
};

 
static int __init arm_idle_init_cpu(int cpu)
{
	int ret;
	struct cpuidle_driver *drv;

	drv = kmemdup(&arm_idle_driver, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->cpumask = (struct cpumask *)cpumask_of(cpu);

	 
	ret = dt_init_idle_driver(drv, arm_idle_state_match, 1);
	if (ret <= 0) {
		ret = ret ? : -ENODEV;
		goto out_kfree_drv;
	}

	 
	ret = arm_cpuidle_init(cpu);

	 
	if (ret) {
		if (ret != -EOPNOTSUPP)
			pr_err("CPU %d failed to init idle CPU ops\n", cpu);
		ret = ret == -ENXIO ? 0 : ret;
		goto out_kfree_drv;
	}

	ret = cpuidle_register(drv, NULL);
	if (ret)
		goto out_kfree_drv;

	cpuidle_cooling_register(drv);

	return 0;

out_kfree_drv:
	kfree(drv);
	return ret;
}

 
static int __init arm_idle_init(void)
{
	int cpu, ret;
	struct cpuidle_driver *drv;
	struct cpuidle_device *dev;

	for_each_possible_cpu(cpu) {
		ret = arm_idle_init_cpu(cpu);
		if (ret)
			goto out_fail;
	}

	return 0;

out_fail:
	while (--cpu >= 0) {
		dev = per_cpu(cpuidle_devices, cpu);
		drv = cpuidle_get_cpu_driver(dev);
		cpuidle_unregister(drv);
		kfree(drv);
	}

	return ret;
}
device_initcall(arm_idle_init);
