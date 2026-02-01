
 

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/polynomial.h>
#include <linux/seqlock.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include "bt1-pvt.h"

 
static const struct pvt_sensor_info pvt_info[] = {
	PVT_SENSOR_INFO(0, "CPU Core Temperature", hwmon_temp, TEMP, TTHRES),
	PVT_SENSOR_INFO(0, "CPU Core Voltage", hwmon_in, VOLT, VTHRES),
	PVT_SENSOR_INFO(1, "CPU Core Low-Vt", hwmon_in, LVT, LTHRES),
	PVT_SENSOR_INFO(2, "CPU Core High-Vt", hwmon_in, HVT, HTHRES),
	PVT_SENSOR_INFO(3, "CPU Core Standard-Vt", hwmon_in, SVT, STHRES),
};

 
static const struct polynomial __maybe_unused poly_temp_to_N = {
	.total_divider = 10000,
	.terms = {
		{4, 18322, 10000, 10000},
		{3, 2343, 10000, 10},
		{2, 87018, 10000, 10},
		{1, 39269, 1000, 1},
		{0, 1720400, 1, 1}
	}
};

static const struct polynomial poly_N_to_temp = {
	.total_divider = 1,
	.terms = {
		{4, -16743, 1000, 1},
		{3, 81542, 1000, 1},
		{2, -182010, 1000, 1},
		{1, 310200, 1000, 1},
		{0, -48380, 1, 1}
	}
};

 
static const struct polynomial __maybe_unused poly_volt_to_N = {
	.total_divider = 10,
	.terms = {
		{1, 18658, 1000, 1},
		{0, -11572, 1, 1}
	}
};

static const struct polynomial poly_N_to_volt = {
	.total_divider = 10,
	.terms = {
		{1, 100000, 18658, 1},
		{0, 115720000, 1, 18658}
	}
};

static inline u32 pvt_update(void __iomem *reg, u32 mask, u32 data)
{
	u32 old;

	old = readl_relaxed(reg);
	writel((old & ~mask) | (data & mask), reg);

	return old & mask;
}

 
static inline void pvt_set_mode(struct pvt_hwmon *pvt, u32 mode)
{
	u32 old;

	mode = FIELD_PREP(PVT_CTRL_MODE_MASK, mode);

	old = pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, 0);
	pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_MODE_MASK | PVT_CTRL_EN,
		   mode | old);
}

static inline u32 pvt_calc_trim(long temp)
{
	temp = clamp_val(temp, 0, PVT_TRIM_TEMP);

	return DIV_ROUND_UP(temp, PVT_TRIM_STEP);
}

static inline void pvt_set_trim(struct pvt_hwmon *pvt, u32 trim)
{
	u32 old;

	trim = FIELD_PREP(PVT_CTRL_TRIM_MASK, trim);

	old = pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, 0);
	pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_TRIM_MASK | PVT_CTRL_EN,
		   trim | old);
}

static inline void pvt_set_tout(struct pvt_hwmon *pvt, u32 tout)
{
	u32 old;

	old = pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, 0);
	writel(tout, pvt->regs + PVT_TTIMEOUT);
	pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, old);
}

 

#if defined(CONFIG_SENSORS_BT1_PVT_ALARMS)

#define pvt_hard_isr NULL

