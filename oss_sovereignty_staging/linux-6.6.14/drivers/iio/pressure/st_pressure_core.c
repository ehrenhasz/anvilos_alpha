
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <asm/unaligned.h>

#include <linux/iio/common/st_sensors.h>
#include "st_pressure.h"

 

#define MCELSIUS_PER_CELSIUS			1000

 
#define ST_PRESS_LSB_PER_MBAR			4096UL
#define ST_PRESS_KPASCAL_NANO_SCALE		(100000000UL / \
						 ST_PRESS_LSB_PER_MBAR)

 
#define ST_PRESS_LSB_PER_CELSIUS		480UL
#define ST_PRESS_MILLI_CELSIUS_OFFSET		42500UL

 
#define ST_PRESS_FS_AVL_1100MB			1100
#define ST_PRESS_FS_AVL_1260MB			1260

#define ST_PRESS_1_OUT_XL_ADDR			0x28
#define ST_TEMP_1_OUT_L_ADDR			0x2b

 
#define ST_PRESS_LPS001WP_LSB_PER_MBAR		16UL
 
#define ST_PRESS_LPS001WP_LSB_PER_CELSIUS	64UL
 
#define ST_PRESS_LPS001WP_FS_AVL_PRESS_GAIN \
	(100000000UL / ST_PRESS_LPS001WP_LSB_PER_MBAR)
 
#define ST_PRESS_LPS001WP_OUT_L_ADDR		0x28
#define ST_TEMP_LPS001WP_OUT_L_ADDR		0x2a

 
#define ST_PRESS_LPS25H_OUT_XL_ADDR		0x28
#define ST_TEMP_LPS25H_OUT_L_ADDR		0x2b

 
#define ST_PRESS_LPS22HB_LSB_PER_CELSIUS	100UL

static const struct iio_chan_spec st_press_1_channels[] = {
	{
		.type = IIO_PRESSURE,
		.address = ST_PRESS_1_OUT_XL_ADDR,
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 24,
			.storagebits = 32,
			.endianness = IIO_LE,
		},
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
	},
	{
		.type = IIO_TEMP,
		.address = ST_TEMP_1_OUT_L_ADDR,
		.scan_index = 1,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_SCALE) |
			BIT(IIO_CHAN_INFO_OFFSET),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
	},
	IIO_CHAN_SOFT_TIMESTAMP(2)
};

static const struct iio_chan_spec st_press_lps001wp_channels[] = {
	{
		.type = IIO_PRESSURE,
		.address = ST_PRESS_LPS001WP_OUT_L_ADDR,
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_SCALE),
	},
	{
		.type = IIO_TEMP,
		.address = ST_TEMP_LPS001WP_OUT_L_ADDR,
		.scan_index = 1,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_SCALE),
	},
	IIO_CHAN_SOFT_TIMESTAMP(2)
};

static const struct iio_chan_spec st_press_lps22hb_channels[] = {
	{
		.type = IIO_PRESSURE,
		.address = ST_PRESS_1_OUT_XL_ADDR,
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 24,
			.storagebits = 32,
			.endianness = IIO_LE,
		},
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
	},
	{
		.type = IIO_TEMP,
		.address = ST_TEMP_1_OUT_L_ADDR,
		.scan_index = 1,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
		.info_mask_separate =
			BIT(IIO_CHAN_INFO_RAW) |
			BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SAMP_FREQ),
	},
	IIO_CHAN_SOFT_TIMESTAMP(2)
};

