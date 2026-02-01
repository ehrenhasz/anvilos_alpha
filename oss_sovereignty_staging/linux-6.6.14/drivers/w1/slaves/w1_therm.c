
 

#include <asm/types.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/hwmon.h>
#include <linux/string.h>
#include <linux/jiffies.h>

#include <linux/w1.h>

#define W1_THERM_DS18S20	0x10
#define W1_THERM_DS1822		0x22
#define W1_THERM_DS18B20	0x28
#define W1_THERM_DS1825		0x3B
#define W1_THERM_DS28EA00	0x42

 
static int w1_strong_pullup = 1;
module_param_named(strong_pullup, w1_strong_pullup, int, 0);

 
static u16 bulk_read_device_counter;  

 
#define W1_RECALL_EEPROM	0xB8

 
#define W1_THERM_MAX_TRY		5

 
#define W1_THERM_RETRY_DELAY		20

 
#define W1_THERM_EEPROM_WRITE_DELAY	10

#define EEPROM_CMD_WRITE    "save"	 
#define EEPROM_CMD_READ     "restore"	 
#define BULK_TRIGGER_CMD    "trigger"	 

#define MIN_TEMP	-55	 
#define MAX_TEMP	125	 

 
#define CONV_TIME_DEFAULT 0
#define CONV_TIME_MEASURE 1

 
#define W1_THERM_CHECK_RESULT 1	 
#define W1_THERM_POLL_COMPLETION 2	 
#define W1_THERM_FEATURES_MASK 3		 

 
#define W1_POLL_PERIOD 32
#define W1_POLL_CONVERT_TEMP 2000	 
#define W1_POLL_RECALL_EEPROM 500	 

 
 
#define W1_THERM_RESOLUTION_MASK 0xE0
 
#define W1_THERM_RESOLUTION_SHIFT 5
 
#define W1_THERM_RESOLUTION_SHIFT 5
 
#define W1_THERM_RESOLUTION_MIN 9
 
#define W1_THERM_RESOLUTION_MAX 14

 

 
#define SLAVE_SPECIFIC_FUNC(sl) \
	(((struct w1_therm_family_data *)(sl->family_data))->specific_functions)

 
#define SLAVE_POWERMODE(sl) \
	(((struct w1_therm_family_data *)(sl->family_data))->external_powered)

 
#define SLAVE_RESOLUTION(sl) \
	(((struct w1_therm_family_data *)(sl->family_data))->resolution)

 
 #define SLAVE_CONV_TIME_OVERRIDE(sl) \
	(((struct w1_therm_family_data *)(sl->family_data))->conv_time_override)

 
 #define SLAVE_FEATURES(sl) \
	(((struct w1_therm_family_data *)(sl->family_data))->features)

 
#define SLAVE_CONVERT_TRIGGERED(sl) \
	(((struct w1_therm_family_data *)(sl->family_data))->convert_triggered)

 
#define THERM_REFCNT(family_data) \
	(&((struct w1_therm_family_data *)family_data)->refcnt)

 

 
struct w1_therm_family_converter {
	u8		broken;
	u16		reserved;
	struct w1_family	*f;
	int		(*convert)(u8 rom[9]);
	int		(*get_conversion_time)(struct w1_slave *sl);
	int		(*set_resolution)(struct w1_slave *sl, int val);
	int		(*get_resolution)(struct w1_slave *sl);
	int		(*write_data)(struct w1_slave *sl, const u8 *data);
	bool		bulk_read;
};

 
struct w1_therm_family_data {
	uint8_t rom[9];
	atomic_t refcnt;
	int external_powered;
	int resolution;
	int convert_triggered;
	int conv_time_override;
	unsigned int features;
	struct w1_therm_family_converter *specific_functions;
};

 
struct therm_info {
	u8 rom[9];
	u8 crc;
	u8 verdict;
};

 

 
static int reset_select_slave(struct w1_slave *sl);

 
static int convert_t(struct w1_slave *sl, struct therm_info *info);

 
static int read_scratchpad(struct w1_slave *sl, struct therm_info *info);

 
static int write_scratchpad(struct w1_slave *sl, const u8 *data, u8 nb_bytes);

 
static int copy_scratchpad(struct w1_slave *sl);

 
static int recall_eeprom(struct w1_slave *sl);

 
static int read_powermode(struct w1_slave *sl);

 
static int trigger_bulk_read(struct w1_master *dev_master);

 

static ssize_t w1_slave_show(struct device *device,
	struct device_attribute *attr, char *buf);

static ssize_t w1_slave_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size);

static ssize_t w1_seq_show(struct device *device,
	struct device_attribute *attr, char *buf);

static ssize_t temperature_show(struct device *device,
	struct device_attribute *attr, char *buf);

static ssize_t ext_power_show(struct device *device,
	struct device_attribute *attr, char *buf);

static ssize_t resolution_show(struct device *device,
	struct device_attribute *attr, char *buf);

static ssize_t resolution_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size);

static ssize_t eeprom_cmd_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size);

static ssize_t alarms_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size);

static ssize_t alarms_show(struct device *device,
	struct device_attribute *attr, char *buf);

static ssize_t therm_bulk_read_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size);

static ssize_t therm_bulk_read_show(struct device *device,
	struct device_attribute *attr, char *buf);

static ssize_t conv_time_show(struct device *device,
			      struct device_attribute *attr, char *buf);

static ssize_t conv_time_store(struct device *device,
			       struct device_attribute *attr, const char *buf,
			       size_t size);

static ssize_t features_show(struct device *device,
			      struct device_attribute *attr, char *buf);

static ssize_t features_store(struct device *device,
			       struct device_attribute *attr, const char *buf,
			       size_t size);
 

static DEVICE_ATTR_RW(w1_slave);
static DEVICE_ATTR_RO(w1_seq);
static DEVICE_ATTR_RO(temperature);
static DEVICE_ATTR_RO(ext_power);
static DEVICE_ATTR_RW(resolution);
static DEVICE_ATTR_WO(eeprom_cmd);
static DEVICE_ATTR_RW(alarms);
static DEVICE_ATTR_RW(conv_time);
static DEVICE_ATTR_RW(features);

static DEVICE_ATTR_RW(therm_bulk_read);  

 

 
static int w1_therm_add_slave(struct w1_slave *sl);

 
static void w1_therm_remove_slave(struct w1_slave *sl);

 

static struct attribute *w1_therm_attrs[] = {
	&dev_attr_w1_slave.attr,
	&dev_attr_temperature.attr,
	&dev_attr_ext_power.attr,
	&dev_attr_resolution.attr,
	&dev_attr_eeprom_cmd.attr,
	&dev_attr_alarms.attr,
	&dev_attr_conv_time.attr,
	&dev_attr_features.attr,
	NULL,
};

