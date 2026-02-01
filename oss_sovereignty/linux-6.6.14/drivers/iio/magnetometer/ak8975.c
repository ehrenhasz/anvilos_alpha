
 

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

 
#define AK8975_REG_WIA			0x00
#define AK8975_DEVICE_ID		0x48

#define AK8975_REG_INFO			0x01

#define AK8975_REG_ST1			0x02
#define AK8975_REG_ST1_DRDY_SHIFT	0
#define AK8975_REG_ST1_DRDY_MASK	(1 << AK8975_REG_ST1_DRDY_SHIFT)

#define AK8975_REG_HXL			0x03
#define AK8975_REG_HXH			0x04
#define AK8975_REG_HYL			0x05
#define AK8975_REG_HYH			0x06
#define AK8975_REG_HZL			0x07
#define AK8975_REG_HZH			0x08
#define AK8975_REG_ST2			0x09
#define AK8975_REG_ST2_DERR_SHIFT	2
#define AK8975_REG_ST2_DERR_MASK	(1 << AK8975_REG_ST2_DERR_SHIFT)

#define AK8975_REG_ST2_HOFL_SHIFT	3
#define AK8975_REG_ST2_HOFL_MASK	(1 << AK8975_REG_ST2_HOFL_SHIFT)

#define AK8975_REG_CNTL			0x0A
#define AK8975_REG_CNTL_MODE_SHIFT	0
#define AK8975_REG_CNTL_MODE_MASK	(0xF << AK8975_REG_CNTL_MODE_SHIFT)
#define AK8975_REG_CNTL_MODE_POWER_DOWN	0x00
#define AK8975_REG_CNTL_MODE_ONCE	0x01
#define AK8975_REG_CNTL_MODE_SELF_TEST	0x08
#define AK8975_REG_CNTL_MODE_FUSE_ROM	0x0F

#define AK8975_REG_RSVC			0x0B
#define AK8975_REG_ASTC			0x0C
#define AK8975_REG_TS1			0x0D
#define AK8975_REG_TS2			0x0E
#define AK8975_REG_I2CDIS		0x0F
#define AK8975_REG_ASAX			0x10
#define AK8975_REG_ASAY			0x11
#define AK8975_REG_ASAZ			0x12

#define AK8975_MAX_REGS			AK8975_REG_ASAZ

 
#define AK09912_REG_WIA1		0x00
#define AK09912_REG_WIA2		0x01
#define AK09916_DEVICE_ID		0x09
#define AK09912_DEVICE_ID		0x04
#define AK09911_DEVICE_ID		0x05

#define AK09911_REG_INFO1		0x02
#define AK09911_REG_INFO2		0x03

#define AK09912_REG_ST1			0x10

#define AK09912_REG_ST1_DRDY_SHIFT	0
#define AK09912_REG_ST1_DRDY_MASK	(1 << AK09912_REG_ST1_DRDY_SHIFT)

#define AK09912_REG_HXL			0x11
#define AK09912_REG_HXH			0x12
#define AK09912_REG_HYL			0x13
#define AK09912_REG_HYH			0x14
#define AK09912_REG_HZL			0x15
#define AK09912_REG_HZH			0x16
#define AK09912_REG_TMPS		0x17

#define AK09912_REG_ST2			0x18
#define AK09912_REG_ST2_HOFL_SHIFT	3
#define AK09912_REG_ST2_HOFL_MASK	(1 << AK09912_REG_ST2_HOFL_SHIFT)

#define AK09912_REG_CNTL1		0x30

#define AK09912_REG_CNTL2		0x31
#define AK09912_REG_CNTL_MODE_POWER_DOWN	0x00
#define AK09912_REG_CNTL_MODE_ONCE	0x01
#define AK09912_REG_CNTL_MODE_SELF_TEST	0x10
#define AK09912_REG_CNTL_MODE_FUSE_ROM	0x1F
#define AK09912_REG_CNTL2_MODE_SHIFT	0
#define AK09912_REG_CNTL2_MODE_MASK	(0x1F << AK09912_REG_CNTL2_MODE_SHIFT)

#define AK09912_REG_CNTL3		0x32

#define AK09912_REG_TS1			0x33
#define AK09912_REG_TS2			0x34
#define AK09912_REG_TS3			0x35
#define AK09912_REG_I2CDIS		0x36
#define AK09912_REG_TS4			0x37