static const struct st_sensor_settings st_press_sensors_settings[] = {
	{
		 
		.wai = 0xbb,
		.wai_addr = ST_SENSORS_DEFAULT_WAI_ADDRESS,
		.sensors_supported = {
			[0] = LPS331AP_PRESS_DEV_NAME,
		},
		.ch = (struct iio_chan_spec *)st_press_1_channels,
		.num_ch = ARRAY_SIZE(st_press_1_channels),
		.odr = {
			.addr = 0x20,
			.mask = 0x70,
			.odr_avl = {
				{ .hz = 1, .value = 0x01 },
				{ .hz = 7, .value = 0x05 },
				{ .hz = 13, .value = 0x06 },
				{ .hz = 25, .value = 0x07 },
			},
		},
		.pw = {
			.addr = 0x20,
			.mask = 0x80,
			.value_on = ST_SENSORS_DEFAULT_POWER_ON_VALUE,
			.value_off = ST_SENSORS_DEFAULT_POWER_OFF_VALUE,
		},
		.fs = {
			.addr = 0x23,
			.mask = 0x30,
			.fs_avl = {
				 
				[0] = {
					.num = ST_PRESS_FS_AVL_1260MB,
					.gain = ST_PRESS_KPASCAL_NANO_SCALE,
					.gain2 = ST_PRESS_LSB_PER_CELSIUS,
				},
			},
		},
		.bdu = {
			.addr = 0x20,
			.mask = 0x04,
		},
		.drdy_irq = {
			.int1 = {
				.addr = 0x22,
				.mask = 0x04,
				.addr_od = 0x22,
				.mask_od = 0x40,
			},
			.int2 = {
				.addr = 0x22,
				.mask = 0x20,
				.addr_od = 0x22,
				.mask_od = 0x40,
			},
			.addr_ihl = 0x22,
			.mask_ihl = 0x80,
			.stat_drdy = {
				.addr = ST_SENSORS_DEFAULT_STAT_ADDR,
				.mask = 0x03,
			},
		},
		.sim = {
			.addr = 0x20,
			.value = BIT(0),
		},
		.multi_read_bit = true,
		.bootime = 2,
	},
	{
		 
		.wai = 0xba,
		.wai_addr = ST_SENSORS_DEFAULT_WAI_ADDRESS,
		.sensors_supported = {
			[0] = LPS001WP_PRESS_DEV_NAME,
		},
		.ch = (struct iio_chan_spec *)st_press_lps001wp_channels,
		.num_ch = ARRAY_SIZE(st_press_lps001wp_channels),
		.odr = {
			.addr = 0x20,
			.mask = 0x30,
			.odr_avl = {
				{ .hz = 1, .value = 0x01 },
				{ .hz = 7, .value = 0x02 },
				{ .hz = 13, .value = 0x03 },
			},
		},
		.pw = {
			.addr = 0x20,
			.mask = 0x40,
			.value_on = ST_SENSORS_DEFAULT_POWER_ON_VALUE,
			.value_off = ST_SENSORS_DEFAULT_POWER_OFF_VALUE,
		},
		.fs = {
			.fs_avl = {
				 
				[0] = {
					.num = ST_PRESS_FS_AVL_1100MB,
					.gain = ST_PRESS_LPS001WP_FS_AVL_PRESS_GAIN,
					.gain2 = ST_PRESS_LPS001WP_LSB_PER_CELSIUS,
				},
			},
		},
		.bdu = {
			.addr = 0x20,
			.mask = 0x04,
		},
		.sim = {
			.addr = 0x20,
			.value = BIT(0),
		},
		.multi_read_bit = true,
		.bootime = 2,
	},
	{
		 
		.wai = 0xbd,
		.wai_addr = ST_SENSORS_DEFAULT_WAI_ADDRESS,
		.sensors_supported = {
			[0] = LPS25H_PRESS_DEV_NAME,
		},
		.ch = (struct iio_chan_spec *)st_press_1_channels,
		.num_ch = ARRAY_SIZE(st_press_1_channels),
		.odr = {
			.addr = 0x20,
			.mask = 0x70,
			.odr_avl = {
				{ .hz = 1, .value = 0x01 },
				{ .hz = 7, .value = 0x02 },
				{ .hz = 13, .value = 0x03 },
				{ .hz = 25, .value = 0x04 },
			},
		},
		.pw = {
			.addr = 0x20,
			.mask = 0x80,
			.value_on = ST_SENSORS_DEFAULT_POWER_ON_VALUE,
			.value_off = ST_SENSORS_DEFAULT_POWER_OFF_VALUE,
		},
		.fs = {
			.fs_avl = {
				 
				[0] = {
					.num = ST_PRESS_FS_AVL_1260MB,
					.gain = ST_PRESS_KPASCAL_NANO_SCALE,
					.gain2 = ST_PRESS_LSB_PER_CELSIUS,
				},
			},
		},
		.bdu = {
			.addr = 0x20,
			.mask = 0x04,
		},
		.drdy_irq = {
			.int1 = {
				.addr = 0x23,
				.mask = 0x01,
				.addr_od = 0x22,
				.mask_od = 0x40,
			},
			.addr_ihl = 0x22,
			.mask_ihl = 0x80,
			.stat_drdy = {
				.addr = ST_SENSORS_DEFAULT_STAT_ADDR,
				.mask = 0x03,
			},
		},
		.sim = {
			.addr = 0x20,
			.value = BIT(0),
		},
		.multi_read_bit = true,
		.bootime = 2,
	},
	{
		 
		.wai = 0xb1,
		.wai_addr = ST_SENSORS_DEFAULT_WAI_ADDRESS,
		.sensors_supported = {
			[0] = LPS22HB_PRESS_DEV_NAME,
			[1] = LPS33HW_PRESS_DEV_NAME,
			[2] = LPS35HW_PRESS_DEV_NAME,
		},
		.ch = (struct iio_chan_spec *)st_press_lps22hb_channels,
		.num_ch = ARRAY_SIZE(st_press_lps22hb_channels),
		.odr = {
			.addr = 0x10,
			.mask = 0x70,
			.odr_avl = {
				{ .hz = 1, .value = 0x01 },
				{ .hz = 10, .value = 0x02 },
				{ .hz = 25, .value = 0x03 },
				{ .hz = 50, .value = 0x04 },
				{ .hz = 75, .value = 0x05 },
			},
		},
		.pw = {
			.addr = 0x10,
			.mask = 0x70,
			.value_off = ST_SENSORS_DEFAULT_POWER_OFF_VALUE,
		},
		.fs = {
			.fs_avl = {
				 
				[0] = {
					.num = ST_PRESS_FS_AVL_1260MB,
					.gain = ST_PRESS_KPASCAL_NANO_SCALE,
					.gain2 = ST_PRESS_LPS22HB_LSB_PER_CELSIUS,
				},
			},
		},
		.bdu = {
			.addr = 0x10,
			.mask = 0x02,
		},
		.drdy_irq = {
			.int1 = {
				.addr = 0x12,
				.mask = 0x04,
				.addr_od = 0x12,
				.mask_od = 0x40,
			},
			.addr_ihl = 0x12,
			.mask_ihl = 0x80,
			.stat_drdy = {
				.addr = ST_SENSORS_DEFAULT_STAT_ADDR,
				.mask = 0x03,
			},
		},
		.sim = {
			.addr = 0x10,
			.value = BIT(0),
		},
		.multi_read_bit = false,
		.bootime = 2,
	},
	{
		 
		.wai = 0xb3,
		.wai_addr = ST_SENSORS_DEFAULT_WAI_ADDRESS,
		.sensors_supported = {
			[0] = LPS22HH_PRESS_DEV_NAME,
		},
		.ch = (struct iio_chan_spec *)st_press_lps22hb_channels,
		.num_ch = ARRAY_SIZE(st_press_lps22hb_channels),
		.odr = {
			.addr = 0x10,
			.mask = 0x70,
			.odr_avl = {
				{ .hz = 1, .value = 0x01 },
				{ .hz = 10, .value = 0x02 },
				{ .hz = 25, .value = 0x03 },
				{ .hz = 50, .value = 0x04 },
				{ .hz = 75, .value = 0x05 },
				{ .hz = 100, .value = 0x06 },
				{ .hz = 200, .value = 0x07 },
			},
		},
		.pw = {
			.addr = 0x10,
			.mask = 0x70,
			.value_off = ST_SENSORS_DEFAULT_POWER_OFF_VALUE,
		},
		.fs = {
			.fs_avl = {
				 
				[0] = {
					.num = ST_PRESS_FS_AVL_1260MB,
					.gain = ST_PRESS_KPASCAL_NANO_SCALE,
					.gain2 = ST_PRESS_LPS22HB_LSB_PER_CELSIUS,
				},
			},
		},
		.bdu = {
			.addr = 0x10,
			.mask = BIT(1),
		},
		.drdy_irq = {
			.int1 = {
				.addr = 0x12,
				.mask = BIT(2),
				.addr_od = 0x11,
				.mask_od = BIT(5),
			},
			.addr_ihl = 0x11,
			.mask_ihl = BIT(6),
			.stat_drdy = {
				.addr = ST_SENSORS_DEFAULT_STAT_ADDR,
				.mask = 0x03,
			},
		},
		.sim = {
			.addr = 0x10,
			.value = BIT(0),
		},
		.multi_read_bit = false,
		.bootime = 2,
	},
	{
		 
		.wai = 0xb4,
		.wai_addr = ST_SENSORS_DEFAULT_WAI_ADDRESS,
		.sensors_supported = {
			[0] = LPS22DF_PRESS_DEV_NAME,
		},
		.ch = (struct iio_chan_spec *)st_press_lps22hb_channels,
		.num_ch = ARRAY_SIZE(st_press_lps22hb_channels),
		.odr = {
			.addr = 0x10,
			.mask = 0x78,
			.odr_avl = {
				{ .hz = 1, .value = 0x01 },
				{ .hz = 4, .value = 0x02 },
				{ .hz = 10, .value = 0x03 },
				{ .hz = 25, .value = 0x04 },
				{ .hz = 50, .value = 0x05 },
				{ .hz = 75, .value = 0x06 },
				{ .hz = 100, .value = 0x07 },
				{ .hz = 200, .value = 0x08 },
			},
		},
		.pw = {
			.addr = 0x10,
			.mask = 0x78,
			.value_off = ST_SENSORS_DEFAULT_POWER_OFF_VALUE,
		},
		.fs = {
			.fs_avl = {
				 
				[0] = {
					.num = ST_PRESS_FS_AVL_1260MB,
					.gain = ST_PRESS_KPASCAL_NANO_SCALE,
					.gain2 = ST_PRESS_LPS22HB_LSB_PER_CELSIUS,
				},
			},
		},
		.bdu = {
			.addr = 0x11,
			.mask = BIT(3),
		},
		.drdy_irq = {
			.int1 = {
				.addr = 0x13,
				.mask = BIT(5),
				.addr_od = 0x12,
				.mask_od = BIT(1),
			},
			.addr_ihl = 0x12,
			.mask_ihl = BIT(3),
			.stat_drdy = {
				.addr = ST_SENSORS_DEFAULT_STAT_ADDR,
				.mask = 0x03,
			},
		},
		.sim = {
			.addr = 0x0E,
			.value = BIT(5),
		},
		.multi_read_bit = false,
		.bootime = 2,
	},
};