static struct attribute *w1_ds18s20_attrs[] = {
	&dev_attr_w1_slave.attr,
	&dev_attr_temperature.attr,
	&dev_attr_ext_power.attr,
	&dev_attr_eeprom_cmd.attr,
	&dev_attr_alarms.attr,
	&dev_attr_conv_time.attr,
	&dev_attr_features.attr,
	NULL,
};

static struct attribute *w1_ds28ea00_attrs[] = {
	&dev_attr_w1_slave.attr,
	&dev_attr_w1_seq.attr,
	&dev_attr_temperature.attr,
	&dev_attr_ext_power.attr,
	&dev_attr_resolution.attr,
	&dev_attr_eeprom_cmd.attr,
	&dev_attr_alarms.attr,
	&dev_attr_conv_time.attr,
	&dev_attr_features.attr,
	NULL,
};

 

ATTRIBUTE_GROUPS(w1_therm);
ATTRIBUTE_GROUPS(w1_ds18s20);
ATTRIBUTE_GROUPS(w1_ds28ea00);

#if IS_REACHABLE(CONFIG_HWMON)
static int w1_read_temp(struct device *dev, u32 attr, int channel,
			long *val);

static umode_t w1_is_visible(const void *_data, enum hwmon_sensor_types type,
			     u32 attr, int channel)
{
	return attr == hwmon_temp_input ? 0444 : 0;
}

static int w1_read(struct device *dev, enum hwmon_sensor_types type,
		   u32 attr, int channel, long *val)
{
	switch (type) {
	case hwmon_temp:
		return w1_read_temp(dev, attr, channel, val);
	default:
		return -EOPNOTSUPP;
	}
}

static const u32 w1_temp_config[] = {
	HWMON_T_INPUT,
	0
};

static const struct hwmon_channel_info w1_temp = {
	.type = hwmon_temp,
	.config = w1_temp_config,
};

static const struct hwmon_channel_info * const w1_info[] = {
	&w1_temp,
	NULL
};

static const struct hwmon_ops w1_hwmon_ops = {
	.is_visible = w1_is_visible,
	.read = w1_read,
};

static const struct hwmon_chip_info w1_chip_info = {
	.ops = &w1_hwmon_ops,
	.info = w1_info,
};
#define W1_CHIPINFO	(&w1_chip_info)
#else
#define W1_CHIPINFO	NULL
#endif

 

static const struct w1_family_ops w1_therm_fops = {
	.add_slave	= w1_therm_add_slave,
	.remove_slave	= w1_therm_remove_slave,
	.groups		= w1_therm_groups,
	.chip_info	= W1_CHIPINFO,
};

static const struct w1_family_ops w1_ds18s20_fops = {
	.add_slave	= w1_therm_add_slave,
	.remove_slave	= w1_therm_remove_slave,
	.groups		= w1_ds18s20_groups,
	.chip_info	= W1_CHIPINFO,
};

static const struct w1_family_ops w1_ds28ea00_fops = {
	.add_slave	= w1_therm_add_slave,
	.remove_slave	= w1_therm_remove_slave,
	.groups		= w1_ds28ea00_groups,
	.chip_info	= W1_CHIPINFO,
};

 

static struct w1_family w1_therm_family_DS18S20 = {
	.fid = W1_THERM_DS18S20,
	.fops = &w1_ds18s20_fops,
};

static struct w1_family w1_therm_family_DS18B20 = {
	.fid = W1_THERM_DS18B20,
	.fops = &w1_therm_fops,
};

static struct w1_family w1_therm_family_DS1822 = {
	.fid = W1_THERM_DS1822,
	.fops = &w1_therm_fops,
};

static struct w1_family w1_therm_family_DS28EA00 = {
	.fid = W1_THERM_DS28EA00,
	.fops = &w1_ds28ea00_fops,
};

static struct w1_family w1_therm_family_DS1825 = {
	.fid = W1_THERM_DS1825,
	.fops = &w1_therm_fops,
};

 

static inline int w1_DS18B20_convert_time(struct w1_slave *sl)
{
	int ret;

	if (!sl->family_data)
		return -ENODEV;	 

	if (SLAVE_CONV_TIME_OVERRIDE(sl) != CONV_TIME_DEFAULT)
		return SLAVE_CONV_TIME_OVERRIDE(sl);

	 
	switch (SLAVE_RESOLUTION(sl)) {
	case 9:
		ret = 95;
		break;
	case 10:
		ret = 190;
		break;
	case 11:
		ret = 375;
		break;
	case 12:
		ret = 750;
		break;
	case 13:
		ret = 850;   
		break;
	case 14:
		ret = 1600;  
		break;
	default:
		ret = 750;
	}
	return ret;
}

static inline int w1_DS18S20_convert_time(struct w1_slave *sl)
{
	if (!sl->family_data)
		return -ENODEV;	 

	if (SLAVE_CONV_TIME_OVERRIDE(sl) == CONV_TIME_DEFAULT)
		return 750;  
	else
		return SLAVE_CONV_TIME_OVERRIDE(sl);
}

static inline int w1_DS1825_convert_time(struct w1_slave *sl)
{
	int ret;

	if (!sl->family_data)
		return -ENODEV;	 

	if (SLAVE_CONV_TIME_OVERRIDE(sl) != CONV_TIME_DEFAULT)
		return SLAVE_CONV_TIME_OVERRIDE(sl);

	 
	switch (SLAVE_RESOLUTION(sl)) {
	case 9:
		ret = 95;
		break;
	case 10:
		ret = 190;
		break;
	case 11:
		ret = 375;
		break;
	case 12:
		ret = 750;
		break;
	case 14:
		ret = 100;  
		break;
	default:
		ret = 750;
	}
	return ret;
}

static inline int w1_DS18B20_write_data(struct w1_slave *sl,
				const u8 *data)
{
	return write_scratchpad(sl, data, 3);
}

static inline int w1_DS18S20_write_data(struct w1_slave *sl,
				const u8 *data)
{
	 
	return write_scratchpad(sl, data, 2);
}

static inline int w1_DS18B20_set_resolution(struct w1_slave *sl, int val)
{
	int ret;
	struct therm_info info, info2;

	 
	 
	 
	if (val < W1_THERM_RESOLUTION_MIN || val > W1_THERM_RESOLUTION_MAX)
		return -EINVAL;

	 
	val = (val - W1_THERM_RESOLUTION_MIN) << W1_THERM_RESOLUTION_SHIFT;

	 
	ret = read_scratchpad(sl, &info);

	if (ret)
		return ret;


	info.rom[4] &= ~W1_THERM_RESOLUTION_MASK;
	info.rom[4] |= val;

	 
	ret = w1_DS18B20_write_data(sl, info.rom + 2);
	if (ret)
		return ret;

	 
	ret = read_scratchpad(sl, &info2);
	if (ret)
		 
		return ret;

	if ((info2.rom[4] & W1_THERM_RESOLUTION_MASK) == (info.rom[4] & W1_THERM_RESOLUTION_MASK))
		return 0;

	 
	return -EIO;
}

