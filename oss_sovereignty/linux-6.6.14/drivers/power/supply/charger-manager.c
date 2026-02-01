
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/power/charger-manager.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/thermal.h>

static struct {
	const char *name;
	u64 extcon_type;
} extcon_mapping[] = {
	 
	{ "USB", EXTCON_USB },
	{ "USB-HOST", EXTCON_USB_HOST },
	{ "SDP", EXTCON_CHG_USB_SDP },
	{ "DCP", EXTCON_CHG_USB_DCP },
	{ "CDP", EXTCON_CHG_USB_CDP },
	{ "ACA", EXTCON_CHG_USB_ACA },
	{ "FAST-CHARGER", EXTCON_CHG_USB_FAST },
	{ "SLOW-CHARGER", EXTCON_CHG_USB_SLOW },
	{ "WPT", EXTCON_CHG_WPT },
	{ "PD", EXTCON_CHG_USB_PD },
	{ "DOCK", EXTCON_DOCK },
	{ "JIG", EXTCON_JIG },
	{ "MECHANICAL", EXTCON_MECHANICAL },
	 
	{ "TA", EXTCON_CHG_USB_SDP },
	{ "CHARGE-DOWNSTREAM", EXTCON_CHG_USB_CDP },
};

 
#define CM_DEFAULT_RECHARGE_TEMP_DIFF	50
#define CM_DEFAULT_CHARGE_TEMP_MAX	500

 
#define	CM_JIFFIES_SMALL	(2)

 
#define CM_MIN_VALID(x, y)	x = (((y > 0) && ((x) > (y))) ? (y) : (x))

 
#define CM_RTC_SMALL		(2)

static LIST_HEAD(cm_list);
static DEFINE_MUTEX(cm_list_mtx);

 
static struct alarm *cm_timer;

static bool cm_suspended;
static bool cm_timer_set;
static unsigned long cm_suspend_duration_ms;

 
static unsigned long polling_jiffy = ULONG_MAX;  
static unsigned long next_polling;  
static struct workqueue_struct *cm_wq;  
static struct delayed_work cm_monitor_work;  

 
static bool is_batt_present(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool present = false;
	int i, ret;

	switch (cm->desc->battery_present) {
	case CM_BATTERY_PRESENT:
		present = true;
		break;
	case CM_NO_BATTERY:
		break;
	case CM_FUEL_GAUGE:
		psy = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!psy)
			break;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
				&val);
		if (ret == 0 && val.intval)
			present = true;
		power_supply_put(psy);
		break;
	case CM_CHARGER_STAT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_PRESENT, &val);
			power_supply_put(psy);
			if (ret == 0 && val.intval) {
				present = true;
				break;
			}
		}
		break;
	}

	return present;
}

 
static bool is_ext_pwr_online(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool online = false;
	int i, ret;

	 
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE,
				&val);
		power_supply_put(psy);
		if (ret == 0 && val.intval) {
			online = true;
			break;
		}
	}

	return online;
}

 
static int get_batt_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uV = val.intval;
	return 0;
}

 
static bool is_charging(struct charger_manager *cm)
{
	int i, ret;
	bool charging = false;
	struct power_supply *psy;
	union power_supply_propval val;

	 
	if (!is_batt_present(cm))
		return false;

	 
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		 
		if (cm->emergency_stop)
			continue;
		if (!cm->charger_enabled)
			continue;

		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}

		 
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE,
				&val);
		if (ret) {
			dev_warn(cm->dev, "Cannot read ONLINE value from %s\n",
				 cm->desc->psy_charger_stat[i]);
			power_supply_put(psy);
			continue;
		}
		if (val.intval == 0) {
			power_supply_put(psy);
			continue;
		}

		 
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS,
				&val);
		power_supply_put(psy);
		if (ret) {
			dev_warn(cm->dev, "Cannot read STATUS value from %s\n",
				 cm->desc->psy_charger_stat[i]);
			continue;
		}
		if (val.intval == POWER_SUPPLY_STATUS_FULL ||
				val.intval == POWER_SUPPLY_STATUS_DISCHARGING ||
				val.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
			continue;

		 
		charging = true;
		break;
	}

	return charging;
}

 
static bool is_full_charged(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	bool is_full = false;
	int ret = 0;
	int uV;

	 
	if (!is_batt_present(cm))
		return false;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return false;

	 
	if (desc->fullbatt_uV > 0) {
		ret = get_batt_uV(cm, &uV);
		if (!ret) {
			 
			if (cm->battery_status == POWER_SUPPLY_STATUS_FULL
					&& desc->fullbatt_vchkdrop_uV)
				uV += desc->fullbatt_vchkdrop_uV;
			if (uV >= desc->fullbatt_uV)
				return true;
		}
	}

	if (desc->fullbatt_full_capacity > 0) {
		val.intval = 0;

		 
		ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CHARGE_FULL, &val);
		if (!ret && val.intval > desc->fullbatt_full_capacity) {
			is_full = true;
			goto out;
		}
	}

	 
	if (desc->fullbatt_soc > 0) {
		val.intval = 0;

		ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CAPACITY, &val);
		if (!ret && val.intval >= desc->fullbatt_soc) {
			is_full = true;
			goto out;
		}
	}

