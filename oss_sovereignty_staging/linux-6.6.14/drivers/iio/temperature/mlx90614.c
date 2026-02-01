
 

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/jiffies.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define MLX90614_OP_RAM		0x00
#define MLX90614_OP_EEPROM	0x20
#define MLX90614_OP_SLEEP	0xff

#define MLX90615_OP_EEPROM	0x10
#define MLX90615_OP_RAM		0x20
#define MLX90615_OP_SLEEP	0xc6

 
#define MLX90614_CONFIG_IIR_SHIFT 0  
#define MLX90614_CONFIG_IIR_MASK (0x7 << MLX90614_CONFIG_IIR_SHIFT)
#define MLX90614_CONFIG_DUAL_SHIFT 6  
#define MLX90614_CONFIG_DUAL_MASK (1 << MLX90614_CONFIG_DUAL_SHIFT)
#define MLX90614_CONFIG_FIR_SHIFT 8  
#define MLX90614_CONFIG_FIR_MASK (0x7 << MLX90614_CONFIG_FIR_SHIFT)

#define MLX90615_CONFIG_IIR_SHIFT 12  
#define MLX90615_CONFIG_IIR_MASK (0x7 << MLX90615_CONFIG_IIR_SHIFT)

 
#define MLX90614_TIMING_EEPROM 20  
#define MLX90614_TIMING_WAKEUP 34  
#define MLX90614_TIMING_STARTUP 250  

#define MLX90615_TIMING_WAKEUP 22  

#define MLX90614_AUTOSLEEP_DELAY 5000  

 
#define MLX90614_CONST_OFFSET_DEC -13657  
#define MLX90614_CONST_OFFSET_REM 500000  
#define MLX90614_CONST_SCALE 20  
#define MLX90614_CONST_FIR 0x7  

 
#define field_get(_mask, _reg)	(((_reg) & (_mask)) >> (ffs(_mask) - 1))
#define field_prep(_mask, _val)	(((_val) << (ffs(_mask) - 1)) & (_mask))

struct mlx_chip_info {
	 
	 
	u8			op_eeprom_emissivity;
	u8			op_eeprom_config1;
	 
	 
	u8			op_ram_ta;
	 
	u8			op_ram_tobj1;
	 
	u8			op_ram_tobj2;
	u8			op_sleep;
	 
	u8			dual_channel;
	u8			wakeup_delay_ms;
	u16			emissivity_max;
	u16			fir_config_mask;
	u16			iir_config_mask;
	int			iir_valid_offset;
	u16			iir_values[8];
	int			iir_freqs[8][2];
};

struct mlx90614_data {
	struct i2c_client *client;
	struct mutex lock;  
	struct gpio_desc *wakeup_gpio;  
	const struct mlx_chip_info *chip_info;  
	unsigned long ready_timestamp;  
};

 
static s32 mlx90614_write_word(const struct i2c_client *client, u8 command,
			       u16 value)
{
	 
	union i2c_smbus_data data;
	s32 ret;

	dev_dbg(&client->dev, "Writing 0x%x to address 0x%x", value, command);

	data.word = 0x0000;  
	ret = i2c_smbus_xfer(client->adapter, client->addr,
			     client->flags | I2C_CLIENT_PEC,
			     I2C_SMBUS_WRITE, command,
			     I2C_SMBUS_WORD_DATA, &data);
	if (ret < 0)
		return ret;

	msleep(MLX90614_TIMING_EEPROM);

	data.word = value;  
	ret = i2c_smbus_xfer(client->adapter, client->addr,
			     client->flags | I2C_CLIENT_PEC,
			     I2C_SMBUS_WRITE, command,
			     I2C_SMBUS_WORD_DATA, &data);

	msleep(MLX90614_TIMING_EEPROM);

	return ret;
}

 
static inline s32 mlx90614_iir_search(const struct i2c_client *client,
				      int value)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct mlx90614_data *data = iio_priv(indio_dev);
	const struct mlx_chip_info *chip_info = data->chip_info;
	int i;
	s32 ret;

	for (i = chip_info->iir_valid_offset;
	     i < ARRAY_SIZE(chip_info->iir_values);
	     i++) {
		if (value == chip_info->iir_values[i])
			break;
	}

	if (i == ARRAY_SIZE(chip_info->iir_values))
		return -EINVAL;

	 
	ret = i2c_smbus_read_word_data(client, chip_info->op_eeprom_config1);
	if (ret < 0)
		return ret;

	 
	if (chip_info->fir_config_mask) {
		ret &= ~chip_info->fir_config_mask;
		ret |= field_prep(chip_info->fir_config_mask, MLX90614_CONST_FIR);
	}

	ret &= ~chip_info->iir_config_mask;
	ret |= field_prep(chip_info->iir_config_mask, i);

	 
	ret = mlx90614_write_word(client, chip_info->op_eeprom_config1, ret);
	return ret;
}