static inline int w1_DS18B20_get_resolution(struct w1_slave *sl)
{
	int ret;
	int resolution;
	struct therm_info info;

	ret = read_scratchpad(sl, &info);

	if (ret)
		return ret;

	resolution = ((info.rom[4] & W1_THERM_RESOLUTION_MASK) >> W1_THERM_RESOLUTION_SHIFT)
		+ W1_THERM_RESOLUTION_MIN;
	 
	if (resolution > W1_THERM_RESOLUTION_MAX)
		resolution = W1_THERM_RESOLUTION_MAX;

	return resolution;
}

 
static inline int w1_DS18B20_convert_temp(u8 rom[9])
{
	u16 bv;
	s16 t;

	 
	bv = le16_to_cpup((__le16 *)rom);

	 
	if (rom[4] & 0x80) {
		 
		 
		bv = (bv << 2) | (rom[4] & 3);
		t = (s16) bv;	 
		return (int)t * 1000 / 64;	 
	}
	t = (s16)bv;	 
	return (int)t * 1000 / 16;	 
}

 
static inline int w1_DS18S20_convert_temp(u8 rom[9])
{
	int t, h;

	if (!rom[7]) {
		pr_debug("%s: Invalid argument for conversion\n", __func__);
		return 0;
	}

	if (rom[1] == 0)
		t = ((s32)rom[0] >> 1)*1000;
	else
		t = 1000*(-1*(s32)(0x100-rom[0]) >> 1);

	t -= 250;
	h = 1000*((s32)rom[7] - (s32)rom[6]);
	h /= (s32)rom[7];
	t += h;

	return t;
}

 

static inline int w1_DS1825_convert_temp(u8 rom[9])
{
	u16 bv;
	s16 t;

	 
	bv = le16_to_cpup((__le16 *)rom);

	 
	if (rom[4] & 0x80) {
		 
		 
		bv = (bv & 0xFFFC);  
	}
	t = (s16)bv;	 
	return (int)t * 1000 / 16;	 
}

 
 

static struct w1_therm_family_converter w1_therm_families[] = {
	{
		.f				= &w1_therm_family_DS18S20,
		.convert			= w1_DS18S20_convert_temp,
		.get_conversion_time	= w1_DS18S20_convert_time,
		.set_resolution		= NULL,	 
		.get_resolution		= NULL,	 
		.write_data			= w1_DS18S20_write_data,
		.bulk_read			= true
	},
	{
		.f				= &w1_therm_family_DS1822,
		.convert			= w1_DS18B20_convert_temp,
		.get_conversion_time	= w1_DS18B20_convert_time,
		.set_resolution		= w1_DS18B20_set_resolution,
		.get_resolution		= w1_DS18B20_get_resolution,
		.write_data			= w1_DS18B20_write_data,
		.bulk_read			= true
	},
	{
		 
		.f				= &w1_therm_family_DS18B20,
		.convert			= w1_DS18B20_convert_temp,
		.get_conversion_time	= w1_DS18B20_convert_time,
		.set_resolution		= w1_DS18B20_set_resolution,
		.get_resolution		= w1_DS18B20_get_resolution,
		.write_data			= w1_DS18B20_write_data,
		.bulk_read			= true
	},
	{
		.f				= &w1_therm_family_DS28EA00,
		.convert			= w1_DS18B20_convert_temp,
		.get_conversion_time	= w1_DS18B20_convert_time,
		.set_resolution		= w1_DS18B20_set_resolution,
		.get_resolution		= w1_DS18B20_get_resolution,
		.write_data			= w1_DS18B20_write_data,
		.bulk_read			= false
	},
	{
		 
		.f				= &w1_therm_family_DS1825,
		.convert			= w1_DS1825_convert_temp,
		.get_conversion_time	= w1_DS1825_convert_time,
		.set_resolution		= w1_DS18B20_set_resolution,
		.get_resolution		= w1_DS18B20_get_resolution,
		.write_data			= w1_DS18B20_write_data,
		.bulk_read			= true
	}
};

 

 
static struct w1_therm_family_converter *device_family(struct w1_slave *sl)
{
	struct w1_therm_family_converter *ret = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(w1_therm_families); ++i) {
		if (w1_therm_families[i].f->fid == sl->family->fid) {
			ret = &w1_therm_families[i];
			break;
		}
	}
	return ret;
}

 
static inline bool bus_mutex_lock(struct mutex *lock)
{
	int max_trying = W1_THERM_MAX_TRY;

	 
	while (mutex_lock_interruptible(lock) != 0 && max_trying > 0) {
		unsigned long sleep_rem;

		sleep_rem = msleep_interruptible(W1_THERM_RETRY_DELAY);
		if (!sleep_rem)
			max_trying--;
	}

	if (!max_trying)
		return false;	 

	return true;
}

 
static int check_family_data(struct w1_slave *sl)
{
	if ((!sl->family_data) || (!SLAVE_SPECIFIC_FUNC(sl))) {
		dev_info(&sl->dev,
			 "%s: Device is not supported by the driver\n", __func__);
		return -EINVAL;   
	}
	return 0;
}

 
static inline bool bulk_read_support(struct w1_slave *sl)
{
	if (SLAVE_SPECIFIC_FUNC(sl))
		return SLAVE_SPECIFIC_FUNC(sl)->bulk_read;

	dev_info(&sl->dev,
		"%s: Device not supported by the driver\n", __func__);

	return false;   
}

 
static inline int conversion_time(struct w1_slave *sl)
{
	if (SLAVE_SPECIFIC_FUNC(sl))
		return SLAVE_SPECIFIC_FUNC(sl)->get_conversion_time(sl);

	dev_info(&sl->dev,
		"%s: Device not supported by the driver\n", __func__);

	return -ENODEV;   
}

 
static inline int temperature_from_RAM(struct w1_slave *sl, u8 rom[9])
{
	if (SLAVE_SPECIFIC_FUNC(sl))
		return SLAVE_SPECIFIC_FUNC(sl)->convert(rom);

	dev_info(&sl->dev,
		"%s: Device not supported by the driver\n", __func__);

	return 0;   
}

 
static inline s8 int_to_short(int i)
{
	 
	i = clamp(i, MIN_TEMP, MAX_TEMP);
	return (s8) i;
}

 