#define AK09912_REG_ASAX		0x60
#define AK09912_REG_ASAY		0x61
#define AK09912_REG_ASAZ		0x62

#define AK09912_MAX_REGS		AK09912_REG_ASAZ

 
#define AK8975_MAX_CONVERSION_TIMEOUT	500
#define AK8975_CONVERSION_DONE_POLL_TIME 10
#define AK8975_DATA_READY_TIMEOUT	((100*HZ)/1000)

 
static long ak8975_raw_to_gauss(u16 data)
{
	return (((long)data + 128) * 3000) / 256;
}

 

static long ak8963_09911_raw_to_gauss(u16 data)
{
	return (((long)data + 128) * 6000) / 256;
}

 
static long ak09912_raw_to_gauss(u16 data)
{
	return (((long)data + 128) * 1500) / 256;
}

 
enum asahi_compass_chipset {
	AKXXXX		= 0,
	AK8975,
	AK8963,
	AK09911,
	AK09912,
	AK09916,
};

enum ak_ctrl_reg_addr {
	ST1,
	ST2,
	CNTL,
	ASA_BASE,
	MAX_REGS,
	REGS_END,
};

enum ak_ctrl_reg_mask {
	ST1_DRDY,
	ST2_HOFL,
	ST2_DERR,
	CNTL_MODE,
	MASK_END,
};

enum ak_ctrl_mode {
	POWER_DOWN,
	MODE_ONCE,
	SELF_TEST,
	FUSE_ROM,
	MODE_END,
};

struct ak_def {
	enum asahi_compass_chipset type;
	long (*raw_to_gauss)(u16 data);
	u16 range;
	u8 ctrl_regs[REGS_END];
	u8 ctrl_masks[MASK_END];
	u8 ctrl_modes[MODE_END];
	u8 data_regs[3];
};

