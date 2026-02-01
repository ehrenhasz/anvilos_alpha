
 

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define TMAG5273_DEVICE_CONFIG_1	 0x00
#define TMAG5273_DEVICE_CONFIG_2	 0x01
#define TMAG5273_SENSOR_CONFIG_1	 0x02
#define TMAG5273_SENSOR_CONFIG_2	 0x03
#define TMAG5273_X_THR_CONFIG		 0x04
#define TMAG5273_Y_THR_CONFIG		 0x05
#define TMAG5273_Z_THR_CONFIG		 0x06
#define TMAG5273_T_CONFIG		 0x07
#define TMAG5273_INT_CONFIG_1		 0x08
#define TMAG5273_MAG_GAIN_CONFIG	 0x09
#define TMAG5273_MAG_OFFSET_CONFIG_1	 0x0A
#define TMAG5273_MAG_OFFSET_CONFIG_2	 0x0B
#define TMAG5273_I2C_ADDRESS		 0x0C
#define TMAG5273_DEVICE_ID		 0x0D
#define TMAG5273_MANUFACTURER_ID_LSB	 0x0E
#define TMAG5273_MANUFACTURER_ID_MSB	 0x0F
#define TMAG5273_T_MSB_RESULT		 0x10
#define TMAG5273_T_LSB_RESULT		 0x11
#define TMAG5273_X_MSB_RESULT		 0x12
#define TMAG5273_X_LSB_RESULT		 0x13
#define TMAG5273_Y_MSB_RESULT		 0x14
#define TMAG5273_Y_LSB_RESULT		 0x15
#define TMAG5273_Z_MSB_RESULT		 0x16
#define TMAG5273_Z_LSB_RESULT		 0x17
#define TMAG5273_CONV_STATUS		 0x18
#define TMAG5273_ANGLE_RESULT_MSB	 0x19
#define TMAG5273_ANGLE_RESULT_LSB	 0x1A
#define TMAG5273_MAGNITUDE_RESULT	 0x1B
#define TMAG5273_DEVICE_STATUS		 0x1C
#define TMAG5273_MAX_REG		 TMAG5273_DEVICE_STATUS

#define TMAG5273_AUTOSLEEP_DELAY_MS	 5000
#define TMAG5273_MAX_AVERAGE             32

 
#define TMAG5273_MANUFACTURER_ID	 0x5449

 
#define TMAG5273_AVG_MODE_MASK		 GENMASK(4, 2)
#define TMAG5273_AVG_1_MODE		 FIELD_PREP(TMAG5273_AVG_MODE_MASK, 0)
#define TMAG5273_AVG_2_MODE		 FIELD_PREP(TMAG5273_AVG_MODE_MASK, 1)
#define TMAG5273_AVG_4_MODE		 FIELD_PREP(TMAG5273_AVG_MODE_MASK, 2)
#define TMAG5273_AVG_8_MODE		 FIELD_PREP(TMAG5273_AVG_MODE_MASK, 3)
#define TMAG5273_AVG_16_MODE		 FIELD_PREP(TMAG5273_AVG_MODE_MASK, 4)
#define TMAG5273_AVG_32_MODE		 FIELD_PREP(TMAG5273_AVG_MODE_MASK, 5)

 
#define TMAG5273_OP_MODE_MASK		 GENMASK(1, 0)
#define TMAG5273_OP_MODE_STANDBY	 FIELD_PREP(TMAG5273_OP_MODE_MASK, 0)
#define TMAG5273_OP_MODE_SLEEP		 FIELD_PREP(TMAG5273_OP_MODE_MASK, 1)
#define TMAG5273_OP_MODE_CONT		 FIELD_PREP(TMAG5273_OP_MODE_MASK, 2)
#define TMAG5273_OP_MODE_WAKEUP		 FIELD_PREP(TMAG5273_OP_MODE_MASK, 3)

 
#define TMAG5273_MAG_CH_EN_MASK		 GENMASK(7, 4)
#define TMAG5273_MAG_CH_EN_X_Y_Z	 7

 
#define TMAG5273_Z_RANGE_MASK		 BIT(0)
#define TMAG5273_X_Y_RANGE_MASK		 BIT(1)
#define TMAG5273_ANGLE_EN_MASK		 GENMASK(3, 2)
#define TMAG5273_ANGLE_EN_OFF		 0
#define TMAG5273_ANGLE_EN_X_Y		 1
#define TMAG5273_ANGLE_EN_Y_Z		 2
#define TMAG5273_ANGLE_EN_X_Z		 3

 
#define TMAG5273_T_CH_EN		 BIT(0)

 
#define TMAG5273_VERSION_MASK		 GENMASK(1, 0)

 
#define TMAG5273_CONV_STATUS_COMPLETE	 BIT(0)