static int w1_therm_add_slave(struct w1_slave *sl)
{
	struct w1_therm_family_converter *sl_family_conv;

	 
	sl->family_data = kzalloc(sizeof(struct w1_therm_family_data),
		GFP_KERNEL);
	if (!sl->family_data)
		return -ENOMEM;

	atomic_set(THERM_REFCNT(sl->family_data), 1);

	 
	sl_family_conv = device_family(sl);
	if (!sl_family_conv) {
		kfree(sl->family_data);
		return -ENODEV;
	}
	 
	SLAVE_SPECIFIC_FUNC(sl) = sl_family_conv;

	if (bulk_read_support(sl)) {
		 
		if (!bulk_read_device_counter) {
			int err = device_create_file(&sl->master->dev,
				&dev_attr_therm_bulk_read);

			if (err)
				dev_warn(&sl->dev,
				"%s: Device has been added, but bulk read is unavailable. err=%d\n",
				__func__, err);
		}
		 
		bulk_read_device_counter++;
	}

	 
	SLAVE_POWERMODE(sl) = read_powermode(sl);

	if (SLAVE_POWERMODE(sl) < 0) {
		 
		dev_warn(&sl->dev,
			"%s: Device has been added, but power_mode may be corrupted. err=%d\n",
			 __func__, SLAVE_POWERMODE(sl));
	}

	 
	if (SLAVE_SPECIFIC_FUNC(sl)->get_resolution) {
		SLAVE_RESOLUTION(sl) =
			SLAVE_SPECIFIC_FUNC(sl)->get_resolution(sl);
		if (SLAVE_RESOLUTION(sl) < 0) {
			 
			dev_warn(&sl->dev,
				"%s:Device has been added, but resolution may be corrupted. err=%d\n",
				__func__, SLAVE_RESOLUTION(sl));
		}
	}

	 
	SLAVE_CONVERT_TRIGGERED(sl) = 0;

	return 0;
}

static void w1_therm_remove_slave(struct w1_slave *sl)
{
	int refcnt = atomic_sub_return(1, THERM_REFCNT(sl->family_data));

	if (bulk_read_support(sl)) {
		bulk_read_device_counter--;
		 
		if (!bulk_read_device_counter)
			device_remove_file(&sl->master->dev,
				&dev_attr_therm_bulk_read);
	}

	while (refcnt) {
		msleep(1000);
		refcnt = atomic_read(THERM_REFCNT(sl->family_data));
	}
	kfree(sl->family_data);
	sl->family_data = NULL;
}

 

 
static int reset_select_slave(struct w1_slave *sl)
{
	u8 match[9] = { W1_MATCH_ROM, };
	u64 rn = le64_to_cpu(*((u64 *)&sl->reg_num));

	if (w1_reset_bus(sl->master))
		return -ENODEV;

	memcpy(&match[1], &rn, 8);
	w1_write_block(sl->master, match, 9);

	return 0;
}

 
static int w1_poll_completion(struct w1_master *dev_master, int tout_ms)
{
	int i;

	for (i = 0; i < tout_ms/W1_POLL_PERIOD; i++) {
		 
		msleep(W1_POLL_PERIOD);

		 
		if (w1_read_8(dev_master) == 0xFF)
			break;
	}
	if (i == tout_ms/W1_POLL_PERIOD)
		return -EIO;

	return 0;
}

static int convert_t(struct w1_slave *sl, struct therm_info *info)
{
	struct w1_master *dev_master = sl->master;
	int max_trying = W1_THERM_MAX_TRY;
	int t_conv;
	int ret = -ENODEV;
	bool strong_pullup;

	if (!sl->family_data)
		goto error;

	strong_pullup = (w1_strong_pullup == 2 ||
					(!SLAVE_POWERMODE(sl) &&
					w1_strong_pullup));

	if (strong_pullup && SLAVE_FEATURES(sl) & W1_THERM_POLL_COMPLETION) {
		dev_warn(&sl->dev,
			"%s: Disabling W1_THERM_POLL_COMPLETION in parasite power mode.\n",
			__func__);
		SLAVE_FEATURES(sl) &= ~W1_THERM_POLL_COMPLETION;
	}

	 
	t_conv = conversion_time(sl);

	memset(info->rom, 0, sizeof(info->rom));

	 
	atomic_inc(THERM_REFCNT(sl->family_data));

	if (!bus_mutex_lock(&dev_master->bus_mutex)) {
		ret = -EAGAIN;	 
		goto dec_refcnt;
	}

	while (max_trying-- && ret) {  

		info->verdict = 0;
		info->crc = 0;
		 
		if (!reset_select_slave(sl)) {
			unsigned long sleep_rem;

			 
			if (strong_pullup)
				w1_next_pullup(dev_master, t_conv);

			w1_write_8(dev_master, W1_CONVERT_TEMP);

			if (SLAVE_FEATURES(sl) & W1_THERM_POLL_COMPLETION) {
				ret = w1_poll_completion(dev_master, W1_POLL_CONVERT_TEMP);
				if (ret) {
					dev_dbg(&sl->dev, "%s: Timeout\n", __func__);
					goto mt_unlock;
				}
				mutex_unlock(&dev_master->bus_mutex);
			} else if (!strong_pullup) {  
				sleep_rem = msleep_interruptible(t_conv);
				if (sleep_rem != 0) {
					ret = -EINTR;
					goto mt_unlock;
				}
				mutex_unlock(&dev_master->bus_mutex);
			} else {  
				mutex_unlock(&dev_master->bus_mutex);
				sleep_rem = msleep_interruptible(t_conv);
				if (sleep_rem != 0) {
					ret = -EINTR;
					goto dec_refcnt;
				}
			}
			ret = read_scratchpad(sl, info);

			 
			if ((SLAVE_FEATURES(sl) & W1_THERM_CHECK_RESULT) &&
				(info->rom[6] == 0xC) &&
				((info->rom[1] == 0x5 && info->rom[0] == 0x50) ||
				(info->rom[1] == 0x7 && info->rom[0] == 0xFF))
			) {
				 
				ret = -EIO;
			}

			goto dec_refcnt;
		}

	}

mt_unlock:
	mutex_unlock(&dev_master->bus_mutex);
dec_refcnt:
	atomic_dec(THERM_REFCNT(sl->family_data));
error:
	return ret;
}