static const struct ak_def ak_def_array[] = {
	{
		.type = AK8975,
		.raw_to_gauss = ak8975_raw_to_gauss,
		.range = 4096,
		.ctrl_regs = {
			AK8975_REG_ST1,
			AK8975_REG_ST2,
			AK8975_REG_CNTL,
			AK8975_REG_ASAX,
			AK8975_MAX_REGS},
		.ctrl_masks = {
			AK8975_REG_ST1_DRDY_MASK,
			AK8975_REG_ST2_HOFL_MASK,
			AK8975_REG_ST2_DERR_MASK,
			AK8975_REG_CNTL_MODE_MASK},
		.ctrl_modes = {
			AK8975_REG_CNTL_MODE_POWER_DOWN,
			AK8975_REG_CNTL_MODE_ONCE,
			AK8975_REG_CNTL_MODE_SELF_TEST,
			AK8975_REG_CNTL_MODE_FUSE_ROM},
		.data_regs = {
			AK8975_REG_HXL,
			AK8975_REG_HYL,
			AK8975_REG_HZL},
	},
	{
		.type = AK8963,
		.raw_to_gauss = ak8963_09911_raw_to_gauss,
		.range = 8190,
		.ctrl_regs = {
			AK8975_REG_ST1,
			AK8975_REG_ST2,
			AK8975_REG_CNTL,
			AK8975_REG_ASAX,
			AK8975_MAX_REGS},
		.ctrl_masks = {
			AK8975_REG_ST1_DRDY_MASK,
			AK8975_REG_ST2_HOFL_MASK,
			0,
			AK8975_REG_CNTL_MODE_MASK},
		.ctrl_modes = {
			AK8975_REG_CNTL_MODE_POWER_DOWN,
			AK8975_REG_CNTL_MODE_ONCE,
			AK8975_REG_CNTL_MODE_SELF_TEST,
			AK8975_REG_CNTL_MODE_FUSE_ROM},
		.data_regs = {
			AK8975_REG_HXL,
			AK8975_REG_HYL,
			AK8975_REG_HZL},
	},
	{
		.type = AK09911,
		.raw_to_gauss = ak8963_09911_raw_to_gauss,
		.range = 8192,
		.ctrl_regs = {
			AK09912_REG_ST1,
			AK09912_REG_ST2,
			AK09912_REG_CNTL2,
			AK09912_REG_ASAX,
			AK09912_MAX_REGS},
		.ctrl_masks = {
			AK09912_REG_ST1_DRDY_MASK,
			AK09912_REG_ST2_HOFL_MASK,
			0,
			AK09912_REG_CNTL2_MODE_MASK},
		.ctrl_modes = {
			AK09912_REG_CNTL_MODE_POWER_DOWN,
			AK09912_REG_CNTL_MODE_ONCE,
			AK09912_REG_CNTL_MODE_SELF_TEST,
			AK09912_REG_CNTL_MODE_FUSE_ROM},
		.data_regs = {
			AK09912_REG_HXL,
			AK09912_REG_HYL,
			AK09912_REG_HZL},
	},
	{
		.type = AK09912,
		.raw_to_gauss = ak09912_raw_to_gauss,
		.range = 32752,
		.ctrl_regs = {
			AK09912_REG_ST1,
			AK09912_REG_ST2,
			AK09912_REG_CNTL2,
			AK09912_REG_ASAX,
			AK09912_MAX_REGS},
		.ctrl_masks = {
			AK09912_REG_ST1_DRDY_MASK,
			AK09912_REG_ST2_HOFL_MASK,
			0,
			AK09912_REG_CNTL2_MODE_MASK},
		.ctrl_modes = {
			AK09912_REG_CNTL_MODE_POWER_DOWN,
			AK09912_REG_CNTL_MODE_ONCE,
			AK09912_REG_CNTL_MODE_SELF_TEST,
			AK09912_REG_CNTL_MODE_FUSE_ROM},
		.data_regs = {
			AK09912_REG_HXL,
			AK09912_REG_HYL,
			AK09912_REG_HZL},
	},
	{
		.type = AK09916,
		.raw_to_gauss = ak09912_raw_to_gauss,
		.range = 32752,
		.ctrl_regs = {
			AK09912_REG_ST1,
			AK09912_REG_ST2,
			AK09912_REG_CNTL2,
			AK09912_REG_ASAX,
			AK09912_MAX_REGS},
		.ctrl_masks = {
			AK09912_REG_ST1_DRDY_MASK,
			AK09912_REG_ST2_HOFL_MASK,
			0,
			AK09912_REG_CNTL2_MODE_MASK},
		.ctrl_modes = {
			AK09912_REG_CNTL_MODE_POWER_DOWN,
			AK09912_REG_CNTL_MODE_ONCE,
			AK09912_REG_CNTL_MODE_SELF_TEST,
			AK09912_REG_CNTL_MODE_FUSE_ROM},
		.data_regs = {
			AK09912_REG_HXL,
			AK09912_REG_HYL,
			AK09912_REG_HZL},
	}
};

 
struct ak8975_data {
	struct i2c_client	*client;
	const struct ak_def	*def;
	struct mutex		lock;
	u8			asa[3];
	long			raw_to_gauss[3];
	struct gpio_desc	*eoc_gpiod;
	struct gpio_desc	*reset_gpiod;
	int			eoc_irq;
	wait_queue_head_t	data_ready_queue;
	unsigned long		flags;
	u8			cntl_cache;
	struct iio_mount_matrix orientation;
	struct regulator	*vdd;
	struct regulator	*vid;

	 
	struct {
		s16 channels[3];
		s64 ts __aligned(8);
	} scan;
};

 
static int ak8975_power_on(const struct ak8975_data *data)
{
	int ret;

	ret = regulator_enable(data->vdd);
	if (ret) {
		dev_warn(&data->client->dev,
			 "Failed to enable specified Vdd supply\n");
		return ret;
	}
	ret = regulator_enable(data->vid);
	if (ret) {
		dev_warn(&data->client->dev,
			 "Failed to enable specified Vid supply\n");
		regulator_disable(data->vdd);
		return ret;
	}

	gpiod_set_value_cansleep(data->reset_gpiod, 0);

	 
	usleep_range(500, 1000);
	return 0;
}

 
static void ak8975_power_off(const struct ak8975_data *data)
{
	gpiod_set_value_cansleep(data->reset_gpiod, 1);

	regulator_disable(data->vid);
	regulator_disable(data->vdd);
}

 
static int ak8975_who_i_am(struct i2c_client *client,
			   enum asahi_compass_chipset type)
{
	u8 wia_val[2];
	int ret;

	 
	ret = i2c_smbus_read_i2c_block_data_or_emulated(
			client, AK09912_REG_WIA1, 2, wia_val);
	if (ret < 0) {
		dev_err(&client->dev, "Error reading WIA\n");
		return ret;
	}

	if (wia_val[0] != AK8975_DEVICE_ID)
		return -ENODEV;

