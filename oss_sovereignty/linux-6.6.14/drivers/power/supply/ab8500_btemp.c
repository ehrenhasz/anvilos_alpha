
 

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/of.h>
#include <linux/mfd/core.h>
#include <linux/mfd/abx500.h>
#include <linux/mfd/abx500/ab8500.h>
#include <linux/thermal.h>
#include <linux/iio/consumer.h>
#include <linux/fixp-arith.h>

#include "ab8500-bm.h"

#define BTEMP_THERMAL_LOW_LIMIT		-10
#define BTEMP_THERMAL_MED_LIMIT		0
#define BTEMP_THERMAL_HIGH_LIMIT_52	52
#define BTEMP_THERMAL_HIGH_LIMIT_57	57
#define BTEMP_THERMAL_HIGH_LIMIT_62	62

#define BTEMP_BATCTRL_CURR_SRC_7UA	7
#define BTEMP_BATCTRL_CURR_SRC_20UA	20

#define BTEMP_BATCTRL_CURR_SRC_16UA	16
#define BTEMP_BATCTRL_CURR_SRC_18UA	18

#define BTEMP_BATCTRL_CURR_SRC_60UA	60
#define BTEMP_BATCTRL_CURR_SRC_120UA	120

 
struct ab8500_btemp_interrupts {
	char *name;
	irqreturn_t (*isr)(int irq, void *data);
};

struct ab8500_btemp_events {
	bool batt_rem;
	bool btemp_high;
	bool btemp_medhigh;
	bool btemp_lowmed;
	bool btemp_low;
	bool ac_conn;
	bool usb_conn;
};

struct ab8500_btemp_ranges {
	int btemp_high_limit;
	int btemp_med_limit;
	int btemp_low_limit;
};

 
struct ab8500_btemp {
	struct device *dev;
	struct list_head node;
	int curr_source;
	int bat_temp;
	int prev_bat_temp;
	struct ab8500 *parent;
	struct thermal_zone_device *tz;
	struct iio_channel *bat_ctrl;
	struct ab8500_fg *fg;
	struct ab8500_bm_data *bm;
	struct power_supply *btemp_psy;
	struct ab8500_btemp_events events;
	struct ab8500_btemp_ranges btemp_ranges;
	struct workqueue_struct *btemp_wq;
	struct delayed_work btemp_periodic_work;
	bool initialized;
};

 
static enum power_supply_property ab8500_btemp_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TEMP,
};