static int conv_time_measure(struct w1_slave *sl, int *conv_time)
{
	struct therm_info inf,
		*info = &inf;
	struct w1_master *dev_master = sl->master;
	int max_trying = W1_THERM_MAX_TRY;
	int ret = -ENODEV;
	bool strong_pullup;

	if (!sl->family_data)
		goto error;

	strong_pullup = (w1_strong_pullup == 2 ||
		(!SLAVE_POWERMODE(sl) &&
		w1_strong_pullup));

	if (strong_pullup) {
		pr_info("%s: Measure with strong_pullup is not supported.\n", __func__);
		return -EINVAL;
	}

	memset(info->rom, 0, sizeof(info->rom));

	 
	atomic_inc(THERM_REFCNT(sl->family_data));

	if (!bus_mutex_lock(&dev_master->bus_mutex)) {
		ret = -EAGAIN;	 
		goto dec_refcnt;
	}

	while (max_trying-- && ret) {  
		info->verdict = 0;
		info->crc = 0;
		 
		if (!reset_select_slave(sl)) {
			int j_start, j_end;

			 
			w1_write_8(dev_master, W1_CONVERT_TEMP);

			j_start = jiffies;
			ret = w1_poll_completion(dev_master, W1_POLL_CONVERT_TEMP);
			if (ret) {
				dev_dbg(&sl->dev, "%s: Timeout\n", __func__);
				goto mt_unlock;
			}
			j_end = jiffies;
			 
			*conv_time = jiffies_to_msecs(j_end-j_start)*12/10;
			pr_debug("W1 Measure complete, conv_time = %d, HZ=%d.\n",
				*conv_time, HZ);
			if (*conv_time <= CONV_TIME_MEASURE) {
				ret = -EIO;
				goto mt_unlock;
			}
			mutex_unlock(&dev_master->bus_mutex);
			ret = read_scratchpad(sl, info);
			goto dec_refcnt;
		}

	}
mt_unlock:
	mutex_unlock(&dev_master->bus_mutex);
dec_refcnt:
	atomic_dec(THERM_REFCNT(sl->family_data));
error:
	return ret;
}

static int read_scratchpad(struct w1_slave *sl, struct therm_info *info)
{
	struct w1_master *dev_master = sl->master;
	int max_trying = W1_THERM_MAX_TRY;
	int ret = -ENODEV;

	info->verdict = 0;

	if (!sl->family_data)
		goto error;

	memset(info->rom, 0, sizeof(info->rom));

	 
	atomic_inc(THERM_REFCNT(sl->family_data));

	if (!bus_mutex_lock(&dev_master->bus_mutex)) {
		ret = -EAGAIN;	 
		goto dec_refcnt;
	}

	while (max_trying-- && ret) {  
		 
		if (!reset_select_slave(sl)) {
			u8 nb_bytes_read;

			w1_write_8(dev_master, W1_READ_SCRATCHPAD);

			nb_bytes_read = w1_read_block(dev_master, info->rom, 9);
			if (nb_bytes_read != 9) {
				dev_warn(&sl->dev,
					"w1_read_block(): returned %u instead of 9.\n",
					nb_bytes_read);
				ret = -EIO;
			}

			info->crc = w1_calc_crc8(info->rom, 8);

			if (info->rom[8] == info->crc) {
				info->verdict = 1;
				ret = 0;
			} else
				ret = -EIO;  
		}

	}
	mutex_unlock(&dev_master->bus_mutex);

dec_refcnt:
	atomic_dec(THERM_REFCNT(sl->family_data));
error:
	return ret;
}

static int write_scratchpad(struct w1_slave *sl, const u8 *data, u8 nb_bytes)
{
	struct w1_master *dev_master = sl->master;
	int max_trying = W1_THERM_MAX_TRY;
	int ret = -ENODEV;

	if (!sl->family_data)
		goto error;

	 
	atomic_inc(THERM_REFCNT(sl->family_data));

	if (!bus_mutex_lock(&dev_master->bus_mutex)) {
		ret = -EAGAIN;	 
		goto dec_refcnt;
	}

	while (max_trying-- && ret) {  
		 
		if (!reset_select_slave(sl)) {
			w1_write_8(dev_master, W1_WRITE_SCRATCHPAD);
			w1_write_block(dev_master, data, nb_bytes);
			ret = 0;
		}
	}
	mutex_unlock(&dev_master->bus_mutex);

dec_refcnt:
	atomic_dec(THERM_REFCNT(sl->family_data));
error:
	return ret;
}

static int copy_scratchpad(struct w1_slave *sl)
{
	struct w1_master *dev_master = sl->master;
	int max_trying = W1_THERM_MAX_TRY;
	int t_write, ret = -ENODEV;
	bool strong_pullup;

	if (!sl->family_data)
		goto error;

	t_write = W1_THERM_EEPROM_WRITE_DELAY;
	strong_pullup = (w1_strong_pullup == 2 ||
					(!SLAVE_POWERMODE(sl) &&
					w1_strong_pullup));

	 
	atomic_inc(THERM_REFCNT(sl->family_data));

	if (!bus_mutex_lock(&dev_master->bus_mutex)) {
		ret = -EAGAIN;	 
		goto dec_refcnt;
	}

	while (max_trying-- && ret) {  
		 
		if (!reset_select_slave(sl)) {
			unsigned long sleep_rem;

			 
			if (strong_pullup)
				w1_next_pullup(dev_master, t_write);

			w1_write_8(dev_master, W1_COPY_SCRATCHPAD);

			if (strong_pullup) {
				sleep_rem = msleep_interruptible(t_write);
				if (sleep_rem != 0) {
					ret = -EINTR;
					goto mt_unlock;
				}
			}
			ret = 0;
		}

	}

mt_unlock:
	mutex_unlock(&dev_master->bus_mutex);
dec_refcnt:
	atomic_dec(THERM_REFCNT(sl->family_data));
error:
	return ret;
}

static int recall_eeprom(struct w1_slave *sl)
{
	struct w1_master *dev_master = sl->master;
	int max_trying = W1_THERM_MAX_TRY;
	int ret = -ENODEV;

	if (!sl->family_data)
		goto error;

	 
	atomic_inc(THERM_REFCNT(sl->family_data));

	if (!bus_mutex_lock(&dev_master->bus_mutex)) {
		ret = -EAGAIN;	 
		goto dec_refcnt;
	}

	while (max_trying-- && ret) {  
		 
		if (!reset_select_slave(sl)) {

			w1_write_8(dev_master, W1_RECALL_EEPROM);
			ret = w1_poll_completion(dev_master, W1_POLL_RECALL_EEPROM);
		}

	}

	mutex_unlock(&dev_master->bus_mutex);

dec_refcnt:
	atomic_dec(THERM_REFCNT(sl->family_data));
error:
	return ret;
}

static int read_powermode(struct w1_slave *sl)
{
	struct w1_master *dev_master = sl->master;
	int max_trying = W1_THERM_MAX_TRY;
	int  ret = -ENODEV;

	if (!sl->family_data)
		goto error;

	 
	atomic_inc(THERM_REFCNT(sl->family_data));

	if (!bus_mutex_lock(&dev_master->bus_mutex)) {
		ret = -EAGAIN;	 
		goto dec_refcnt;
	}

	while ((max_trying--) && (ret < 0)) {
		 
		if (!reset_select_slave(sl)) {
			w1_write_8(dev_master, W1_READ_PSUPPLY);
			 
			ret = w1_touch_bit(dev_master, 1);
			 
		}
	}
	mutex_unlock(&dev_master->bus_mutex);

dec_refcnt:
	atomic_dec(THERM_REFCNT(sl->family_data));
error:
	return ret;
}