out:
	power_supply_put(fuel_gauge);
	return is_full;
}

 
static bool is_polling_required(struct charger_manager *cm)
{
	switch (cm->desc->polling_mode) {
	case CM_POLL_DISABLE:
		return false;
	case CM_POLL_ALWAYS:
		return true;
	case CM_POLL_EXTERNAL_POWER_ONLY:
		return is_ext_pwr_online(cm);
	case CM_POLL_CHARGING_ONLY:
		return is_charging(cm);
	default:
		dev_warn(cm->dev, "Incorrect polling_mode (%d)\n",
			 cm->desc->polling_mode);
	}

	return false;
}

 
static int try_charger_enable(struct charger_manager *cm, bool enable)
{
	int err = 0, i;
	struct charger_desc *desc = cm->desc;

	 
	if (enable == cm->charger_enabled)
		return 0;

	if (enable) {
		if (cm->emergency_stop)
			return -EAGAIN;

		 
		cm->charging_start_time = ktime_to_ms(ktime_get());
		cm->charging_end_time = 0;

		for (i = 0 ; i < desc->num_charger_regulators ; i++) {
			if (desc->charger_regulators[i].externally_control)
				continue;

			err = regulator_enable(desc->charger_regulators[i].consumer);
			if (err < 0) {
				dev_warn(cm->dev, "Cannot enable %s regulator\n",
					 desc->charger_regulators[i].regulator_name);
			}
		}
	} else {
		 
		cm->charging_start_time = 0;
		cm->charging_end_time = ktime_to_ms(ktime_get());

		for (i = 0 ; i < desc->num_charger_regulators ; i++) {
			if (desc->charger_regulators[i].externally_control)
				continue;

			err = regulator_disable(desc->charger_regulators[i].consumer);
			if (err < 0) {
				dev_warn(cm->dev, "Cannot disable %s regulator\n",
					 desc->charger_regulators[i].regulator_name);
			}
		}

		 
		for (i = 0; i < desc->num_charger_regulators; i++) {
			if (regulator_is_enabled(
				    desc->charger_regulators[i].consumer)) {
				regulator_force_disable(
					desc->charger_regulators[i].consumer);
				dev_warn(cm->dev, "Disable regulator(%s) forcibly\n",
					 desc->charger_regulators[i].regulator_name);
			}
		}
	}

	if (!err)
		cm->charger_enabled = enable;

	return err;
}

 
static int check_charging_duration(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	u64 curr = ktime_to_ms(ktime_get());
	u64 duration;
	int ret = false;

	if (!desc->charging_max_duration_ms &&
			!desc->discharging_max_duration_ms)
		return ret;

	if (cm->charger_enabled) {
		duration = curr - cm->charging_start_time;

		if (duration > desc->charging_max_duration_ms) {
			dev_info(cm->dev, "Charging duration exceed %ums\n",
				 desc->charging_max_duration_ms);
			ret = true;
		}
	} else if (cm->battery_status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
		duration = curr - cm->charging_end_time;

		if (duration > desc->discharging_max_duration_ms) {
			dev_info(cm->dev, "Discharging duration exceed %ums\n",
				 desc->discharging_max_duration_ms);
			ret = true;
		}
	}

	return ret;
}

static int cm_get_battery_temperature_by_psy(struct charger_manager *cm,
					int *temp)
{
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_TEMP,
				(union power_supply_propval *)temp);
	power_supply_put(fuel_gauge);

	return ret;
}

static int cm_get_battery_temperature(struct charger_manager *cm,
					int *temp)
{
	int ret;

	if (!cm->desc->measure_battery_temp)
		return -ENODEV;

#ifdef CONFIG_THERMAL
	if (cm->tzd_batt) {
		ret = thermal_zone_get_temp(cm->tzd_batt, temp);
		if (!ret)
			 
			*temp /= 100;
	} else
#endif
	{
		 
		ret = cm_get_battery_temperature_by_psy(cm, temp);
	}

	return ret;
}