	switch (type) {
	case AK8975:
	case AK8963:
		return 0;
	case AK09911:
		if (wia_val[1] == AK09911_DEVICE_ID)
			return 0;
		break;
	case AK09912:
		if (wia_val[1] == AK09912_DEVICE_ID)
			return 0;
		break;
	case AK09916:
		if (wia_val[1] == AK09916_DEVICE_ID)
			return 0;
		break;
	default:
		dev_err(&client->dev, "Type %d unknown\n", type);
	}
	return -ENODEV;
}

 
static int ak8975_set_mode(struct ak8975_data *data, enum ak_ctrl_mode mode)
{
	u8 regval;
	int ret;

	regval = (data->cntl_cache & ~data->def->ctrl_masks[CNTL_MODE]) |
		 data->def->ctrl_modes[mode];
	ret = i2c_smbus_write_byte_data(data->client,
					data->def->ctrl_regs[CNTL], regval);
	if (ret < 0) {
		return ret;
	}
	data->cntl_cache = regval;
	 
	usleep_range(100, 500);

	return 0;
}

 
static irqreturn_t ak8975_irq_handler(int irq, void *data)
{
	struct ak8975_data *ak8975 = data;

	set_bit(0, &ak8975->flags);
	wake_up(&ak8975->data_ready_queue);

	return IRQ_HANDLED;
}

 
static int ak8975_setup_irq(struct ak8975_data *data)
{
	struct i2c_client *client = data->client;
	int rc;
	int irq;

	init_waitqueue_head(&data->data_ready_queue);
	clear_bit(0, &data->flags);
	if (client->irq)
		irq = client->irq;
	else
		irq = gpiod_to_irq(data->eoc_gpiod);

	rc = devm_request_irq(&client->dev, irq, ak8975_irq_handler,
			      IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			      dev_name(&client->dev), data);
	if (rc < 0) {
		dev_err(&client->dev, "irq %d request failed: %d\n", irq, rc);
		return rc;
	}

	data->eoc_irq = irq;

	return rc;
}


 
static int ak8975_setup(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ak8975_data *data = iio_priv(indio_dev);
	int ret;

	 
	ret = ak8975_set_mode(data, FUSE_ROM);
	if (ret < 0) {
		dev_err(&client->dev, "Error in setting fuse access mode\n");
		return ret;
	}

	 
	ret = i2c_smbus_read_i2c_block_data_or_emulated(
			client, data->def->ctrl_regs[ASA_BASE],
			3, data->asa);
	if (ret < 0) {
		dev_err(&client->dev, "Not able to read asa data\n");
		return ret;
	}

	 
	ret = ak8975_set_mode(data, POWER_DOWN);
	if (ret < 0) {
		dev_err(&client->dev, "Error in setting power-down mode\n");
		return ret;
	}

	if (data->eoc_gpiod || client->irq > 0) {
		ret = ak8975_setup_irq(data);
		if (ret < 0) {
			dev_err(&client->dev,
				"Error setting data ready interrupt\n");
			return ret;
		}
	}

	data->raw_to_gauss[0] = data->def->raw_to_gauss(data->asa[0]);
	data->raw_to_gauss[1] = data->def->raw_to_gauss(data->asa[1]);
	data->raw_to_gauss[2] = data->def->raw_to_gauss(data->asa[2]);

	return 0;
}

static int wait_conversion_complete_gpio(struct ak8975_data *data)
{
	struct i2c_client *client = data->client;
	u32 timeout_ms = AK8975_MAX_CONVERSION_TIMEOUT;
	int ret;

	 
	while (timeout_ms) {
		msleep(AK8975_CONVERSION_DONE_POLL_TIME);
		if (gpiod_get_value(data->eoc_gpiod))
			break;
		timeout_ms -= AK8975_CONVERSION_DONE_POLL_TIME;
	}
	if (!timeout_ms) {
		dev_err(&client->dev, "Conversion timeout happened\n");
		return -EINVAL;
	}

	ret = i2c_smbus_read_byte_data(client, data->def->ctrl_regs[ST1]);
	if (ret < 0)
		dev_err(&client->dev, "Error in reading ST1\n");

	return ret;
}

