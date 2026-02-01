


#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/slab.h>

 
#define SC27XX_MODULE_EN0		0xc08
#define SC27XX_CLK_EN0			0xc18
#define SC27XX_FGU_EN			BIT(7)
#define SC27XX_FGU_RTC_EN		BIT(6)

 
#define SC27XX_FGU_START		0x0
#define SC27XX_FGU_CONFIG		0x4
#define SC27XX_FGU_ADC_CONFIG		0x8
#define SC27XX_FGU_STATUS		0xc
#define SC27XX_FGU_INT_EN		0x10
#define SC27XX_FGU_INT_CLR		0x14
#define SC27XX_FGU_INT_STS		0x1c
#define SC27XX_FGU_VOLTAGE		0x20
#define SC27XX_FGU_OCV			0x24
#define SC27XX_FGU_POCV			0x28
#define SC27XX_FGU_CURRENT		0x2c
#define SC27XX_FGU_LOW_OVERLOAD		0x34
#define SC27XX_FGU_CLBCNT_SETH		0x50
#define SC27XX_FGU_CLBCNT_SETL		0x54
#define SC27XX_FGU_CLBCNT_DELTH		0x58
#define SC27XX_FGU_CLBCNT_DELTL		0x5c
#define SC27XX_FGU_CLBCNT_VALH		0x68
#define SC27XX_FGU_CLBCNT_VALL		0x6c
#define SC27XX_FGU_CLBCNT_QMAXL		0x74
#define SC27XX_FGU_USER_AREA_SET	0xa0
#define SC27XX_FGU_USER_AREA_CLEAR	0xa4
#define SC27XX_FGU_USER_AREA_STATUS	0xa8
#define SC27XX_FGU_VOLTAGE_BUF		0xd0
#define SC27XX_FGU_CURRENT_BUF		0xf0

#define SC27XX_WRITE_SELCLB_EN		BIT(0)
#define SC27XX_FGU_CLBCNT_MASK		GENMASK(15, 0)
#define SC27XX_FGU_CLBCNT_SHIFT		16
#define SC27XX_FGU_LOW_OVERLOAD_MASK	GENMASK(12, 0)

#define SC27XX_FGU_INT_MASK		GENMASK(9, 0)
#define SC27XX_FGU_LOW_OVERLOAD_INT	BIT(0)
#define SC27XX_FGU_CLBCNT_DELTA_INT	BIT(2)

#define SC27XX_FGU_MODE_AREA_MASK	GENMASK(15, 12)
#define SC27XX_FGU_CAP_AREA_MASK	GENMASK(11, 0)
#define SC27XX_FGU_MODE_AREA_SHIFT	12

#define SC27XX_FGU_FIRST_POWERTON	GENMASK(3, 0)
#define SC27XX_FGU_DEFAULT_CAP		GENMASK(11, 0)
#define SC27XX_FGU_NORMAIL_POWERTON	0x5

#define SC27XX_FGU_CUR_BASIC_ADC	8192
#define SC27XX_FGU_SAMPLE_HZ		2
 
#define SC27XX_FGU_IDEAL_RESISTANCE	20000

 
struct sc27xx_fgu_data {
	struct regmap *regmap;
	struct device *dev;
	struct power_supply *battery;
	u32 base;
	struct mutex lock;
	struct gpio_desc *gpiod;
	struct iio_channel *channel;
	struct iio_channel *charge_chan;
	bool bat_present;
	int internal_resist;
	int total_cap;
	int init_cap;
	int alarm_cap;
	int init_clbcnt;
	int max_volt;
	int min_volt;
	int boot_volt;
	int table_len;
	int resist_table_len;
	int cur_1000ma_adc;
	int vol_1000mv_adc;
	int calib_resist;
	struct power_supply_battery_ocv_table *cap_table;
	struct power_supply_resistance_temp_table *resist_table;
};

static int sc27xx_fgu_cap_to_clbcnt(struct sc27xx_fgu_data *data, int capacity);
static void sc27xx_fgu_capacity_calibration(struct sc27xx_fgu_data *data,
					    int cap, bool int_mode);