static int cm_check_thermal_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int temp, upper_limit, lower_limit;
	int ret = 0;

	ret = cm_get_battery_temperature(cm, &temp);
	if (ret) {
		 
		dev_err(cm->dev, "Failed to get battery temperature\n");
		return 0;
	}

	upper_limit = desc->temp_max;
	lower_limit = desc->temp_min;

	if (cm->emergency_stop) {
		upper_limit -= desc->temp_diff;
		lower_limit += desc->temp_diff;
	}

	if (temp > upper_limit)
		ret = CM_BATT_OVERHEAT;
	else if (temp < lower_limit)
		ret = CM_BATT_COLD;
	else
		ret = CM_BATT_OK;

	cm->emergency_stop = ret;

	return ret;
}

 
static int cm_get_target_status(struct charger_manager *cm)
{
	if (!is_ext_pwr_online(cm))
		return POWER_SUPPLY_STATUS_DISCHARGING;

	if (cm_check_thermal_status(cm)) {
		 
		if (check_charging_duration(cm))
			goto charging_ok;
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	switch (cm->battery_status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		 
		if (check_charging_duration(cm))
			return POWER_SUPPLY_STATUS_FULL;
		fallthrough;
	case POWER_SUPPLY_STATUS_FULL:
		if (is_full_charged(cm))
			return POWER_SUPPLY_STATUS_FULL;
		fallthrough;
	default:
		break;
	}

charging_ok:
	 
	return POWER_SUPPLY_STATUS_CHARGING;
}

 
static bool _cm_monitor(struct charger_manager *cm)
{
	int target;

	target = cm_get_target_status(cm);

	try_charger_enable(cm, (target == POWER_SUPPLY_STATUS_CHARGING));

	if (cm->battery_status != target) {
		cm->battery_status = target;
		power_supply_changed(cm->charger_psy);
	}

	return (cm->battery_status == POWER_SUPPLY_STATUS_NOT_CHARGING);
}

 
static bool cm_monitor(void)
{
	bool stop = false;
	struct charger_manager *cm;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (_cm_monitor(cm))
			stop = true;
	}

	mutex_unlock(&cm_list_mtx);

	return stop;
}

 
static void _setup_polling(struct work_struct *work)
{
	unsigned long min = ULONG_MAX;
	struct charger_manager *cm;
	bool keep_polling = false;
	unsigned long _next_polling;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (is_polling_required(cm) && cm->desc->polling_interval_ms) {
			keep_polling = true;

			if (min > cm->desc->polling_interval_ms)
				min = cm->desc->polling_interval_ms;
		}
	}

	polling_jiffy = msecs_to_jiffies(min);
	if (polling_jiffy <= CM_JIFFIES_SMALL)
		polling_jiffy = CM_JIFFIES_SMALL + 1;

	if (!keep_polling)
		polling_jiffy = ULONG_MAX;
	if (polling_jiffy == ULONG_MAX)
		goto out;

	WARN(cm_wq == NULL, "charger-manager: workqueue not initialized"
			    ". try it later. %s\n", __func__);

	 
	_next_polling = jiffies + polling_jiffy;

	if (time_before(_next_polling, next_polling)) {
		mod_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy);
		next_polling = _next_polling;
	} else {
		if (queue_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy))
			next_polling = _next_polling;
	}
out:
	mutex_unlock(&cm_list_mtx);
}
static DECLARE_WORK(setup_polling, _setup_polling);

 
static void cm_monitor_poller(struct work_struct *work)
{
	cm_monitor();
	schedule_work(&setup_polling);
}

static int charger_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	struct charger_desc *desc = cm->desc;
	struct power_supply *fuel_gauge = NULL;
	int ret = 0;
	int uV;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = cm->battery_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (cm->emergency_stop == CM_BATT_OVERHEAT)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (cm->emergency_stop == CM_BATT_COLD)
			val->intval = POWER_SUPPLY_HEALTH_COLD;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (is_batt_present(cm))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = get_batt_uV(cm, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!fuel_gauge) {
			ret = -ENODEV;
			break;
		}
		ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CURRENT_NOW, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		return cm_get_battery_temperature(cm, &val->intval);
	case POWER_SUPPLY_PROP_CAPACITY:
		if (!is_batt_present(cm)) {
			 
			val->intval = 100;
			break;
		}

		fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!fuel_gauge) {
			ret = -ENODEV;
			break;
		}

		ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_CAPACITY, val);
		if (ret)
			break;

		if (val->intval > 100) {
			val->intval = 100;
			break;
		}
		if (val->intval < 0)
			val->intval = 0;

		 
		if (is_charging(cm))
			break;

		 
		ret = get_batt_uV(cm, &uV);
		if (ret) {
			 
			ret = 0;
			break;
		}

		if (desc->fullbatt_uV > 0 && uV >= desc->fullbatt_uV &&
		    !is_charging(cm)) {
			val->intval = 100;
			break;
		}

		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (is_ext_pwr_online(cm))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!fuel_gauge) {
			ret = -ENODEV;
			break;
		}
		ret = power_supply_get_property(fuel_gauge, psp, val);
		break;
	default:
		return -EINVAL;
	}
	if (fuel_gauge)
		power_supply_put(fuel_gauge);
	return ret;
}

#define NUM_CHARGER_PSY_OPTIONAL	(4)
static enum power_supply_property default_charger_props[] = {
	 
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_ONLINE,
	 
};