static irqreturn_t pvt_soft_isr(int irq, void *data)
{
	const struct pvt_sensor_info *info;
	struct pvt_hwmon *pvt = data;
	struct pvt_cache *cache;
	u32 val, thres_sts, old;

	 
	thres_sts = readl(pvt->regs + PVT_RAW_INTR_STAT);

	 
	cache = &pvt->cache[pvt->sensor];
	info = &pvt_info[pvt->sensor];
	pvt->sensor = (pvt->sensor == PVT_SENSOR_LAST) ?
		      PVT_SENSOR_FIRST : (pvt->sensor + 1);

	 
	mutex_lock(&pvt->iface_mtx);

	old = pvt_update(pvt->regs + PVT_INTR_MASK, PVT_INTR_DVALID,
			 PVT_INTR_DVALID);

	val = readl(pvt->regs + PVT_DATA);

	pvt_set_mode(pvt, pvt_info[pvt->sensor].mode);

	pvt_update(pvt->regs + PVT_INTR_MASK, PVT_INTR_DVALID, old);

	mutex_unlock(&pvt->iface_mtx);

	 
	write_seqlock(&cache->data_seqlock);

	cache->data = FIELD_GET(PVT_DATA_DATA_MASK, val);

	write_sequnlock(&cache->data_seqlock);

	 
	if ((thres_sts & info->thres_sts_lo) ^ cache->thres_sts_lo) {
		WRITE_ONCE(cache->thres_sts_lo, thres_sts & info->thres_sts_lo);
		hwmon_notify_event(pvt->hwmon, info->type, info->attr_min_alarm,
				   info->channel);
	} else if ((thres_sts & info->thres_sts_hi) ^ cache->thres_sts_hi) {
		WRITE_ONCE(cache->thres_sts_hi, thres_sts & info->thres_sts_hi);
		hwmon_notify_event(pvt->hwmon, info->type, info->attr_max_alarm,
				   info->channel);
	}

	return IRQ_HANDLED;
}

static inline umode_t pvt_limit_is_visible(enum pvt_sensor_type type)
{
	return 0644;
}

static inline umode_t pvt_alarm_is_visible(enum pvt_sensor_type type)
{
	return 0444;
}

static int pvt_read_data(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			 long *val)
{
	struct pvt_cache *cache = &pvt->cache[type];
	unsigned int seq;
	u32 data;

	do {
		seq = read_seqbegin(&cache->data_seqlock);
		data = cache->data;
	} while (read_seqretry(&cache->data_seqlock, seq));

	if (type == PVT_TEMP)
		*val = polynomial_calc(&poly_N_to_temp, data);
	else
		*val = polynomial_calc(&poly_N_to_volt, data);

	return 0;
}

static int pvt_read_limit(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			  bool is_low, long *val)
{
	u32 data;

	 
	data = readl(pvt->regs + pvt_info[type].thres_base);

	if (is_low)
		data = FIELD_GET(PVT_THRES_LO_MASK, data);
	else
		data = FIELD_GET(PVT_THRES_HI_MASK, data);

	if (type == PVT_TEMP)
		*val = polynomial_calc(&poly_N_to_temp, data);
	else
		*val = polynomial_calc(&poly_N_to_volt, data);

	return 0;
}

static int pvt_write_limit(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			   bool is_low, long val)
{
	u32 data, limit, mask;
	int ret;

	if (type == PVT_TEMP) {
		val = clamp(val, PVT_TEMP_MIN, PVT_TEMP_MAX);
		data = polynomial_calc(&poly_temp_to_N, val);
	} else {
		val = clamp(val, PVT_VOLT_MIN, PVT_VOLT_MAX);
		data = polynomial_calc(&poly_volt_to_N, val);
	}

	 
	ret = mutex_lock_interruptible(&pvt->iface_mtx);
	if (ret)
		return ret;

	 
	limit = readl(pvt->regs + pvt_info[type].thres_base);
	if (is_low) {
		limit = FIELD_GET(PVT_THRES_HI_MASK, limit);
		data = clamp_val(data, PVT_DATA_MIN, limit);
		data = FIELD_PREP(PVT_THRES_LO_MASK, data);
		mask = PVT_THRES_LO_MASK;
	} else {
		limit = FIELD_GET(PVT_THRES_LO_MASK, limit);
		data = clamp_val(data, limit, PVT_DATA_MAX);
		data = FIELD_PREP(PVT_THRES_HI_MASK, data);
		mask = PVT_THRES_HI_MASK;
	}

	pvt_update(pvt->regs + pvt_info[type].thres_base, mask, data);

	mutex_unlock(&pvt->iface_mtx);

	return 0;
}

static int pvt_read_alarm(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			  bool is_low, long *val)
{
	if (is_low)
		*val = !!READ_ONCE(pvt->cache[type].thres_sts_lo);
	else
		*val = !!READ_ONCE(pvt->cache[type].thres_sts_hi);

	return 0;
}

