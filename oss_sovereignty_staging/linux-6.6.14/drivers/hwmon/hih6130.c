
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

 
struct hih6130 {
	struct i2c_client *client;
	struct mutex lock;
	bool valid;
	unsigned long last_update;
	int temperature;
	int humidity;
	size_t write_length;
};

 
static inline int hih6130_temp_ticks_to_millicelsius(int ticks)
{
	ticks = ticks >> 2;
	 
	return (DIV_ROUND_CLOSEST(ticks * 1650, 16382) - 400) * 100;
}

 
static inline int hih6130_rh_ticks_to_per_cent_mille(int ticks)
{
	ticks &= ~0xC000;  
	 
	return DIV_ROUND_CLOSEST(ticks * 1000, 16382) * 100;
}

 
static int hih6130_update_measurements(struct device *dev)
{
	struct hih6130 *hih6130 = dev_get_drvdata(dev);
	struct i2c_client *client = hih6130->client;
	int ret = 0;
	int t;
	unsigned char tmp[4];
	struct i2c_msg msgs[1] = {
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 4,
			.buf = tmp,
		}
	};

	mutex_lock(&hih6130->lock);

	 
	if (time_after(jiffies, hih6130->last_update + HZ) || !hih6130->valid) {

		 
		tmp[0] = 0;
		ret = i2c_master_send(client, tmp, hih6130->write_length);
		if (ret < 0)
			goto out;

		 
		msleep(40);

		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			goto out;

		if ((tmp[0] & 0xC0) != 0) {
			dev_err(&client->dev, "Error while reading measurement result\n");
			ret = -EIO;
			goto out;
		}

		t = (tmp[0] << 8) + tmp[1];
		hih6130->humidity = hih6130_rh_ticks_to_per_cent_mille(t);

		t = (tmp[2] << 8) + tmp[3];
		hih6130->temperature = hih6130_temp_ticks_to_millicelsius(t);

		hih6130->last_update = jiffies;
		hih6130->valid = true;
	}
out:
	mutex_unlock(&hih6130->lock);

	return ret >= 0 ? 0 : ret;
}

 
static ssize_t hih6130_temperature_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct hih6130 *hih6130 = dev_get_drvdata(dev);
	int ret;

	ret = hih6130_update_measurements(dev);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", hih6130->temperature);
}

 
static ssize_t hih6130_humidity_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct hih6130 *hih6130 = dev_get_drvdata(dev);
	int ret;

	ret = hih6130_update_measurements(dev);
	if (ret < 0)
		return ret;
	return sprintf(buf, "%d\n", hih6130->humidity);
}

 
static SENSOR_DEVICE_ATTR_RO(temp1_input, hih6130_temperature, 0);
static SENSOR_DEVICE_ATTR_RO(humidity1_input, hih6130_humidity, 0);

static struct attribute *hih6130_attrs[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_humidity1_input.dev_attr.attr,
	NULL
};

ATTRIBUTE_GROUPS(hih6130);

static int hih6130_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct hih6130 *hih6130;
	struct device *hwmon_dev;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "adapter does not support true I2C\n");
		return -ENODEV;
	}

	hih6130 = devm_kzalloc(dev, sizeof(*hih6130), GFP_KERNEL);
	if (!hih6130)
		return -ENOMEM;

	hih6130->client = client;
	mutex_init(&hih6130->lock);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_QUICK))
		hih6130->write_length = 1;

	hwmon_dev = devm_hwmon_device_register_with_groups(dev, client->name,
							   hih6130,
							   hih6130_groups);
	return PTR_ERR_OR_ZERO(hwmon_dev);
}

 
static const struct i2c_device_id hih6130_id[] = {
	{ "hih6130", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, hih6130_id);

static const struct of_device_id __maybe_unused hih6130_of_match[] = {
	{ .compatible = "honeywell,hih6130", },
	{ }
};
MODULE_DEVICE_TABLE(of, hih6130_of_match);

static struct i2c_driver hih6130_driver = {
	.driver = {
		.name = "hih6130",
		.of_match_table = of_match_ptr(hih6130_of_match),
	},
	.probe       = hih6130_probe,
	.id_table    = hih6130_id,
};

module_i2c_driver(hih6130_driver);

MODULE_AUTHOR("Iain Paton <ipaton0@gmail.com>");
MODULE_DESCRIPTION("Honeywell HIH-6130 humidity and temperature sensor driver");
MODULE_LICENSE("GPL");