static const struct power_supply_desc psy_default = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = default_charger_props,
	.num_properties = ARRAY_SIZE(default_charger_props),
	.get_property = charger_get_property,
	.no_thermal = true,
};

 
static bool cm_setup_timer(void)
{
	struct charger_manager *cm;
	unsigned int wakeup_ms = UINT_MAX;
	int timer_req = 0;

	if (time_after(next_polling, jiffies))
		CM_MIN_VALID(wakeup_ms,
			jiffies_to_msecs(next_polling - jiffies));

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		 
		if (!is_polling_required(cm) && !cm->emergency_stop)
			continue;
		timer_req++;
		if (cm->desc->polling_interval_ms == 0)
			continue;
		CM_MIN_VALID(wakeup_ms, cm->desc->polling_interval_ms);
	}
	mutex_unlock(&cm_list_mtx);

	if (timer_req && cm_timer) {
		ktime_t now, add;

		 
		if (wakeup_ms == UINT_MAX ||
			wakeup_ms < CM_RTC_SMALL * MSEC_PER_SEC)
			wakeup_ms = 2 * CM_RTC_SMALL * MSEC_PER_SEC;

		pr_info("Charger Manager wakeup timer: %u ms\n", wakeup_ms);

		now = ktime_get_boottime();
		add = ktime_set(wakeup_ms / MSEC_PER_SEC,
				(wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
		alarm_start(cm_timer, ktime_add(now, add));

		cm_suspend_duration_ms = wakeup_ms;

		return true;
	}
	return false;
}

 
static void charger_extcon_work(struct work_struct *work)
{
	struct charger_cable *cable =
			container_of(work, struct charger_cable, wq);
	int ret;

	if (cable->attached && cable->min_uA != 0 && cable->max_uA != 0) {
		ret = regulator_set_current_limit(cable->charger->consumer,
					cable->min_uA, cable->max_uA);
		if (ret < 0) {
			pr_err("Cannot set current limit of %s (%s)\n",
			       cable->charger->regulator_name, cable->name);
			return;
		}

		pr_info("Set current limit of %s : %duA ~ %duA\n",
			cable->charger->regulator_name,
			cable->min_uA, cable->max_uA);
	}

	cancel_delayed_work(&cm_monitor_work);
	queue_delayed_work(cm_wq, &cm_monitor_work, 0);
}

 
static int charger_extcon_notifier(struct notifier_block *self,
			unsigned long event, void *ptr)
{
	struct charger_cable *cable =
		container_of(self, struct charger_cable, nb);

	 
	cable->attached = event;

	 
	schedule_work(&cable->wq);

	return NOTIFY_DONE;
}

 
static int charger_extcon_init(struct charger_manager *cm,
		struct charger_cable *cable)
{
	int ret, i;
	u64 extcon_type = EXTCON_NONE;

	 
	INIT_WORK(&cable->wq, charger_extcon_work);
	cable->nb.notifier_call = charger_extcon_notifier;

	cable->extcon_dev = extcon_get_extcon_dev(cable->extcon_name);
	if (IS_ERR(cable->extcon_dev)) {
		pr_err("Cannot find extcon_dev for %s (cable: %s)\n",
			cable->extcon_name, cable->name);
		return PTR_ERR(cable->extcon_dev);
	}

	for (i = 0; i < ARRAY_SIZE(extcon_mapping); i++) {
		if (!strcmp(cable->name, extcon_mapping[i].name)) {
			extcon_type = extcon_mapping[i].extcon_type;
			break;
		}
	}
	if (extcon_type == EXTCON_NONE) {
		pr_err("Cannot find cable for type %s", cable->name);
		return -EINVAL;
	}

	cable->extcon_type = extcon_type;

	ret = devm_extcon_register_notifier(cm->dev, cable->extcon_dev,
		cable->extcon_type, &cable->nb);
	if (ret < 0) {
		pr_err("Cannot register extcon_dev for %s (cable: %s)\n",
			cable->extcon_name, cable->name);
		return ret;
	}

	return 0;
}

 
static int charger_manager_register_extcon(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct charger_regulator *charger;
	unsigned long event;
	int ret;
	int i;
	int j;

	for (i = 0; i < desc->num_charger_regulators; i++) {
		charger = &desc->charger_regulators[i];

		charger->consumer = regulator_get(cm->dev,
					charger->regulator_name);
		if (IS_ERR(charger->consumer)) {
			dev_err(cm->dev, "Cannot find charger(%s)\n",
				charger->regulator_name);
			return PTR_ERR(charger->consumer);
		}
		charger->cm = cm;

		for (j = 0; j < charger->num_cables; j++) {
			struct charger_cable *cable = &charger->cables[j];

			ret = charger_extcon_init(cm, cable);
			if (ret < 0) {
				dev_err(cm->dev, "Cannot initialize charger(%s)\n",
					charger->regulator_name);
				return ret;
			}
			cable->charger = charger;
			cable->cm = cm;

			event = extcon_get_state(cable->extcon_dev,
				cable->extcon_type);
			charger_extcon_notifier(&cable->nb,
				event, NULL);
		}
	}

	return 0;
}

 
static ssize_t charger_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator, attr_name);

	return sysfs_emit(buf, "%s\n", charger->regulator_name);
}