static const struct hwmon_channel_info * const pvt_channel_info[] = {
	HWMON_CHANNEL_INFO(chip,
			   HWMON_C_REGISTER_TZ | HWMON_C_UPDATE_INTERVAL),
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_TYPE | HWMON_T_LABEL |
			   HWMON_T_MIN | HWMON_T_MIN_ALARM |
			   HWMON_T_MAX | HWMON_T_MAX_ALARM |
			   HWMON_T_OFFSET),
	HWMON_CHANNEL_INFO(in,
			   HWMON_I_INPUT | HWMON_I_LABEL |
			   HWMON_I_MIN | HWMON_I_MIN_ALARM |
			   HWMON_I_MAX | HWMON_I_MAX_ALARM,
			   HWMON_I_INPUT | HWMON_I_LABEL |
			   HWMON_I_MIN | HWMON_I_MIN_ALARM |
			   HWMON_I_MAX | HWMON_I_MAX_ALARM,
			   HWMON_I_INPUT | HWMON_I_LABEL |
			   HWMON_I_MIN | HWMON_I_MIN_ALARM |
			   HWMON_I_MAX | HWMON_I_MAX_ALARM,
			   HWMON_I_INPUT | HWMON_I_LABEL |
			   HWMON_I_MIN | HWMON_I_MIN_ALARM |
			   HWMON_I_MAX | HWMON_I_MAX_ALARM),
	NULL
};

#else  

static irqreturn_t pvt_hard_isr(int irq, void *data)
{
	struct pvt_hwmon *pvt = data;
	struct pvt_cache *cache;
	u32 val;

	 
	pvt_update(pvt->regs + PVT_INTR_MASK, PVT_INTR_DVALID,
		   PVT_INTR_DVALID);

	 
	val = readl(pvt->regs + PVT_DATA);
	if (!(val & PVT_DATA_VALID)) {
		dev_err(pvt->dev, "Got IRQ when data isn't valid\n");
		return IRQ_HANDLED;
	}

	cache = &pvt->cache[pvt->sensor];

	WRITE_ONCE(cache->data, FIELD_GET(PVT_DATA_DATA_MASK, val));

	complete(&cache->conversion);

	return IRQ_HANDLED;
}

#define pvt_soft_isr NULL

static inline umode_t pvt_limit_is_visible(enum pvt_sensor_type type)
{
	return 0;
}

static inline umode_t pvt_alarm_is_visible(enum pvt_sensor_type type)
{
	return 0;
}

static int pvt_read_data(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			 long *val)
{
	struct pvt_cache *cache = &pvt->cache[type];
	unsigned long timeout;
	u32 data;
	int ret;

	 
	ret = mutex_lock_interruptible(&pvt->iface_mtx);
	if (ret)
		return ret;

	pvt->sensor = type;
	pvt_set_mode(pvt, pvt_info[type].mode);

	 
	pvt_update(pvt->regs + PVT_INTR_MASK, PVT_INTR_DVALID, 0);
	pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, PVT_CTRL_EN);

	 
	timeout = 2 * usecs_to_jiffies(ktime_to_us(pvt->timeout));
	ret = wait_for_completion_timeout(&cache->conversion, timeout);

	pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, 0);
	pvt_update(pvt->regs + PVT_INTR_MASK, PVT_INTR_DVALID,
		   PVT_INTR_DVALID);

	data = READ_ONCE(cache->data);

	mutex_unlock(&pvt->iface_mtx);

	if (!ret)
		return -ETIMEDOUT;

	if (type == PVT_TEMP)
		*val = polynomial_calc(&poly_N_to_temp, data);
	else
		*val = polynomial_calc(&poly_N_to_volt, data);

	return 0;
}

static int pvt_read_limit(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			  bool is_low, long *val)
{
	return -EOPNOTSUPP;
}

static int pvt_write_limit(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			   bool is_low, long val)
{
	return -EOPNOTSUPP;
}

static int pvt_read_alarm(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			  bool is_low, long *val)
{
	return -EOPNOTSUPP;
}

static const struct hwmon_channel_info * const pvt_channel_info[] = {
	HWMON_CHANNEL_INFO(chip,
			   HWMON_C_REGISTER_TZ | HWMON_C_UPDATE_INTERVAL),
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_TYPE | HWMON_T_LABEL |
			   HWMON_T_OFFSET),
	HWMON_CHANNEL_INFO(in,
			   HWMON_I_INPUT | HWMON_I_LABEL,
			   HWMON_I_INPUT | HWMON_I_LABEL,
			   HWMON_I_INPUT | HWMON_I_LABEL,
			   HWMON_I_INPUT | HWMON_I_LABEL),
	NULL
};