static void sc27xx_fgu_adjust_cap(struct sc27xx_fgu_data *data, int cap);
static int sc27xx_fgu_get_temp(struct sc27xx_fgu_data *data, int *temp);

static const char * const sc27xx_charger_supply_name[] = {
	"sc2731_charger",
	"sc2720_charger",
	"sc2721_charger",
	"sc2723_charger",
};

static int sc27xx_fgu_adc_to_current(struct sc27xx_fgu_data *data, s64 adc)
{
	return DIV_S64_ROUND_CLOSEST(adc * 1000, data->cur_1000ma_adc);
}

static int sc27xx_fgu_adc_to_voltage(struct sc27xx_fgu_data *data, s64 adc)
{
	return DIV_S64_ROUND_CLOSEST(adc * 1000, data->vol_1000mv_adc);
}

static int sc27xx_fgu_voltage_to_adc(struct sc27xx_fgu_data *data, int vol)
{
	return DIV_ROUND_CLOSEST(vol * data->vol_1000mv_adc, 1000);
}

static bool sc27xx_fgu_is_first_poweron(struct sc27xx_fgu_data *data)
{
	int ret, status, cap, mode;

	ret = regmap_read(data->regmap,
			  data->base + SC27XX_FGU_USER_AREA_STATUS, &status);
	if (ret)
		return false;

	 
	mode = (status & SC27XX_FGU_MODE_AREA_MASK) >> SC27XX_FGU_MODE_AREA_SHIFT;
	cap = status & SC27XX_FGU_CAP_AREA_MASK;

	 
	if (mode == SC27XX_FGU_FIRST_POWERTON || cap == SC27XX_FGU_DEFAULT_CAP)
		return true;

	return false;
}

static int sc27xx_fgu_save_boot_mode(struct sc27xx_fgu_data *data,
				     int boot_mode)
{
	int ret;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR,
				 SC27XX_FGU_MODE_AREA_MASK,
				 SC27XX_FGU_MODE_AREA_MASK);
	if (ret)
		return ret;

	 
	udelay(200);

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_SET,
				 SC27XX_FGU_MODE_AREA_MASK,
				 boot_mode << SC27XX_FGU_MODE_AREA_SHIFT);
	if (ret)
		return ret;

	 
	udelay(200);

	 
	return regmap_update_bits(data->regmap,
				  data->base + SC27XX_FGU_USER_AREA_CLEAR,
				  SC27XX_FGU_MODE_AREA_MASK, 0);
}

static int sc27xx_fgu_save_last_cap(struct sc27xx_fgu_data *data, int cap)
{
	int ret;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR,
				 SC27XX_FGU_CAP_AREA_MASK,
				 SC27XX_FGU_CAP_AREA_MASK);
	if (ret)
		return ret;

	 
	udelay(200);

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_SET,
				 SC27XX_FGU_CAP_AREA_MASK, cap);
	if (ret)
		return ret;

	 
	udelay(200);

	 
	return regmap_update_bits(data->regmap,
				  data->base + SC27XX_FGU_USER_AREA_CLEAR,
				  SC27XX_FGU_CAP_AREA_MASK, 0);
}

static int sc27xx_fgu_read_last_cap(struct sc27xx_fgu_data *data, int *cap)
{
	int ret, value;

	ret = regmap_read(data->regmap,
			  data->base + SC27XX_FGU_USER_AREA_STATUS, &value);
	if (ret)
		return ret;

	*cap = value & SC27XX_FGU_CAP_AREA_MASK;
	return 0;
}

 
static int sc27xx_fgu_get_boot_capacity(struct sc27xx_fgu_data *data, int *cap)
{
	int volt, cur, oci, ocv, ret;
	bool is_first_poweron = sc27xx_fgu_is_first_poweron(data);

	 
	if (!is_first_poweron) {
		ret = sc27xx_fgu_read_last_cap(data, cap);
		if (ret)
			return ret;

		return sc27xx_fgu_save_boot_mode(data, SC27XX_FGU_NORMAIL_POWERTON);
	}

	 
	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CLBCNT_QMAXL,
			  &cur);
	if (ret)
		return ret;

	cur <<= 1;
	oci = sc27xx_fgu_adc_to_current(data, cur - SC27XX_FGU_CUR_BASIC_ADC);

	 
	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_POCV, &volt);
	if (ret)
		return ret;

	volt = sc27xx_fgu_adc_to_voltage(data, volt);
	ocv = volt * 1000 - oci * data->internal_resist;
	data->boot_volt = ocv;

	 
	*cap = power_supply_ocv2cap_simple(data->cap_table, data->table_len,
					   ocv);

	ret = sc27xx_fgu_save_last_cap(data, *cap);
	if (ret)
		return ret;

	return sc27xx_fgu_save_boot_mode(data, SC27XX_FGU_NORMAIL_POWERTON);
}