#ifdef CONFIG_PM
 
static int mlx90614_power_get(struct mlx90614_data *data, bool startup)
{
	unsigned long now;
	int ret;

	if (!data->wakeup_gpio)
		return 0;

	ret = pm_runtime_resume_and_get(&data->client->dev);
	if (ret < 0)
		return ret;

	if (startup) {
		now = jiffies;
		if (time_before(now, data->ready_timestamp) &&
		    msleep_interruptible(jiffies_to_msecs(
				data->ready_timestamp - now)) != 0) {
			pm_runtime_put_autosuspend(&data->client->dev);
			return -EINTR;
		}
	}

	return 0;
}

static void mlx90614_power_put(struct mlx90614_data *data)
{
	if (!data->wakeup_gpio)
		return;

	pm_runtime_mark_last_busy(&data->client->dev);
	pm_runtime_put_autosuspend(&data->client->dev);
}
#else
static inline int mlx90614_power_get(struct mlx90614_data *data, bool startup)
{
	return 0;
}

static inline void mlx90614_power_put(struct mlx90614_data *data)
{
}
#endif

static int mlx90614_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *channel, int *val,
			    int *val2, long mask)
{
	struct mlx90614_data *data = iio_priv(indio_dev);
	const struct mlx_chip_info *chip_info = data->chip_info;
	u8 cmd, idx;
	s32 ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:  
		switch (channel->channel2) {
		case IIO_MOD_TEMP_AMBIENT:
			cmd = chip_info->op_ram_ta;
			break;
		case IIO_MOD_TEMP_OBJECT:
			if (chip_info->dual_channel && channel->channel)
				return -EINVAL;

			switch (channel->channel) {
			case 0:
				cmd = chip_info->op_ram_tobj1;
				break;
			case 1:
				cmd = chip_info->op_ram_tobj2;
				break;
			default:
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
		}

		ret = mlx90614_power_get(data, true);
		if (ret < 0)
			return ret;
		ret = i2c_smbus_read_word_data(data->client, cmd);
		mlx90614_power_put(data);

		if (ret < 0)
			return ret;

		 
		if (ret & 0x8000)
			return -EIO;

		*val = ret;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_OFFSET:
		*val = MLX90614_CONST_OFFSET_DEC;
		*val2 = MLX90614_CONST_OFFSET_REM;
		return IIO_VAL_INT_PLUS_MICRO;
	case IIO_CHAN_INFO_SCALE:
		*val = MLX90614_CONST_SCALE;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_CALIBEMISSIVITY:  
		ret = mlx90614_power_get(data, false);
		if (ret < 0)
			return ret;

		mutex_lock(&data->lock);
		ret = i2c_smbus_read_word_data(data->client,
					       chip_info->op_eeprom_emissivity);
		mutex_unlock(&data->lock);
		mlx90614_power_put(data);

		if (ret < 0)
			return ret;

		if (ret == chip_info->emissivity_max) {
			*val = 1;
			*val2 = 0;
		} else {
			*val = 0;
			*val2 = ret * NSEC_PER_SEC / chip_info->emissivity_max;
		}
		return IIO_VAL_INT_PLUS_NANO;
	 
	case IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY:
		ret = mlx90614_power_get(data, false);
		if (ret < 0)
			return ret;

		mutex_lock(&data->lock);
		ret = i2c_smbus_read_word_data(data->client,
					       chip_info->op_eeprom_config1);
		mutex_unlock(&data->lock);
		mlx90614_power_put(data);

		if (ret < 0)
			return ret;

		idx = field_get(chip_info->iir_config_mask, ret) -
		      chip_info->iir_valid_offset;

		*val = chip_info->iir_values[idx] / 100;
		*val2 = (chip_info->iir_values[idx] % 100) * 10000;
		return IIO_VAL_INT_PLUS_MICRO;
	default:
		return -EINVAL;
	}
}

