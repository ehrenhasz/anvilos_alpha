
 
#define pr_fmt(fmt) "cpuidle cooling: " fmt

#include <linux/cpu.h>
#include <linux/cpu_cooling.h>
#include <linux/cpuidle.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/idle_inject.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/thermal.h>

 
struct cpuidle_cooling_device {
	struct idle_inject_device *ii_dev;
	unsigned long state;
};

 
static unsigned int cpuidle_cooling_runtime(unsigned int idle_duration_us,
					    unsigned long state)
{
	if (!state)
		return 0;

	return ((idle_duration_us * 100) / state) - idle_duration_us;
}

 
static int cpuidle_cooling_get_max_state(struct thermal_cooling_device *cdev,
					 unsigned long *state)
{
	 
	*state = 100;

	return 0;
}

 
static int cpuidle_cooling_get_cur_state(struct thermal_cooling_device *cdev,
					 unsigned long *state)
{
	struct cpuidle_cooling_device *idle_cdev = cdev->devdata;

	*state = idle_cdev->state;

	return 0;
}

 
static int cpuidle_cooling_set_cur_state(struct thermal_cooling_device *cdev,
					 unsigned long state)
{
	struct cpuidle_cooling_device *idle_cdev = cdev->devdata;
	struct idle_inject_device *ii_dev = idle_cdev->ii_dev;
	unsigned long current_state = idle_cdev->state;
	unsigned int runtime_us, idle_duration_us;

	idle_cdev->state = state;

	idle_inject_get_duration(ii_dev, &runtime_us, &idle_duration_us);

	runtime_us = cpuidle_cooling_runtime(idle_duration_us, state);

	idle_inject_set_duration(ii_dev, runtime_us, idle_duration_us);

	if (current_state == 0 && state > 0) {
		idle_inject_start(ii_dev);
	} else if (current_state > 0 && !state)  {
		idle_inject_stop(ii_dev);
	}

	return 0;
}

 
static struct thermal_cooling_device_ops cpuidle_cooling_ops = {
	.get_max_state = cpuidle_cooling_get_max_state,
	.get_cur_state = cpuidle_cooling_get_cur_state,
	.set_cur_state = cpuidle_cooling_set_cur_state,
};

 
static int __cpuidle_cooling_register(struct device_node *np,
				      struct cpuidle_driver *drv)
{
	struct idle_inject_device *ii_dev;
	struct cpuidle_cooling_device *idle_cdev;
	struct thermal_cooling_device *cdev;
	struct device *dev;
	unsigned int idle_duration_us = TICK_USEC;
	unsigned int latency_us = UINT_MAX;
	char *name;
	int ret;

	idle_cdev = kzalloc(sizeof(*idle_cdev), GFP_KERNEL);
	if (!idle_cdev) {
		ret = -ENOMEM;
		goto out;
	}

	ii_dev = idle_inject_register(drv->cpumask);
	if (!ii_dev) {
		ret = -EINVAL;
		goto out_kfree;
	}

	of_property_read_u32(np, "duration-us", &idle_duration_us);
	of_property_read_u32(np, "exit-latency-us", &latency_us);

	idle_inject_set_duration(ii_dev, TICK_USEC, idle_duration_us);
	idle_inject_set_latency(ii_dev, latency_us);

	idle_cdev->ii_dev = ii_dev;

	dev = get_cpu_device(cpumask_first(drv->cpumask));

	name = kasprintf(GFP_KERNEL, "idle-%s", dev_name(dev));
	if (!name) {
		ret = -ENOMEM;
		goto out_unregister;
	}

	cdev = thermal_of_cooling_device_register(np, name, idle_cdev,
						  &cpuidle_cooling_ops);
	if (IS_ERR(cdev)) {
		ret = PTR_ERR(cdev);
		goto out_kfree_name;
	}

	pr_debug("%s: Idle injection set with idle duration=%u, latency=%u\n",
		 name, idle_duration_us, latency_us);

	kfree(name);

	return 0;

out_kfree_name:
	kfree(name);
out_unregister:
	idle_inject_unregister(ii_dev);
out_kfree:
	kfree(idle_cdev);
out:
	return ret;
}

 
void cpuidle_cooling_register(struct cpuidle_driver *drv)
{
	struct device_node *cooling_node;
	struct device_node *cpu_node;
	int cpu, ret;

	for_each_cpu(cpu, drv->cpumask) {

		cpu_node = of_cpu_device_node_get(cpu);

		cooling_node = of_get_child_by_name(cpu_node, "thermal-idle");

		of_node_put(cpu_node);

		if (!cooling_node) {
			pr_debug("'thermal-idle' node not found for cpu%d\n", cpu);
			continue;
		}

		ret = __cpuidle_cooling_register(cooling_node, drv);

		of_node_put(cooling_node);

		if (ret) {
			pr_err("Failed to register the cpuidle cooling device" \
			       "for cpu%d: %d\n", cpu, ret);
			break;
		}
	}
}