static int wait_conversion_complete_polled(struct ak8975_data *data)
{
	struct i2c_client *client = data->client;
	u8 read_status;
	u32 timeout_ms = AK8975_MAX_CONVERSION_TIMEOUT;
	int ret;

	 
	while (timeout_ms) {
		msleep(AK8975_CONVERSION_DONE_POLL_TIME);
		ret = i2c_smbus_read_byte_data(client,
					       data->def->ctrl_regs[ST1]);
		if (ret < 0) {
			dev_err(&client->dev, "Error in reading ST1\n");
			return ret;
		}
		read_status = ret;
		if (read_status)
			break;
		timeout_ms -= AK8975_CONVERSION_DONE_POLL_TIME;
	}
	if (!timeout_ms) {
		dev_err(&client->dev, "Conversion timeout happened\n");
		return -EINVAL;
	}

	return read_status;
}

 
static int wait_conversion_complete_interrupt(struct ak8975_data *data)
{
	int ret;

	ret = wait_event_timeout(data->data_ready_queue,
				 test_bit(0, &data->flags),
				 AK8975_DATA_READY_TIMEOUT);
	clear_bit(0, &data->flags);

	return ret > 0 ? 0 : -ETIME;
}

static int ak8975_start_read_axis(struct ak8975_data *data,
				  const struct i2c_client *client)
{
	 
	int ret = ak8975_set_mode(data, MODE_ONCE);

	if (ret < 0) {
		dev_err(&client->dev, "Error in setting operating mode\n");
		return ret;
	}

	 
	if (data->eoc_irq)
		ret = wait_conversion_complete_interrupt(data);
	else if (data->eoc_gpiod)
		ret = wait_conversion_complete_gpio(data);
	else
		ret = wait_conversion_complete_polled(data);
	if (ret < 0)
		return ret;

	 
	if (ret & data->def->ctrl_masks[ST1_DRDY]) {
		ret = i2c_smbus_read_byte_data(client,
					       data->def->ctrl_regs[ST2]);
		if (ret < 0) {
			dev_err(&client->dev, "Error in reading ST2\n");
			return ret;
		}
		if (ret & (data->def->ctrl_masks[ST2_DERR] |
			   data->def->ctrl_masks[ST2_HOFL])) {
			dev_err(&client->dev, "ST2 status error 0x%x\n", ret);
			return -EINVAL;
		}
	}

	return 0;
}

 
static int ak8975_read_axis(struct iio_dev *indio_dev, int index, int *val)
{
	struct ak8975_data *data = iio_priv(indio_dev);
	const struct i2c_client *client = data->client;
	const struct ak_def *def = data->def;
	__le16 rval;
	u16 buff;
	int ret;

	pm_runtime_get_sync(&data->client->dev);

	mutex_lock(&data->lock);

	ret = ak8975_start_read_axis(data, client);
	if (ret)
		goto exit;

	ret = i2c_smbus_read_i2c_block_data_or_emulated(
			client, def->data_regs[index],
			sizeof(rval), (u8*)&rval);
	if (ret < 0)
		goto exit;

	mutex_unlock(&data->lock);

	pm_runtime_mark_last_busy(&data->client->dev);
	pm_runtime_put_autosuspend(&data->client->dev);

	 
	buff = le16_to_cpu(rval);
	*val = clamp_t(s16, buff, -def->range, def->range);
	return IIO_VAL_INT;

exit:
	mutex_unlock(&data->lock);
	dev_err(&client->dev, "Error in reading axis\n");
	return ret;
}

static int ak8975_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2,
			   long mask)
{
	struct ak8975_data *data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		return ak8975_read_axis(indio_dev, chan->address, val);
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = data->raw_to_gauss[chan->address];
		return IIO_VAL_INT_PLUS_MICRO;
	}
	return -EINVAL;
}

static const struct iio_mount_matrix *
ak8975_get_mount_matrix(const struct iio_dev *indio_dev,
			const struct iio_chan_spec *chan)
{
	struct ak8975_data *data = iio_priv(indio_dev);

	return &data->orientation;
}

static const struct iio_chan_spec_ext_info ak8975_ext_info[] = {
	IIO_MOUNT_MATRIX(IIO_SHARED_BY_DIR, ak8975_get_mount_matrix),
	{ }
};

#define AK8975_CHANNEL(axis, index)					\
	{								\
		.type = IIO_MAGN,					\
		.modified = 1,						\
		.channel2 = IIO_MOD_##axis,				\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
			     BIT(IIO_CHAN_INFO_SCALE),			\
		.address = index,					\
		.scan_index = index,					\
		.scan_type = {						\
			.sign = 's',					\
			.realbits = 16,					\
			.storagebits = 16,				\
			.endianness = IIO_CPU				\
		},							\
		.ext_info = ak8975_ext_info,				\
	}