static int mlx90614_write_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *channel, int val,
			     int val2, long mask)
{
	struct mlx90614_data *data = iio_priv(indio_dev);
	const struct mlx_chip_info *chip_info = data->chip_info;
	s32 ret;

	switch (mask) {
	case IIO_CHAN_INFO_CALIBEMISSIVITY:  
		if (val < 0 || val2 < 0 || val > 1 || (val == 1 && val2 != 0))
			return -EINVAL;
		val = val * chip_info->emissivity_max +
		      val2 * chip_info->emissivity_max / NSEC_PER_SEC;

		ret = mlx90614_power_get(data, false);
		if (ret < 0)
			return ret;

		mutex_lock(&data->lock);
		ret = mlx90614_write_word(data->client,
					  chip_info->op_eeprom_emissivity, val);
		mutex_unlock(&data->lock);
		mlx90614_power_put(data);

		return ret;
	case IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY:  
		if (val < 0 || val2 < 0)
			return -EINVAL;

		ret = mlx90614_power_get(data, false);
		if (ret < 0)
			return ret;

		mutex_lock(&data->lock);
		ret = mlx90614_iir_search(data->client,
					  val * 100 + val2 / 10000);
		mutex_unlock(&data->lock);
		mlx90614_power_put(data);

		return ret;
	default:
		return -EINVAL;
	}
}

static int mlx90614_write_raw_get_fmt(struct iio_dev *indio_dev,
				     struct iio_chan_spec const *channel,
				     long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_CALIBEMISSIVITY:
		return IIO_VAL_INT_PLUS_NANO;
	case IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY:
		return IIO_VAL_INT_PLUS_MICRO;
	default:
		return -EINVAL;
	}
}

static int mlx90614_read_avail(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       const int **vals, int *type, int *length,
			       long mask)
{
	struct mlx90614_data *data = iio_priv(indio_dev);
	const struct mlx_chip_info *chip_info = data->chip_info;

	switch (mask) {
	case IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY:
		*vals = (int *)chip_info->iir_freqs;
		*type = IIO_VAL_INT_PLUS_MICRO;
		*length = 2 * (ARRAY_SIZE(chip_info->iir_freqs) -
			       chip_info->iir_valid_offset);
		return IIO_AVAIL_LIST;
	default:
		return -EINVAL;
	}
}

static const struct iio_chan_spec mlx90614_channels[] = {
	{
		.type = IIO_TEMP,
		.modified = 1,
		.channel2 = IIO_MOD_TEMP_AMBIENT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_OFFSET) |
		    BIT(IIO_CHAN_INFO_SCALE),
	},
	{
		.type = IIO_TEMP,
		.modified = 1,
		.channel2 = IIO_MOD_TEMP_OBJECT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
		    BIT(IIO_CHAN_INFO_CALIBEMISSIVITY) |
			BIT(IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY),
		.info_mask_separate_available =
			BIT(IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_OFFSET) |
		    BIT(IIO_CHAN_INFO_SCALE),
	},
	{
		.type = IIO_TEMP,
		.indexed = 1,
		.modified = 1,
		.channel = 1,
		.channel2 = IIO_MOD_TEMP_OBJECT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
		    BIT(IIO_CHAN_INFO_CALIBEMISSIVITY) |
			BIT(IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY),
		.info_mask_separate_available =
			BIT(IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_OFFSET) |
		    BIT(IIO_CHAN_INFO_SCALE),
	},
};

static const struct iio_info mlx90614_info = {
	.read_raw = mlx90614_read_raw,
	.write_raw = mlx90614_write_raw,
	.write_raw_get_fmt = mlx90614_write_raw_get_fmt,
	.read_avail = mlx90614_read_avail,
};

#ifdef CONFIG_PM
static int mlx90614_sleep(struct mlx90614_data *data)
{
	const struct mlx_chip_info *chip_info = data->chip_info;
	s32 ret;

	if (!data->wakeup_gpio) {
		dev_dbg(&data->client->dev, "Sleep disabled");
		return -ENOSYS;
	}

	dev_dbg(&data->client->dev, "Requesting sleep");

	mutex_lock(&data->lock);
	ret = i2c_smbus_xfer(data->client->adapter, data->client->addr,
			     data->client->flags | I2C_CLIENT_PEC,
			     I2C_SMBUS_WRITE, chip_info->op_sleep,
			     I2C_SMBUS_BYTE, NULL);
	mutex_unlock(&data->lock);

	return ret;
}

