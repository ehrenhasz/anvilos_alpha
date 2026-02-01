
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/jiffies.h>

 
enum ltc4215_cmd {
	LTC4215_CONTROL			= 0x00,  
	LTC4215_ALERT			= 0x01,  
	LTC4215_STATUS			= 0x02,  
	LTC4215_FAULT			= 0x03,  
	LTC4215_SENSE			= 0x04,  
	LTC4215_SOURCE			= 0x05,  
	LTC4215_ADIN			= 0x06,  
};

struct ltc4215_data {
	struct i2c_client *client;

	struct mutex update_lock;
	bool valid;
	unsigned long last_updated;  

	 
	u8 regs[7];
};

static struct ltc4215_data *ltc4215_update_device(struct device *dev)
{
	struct ltc4215_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	s32 val;
	int i;

	mutex_lock(&data->update_lock);

	 
	if (time_after(jiffies, data->last_updated + HZ / 10) || !data->valid) {

		dev_dbg(&client->dev, "Starting ltc4215 update\n");

		 
		for (i = 0; i < ARRAY_SIZE(data->regs); i++) {
			val = i2c_smbus_read_byte_data(client, i);
			if (unlikely(val < 0))
				data->regs[i] = 0;
			else
				data->regs[i] = val;
		}

		data->last_updated = jiffies;
		data->valid = true;
	}

	mutex_unlock(&data->update_lock);

	return data;
}

 
static int ltc4215_get_voltage(struct device *dev, u8 reg)
{
	struct ltc4215_data *data = ltc4215_update_device(dev);
	const u8 regval = data->regs[reg];
	u32 voltage = 0;

	switch (reg) {
	case LTC4215_SENSE:
		 
		voltage = regval * 151 / 1000;
		break;
	case LTC4215_SOURCE:
		 
		voltage = regval * 605 / 10;
		break;
	case LTC4215_ADIN:
		 
		voltage = regval * 482 * 125 / 1000;
		break;
	default:
		 
		WARN_ON_ONCE(1);
		break;
	}

	return voltage;
}

 
static unsigned int ltc4215_get_current(struct device *dev)
{
	struct ltc4215_data *data = ltc4215_update_device(dev);

	 

	 
	const unsigned int voltage = data->regs[LTC4215_SENSE] * 151;

	 
	const unsigned int curr = voltage / 4;

	return curr;
}

static ssize_t ltc4215_voltage_show(struct device *dev,
				    struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	const int voltage = ltc4215_get_voltage(dev, attr->index);

	return sysfs_emit(buf, "%d\n", voltage);
}

static ssize_t ltc4215_current_show(struct device *dev,
				    struct device_attribute *da, char *buf)
{
	const unsigned int curr = ltc4215_get_current(dev);

	return sysfs_emit(buf, "%u\n", curr);
}

static ssize_t ltc4215_power_show(struct device *dev,
				  struct device_attribute *da, char *buf)
{
	const unsigned int curr = ltc4215_get_current(dev);
	const int output_voltage = ltc4215_get_voltage(dev, LTC4215_ADIN);

	 
	const unsigned int power = abs(output_voltage * curr);

	return sysfs_emit(buf, "%u\n", power);
}

static ssize_t ltc4215_alarm_show(struct device *dev,
				  struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct ltc4215_data *data = ltc4215_update_device(dev);
	const u8 reg = data->regs[LTC4215_STATUS];
	const u32 mask = attr->index;

	return sysfs_emit(buf, "%u\n", !!(reg & mask));
}

 

 

 
static SENSOR_DEVICE_ATTR_RO(curr1_input, ltc4215_current, 0);
static SENSOR_DEVICE_ATTR_RO(curr1_max_alarm, ltc4215_alarm, 1 << 2);

 
static SENSOR_DEVICE_ATTR_RO(power1_input, ltc4215_power, 0);

 
static SENSOR_DEVICE_ATTR_RO(in1_input, ltc4215_voltage, LTC4215_ADIN);
static SENSOR_DEVICE_ATTR_RO(in1_max_alarm, ltc4215_alarm, 1 << 0);
static SENSOR_DEVICE_ATTR_RO(in1_min_alarm, ltc4215_alarm, 1 << 1);

 
static SENSOR_DEVICE_ATTR_RO(in2_input, ltc4215_voltage, LTC4215_SOURCE);
static SENSOR_DEVICE_ATTR_RO(in2_min_alarm, ltc4215_alarm, 1 << 3);

 
static struct attribute *ltc4215_attrs[] = {
	&sensor_dev_attr_curr1_input.dev_attr.attr,
	&sensor_dev_attr_curr1_max_alarm.dev_attr.attr,

	&sensor_dev_attr_power1_input.dev_attr.attr,

	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_max_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_min_alarm.dev_attr.attr,

	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_min_alarm.dev_attr.attr,

	NULL,
};
ATTRIBUTE_GROUPS(ltc4215);

static int ltc4215_probe(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	struct device *dev = &client->dev;
	struct ltc4215_data *data;
	struct device *hwmon_dev;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	mutex_init(&data->update_lock);

	 
	i2c_smbus_write_byte_data(client, LTC4215_FAULT, 0x00);

	hwmon_dev = devm_hwmon_device_register_with_groups(dev, client->name,
							   data,
							   ltc4215_groups);
	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static const struct i2c_device_id ltc4215_id[] = {
	{ "ltc4215", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ltc4215_id);

 
static struct i2c_driver ltc4215_driver = {
	.driver = {
		.name	= "ltc4215",
	},
	.probe		= ltc4215_probe,
	.id_table	= ltc4215_id,
};

module_i2c_driver(ltc4215_driver);

MODULE_AUTHOR("Ira W. Snyder <iws@ovro.caltech.edu>");
MODULE_DESCRIPTION("LTC4215 driver");
MODULE_LICENSE("GPL");