static int trigger_bulk_read(struct w1_master *dev_master)
{
	struct w1_slave *sl = NULL;  
	int max_trying = W1_THERM_MAX_TRY;
	int t_conv = 0;
	int ret = -ENODEV;
	bool strong_pullup = false;

	 
	list_for_each_entry(sl, &dev_master->slist, w1_slave_entry) {
		if (!sl->family_data)
			goto error;
		if (bulk_read_support(sl)) {
			int t_cur = conversion_time(sl);

			t_conv = max(t_cur, t_conv);
			strong_pullup = strong_pullup ||
					(w1_strong_pullup == 2 ||
					(!SLAVE_POWERMODE(sl) &&
					w1_strong_pullup));
		}
	}

	 
	if (!t_conv)
		goto error;

	if (!bus_mutex_lock(&dev_master->bus_mutex)) {
		ret = -EAGAIN;	 
		goto error;
	}

	while ((max_trying--) && (ret < 0)) {  

		if (!w1_reset_bus(dev_master)) {	 
			unsigned long sleep_rem;

			w1_write_8(dev_master, W1_SKIP_ROM);

			if (strong_pullup)	 
				w1_next_pullup(dev_master, t_conv);

			w1_write_8(dev_master, W1_CONVERT_TEMP);

			 
			list_for_each_entry(sl,
				&dev_master->slist, w1_slave_entry) {
				if (bulk_read_support(sl))
					SLAVE_CONVERT_TRIGGERED(sl) = -1;
			}

			if (strong_pullup) {  
				sleep_rem = msleep_interruptible(t_conv);
				if (sleep_rem != 0) {
					ret = -EINTR;
					goto mt_unlock;
				}
				mutex_unlock(&dev_master->bus_mutex);
			} else {
				mutex_unlock(&dev_master->bus_mutex);
				sleep_rem = msleep_interruptible(t_conv);
				if (sleep_rem != 0) {
					ret = -EINTR;
					goto set_flag;
				}
			}
			ret = 0;
			goto set_flag;
		}
	}

mt_unlock:
	mutex_unlock(&dev_master->bus_mutex);
set_flag:
	 
	list_for_each_entry(sl, &dev_master->slist, w1_slave_entry) {
		if (bulk_read_support(sl))
			SLAVE_CONVERT_TRIGGERED(sl) = 1;
	}
error:
	return ret;
}

 

static ssize_t w1_slave_show(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	struct therm_info info;
	u8 *family_data = sl->family_data;
	int ret, i;
	ssize_t c = PAGE_SIZE;

	if (bulk_read_support(sl)) {
		if (SLAVE_CONVERT_TRIGGERED(sl) < 0) {
			dev_dbg(device,
				"%s: Conversion in progress, retry later\n",
				__func__);
			return 0;
		} else if (SLAVE_CONVERT_TRIGGERED(sl) > 0) {
			 
			ret = read_scratchpad(sl, &info);
			SLAVE_CONVERT_TRIGGERED(sl) = 0;
		} else
			ret = convert_t(sl, &info);
	} else
		ret = convert_t(sl, &info);

	if (ret < 0) {
		dev_dbg(device,
			"%s: Temperature data may be corrupted. err=%d\n",
			__func__, ret);
		return 0;
	}

	for (i = 0; i < 9; ++i)
		c -= snprintf(buf + PAGE_SIZE - c, c, "%02x ", info.rom[i]);
	c -= snprintf(buf + PAGE_SIZE - c, c, ": crc=%02x %s\n",
		      info.crc, (info.verdict) ? "YES" : "NO");

	if (info.verdict)
		memcpy(family_data, info.rom, sizeof(info.rom));
	else
		dev_warn(device, "%s:Read failed CRC check\n", __func__);

	for (i = 0; i < 9; ++i)
		c -= snprintf(buf + PAGE_SIZE - c, c, "%02x ",
			      ((u8 *)family_data)[i]);

	c -= snprintf(buf + PAGE_SIZE - c, c, "t=%d\n",
			temperature_from_RAM(sl, info.rom));

	ret = PAGE_SIZE - c;
	return ret;
}

static ssize_t w1_slave_store(struct device *device,
			      struct device_attribute *attr, const char *buf,
			      size_t size)
{
	int val, ret = 0;
	struct w1_slave *sl = dev_to_w1_slave(device);

	ret = kstrtoint(buf, 10, &val);  

	if (ret) {	 
		dev_info(device,
			"%s: conversion error. err= %d\n", __func__, ret);
		return size;	 
	}

	if ((!sl->family_data) || (!SLAVE_SPECIFIC_FUNC(sl))) {
		dev_info(device,
			"%s: Device not supported by the driver\n", __func__);
		return size;   
	}

	if (val == 0)	 
		ret = copy_scratchpad(sl);
	else {
		if (SLAVE_SPECIFIC_FUNC(sl)->set_resolution)
			ret = SLAVE_SPECIFIC_FUNC(sl)->set_resolution(sl, val);
	}

	if (ret) {
		dev_warn(device, "%s: Set resolution - error %d\n", __func__, ret);
		 
		return ret;
	}
	SLAVE_RESOLUTION(sl) = val;
	 
	SLAVE_CONV_TIME_OVERRIDE(sl) = CONV_TIME_DEFAULT;

	return size;  
}

static ssize_t temperature_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	struct therm_info info;
	int ret = 0;

	if ((!sl->family_data) || (!SLAVE_SPECIFIC_FUNC(sl))) {
		dev_info(device,
			"%s: Device not supported by the driver\n", __func__);
		return 0;   
	}

	if (bulk_read_support(sl)) {
		if (SLAVE_CONVERT_TRIGGERED(sl) < 0) {
			dev_dbg(device,
				"%s: Conversion in progress, retry later\n",
				__func__);
			return 0;
		} else if (SLAVE_CONVERT_TRIGGERED(sl) > 0) {
			 
			ret = read_scratchpad(sl, &info);
			SLAVE_CONVERT_TRIGGERED(sl) = 0;
		} else
			ret = convert_t(sl, &info);
	} else
		ret = convert_t(sl, &info);

	if (ret < 0) {
		dev_dbg(device,
			"%s: Temperature data may be corrupted. err=%d\n",
			__func__, ret);
		return 0;
	}

	return sprintf(buf, "%d\n", temperature_from_RAM(sl, info.rom));
}

static ssize_t ext_power_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);

	if (!sl->family_data) {
		dev_info(device,
			"%s: Device not supported by the driver\n", __func__);
		return 0;   
	}

	 
	SLAVE_POWERMODE(sl) = read_powermode(sl);

	if (SLAVE_POWERMODE(sl) < 0) {
		dev_dbg(device,
			"%s: Power_mode may be corrupted. err=%d\n",
			__func__, SLAVE_POWERMODE(sl));
	}
	return sprintf(buf, "%d\n", SLAVE_POWERMODE(sl));
}

