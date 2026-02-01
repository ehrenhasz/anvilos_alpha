
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/list.h>
#include <asm/div64.h>
#include <linux/mfd/da9150/core.h>
#include <linux/mfd/da9150/registers.h>
#include <linux/devm-helpers.h>

 
#define DA9150_QIF_READ		(0x0 << 7)
#define DA9150_QIF_WRITE	(0x1 << 7)
#define DA9150_QIF_CODE_MASK	0x7F

#define DA9150_QIF_BYTE_SIZE	8
#define DA9150_QIF_BYTE_MASK	0xFF
#define DA9150_QIF_SHORT_SIZE	2
#define DA9150_QIF_LONG_SIZE	4

 
#define DA9150_QIF_UAVG			6
#define DA9150_QIF_UAVG_SIZE		DA9150_QIF_LONG_SIZE
#define DA9150_QIF_IAVG			8
#define DA9150_QIF_IAVG_SIZE		DA9150_QIF_LONG_SIZE
#define DA9150_QIF_NTCAVG		12
#define DA9150_QIF_NTCAVG_SIZE		DA9150_QIF_LONG_SIZE
#define DA9150_QIF_SHUNT_VAL		36
#define DA9150_QIF_SHUNT_VAL_SIZE	DA9150_QIF_SHORT_SIZE
#define DA9150_QIF_SD_GAIN		38
#define DA9150_QIF_SD_GAIN_SIZE		DA9150_QIF_LONG_SIZE
#define DA9150_QIF_FCC_MAH		40
#define DA9150_QIF_FCC_MAH_SIZE		DA9150_QIF_SHORT_SIZE
#define DA9150_QIF_SOC_PCT		43
#define DA9150_QIF_SOC_PCT_SIZE		DA9150_QIF_SHORT_SIZE
#define DA9150_QIF_CHARGE_LIMIT		44
#define DA9150_QIF_CHARGE_LIMIT_SIZE	DA9150_QIF_SHORT_SIZE
#define DA9150_QIF_DISCHARGE_LIMIT	45
#define DA9150_QIF_DISCHARGE_LIMIT_SIZE	DA9150_QIF_SHORT_SIZE
#define DA9150_QIF_FW_MAIN_VER		118
#define DA9150_QIF_FW_MAIN_VER_SIZE	DA9150_QIF_SHORT_SIZE
#define DA9150_QIF_E_FG_STATUS		126
#define DA9150_QIF_E_FG_STATUS_SIZE	DA9150_QIF_SHORT_SIZE
#define DA9150_QIF_SYNC			127
#define DA9150_QIF_SYNC_SIZE		DA9150_QIF_SHORT_SIZE
#define DA9150_QIF_MAX_CODES		128

 
#define DA9150_QIF_SYNC_TIMEOUT		1000
#define DA9150_QIF_SYNC_RETRIES		10

 
#define DA9150_FG_IRQ_LOW_SOC_MASK	(1 << 0)
#define DA9150_FG_IRQ_HIGH_SOC_MASK	(1 << 1)
#define DA9150_FG_IRQ_SOC_MASK	\
	(DA9150_FG_IRQ_LOW_SOC_MASK | DA9150_FG_IRQ_HIGH_SOC_MASK)

 
struct da9150_fg {
	struct da9150 *da9150;
	struct device *dev;

	struct mutex io_lock;

	struct power_supply *battery;
	struct delayed_work work;
	u32 interval;

	int warn_soc;
	int crit_soc;
	int soc;
};

 
static u32 da9150_fg_read_attr(struct da9150_fg *fg, u8 code, u8 size)

{
	u8 buf[DA9150_QIF_LONG_SIZE];
	u8 read_addr;
	u32 res = 0;
	int i;

	 
	read_addr = (code & DA9150_QIF_CODE_MASK) | DA9150_QIF_READ;

	da9150_read_qif(fg->da9150, read_addr, size, buf);
	for (i = 0; i < size; ++i)
		res |= (buf[i] << (i * DA9150_QIF_BYTE_SIZE));

	return res;
}

static void da9150_fg_write_attr(struct da9150_fg *fg, u8 code, u8 size,
				 u32 val)