static ssize_t charger_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator, attr_state);
	int state = 0;

	if (!charger->externally_control)
		state = regulator_is_enabled(charger->consumer);

	return sysfs_emit(buf, "%s\n", state ? "enabled" : "disabled");
}

static ssize_t charger_externally_control_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger = container_of(attr,
			struct charger_regulator, attr_externally_control);

	return sysfs_emit(buf, "%d\n", charger->externally_control);
}

static ssize_t charger_externally_control_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
					attr_externally_control);
	struct charger_manager *cm = charger->cm;
	struct charger_desc *desc = cm->desc;
	int i;
	int ret;
	int externally_control;
	int chargers_externally_control = 1;

	ret = sscanf(buf, "%d", &externally_control);
	if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}

	if (!externally_control) {
		charger->externally_control = 0;
		return count;
	}

	for (i = 0; i < desc->num_charger_regulators; i++) {
		if (&desc->charger_regulators[i] != charger &&
			!desc->charger_regulators[i].externally_control) {
			 
			chargers_externally_control = 0;
			break;
		}
	}

	if (!chargers_externally_control) {
		if (cm->charger_enabled) {
			try_charger_enable(charger->cm, false);
			charger->externally_control = externally_control;
			try_charger_enable(charger->cm, true);
		} else {
			charger->externally_control = externally_control;
		}
	} else {
		dev_warn(cm->dev,
			 "'%s' regulator should be controlled in charger-manager because charger-manager must need at least one charger for charging\n",
			 charger->regulator_name);
	}

	return count;
}

 
static int charger_manager_prepare_sysfs(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct charger_regulator *charger;
	int chargers_externally_control = 1;
	char *name;
	int i;

	 
	for (i = 0; i < desc->num_charger_regulators; i++) {
		charger = &desc->charger_regulators[i];

		name = devm_kasprintf(cm->dev, GFP_KERNEL, "charger.%d", i);
		if (!name)
			return -ENOMEM;

		charger->attrs[0] = &charger->attr_name.attr;
		charger->attrs[1] = &charger->attr_state.attr;
		charger->attrs[2] = &charger->attr_externally_control.attr;
		charger->attrs[3] = NULL;

		charger->attr_grp.name = name;
		charger->attr_grp.attrs = charger->attrs;
		desc->sysfs_groups[i] = &charger->attr_grp;

		sysfs_attr_init(&charger->attr_name.attr);
		charger->attr_name.attr.name = "name";
		charger->attr_name.attr.mode = 0444;
		charger->attr_name.show = charger_name_show;

		sysfs_attr_init(&charger->attr_state.attr);
		charger->attr_state.attr.name = "state";
		charger->attr_state.attr.mode = 0444;
		charger->attr_state.show = charger_state_show;

		sysfs_attr_init(&charger->attr_externally_control.attr);
		charger->attr_externally_control.attr.name
				= "externally_control";
		charger->attr_externally_control.attr.mode = 0644;
		charger->attr_externally_control.show
				= charger_externally_control_show;
		charger->attr_externally_control.store
				= charger_externally_control_store;

		if (!desc->charger_regulators[i].externally_control ||
				!chargers_externally_control)
			chargers_externally_control = 0;

		dev_info(cm->dev, "'%s' regulator's externally_control is %d\n",
			 charger->regulator_name, charger->externally_control);
	}

	if (chargers_externally_control) {
		dev_err(cm->dev, "Cannot register regulator because charger-manager must need at least one charger for charging battery\n");
		return -EINVAL;
	}

	return 0;
}

static int cm_init_thermal_data(struct charger_manager *cm,
		struct power_supply *fuel_gauge,
		enum power_supply_property *properties,
		size_t *num_properties)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	int ret;

	 
	ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_TEMP, &val);

	if (!ret) {
		properties[*num_properties] = POWER_SUPPLY_PROP_TEMP;
		(*num_properties)++;
		cm->desc->measure_battery_temp = true;
	}
#ifdef CONFIG_THERMAL
	if (ret && desc->thermal_zone) {
		cm->tzd_batt =
			thermal_zone_get_zone_by_name(desc->thermal_zone);
		if (IS_ERR(cm->tzd_batt))
			return PTR_ERR(cm->tzd_batt);

		 
		properties[*num_properties] = POWER_SUPPLY_PROP_TEMP;
		(*num_properties)++;
		cm->desc->measure_battery_temp = true;
		ret = 0;
	}