static int sc27xx_fgu_set_clbcnt(struct sc27xx_fgu_data *data, int clbcnt)
{
	int ret;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_CLBCNT_SETL,
				 SC27XX_FGU_CLBCNT_MASK, clbcnt);
	if (ret)
		return ret;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_CLBCNT_SETH,
				 SC27XX_FGU_CLBCNT_MASK,
				 clbcnt >> SC27XX_FGU_CLBCNT_SHIFT);
	if (ret)
		return ret;

	return regmap_update_bits(data->regmap, data->base + SC27XX_FGU_START,
				 SC27XX_WRITE_SELCLB_EN,
				 SC27XX_WRITE_SELCLB_EN);
}

static int sc27xx_fgu_get_clbcnt(struct sc27xx_fgu_data *data, int *clb_cnt)
{
	int ccl, cch, ret;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CLBCNT_VALL,
			  &ccl);
	if (ret)
		return ret;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CLBCNT_VALH,
			  &cch);
	if (ret)
		return ret;

	*clb_cnt = ccl & SC27XX_FGU_CLBCNT_MASK;
	*clb_cnt |= (cch & SC27XX_FGU_CLBCNT_MASK) << SC27XX_FGU_CLBCNT_SHIFT;

	return 0;
}

static int sc27xx_fgu_get_vol_now(struct sc27xx_fgu_data *data, int *val)
{
	int ret;
	u32 vol;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_VOLTAGE_BUF,
			  &vol);
	if (ret)
		return ret;

	 
	*val = sc27xx_fgu_adc_to_voltage(data, vol);

	return 0;
}

static int sc27xx_fgu_get_cur_now(struct sc27xx_fgu_data *data, int *val)
{
	int ret;
	u32 cur;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CURRENT_BUF,
			  &cur);
	if (ret)
		return ret;

	 
	*val = sc27xx_fgu_adc_to_current(data, cur - SC27XX_FGU_CUR_BASIC_ADC);

	return 0;
}

static int sc27xx_fgu_get_capacity(struct sc27xx_fgu_data *data, int *cap)
{
	int ret, cur_clbcnt, delta_clbcnt, delta_cap, temp;

	 
	ret = sc27xx_fgu_get_clbcnt(data, &cur_clbcnt);
	if (ret)
		return ret;

	delta_clbcnt = cur_clbcnt - data->init_clbcnt;

	 
	temp = DIV_ROUND_CLOSEST(delta_clbcnt * 10, 36 * SC27XX_FGU_SAMPLE_HZ);
	temp = sc27xx_fgu_adc_to_current(data, temp / 1000);

	 
	delta_cap = DIV_ROUND_CLOSEST(temp * 100, data->total_cap);
	*cap = delta_cap + data->init_cap;

	 
	sc27xx_fgu_capacity_calibration(data, *cap, false);

	return 0;
}

static int sc27xx_fgu_get_vbat_vol(struct sc27xx_fgu_data *data, int *val)
{
	int ret, vol;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_VOLTAGE, &vol);
	if (ret)
		return ret;

	 
	*val = sc27xx_fgu_adc_to_voltage(data, vol);

	return 0;
}

static int sc27xx_fgu_get_current(struct sc27xx_fgu_data *data, int *val)
{
	int ret, cur;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CURRENT, &cur);
	if (ret)
		return ret;

	 
	*val = sc27xx_fgu_adc_to_current(data, cur - SC27XX_FGU_CUR_BASIC_ADC);

	return 0;
}