static int mlx90614_wakeup(struct mlx90614_data *data)
{
	const struct mlx_chip_info *chip_info = data->chip_info;

	if (!data->wakeup_gpio) {
		dev_dbg(&data->client->dev, "Wake-up disabled");
		return -ENOSYS;
	}

	dev_dbg(&data->client->dev, "Requesting wake-up");

	i2c_lock_bus(data->client->adapter, I2C_LOCK_ROOT_ADAPTER);
	gpiod_direction_output(data->wakeup_gpio, 0);
	msleep(chip_info->wakeup_delay_ms);
	gpiod_direction_input(data->wakeup_gpio);
	i2c_unlock_bus(data->client->adapter, I2C_LOCK_ROOT_ADAPTER);

	data->ready_timestamp = jiffies +
			msecs_to_jiffies(MLX90614_TIMING_STARTUP);

	 
	i2c_smbus_read_word_data(data->client, chip_info->op_eeprom_config1);

	return 0;
}

 
static struct gpio_desc *mlx90614_probe_wakeup(struct i2c_client *client)
{
	struct gpio_desc *gpio;

	if (!i2c_check_functionality(client->adapter,
						I2C_FUNC_SMBUS_WRITE_BYTE)) {
		dev_info(&client->dev,
			 "i2c adapter does not support SMBUS_WRITE_BYTE, sleep disabled");
		return NULL;
	}

	gpio = devm_gpiod_get_optional(&client->dev, "wakeup", GPIOD_IN);

	if (IS_ERR(gpio)) {
		dev_warn(&client->dev,
			 "gpio acquisition failed with error %ld, sleep disabled",
			 PTR_ERR(gpio));
		return NULL;
	} else if (!gpio) {
		dev_info(&client->dev,
			 "wakeup-gpio not found, sleep disabled");
	}

	return gpio;
}
#else
static inline int mlx90614_sleep(struct mlx90614_data *data)
{
	return -ENOSYS;
}
static inline int mlx90614_wakeup(struct mlx90614_data *data)
{
	return -ENOSYS;
}
static inline struct gpio_desc *mlx90614_probe_wakeup(struct i2c_client *client)
{
	return NULL;
}
#endif

 
static int mlx90614_probe_num_ir_sensors(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct mlx90614_data *data = iio_priv(indio_dev);
	const struct mlx_chip_info *chip_info = data->chip_info;
	s32 ret;

	if (chip_info->dual_channel)
		return 0;

	ret = i2c_smbus_read_word_data(client, chip_info->op_eeprom_config1);

	if (ret < 0)
		return ret;

	return (ret & MLX90614_CONFIG_DUAL_MASK) ? 1 : 0;
}

static int mlx90614_probe(struct i2c_client *client)
{
	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	struct iio_dev *indio_dev;
	struct mlx90614_data *data;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WORD_DATA))
		return -EOPNOTSUPP;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);
	data->client = client;
	mutex_init(&data->lock);
	data->wakeup_gpio = mlx90614_probe_wakeup(client);
	data->chip_info = device_get_match_data(&client->dev);

	mlx90614_wakeup(data);

	indio_dev->name = id->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &mlx90614_info;

	ret = mlx90614_probe_num_ir_sensors(client);
	switch (ret) {
	case 0:
		dev_dbg(&client->dev, "Found single sensor");
		indio_dev->channels = mlx90614_channels;
		indio_dev->num_channels = 2;
		break;
	case 1:
		dev_dbg(&client->dev, "Found dual sensor");
		indio_dev->channels = mlx90614_channels;
		indio_dev->num_channels = 3;
		break;
	default:
		return ret;
	}

	if (data->wakeup_gpio) {
		pm_runtime_set_autosuspend_delay(&client->dev,
						 MLX90614_AUTOSLEEP_DELAY);
		pm_runtime_use_autosuspend(&client->dev);
		pm_runtime_set_active(&client->dev);
		pm_runtime_enable(&client->dev);
	}

	return iio_device_register(indio_dev);
}

static void mlx90614_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct mlx90614_data *data = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);

	if (data->wakeup_gpio) {
		pm_runtime_disable(&client->dev);
		if (!pm_runtime_status_suspended(&client->dev))
			mlx90614_sleep(data);
		pm_runtime_set_suspended(&client->dev);
	}
}