#endif
	if (cm->desc->measure_battery_temp) {
		 
		if (!desc->temp_max)
			desc->temp_max = CM_DEFAULT_CHARGE_TEMP_MAX;
		if (!desc->temp_diff)
			desc->temp_diff = CM_DEFAULT_RECHARGE_TEMP_DIFF;
	}

	return ret;
}

static const struct of_device_id charger_manager_match[] = {
	{
		.compatible = "charger-manager",
	},
	{},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static struct charger_desc *of_cm_parse_desc(struct device *dev)
{
	struct charger_desc *desc;
	struct device_node *np = dev->of_node;
	u32 poll_mode = CM_POLL_DISABLE;
	u32 battery_stat = CM_NO_BATTERY;
	int num_chgs = 0;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "cm-name", &desc->psy_name);

	of_property_read_u32(np, "cm-poll-mode", &poll_mode);
	desc->polling_mode = poll_mode;

	of_property_read_u32(np, "cm-poll-interval",
				&desc->polling_interval_ms);

	of_property_read_u32(np, "cm-fullbatt-vchkdrop-volt",
					&desc->fullbatt_vchkdrop_uV);
	of_property_read_u32(np, "cm-fullbatt-voltage", &desc->fullbatt_uV);
	of_property_read_u32(np, "cm-fullbatt-soc", &desc->fullbatt_soc);
	of_property_read_u32(np, "cm-fullbatt-capacity",
					&desc->fullbatt_full_capacity);

	of_property_read_u32(np, "cm-battery-stat", &battery_stat);
	desc->battery_present = battery_stat;

	 
	num_chgs = of_property_count_strings(np, "cm-chargers");
	if (num_chgs > 0) {
		int i;

		 
		desc->psy_charger_stat = devm_kcalloc(dev,
						      num_chgs + 1,
						      sizeof(char *),
						      GFP_KERNEL);
		if (!desc->psy_charger_stat)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < num_chgs; i++)
			of_property_read_string_index(np, "cm-chargers",
						      i, &desc->psy_charger_stat[i]);
	}

	of_property_read_string(np, "cm-fuel-gauge", &desc->psy_fuel_gauge);

	of_property_read_string(np, "cm-thermal-zone", &desc->thermal_zone);

	of_property_read_u32(np, "cm-battery-cold", &desc->temp_min);
	if (of_property_read_bool(np, "cm-battery-cold-in-minus"))
		desc->temp_min *= -1;
	of_property_read_u32(np, "cm-battery-hot", &desc->temp_max);
	of_property_read_u32(np, "cm-battery-temp-diff", &desc->temp_diff);

	of_property_read_u32(np, "cm-charging-max",
				&desc->charging_max_duration_ms);
	of_property_read_u32(np, "cm-discharging-max",
				&desc->discharging_max_duration_ms);

	 
	desc->num_charger_regulators = of_get_child_count(np);
	if (desc->num_charger_regulators) {
		struct charger_regulator *chg_regs;
		struct device_node *child;

		chg_regs = devm_kcalloc(dev,
					desc->num_charger_regulators,
					sizeof(*chg_regs),
					GFP_KERNEL);
		if (!chg_regs)
			return ERR_PTR(-ENOMEM);

		desc->charger_regulators = chg_regs;

		desc->sysfs_groups = devm_kcalloc(dev,
					desc->num_charger_regulators + 1,
					sizeof(*desc->sysfs_groups),
					GFP_KERNEL);
		if (!desc->sysfs_groups)
			return ERR_PTR(-ENOMEM);

		for_each_child_of_node(np, child) {
			struct charger_cable *cables;
			struct device_node *_child;

			of_property_read_string(child, "cm-regulator-name",
					&chg_regs->regulator_name);

			 
			chg_regs->num_cables = of_get_child_count(child);
			if (chg_regs->num_cables) {
				cables = devm_kcalloc(dev,
						      chg_regs->num_cables,
						      sizeof(*cables),
						      GFP_KERNEL);
				if (!cables) {
					of_node_put(child);
					return ERR_PTR(-ENOMEM);
				}

				chg_regs->cables = cables;

				for_each_child_of_node(child, _child) {
					of_property_read_string(_child,
					"cm-cable-name", &cables->name);
					of_property_read_string(_child,
					"cm-cable-extcon",
					&cables->extcon_name);
					of_property_read_u32(_child,
					"cm-cable-min",
					&cables->min_uA);
					of_property_read_u32(_child,
					"cm-cable-max",
					&cables->max_uA);
					cables++;
				}
			}
			chg_regs++;
		}
	}
	return desc;
}

static inline struct charger_desc *cm_get_drv_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		return of_cm_parse_desc(&pdev->dev);
	return dev_get_platdata(&pdev->dev);
}

