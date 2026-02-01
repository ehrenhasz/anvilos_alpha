

 

#include <linux/delay.h>
#include <linux/hwmon.h>
#include <linux/i2c.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/crc8.h>

#define AHT10_MEAS_SIZE		6

#define AHT20_MEAS_SIZE		7
#define AHT20_CRC8_POLY		0x31

 
#define AHT10_DEFAULT_MIN_POLL_INTERVAL	2000
#define AHT10_MIN_POLL_INTERVAL		2000

 
#define AHT10_MEAS_DELAY	80000
#define AHT10_CMD_DELAY		350000
#define AHT10_DELAY_EXTRA	100000

 
#define AHT10_CMD_INIT	0b11100001
#define AHT10_CMD_MEAS	0b10101100
#define AHT10_CMD_RST	0b10111010

 
#define AHT10_CAL_ENABLED	BIT(3)
#define AHT10_BUSY		BIT(7)
#define AHT10_MODE_NOR		(BIT(5) | BIT(6))
#define AHT10_MODE_CYC		BIT(5)
#define AHT10_MODE_CMD		BIT(6)

#define AHT10_MAX_POLL_INTERVAL_LEN	30

enum aht10_variant { aht10, aht20 };

static const struct i2c_device_id aht10_id[] = {
	{ "aht10", aht10 },
	{ "aht20", aht20 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, aht10_id);

 

struct aht10_data {
	struct i2c_client *client;
	 
	struct mutex lock;
	ktime_t min_poll_interval;
	ktime_t previous_poll_time;
	int temperature;
	int humidity;
	bool crc8;
	unsigned int meas_size;
};

 
static int aht10_init(struct aht10_data *data)
{
	const u8 cmd_init[] = {AHT10_CMD_INIT, AHT10_CAL_ENABLED | AHT10_MODE_CYC,
			       0x00};
	int res;
	u8 status;
	struct i2c_client *client = data->client;

	res = i2c_master_send(client, cmd_init, 3);
	if (res < 0)
		return res;

	usleep_range(AHT10_CMD_DELAY, AHT10_CMD_DELAY +
		     AHT10_DELAY_EXTRA);

	res = i2c_master_recv(client, &status, 1);
	if (res != 1)
		return -ENODATA;

	if (status & AHT10_BUSY)
		return -EBUSY;

	return 0;
}

 
static int aht10_polltime_expired(struct aht10_data *data)
{
	ktime_t current_time = ktime_get_boottime();
	ktime_t difference = ktime_sub(current_time, data->previous_poll_time);

	return ktime_after(difference, data->min_poll_interval);
}

DECLARE_CRC8_TABLE(crc8_table);

 
static int crc8_check(u8 *raw_data, int count)
{
	 
	return crc8(crc8_table, raw_data, count, CRC8_INIT_VALUE);
}

 
static int aht10_read_values(struct aht10_data *data)
{
	const u8 cmd_meas[] = {AHT10_CMD_MEAS, 0x33, 0x00};
	u32 temp, hum;
	int res;
	u8 raw_data[AHT20_MEAS_SIZE];
	struct i2c_client *client = data->client;

	mutex_lock(&data->lock);
	if (!aht10_polltime_expired(data)) {
		mutex_unlock(&data->lock);
		return 0;
	}

	res = i2c_master_send(client, cmd_meas, sizeof(cmd_meas));
	if (res < 0) {
		mutex_unlock(&data->lock);
		return res;
	}

	usleep_range(AHT10_MEAS_DELAY, AHT10_MEAS_DELAY + AHT10_DELAY_EXTRA);

	res = i2c_master_recv(client, raw_data, data->meas_size);
	if (res != data->meas_size) {
		mutex_unlock(&data->lock);
		if (res >= 0)
			return -ENODATA;
		return res;
	}

	if (data->crc8 && crc8_check(raw_data, data->meas_size)) {
		mutex_unlock(&data->lock);
		return -EIO;
	}

	hum =   ((u32)raw_data[1] << 12u) |
		((u32)raw_data[2] << 4u) |
		((raw_data[3] & 0xF0u) >> 4u);

	temp =  ((u32)(raw_data[3] & 0x0Fu) << 16u) |
		((u32)raw_data[4] << 8u) |
		raw_data[5];

	temp = ((temp * 625) >> 15u) * 10;
	hum = ((hum * 625) >> 16u) * 10;

	data->temperature = (int)temp - 50000;
	data->humidity = hum;
	data->previous_poll_time = ktime_get_boottime();

	mutex_unlock(&data->lock);
	return 0;
}

 
static ssize_t aht10_interval_write(struct aht10_data *data,
				    long val)
{
	data->min_poll_interval = ms_to_ktime(clamp_val(val, 2000, LONG_MAX));
	return 0;
}

 
static ssize_t aht10_interval_read(struct aht10_data *data,
				   long *val)
{
	*val = ktime_to_ms(data->min_poll_interval);
	return 0;
}

 
static int aht10_temperature1_read(struct aht10_data *data, long *val)
{
	int res;

	res = aht10_read_values(data);
	if (res < 0)
		return res;

	*val = data->temperature;
	return 0;
}

 
static int aht10_humidity1_read(struct aht10_data *data, long *val)
{
	int res;

	res = aht10_read_values(data);
	if (res < 0)
		return res;

	*val = data->humidity;
	return 0;
}

static umode_t aht10_hwmon_visible(const void *data, enum hwmon_sensor_types type,
				   u32 attr, int channel)
{
	switch (type) {
	case hwmon_temp:
	case hwmon_humidity:
		return 0444;
	case hwmon_chip:
		return 0644;
	default:
		return 0;
	}
}

static int aht10_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
			    u32 attr, int channel, long *val)
{
	struct aht10_data *data = dev_get_drvdata(dev);

	switch (type) {
	case hwmon_temp:
		return aht10_temperature1_read(data, val);
	case hwmon_humidity:
		return aht10_humidity1_read(data, val);
	case hwmon_chip:
		return aht10_interval_read(data, val);
	default:
		return -EOPNOTSUPP;
	}
}