enum tmag5273_channels {
	TEMPERATURE = 0,
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
	ANGLE,
	MAGNITUDE,
};

enum tmag5273_scale_index {
	MAGN_RANGE_LOW = 0,
	MAGN_RANGE_HIGH,
	MAGN_RANGE_NUM
};

 
struct tmag5273_data {
	struct device *dev;
	unsigned int devid;
	unsigned int version;
	char name[16];
	unsigned int conv_avg;
	unsigned int scale;
	enum tmag5273_scale_index scale_index;
	unsigned int angle_measurement;
	struct regmap *map;
	struct regulator *vcc;

	 
	struct mutex lock;
};

static const char *const tmag5273_angle_names[] = { "off", "x-y", "y-z", "x-z" };

 
static const unsigned int tmag5273_avg_table[] = {
	1, 2, 4, 8, 16, 32,
};

 
static const struct iio_val_int_plus_micro tmag5273_scale[][MAGN_RANGE_NUM] = {
	{ { 0,     0 }, { 0,     0 } },
	{ { 0, 12200 }, { 0, 24400 } },
	{ { 0, 40600 }, { 0, 81200 } },
	{ { 0,     0 }, { 0,     0 } },
};

static int tmag5273_get_measure(struct tmag5273_data *data, s16 *t, s16 *x,
				s16 *y, s16 *z, u16 *angle, u16 *magnitude)
{
	unsigned int status, val;
	__be16 reg_data[4];
	int ret;

	mutex_lock(&data->lock);

	 
	ret = regmap_read_poll_timeout(data->map, TMAG5273_CONV_STATUS, status,
				       status & TMAG5273_CONV_STATUS_COMPLETE,
				       100, 10000);
	if (ret) {
		dev_err(data->dev, "timeout waiting for measurement\n");
		goto out_unlock;
	}

	ret = regmap_bulk_read(data->map, TMAG5273_T_MSB_RESULT, reg_data,
			       sizeof(reg_data));
	if (ret)
		goto out_unlock;
	*t = be16_to_cpu(reg_data[0]);
	*x = be16_to_cpu(reg_data[1]);
	*y = be16_to_cpu(reg_data[2]);
	*z = be16_to_cpu(reg_data[3]);

	ret = regmap_bulk_read(data->map, TMAG5273_ANGLE_RESULT_MSB,
			       &reg_data[0], sizeof(reg_data[0]));
	if (ret)
		goto out_unlock;
	 
	*angle = be16_to_cpu(reg_data[0]);

	ret = regmap_read(data->map, TMAG5273_MAGNITUDE_RESULT, &val);
	if (ret < 0)
		goto out_unlock;
	*magnitude = val;

out_unlock:
	mutex_unlock(&data->lock);
	return ret;
}

static int tmag5273_write_osr(struct tmag5273_data *data, int val)
{
	int i;

	if (val == data->conv_avg)
		return 0;

	for (i = 0; i < ARRAY_SIZE(tmag5273_avg_table); i++) {
		if (tmag5273_avg_table[i] == val)
			break;
	}
	if (i == ARRAY_SIZE(tmag5273_avg_table))
		return -EINVAL;
	data->conv_avg = val;

	return regmap_update_bits(data->map, TMAG5273_DEVICE_CONFIG_1,
				  TMAG5273_AVG_MODE_MASK,
				  FIELD_PREP(TMAG5273_AVG_MODE_MASK, i));
}

static int tmag5273_write_scale(struct tmag5273_data *data, int scale_micro)
{
	u32 value;
	int i;

	for (i = 0; i < ARRAY_SIZE(tmag5273_scale[0]); i++) {
		if (tmag5273_scale[data->version][i].micro == scale_micro)
			break;
	}
	if (i == ARRAY_SIZE(tmag5273_scale[0]))
		return -EINVAL;
	data->scale_index = i;

	if (data->scale_index == MAGN_RANGE_LOW)
		value = 0;
	else
		value = TMAG5273_Z_RANGE_MASK | TMAG5273_X_Y_RANGE_MASK;

	return regmap_update_bits(data->map, TMAG5273_SENSOR_CONFIG_2,
				  TMAG5273_Z_RANGE_MASK | TMAG5273_X_Y_RANGE_MASK, value);
}