static enum alarmtimer_restart cm_timer_func(struct alarm *alarm, ktime_t now)
{
	cm_timer_set = false;
	return ALARMTIMER_NORESTART;
}

static int charger_manager_probe(struct platform_device *pdev)
{
	struct charger_desc *desc = cm_get_drv_data(pdev);
	struct charger_manager *cm;
	int ret, i = 0;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	enum power_supply_property *properties;
	size_t num_properties;
	struct power_supply_config psy_cfg = {};

	if (IS_ERR(desc)) {
		dev_err(&pdev->dev, "No platform data (desc) found\n");
		return PTR_ERR(desc);
	}

	cm = devm_kzalloc(&pdev->dev, sizeof(*cm), GFP_KERNEL);
	if (!cm)
		return -ENOMEM;

	 
	cm->dev = &pdev->dev;
	cm->desc = desc;
	psy_cfg.drv_data = cm;

	 
	if (alarmtimer_get_rtcdev()) {
		cm_timer = devm_kzalloc(cm->dev, sizeof(*cm_timer), GFP_KERNEL);
		if (!cm_timer)
			return -ENOMEM;
		alarm_init(cm_timer, ALARM_BOOTTIME, cm_timer_func);
	}

	 
	if (desc->fullbatt_uV == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery voltage threshold as it is not supplied\n");
	}
	if (!desc->fullbatt_vchkdrop_uV) {
		dev_info(&pdev->dev, "Disabling full-battery voltage drop checking mechanism as it is not supplied\n");
		desc->fullbatt_vchkdrop_uV = 0;
	}
	if (desc->fullbatt_soc == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery soc(state of charge) threshold as it is not supplied\n");
	}
	if (desc->fullbatt_full_capacity == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery full capacity threshold as it is not supplied\n");
	}

	if (!desc->charger_regulators || desc->num_charger_regulators < 1) {
		dev_err(&pdev->dev, "charger_regulators undefined\n");
		return -EINVAL;
	}

	if (!desc->psy_charger_stat || !desc->psy_charger_stat[0]) {
		dev_err(&pdev->dev, "No power supply defined\n");
		return -EINVAL;
	}

	if (!desc->psy_fuel_gauge) {
		dev_err(&pdev->dev, "No fuel gauge power supply defined\n");
		return -EINVAL;
	}

	 
	for (i = 0; desc->psy_charger_stat[i]; i++) {
		struct power_supply *psy;

		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			return -ENODEV;
		}
		power_supply_put(psy);
	}

	if (cm->desc->polling_mode != CM_POLL_DISABLE &&
	    (desc->polling_interval_ms == 0 ||
	     msecs_to_jiffies(desc->polling_interval_ms) <= CM_JIFFIES_SMALL)) {
		dev_err(&pdev->dev, "polling_interval_ms is too small\n");
		return -EINVAL;
	}

	if (!desc->charging_max_duration_ms ||
			!desc->discharging_max_duration_ms) {
		dev_info(&pdev->dev, "Cannot limit charging duration checking mechanism to prevent overcharge/overheat and control discharging duration\n");
		desc->charging_max_duration_ms = 0;
		desc->discharging_max_duration_ms = 0;
	}

	platform_set_drvdata(pdev, cm);

	memcpy(&cm->charger_psy_desc, &psy_default, sizeof(psy_default));

	if (!desc->psy_name)
		strncpy(cm->psy_name_buf, psy_default.name, PSY_NAME_MAX);
	else
		strncpy(cm->psy_name_buf, desc->psy_name, PSY_NAME_MAX);
	cm->charger_psy_desc.name = cm->psy_name_buf;

	 
	properties = devm_kcalloc(&pdev->dev,
			     ARRAY_SIZE(default_charger_props) +
				NUM_CHARGER_PSY_OPTIONAL,
			     sizeof(*properties), GFP_KERNEL);
	if (!properties)
		return -ENOMEM;

	memcpy(properties, default_charger_props,
		sizeof(enum power_supply_property) *
		ARRAY_SIZE(default_charger_props));
	num_properties = ARRAY_SIZE(default_charger_props);

	 
	fuel_gauge = power_supply_get_by_name(desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fuel_gauge);
		return -ENODEV;
	}
	if (!power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_CHARGE_FULL, &val)) {
		properties[num_properties] =
				POWER_SUPPLY_PROP_CHARGE_FULL;
		num_properties++;
	}
	if (!power_supply_get_property(fuel_gauge,
					  POWER_SUPPLY_PROP_CHARGE_NOW, &val)) {
		properties[num_properties] =
				POWER_SUPPLY_PROP_CHARGE_NOW;
		num_properties++;
	}
	if (!power_supply_get_property(fuel_gauge,
					  POWER_SUPPLY_PROP_CURRENT_NOW,
					  &val)) {
		properties[num_properties] =
				POWER_SUPPLY_PROP_CURRENT_NOW;
		num_properties++;
	}

	ret = cm_init_thermal_data(cm, fuel_gauge, properties, &num_properties);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize thermal data\n");
		cm->desc->measure_battery_temp = false;
	}
	power_supply_put(fuel_gauge);

	cm->charger_psy_desc.properties = properties;
	cm->charger_psy_desc.num_properties = num_properties;

	 
	ret = charger_manager_prepare_sysfs(cm);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Cannot prepare sysfs entry of regulators\n");
		return ret;
	}
	psy_cfg.attr_grp = desc->sysfs_groups;

	cm->charger_psy = power_supply_register(&pdev->dev,
						&cm->charger_psy_desc,
						&psy_cfg);
	if (IS_ERR(cm->charger_psy)) {
		dev_err(&pdev->dev, "Cannot register charger-manager with name \"%s\"\n",
			cm->charger_psy_desc.name);
		return PTR_ERR(cm->charger_psy);
	}

	 
	ret = charger_manager_register_extcon(cm);
	if (ret < 0) {
		dev_err(&pdev->dev, "Cannot initialize extcon device\n");
		goto err_reg_extcon;
	}

	 
	mutex_lock(&cm_list_mtx);
	list_add(&cm->entry, &cm_list);
	mutex_unlock(&cm_list_mtx);

	 
	device_init_wakeup(&pdev->dev, true);
	device_set_wakeup_capable(&pdev->dev, false);

	 
	cm_monitor();

	schedule_work(&setup_polling);

	return 0;