static ssize_t resolution_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);

	if ((!sl->family_data) || (!SLAVE_SPECIFIC_FUNC(sl))) {
		dev_info(device,
			"%s: Device not supported by the driver\n", __func__);
		return 0;   
	}

	 
	SLAVE_RESOLUTION(sl) = SLAVE_SPECIFIC_FUNC(sl)->get_resolution(sl);
	if (SLAVE_RESOLUTION(sl) < 0) {
		dev_dbg(device,
			"%s: Resolution may be corrupted. err=%d\n",
			__func__, SLAVE_RESOLUTION(sl));
	}

	return sprintf(buf, "%d\n", SLAVE_RESOLUTION(sl));
}

static ssize_t resolution_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	int val;
	int ret = 0;

	ret = kstrtoint(buf, 10, &val);  

	if (ret) {	 
		dev_info(device,
			"%s: conversion error. err= %d\n", __func__, ret);
		return size;	 
	}

	if ((!sl->family_data) || (!SLAVE_SPECIFIC_FUNC(sl))) {
		dev_info(device,
			"%s: Device not supported by the driver\n", __func__);
		return size;   
	}

	 

	 
	ret = SLAVE_SPECIFIC_FUNC(sl)->set_resolution(sl, val);

	if (ret)
		return ret;

	SLAVE_RESOLUTION(sl) = val;
	 
	SLAVE_CONV_TIME_OVERRIDE(sl) = CONV_TIME_DEFAULT;

	return size;
}

static ssize_t eeprom_cmd_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	int ret = -EINVAL;  

	if (size == sizeof(EEPROM_CMD_WRITE)) {
		if (!strncmp(buf, EEPROM_CMD_WRITE, sizeof(EEPROM_CMD_WRITE)-1))
			ret = copy_scratchpad(sl);
	} else if (size == sizeof(EEPROM_CMD_READ)) {
		if (!strncmp(buf, EEPROM_CMD_READ, sizeof(EEPROM_CMD_READ)-1))
			ret = recall_eeprom(sl);
	}

	if (ret)
		dev_info(device, "%s: error in process %d\n", __func__, ret);

	return size;
}

static ssize_t alarms_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	int ret;
	s8 th = 0, tl = 0;
	struct therm_info scratchpad;

	ret = read_scratchpad(sl, &scratchpad);

	if (!ret)	{
		th = scratchpad.rom[2];  
		tl = scratchpad.rom[3];  
	} else {
		dev_info(device,
			"%s: error reading alarms register %d\n",
			__func__, ret);
	}

	return sprintf(buf, "%hd %hd\n", tl, th);
}

static ssize_t alarms_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	struct therm_info info;
	u8 new_config_register[3];	 
	int temp, ret;
	char *token = NULL;
	s8 tl, th;	 
	char *p_args, *orig;

	p_args = orig = kmalloc(size, GFP_KERNEL);
	 
	if (!p_args) {
		dev_warn(device,
			"%s: error unable to allocate memory %d\n",
			__func__, -ENOMEM);
		return size;
	}
	strcpy(p_args, buf);

	 
	token = strsep(&p_args, " ");

	if (!token)	{
		dev_info(device,
			"%s: error parsing args %d\n", __func__, -EINVAL);
		goto free_m;
	}

	 
	ret = kstrtoint (token, 10, &temp);
	if (ret) {
		dev_info(device,
			"%s: error parsing args %d\n", __func__, ret);
		goto free_m;
	}

	tl = int_to_short(temp);

	 
	token = strsep(&p_args, " ");
	if (!token)	{
		dev_info(device,
			"%s: error parsing args %d\n", __func__, -EINVAL);
		goto free_m;
	}
	 
	ret = kstrtoint (token, 10, &temp);
	if (ret) {
		dev_info(device,
			"%s: error parsing args %d\n", __func__, ret);
		goto free_m;
	}

	 
	th = int_to_short(temp);

	 
	if (tl > th)
		swap(tl, th);

	 
	ret = read_scratchpad(sl, &info);
	if (!ret) {
		new_config_register[0] = th;	 
		new_config_register[1] = tl;	 
		new_config_register[2] = info.rom[4]; 
	} else {
		dev_info(device,
			"%s: error reading from the slave device %d\n",
			__func__, ret);
		goto free_m;
	}

	 
	if (!SLAVE_SPECIFIC_FUNC(sl)) {
		dev_info(device,
			"%s: Device not supported by the driver %d\n",
			__func__, -ENODEV);
		goto free_m;
	}

	ret = SLAVE_SPECIFIC_FUNC(sl)->write_data(sl, new_config_register);
	if (ret)
		dev_info(device,
			"%s: error writing to the slave device %d\n",
			__func__, ret);

free_m:
	 
	kfree(orig);

	return size;
}

static ssize_t therm_bulk_read_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct w1_master *dev_master = dev_to_w1_master(device);
	int ret = -EINVAL;  

	if (size == sizeof(BULK_TRIGGER_CMD))
		if (!strncmp(buf, BULK_TRIGGER_CMD,
				sizeof(BULK_TRIGGER_CMD)-1))
			ret = trigger_bulk_read(dev_master);

	if (ret)
		dev_info(device,
			"%s: unable to trigger a bulk read on the bus. err=%d\n",
			__func__, ret);

	return size;
}

static ssize_t therm_bulk_read_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct w1_master *dev_master = dev_to_w1_master(device);
	struct w1_slave *sl = NULL;
	int ret = 0;

	list_for_each_entry(sl, &dev_master->slist, w1_slave_entry) {
		if (sl->family_data) {
			if (bulk_read_support(sl)) {
				if (SLAVE_CONVERT_TRIGGERED(sl) == -1) {
					ret = -1;
					goto show_result;
				}
				if (SLAVE_CONVERT_TRIGGERED(sl) == 1)
					 
					ret = 1;
			}
		}
	}
show_result:
	return sprintf(buf, "%d\n", ret);
}

static ssize_t conv_time_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);

	if ((!sl->family_data) || (!SLAVE_SPECIFIC_FUNC(sl))) {
		dev_info(device,
			"%s: Device is not supported by the driver\n", __func__);
		return 0;   
	}
	return sprintf(buf, "%d\n", conversion_time(sl));
}