static int tmag5273_read_avail(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       const int **vals, int *type, int *length,
			       long mask)
{
	struct tmag5273_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_OVERSAMPLING_RATIO:
		*vals = tmag5273_avg_table;
		*type = IIO_VAL_INT;
		*length = ARRAY_SIZE(tmag5273_avg_table);
		return IIO_AVAIL_LIST;
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_MAGN:
			*type = IIO_VAL_INT_PLUS_MICRO;
			*vals = (int *)tmag5273_scale[data->version];
			*length = ARRAY_SIZE(tmag5273_scale[data->version]) *
				  MAGN_RANGE_NUM;
			return IIO_AVAIL_LIST;
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

static int tmag5273_read_raw(struct iio_dev *indio_dev,
			     const struct iio_chan_spec *chan, int *val,
			     int *val2, long mask)
{
	struct tmag5273_data *data = iio_priv(indio_dev);
	s16 t, x, y, z;
	u16 angle, magnitude;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED:
	case IIO_CHAN_INFO_RAW:
		ret = pm_runtime_resume_and_get(data->dev);
		if (ret < 0)
			return ret;

		ret = tmag5273_get_measure(data, &t, &x, &y, &z, &angle, &magnitude);

		pm_runtime_mark_last_busy(data->dev);
		pm_runtime_put_autosuspend(data->dev);

		if (ret)
			return ret;

		switch (chan->address) {
		case TEMPERATURE:
			*val = t;
			return IIO_VAL_INT;
		case AXIS_X:
			*val = x;
			return IIO_VAL_INT;
		case AXIS_Y:
			*val = y;
			return IIO_VAL_INT;
		case AXIS_Z:
			*val = z;
			return IIO_VAL_INT;
		case ANGLE:
			*val = angle;
			return IIO_VAL_INT;
		case MAGNITUDE:
			*val = magnitude;
			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_TEMP:
			 
			*val = 10000;
			*val2 = 601;
			return IIO_VAL_FRACTIONAL;
		case IIO_MAGN:
			 
			*val = 0;
			*val2 = tmag5273_scale[data->version]
					      [data->scale_index].micro;
			return IIO_VAL_INT_PLUS_MICRO;
		case IIO_ANGL:
			 
			*val = 1000;
			*val2 = 916732;
			return IIO_VAL_FRACTIONAL;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OFFSET:
		switch (chan->type) {
		case IIO_TEMP:
			*val = -16005;
			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OVERSAMPLING_RATIO:
		*val = data->conv_avg;
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static int tmag5273_write_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan, int val,
			      int val2, long mask)
{
	struct tmag5273_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_OVERSAMPLING_RATIO:
		return tmag5273_write_osr(data, val);
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_MAGN:
			if (val)
				return -EINVAL;
			return tmag5273_write_scale(data, val2);
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

#define TMAG5273_AXIS_CHANNEL(axis, index)				     \
	{								     \
		.type = IIO_MAGN,					     \
		.modified = 1,						     \
		.channel2 = IIO_MOD_##axis,				     \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		     \
				      BIT(IIO_CHAN_INFO_SCALE),		     \
		.info_mask_shared_by_type_available =			     \
				      BIT(IIO_CHAN_INFO_SCALE),		     \
		.info_mask_shared_by_all =				     \
				      BIT(IIO_CHAN_INFO_OVERSAMPLING_RATIO), \
		.info_mask_shared_by_all_available =			     \
				      BIT(IIO_CHAN_INFO_OVERSAMPLING_RATIO), \
		.address = index,					     \
		.scan_index = index,					     \
		.scan_type = {						     \
			.sign = 's',					     \
			.realbits = 16,					     \
			.storagebits = 16,				     \
			.endianness = IIO_CPU,				     \
		},							     \
	}

static const struct iio_chan_spec tmag5273_channels[] = {
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_SCALE) |
			BIT(IIO_CHAN_INFO_OFFSET),
		.address = TEMPERATURE,
		.scan_index = TEMPERATURE,
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_CPU,
		},
	},
	TMAG5273_AXIS_CHANNEL(X, AXIS_X),
	TMAG5273_AXIS_CHANNEL(Y, AXIS_Y),
	TMAG5273_AXIS_CHANNEL(Z, AXIS_Z),
	{
		.type = IIO_ANGL,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all =
				      BIT(IIO_CHAN_INFO_OVERSAMPLING_RATIO),
		.info_mask_shared_by_all_available =
				      BIT(IIO_CHAN_INFO_OVERSAMPLING_RATIO),
		.address = ANGLE,
		.scan_index = ANGLE,
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_CPU,
		},
	},
	{
		.type = IIO_DISTANCE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_all =
				      BIT(IIO_CHAN_INFO_OVERSAMPLING_RATIO),
		.info_mask_shared_by_all_available =
				      BIT(IIO_CHAN_INFO_OVERSAMPLING_RATIO),
		.address = MAGNITUDE,
		.scan_index = MAGNITUDE,
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_CPU,
		},
	},
	IIO_CHAN_SOFT_TIMESTAMP(6),
};