err_reg_extcon:
	for (i = 0; i < desc->num_charger_regulators; i++)
		regulator_put(desc->charger_regulators[i].consumer);

	power_supply_unregister(cm->charger_psy);

	return ret;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);
	struct charger_desc *desc = cm->desc;
	int i = 0;

	 
	mutex_lock(&cm_list_mtx);
	list_del(&cm->entry);
	mutex_unlock(&cm_list_mtx);

	cancel_work_sync(&setup_polling);
	cancel_delayed_work_sync(&cm_monitor_work);

	for (i = 0 ; i < desc->num_charger_regulators ; i++)
		regulator_put(desc->charger_regulators[i].consumer);

	power_supply_unregister(cm->charger_psy);

	try_charger_enable(cm, false);

	return 0;
}

static const struct platform_device_id charger_manager_id[] = {
	{ "charger-manager", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, charger_manager_id);

static int cm_suspend_noirq(struct device *dev)
{
	if (device_may_wakeup(dev)) {
		device_set_wakeup_capable(dev, false);
		return -EAGAIN;
	}

	return 0;
}

static bool cm_need_to_awake(void)
{
	struct charger_manager *cm;

	if (cm_timer)
		return false;

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		if (is_charging(cm)) {
			mutex_unlock(&cm_list_mtx);
			return true;
		}
	}
	mutex_unlock(&cm_list_mtx);

	return false;
}

static int cm_suspend_prepare(struct device *dev)
{
	if (cm_need_to_awake())
		return -EBUSY;

	if (!cm_suspended)
		cm_suspended = true;

	cm_timer_set = cm_setup_timer();

	if (cm_timer_set) {
		cancel_work_sync(&setup_polling);
		cancel_delayed_work_sync(&cm_monitor_work);
	}

	return 0;
}

static void cm_suspend_complete(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (cm_suspended)
		cm_suspended = false;

	if (cm_timer_set) {
		ktime_t remain;

		alarm_cancel(cm_timer);
		cm_timer_set = false;
		remain = alarm_expires_remaining(cm_timer);
		cm_suspend_duration_ms -= ktime_to_ms(remain);
		schedule_work(&setup_polling);
	}

	_cm_monitor(cm);

	device_set_wakeup_capable(cm->dev, false);
}

static const struct dev_pm_ops charger_manager_pm = {
	.prepare	= cm_suspend_prepare,
	.suspend_noirq	= cm_suspend_noirq,
	.complete	= cm_suspend_complete,
};

static struct platform_driver charger_manager_driver = {
	.driver = {
		.name = "charger-manager",
		.pm = &charger_manager_pm,
		.of_match_table = charger_manager_match,
	},
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.id_table = charger_manager_id,
};

static int __init charger_manager_init(void)
{
	cm_wq = create_freezable_workqueue("charger_manager");
	if (unlikely(!cm_wq))
		return -ENOMEM;

	INIT_DELAYED_WORK(&cm_monitor_work, cm_monitor_poller);

	return platform_driver_register(&charger_manager_driver);
}
late_initcall(charger_manager_init);

static void __exit charger_manager_cleanup(void)
{
	destroy_workqueue(cm_wq);
	cm_wq = NULL;

	platform_driver_unregister(&charger_manager_driver);
}
module_exit(charger_manager_cleanup);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("Charger Manager");
MODULE_LICENSE("GPL");