{
	u8 buf[DA9150_QIF_LONG_SIZE];
	u8 write_addr;
	int i;

	 
	write_addr = (code & DA9150_QIF_CODE_MASK) | DA9150_QIF_WRITE;

	for (i = 0; i < size; ++i) {
		buf[i] = (val >> (i * DA9150_QIF_BYTE_SIZE)) &
			 DA9150_QIF_BYTE_MASK;
	}
	da9150_write_qif(fg->da9150, write_addr, size, buf);
}

 
static void da9150_fg_read_sync_start(struct da9150_fg *fg)
{
	int i = 0;
	u32 res = 0;

	mutex_lock(&fg->io_lock);

	 
	res = da9150_fg_read_attr(fg, DA9150_QIF_SYNC,
				  DA9150_QIF_SYNC_SIZE);
	if (res > 0)
		da9150_fg_write_attr(fg, DA9150_QIF_SYNC,
				     DA9150_QIF_SYNC_SIZE, 0);

	 
	res = 0;
	while ((res == 0) && (i++ < DA9150_QIF_SYNC_RETRIES)) {
		usleep_range(DA9150_QIF_SYNC_TIMEOUT,
			     DA9150_QIF_SYNC_TIMEOUT * 2);
		res = da9150_fg_read_attr(fg, DA9150_QIF_SYNC,
					  DA9150_QIF_SYNC_SIZE);
	}

	 
	if (res == 0)
		dev_err(fg->dev, "Failed to perform QIF read sync!\n");
}

 
static inline void da9150_fg_read_sync_end(struct da9150_fg *fg)
{
	mutex_unlock(&fg->io_lock);
}

 
static u32 da9150_fg_read_attr_sync(struct da9150_fg *fg, u8 code, u8 size)
{
	u32 val;

	da9150_fg_read_sync_start(fg);
	val = da9150_fg_read_attr(fg, code, size);
	da9150_fg_read_sync_end(fg);

	return val;
}

 
static void da9150_fg_write_attr_sync(struct da9150_fg *fg, u8 code, u8 size,
				      u32 val)
{
	int i = 0;
	u32 res = 0, sync_val;

	mutex_lock(&fg->io_lock);

	 
	res = da9150_fg_read_attr(fg, DA9150_QIF_SYNC,
				  DA9150_QIF_SYNC_SIZE);

	 
	while ((res == 0) && (i++ < DA9150_QIF_SYNC_RETRIES)) {
		usleep_range(DA9150_QIF_SYNC_TIMEOUT,
			     DA9150_QIF_SYNC_TIMEOUT * 2);
		res = da9150_fg_read_attr(fg, DA9150_QIF_SYNC,
					  DA9150_QIF_SYNC_SIZE);
	}

	if (res == 0) {
		dev_err(fg->dev, "Timeout waiting for existing QIF sync!\n");
		mutex_unlock(&fg->io_lock);
		return;
	}

	 
	da9150_fg_write_attr(fg, code, size, val);

	 
	i = 0;
	sync_val = res;
	while ((res == sync_val) && (i++ < DA9150_QIF_SYNC_RETRIES)) {
		usleep_range(DA9150_QIF_SYNC_TIMEOUT,
			     DA9150_QIF_SYNC_TIMEOUT * 2);
		res = da9150_fg_read_attr(fg, DA9150_QIF_SYNC,
					  DA9150_QIF_SYNC_SIZE);
	}

	mutex_unlock(&fg->io_lock);

	 
	if (res != (sync_val + 1))
		dev_err(fg->dev, "Error performing QIF sync write for code %d\n",
			code);
}

 
static int da9150_fg_capacity(struct da9150_fg *fg,
			      union power_supply_propval *val)
{
	val->intval = da9150_fg_read_attr_sync(fg, DA9150_QIF_SOC_PCT,
					       DA9150_QIF_SOC_PCT_SIZE);

	if (val->intval > 100)
		val->intval = 100;

	return 0;
}

static int da9150_fg_current_avg(struct da9150_fg *fg,
				 union power_supply_propval *val)
{
	u32 iavg, sd_gain, shunt_val;
	u64 div, res;

	da9150_fg_read_sync_start(fg);
	iavg = da9150_fg_read_attr(fg, DA9150_QIF_IAVG,
				   DA9150_QIF_IAVG_SIZE);
	shunt_val = da9150_fg_read_attr(fg, DA9150_QIF_SHUNT_VAL,
					DA9150_QIF_SHUNT_VAL_SIZE);
	sd_gain = da9150_fg_read_attr(fg, DA9150_QIF_SD_GAIN,
				      DA9150_QIF_SD_GAIN_SIZE);
	da9150_fg_read_sync_end(fg);