static const struct iio_info tmag5273_info = {
	.read_avail = tmag5273_read_avail,
	.read_raw = tmag5273_read_raw,
	.write_raw = tmag5273_write_raw,
};

static bool tmag5273_volatile_reg(struct device *dev, unsigned int reg)
{
	return reg >= TMAG5273_T_MSB_RESULT && reg <= TMAG5273_MAGNITUDE_RESULT;
}

static const struct regmap_config tmag5273_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = TMAG5273_MAX_REG,
	.volatile_reg = tmag5273_volatile_reg,
};

static int tmag5273_set_operating_mode(struct tmag5273_data *data,
				       unsigned int val)
{
	return regmap_write(data->map, TMAG5273_DEVICE_CONFIG_2, val);
}

static void tmag5273_read_device_property(struct tmag5273_data *data)
{
	struct device *dev = data->dev;
	const char *str;
	int ret;

	data->angle_measurement = TMAG5273_ANGLE_EN_X_Y;

	ret = device_property_read_string(dev, "ti,angle-measurement", &str);
	if (ret)
		return;

	ret = match_string(tmag5273_angle_names,
			   ARRAY_SIZE(tmag5273_angle_names), str);
	if (ret >= 0)
		data->angle_measurement = ret;
}

static void tmag5273_wake_up(struct tmag5273_data *data)
{
	int val;

	 
	regmap_read(data->map, TMAG5273_DEVICE_ID, &val);
	 
	usleep_range(80, 200);
}

static int tmag5273_chip_init(struct tmag5273_data *data)
{
	int ret;

	ret = regmap_write(data->map, TMAG5273_DEVICE_CONFIG_1,
			   TMAG5273_AVG_32_MODE);
	if (ret)
		return ret;
	data->conv_avg = 32;

	ret = regmap_write(data->map, TMAG5273_DEVICE_CONFIG_2,
			   TMAG5273_OP_MODE_CONT);
	if (ret)
		return ret;

	ret = regmap_write(data->map, TMAG5273_SENSOR_CONFIG_1,
			   FIELD_PREP(TMAG5273_MAG_CH_EN_MASK,
				      TMAG5273_MAG_CH_EN_X_Y_Z));
	if (ret)
		return ret;

	ret = regmap_write(data->map, TMAG5273_SENSOR_CONFIG_2,
			   FIELD_PREP(TMAG5273_ANGLE_EN_MASK,
				      data->angle_measurement));
	if (ret)
		return ret;
	data->scale_index = MAGN_RANGE_LOW;

	return regmap_write(data->map, TMAG5273_T_CONFIG, TMAG5273_T_CH_EN);
}

static int tmag5273_check_device_id(struct tmag5273_data *data)
{
	__le16 devid;
	int val, ret;

	ret = regmap_read(data->map, TMAG5273_DEVICE_ID, &val);
	if (ret)
		return dev_err_probe(data->dev, ret, "failed to power on device\n");
	data->version = FIELD_PREP(TMAG5273_VERSION_MASK, val);

	ret = regmap_bulk_read(data->map, TMAG5273_MANUFACTURER_ID_LSB, &devid,
			       sizeof(devid));
	if (ret)
		return dev_err_probe(data->dev, ret, "failed to read device ID\n");
	data->devid = le16_to_cpu(devid);

	switch (data->devid) {
	case TMAG5273_MANUFACTURER_ID:
		 
		snprintf(data->name, sizeof(data->name), "tmag5273x%1u", data->version);
		if (data->version < 1 || data->version > 2)
			dev_warn(data->dev, "Unsupported device %s\n", data->name);
		return 0;
	default:
		 
		dev_warn(data->dev, "Unknown device ID 0x%x\n", data->devid);
		return 0;
	}
}