static ssize_t conv_time_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int val, ret = 0;
	struct w1_slave *sl = dev_to_w1_slave(device);

	if (kstrtoint(buf, 10, &val))  
		return -EINVAL;

	if (check_family_data(sl))
		return -ENODEV;

	if (val != CONV_TIME_MEASURE) {
		if (val >= CONV_TIME_DEFAULT)
			SLAVE_CONV_TIME_OVERRIDE(sl) = val;
		else
			return -EINVAL;

	} else {
		int conv_time;

		ret = conv_time_measure(sl, &conv_time);
		if (ret)
			return -EIO;
		SLAVE_CONV_TIME_OVERRIDE(sl) = conv_time;
	}
	return size;
}

static ssize_t features_show(struct device *device,
			     struct device_attribute *attr, char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);

	if ((!sl->family_data) || (!SLAVE_SPECIFIC_FUNC(sl))) {
		dev_info(device,
			 "%s: Device not supported by the driver\n", __func__);
		return 0;   
	}
	return sprintf(buf, "%u\n", SLAVE_FEATURES(sl));
}

static ssize_t features_store(struct device *device,
			      struct device_attribute *attr, const char *buf, size_t size)
{
	int val, ret = 0;
	bool strong_pullup;
	struct w1_slave *sl = dev_to_w1_slave(device);

	ret = kstrtouint(buf, 10, &val);  
	if (ret)
		return -EINVAL;   

	if ((!sl->family_data) || (!SLAVE_SPECIFIC_FUNC(sl))) {
		dev_info(device, "%s: Device not supported by the driver\n", __func__);
		return -ENODEV;
	}

	if ((val & W1_THERM_FEATURES_MASK) != val)
		return -EINVAL;

	SLAVE_FEATURES(sl) = val;

	strong_pullup = (w1_strong_pullup == 2 ||
			 (!SLAVE_POWERMODE(sl) &&
			  w1_strong_pullup));

	if (strong_pullup && SLAVE_FEATURES(sl) & W1_THERM_POLL_COMPLETION) {
		dev_warn(&sl->dev,
			 "%s: W1_THERM_POLL_COMPLETION disabled in parasite power mode.\n",
			 __func__);
		SLAVE_FEATURES(sl) &= ~W1_THERM_POLL_COMPLETION;
	}

	return size;
}

#if IS_REACHABLE(CONFIG_HWMON)
static int w1_read_temp(struct device *device, u32 attr, int channel,
			long *val)
{
	struct w1_slave *sl = dev_get_drvdata(device);
	struct therm_info info;
	int ret;

	switch (attr) {
	case hwmon_temp_input:
		ret = convert_t(sl, &info);
		if (ret)
			return ret;

		if (!info.verdict) {
			ret = -EIO;
			return ret;
		}

		*val = temperature_from_RAM(sl, info.rom);
		ret = 0;
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	return ret;
}
#endif

#define W1_42_CHAIN	0x99
#define W1_42_CHAIN_OFF	0x3C
#define W1_42_CHAIN_OFF_INV	0xC3
#define W1_42_CHAIN_ON	0x5A
#define W1_42_CHAIN_ON_INV	0xA5
#define W1_42_CHAIN_DONE 0x96
#define W1_42_CHAIN_DONE_INV 0x69
#define W1_42_COND_READ	0x0F
#define W1_42_SUCCESS_CONFIRM_BYTE 0xAA
#define W1_42_FINISHED_BYTE 0xFF
static ssize_t w1_seq_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(device);
	ssize_t c = PAGE_SIZE;
	int i;
	u8 ack;
	u64 rn;
	struct w1_reg_num *reg_num;
	int seq = 0;

	mutex_lock(&sl->master->bus_mutex);
	 
	if (w1_reset_bus(sl->master))
		goto error;
	w1_write_8(sl->master, W1_SKIP_ROM);
	w1_write_8(sl->master, W1_42_CHAIN);
	w1_write_8(sl->master, W1_42_CHAIN_ON);
	w1_write_8(sl->master, W1_42_CHAIN_ON_INV);
	msleep(sl->master->pullup_duration);

	 
	ack = w1_read_8(sl->master);
	if (ack != W1_42_SUCCESS_CONFIRM_BYTE)
		goto error;

	 
	for (i = 0; i <= 64; i++) {
		if (w1_reset_bus(sl->master))
			goto error;

		w1_write_8(sl->master, W1_42_COND_READ);
		w1_read_block(sl->master, (u8 *)&rn, 8);
		reg_num = (struct w1_reg_num *) &rn;
		if (reg_num->family == W1_42_FINISHED_BYTE)
			break;
		if (sl->reg_num.id == reg_num->id)
			seq = i;

		if (w1_reset_bus(sl->master))
			goto error;

		 
		w1_write_8(sl->master, W1_MATCH_ROM);
		w1_write_block(sl->master, (u8 *)&rn, 8);
		w1_write_8(sl->master, W1_42_CHAIN);
		w1_write_8(sl->master, W1_42_CHAIN_DONE);
		w1_write_8(sl->master, W1_42_CHAIN_DONE_INV);

		 
		ack = w1_read_8(sl->master);
		if (ack != W1_42_SUCCESS_CONFIRM_BYTE)
			goto error;
	}

	 
	if (w1_reset_bus(sl->master))
		goto error;
	w1_write_8(sl->master, W1_SKIP_ROM);
	w1_write_8(sl->master, W1_42_CHAIN);
	w1_write_8(sl->master, W1_42_CHAIN_OFF);
	w1_write_8(sl->master, W1_42_CHAIN_OFF_INV);

	 
	ack = w1_read_8(sl->master);
	if (ack != W1_42_SUCCESS_CONFIRM_BYTE)
		goto error;
	mutex_unlock(&sl->master->bus_mutex);

	c -= snprintf(buf + PAGE_SIZE - c, c, "%d\n", seq);
	return PAGE_SIZE - c;
error:
	mutex_unlock(&sl->master->bus_mutex);
	return -EIO;
}

static int __init w1_therm_init(void)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(w1_therm_families); ++i) {
		err = w1_register_family(w1_therm_families[i].f);
		if (err)
			w1_therm_families[i].broken = 1;
	}

	return 0;
}

static void __exit w1_therm_fini(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(w1_therm_families); ++i)
		if (!w1_therm_families[i].broken)
			w1_unregister_family(w1_therm_families[i].f);
}

module_init(w1_therm_init);
module_exit(w1_therm_fini);

MODULE_AUTHOR("Evgeniy Polyakov <zbr@ioremap.net>");
MODULE_DESCRIPTION("Driver for 1-wire Dallas network protocol, temperature family.");
MODULE_LICENSE("GPL");
MODULE_ALIAS("w1-family-" __stringify(W1_THERM_DS18S20));
MODULE_ALIAS("w1-family-" __stringify(W1_THERM_DS1822));
MODULE_ALIAS("w1-family-" __stringify(W1_THERM_DS18B20));
MODULE_ALIAS("w1-family-" __stringify(W1_THERM_DS1825));
MODULE_ALIAS("w1-family-" __stringify(W1_THERM_DS28EA00));