	div = (u64) (sd_gain * shunt_val * 65536ULL);
	do_div(div, 1000000);
	res = (u64) (iavg * 1000000ULL);
	do_div(res, div);

	val->intval = (int) res;

	return 0;
}

static int da9150_fg_voltage_avg(struct da9150_fg *fg,
				 union power_supply_propval *val)
{
	u64 res;

	val->intval = da9150_fg_read_attr_sync(fg, DA9150_QIF_UAVG,
					       DA9150_QIF_UAVG_SIZE);

	res = (u64) (val->intval * 186ULL);
	do_div(res, 10000);
	val->intval = (int) res;

	return 0;
}

static int da9150_fg_charge_full(struct da9150_fg *fg,
				 union power_supply_propval *val)
{
	val->intval = da9150_fg_read_attr_sync(fg, DA9150_QIF_FCC_MAH,
					       DA9150_QIF_FCC_MAH_SIZE);

	val->intval = val->intval * 1000;

	return 0;
}

 
static int da9150_fg_temp(struct da9150_fg *fg,
			  union power_supply_propval *val)
{
	val->intval = da9150_fg_read_attr_sync(fg, DA9150_QIF_NTCAVG,
					       DA9150_QIF_NTCAVG_SIZE);

	val->intval = (val->intval * 10) / 1048576;

	return 0;
}

static enum power_supply_property da9150_fg_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_TEMP,
};

