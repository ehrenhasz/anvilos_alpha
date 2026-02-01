
 

#include <linux/devfreq.h>
#include <linux/devfreq_cooling.h>
#include <linux/energy_model.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/thermal.h>
#include <linux/units.h>

#include "thermal_trace.h"

#define SCALE_ERROR_MITIGATION	100

 
struct devfreq_cooling_device {
	struct thermal_cooling_device *cdev;
	struct thermal_cooling_device_ops cooling_ops;
	struct devfreq *devfreq;
	unsigned long cooling_state;
	u32 *freq_table;
	size_t max_state;
	struct devfreq_cooling_power *power_ops;
	u32 res_util;
	int capped_state;
	struct dev_pm_qos_request req_max_freq;
	struct em_perf_domain *em_pd;
};

static int devfreq_cooling_get_max_state(struct thermal_cooling_device *cdev,
					 unsigned long *state)
{
	struct devfreq_cooling_device *dfc = cdev->devdata;

	*state = dfc->max_state;

	return 0;
}

static int devfreq_cooling_get_cur_state(struct thermal_cooling_device *cdev,
					 unsigned long *state)
{
	struct devfreq_cooling_device *dfc = cdev->devdata;

	*state = dfc->cooling_state;

	return 0;
}

static int devfreq_cooling_set_cur_state(struct thermal_cooling_device *cdev,
					 unsigned long state)
{
	struct devfreq_cooling_device *dfc = cdev->devdata;
	struct devfreq *df = dfc->devfreq;
	struct device *dev = df->dev.parent;
	unsigned long freq;
	int perf_idx;

	if (state == dfc->cooling_state)
		return 0;

	dev_dbg(dev, "Setting cooling state %lu\n", state);

	if (state > dfc->max_state)
		return -EINVAL;

	if (dfc->em_pd) {
		perf_idx = dfc->max_state - state;
		freq = dfc->em_pd->table[perf_idx].frequency * 1000;
	} else {
		freq = dfc->freq_table[state];
	}

	dev_pm_qos_update_request(&dfc->req_max_freq,
				  DIV_ROUND_UP(freq, HZ_PER_KHZ));

	dfc->cooling_state = state;

	return 0;
}

 
static int get_perf_idx(struct em_perf_domain *em_pd, unsigned long freq)
{
	int i;

	for (i = 0; i < em_pd->nr_perf_states; i++) {
		if (em_pd->table[i].frequency == freq)
			return i;
	}

	return -EINVAL;
}

static unsigned long get_voltage(struct devfreq *df, unsigned long freq)
{
	struct device *dev = df->dev.parent;
	unsigned long voltage;
	struct dev_pm_opp *opp;

	opp = dev_pm_opp_find_freq_exact(dev, freq, true);
	if (PTR_ERR(opp) == -ERANGE)
		opp = dev_pm_opp_find_freq_exact(dev, freq, false);

	if (IS_ERR(opp)) {
		dev_err_ratelimited(dev, "Failed to find OPP for frequency %lu: %ld\n",
				    freq, PTR_ERR(opp));
		return 0;
	}

	voltage = dev_pm_opp_get_voltage(opp) / 1000;  
	dev_pm_opp_put(opp);

	if (voltage == 0) {
		dev_err_ratelimited(dev,
				    "Failed to get voltage for frequency %lu\n",
				    freq);
	}

	return voltage;
}

static void _normalize_load(struct devfreq_dev_status *status)
{
	if (status->total_time > 0xfffff) {
		status->total_time >>= 10;
		status->busy_time >>= 10;
	}

	status->busy_time <<= 10;
	status->busy_time /= status->total_time ? : 1;

	status->busy_time = status->busy_time ? : 1;
	status->total_time = 1024;
}