static const struct iio_chan_spec ak8975_channels[] = {
	AK8975_CHANNEL(X, 0), AK8975_CHANNEL(Y, 1), AK8975_CHANNEL(Z, 2),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static const unsigned long ak8975_scan_masks[] = { 0x7, 0 };

static const struct iio_info ak8975_info = {
	.read_raw = &ak8975_read_raw,
};

static const struct acpi_device_id ak_acpi_match[] = {
	{"AK8975", AK8975},
	{"AK8963", AK8963},
	{"INVN6500", AK8963},
	{"AK009911", AK09911},
	{"AK09911", AK09911},
	{"AKM9911", AK09911},
	{"AK09912", AK09912},
	{ }
};
MODULE_DEVICE_TABLE(acpi, ak_acpi_match);

static void ak8975_fill_buffer(struct iio_dev *indio_dev)
{
	struct ak8975_data *data = iio_priv(indio_dev);
	const struct i2c_client *client = data->client;
	const struct ak_def *def = data->def;
	int ret;
	__le16 fval[3];

	mutex_lock(&data->lock);

	ret = ak8975_start_read_axis(data, client);
	if (ret)
		goto unlock;

	 
	ret = i2c_smbus_read_i2c_block_data_or_emulated(client,
							def->data_regs[0],
							3 * sizeof(fval[0]),
							(u8 *)fval);
	if (ret < 0)
		goto unlock;

	mutex_unlock(&data->lock);

	 
	data->scan.channels[0] = clamp_t(s16, le16_to_cpu(fval[0]), -def->range, def->range);
	data->scan.channels[1] = clamp_t(s16, le16_to_cpu(fval[1]), -def->range, def->range);
	data->scan.channels[2] = clamp_t(s16, le16_to_cpu(fval[2]), -def->range, def->range);

	iio_push_to_buffers_with_timestamp(indio_dev, &data->scan,
					   iio_get_time_ns(indio_dev));

	return;

unlock:
	mutex_unlock(&data->lock);
	dev_err(&client->dev, "Error in reading axes block\n");
}

static irqreturn_t ak8975_handle_trigger(int irq, void *p)
{
	const struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;

	ak8975_fill_buffer(indio_dev);
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int ak8975_probe(struct i2c_client *client)
{
	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	struct ak8975_data *data;
	struct iio_dev *indio_dev;
	struct gpio_desc *eoc_gpiod;
	struct gpio_desc *reset_gpiod;
	const void *match;
	unsigned int i;
	int err;
	enum asahi_compass_chipset chipset;
	const char *name = NULL;

	 
	eoc_gpiod = devm_gpiod_get_optional(&client->dev, NULL, GPIOD_IN);
	if (IS_ERR(eoc_gpiod))
		return PTR_ERR(eoc_gpiod);
	if (eoc_gpiod)
		gpiod_set_consumer_name(eoc_gpiod, "ak_8975");

	 
	reset_gpiod = devm_gpiod_get_optional(&client->dev,
					      "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(reset_gpiod))
		return PTR_ERR(reset_gpiod);

	 
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (indio_dev == NULL)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);

	data->client = client;
	data->eoc_gpiod = eoc_gpiod;
	data->reset_gpiod = reset_gpiod;
	data->eoc_irq = 0;

	err = iio_read_mount_matrix(&client->dev, &data->orientation);
	if (err)
		return err;

	 
	match = device_get_match_data(&client->dev);
	if (match) {
		chipset = (uintptr_t)match;
		name = dev_name(&client->dev);
	} else if (id) {
		chipset = (enum asahi_compass_chipset)(id->driver_data);
		name = id->name;
	} else
		return -ENOSYS;

	for (i = 0; i < ARRAY_SIZE(ak_def_array); i++)
		if (ak_def_array[i].type == chipset)
			break;

	if (i == ARRAY_SIZE(ak_def_array)) {
		dev_err(&client->dev, "AKM device type unsupported: %d\n",
			chipset);
		return -ENODEV;
	}

	data->def = &ak_def_array[i];

	 
	data->vdd = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR(data->vdd))
		return PTR_ERR(data->vdd);
	data->vid = devm_regulator_get(&client->dev, "vid");
	if (IS_ERR(data->vid))
		return PTR_ERR(data->vid);

	err = ak8975_power_on(data);
	if (err)
		return err;

	err = ak8975_who_i_am(client, data->def->type);
	if (err < 0) {
		dev_err(&client->dev, "Unexpected device\n");
		goto power_off;
	}
	dev_dbg(&client->dev, "Asahi compass chip %s\n", name);

	 
	err = ak8975_setup(client);
	if (err < 0) {
		dev_err(&client->dev, "%s initialization fails\n", name);
		goto power_off;
	}

	mutex_init(&data->lock);
	indio_dev->channels = ak8975_channels;
	indio_dev->num_channels = ARRAY_SIZE(ak8975_channels);
	indio_dev->info = &ak8975_info;
	indio_dev->available_scan_masks = ak8975_scan_masks;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->name = name;

	err = iio_triggered_buffer_setup(indio_dev, NULL, ak8975_handle_trigger,
					 NULL);
	if (err) {
		dev_err(&client->dev, "triggered buffer setup failed\n");
		goto power_off;
	}

	err = iio_device_register(indio_dev);
	if (err) {
		dev_err(&client->dev, "device register failed\n");
		goto cleanup_buffer;
	}

	 
	pm_runtime_get_noresume(&client->dev);
	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	 
	pm_runtime_set_autosuspend_delay(&client->dev, 50);
	pm_runtime_use_autosuspend(&client->dev);
	pm_runtime_put(&client->dev);

	return 0;

cleanup_buffer:
	iio_triggered_buffer_cleanup(indio_dev);
power_off:
	ak8975_power_off(data);
	return err;
}

static void ak8975_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ak8975_data *data = iio_priv(indio_dev);

	pm_runtime_get_sync(&client->dev);
	pm_runtime_put_noidle(&client->dev);
	pm_runtime_disable(&client->dev);
	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	ak8975_set_mode(data, POWER_DOWN);
	ak8975_power_off(data);
}