static int st_press_write_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *ch,
			      int val,
			      int val2,
			      long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		if (val2)
			return -EINVAL;

		return st_sensors_set_odr(indio_dev, val);
	default:
		return -EINVAL;
	}
}

static int st_press_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *ch, int *val,
							int *val2, long mask)
{
	int err;
	struct st_sensor_data *press_data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		err = st_sensors_read_info_raw(indio_dev, ch, val);
		if (err < 0)
			goto read_error;

		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		switch (ch->type) {
		case IIO_PRESSURE:
			*val = 0;
			*val2 = press_data->current_fullscale->gain;
			return IIO_VAL_INT_PLUS_NANO;
		case IIO_TEMP:
			*val = MCELSIUS_PER_CELSIUS;
			*val2 = press_data->current_fullscale->gain2;
			return IIO_VAL_FRACTIONAL;
		default:
			err = -EINVAL;
			goto read_error;
		}

	case IIO_CHAN_INFO_OFFSET:
		switch (ch->type) {
		case IIO_TEMP:
			*val = ST_PRESS_MILLI_CELSIUS_OFFSET *
			       press_data->current_fullscale->gain2;
			*val2 = MCELSIUS_PER_CELSIUS;
			break;
		default:
			err = -EINVAL;
			goto read_error;
		}

		return IIO_VAL_FRACTIONAL;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = press_data->odr;
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}