static int sc27xx_fgu_get_vbat_ocv(struct sc27xx_fgu_data *data, int *val)
{
	int vol, cur, ret, temp, resistance;

	ret = sc27xx_fgu_get_vbat_vol(data, &vol);
	if (ret)
		return ret;

	ret = sc27xx_fgu_get_current(data, &cur);
	if (ret)
		return ret;

	resistance = data->internal_resist;
	if (data->resist_table_len > 0) {
		ret = sc27xx_fgu_get_temp(data, &temp);
		if (ret)
			return ret;

		resistance = power_supply_temp2resist_simple(data->resist_table,
						data->resist_table_len, temp);
		resistance = data->internal_resist * resistance / 100;
	}

	 
	*val = vol * 1000 - cur * resistance;

	return 0;
}

static int sc27xx_fgu_get_charge_vol(struct sc27xx_fgu_data *data, int *val)
{
	int ret, vol;

	ret = iio_read_channel_processed(data->charge_chan, &vol);
	if (ret < 0)
		return ret;

	*val = vol * 1000;
	return 0;
}

static int sc27xx_fgu_get_temp(struct sc27xx_fgu_data *data, int *temp)
{
	return iio_read_channel_processed(data->channel, temp);
}

static int sc27xx_fgu_get_health(struct sc27xx_fgu_data *data, int *health)
{
	int ret, vol;

	ret = sc27xx_fgu_get_vbat_vol(data, &vol);
	if (ret)
		return ret;

	if (vol > data->max_volt)
		*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else
		*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int sc27xx_fgu_get_status(struct sc27xx_fgu_data *data, int *status)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(sc27xx_charger_supply_name); i++) {
		psy = power_supply_get_by_name(sc27xx_charger_supply_name[i]);
		if (!psy)
			continue;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS,
						&val);
		power_supply_put(psy);
		if (ret)
			return ret;

		*status = val.intval;
	}

	return ret;
}