#endif  

static inline bool pvt_hwmon_channel_is_valid(enum hwmon_sensor_types type,
					      int ch)
{
	switch (type) {
	case hwmon_temp:
		if (ch < 0 || ch >= PVT_TEMP_CHS)
			return false;
		break;
	case hwmon_in:
		if (ch < 0 || ch >= PVT_VOLT_CHS)
			return false;
		break;
	default:
		break;
	}

	 
	return true;
}

static umode_t pvt_hwmon_is_visible(const void *data,
				    enum hwmon_sensor_types type,
				    u32 attr, int ch)
{
	if (!pvt_hwmon_channel_is_valid(type, ch))
		return 0;

	switch (type) {
	case hwmon_chip:
		switch (attr) {
		case hwmon_chip_update_interval:
			return 0644;
		}
		break;
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
		case hwmon_temp_type:
		case hwmon_temp_label:
			return 0444;
		case hwmon_temp_min:
		case hwmon_temp_max:
			return pvt_limit_is_visible(ch);
		case hwmon_temp_min_alarm:
		case hwmon_temp_max_alarm:
			return pvt_alarm_is_visible(ch);
		case hwmon_temp_offset:
			return 0644;
		}
		break;
	case hwmon_in:
		switch (attr) {
		case hwmon_in_input:
		case hwmon_in_label:
			return 0444;
		case hwmon_in_min:
		case hwmon_in_max:
			return pvt_limit_is_visible(PVT_VOLT + ch);
		case hwmon_in_min_alarm:
		case hwmon_in_max_alarm:
			return pvt_alarm_is_visible(PVT_VOLT + ch);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int pvt_read_trim(struct pvt_hwmon *pvt, long *val)
{
	u32 data;

	data = readl(pvt->regs + PVT_CTRL);
	*val = FIELD_GET(PVT_CTRL_TRIM_MASK, data) * PVT_TRIM_STEP;

	return 0;
}

static int pvt_write_trim(struct pvt_hwmon *pvt, long val)
{
	u32 trim;
	int ret;

	 
	ret = mutex_lock_interruptible(&pvt->iface_mtx);
	if (ret)
		return ret;

	trim = pvt_calc_trim(val);
	pvt_set_trim(pvt, trim);

	mutex_unlock(&pvt->iface_mtx);

	return 0;
}

static int pvt_read_timeout(struct pvt_hwmon *pvt, long *val)
{
	int ret;

	ret = mutex_lock_interruptible(&pvt->iface_mtx);
	if (ret)
		return ret;

	 
	*val = ktime_to_ms(pvt->timeout);

	mutex_unlock(&pvt->iface_mtx);

	return 0;
}

static int pvt_write_timeout(struct pvt_hwmon *pvt, long val)
{
	unsigned long rate;
	ktime_t kt, cache;
	u32 data;
	int ret;

	rate = clk_get_rate(pvt->clks[PVT_CLOCK_REF].clk);
	if (!rate)
		return -ENODEV;

	 
	cache = kt = ms_to_ktime(val);
#if defined(CONFIG_SENSORS_BT1_PVT_ALARMS)
	kt = ktime_divns(kt, PVT_SENSORS_NUM);
#endif

	 
	kt = ktime_sub_ns(kt, PVT_TOUT_MIN);
	if (ktime_to_ns(kt) < 0)
		kt = ktime_set(0, 0);

	 
	data = ktime_divns(kt * rate, NSEC_PER_SEC);

	 
	ret = mutex_lock_interruptible(&pvt->iface_mtx);
	if (ret)
		return ret;

	pvt_set_tout(pvt, data);
	pvt->timeout = cache;

	mutex_unlock(&pvt->iface_mtx);

	return 0;
}

static int pvt_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
			  u32 attr, int ch, long *val)
{
	struct pvt_hwmon *pvt = dev_get_drvdata(dev);

	if (!pvt_hwmon_channel_is_valid(type, ch))
		return -EINVAL;

	switch (type) {
	case hwmon_chip:
		switch (attr) {
		case hwmon_chip_update_interval:
			return pvt_read_timeout(pvt, val);
		}
		break;
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			return pvt_read_data(pvt, ch, val);
		case hwmon_temp_type:
			*val = 1;
			return 0;
		case hwmon_temp_min:
			return pvt_read_limit(pvt, ch, true, val);
		case hwmon_temp_max:
			return pvt_read_limit(pvt, ch, false, val);
		case hwmon_temp_min_alarm:
			return pvt_read_alarm(pvt, ch, true, val);
		case hwmon_temp_max_alarm:
			return pvt_read_alarm(pvt, ch, false, val);
		case hwmon_temp_offset:
			return pvt_read_trim(pvt, val);
		}
		break;
	case hwmon_in:
		switch (attr) {
		case hwmon_in_input:
			return pvt_read_data(pvt, PVT_VOLT + ch, val);
		case hwmon_in_min:
			return pvt_read_limit(pvt, PVT_VOLT + ch, true, val);
		case hwmon_in_max:
			return pvt_read_limit(pvt, PVT_VOLT + ch, false, val);
		case hwmon_in_min_alarm:
			return pvt_read_alarm(pvt, PVT_VOLT + ch, true, val);
		case hwmon_in_max_alarm:
			return pvt_read_alarm(pvt, PVT_VOLT + ch, false, val);
		}
		break;
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static int pvt_hwmon_read_string(struct device *dev,
				 enum hwmon_sensor_types type,
				 u32 attr, int ch, const char **str)
{
	if (!pvt_hwmon_channel_is_valid(type, ch))
		return -EINVAL;

	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_label:
			*str = pvt_info[ch].label;
			return 0;
		}
		break;
	case hwmon_in:
		switch (attr) {
		case hwmon_in_label:
			*str = pvt_info[PVT_VOLT + ch].label;
			return 0;
		}
		break;
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static int pvt_hwmon_write(struct device *dev, enum hwmon_sensor_types type,
			   u32 attr, int ch, long val)
{
	struct pvt_hwmon *pvt = dev_get_drvdata(dev);

	if (!pvt_hwmon_channel_is_valid(type, ch))
		return -EINVAL;

	switch (type) {
	case hwmon_chip:
		switch (attr) {
		case hwmon_chip_update_interval:
			return pvt_write_timeout(pvt, val);
		}
		break;
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_min:
			return pvt_write_limit(pvt, ch, true, val);
		case hwmon_temp_max:
			return pvt_write_limit(pvt, ch, false, val);
		case hwmon_temp_offset:
			return pvt_write_trim(pvt, val);
		}
		break;
	case hwmon_in:
		switch (attr) {
		case hwmon_in_min:
			return pvt_write_limit(pvt, PVT_VOLT + ch, true, val);
		case hwmon_in_max:
			return pvt_write_limit(pvt, PVT_VOLT + ch, false, val);
		}
		break;
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static const struct hwmon_ops pvt_hwmon_ops = {
	.is_visible = pvt_hwmon_is_visible,
	.read = pvt_hwmon_read,
	.read_string = pvt_hwmon_read_string,
	.write = pvt_hwmon_write
};

static const struct hwmon_chip_info pvt_hwmon_info = {
	.ops = &pvt_hwmon_ops,
	.info = pvt_channel_info
};

static void pvt_clear_data(void *data)
{
	struct pvt_hwmon *pvt = data;
#if !defined(CONFIG_SENSORS_BT1_PVT_ALARMS)
	int idx;

	for (idx = 0; idx < PVT_SENSORS_NUM; ++idx)
		complete_all(&pvt->cache[idx].conversion);
#endif

	mutex_destroy(&pvt->iface_mtx);
}

static struct pvt_hwmon *pvt_create_data(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pvt_hwmon *pvt;
	int ret, idx;

	pvt = devm_kzalloc(dev, sizeof(*pvt), GFP_KERNEL);
	if (!pvt)
		return ERR_PTR(-ENOMEM);

	ret = devm_add_action(dev, pvt_clear_data, pvt);
	if (ret) {
		dev_err(dev, "Can't add PVT data clear action\n");
		return ERR_PTR(ret);
	}

	pvt->dev = dev;
	pvt->sensor = PVT_SENSOR_FIRST;
	mutex_init(&pvt->iface_mtx);

#if defined(CONFIG_SENSORS_BT1_PVT_ALARMS)
	for (idx = 0; idx < PVT_SENSORS_NUM; ++idx)
		seqlock_init(&pvt->cache[idx].data_seqlock);
#else
	for (idx = 0; idx < PVT_SENSORS_NUM; ++idx)
		init_completion(&pvt->cache[idx].conversion);
#endif

	return pvt;
}

static int pvt_request_regs(struct pvt_hwmon *pvt)
{
	struct platform_device *pdev = to_platform_device(pvt->dev);

	pvt->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pvt->regs))
		return PTR_ERR(pvt->regs);

	return 0;
}

static void pvt_disable_clks(void *data)
{
	struct pvt_hwmon *pvt = data;

	clk_bulk_disable_unprepare(PVT_CLOCK_NUM, pvt->clks);
}

static int pvt_request_clks(struct pvt_hwmon *pvt)
{
	int ret;

	pvt->clks[PVT_CLOCK_APB].id = "pclk";
	pvt->clks[PVT_CLOCK_REF].id = "ref";

	ret = devm_clk_bulk_get(pvt->dev, PVT_CLOCK_NUM, pvt->clks);
	if (ret) {
		dev_err(pvt->dev, "Couldn't get PVT clocks descriptors\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(PVT_CLOCK_NUM, pvt->clks);
	if (ret) {
		dev_err(pvt->dev, "Couldn't enable the PVT clocks\n");
		return ret;
	}

	ret = devm_add_action_or_reset(pvt->dev, pvt_disable_clks, pvt);
	if (ret) {
		dev_err(pvt->dev, "Can't add PVT clocks disable action\n");
		return ret;
	}

	return 0;
}

static int pvt_check_pwr(struct pvt_hwmon *pvt)
{
	unsigned long tout;
	int ret = 0;
	u32 data;

	 
	pvt_update(pvt->regs + PVT_INTR_MASK, PVT_INTR_ALL, PVT_INTR_ALL);
	pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, PVT_CTRL_EN);
	pvt_set_tout(pvt, 0);
	readl(pvt->regs + PVT_DATA);

	tout = PVT_TOUT_MIN / NSEC_PER_USEC;
	usleep_range(tout, 2 * tout);

	data = readl(pvt->regs + PVT_DATA);
	if (!(data & PVT_DATA_VALID)) {
		ret = -ENODEV;
		dev_err(pvt->dev, "Sensor is powered down\n");
	}

	pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, 0);

	return ret;
}

static int pvt_init_iface(struct pvt_hwmon *pvt)
{
	unsigned long rate;
	u32 trim, temp;

	rate = clk_get_rate(pvt->clks[PVT_CLOCK_REF].clk);
	if (!rate) {
		dev_err(pvt->dev, "Invalid reference clock rate\n");
		return -ENODEV;
	}

	 
	pvt_update(pvt->regs + PVT_INTR_MASK, PVT_INTR_ALL, PVT_INTR_ALL);
	pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, 0);
	readl(pvt->regs + PVT_CLR_INTR);
	readl(pvt->regs + PVT_DATA);

	 
	pvt_set_mode(pvt, pvt_info[pvt->sensor].mode);
	pvt_set_tout(pvt, PVT_TOUT_DEF);

	 
#if defined(CONFIG_SENSORS_BT1_PVT_ALARMS)
	pvt->timeout = ktime_set(PVT_SENSORS_NUM * PVT_TOUT_DEF, 0);
	pvt->timeout = ktime_divns(pvt->timeout, rate);
	pvt->timeout = ktime_add_ns(pvt->timeout, PVT_SENSORS_NUM * PVT_TOUT_MIN);
#else
	pvt->timeout = ktime_set(PVT_TOUT_DEF, 0);
	pvt->timeout = ktime_divns(pvt->timeout, rate);
	pvt->timeout = ktime_add_ns(pvt->timeout, PVT_TOUT_MIN);
#endif

	trim = PVT_TRIM_DEF;
	if (!of_property_read_u32(pvt->dev->of_node,
	     "baikal,pvt-temp-offset-millicelsius", &temp))
		trim = pvt_calc_trim(temp);

	pvt_set_trim(pvt, trim);

	return 0;
}

