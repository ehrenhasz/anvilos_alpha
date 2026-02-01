
 
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/string.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/buffer.h>
#include <linux/iio/sw_device.h>
#include "iio_simple_dummy.h"

static const struct config_item_type iio_dummy_type = {
	.ct_owner = THIS_MODULE,
};

 
struct iio_dummy_accel_calibscale {
	int val;
	int val2;
	int regval;  
};

static const struct iio_dummy_accel_calibscale dummy_scales[] = {
	{ 0, 100, 0x8 },  
	{ 0, 133, 0x7 },  
	{ 733, 13, 0x9 },  
};

#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS

 
static const struct iio_event_spec iio_dummy_event = {
	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_RISING,
	.mask_separate = BIT(IIO_EV_INFO_VALUE) | BIT(IIO_EV_INFO_ENABLE),
};

 
static const struct iio_event_spec step_detect_event = {
	.type = IIO_EV_TYPE_CHANGE,
	.dir = IIO_EV_DIR_NONE,
	.mask_separate = BIT(IIO_EV_INFO_ENABLE),
};

 
static const struct iio_event_spec iio_running_event = {
	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_RISING,
	.mask_separate = BIT(IIO_EV_INFO_VALUE) | BIT(IIO_EV_INFO_ENABLE),
};

 
static const struct iio_event_spec iio_walking_event = {
	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_FALLING,
	.mask_separate = BIT(IIO_EV_INFO_VALUE) | BIT(IIO_EV_INFO_ENABLE),
};
#endif

 
static const struct iio_chan_spec iio_dummy_channels[] = {
	 
	{
		.type = IIO_VOLTAGE,
		 
		.indexed = 1,
		.channel = 0,
		 
		.info_mask_separate =
		 
		BIT(IIO_CHAN_INFO_RAW) |
		 
		BIT(IIO_CHAN_INFO_OFFSET) |
		 
		BIT(IIO_CHAN_INFO_SCALE),
		 
		.info_mask_shared_by_dir = BIT(IIO_CHAN_INFO_SAMP_FREQ),
		 
		.scan_index = DUMMY_INDEX_VOLTAGE_0,
		.scan_type = {  
			.sign = 'u',  
			.realbits = 13,  
			.storagebits = 16,  
			.shift = 0,  
		},
#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS
		.event_spec = &iio_dummy_event,
		.num_event_specs = 1,
#endif  
	},
	 
	{
		.type = IIO_VOLTAGE,
		.differential = 1,
		 
		.indexed = 1,
		.channel = 1,
		.channel2 = 2,
		 
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		 
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
		 
		.scan_index = DUMMY_INDEX_DIFFVOLTAGE_1M2,
		.scan_type = {  
			.sign = 's',  
			.realbits = 12,  
			.storagebits = 16,  
			.shift = 0,  
		},
	},
	 
	{
		.type = IIO_VOLTAGE,
		.differential = 1,
		.indexed = 1,
		.channel = 3,
		.channel2 = 4,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_dir = BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.scan_index = DUMMY_INDEX_DIFFVOLTAGE_3M4,
		.scan_type = {
			.sign = 's',
			.realbits = 11,
			.storagebits = 16,
			.shift = 0,
		},
	},
	 
	{
		.type = IIO_ACCEL,
		.modified = 1,
		 
		.channel2 = IIO_MOD_X,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
		 
		BIT(IIO_CHAN_INFO_CALIBSCALE) |
		BIT(IIO_CHAN_INFO_CALIBBIAS),
		.info_mask_shared_by_dir = BIT(IIO_CHAN_INFO_SAMP_FREQ),
		.scan_index = DUMMY_INDEX_ACCELX,
		.scan_type = {  
			.sign = 's',  
			.realbits = 16,  
			.storagebits = 16,  
			.shift = 0,  
		},
	},
	 
	IIO_CHAN_SOFT_TIMESTAMP(4),
	 
	{
		.type = IIO_VOLTAGE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = -1,  
		.output = 1,
		.indexed = 1,
		.channel = 0,
	},
	{
		.type = IIO_STEPS,
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_ENABLE) |
			BIT(IIO_CHAN_INFO_CALIBHEIGHT),
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
		.scan_index = -1,  
#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS
		.event_spec = &step_detect_event,
		.num_event_specs = 1,
#endif  
	},
	{
		.type = IIO_ACTIVITY,
		.modified = 1,
		.channel2 = IIO_MOD_RUNNING,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
		.scan_index = -1,  
#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS
		.event_spec = &iio_running_event,
		.num_event_specs = 1,
#endif  
	},
	{
		.type = IIO_ACTIVITY,
		.modified = 1,
		.channel2 = IIO_MOD_WALKING,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
		.scan_index = -1,  
#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS
		.event_spec = &iio_walking_event,
		.num_event_specs = 1,
#endif  
	},
};

 
static int iio_dummy_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val,
			      int *val2,
			      long mask)
{
	struct iio_dummy_state *st = iio_priv(indio_dev);
	int ret = -EINVAL;

	mutex_lock(&st->lock);
	switch (mask) {
	case IIO_CHAN_INFO_RAW:  
		switch (chan->type) {
		case IIO_VOLTAGE:
			if (chan->output) {
				 
				*val = st->dac_val;
				ret = IIO_VAL_INT;
			} else if (chan->differential) {
				if (chan->channel == 1)
					*val = st->differential_adc_val[0];
				else
					*val = st->differential_adc_val[1];
				ret = IIO_VAL_INT;
			} else {
				*val = st->single_ended_adc_val;
				ret = IIO_VAL_INT;
			}
			break;
		case IIO_ACCEL:
			*val = st->accel_val;
			ret = IIO_VAL_INT;
			break;
		default:
			break;
		}
		break;
	case IIO_CHAN_INFO_PROCESSED:
		switch (chan->type) {
		case IIO_STEPS:
			*val = st->steps;
			ret = IIO_VAL_INT;
			break;
		case IIO_ACTIVITY:
			switch (chan->channel2) {
			case IIO_MOD_RUNNING:
				*val = st->activity_running;
				ret = IIO_VAL_INT;
				break;
			case IIO_MOD_WALKING:
				*val = st->activity_walking;
				ret = IIO_VAL_INT;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		break;
	case IIO_CHAN_INFO_OFFSET:
		 
		*val = 7;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_VOLTAGE:
			switch (chan->differential) {
			case 0:
				 
				*val = 0;
				*val2 = 1333;
				ret = IIO_VAL_INT_PLUS_MICRO;
				break;
			case 1:
				 
				*val = 0;
				*val2 = 1344;
				ret = IIO_VAL_INT_PLUS_NANO;
			}
			break;
		default:
			break;
		}
		break;
	case IIO_CHAN_INFO_CALIBBIAS:
		 
		*val = st->accel_calibbias;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_CALIBSCALE:
		*val = st->accel_calibscale->val;
		*val2 = st->accel_calibscale->val2;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = 3;
		*val2 = 33;
		ret = IIO_VAL_INT_PLUS_NANO;
		break;
	case IIO_CHAN_INFO_ENABLE:
		switch (chan->type) {
		case IIO_STEPS:
			*val = st->steps_enabled;
			ret = IIO_VAL_INT;
			break;
		default:
			break;
		}
		break;
	case IIO_CHAN_INFO_CALIBHEIGHT:
		switch (chan->type) {
		case IIO_STEPS:
			*val = st->height;
			ret = IIO_VAL_INT;
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
	mutex_unlock(&st->lock);
	return ret;
}

 
static int iio_dummy_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask)
{
	int i;
	int ret = 0;
	struct iio_dummy_state *st = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		switch (chan->type) {
		case IIO_VOLTAGE:
			if (chan->output == 0)
				return -EINVAL;

			 
			mutex_lock(&st->lock);
			st->dac_val = val;
			mutex_unlock(&st->lock);
			return 0;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_PROCESSED:
		switch (chan->type) {
		case IIO_STEPS:
			mutex_lock(&st->lock);
			st->steps = val;
			mutex_unlock(&st->lock);
			return 0;
		case IIO_ACTIVITY:
			if (val < 0)
				val = 0;
			if (val > 100)
				val = 100;
			switch (chan->channel2) {
			case IIO_MOD_RUNNING:
				st->activity_running = val;
				return 0;
			case IIO_MOD_WALKING:
				st->activity_walking = val;
				return 0;
			default:
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_CALIBSCALE:
		mutex_lock(&st->lock);
		 
		for (i = 0; i < ARRAY_SIZE(dummy_scales); i++)
			if (val == dummy_scales[i].val &&
			    val2 == dummy_scales[i].val2)
				break;
		if (i == ARRAY_SIZE(dummy_scales))
			ret = -EINVAL;
		else
			st->accel_calibscale = &dummy_scales[i];
		mutex_unlock(&st->lock);
		return ret;
	case IIO_CHAN_INFO_CALIBBIAS:
		mutex_lock(&st->lock);
		st->accel_calibbias = val;
		mutex_unlock(&st->lock);
		return 0;
	case IIO_CHAN_INFO_ENABLE:
		switch (chan->type) {
		case IIO_STEPS:
			mutex_lock(&st->lock);
			st->steps_enabled = val;
			mutex_unlock(&st->lock);
			return 0;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_CALIBHEIGHT:
		switch (chan->type) {
		case IIO_STEPS:
			st->height = val;
			return 0;
		default:
			return -EINVAL;
		}

	default:
		return -EINVAL;
	}
}

 
static const struct iio_info iio_dummy_info = {
	.read_raw = &iio_dummy_read_raw,
	.write_raw = &iio_dummy_write_raw,
#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS
	.read_event_config = &iio_simple_dummy_read_event_config,
	.write_event_config = &iio_simple_dummy_write_event_config,
	.read_event_value = &iio_simple_dummy_read_event_value,
	.write_event_value = &iio_simple_dummy_write_event_value,
#endif  
};

 
static int iio_dummy_init_device(struct iio_dev *indio_dev)
{
	struct iio_dummy_state *st = iio_priv(indio_dev);

	st->dac_val = 0;
	st->single_ended_adc_val = 73;
	st->differential_adc_val[0] = 33;
	st->differential_adc_val[1] = -34;
	st->accel_val = 34;
	st->accel_calibbias = -7;
	st->accel_calibscale = &dummy_scales[0];
	st->steps = 47;
	st->activity_running = 98;
	st->activity_walking = 4;

	return 0;
}

 
static struct iio_sw_device *iio_dummy_probe(const char *name)
{
	int ret;
	struct iio_dev *indio_dev;
	struct iio_dummy_state *st;
	struct iio_sw_device *swd;
	struct device *parent = NULL;

	 

	swd = kzalloc(sizeof(*swd), GFP_KERNEL);
	if (!swd)
		return ERR_PTR(-ENOMEM);

	 
	indio_dev = iio_device_alloc(parent, sizeof(*st));
	if (!indio_dev) {
		ret = -ENOMEM;
		goto error_free_swd;
	}

	st = iio_priv(indio_dev);
	mutex_init(&st->lock);

	iio_dummy_init_device(indio_dev);

	  
	swd->device = indio_dev;

	 
	indio_dev->name = kstrdup(name, GFP_KERNEL);
	if (!indio_dev->name) {
		ret = -ENOMEM;
		goto error_free_device;
	}

	 
	indio_dev->channels = iio_dummy_channels;
	indio_dev->num_channels = ARRAY_SIZE(iio_dummy_channels);

	 
	indio_dev->info = &iio_dummy_info;

	 
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = iio_simple_dummy_events_register(indio_dev);
	if (ret < 0)
		goto error_free_name;

	ret = iio_simple_dummy_configure_buffer(indio_dev);
	if (ret < 0)
		goto error_unregister_events;

	ret = iio_device_register(indio_dev);
	if (ret < 0)
		goto error_unconfigure_buffer;

	iio_swd_group_init_type_name(swd, name, &iio_dummy_type);

	return swd;
error_unconfigure_buffer:
	iio_simple_dummy_unconfigure_buffer(indio_dev);
error_unregister_events:
	iio_simple_dummy_events_unregister(indio_dev);
error_free_name:
	kfree(indio_dev->name);
error_free_device:
	iio_device_free(indio_dev);
error_free_swd:
	kfree(swd);
	return ERR_PTR(ret);
}

 
static int iio_dummy_remove(struct iio_sw_device *swd)
{
	 
	struct iio_dev *indio_dev = swd->device;

	 
	iio_device_unregister(indio_dev);

	 

	 
	iio_simple_dummy_unconfigure_buffer(indio_dev);

	iio_simple_dummy_events_unregister(indio_dev);

	 
	kfree(indio_dev->name);
	iio_device_free(indio_dev);

	return 0;
}

 
static const struct iio_sw_device_ops iio_dummy_device_ops = {
	.probe = iio_dummy_probe,
	.remove = iio_dummy_remove,
};

static struct iio_sw_device_type iio_dummy_device = {
	.name = "dummy",
	.owner = THIS_MODULE,
	.ops = &iio_dummy_device_ops,
};

module_iio_sw_device_driver(iio_dummy_device);

MODULE_AUTHOR("Jonathan Cameron <jic23@kernel.org>");
MODULE_DESCRIPTION("IIO dummy driver");
MODULE_LICENSE("GPL v2");