static int sc27xx_fgu_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct sc27xx_fgu_data *data = power_supply_get_drvdata(psy);
	int ret = 0;
	int value;

	mutex_lock(&data->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = sc27xx_fgu_get_status(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		ret = sc27xx_fgu_get_health(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->bat_present;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		ret = sc27xx_fgu_get_temp(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		ret = sc27xx_fgu_get_capacity(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = sc27xx_fgu_get_vbat_vol(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = sc27xx_fgu_get_vbat_ocv(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc27xx_fgu_get_charge_vol(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = sc27xx_fgu_get_current(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = data->total_cap * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = sc27xx_fgu_get_clbcnt(data, &value);
		if (ret)
			goto error;

		value = DIV_ROUND_CLOSEST(value * 10,
					  36 * SC27XX_FGU_SAMPLE_HZ);
		val->intval = sc27xx_fgu_adc_to_current(data, value);

		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sc27xx_fgu_get_vol_now(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = sc27xx_fgu_get_cur_now(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_BOOT:
		val->intval = data->boot_volt;
		break;

	default:
		ret = -EINVAL;
		break;
	}

error:
	mutex_unlock(&data->lock);
	return ret;
}

static int sc27xx_fgu_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	struct sc27xx_fgu_data *data = power_supply_get_drvdata(psy);
	int ret;

	mutex_lock(&data->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = sc27xx_fgu_save_last_cap(data, val->intval);
		if (ret < 0)
			dev_err(data->dev, "failed to save battery capacity\n");
		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		sc27xx_fgu_adjust_cap(data, val->intval);
		ret = 0;
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		data->total_cap = val->intval / 1000;
		ret = 0;
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&data->lock);

	return ret;
}

static int sc27xx_fgu_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	return psp == POWER_SUPPLY_PROP_CAPACITY ||
		psp == POWER_SUPPLY_PROP_CALIBRATE ||
		psp == POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN;
}

static enum power_supply_property sc27xx_fgu_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_BOOT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_CHARGE_NOW
};

static const struct power_supply_desc sc27xx_fgu_desc = {
	.name			= "sc27xx-fgu",
	.type			= POWER_SUPPLY_TYPE_BATTERY,
	.properties		= sc27xx_fgu_props,
	.num_properties		= ARRAY_SIZE(sc27xx_fgu_props),
	.get_property		= sc27xx_fgu_get_property,
	.set_property		= sc27xx_fgu_set_property,
	.external_power_changed	= power_supply_changed,
	.property_is_writeable	= sc27xx_fgu_property_is_writeable,
	.no_thermal		= true,
};

static void sc27xx_fgu_adjust_cap(struct sc27xx_fgu_data *data, int cap)
{
	int ret;

	data->init_cap = cap;
	ret = sc27xx_fgu_get_clbcnt(data, &data->init_clbcnt);
	if (ret)
		dev_err(data->dev, "failed to get init coulomb counter\n");
}

static void sc27xx_fgu_capacity_calibration(struct sc27xx_fgu_data *data,
					    int cap, bool int_mode)
{
	int ret, ocv, chg_sts, adc;

	ret = sc27xx_fgu_get_vbat_ocv(data, &ocv);
	if (ret) {
		dev_err(data->dev, "get battery ocv error.\n");
		return;
	}

	ret = sc27xx_fgu_get_status(data, &chg_sts);
	if (ret) {
		dev_err(data->dev, "get charger status error.\n");
		return;
	}

	 
	if (chg_sts == POWER_SUPPLY_STATUS_CHARGING)
		return;

	if ((ocv > data->cap_table[0].ocv && cap < 100) || cap > 100) {
		 
		sc27xx_fgu_adjust_cap(data, 100);
	} else if (ocv <= data->cap_table[data->table_len - 1].ocv) {
		 
		sc27xx_fgu_adjust_cap(data, 0);
	} else if ((ocv > data->cap_table[data->table_len - 1].ocv && cap <= 0) ||
		   (ocv > data->min_volt && cap <= data->alarm_cap)) {
		 
		int cur_cap = power_supply_ocv2cap_simple(data->cap_table,
							  data->table_len, ocv);

		sc27xx_fgu_adjust_cap(data, cur_cap);
	} else if (ocv <= data->min_volt) {
		 
		if (cap > data->alarm_cap) {
			sc27xx_fgu_adjust_cap(data, data->alarm_cap);
		} else {
			int cur_cap;

			 
			cur_cap = power_supply_ocv2cap_simple(data->cap_table,
							      data->table_len,
							      ocv);
			sc27xx_fgu_adjust_cap(data, cur_cap);
		}

		if (!int_mode)
			return;

		 
		data->min_volt = data->cap_table[data->table_len - 1].ocv;
		data->alarm_cap = power_supply_ocv2cap_simple(data->cap_table,
							      data->table_len,
							      data->min_volt);

		adc = sc27xx_fgu_voltage_to_adc(data, data->min_volt / 1000);
		regmap_update_bits(data->regmap,
				   data->base + SC27XX_FGU_LOW_OVERLOAD,
				   SC27XX_FGU_LOW_OVERLOAD_MASK, adc);
	}
}

static irqreturn_t sc27xx_fgu_interrupt(int irq, void *dev_id)
{
	struct sc27xx_fgu_data *data = dev_id;
	int ret, cap;
	u32 status;

	mutex_lock(&data->lock);

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_INT_STS,
			  &status);
	if (ret)
		goto out;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_CLR,
				 status, status);
	if (ret)
		goto out;

	 
	if (!(status & SC27XX_FGU_LOW_OVERLOAD_INT))
		goto out;

	ret = sc27xx_fgu_get_capacity(data, &cap);
	if (ret)
		goto out;

	sc27xx_fgu_capacity_calibration(data, cap, true);

out:
	mutex_unlock(&data->lock);

	power_supply_changed(data->battery);
	return IRQ_HANDLED;
}

static irqreturn_t sc27xx_fgu_bat_detection(int irq, void *dev_id)
{
	struct sc27xx_fgu_data *data = dev_id;
	int state;

	mutex_lock(&data->lock);

	state = gpiod_get_value_cansleep(data->gpiod);
	if (state < 0) {
		dev_err(data->dev, "failed to get gpio state\n");
		mutex_unlock(&data->lock);
		return IRQ_RETVAL(state);
	}

	data->bat_present = !!state;

	mutex_unlock(&data->lock);

	power_supply_changed(data->battery);
	return IRQ_HANDLED;
}

static void sc27xx_fgu_disable(void *_data)
{
	struct sc27xx_fgu_data *data = _data;

	regmap_update_bits(data->regmap, SC27XX_CLK_EN0, SC27XX_FGU_RTC_EN, 0);
	regmap_update_bits(data->regmap, SC27XX_MODULE_EN0, SC27XX_FGU_EN, 0);
}

static int sc27xx_fgu_cap_to_clbcnt(struct sc27xx_fgu_data *data, int capacity)
{
	 
	int cur_cap = DIV_ROUND_CLOSEST(data->total_cap * capacity, 100);

	 
	return DIV_ROUND_CLOSEST(cur_cap * 36 * data->cur_1000ma_adc * SC27XX_FGU_SAMPLE_HZ, 10);
}

static int sc27xx_fgu_calibration(struct sc27xx_fgu_data *data)
{
	struct nvmem_cell *cell;
	int calib_data, cal_4200mv;
	void *buf;
	size_t len;

	cell = nvmem_cell_get(data->dev, "fgu_calib");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));

	 
	cal_4200mv = (calib_data & 0x1ff) + 6963 - 4096 - 256;
	data->vol_1000mv_adc = DIV_ROUND_CLOSEST(cal_4200mv * 10, 42);
	data->cur_1000ma_adc =
		DIV_ROUND_CLOSEST(data->vol_1000mv_adc * 4 * data->calib_resist,
				  SC27XX_FGU_IDEAL_RESISTANCE);

	kfree(buf);
	return 0;
}

static int sc27xx_fgu_hw_init(struct sc27xx_fgu_data *data)
{
	struct power_supply_battery_info *info;
	struct power_supply_battery_ocv_table *table;
	int ret, delta_clbcnt, alarm_adc;

	ret = power_supply_get_battery_info(data->battery, &info);
	if (ret) {
		dev_err(data->dev, "failed to get battery information\n");
		return ret;
	}

	data->total_cap = info->charge_full_design_uah / 1000;
	data->max_volt = info->constant_charge_voltage_max_uv / 1000;
	data->internal_resist = info->factory_internal_resistance_uohm / 1000;
	data->min_volt = info->voltage_min_design_uv;

	 
	table = power_supply_find_ocv2cap_table(info, 20, &data->table_len);
	if (!table)
		return -EINVAL;

	data->cap_table = devm_kmemdup(data->dev, table,
				       data->table_len * sizeof(*table),
				       GFP_KERNEL);
	if (!data->cap_table) {
		power_supply_put_battery_info(data->battery, info);
		return -ENOMEM;
	}

	data->alarm_cap = power_supply_ocv2cap_simple(data->cap_table,
						      data->table_len,
						      data->min_volt);
	if (!data->alarm_cap)
		data->alarm_cap += 1;

	data->resist_table_len = info->resist_table_size;
	if (data->resist_table_len > 0) {
		data->resist_table = devm_kmemdup(data->dev, info->resist_table,
						  data->resist_table_len *
						  sizeof(struct power_supply_resistance_temp_table),
						  GFP_KERNEL);
		if (!data->resist_table) {
			power_supply_put_battery_info(data->battery, info);
			return -ENOMEM;
		}
	}

	power_supply_put_battery_info(data->battery, info);

	ret = sc27xx_fgu_calibration(data);
	if (ret)
		return ret;

	 
	ret = regmap_update_bits(data->regmap, SC27XX_MODULE_EN0,
				 SC27XX_FGU_EN, SC27XX_FGU_EN);
	if (ret) {
		dev_err(data->dev, "failed to enable fgu\n");
		return ret;
	}

	 
	ret = regmap_update_bits(data->regmap, SC27XX_CLK_EN0,
				 SC27XX_FGU_RTC_EN, SC27XX_FGU_RTC_EN);
	if (ret) {
		dev_err(data->dev, "failed to enable fgu RTC clock\n");
		goto disable_fgu;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_CLR,
				 SC27XX_FGU_INT_MASK, SC27XX_FGU_INT_MASK);
	if (ret) {
		dev_err(data->dev, "failed to clear interrupt status\n");
		goto disable_clk;
	}

	 
	alarm_adc = sc27xx_fgu_voltage_to_adc(data, data->min_volt / 1000);
	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_LOW_OVERLOAD,
				 SC27XX_FGU_LOW_OVERLOAD_MASK, alarm_adc);
	if (ret) {
		dev_err(data->dev, "failed to set fgu low overload\n");
		goto disable_clk;
	}

	 
	delta_clbcnt = sc27xx_fgu_cap_to_clbcnt(data, 1);

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_CLBCNT_DELTL,
				 SC27XX_FGU_CLBCNT_MASK, delta_clbcnt);
	if (ret) {
		dev_err(data->dev, "failed to set low delta coulomb counter\n");
		goto disable_clk;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_CLBCNT_DELTH,
				 SC27XX_FGU_CLBCNT_MASK,
				 delta_clbcnt >> SC27XX_FGU_CLBCNT_SHIFT);
	if (ret) {
		dev_err(data->dev, "failed to set high delta coulomb counter\n");
		goto disable_clk;
	}

	 
	ret = sc27xx_fgu_get_boot_capacity(data, &data->init_cap);
	if (ret) {
		dev_err(data->dev, "failed to get boot capacity\n");
		goto disable_clk;
	}

	 
	data->init_clbcnt = sc27xx_fgu_cap_to_clbcnt(data, data->init_cap);
	ret = sc27xx_fgu_set_clbcnt(data, data->init_clbcnt);
	if (ret) {
		dev_err(data->dev, "failed to initialize coulomb counter\n");
		goto disable_clk;
	}

	return 0;

disable_clk:
	regmap_update_bits(data->regmap, SC27XX_CLK_EN0, SC27XX_FGU_RTC_EN, 0);
disable_fgu:
	regmap_update_bits(data->regmap, SC27XX_MODULE_EN0, SC27XX_FGU_EN, 0);

	return ret;
}