static int ak8975_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ak8975_data *data = iio_priv(indio_dev);
	int ret;

	 
	ret = ak8975_set_mode(data, POWER_DOWN);
	if (ret < 0) {
		dev_err(&client->dev, "Error in setting power-down mode\n");
		return ret;
	}
	 
	ak8975_power_off(data);

	return 0;
}

static int ak8975_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ak8975_data *data = iio_priv(indio_dev);
	int ret;

	 
	ak8975_power_on(data);
	 
	ret = ak8975_set_mode(data, POWER_DOWN);
	if (ret < 0) {
		dev_err(&client->dev, "Error in setting power-down mode\n");
		return ret;
	}

	return 0;
}

static DEFINE_RUNTIME_DEV_PM_OPS(ak8975_dev_pm_ops, ak8975_runtime_suspend,
				 ak8975_runtime_resume, NULL);

static const struct i2c_device_id ak8975_id[] = {
	{"ak8975", AK8975},
	{"ak8963", AK8963},
	{"AK8963", AK8963},
	{"ak09911", AK09911},
	{"ak09912", AK09912},
	{"ak09916", AK09916},
	{}
};

MODULE_DEVICE_TABLE(i2c, ak8975_id);

static const struct of_device_id ak8975_of_match[] = {
	{ .compatible = "asahi-kasei,ak8975", },
	{ .compatible = "ak8975", },
	{ .compatible = "asahi-kasei,ak8963", },
	{ .compatible = "ak8963", },
	{ .compatible = "asahi-kasei,ak09911", },
	{ .compatible = "ak09911", },
	{ .compatible = "asahi-kasei,ak09912", },
	{ .compatible = "ak09912", },
	{ .compatible = "asahi-kasei,ak09916", },
	{ .compatible = "ak09916", },
	{}
};
MODULE_DEVICE_TABLE(of, ak8975_of_match);

static struct i2c_driver ak8975_driver = {
	.driver = {
		.name	= "ak8975",
		.pm = pm_ptr(&ak8975_dev_pm_ops),
		.of_match_table = ak8975_of_match,
		.acpi_match_table = ak_acpi_match,
	},
	.probe		= ak8975_probe,
	.remove		= ak8975_remove,
	.id_table	= ak8975_id,
};
module_i2c_driver(ak8975_driver);

MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_DESCRIPTION("AK8975 magnetometer driver");
MODULE_LICENSE("GPL");