read_error:
	return err;
}

static ST_SENSORS_DEV_ATTR_SAMP_FREQ_AVAIL();

static struct attribute *st_press_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_press_attribute_group = {
	.attrs = st_press_attributes,
};

static const struct iio_info press_info = {
	.attrs = &st_press_attribute_group,
	.read_raw = &st_press_read_raw,
	.write_raw = &st_press_write_raw,
	.debugfs_reg_access = &st_sensors_debugfs_reg_access,
};

#ifdef CONFIG_IIO_TRIGGER
static const struct iio_trigger_ops st_press_trigger_ops = {
	.set_trigger_state = ST_PRESS_TRIGGER_SET_STATE,
	.validate_device = st_sensors_validate_device,
};
#define ST_PRESS_TRIGGER_OPS (&st_press_trigger_ops)
#else
#define ST_PRESS_TRIGGER_OPS NULL
#endif

 
const struct st_sensor_settings *st_press_get_settings(const char *name)
{
	int index = st_sensors_get_settings_index(name,
					st_press_sensors_settings,
					ARRAY_SIZE(st_press_sensors_settings));
	if (index < 0)
		return NULL;

	return &st_press_sensors_settings[index];
}
EXPORT_SYMBOL_NS(st_press_get_settings, IIO_ST_SENSORS);

