
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/acpi.h>
#include <acpi/processor.h>
#include <linux/uaccess.h>

#ifdef CONFIG_CPU_FREQ

 

#define CPUFREQ_THERMAL_MIN_STEP 0
#define CPUFREQ_THERMAL_MAX_STEP 3

static DEFINE_PER_CPU(unsigned int, cpufreq_thermal_reduction_pctg);

#define reduction_pctg(cpu) \
	per_cpu(cpufreq_thermal_reduction_pctg, phys_package_first_cpu(cpu))

 
static int phys_package_first_cpu(int cpu)
{
	int i;
	int id = topology_physical_package_id(cpu);

	for_each_online_cpu(i)
		if (topology_physical_package_id(i) == id)
			return i;
	return 0;
}

static int cpu_has_cpufreq(unsigned int cpu)
{
	struct cpufreq_policy *policy;

	if (!acpi_processor_cpufreq_init)
		return 0;

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		cpufreq_cpu_put(policy);
		return 1;
	}
	return 0;
}

static int cpufreq_get_max_state(unsigned int cpu)
{
	if (!cpu_has_cpufreq(cpu))
		return 0;

	return CPUFREQ_THERMAL_MAX_STEP;
}

static int cpufreq_get_cur_state(unsigned int cpu)
{
	if (!cpu_has_cpufreq(cpu))
		return 0;

	return reduction_pctg(cpu);
}

static int cpufreq_set_cur_state(unsigned int cpu, int state)
{
	struct cpufreq_policy *policy;
	struct acpi_processor *pr;
	unsigned long max_freq;
	int i, ret;

	if (!cpu_has_cpufreq(cpu))
		return 0;

	reduction_pctg(cpu) = state;

	 
	for_each_online_cpu(i) {
		if (topology_physical_package_id(i) !=
		    topology_physical_package_id(cpu))
			continue;

		pr = per_cpu(processors, i);

		if (unlikely(!freq_qos_request_active(&pr->thermal_req)))
			continue;

		policy = cpufreq_cpu_get(i);
		if (!policy)
			return -EINVAL;

		max_freq = (policy->cpuinfo.max_freq * (100 - reduction_pctg(i) * 20)) / 100;

		cpufreq_cpu_put(policy);

		ret = freq_qos_update_request(&pr->thermal_req, max_freq);
		if (ret < 0) {
			pr_warn("Failed to update thermal freq constraint: CPU%d (%d)\n",
				pr->id, ret);
		}
	}
	return 0;
}

void acpi_thermal_cpufreq_init(struct cpufreq_policy *policy)
{
	unsigned int cpu;

	for_each_cpu(cpu, policy->related_cpus) {
		struct acpi_processor *pr = per_cpu(processors, cpu);
		int ret;

		if (!pr)
			continue;

		ret = freq_qos_add_request(&policy->constraints,
					   &pr->thermal_req,
					   FREQ_QOS_MAX, INT_MAX);
		if (ret < 0) {
			pr_err("Failed to add freq constraint for CPU%d (%d)\n",
			       cpu, ret);
			continue;
		}

		thermal_cooling_device_update(pr->cdev);
	}
}

void acpi_thermal_cpufreq_exit(struct cpufreq_policy *policy)
{
	unsigned int cpu;

	for_each_cpu(cpu, policy->related_cpus) {
		struct acpi_processor *pr = per_cpu(processors, cpu);

		if (!pr)
			continue;

		freq_qos_remove_request(&pr->thermal_req);

		thermal_cooling_device_update(pr->cdev);
	}
}
#else				 
static int cpufreq_get_max_state(unsigned int cpu)
{
	return 0;
}

static int cpufreq_get_cur_state(unsigned int cpu)
{
	return 0;
}

static int cpufreq_set_cur_state(unsigned int cpu, int state)
{
	return 0;
}

#endif

 
static int acpi_processor_max_state(struct acpi_processor *pr)
{
	int max_state = 0;

	 
	max_state += cpufreq_get_max_state(pr->id);
	if (pr->flags.throttling)
		max_state += (pr->throttling.state_count -1);

	return max_state;
}
static int
processor_get_max_state(struct thermal_cooling_device *cdev,
			unsigned long *state)
{
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr;

	if (!device)
		return -EINVAL;

	pr = acpi_driver_data(device);
	if (!pr)
		return -EINVAL;

	*state = acpi_processor_max_state(pr);
	return 0;
}

static int
processor_get_cur_state(struct thermal_cooling_device *cdev,
			unsigned long *cur_state)
{
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr;

	if (!device)
		return -EINVAL;

	pr = acpi_driver_data(device);
	if (!pr)
		return -EINVAL;

	*cur_state = cpufreq_get_cur_state(pr->id);
	if (pr->flags.throttling)
		*cur_state += pr->throttling.state;
	return 0;
}

static int
processor_set_cur_state(struct thermal_cooling_device *cdev,
			unsigned long state)
{
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr;
	int result = 0;
	int max_pstate;

	if (!device)
		return -EINVAL;

	pr = acpi_driver_data(device);
	if (!pr)
		return -EINVAL;

	max_pstate = cpufreq_get_max_state(pr->id);

	if (state > acpi_processor_max_state(pr))
		return -EINVAL;

	if (state <= max_pstate) {
		if (pr->flags.throttling && pr->throttling.state)
			result = acpi_processor_set_throttling(pr, 0, false);
		cpufreq_set_cur_state(pr->id, state);
	} else {
		cpufreq_set_cur_state(pr->id, max_pstate);
		result = acpi_processor_set_throttling(pr,
				state - max_pstate, false);
	}
	return result;
}

const struct thermal_cooling_device_ops processor_cooling_ops = {
	.get_max_state = processor_get_max_state,
	.get_cur_state = processor_get_cur_state,
	.set_cur_state = processor_set_cur_state,
};

int acpi_processor_thermal_init(struct acpi_processor *pr,
				struct acpi_device *device)
{
	int result = 0;

	pr->cdev = thermal_cooling_device_register("Processor", device,
						   &processor_cooling_ops);
	if (IS_ERR(pr->cdev)) {
		result = PTR_ERR(pr->cdev);
		return result;
	}

	dev_dbg(&device->dev, "registered as cooling_device%d\n",
		pr->cdev->id);

	result = sysfs_create_link(&device->dev.kobj,
				   &pr->cdev->device.kobj,
				   "thermal_cooling");
	if (result) {
		dev_err(&device->dev,
			"Failed to create sysfs link 'thermal_cooling'\n");
		goto err_thermal_unregister;
	}

	result = sysfs_create_link(&pr->cdev->device.kobj,
				   &device->dev.kobj,
				   "device");
	if (result) {
		dev_err(&pr->cdev->device,
			"Failed to create sysfs link 'device'\n");
		goto err_remove_sysfs_thermal;
	}

	return 0;

err_remove_sysfs_thermal:
	sysfs_remove_link(&device->dev.kobj, "thermal_cooling");
err_thermal_unregister:
	thermal_cooling_device_unregister(pr->cdev);

	return result;
}

void acpi_processor_thermal_exit(struct acpi_processor *pr,
				 struct acpi_device *device)
{
	if (pr->cdev) {
		sysfs_remove_link(&device->dev.kobj, "thermal_cooling");
		sysfs_remove_link(&pr->cdev->device.kobj, "device");
		thermal_cooling_device_unregister(pr->cdev);
		pr->cdev = NULL;
	}
}