static const struct mlx_chip_info mlx90614_chip_info = {
	.op_eeprom_emissivity		= MLX90614_OP_EEPROM | 0x04,
	.op_eeprom_config1		= MLX90614_OP_EEPROM | 0x05,
	.op_ram_ta			= MLX90614_OP_RAM | 0x06,
	.op_ram_tobj1			= MLX90614_OP_RAM | 0x07,
	.op_ram_tobj2			= MLX90614_OP_RAM | 0x08,
	.op_sleep			= MLX90614_OP_SLEEP,
	.dual_channel			= true,
	.wakeup_delay_ms		= MLX90614_TIMING_WAKEUP,
	.emissivity_max			= 65535,
	.fir_config_mask		= MLX90614_CONFIG_FIR_MASK,
	.iir_config_mask		= MLX90614_CONFIG_IIR_MASK,
	.iir_valid_offset		= 0,
	.iir_values			= { 77, 31, 20, 15, 723, 153, 110, 86 },
	.iir_freqs			= {
		{ 0, 150000 },	 
		{ 0, 200000 },	 
		{ 0, 310000 },	 
		{ 0, 770000 },	 
		{ 0, 860000 },	 
		{ 1, 100000 },	 
		{ 1, 530000 },	 
		{ 7, 230000 }	 
	},
};

static const struct mlx_chip_info mlx90615_chip_info = {
	.op_eeprom_emissivity		= MLX90615_OP_EEPROM | 0x03,
	.op_eeprom_config1		= MLX90615_OP_EEPROM | 0x02,
	.op_ram_ta			= MLX90615_OP_RAM | 0x06,
	.op_ram_tobj1			= MLX90615_OP_RAM | 0x07,
	.op_ram_tobj2			= MLX90615_OP_RAM | 0x08,
	.op_sleep			= MLX90615_OP_SLEEP,
	.dual_channel			= false,
	.wakeup_delay_ms		= MLX90615_TIMING_WAKEUP,
	.emissivity_max			= 16383,
	.fir_config_mask		= 0,	 
	.iir_config_mask		= MLX90615_CONFIG_IIR_MASK,
	 
	.iir_valid_offset		= 1,
	.iir_values			= { 500, 50, 30, 20, 15, 13, 10 },
	.iir_freqs			= {
		{ 0, 100000 },	 
		{ 0, 130000 },	 
		{ 0, 150000 },	 
		{ 0, 200000 },	 
		{ 0, 300000 },	 
		{ 0, 500000 },	 
		{ 5, 000000 },	 
	},
};

static const struct i2c_device_id mlx90614_id[] = {
	{ "mlx90614", .driver_data = (kernel_ulong_t)&mlx90614_chip_info },
	{ "mlx90615", .driver_data = (kernel_ulong_t)&mlx90615_chip_info },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mlx90614_id);

static const struct of_device_id mlx90614_of_match[] = {
	{ .compatible = "melexis,mlx90614", .data = &mlx90614_chip_info },
	{ .compatible = "melexis,mlx90615", .data = &mlx90615_chip_info },
	{ }
};
MODULE_DEVICE_TABLE(of, mlx90614_of_match);

static int mlx90614_pm_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct mlx90614_data *data = iio_priv(indio_dev);

	if (data->wakeup_gpio && pm_runtime_active(dev))
		return mlx90614_sleep(data);

	return 0;
}

static int mlx90614_pm_resume(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct mlx90614_data *data = iio_priv(indio_dev);
	int err;

	if (data->wakeup_gpio) {
		err = mlx90614_wakeup(data);
		if (err < 0)
			return err;

		pm_runtime_disable(dev);
		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);
	}

	return 0;
}

static int mlx90614_pm_runtime_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct mlx90614_data *data = iio_priv(indio_dev);

	return mlx90614_sleep(data);
}

static int mlx90614_pm_runtime_resume(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));
	struct mlx90614_data *data = iio_priv(indio_dev);

	return mlx90614_wakeup(data);
}

static const struct dev_pm_ops mlx90614_pm_ops = {
	SYSTEM_SLEEP_PM_OPS(mlx90614_pm_suspend, mlx90614_pm_resume)
	RUNTIME_PM_OPS(mlx90614_pm_runtime_suspend,
		       mlx90614_pm_runtime_resume, NULL)
};

static struct i2c_driver mlx90614_driver = {
	.driver = {
		.name	= "mlx90614",
		.of_match_table = mlx90614_of_match,
		.pm	= pm_ptr(&mlx90614_pm_ops),
	},
	.probe = mlx90614_probe,
	.remove = mlx90614_remove,
	.id_table = mlx90614_id,
};
module_i2c_driver(mlx90614_driver);

MODULE_AUTHOR("Peter Meerwald <pmeerw@pmeerw.net>");
MODULE_AUTHOR("Vianney le Clément de Saint-Marcq <vianney.leclement@essensium.com>");
MODULE_AUTHOR("Crt Mori <cmo@melexis.com>");
MODULE_DESCRIPTION("Melexis MLX90614 contactless IR temperature sensor driver");
MODULE_LICENSE("GPL");