int st_press_common_probe(struct iio_dev *indio_dev)
{
	struct st_sensor_data *press_data = iio_priv(indio_dev);
	struct device *parent = indio_dev->dev.parent;
	struct st_sensors_platform_data *pdata = dev_get_platdata(parent);
	int err;

	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &press_info;

	err = st_sensors_verify_id(indio_dev);
	if (err < 0)
		return err;

	 
	press_data->num_data_channels = press_data->sensor_settings->num_ch - 1;
	indio_dev->channels = press_data->sensor_settings->ch;
	indio_dev->num_channels = press_data->sensor_settings->num_ch;

	press_data->current_fullscale = &press_data->sensor_settings->fs.fs_avl[0];

	press_data->odr = press_data->sensor_settings->odr.odr_avl[0].hz;

	 
	if (!pdata && (press_data->sensor_settings->drdy_irq.int1.addr ||
		       press_data->sensor_settings->drdy_irq.int2.addr))
		pdata =	(struct st_sensors_platform_data *)&default_press_pdata;

	err = st_sensors_init_sensor(indio_dev, pdata);
	if (err < 0)
		return err;

	err = st_press_allocate_ring(indio_dev);
	if (err < 0)
		return err;

	if (press_data->irq > 0) {
		err = st_sensors_allocate_trigger(indio_dev,
						  ST_PRESS_TRIGGER_OPS);
		if (err < 0)
			return err;
	}

	return devm_iio_device_register(parent, indio_dev);
}
EXPORT_SYMBOL_NS(st_press_common_probe, IIO_ST_SENSORS);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics pressures driver");
MODULE_LICENSE("GPL v2");
MODULE_IMPORT_NS(IIO_ST_SENSORS);
