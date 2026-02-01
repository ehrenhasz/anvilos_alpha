
 
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <asm/cpu.h>
#include <asm/cputype.h>
#include <asm/cpuidle.h>
#include <asm/mcpm.h>
#include <asm/smp_plat.h>
#include <asm/suspend.h>

#include "dt_idle_states.h"

static int bl_enter_powerdown(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int idx);

 
static struct cpuidle_driver bl_idle_little_driver = {
	.name = "little_idle",
	.owner = THIS_MODULE,
	.states[0] = ARM_CPUIDLE_WFI_STATE,
	.states[1] = {
		.enter			= bl_enter_powerdown,
		.exit_latency		= 700,
		.target_residency	= 2500,
		.flags			= CPUIDLE_FLAG_TIMER_STOP |
					  CPUIDLE_FLAG_RCU_IDLE,
		.name			= "C1",
		.desc			= "ARM little-cluster power down",
	},
	.state_count = 2,
};

static const struct of_device_id bl_idle_state_match[] __initconst = {
	{ .compatible = "arm,idle-state",
	  .data = bl_enter_powerdown },
	{ },
};

static struct cpuidle_driver bl_idle_big_driver = {
	.name = "big_idle",
	.owner = THIS_MODULE,
	.states[0] = ARM_CPUIDLE_WFI_STATE,
	.states[1] = {
		.enter			= bl_enter_powerdown,
		.exit_latency		= 500,
		.target_residency	= 2000,
		.flags			= CPUIDLE_FLAG_TIMER_STOP |
					  CPUIDLE_FLAG_RCU_IDLE,
		.name			= "C1",
		.desc			= "ARM big-cluster power down",
	},
	.state_count = 2,
};

 
static int notrace bl_powerdown_finisher(unsigned long arg)
{
	 
	unsigned int mpidr = read_cpuid_mpidr();
	unsigned int cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);
	unsigned int cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);

	mcpm_set_entry_vector(cpu, cluster, cpu_resume);
	mcpm_cpu_suspend();

	 
	return 1;
}

 
static __cpuidle int bl_enter_powerdown(struct cpuidle_device *dev,
					struct cpuidle_driver *drv, int idx)
{
	cpu_pm_enter();
	ct_cpuidle_enter();

	cpu_suspend(0, bl_powerdown_finisher);

	 
	mcpm_cpu_powered_up();
	ct_cpuidle_exit();

	cpu_pm_exit();

	return idx;
}

static int __init bl_idle_driver_init(struct cpuidle_driver *drv, int part_id)
{
	struct cpumask *cpumask;
	int cpu;

	cpumask = kzalloc(cpumask_size(), GFP_KERNEL);
	if (!cpumask)
		return -ENOMEM;

	for_each_possible_cpu(cpu)
		if (smp_cpuid_part(cpu) == part_id)
			cpumask_set_cpu(cpu, cpumask);

	drv->cpumask = cpumask;

	return 0;
}

static const struct of_device_id compatible_machine_match[] = {
	{ .compatible = "arm,vexpress,v2p-ca15_a7" },
	{ .compatible = "google,peach" },
	{},
};

static int __init bl_idle_init(void)
{
	int ret;
	struct device_node *root = of_find_node_by_path("/");
	const struct of_device_id *match_id;

	if (!root)
		return -ENODEV;

	 
	match_id = of_match_node(compatible_machine_match, root);

	of_node_put(root);

	if (!match_id)
		return -ENODEV;

	if (!mcpm_is_available())
		return -EUNATCH;

	 
	ret = bl_idle_driver_init(&bl_idle_little_driver,
				  ARM_CPU_PART_CORTEX_A7);
	if (ret)
		return ret;

	ret = bl_idle_driver_init(&bl_idle_big_driver, ARM_CPU_PART_CORTEX_A15);
	if (ret)
		goto out_uninit_little;

	 
	ret = dt_init_idle_driver(&bl_idle_big_driver, bl_idle_state_match, 1);
	if (ret < 0)
		goto out_uninit_big;

	 
	ret = dt_init_idle_driver(&bl_idle_little_driver,
				  bl_idle_state_match, 1);
	if (ret < 0)
		goto out_uninit_big;

	ret = cpuidle_register(&bl_idle_little_driver, NULL);
	if (ret)
		goto out_uninit_big;

	ret = cpuidle_register(&bl_idle_big_driver, NULL);
	if (ret)
		goto out_unregister_little;

	return 0;

out_unregister_little:
	cpuidle_unregister(&bl_idle_little_driver);
out_uninit_big:
	kfree(bl_idle_big_driver.cpumask);
out_uninit_little:
	kfree(bl_idle_little_driver.cpumask);

	return ret;
}
device_initcall(bl_idle_init);
