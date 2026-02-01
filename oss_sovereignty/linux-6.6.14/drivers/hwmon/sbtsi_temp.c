
 

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>

 
#define SBTSI_REG_TEMP_INT		0x01  
#define SBTSI_REG_STATUS		0x02  
#define SBTSI_REG_CONFIG		0x03  
#define SBTSI_REG_TEMP_HIGH_INT		0x07  
#define SBTSI_REG_TEMP_LOW_INT		0x08  
#define SBTSI_REG_TEMP_DEC		0x10  
#define SBTSI_REG_TEMP_HIGH_DEC		0x13  
#define SBTSI_REG_TEMP_LOW_DEC		0x14  

#define SBTSI_CONFIG_READ_ORDER_SHIFT	5

#define SBTSI_TEMP_MIN	0
#define SBTSI_TEMP_MAX	255875

 
struct sbtsi_data {
	struct i2c_client *client;
	struct mutex lock;
};

 
static inline int sbtsi_reg_to_mc(s32 integer, s32 decimal)
{
	return ((integer << 3) + (decimal >> 5)) * 125;
}

 
static inline void sbtsi_mc_to_reg(s32 temp, u8 *integer, u8 *decimal)
{
	temp /= 125;
	*integer = temp >> 3;
	*decimal = (temp & 0x7) << 5;
}

static int sbtsi_read(struct device *dev, enum hwmon_sensor_types type,
		      u32 attr, int channel, long *val)
{
	struct sbtsi_data *data = dev_get_drvdata(dev);
	s32 temp_int, temp_dec;
	int err;

	switch (attr) {
	case hwmon_temp_input:
		 
		err = i2c_smbus_read_byte_data(data->client, SBTSI_REG_CONFIG);
		if (err < 0)
			return err;

		mutex_lock(&data->lock);
		if (err & BIT(SBTSI_CONFIG_READ_ORDER_SHIFT)) {
			temp_dec = i2c_smbus_read_byte_data(data->client, SBTSI_REG_TEMP_DEC);
			temp_int = i2c_smbus_read_byte_data(data->client, SBTSI_REG_TEMP_INT);
		} else {
			temp_int = i2c_smbus_read_byte_data(data->client, SBTSI_REG_TEMP_INT);
			temp_dec = i2c_smbus_read_byte_data(data->client, SBTSI_REG_TEMP_DEC);
		}
		mutex_unlock(&data->lock);
		break;
	case hwmon_temp_max:
		mutex_lock(&data->lock);
		temp_int = i2c_smbus_read_byte_data(data->client, SBTSI_REG_TEMP_HIGH_INT);
		temp_dec = i2c_smbus_read_byte_data(data->client, SBTSI_REG_TEMP_HIGH_DEC);
		mutex_unlock(&data->lock);
		break;
	case hwmon_temp_min:
		mutex_lock(&data->lock);
		temp_int = i2c_smbus_read_byte_data(data->client, SBTSI_REG_TEMP_LOW_INT);
		temp_dec = i2c_smbus_read_byte_data(data->client, SBTSI_REG_TEMP_LOW_DEC);
		mutex_unlock(&data->lock);
		break;
	default:
		return -EINVAL;
	}


	if (temp_int < 0)
		return temp_int;
	if (temp_dec < 0)
		return temp_dec;

	*val = sbtsi_reg_to_mc(temp_int, temp_dec);

	return 0;
}

static int sbtsi_write(struct device *dev, enum hwmon_sensor_types type,
		       u32 attr, int channel, long val)
{
	struct sbtsi_data *data = dev_get_drvdata(dev);
	int reg_int, reg_dec, err;
	u8 temp_int, temp_dec;

	switch (attr) {
	case hwmon_temp_max:
		reg_int = SBTSI_REG_TEMP_HIGH_INT;
		reg_dec = SBTSI_REG_TEMP_HIGH_DEC;
		break;
	case hwmon_temp_min:
		reg_int = SBTSI_REG_TEMP_LOW_INT;
		reg_dec = SBTSI_REG_TEMP_LOW_DEC;
		break;
	default:
		return -EINVAL;
	}

	val = clamp_val(val, SBTSI_TEMP_MIN, SBTSI_TEMP_MAX);
	sbtsi_mc_to_reg(val, &temp_int, &temp_dec);

	mutex_lock(&data->lock);
	err = i2c_smbus_write_byte_data(data->client, reg_int, temp_int);
	if (err)
		goto exit;

	err = i2c_smbus_write_byte_data(data->client, reg_dec, temp_dec);
exit:
	mutex_unlock(&data->lock);
	return err;
}

static umode_t sbtsi_is_visible(const void *data,
				enum hwmon_sensor_types type,
				u32 attr, int channel)
{
	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			return 0444;
		case hwmon_temp_min:
			return 0644;
		case hwmon_temp_max:
			return 0644;
		}
		break;
	default:
		break;
	}
	return 0;
}

static const struct hwmon_channel_info * const sbtsi_info[] = {
	HWMON_CHANNEL_INFO(chip, HWMON_C_REGISTER_TZ),
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT | HWMON_T_MIN | HWMON_T_MAX),
	NULL
};

static const struct hwmon_ops sbtsi_hwmon_ops = {
	.is_visible = sbtsi_is_visible,
	.read = sbtsi_read,
	.write = sbtsi_write,
};

static const struct hwmon_chip_info sbtsi_chip_info = {
	.ops = &sbtsi_hwmon_ops,
	.info = sbtsi_info,
};

static int sbtsi_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct device *hwmon_dev;
	struct sbtsi_data *data;

	data = devm_kzalloc(dev, sizeof(struct sbtsi_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	mutex_init(&data->lock);

	hwmon_dev = devm_hwmon_device_register_with_info(dev, client->name, data, &sbtsi_chip_info,
							 NULL);

	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static const struct i2c_device_id sbtsi_id[] = {
	{"sbtsi", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sbtsi_id);

static const struct of_device_id __maybe_unused sbtsi_of_match[] = {
	{
		.compatible = "amd,sbtsi",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, sbtsi_of_match);

static struct i2c_driver sbtsi_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name = "sbtsi",
		.of_match_table = of_match_ptr(sbtsi_of_match),
	},
	.probe = sbtsi_probe,
	.id_table = sbtsi_id,
};

module_i2c_driver(sbtsi_driver);

MODULE_AUTHOR("Kun Yi <kunyi@google.com>");
MODULE_DESCRIPTION("Hwmon driver for AMD SB-TSI emulated sensor");
MODULE_LICENSE("GPL");