static int pvt_request_irq(struct pvt_hwmon *pvt)
{
	struct platform_device *pdev = to_platform_device(pvt->dev);
	int ret;

	pvt->irq = platform_get_irq(pdev, 0);
	if (pvt->irq < 0)
		return pvt->irq;

	ret = devm_request_threaded_irq(pvt->dev, pvt->irq,
					pvt_hard_isr, pvt_soft_isr,
#if defined(CONFIG_SENSORS_BT1_PVT_ALARMS)
					IRQF_SHARED | IRQF_TRIGGER_HIGH |
					IRQF_ONESHOT,
#else
					IRQF_SHARED | IRQF_TRIGGER_HIGH,
#endif
					"pvt", pvt);
	if (ret) {
		dev_err(pvt->dev, "Couldn't request PVT IRQ\n");
		return ret;
	}

	return 0;
}

static int pvt_create_hwmon(struct pvt_hwmon *pvt)
{
	pvt->hwmon = devm_hwmon_device_register_with_info(pvt->dev, "pvt", pvt,
		&pvt_hwmon_info, NULL);
	if (IS_ERR(pvt->hwmon)) {
		dev_err(pvt->dev, "Couldn't create hwmon device\n");
		return PTR_ERR(pvt->hwmon);
	}

	return 0;
}

