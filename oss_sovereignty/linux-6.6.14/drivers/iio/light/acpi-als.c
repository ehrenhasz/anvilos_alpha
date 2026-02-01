
 

#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/mutex.h>

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>

#define ACPI_ALS_CLASS			"als"
#define ACPI_ALS_DEVICE_NAME		"acpi-als"
#define ACPI_ALS_NOTIFY_ILLUMINANCE	0x80

 
static const struct iio_chan_spec acpi_als_channels[] = {
	{
		.type		= IIO_LIGHT,
		.scan_type	= {
			.sign		= 's',
			.realbits	= 32,
			.storagebits	= 32,
		},
		 
		.info_mask_separate	= BIT(IIO_CHAN_INFO_RAW) |
					  BIT(IIO_CHAN_INFO_PROCESSED),
	},
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

 
#define ACPI_ALS_EVT_BUFFER_SIZE		\
	(sizeof(s32) + sizeof(s32) + sizeof(s64))

struct acpi_als {
	struct acpi_device	*device;
	struct mutex		lock;
	struct iio_trigger	*trig;

	s32 evt_buffer[ACPI_ALS_EVT_BUFFER_SIZE / sizeof(s32)]  __aligned(8);
};

 
#define ACPI_ALS_ILLUMINANCE	"_ALI"
#define ACPI_ALS_CHROMATICITY	"_ALC"
#define ACPI_ALS_COLOR_TEMP	"_ALT"
#define ACPI_ALS_POLLING	"_ALP"
#define ACPI_ALS_TABLES		"_ALR"

static int acpi_als_read_value(struct acpi_als *als, char *prop, s32 *val)
{
	unsigned long long temp_val;
	acpi_status status;

	status = acpi_evaluate_integer(als->device->handle, prop, NULL,
				       &temp_val);

	if (ACPI_FAILURE(status)) {
		acpi_evaluation_failure_warn(als->device->handle, prop, status);
		return -EIO;
	}

	*val = temp_val;

	return 0;
}

static void acpi_als_notify(struct acpi_device *device, u32 event)
{
	struct iio_dev *indio_dev = acpi_driver_data(device);
	struct acpi_als *als = iio_priv(indio_dev);

	if (iio_buffer_enabled(indio_dev) && iio_trigger_using_own(indio_dev)) {
		switch (event) {
		case ACPI_ALS_NOTIFY_ILLUMINANCE:
			iio_trigger_poll_nested(als->trig);
			break;
		default:
			 
			dev_dbg(&device->dev,
				"Unhandled ACPI ALS event (%08x)!\n",
				event);
		}
	}
}

static int acpi_als_read_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan, int *val,
			     int *val2, long mask)
{
	struct acpi_als *als = iio_priv(indio_dev);
	s32 temp_val;
	int ret;

	if ((mask != IIO_CHAN_INFO_PROCESSED) && (mask != IIO_CHAN_INFO_RAW))
		return -EINVAL;

	 
	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	ret = acpi_als_read_value(als, ACPI_ALS_ILLUMINANCE, &temp_val);
	if (ret < 0)
		return ret;

	*val = temp_val;

	return IIO_VAL_INT;
}

static const struct iio_info acpi_als_info = {
	.read_raw		= acpi_als_read_raw,
};

static irqreturn_t acpi_als_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct acpi_als *als = iio_priv(indio_dev);
	s32 *buffer = als->evt_buffer;
	s32 val;
	int ret;

	mutex_lock(&als->lock);

	ret = acpi_als_read_value(als, ACPI_ALS_ILLUMINANCE, &val);
	if (ret < 0)
		goto out;
	*buffer = val;

	 
	if (!pf->timestamp)
		pf->timestamp = iio_get_time_ns(indio_dev);

	iio_push_to_buffers_with_timestamp(indio_dev, buffer, pf->timestamp);
out:
	mutex_unlock(&als->lock);
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int acpi_als_add(struct acpi_device *device)
{
	struct device *dev = &device->dev;
	struct iio_dev *indio_dev;
	struct acpi_als *als;
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*als));
	if (!indio_dev)
		return -ENOMEM;

	als = iio_priv(indio_dev);

	device->driver_data = indio_dev;
	als->device = device;
	mutex_init(&als->lock);

	indio_dev->name = ACPI_ALS_DEVICE_NAME;
	indio_dev->info = &acpi_als_info;
	indio_dev->channels = acpi_als_channels;
	indio_dev->num_channels = ARRAY_SIZE(acpi_als_channels);

	als->trig = devm_iio_trigger_alloc(dev, "%s-dev%d", indio_dev->name,
					   iio_device_id(indio_dev));
	if (!als->trig)
		return -ENOMEM;

	ret = devm_iio_trigger_register(dev, als->trig);
	if (ret)
		return ret;
	 
	indio_dev->trig = iio_trigger_get(als->trig);

	ret = devm_iio_triggered_buffer_setup(dev, indio_dev,
					      iio_pollfunc_store_time,
					      acpi_als_trigger_handler,
					      NULL);
	if (ret)
		return ret;

	return devm_iio_device_register(dev, indio_dev);
}

static const struct acpi_device_id acpi_als_device_ids[] = {
	{"ACPI0008", 0},
	{},
};

MODULE_DEVICE_TABLE(acpi, acpi_als_device_ids);

static struct acpi_driver acpi_als_driver = {
	.name	= "acpi_als",
	.class	= ACPI_ALS_CLASS,
	.ids	= acpi_als_device_ids,
	.ops = {
		.add	= acpi_als_add,
		.notify	= acpi_als_notify,
	},
};

module_acpi_driver(acpi_als_driver);

MODULE_AUTHOR("Zhang Rui <rui.zhang@intel.com>");
MODULE_AUTHOR("Martin Liska <marxin.liska@gmail.com>");
MODULE_AUTHOR("Marek Vasut <marex@denx.de>");
MODULE_DESCRIPTION("ACPI Ambient Light Sensor Driver");
MODULE_LICENSE("GPL");