static void tmag5273_power_down(void *data)
{
	tmag5273_set_operating_mode(data, TMAG5273_OP_MODE_SLEEP);
}

static int tmag5273_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct tmag5273_data *data;
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->dev = dev;
	i2c_set_clientdata(i2c, indio_dev);

	data->map = devm_regmap_init_i2c(i2c, &tmag5273_regmap_config);
	if (IS_ERR(data->map))
		return dev_err_probe(dev, PTR_ERR(data->map),
				     "failed to allocate register map\n");

	mutex_init(&data->lock);

	ret = devm_regulator_get_enable(dev, "vcc");
	if (ret)
		return dev_err_probe(dev, ret, "failed to enable regulator\n");

	tmag5273_wake_up(data);

	ret = tmag5273_check_device_id(data);
	if (ret)
		return ret;

	ret = tmag5273_set_operating_mode(data, TMAG5273_OP_MODE_CONT);
	if (ret)
		return dev_err_probe(dev, ret, "failed to power on device\n");

	 
	ret = devm_add_action_or_reset(dev, tmag5273_power_down, data);
	if (ret)
		return dev_err_probe(dev, ret, "failed to add powerdown action\n");

	ret = pm_runtime_set_active(dev);
	if (ret < 0)
		return ret;

	ret = devm_pm_runtime_enable(dev);
	if (ret)
		return ret;

	pm_runtime_get_noresume(dev);
	pm_runtime_set_autosuspend_delay(dev, TMAG5273_AUTOSLEEP_DELAY_MS);
	pm_runtime_use_autosuspend(dev);

	tmag5273_read_device_property(data);

	ret = tmag5273_chip_init(data);
	if (ret)
		return dev_err_probe(dev, ret, "failed to init device\n");

	indio_dev->info = &tmag5273_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->name = data->name;
	indio_dev->channels = tmag5273_channels;
	indio_dev->num_channels = ARRAY_SIZE(tmag5273_channels);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	ret = devm_iio_device_register(dev, indio_dev);
	if (ret)
		return dev_err_probe(dev, ret, "device register failed\n");

	return 0;
}

static int tmag5273_runtime_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tmag5273_data *data = iio_priv(indio_dev);
	int ret;

	ret = tmag5273_set_operating_mode(data, TMAG5273_OP_MODE_SLEEP);
	if (ret)
		dev_err(dev, "failed to power off device (%pe)\n", ERR_PTR(ret));

	return ret;
}

static int tmag5273_runtime_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tmag5273_data *data = iio_priv(indio_dev);
	int ret;

	tmag5273_wake_up(data);

	ret = tmag5273_set_operating_mode(data, TMAG5273_OP_MODE_CONT);
	if (ret)
		dev_err(dev, "failed to power on device (%pe)\n", ERR_PTR(ret));

	return ret;
}

static DEFINE_RUNTIME_DEV_PM_OPS(tmag5273_pm_ops,
				 tmag5273_runtime_suspend, tmag5273_runtime_resume,
				 NULL);

static const struct i2c_device_id tmag5273_id[] = {
	{ "tmag5273" },
	{   }
};
MODULE_DEVICE_TABLE(i2c, tmag5273_id);

static const struct of_device_id tmag5273_of_match[] = {
	{ .compatible = "ti,tmag5273" },
	{   }
};
MODULE_DEVICE_TABLE(of, tmag5273_of_match);

static struct i2c_driver tmag5273_driver = {
	.driver	 = {
		.name = "tmag5273",
		.of_match_table = tmag5273_of_match,
		.pm = pm_ptr(&tmag5273_pm_ops),
	},
	.probe = tmag5273_probe,
	.id_table = tmag5273_id,
};
module_i2c_driver(tmag5273_driver);

MODULE_DESCRIPTION("TI TMAG5273 Low-Power Linear 3D Hall-Effect Sensor driver");
MODULE_AUTHOR("Gerald Loacker <gerald.loacker@wolfvision.net>");
MODULE_LICENSE("GPL");