static int devfreq_cooling_get_requested_power(struct thermal_cooling_device *cdev,
					       u32 *power)
{
	struct devfreq_cooling_device *dfc = cdev->devdata;
	struct devfreq *df = dfc->devfreq;
	struct devfreq_dev_status status;
	unsigned long state;
	unsigned long freq;
	unsigned long voltage;
	int res, perf_idx;

	mutex_lock(&df->lock);
	status = df->last_status;
	mutex_unlock(&df->lock);

	freq = status.current_frequency;

	if (dfc->power_ops && dfc->power_ops->get_real_power) {
		voltage = get_voltage(df, freq);
		if (voltage == 0) {
			res = -EINVAL;
			goto fail;
		}

		res = dfc->power_ops->get_real_power(df, power, freq, voltage);
		if (!res) {
			state = dfc->capped_state;

			 
			dfc->res_util = dfc->em_pd->table[state].power;
			dfc->res_util /= MICROWATT_PER_MILLIWATT;

			dfc->res_util *= SCALE_ERROR_MITIGATION;

			if (*power > 1)
				dfc->res_util /= *power;
		} else {
			goto fail;
		}
	} else {
		 
		perf_idx = get_perf_idx(dfc->em_pd, freq / 1000);
		if (perf_idx < 0) {
			res = -EAGAIN;
			goto fail;
		}

		_normalize_load(&status);

		 
		*power = dfc->em_pd->table[perf_idx].power;
		*power /= MICROWATT_PER_MILLIWATT;
		 
		*power *= status.busy_time;
		*power >>= 10;
	}

	trace_thermal_power_devfreq_get_power(cdev, &status, freq, *power);

	return 0;
fail:
	 
	dfc->res_util = SCALE_ERROR_MITIGATION;
	return res;
}

static int devfreq_cooling_state2power(struct thermal_cooling_device *cdev,
				       unsigned long state, u32 *power)
{
	struct devfreq_cooling_device *dfc = cdev->devdata;
	int perf_idx;

	if (state > dfc->max_state)
		return -EINVAL;

	perf_idx = dfc->max_state - state;
	*power = dfc->em_pd->table[perf_idx].power;
	*power /= MICROWATT_PER_MILLIWATT;

	return 0;
}

static int devfreq_cooling_power2state(struct thermal_cooling_device *cdev,
				       u32 power, unsigned long *state)
{
	struct devfreq_cooling_device *dfc = cdev->devdata;
	struct devfreq *df = dfc->devfreq;
	struct devfreq_dev_status status;
	unsigned long freq, em_power_mw;
	s32 est_power;
	int i;

	mutex_lock(&df->lock);
	status = df->last_status;
	mutex_unlock(&df->lock);

	freq = status.current_frequency;

	if (dfc->power_ops && dfc->power_ops->get_real_power) {
		 
		est_power = power * dfc->res_util;
		est_power /= SCALE_ERROR_MITIGATION;
	} else {
		 
		_normalize_load(&status);
		est_power = power << 10;
		est_power /= status.busy_time;
	}

	 
	for (i = dfc->max_state; i > 0; i--) {
		 
		em_power_mw = dfc->em_pd->table[i].power;
		em_power_mw /= MICROWATT_PER_MILLIWATT;
		if (est_power >= em_power_mw)
			break;
	}

	*state = dfc->max_state - i;
	dfc->capped_state = *state;

	trace_thermal_power_devfreq_limit(cdev, freq, *state, power);
	return 0;
}

 
static int devfreq_cooling_gen_tables(struct devfreq_cooling_device *dfc,
				      int num_opps)
{
	struct devfreq *df = dfc->devfreq;
	struct device *dev = df->dev.parent;
	unsigned long freq;
	int i;

	dfc->freq_table = kcalloc(num_opps, sizeof(*dfc->freq_table),
			     GFP_KERNEL);
	if (!dfc->freq_table)
		return -ENOMEM;

	for (i = 0, freq = ULONG_MAX; i < num_opps; i++, freq--) {
		struct dev_pm_opp *opp;

		opp = dev_pm_opp_find_freq_floor(dev, &freq);
		if (IS_ERR(opp)) {
			kfree(dfc->freq_table);
			return PTR_ERR(opp);
		}

		dev_pm_opp_put(opp);
		dfc->freq_table[i] = freq;
	}

	return 0;
}

 
struct thermal_cooling_device *
of_devfreq_cooling_register_power(struct device_node *np, struct devfreq *df,
				  struct devfreq_cooling_power *dfc_power)
{
	struct thermal_cooling_device *cdev;
	struct device *dev = df->dev.parent;
	struct devfreq_cooling_device *dfc;
	struct em_perf_domain *em;
	struct thermal_cooling_device_ops *ops;
	char *name;
	int err, num_opps;