static int sc27xx_fgu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct power_supply_config fgu_cfg = { };
	struct sc27xx_fgu_data *data;
	int ret, irq;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->regmap = dev_get_regmap(dev->parent, NULL);
	if (!data->regmap) {
		dev_err(dev, "failed to get regmap\n");
		return -ENODEV;
	}

	ret = device_property_read_u32(dev, "reg", &data->base);
	if (ret) {
		dev_err(dev, "failed to get fgu address\n");
		return ret;
	}

	ret = device_property_read_u32(&pdev->dev,
				       "sprd,calib-resistance-micro-ohms",
				       &data->calib_resist);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to get fgu calibration resistance\n");
		return ret;
	}

	data->channel = devm_iio_channel_get(dev, "bat-temp");
	if (IS_ERR(data->channel)) {
		dev_err(dev, "failed to get IIO channel\n");
		return PTR_ERR(data->channel);
	}

	data->charge_chan = devm_iio_channel_get(dev, "charge-vol");
	if (IS_ERR(data->charge_chan)) {
		dev_err(dev, "failed to get charge IIO channel\n");
		return PTR_ERR(data->charge_chan);
	}

	data->gpiod = devm_gpiod_get(dev, "bat-detect", GPIOD_IN);
	if (IS_ERR(data->gpiod)) {
		dev_err(dev, "failed to get battery detection GPIO\n");
		return PTR_ERR(data->gpiod);
	}

	ret = gpiod_get_value_cansleep(data->gpiod);
	if (ret < 0) {
		dev_err(dev, "failed to get gpio state\n");
		return ret;
	}

	data->bat_present = !!ret;
	mutex_init(&data->lock);
	data->dev = dev;
	platform_set_drvdata(pdev, data);

	fgu_cfg.drv_data = data;
	fgu_cfg.of_node = np;
	data->battery = devm_power_supply_register(dev, &sc27xx_fgu_desc,
						   &fgu_cfg);
	if (IS_ERR(data->battery)) {
		dev_err(dev, "failed to register power supply\n");
		return PTR_ERR(data->battery);
	}

	ret = sc27xx_fgu_hw_init(data);
	if (ret) {
		dev_err(dev, "failed to initialize fgu hardware\n");
		return ret;
	}

	ret = devm_add_action_or_reset(dev, sc27xx_fgu_disable, data);
	if (ret) {
		dev_err(dev, "failed to add fgu disable action\n");
		return ret;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(data->dev, irq, NULL,
					sc27xx_fgu_interrupt,
					IRQF_NO_SUSPEND | IRQF_ONESHOT,
					pdev->name, data);
	if (ret) {
		dev_err(data->dev, "failed to request fgu IRQ\n");
		return ret;
	}

	irq = gpiod_to_irq(data->gpiod);
	if (irq < 0) {
		dev_err(dev, "failed to translate GPIO to IRQ\n");
		return irq;
	}

	ret = devm_request_threaded_irq(dev, irq, NULL,
					sc27xx_fgu_bat_detection,
					IRQF_ONESHOT | IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					pdev->name, data);
	if (ret) {
		dev_err(dev, "failed to request IRQ\n");
		return ret;
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sc27xx_fgu_resume(struct device *dev)
{
	struct sc27xx_fgu_data *data = dev_get_drvdata(dev);
	int ret;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
				 SC27XX_FGU_LOW_OVERLOAD_INT |
				 SC27XX_FGU_CLBCNT_DELTA_INT, 0);
	if (ret) {
		dev_err(data->dev, "failed to disable fgu interrupts\n");
		return ret;
	}

	return 0;
}

static int sc27xx_fgu_suspend(struct device *dev)
{
	struct sc27xx_fgu_data *data = dev_get_drvdata(dev);
	int ret, status, ocv;

	ret = sc27xx_fgu_get_status(data, &status);
	if (ret)
		return ret;

	 
	if (status != POWER_SUPPLY_STATUS_NOT_CHARGING &&
	    status != POWER_SUPPLY_STATUS_DISCHARGING)
		return 0;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
				 SC27XX_FGU_LOW_OVERLOAD_INT,
				 SC27XX_FGU_LOW_OVERLOAD_INT);
	if (ret) {
		dev_err(data->dev, "failed to enable low voltage interrupt\n");
		return ret;
	}

	ret = sc27xx_fgu_get_vbat_ocv(data, &ocv);
	if (ret)
		goto disable_int;

	 
	if (ocv < data->min_volt) {
		ret = regmap_update_bits(data->regmap,
					 data->base + SC27XX_FGU_INT_EN,
					 SC27XX_FGU_CLBCNT_DELTA_INT,
					 SC27XX_FGU_CLBCNT_DELTA_INT);
		if (ret) {
			dev_err(data->dev,
				"failed to enable coulomb threshold int\n");
			goto disable_int;
		}
	}

	return 0;

disable_int:
	regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
			   SC27XX_FGU_LOW_OVERLOAD_INT, 0);
	return ret;
}
#endif

static const struct dev_pm_ops sc27xx_fgu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sc27xx_fgu_suspend, sc27xx_fgu_resume)
};

static const struct of_device_id sc27xx_fgu_of_match[] = {
	{ .compatible = "sprd,sc2731-fgu", },
	{ }
};
MODULE_DEVICE_TABLE(of, sc27xx_fgu_of_match);

static struct platform_driver sc27xx_fgu_driver = {
	.probe = sc27xx_fgu_probe,
	.driver = {
		.name = "sc27xx-fgu",
		.of_match_table = sc27xx_fgu_of_match,
		.pm = &sc27xx_fgu_pm_ops,
	}
};

module_platform_driver(sc27xx_fgu_driver);

MODULE_DESCRIPTION("Spreadtrum SC27XX PMICs Fual Gauge Unit Driver");
MODULE_LICENSE("GPL v2");