#if defined(CONFIG_SENSORS_BT1_PVT_ALARMS)

static void pvt_disable_iface(void *data)
{
	struct pvt_hwmon *pvt = data;

	mutex_lock(&pvt->iface_mtx);
	pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, 0);
	pvt_update(pvt->regs + PVT_INTR_MASK, PVT_INTR_DVALID,
		   PVT_INTR_DVALID);
	mutex_unlock(&pvt->iface_mtx);
}

static int pvt_enable_iface(struct pvt_hwmon *pvt)
{
	int ret;

	ret = devm_add_action(pvt->dev, pvt_disable_iface, pvt);
	if (ret) {
		dev_err(pvt->dev, "Can't add PVT disable interface action\n");
		return ret;
	}

	 
	mutex_lock(&pvt->iface_mtx);
	pvt_update(pvt->regs + PVT_INTR_MASK, PVT_INTR_DVALID, 0);
	pvt_update(pvt->regs + PVT_CTRL, PVT_CTRL_EN, PVT_CTRL_EN);
	mutex_unlock(&pvt->iface_mtx);

	return 0;
}

#else  

static int pvt_enable_iface(struct pvt_hwmon *pvt)
{
	return 0;
}

#endif  

static int pvt_probe(struct platform_device *pdev)
{
	struct pvt_hwmon *pvt;
	int ret;

	pvt = pvt_create_data(pdev);
	if (IS_ERR(pvt))
		return PTR_ERR(pvt);

	ret = pvt_request_regs(pvt);
	if (ret)
		return ret;

	ret = pvt_request_clks(pvt);
	if (ret)
		return ret;

	ret = pvt_check_pwr(pvt);
	if (ret)
		return ret;

	ret = pvt_init_iface(pvt);
	if (ret)
		return ret;

	ret = pvt_request_irq(pvt);
	if (ret)
		return ret;

	ret = pvt_create_hwmon(pvt);
	if (ret)
		return ret;

	ret = pvt_enable_iface(pvt);
	if (ret)
		return ret;

	return 0;
}

static const struct of_device_id pvt_of_match[] = {
	{ .compatible = "baikal,bt1-pvt" },
	{ }
};
MODULE_DEVICE_TABLE(of, pvt_of_match);

static struct platform_driver pvt_driver = {
	.probe = pvt_probe,
	.driver = {
		.name = "bt1-pvt",
		.of_match_table = pvt_of_match
	}
};
module_platform_driver(pvt_driver);

MODULE_AUTHOR("Maxim Kaurkin <maxim.kaurkin@baikalelectronics.ru>");
MODULE_DESCRIPTION("Baikal-T1 PVT driver");
MODULE_LICENSE("GPL v2");