	dfc = kzalloc(sizeof(*dfc), GFP_KERNEL);
	if (!dfc)
		return ERR_PTR(-ENOMEM);

	dfc->devfreq = df;

	ops = &dfc->cooling_ops;
	ops->get_max_state = devfreq_cooling_get_max_state;
	ops->get_cur_state = devfreq_cooling_get_cur_state;
	ops->set_cur_state = devfreq_cooling_set_cur_state;

	em = em_pd_get(dev);
	if (em && !em_is_artificial(em)) {
		dfc->em_pd = em;
		ops->get_requested_power =
			devfreq_cooling_get_requested_power;
		ops->state2power = devfreq_cooling_state2power;
		ops->power2state = devfreq_cooling_power2state;

		dfc->power_ops = dfc_power;

		num_opps = em_pd_nr_perf_states(dfc->em_pd);
	} else {
		 
		dev_dbg(dev, "missing proper EM for cooling device\n");

		num_opps = dev_pm_opp_get_opp_count(dev);

		err = devfreq_cooling_gen_tables(dfc, num_opps);
		if (err)
			goto free_dfc;
	}

	if (num_opps <= 0) {
		err = -EINVAL;
		goto free_dfc;
	}

	 
	dfc->max_state = num_opps - 1;

	err = dev_pm_qos_add_request(dev, &dfc->req_max_freq,
				     DEV_PM_QOS_MAX_FREQUENCY,
				     PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	if (err < 0)
		goto free_table;

	err = -ENOMEM;
	name = kasprintf(GFP_KERNEL, "devfreq-%s", dev_name(dev));
	if (!name)
		goto remove_qos_req;

	cdev = thermal_of_cooling_device_register(np, name, dfc, ops);
	kfree(name);

	if (IS_ERR(cdev)) {
		err = PTR_ERR(cdev);
		dev_err(dev,
			"Failed to register devfreq cooling device (%d)\n",
			err);
		goto remove_qos_req;
	}

	dfc->cdev = cdev;

	return cdev;

remove_qos_req:
	dev_pm_qos_remove_request(&dfc->req_max_freq);
free_table:
	kfree(dfc->freq_table);
free_dfc:
	kfree(dfc);

	return ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(of_devfreq_cooling_register_power);

 
struct thermal_cooling_device *
of_devfreq_cooling_register(struct device_node *np, struct devfreq *df)
{
	return of_devfreq_cooling_register_power(np, df, NULL);
}
EXPORT_SYMBOL_GPL(of_devfreq_cooling_register);

 
struct thermal_cooling_device *devfreq_cooling_register(struct devfreq *df)
{
	return of_devfreq_cooling_register(NULL, df);
}
EXPORT_SYMBOL_GPL(devfreq_cooling_register);

 
struct thermal_cooling_device *
devfreq_cooling_em_register(struct devfreq *df,
			    struct devfreq_cooling_power *dfc_power)
{
	struct thermal_cooling_device *cdev;
	struct device *dev;
	int ret;

	if (IS_ERR_OR_NULL(df))
		return ERR_PTR(-EINVAL);

	dev = df->dev.parent;

	ret = dev_pm_opp_of_register_em(dev, NULL);
	if (ret)
		dev_dbg(dev, "Unable to register EM for devfreq cooling device (%d)\n",
			ret);

	cdev = of_devfreq_cooling_register_power(dev->of_node, df, dfc_power);

	if (IS_ERR_OR_NULL(cdev))
		em_dev_unregister_perf_domain(dev);

	return cdev;
}
EXPORT_SYMBOL_GPL(devfreq_cooling_em_register);

 
void devfreq_cooling_unregister(struct thermal_cooling_device *cdev)
{
	struct devfreq_cooling_device *dfc;
	struct device *dev;

	if (IS_ERR_OR_NULL(cdev))
		return;

	dfc = cdev->devdata;
	dev = dfc->devfreq->dev.parent;

	thermal_cooling_device_unregister(dfc->cdev);
	dev_pm_qos_remove_request(&dfc->req_max_freq);

	em_dev_unregister_perf_domain(dev);

	kfree(dfc->freq_table);
	kfree(dfc);
}
EXPORT_SYMBOL_GPL(devfreq_cooling_unregister);