static int da9150_fg_get_prop(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct da9150_fg *fg = dev_get_drvdata(psy->dev.parent);
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = da9150_fg_capacity(fg, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = da9150_fg_current_avg(fg, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = da9150_fg_voltage_avg(fg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = da9150_fg_charge_full(fg, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = da9150_fg_temp(fg, val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

 
static bool da9150_fg_soc_changed(struct da9150_fg *fg)
{
	union power_supply_propval val;

	da9150_fg_capacity(fg, &val);
	if (val.intval != fg->soc) {
		fg->soc = val.intval;
		return true;
	}

	return false;
}

static void da9150_fg_work(struct work_struct *work)
{
	struct da9150_fg *fg = container_of(work, struct da9150_fg, work.work);

	 
	if (da9150_fg_soc_changed(fg))
		power_supply_changed(fg->battery);

	schedule_delayed_work(&fg->work, msecs_to_jiffies(fg->interval));
}

 
static void da9150_fg_soc_event_config(struct da9150_fg *fg)
{
	int soc;

	soc = da9150_fg_read_attr_sync(fg, DA9150_QIF_SOC_PCT,
				       DA9150_QIF_SOC_PCT_SIZE);

	if (soc > fg->warn_soc) {
		 
		da9150_fg_write_attr_sync(fg, DA9150_QIF_DISCHARGE_LIMIT,
					  DA9150_QIF_DISCHARGE_LIMIT_SIZE,
					  fg->warn_soc + 1);
	} else if ((soc <= fg->warn_soc) && (soc > fg->crit_soc)) {
		 
		da9150_fg_write_attr_sync(fg, DA9150_QIF_DISCHARGE_LIMIT,
					  DA9150_QIF_DISCHARGE_LIMIT_SIZE,
					  fg->crit_soc + 1);

		da9150_fg_write_attr_sync(fg, DA9150_QIF_CHARGE_LIMIT,
					  DA9150_QIF_CHARGE_LIMIT_SIZE,
					  fg->warn_soc);
	} else if (soc <= fg->crit_soc) {
		 
		da9150_fg_write_attr_sync(fg, DA9150_QIF_CHARGE_LIMIT,
					  DA9150_QIF_CHARGE_LIMIT_SIZE,
					  fg->crit_soc);
	}
}

static irqreturn_t da9150_fg_irq(int irq, void *data)
{
	struct da9150_fg *fg = data;
	u32 e_fg_status;

	 
	e_fg_status = da9150_fg_read_attr(fg, DA9150_QIF_E_FG_STATUS,
					  DA9150_QIF_E_FG_STATUS_SIZE);

	 
	if (e_fg_status & DA9150_FG_IRQ_SOC_MASK)
		da9150_fg_soc_event_config(fg);

	 
	da9150_fg_write_attr(fg, DA9150_QIF_E_FG_STATUS,
			     DA9150_QIF_E_FG_STATUS_SIZE, e_fg_status);

	return IRQ_HANDLED;
}

static struct da9150_fg_pdata *da9150_fg_dt_pdata(struct device *dev)
{
	struct device_node *fg_node = dev->of_node;
	struct da9150_fg_pdata *pdata;

	pdata = devm_kzalloc(dev, sizeof(struct da9150_fg_pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	of_property_read_u32(fg_node, "dlg,update-interval",
			     &pdata->update_interval);
	of_property_read_u8(fg_node, "dlg,warn-soc-level",
			    &pdata->warn_soc_lvl);
	of_property_read_u8(fg_node, "dlg,crit-soc-level",
			    &pdata->crit_soc_lvl);

	return pdata;
}

static const struct power_supply_desc fg_desc = {
	.name		= "da9150-fg",
	.type		= POWER_SUPPLY_TYPE_BATTERY,
	.properties	= da9150_fg_props,
	.num_properties	= ARRAY_SIZE(da9150_fg_props),
	.get_property	= da9150_fg_get_prop,
};

static int da9150_fg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct da9150 *da9150 = dev_get_drvdata(dev->parent);
	struct da9150_fg_pdata *fg_pdata = dev_get_platdata(dev);
	struct da9150_fg *fg;
	int ver, irq, ret = 0;

	fg = devm_kzalloc(dev, sizeof(*fg), GFP_KERNEL);
	if (fg == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, fg);
	fg->da9150 = da9150;
	fg->dev = dev;

	mutex_init(&fg->io_lock);

	 
	da9150_set_bits(da9150, DA9150_CORE2WIRE_CTRL_A, DA9150_FG_QIF_EN_MASK,
			DA9150_FG_QIF_EN_MASK);

	fg->battery = devm_power_supply_register(dev, &fg_desc, NULL);
	if (IS_ERR(fg->battery)) {
		ret = PTR_ERR(fg->battery);
		return ret;
	}

	ver = da9150_fg_read_attr(fg, DA9150_QIF_FW_MAIN_VER,
				  DA9150_QIF_FW_MAIN_VER_SIZE);
	dev_info(dev, "Version: 0x%x\n", ver);

	 
	if (dev->of_node) {
		fg_pdata = da9150_fg_dt_pdata(dev);
		dev->platform_data = fg_pdata;
	}

	 
	if (fg_pdata) {
		fg->interval = fg_pdata->update_interval;

		if (fg_pdata->warn_soc_lvl > 100)
			dev_warn(dev, "Invalid SOC warning level provided, Ignoring");
		else
			fg->warn_soc = fg_pdata->warn_soc_lvl;

		if ((fg_pdata->crit_soc_lvl > 100) ||
		    (fg_pdata->crit_soc_lvl >= fg_pdata->warn_soc_lvl))
			dev_warn(dev, "Invalid SOC critical level provided, Ignoring");
		else
			fg->crit_soc = fg_pdata->crit_soc_lvl;


	}

	 
	da9150_fg_soc_event_config(fg);

	 
	if (fg->interval) {
		ret = devm_delayed_work_autocancel(dev, &fg->work,
						   da9150_fg_work);
		if (ret) {
			dev_err(dev, "Failed to init work\n");
			return ret;
		}

		schedule_delayed_work(&fg->work,
				      msecs_to_jiffies(fg->interval));
	}

	 
	irq = platform_get_irq_byname(pdev, "FG");
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(dev, irq, NULL, da9150_fg_irq,
					IRQF_ONESHOT, "FG", fg);
	if (ret) {
		dev_err(dev, "Failed to request IRQ %d: %d\n", irq, ret);
		return ret;
	}

	return 0;
}

static int da9150_fg_resume(struct platform_device *pdev)
{
	struct da9150_fg *fg = platform_get_drvdata(pdev);

	 
	if (fg->interval)
		flush_delayed_work(&fg->work);

	return 0;
}

static struct platform_driver da9150_fg_driver = {
	.driver = {
		.name = "da9150-fuel-gauge",
	},
	.probe = da9150_fg_probe,
	.resume = da9150_fg_resume,
};

module_platform_driver(da9150_fg_driver);

MODULE_DESCRIPTION("Fuel-Gauge Driver for DA9150");
MODULE_AUTHOR("Adam Thomson <Adam.Thomson.Opensource@diasemi.com>");
MODULE_LICENSE("GPL");