static int aht10_hwmon_write(struct device *dev, enum hwmon_sensor_types type,
			     u32 attr, int channel, long val)
{
	struct aht10_data *data = dev_get_drvdata(dev);

	switch (type) {
	case hwmon_chip:
		return aht10_interval_write(data, val);
	default:
		return -EOPNOTSUPP;
	}
}

static const struct hwmon_channel_info * const aht10_info[] = {
	HWMON_CHANNEL_INFO(chip, HWMON_C_UPDATE_INTERVAL),
	HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT),
	HWMON_CHANNEL_INFO(humidity, HWMON_H_INPUT),
	NULL,
};

static const struct hwmon_ops aht10_hwmon_ops = {
	.is_visible = aht10_hwmon_visible,
	.read = aht10_hwmon_read,
	.write = aht10_hwmon_write,
};

static const struct hwmon_chip_info aht10_chip_info = {
	.ops = &aht10_hwmon_ops,
	.info = aht10_info,
};

static int aht10_probe(struct i2c_client *client)
{
	const struct i2c_device_id *id = i2c_match_id(aht10_id, client);
	enum aht10_variant variant = id->driver_data;
	struct device *device = &client->dev;
	struct device *hwmon_dev;
	struct aht10_data *data;
	int res;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENOENT;

	data = devm_kzalloc(device, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->min_poll_interval = ms_to_ktime(AHT10_DEFAULT_MIN_POLL_INTERVAL);
	data->client = client;

	switch (variant) {
	case aht20:
		data->meas_size = AHT20_MEAS_SIZE;
		data->crc8 = true;
		crc8_populate_msb(crc8_table, AHT20_CRC8_POLY);
		break;
	default:
		data->meas_size = AHT10_MEAS_SIZE;
		break;
	}

	mutex_init(&data->lock);

	res = aht10_init(data);
	if (res < 0)
		return res;

	res = aht10_read_values(data);
	if (res < 0)
		return res;

	hwmon_dev = devm_hwmon_device_register_with_info(device,
							 client->name,
							 data,
							 &aht10_chip_info,
							 NULL);

	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static struct i2c_driver aht10_driver = {
	.driver = {
		.name = "aht10",
	},
	.probe      = aht10_probe,
	.id_table   = aht10_id,
};

module_i2c_driver(aht10_driver);

MODULE_AUTHOR("Johannes Cornelis Draaijer <jcdra1@gmail.com>");
MODULE_DESCRIPTION("AHT10/AHT20 Temperature and Humidity sensor driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