static LIST_HEAD(ab8500_btemp_list);

 
static int ab8500_btemp_batctrl_volt_to_res(struct ab8500_btemp *di,
	int v_batctrl, int inst_curr)
{
	if (is_ab8500_1p1_or_earlier(di->parent)) {
		 
		return (450000 * (v_batctrl)) / (1800 - v_batctrl);
	}

	 
	return (80000 * (v_batctrl)) / (1800 - v_batctrl);
}

 
static int ab8500_btemp_read_batctrl_voltage(struct ab8500_btemp *di)
{
	int vbtemp, ret;
	static int prev;

	ret = iio_read_channel_processed(di->bat_ctrl, &vbtemp);
	if (ret < 0) {
		dev_err(di->dev,
			"%s ADC conversion failed, using previous value",
			__func__);
		return prev;
	}
	prev = vbtemp;
	return vbtemp;
}

 
static int ab8500_btemp_get_batctrl_res(struct ab8500_btemp *di)
{
	int ret;
	int batctrl = 0;
	int res;
	int inst_curr;
	int i;

	if (!di->fg)
		di->fg = ab8500_fg_get();
	if (!di->fg) {
		dev_err(di->dev, "No fg found\n");
		return -EINVAL;
	}

	ret = ab8500_fg_inst_curr_start(di->fg);

	if (ret) {
		dev_err(di->dev, "Failed to start current measurement\n");
		return ret;
	}

	do {
		msleep(20);
	} while (!ab8500_fg_inst_curr_started(di->fg));

	i = 0;

	do {
		batctrl += ab8500_btemp_read_batctrl_voltage(di);
		i++;
		msleep(20);
	} while (!ab8500_fg_inst_curr_done(di->fg));
	batctrl /= i;

	ret = ab8500_fg_inst_curr_finalize(di->fg, &inst_curr);
	if (ret) {
		dev_err(di->dev, "Failed to finalize current measurement\n");
		return ret;
	}

	res = ab8500_btemp_batctrl_volt_to_res(di, batctrl, inst_curr);

	dev_dbg(di->dev, "%s batctrl: %d res: %d inst_curr: %d samples: %d\n",
		__func__, batctrl, res, inst_curr, i);

	return res;
}

 
static int ab8500_btemp_id(struct ab8500_btemp *di)
{
	struct power_supply_battery_info *bi = di->bm->bi;
	int res;

	di->curr_source = BTEMP_BATCTRL_CURR_SRC_7UA;

	res =  ab8500_btemp_get_batctrl_res(di);
	if (res < 0) {
		dev_err(di->dev, "%s get batctrl res failed\n", __func__);
		return -ENXIO;
	}

	if (power_supply_battery_bti_in_range(bi, res)) {
		dev_info(di->dev, "Battery detected on BATCTRL (pin C3)"
			 " resistance %d Ohm = %d Ohm +/- %d%%\n",
			 res, bi->bti_resistance_ohm,
			 bi->bti_resistance_tolerance);
	} else {
		dev_warn(di->dev, "Battery identified as unknown"
			 ", resistance %d Ohm\n", res);
		return -ENXIO;
	}

	return 0;
}

 
static void ab8500_btemp_periodic_work(struct work_struct *work)
{
	int interval;
	int bat_temp;
	struct ab8500_btemp *di = container_of(work,
		struct ab8500_btemp, btemp_periodic_work.work);
	 
	static int prev = 25;
	int ret;

	if (!di->initialized) {
		 
		if (ab8500_btemp_id(di) < 0)
			dev_warn(di->dev, "failed to identify the battery\n");
	}

	 
	ret = thermal_zone_get_temp(di->tz, &bat_temp);
	if (ret) {
		dev_err(di->dev, "error reading temperature\n");
		bat_temp = prev;
	} else {
		 
		bat_temp /= 1000;
		prev = bat_temp;
	}

	 
	if ((bat_temp == di->prev_bat_temp) || !di->initialized) {
		if ((di->bat_temp != di->prev_bat_temp) || !di->initialized) {
			di->initialized = true;
			di->bat_temp = bat_temp;
			power_supply_changed(di->btemp_psy);
		}
	} else if (bat_temp < di->prev_bat_temp) {
		di->bat_temp--;
		power_supply_changed(di->btemp_psy);
	} else if (bat_temp > di->prev_bat_temp) {
		di->bat_temp++;
		power_supply_changed(di->btemp_psy);
	}
	di->prev_bat_temp = bat_temp;

	if (di->events.ac_conn || di->events.usb_conn)
		interval = di->bm->temp_interval_chg;
	else
		interval = di->bm->temp_interval_nochg;

	 
	queue_delayed_work(di->btemp_wq,
		&di->btemp_periodic_work,
		round_jiffies(interval * HZ));
}

 
static irqreturn_t ab8500_btemp_batctrlindb_handler(int irq, void *_di)
{
	struct ab8500_btemp *di = _di;
	dev_err(di->dev, "Battery removal detected!\n");

	di->events.batt_rem = true;
	power_supply_changed(di->btemp_psy);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_btemp_templow_handler(int irq, void *_di)
{
	struct ab8500_btemp *di = _di;

	if (is_ab8500_3p3_or_earlier(di->parent)) {
		dev_dbg(di->dev, "Ignore false btemp low irq"
			" for ABB cut 1.0, 1.1, 2.0 and 3.3\n");
	} else {
		dev_crit(di->dev, "Battery temperature lower than -10deg c\n");

		di->events.btemp_low = true;
		di->events.btemp_high = false;
		di->events.btemp_medhigh = false;
		di->events.btemp_lowmed = false;
		power_supply_changed(di->btemp_psy);
	}

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_btemp_temphigh_handler(int irq, void *_di)
{
	struct ab8500_btemp *di = _di;

	dev_crit(di->dev, "Battery temperature is higher than MAX temp\n");

	di->events.btemp_high = true;
	di->events.btemp_medhigh = false;
	di->events.btemp_lowmed = false;
	di->events.btemp_low = false;
	power_supply_changed(di->btemp_psy);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_btemp_lowmed_handler(int irq, void *_di)
{
	struct ab8500_btemp *di = _di;

	dev_dbg(di->dev, "Battery temperature is between low and medium\n");

	di->events.btemp_lowmed = true;
	di->events.btemp_medhigh = false;
	di->events.btemp_high = false;
	di->events.btemp_low = false;
	power_supply_changed(di->btemp_psy);

	return IRQ_HANDLED;
}

 
static irqreturn_t ab8500_btemp_medhigh_handler(int irq, void *_di)
{
	struct ab8500_btemp *di = _di;

	dev_dbg(di->dev, "Battery temperature is between medium and high\n");

	di->events.btemp_medhigh = true;
	di->events.btemp_lowmed = false;
	di->events.btemp_high = false;
	di->events.btemp_low = false;
	power_supply_changed(di->btemp_psy);

	return IRQ_HANDLED;
}

 
static void ab8500_btemp_periodic(struct ab8500_btemp *di,
	bool enable)
{
	dev_dbg(di->dev, "Enable periodic temperature measurements: %d\n",
		enable);
	 
	cancel_delayed_work_sync(&di->btemp_periodic_work);

	if (enable)
		queue_delayed_work(di->btemp_wq, &di->btemp_periodic_work, 0);
}

 
static int ab8500_btemp_get_temp(struct ab8500_btemp *di)
{
	int temp = 0;

	 
	if (is_ab8500_3p3_or_earlier(di->parent)) {
		temp = di->bat_temp * 10;
	} else {
		if (di->events.btemp_low) {
			if (temp > di->btemp_ranges.btemp_low_limit)
				temp = di->btemp_ranges.btemp_low_limit * 10;
			else
				temp = di->bat_temp * 10;
		} else if (di->events.btemp_high) {
			if (temp < di->btemp_ranges.btemp_high_limit)
				temp = di->btemp_ranges.btemp_high_limit * 10;
			else
				temp = di->bat_temp * 10;
		} else if (di->events.btemp_lowmed) {
			if (temp > di->btemp_ranges.btemp_med_limit)
				temp = di->btemp_ranges.btemp_med_limit * 10;
			else
				temp = di->bat_temp * 10;
		} else if (di->events.btemp_medhigh) {
			if (temp < di->btemp_ranges.btemp_med_limit)
				temp = di->btemp_ranges.btemp_med_limit * 10;
			else
				temp = di->bat_temp * 10;
		} else
			temp = di->bat_temp * 10;
	}
	return temp;
}

 
static int ab8500_btemp_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	struct ab8500_btemp *di = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		if (di->events.batt_rem)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = ab8500_btemp_get_temp(di);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int ab8500_btemp_get_ext_psy_data(struct device *dev, void *data)
{
	struct power_supply *psy;
	struct power_supply *ext = dev_get_drvdata(dev);
	const char **supplicants = (const char **)ext->supplied_to;
	struct ab8500_btemp *di;
	union power_supply_propval ret;
	int j;

	psy = (struct power_supply *)data;
	di = power_supply_get_drvdata(psy);

	 
	j = match_string(supplicants, ext->num_supplicants, psy->desc->name);
	if (j < 0)
		return 0;

	 
	for (j = 0; j < ext->desc->num_properties; j++) {
		enum power_supply_property prop;
		prop = ext->desc->properties[j];

		if (power_supply_get_property(ext, prop, &ret))
			continue;

		switch (prop) {
		case POWER_SUPPLY_PROP_PRESENT:
			switch (ext->desc->type) {
			case POWER_SUPPLY_TYPE_MAINS:
				 
				if (!ret.intval && di->events.ac_conn) {
					di->events.ac_conn = false;
				}
				 
				else if (ret.intval && !di->events.ac_conn) {
					di->events.ac_conn = true;
					if (!di->events.usb_conn)
						ab8500_btemp_periodic(di, true);
				}
				break;
			case POWER_SUPPLY_TYPE_USB:
				 
				if (!ret.intval && di->events.usb_conn) {
					di->events.usb_conn = false;
				}
				 
				else if (ret.intval && !di->events.usb_conn) {
					di->events.usb_conn = true;
					if (!di->events.ac_conn)
						ab8500_btemp_periodic(di, true);
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	return 0;
}

 
static void ab8500_btemp_external_power_changed(struct power_supply *psy)
{
	class_for_each_device(power_supply_class, NULL, psy,
			      ab8500_btemp_get_ext_psy_data);
}

 
static struct ab8500_btemp_interrupts ab8500_btemp_irq[] = {
	{"BAT_CTRL_INDB", ab8500_btemp_batctrlindb_handler},
	{"BTEMP_LOW", ab8500_btemp_templow_handler},
	{"BTEMP_HIGH", ab8500_btemp_temphigh_handler},
	{"BTEMP_LOW_MEDIUM", ab8500_btemp_lowmed_handler},
	{"BTEMP_MEDIUM_HIGH", ab8500_btemp_medhigh_handler},
};

static int __maybe_unused ab8500_btemp_resume(struct device *dev)
{
	struct ab8500_btemp *di = dev_get_drvdata(dev);

	ab8500_btemp_periodic(di, true);

	return 0;
}

static int __maybe_unused ab8500_btemp_suspend(struct device *dev)
{
	struct ab8500_btemp *di = dev_get_drvdata(dev);

	ab8500_btemp_periodic(di, false);

	return 0;
}

static char *supply_interface[] = {
	"ab8500_chargalg",
	"ab8500_fg",
};

static const struct power_supply_desc ab8500_btemp_desc = {
	.name			= "ab8500_btemp",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= ab8500_btemp_props,
	.num_properties		= ARRAY_SIZE(ab8500_btemp_props),
	.get_property		= ab8500_btemp_get_property,
	.external_power_changed	= ab8500_btemp_external_power_changed,
};

static int ab8500_btemp_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct ab8500_btemp *di = dev_get_drvdata(dev);

	 
	di->btemp_wq =
		alloc_workqueue("ab8500_btemp_wq", WQ_MEM_RECLAIM, 0);
	if (di->btemp_wq == NULL) {
		dev_err(dev, "failed to create work queue\n");
		return -ENOMEM;
	}

	 
	ab8500_btemp_periodic(di, true);

	return 0;
}

static void ab8500_btemp_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct ab8500_btemp *di = dev_get_drvdata(dev);

	 
	destroy_workqueue(di->btemp_wq);
}

static const struct component_ops ab8500_btemp_component_ops = {
	.bind = ab8500_btemp_bind,
	.unbind = ab8500_btemp_unbind,
};

static int ab8500_btemp_probe(struct platform_device *pdev)
{
	struct power_supply_config psy_cfg = {};
	struct device *dev = &pdev->dev;
	struct ab8500_btemp *di;
	int irq, i, ret = 0;
	u8 val;

	di = devm_kzalloc(dev, sizeof(*di), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	di->bm = &ab8500_bm_data;

	 
	di->dev = dev;
	di->parent = dev_get_drvdata(pdev->dev.parent);

	 
	di->tz = thermal_zone_get_zone_by_name("battery-thermal");
	if (IS_ERR(di->tz)) {
		ret = PTR_ERR(di->tz);
		 
		if (ret == -ENODEV)
			ret = -EPROBE_DEFER;
		return dev_err_probe(dev, ret,
				     "failed to get battery thermal zone\n");
	}
	di->bat_ctrl = devm_iio_channel_get(dev, "bat_ctrl");
	if (IS_ERR(di->bat_ctrl)) {
		ret = dev_err_probe(dev, PTR_ERR(di->bat_ctrl),
				    "failed to get BAT CTRL ADC channel\n");
		return ret;
	}

	di->initialized = false;

	psy_cfg.supplied_to = supply_interface;
	psy_cfg.num_supplicants = ARRAY_SIZE(supply_interface);
	psy_cfg.drv_data = di;

	 
	INIT_DEFERRABLE_WORK(&di->btemp_periodic_work,
		ab8500_btemp_periodic_work);

	 
	di->btemp_ranges.btemp_low_limit = BTEMP_THERMAL_LOW_LIMIT;
	di->btemp_ranges.btemp_med_limit = BTEMP_THERMAL_MED_LIMIT;

	ret = abx500_get_register_interruptible(dev, AB8500_CHARGER,
		AB8500_BTEMP_HIGH_TH, &val);
	if (ret < 0) {
		dev_err(dev, "%s ab8500 read failed\n", __func__);
		return ret;
	}
	switch (val) {
	case BTEMP_HIGH_TH_57_0:
	case BTEMP_HIGH_TH_57_1:
		di->btemp_ranges.btemp_high_limit =
			BTEMP_THERMAL_HIGH_LIMIT_57;
		break;
	case BTEMP_HIGH_TH_52:
		di->btemp_ranges.btemp_high_limit =
			BTEMP_THERMAL_HIGH_LIMIT_52;
		break;
	case BTEMP_HIGH_TH_62:
		di->btemp_ranges.btemp_high_limit =
			BTEMP_THERMAL_HIGH_LIMIT_62;
		break;
	}

	 
	di->btemp_psy = devm_power_supply_register(dev, &ab8500_btemp_desc,
						   &psy_cfg);
	if (IS_ERR(di->btemp_psy)) {
		dev_err(dev, "failed to register BTEMP psy\n");
		return PTR_ERR(di->btemp_psy);
	}

	 
	for (i = 0; i < ARRAY_SIZE(ab8500_btemp_irq); i++) {
		irq = platform_get_irq_byname(pdev, ab8500_btemp_irq[i].name);
		if (irq < 0)
			return irq;

		ret = devm_request_threaded_irq(dev, irq, NULL,
			ab8500_btemp_irq[i].isr,
			IRQF_SHARED | IRQF_NO_SUSPEND | IRQF_ONESHOT,
			ab8500_btemp_irq[i].name, di);

		if (ret) {
			dev_err(dev, "failed to request %s IRQ %d: %d\n"
				, ab8500_btemp_irq[i].name, irq, ret);
			return ret;
		}
		dev_dbg(dev, "Requested %s IRQ %d: %d\n",
			ab8500_btemp_irq[i].name, irq, ret);
	}

	platform_set_drvdata(pdev, di);

	list_add_tail(&di->node, &ab8500_btemp_list);

	return component_add(dev, &ab8500_btemp_component_ops);
}

static int ab8500_btemp_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &ab8500_btemp_component_ops);

	return 0;
}

static SIMPLE_DEV_PM_OPS(ab8500_btemp_pm_ops, ab8500_btemp_suspend, ab8500_btemp_resume);

static const struct of_device_id ab8500_btemp_match[] = {
	{ .compatible = "stericsson,ab8500-btemp", },
	{ },
};
MODULE_DEVICE_TABLE(of, ab8500_btemp_match);

struct platform_driver ab8500_btemp_driver = {
	.probe = ab8500_btemp_probe,
	.remove = ab8500_btemp_remove,
	.driver = {
		.name = "ab8500-btemp",
		.of_match_table = ab8500_btemp_match,
		.pm = &ab8500_btemp_pm_ops,
	},
};
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Johan Palsson, Karl Komierowski, Arun R Murthy");
MODULE_ALIAS("platform:ab8500-btemp");
MODULE_DESCRIPTION("AB8500 battery temperature driver");
