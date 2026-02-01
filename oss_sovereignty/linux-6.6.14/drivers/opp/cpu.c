
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/slab.h>

#include "opp.h"

#ifdef CONFIG_CPU_FREQ

 
int dev_pm_opp_init_cpufreq_table(struct device *dev,
				  struct cpufreq_frequency_table **opp_table)
{
	struct dev_pm_opp *opp;
	struct cpufreq_frequency_table *freq_table = NULL;
	int i, max_opps, ret = 0;
	unsigned long rate;

	max_opps = dev_pm_opp_get_opp_count(dev);
	if (max_opps <= 0)
		return max_opps ? max_opps : -ENODATA;

	freq_table = kcalloc((max_opps + 1), sizeof(*freq_table), GFP_KERNEL);
	if (!freq_table)
		return -ENOMEM;

	for (i = 0, rate = 0; i < max_opps; i++, rate++) {
		 
		opp = dev_pm_opp_find_freq_ceil(dev, &rate);
		if (IS_ERR(opp)) {
			ret = PTR_ERR(opp);
			goto out;
		}
		freq_table[i].driver_data = i;
		freq_table[i].frequency = rate / 1000;

		 
		if (dev_pm_opp_is_turbo(opp))
			freq_table[i].flags = CPUFREQ_BOOST_FREQ;

		dev_pm_opp_put(opp);
	}

	freq_table[i].driver_data = i;
	freq_table[i].frequency = CPUFREQ_TABLE_END;

	*opp_table = &freq_table[0];

out:
	if (ret)
		kfree(freq_table);

	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_opp_init_cpufreq_table);

 
void dev_pm_opp_free_cpufreq_table(struct device *dev,
				   struct cpufreq_frequency_table **opp_table)
{
	if (!opp_table)
		return;

	kfree(*opp_table);
	*opp_table = NULL;
}
EXPORT_SYMBOL_GPL(dev_pm_opp_free_cpufreq_table);
#endif	 

void _dev_pm_opp_cpumask_remove_table(const struct cpumask *cpumask,
				      int last_cpu)
{
	struct device *cpu_dev;
	int cpu;

	WARN_ON(cpumask_empty(cpumask));

	for_each_cpu(cpu, cpumask) {
		if (cpu == last_cpu)
			break;

		cpu_dev = get_cpu_device(cpu);
		if (!cpu_dev) {
			pr_err("%s: failed to get cpu%d device\n", __func__,
			       cpu);
			continue;
		}

		dev_pm_opp_remove_table(cpu_dev);
	}
}

 
void dev_pm_opp_cpumask_remove_table(const struct cpumask *cpumask)
{
	_dev_pm_opp_cpumask_remove_table(cpumask, -1);
}
EXPORT_SYMBOL_GPL(dev_pm_opp_cpumask_remove_table);

 
int dev_pm_opp_set_sharing_cpus(struct device *cpu_dev,
				const struct cpumask *cpumask)
{
	struct opp_device *opp_dev;
	struct opp_table *opp_table;
	struct device *dev;
	int cpu, ret = 0;

	opp_table = _find_opp_table(cpu_dev);
	if (IS_ERR(opp_table))
		return PTR_ERR(opp_table);

	for_each_cpu(cpu, cpumask) {
		if (cpu == cpu_dev->id)
			continue;

		dev = get_cpu_device(cpu);
		if (!dev) {
			dev_err(cpu_dev, "%s: failed to get cpu%d device\n",
				__func__, cpu);
			continue;
		}

		opp_dev = _add_opp_dev(dev, opp_table);
		if (!opp_dev) {
			dev_err(dev, "%s: failed to add opp-dev for cpu%d device\n",
				__func__, cpu);
			continue;
		}

		 
		opp_table->shared_opp = OPP_TABLE_ACCESS_SHARED;
	}

	dev_pm_opp_put_opp_table(opp_table);

	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_opp_set_sharing_cpus);

 
int dev_pm_opp_get_sharing_cpus(struct device *cpu_dev, struct cpumask *cpumask)
{
	struct opp_device *opp_dev;
	struct opp_table *opp_table;
	int ret = 0;

	opp_table = _find_opp_table(cpu_dev);
	if (IS_ERR(opp_table))
		return PTR_ERR(opp_table);

	if (opp_table->shared_opp == OPP_TABLE_ACCESS_UNKNOWN) {
		ret = -EINVAL;
		goto put_opp_table;
	}

	cpumask_clear(cpumask);

	if (opp_table->shared_opp == OPP_TABLE_ACCESS_SHARED) {
		mutex_lock(&opp_table->lock);
		list_for_each_entry(opp_dev, &opp_table->dev_list, node)
			cpumask_set_cpu(opp_dev->dev->id, cpumask);
		mutex_unlock(&opp_table->lock);
	} else {
		cpumask_set_cpu(cpu_dev->id, cpumask);
	}

put_opp_table:
	dev_pm_opp_put_opp_table(opp_table);

	return ret;
}
EXPORT_SYMBOL_GPL(dev_pm_opp_get_sharing_cpus);
