
 

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/string_helpers.h>
#include <linux/units.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include "kionix-kx022a.h"

 
#define KX022A_FIFO_LENGTH			43
#define KX022A_FIFO_FULL_VALUE			255
#define KX022A_SOFT_RESET_WAIT_TIME_US		(5 * USEC_PER_MSEC)
#define KX022A_SOFT_RESET_TOTAL_WAIT_TIME_US	(500 * USEC_PER_MSEC)

 
#define KX022A_FIFO_SAMPLES_SIZE_BYTES		6
#define KX022A_FIFO_MAX_BYTES					\
	(KX022A_FIFO_LENGTH * KX022A_FIFO_SAMPLES_SIZE_BYTES)

enum {
	KX022A_STATE_SAMPLE,
	KX022A_STATE_FIFO,
};

 
static const struct regmap_range kx022a_volatile_ranges[] = {
	{
		.range_min = KX022A_REG_XHP_L,
		.range_max = KX022A_REG_COTR,
	}, {
		.range_min = KX022A_REG_TSCP,
		.range_max = KX022A_REG_INT_REL,
	}, {
		 
		.range_min = KX022A_REG_CNTL2,
		.range_max = KX022A_REG_CNTL2,
	}, {
		.range_min = KX022A_REG_BUF_STATUS_1,
		.range_max = KX022A_REG_BUF_READ,
	},
};

static const struct regmap_access_table kx022a_volatile_regs = {
	.yes_ranges = &kx022a_volatile_ranges[0],
	.n_yes_ranges = ARRAY_SIZE(kx022a_volatile_ranges),
};

static const struct regmap_range kx022a_precious_ranges[] = {
	{
		.range_min = KX022A_REG_INT_REL,
		.range_max = KX022A_REG_INT_REL,
	},
};

static const struct regmap_access_table kx022a_precious_regs = {
	.yes_ranges = &kx022a_precious_ranges[0],
	.n_yes_ranges = ARRAY_SIZE(kx022a_precious_ranges),
};

 
static const struct regmap_range kx022a_read_only_ranges[] = {
	{
		.range_min = KX022A_REG_XHP_L,
		.range_max = KX022A_REG_INT_REL,
	}, {
		.range_min = KX022A_REG_BUF_STATUS_1,
		.range_max = KX022A_REG_BUF_STATUS_2,
	}, {
		.range_min = KX022A_REG_BUF_READ,
		.range_max = KX022A_REG_BUF_READ,
	},
};

static const struct regmap_access_table kx022a_ro_regs = {
	.no_ranges = &kx022a_read_only_ranges[0],
	.n_no_ranges = ARRAY_SIZE(kx022a_read_only_ranges),
};

static const struct regmap_range kx022a_write_only_ranges[] = {
	{
		.range_min = KX022A_REG_BTS_WUF_TH,
		.range_max = KX022A_REG_BTS_WUF_TH,
	}, {
		.range_min = KX022A_REG_MAN_WAKE,
		.range_max = KX022A_REG_MAN_WAKE,
	}, {
		.range_min = KX022A_REG_SELF_TEST,
		.range_max = KX022A_REG_SELF_TEST,
	}, {
		.range_min = KX022A_REG_BUF_CLEAR,
		.range_max = KX022A_REG_BUF_CLEAR,
	},
};

static const struct regmap_access_table kx022a_wo_regs = {
	.no_ranges = &kx022a_write_only_ranges[0],
	.n_no_ranges = ARRAY_SIZE(kx022a_write_only_ranges),
};

static const struct regmap_range kx022a_noinc_read_ranges[] = {
	{
		.range_min = KX022A_REG_BUF_READ,
		.range_max = KX022A_REG_BUF_READ,
	},
};

static const struct regmap_access_table kx022a_nir_regs = {
	.yes_ranges = &kx022a_noinc_read_ranges[0],
	.n_yes_ranges = ARRAY_SIZE(kx022a_noinc_read_ranges),
};

const struct regmap_config kx022a_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_table = &kx022a_volatile_regs,
	.rd_table = &kx022a_wo_regs,
	.wr_table = &kx022a_ro_regs,
	.rd_noinc_table = &kx022a_nir_regs,
	.precious_table = &kx022a_precious_regs,
	.max_register = KX022A_MAX_REGISTER,
	.cache_type = REGCACHE_RBTREE,
};
EXPORT_SYMBOL_NS_GPL(kx022a_regmap, IIO_KX022A);

struct kx022a_data {
	struct regmap *regmap;
	struct iio_trigger *trig;
	struct device *dev;
	struct iio_mount_matrix orientation;
	int64_t timestamp, old_timestamp;

	int irq;
	int inc_reg;
	int ien_reg;

	unsigned int state;
	unsigned int odr_ns;

	bool trigger_enabled;
	 
	struct mutex mutex;
	u8 watermark;

	 
	__le16 buffer[8] __aligned(IIO_DMA_MINALIGN);
	struct {
		__le16 channels[3];
		s64 ts __aligned(8);
	} scan;
};

static const struct iio_mount_matrix *
kx022a_get_mount_matrix(const struct iio_dev *idev,
			const struct iio_chan_spec *chan)
{
	struct kx022a_data *data = iio_priv(idev);

	return &data->orientation;
}

enum {
	AXIS_X,
	AXIS_Y,
	AXIS_Z,
	AXIS_MAX
};

static const unsigned long kx022a_scan_masks[] = {
	BIT(AXIS_X) | BIT(AXIS_Y) | BIT(AXIS_Z), 0
};

static const struct iio_chan_spec_ext_info kx022a_ext_info[] = {
	IIO_MOUNT_MATRIX(IIO_SHARED_BY_TYPE, kx022a_get_mount_matrix),
	{ }
};

#define KX022A_ACCEL_CHAN(axis, index)				\
{								\
	.type = IIO_ACCEL,					\
	.modified = 1,						\
	.channel2 = IIO_MOD_##axis,				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |	\
				BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.info_mask_shared_by_type_available =			\
				BIT(IIO_CHAN_INFO_SCALE) |	\
				BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.ext_info = kx022a_ext_info,				\
	.address = KX022A_REG_##axis##OUT_L,			\
	.scan_index = index,					\
	.scan_type = {                                          \
		.sign = 's',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.endianness = IIO_LE,				\
	},							\
}

static const struct iio_chan_spec kx022a_channels[] = {
	KX022A_ACCEL_CHAN(X, 0),
	KX022A_ACCEL_CHAN(Y, 1),
	KX022A_ACCEL_CHAN(Z, 2),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

 
static const int kx022a_accel_samp_freq_table[][2] = {
	{ 0, 780000 },
	{ 1, 563000 },
	{ 3, 125000 },
	{ 6, 250000 },
	{ 12, 500000 },
	{ 25, 0 },
	{ 50, 0 },
	{ 100, 0 },
	{ 200, 0 },
};

static const unsigned int kx022a_odrs[] = {
	1282051282,
	639795266,
	320 * MEGA,
	160 * MEGA,
	80 * MEGA,
	40 * MEGA,
	20 * MEGA,
	10 * MEGA,
	5 * MEGA,
};

 
static const int kx022a_scale_table[][2] = {
	{ 0, 598550 },
	{ 0, 1197101 },
	{ 0, 2394202 },
	{ 0, 4788403 },
};

static int kx022a_read_avail(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan,
			     const int **vals, int *type, int *length,
			     long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		*vals = (const int *)kx022a_accel_samp_freq_table;
		*length = ARRAY_SIZE(kx022a_accel_samp_freq_table) *
			  ARRAY_SIZE(kx022a_accel_samp_freq_table[0]);
		*type = IIO_VAL_INT_PLUS_MICRO;
		return IIO_AVAIL_LIST;
	case IIO_CHAN_INFO_SCALE:
		*vals = (const int *)kx022a_scale_table;
		*length = ARRAY_SIZE(kx022a_scale_table) *
			  ARRAY_SIZE(kx022a_scale_table[0]);
		*type = IIO_VAL_INT_PLUS_NANO;
		return IIO_AVAIL_LIST;
	default:
		return -EINVAL;
	}
}

#define KX022A_DEFAULT_PERIOD_NS (20 * NSEC_PER_MSEC)

static void kx022a_reg2freq(unsigned int val,  int *val1, int *val2)
{
	*val1 = kx022a_accel_samp_freq_table[val & KX022A_MASK_ODR][0];
	*val2 = kx022a_accel_samp_freq_table[val & KX022A_MASK_ODR][1];
}

static void kx022a_reg2scale(unsigned int val, unsigned int *val1,
			     unsigned int *val2)
{
	val &= KX022A_MASK_GSEL;
	val >>= KX022A_GSEL_SHIFT;

	*val1 = kx022a_scale_table[val][0];
	*val2 = kx022a_scale_table[val][1];
}

static int kx022a_turn_on_off_unlocked(struct kx022a_data *data, bool on)
{
	int ret;

	if (on)
		ret = regmap_set_bits(data->regmap, KX022A_REG_CNTL,
				      KX022A_MASK_PC1);
	else
		ret = regmap_clear_bits(data->regmap, KX022A_REG_CNTL,
					KX022A_MASK_PC1);
	if (ret)
		dev_err(data->dev, "Turn %s fail %d\n", str_on_off(on), ret);

	return ret;

}

static int kx022a_turn_off_lock(struct kx022a_data *data)
{
	int ret;

	mutex_lock(&data->mutex);
	ret = kx022a_turn_on_off_unlocked(data, false);
	if (ret)
		mutex_unlock(&data->mutex);

	return ret;
}

static int kx022a_turn_on_unlock(struct kx022a_data *data)
{
	int ret;

	ret = kx022a_turn_on_off_unlocked(data, true);
	mutex_unlock(&data->mutex);

	return ret;
}

static int kx022a_write_raw_get_fmt(struct iio_dev *idev,
				    struct iio_chan_spec const *chan,
				    long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		return IIO_VAL_INT_PLUS_NANO;
	case IIO_CHAN_INFO_SAMP_FREQ:
		return IIO_VAL_INT_PLUS_MICRO;
	default:
		return -EINVAL;
	}
}

static int kx022a_write_raw(struct iio_dev *idev,
			    struct iio_chan_spec const *chan,
			    int val, int val2, long mask)
{
	struct kx022a_data *data = iio_priv(idev);
	int ret, n;

	 
	ret = iio_device_claim_direct_mode(idev);
	if (ret)
		return ret;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		n = ARRAY_SIZE(kx022a_accel_samp_freq_table);

		while (n--)
			if (val == kx022a_accel_samp_freq_table[n][0] &&
			    val2 == kx022a_accel_samp_freq_table[n][1])
				break;
		if (n < 0) {
			ret = -EINVAL;
			goto unlock_out;
		}
		ret = kx022a_turn_off_lock(data);
		if (ret)
			break;

		ret = regmap_update_bits(data->regmap,
					 KX022A_REG_ODCNTL,
					 KX022A_MASK_ODR, n);
		data->odr_ns = kx022a_odrs[n];
		kx022a_turn_on_unlock(data);
		break;
	case IIO_CHAN_INFO_SCALE:
		n = ARRAY_SIZE(kx022a_scale_table);

		while (n-- > 0)
			if (val == kx022a_scale_table[n][0] &&
			    val2 == kx022a_scale_table[n][1])
				break;
		if (n < 0) {
			ret = -EINVAL;
			goto unlock_out;
		}

		ret = kx022a_turn_off_lock(data);
		if (ret)
			break;

		ret = regmap_update_bits(data->regmap, KX022A_REG_CNTL,
					 KX022A_MASK_GSEL,
					 n << KX022A_GSEL_SHIFT);
		kx022a_turn_on_unlock(data);
		break;
	default:
		ret = -EINVAL;
		break;
	}

unlock_out:
	iio_device_release_direct_mode(idev);

	return ret;
}

static int kx022a_fifo_set_wmi(struct kx022a_data *data)
{
	u8 threshold;

	threshold = data->watermark;

	return regmap_update_bits(data->regmap, KX022A_REG_BUF_CNTL1,
				  KX022A_MASK_WM_TH, threshold);
}

static int kx022a_get_axis(struct kx022a_data *data,
			   struct iio_chan_spec const *chan,
			   int *val)
{
	int ret;

	ret = regmap_bulk_read(data->regmap, chan->address, &data->buffer[0],
			       sizeof(__le16));
	if (ret)
		return ret;

	*val = le16_to_cpu(data->buffer[0]);

	return IIO_VAL_INT;
}

static int kx022a_read_raw(struct iio_dev *idev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct kx022a_data *data = iio_priv(idev);
	unsigned int regval;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = iio_device_claim_direct_mode(idev);
		if (ret)
			return ret;

		mutex_lock(&data->mutex);
		ret = kx022a_get_axis(data, chan, val);
		mutex_unlock(&data->mutex);

		iio_device_release_direct_mode(idev);

		return ret;

	case IIO_CHAN_INFO_SAMP_FREQ:
		ret = regmap_read(data->regmap, KX022A_REG_ODCNTL, &regval);
		if (ret)
			return ret;

		if ((regval & KX022A_MASK_ODR) >
		    ARRAY_SIZE(kx022a_accel_samp_freq_table)) {
			dev_err(data->dev, "Invalid ODR\n");
			return -EINVAL;
		}

		kx022a_reg2freq(regval, val, val2);

		return IIO_VAL_INT_PLUS_MICRO;

	case IIO_CHAN_INFO_SCALE:
		ret = regmap_read(data->regmap, KX022A_REG_CNTL, &regval);
		if (ret < 0)
			return ret;

		kx022a_reg2scale(regval, val, val2);

		return IIO_VAL_INT_PLUS_NANO;
	}

	return -EINVAL;
};

static int kx022a_set_watermark(struct iio_dev *idev, unsigned int val)
{
	struct kx022a_data *data = iio_priv(idev);

	if (val > KX022A_FIFO_LENGTH)
		val = KX022A_FIFO_LENGTH;

	mutex_lock(&data->mutex);
	data->watermark = val;
	mutex_unlock(&data->mutex);

	return 0;
}

static ssize_t hwfifo_enabled_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct iio_dev *idev = dev_to_iio_dev(dev);
	struct kx022a_data *data = iio_priv(idev);
	bool state;

	mutex_lock(&data->mutex);
	state = data->state;
	mutex_unlock(&data->mutex);

	return sysfs_emit(buf, "%d\n", state);
}

static ssize_t hwfifo_watermark_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct iio_dev *idev = dev_to_iio_dev(dev);
	struct kx022a_data *data = iio_priv(idev);
	int wm;

	mutex_lock(&data->mutex);
	wm = data->watermark;
	mutex_unlock(&data->mutex);

	return sysfs_emit(buf, "%d\n", wm);
}

static IIO_DEVICE_ATTR_RO(hwfifo_enabled, 0);
static IIO_DEVICE_ATTR_RO(hwfifo_watermark, 0);

static const struct iio_dev_attr *kx022a_fifo_attributes[] = {
	&iio_dev_attr_hwfifo_watermark,
	&iio_dev_attr_hwfifo_enabled,
	NULL
};

static int kx022a_drop_fifo_contents(struct kx022a_data *data)
{
	 
	data->timestamp = 0;

	return regmap_write(data->regmap, KX022A_REG_BUF_CLEAR, 0x0);
}

static int __kx022a_fifo_flush(struct iio_dev *idev, unsigned int samples,
			       bool irq)
{
	struct kx022a_data *data = iio_priv(idev);
	struct device *dev = regmap_get_device(data->regmap);
	__le16 buffer[KX022A_FIFO_LENGTH * 3];
	uint64_t sample_period;
	int count, fifo_bytes;
	bool renable = false;
	int64_t tstamp;
	int ret, i;

	ret = regmap_read(data->regmap, KX022A_REG_BUF_STATUS_1, &fifo_bytes);
	if (ret) {
		dev_err(dev, "Error reading buffer status\n");
		return ret;
	}

	 
	if (fifo_bytes == KX022A_FIFO_FULL_VALUE)
		fifo_bytes = KX022A_FIFO_MAX_BYTES;

	if (fifo_bytes % KX022A_FIFO_SAMPLES_SIZE_BYTES)
		dev_warn(data->dev, "Bad FIFO alignment. Data may be corrupt\n");

	count = fifo_bytes / KX022A_FIFO_SAMPLES_SIZE_BYTES;
	if (!count)
		return 0;

	 
	if (!irq) {
		 
		disable_irq(data->irq);
		renable = true;

		data->old_timestamp = data->timestamp;
		data->timestamp = iio_get_time_ns(idev);
	}

	 
	if (data->old_timestamp) {
		sample_period = data->timestamp - data->old_timestamp;
		do_div(sample_period, count);
	} else {
		sample_period = data->odr_ns;
	}
	tstamp = data->timestamp - (count - 1) * sample_period;

	if (samples && count > samples) {
		 
		data->timestamp -= (count - samples) * sample_period;
		count = samples;
	}

	fifo_bytes = count * KX022A_FIFO_SAMPLES_SIZE_BYTES;
	ret = regmap_noinc_read(data->regmap, KX022A_REG_BUF_READ,
				&buffer[0], fifo_bytes);
	if (ret)
		goto renable_out;

	for (i = 0; i < count; i++) {
		__le16 *sam = &buffer[i * 3];
		__le16 *chs;
		int bit;

		chs = &data->scan.channels[0];
		for_each_set_bit(bit, idev->active_scan_mask, AXIS_MAX)
			chs[bit] = sam[bit];

		iio_push_to_buffers_with_timestamp(idev, &data->scan, tstamp);

		tstamp += sample_period;
	}

	ret = count;

renable_out:
	if (renable)
		enable_irq(data->irq);

	return ret;
}

static int kx022a_fifo_flush(struct iio_dev *idev, unsigned int samples)
{
	struct kx022a_data *data = iio_priv(idev);
	int ret;

	mutex_lock(&data->mutex);
	ret = __kx022a_fifo_flush(idev, samples, false);
	mutex_unlock(&data->mutex);

	return ret;
}

static const struct iio_info kx022a_info = {
	.read_raw = &kx022a_read_raw,
	.write_raw = &kx022a_write_raw,
	.write_raw_get_fmt = &kx022a_write_raw_get_fmt,
	.read_avail = &kx022a_read_avail,

	.validate_trigger	= iio_validate_own_trigger,
	.hwfifo_set_watermark	= kx022a_set_watermark,
	.hwfifo_flush_to_buffer	= kx022a_fifo_flush,
};

static int kx022a_set_drdy_irq(struct kx022a_data *data, bool en)
{
	if (en)
		return regmap_set_bits(data->regmap, KX022A_REG_CNTL,
				       KX022A_MASK_DRDY);

	return regmap_clear_bits(data->regmap, KX022A_REG_CNTL,
				 KX022A_MASK_DRDY);
}

static int kx022a_prepare_irq_pin(struct kx022a_data *data)
{
	 
	int mask = KX022A_MASK_IEN | KX022A_MASK_IPOL |
		   KX022A_MASK_ITYP;
	int val = KX022A_MASK_IEN | KX022A_IPOL_LOW |
		  KX022A_ITYP_LEVEL;
	int ret;

	ret = regmap_update_bits(data->regmap, data->inc_reg, mask, val);
	if (ret)
		return ret;

	 
	mask = KX022A_MASK_INS2_DRDY;

	return regmap_set_bits(data->regmap, data->ien_reg, mask);
}

static int kx022a_fifo_disable(struct kx022a_data *data)
{
	int ret = 0;

	ret = kx022a_turn_off_lock(data);
	if (ret)
		return ret;

	ret = regmap_clear_bits(data->regmap, data->ien_reg, KX022A_MASK_WMI);
	if (ret)
		goto unlock_out;

	ret = regmap_clear_bits(data->regmap, KX022A_REG_BUF_CNTL2,
				KX022A_MASK_BUF_EN);
	if (ret)
		goto unlock_out;

	data->state &= ~KX022A_STATE_FIFO;

	kx022a_drop_fifo_contents(data);

	return kx022a_turn_on_unlock(data);

unlock_out:
	mutex_unlock(&data->mutex);

	return ret;
}

static int kx022a_buffer_predisable(struct iio_dev *idev)
{
	struct kx022a_data *data = iio_priv(idev);

	if (iio_device_get_current_mode(idev) == INDIO_BUFFER_TRIGGERED)
		return 0;

	return kx022a_fifo_disable(data);
}

static int kx022a_fifo_enable(struct kx022a_data *data)
{
	int ret;

	ret = kx022a_turn_off_lock(data);
	if (ret)
		return ret;

	 
	ret = kx022a_fifo_set_wmi(data);
	if (ret)
		goto unlock_out;

	 
	ret = regmap_set_bits(data->regmap, KX022A_REG_BUF_CNTL2,
			      KX022A_MASK_BUF_EN);
	if (ret)
		goto unlock_out;

	data->state |= KX022A_STATE_FIFO;
	ret = regmap_set_bits(data->regmap, data->ien_reg,
			      KX022A_MASK_WMI);
	if (ret)
		goto unlock_out;

	return kx022a_turn_on_unlock(data);

unlock_out:
	mutex_unlock(&data->mutex);

	return ret;
}

static int kx022a_buffer_postenable(struct iio_dev *idev)
{
	struct kx022a_data *data = iio_priv(idev);

	 
	if (iio_device_get_current_mode(idev) == INDIO_BUFFER_TRIGGERED)
		return 0;

	return kx022a_fifo_enable(data);
}

static const struct iio_buffer_setup_ops kx022a_buffer_ops = {
	.postenable = kx022a_buffer_postenable,
	.predisable = kx022a_buffer_predisable,
};

static irqreturn_t kx022a_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *idev = pf->indio_dev;
	struct kx022a_data *data = iio_priv(idev);
	int ret;

	ret = regmap_bulk_read(data->regmap, KX022A_REG_XOUT_L, data->buffer,
			       KX022A_FIFO_SAMPLES_SIZE_BYTES);
	if (ret < 0)
		goto err_read;

	iio_push_to_buffers_with_timestamp(idev, data->buffer, data->timestamp);
err_read:
	iio_trigger_notify_done(idev->trig);

	return IRQ_HANDLED;
}

 
static irqreturn_t kx022a_irq_handler(int irq, void *private)
{
	struct iio_dev *idev = private;
	struct kx022a_data *data = iio_priv(idev);

	data->old_timestamp = data->timestamp;
	data->timestamp = iio_get_time_ns(idev);

	if (data->state & KX022A_STATE_FIFO || data->trigger_enabled)
		return IRQ_WAKE_THREAD;

	return IRQ_NONE;
}

 
static irqreturn_t kx022a_irq_thread_handler(int irq, void *private)
{
	struct iio_dev *idev = private;
	struct kx022a_data *data = iio_priv(idev);
	irqreturn_t ret = IRQ_NONE;

	mutex_lock(&data->mutex);

	if (data->trigger_enabled) {
		iio_trigger_poll_nested(data->trig);
		ret = IRQ_HANDLED;
	}

	if (data->state & KX022A_STATE_FIFO) {
		int ok;

		ok = __kx022a_fifo_flush(idev, KX022A_FIFO_LENGTH, true);
		if (ok > 0)
			ret = IRQ_HANDLED;
	}

	mutex_unlock(&data->mutex);

	return ret;
}

static int kx022a_trigger_set_state(struct iio_trigger *trig,
				    bool state)
{
	struct kx022a_data *data = iio_trigger_get_drvdata(trig);
	int ret = 0;

	mutex_lock(&data->mutex);

	if (data->trigger_enabled == state)
		goto unlock_out;

	if (data->state & KX022A_STATE_FIFO) {
		dev_warn(data->dev, "Can't set trigger when FIFO enabled\n");
		ret = -EBUSY;
		goto unlock_out;
	}

	ret = kx022a_turn_on_off_unlocked(data, false);
	if (ret)
		goto unlock_out;

	data->trigger_enabled = state;
	ret = kx022a_set_drdy_irq(data, state);
	if (ret)
		goto unlock_out;

	ret = kx022a_turn_on_off_unlocked(data, true);

unlock_out:
	mutex_unlock(&data->mutex);

	return ret;
}

static const struct iio_trigger_ops kx022a_trigger_ops = {
	.set_trigger_state = kx022a_trigger_set_state,
};

static int kx022a_chip_init(struct kx022a_data *data)
{
	int ret, val;

	 
	ret = regmap_write(data->regmap, KX022A_REG_CNTL2, KX022A_MASK_SRST);
	if (ret)
		return ret;

	 
	msleep(1);

	ret = regmap_read_poll_timeout(data->regmap, KX022A_REG_CNTL2, val,
				       !(val & KX022A_MASK_SRST),
				       KX022A_SOFT_RESET_WAIT_TIME_US,
				       KX022A_SOFT_RESET_TOTAL_WAIT_TIME_US);
	if (ret) {
		dev_err(data->dev, "Sensor reset %s\n",
			val & KX022A_MASK_SRST ? "timeout" : "fail#");
		return ret;
	}

	ret = regmap_reinit_cache(data->regmap, &kx022a_regmap);
	if (ret) {
		dev_err(data->dev, "Failed to reinit reg cache\n");
		return ret;
	}

	 
	ret = regmap_set_bits(data->regmap, KX022A_REG_BUF_CNTL2,
			      KX022A_MASK_BRES16);
	if (ret) {
		dev_err(data->dev, "Failed to set data resolution\n");
		return ret;
	}

	return kx022a_prepare_irq_pin(data);
}

int kx022a_probe_internal(struct device *dev)
{
	static const char * const regulator_names[] = {"io-vdd", "vdd"};
	struct iio_trigger *indio_trig;
	struct fwnode_handle *fwnode;
	struct kx022a_data *data;
	struct regmap *regmap;
	unsigned int chip_id;
	struct iio_dev *idev;
	int ret, irq;
	char *name;

	regmap = dev_get_regmap(dev, NULL);
	if (!regmap) {
		dev_err(dev, "no regmap\n");
		return -EINVAL;
	}

	fwnode = dev_fwnode(dev);
	if (!fwnode)
		return -ENODEV;

	idev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!idev)
		return -ENOMEM;

	data = iio_priv(idev);

	 
	ret = devm_regulator_bulk_get_enable(dev, ARRAY_SIZE(regulator_names),
					     regulator_names);
	if (ret && ret != -ENODEV)
		return dev_err_probe(dev, ret, "failed to enable regulator\n");

	ret = regmap_read(regmap, KX022A_REG_WHO, &chip_id);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to access sensor\n");

	if (chip_id != KX022A_ID) {
		dev_err(dev, "unsupported device 0x%x\n", chip_id);
		return -EINVAL;
	}

	irq = fwnode_irq_get_byname(fwnode, "INT1");
	if (irq > 0) {
		data->inc_reg = KX022A_REG_INC1;
		data->ien_reg = KX022A_REG_INC4;
	} else {
		irq = fwnode_irq_get_byname(fwnode, "INT2");
		if (irq < 0)
			return dev_err_probe(dev, irq, "No suitable IRQ\n");

		data->inc_reg = KX022A_REG_INC5;
		data->ien_reg = KX022A_REG_INC6;
	}

	data->regmap = regmap;
	data->dev = dev;
	data->irq = irq;
	data->odr_ns = KX022A_DEFAULT_PERIOD_NS;
	mutex_init(&data->mutex);

	idev->channels = kx022a_channels;
	idev->num_channels = ARRAY_SIZE(kx022a_channels);
	idev->name = "kx022-accel";
	idev->info = &kx022a_info;
	idev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_SOFTWARE;
	idev->available_scan_masks = kx022a_scan_masks;

	 
	ret = iio_read_mount_matrix(dev, &data->orientation);
	if (ret)
		return ret;

	 
	ret = kx022a_turn_off_lock(data);
	if (ret)
		return ret;

	ret = kx022a_chip_init(data);
	if (ret) {
		mutex_unlock(&data->mutex);
		return ret;
	}

	ret = kx022a_turn_on_unlock(data);
	if (ret)
		return ret;

	ret = devm_iio_triggered_buffer_setup_ext(dev, idev,
						  &iio_pollfunc_store_time,
						  kx022a_trigger_handler,
						  IIO_BUFFER_DIRECTION_IN,
						  &kx022a_buffer_ops,
						  kx022a_fifo_attributes);

	if (ret)
		return dev_err_probe(data->dev, ret,
				     "iio_triggered_buffer_setup_ext FAIL\n");
	indio_trig = devm_iio_trigger_alloc(dev, "%sdata-rdy-dev%d", idev->name,
					    iio_device_id(idev));
	if (!indio_trig)
		return -ENOMEM;

	data->trig = indio_trig;

	indio_trig->ops = &kx022a_trigger_ops;
	iio_trigger_set_drvdata(indio_trig, data);

	 
	name = devm_kasprintf(data->dev, GFP_KERNEL, "%s-kx022a",
			      dev_name(data->dev));

	ret = devm_request_threaded_irq(data->dev, irq, kx022a_irq_handler,
					&kx022a_irq_thread_handler,
					IRQF_ONESHOT, name, idev);
	if (ret)
		return dev_err_probe(data->dev, ret, "Could not request IRQ\n");


	ret = devm_iio_trigger_register(dev, indio_trig);
	if (ret)
		return dev_err_probe(data->dev, ret,
				     "Trigger registration failed\n");

	ret = devm_iio_device_register(data->dev, idev);
	if (ret < 0)
		return dev_err_probe(dev, ret,
				     "Unable to register iio device\n");

	return ret;
}
EXPORT_SYMBOL_NS_GPL(kx022a_probe_internal, IIO_KX022A);

MODULE_DESCRIPTION("ROHM/Kionix KX022A accelerometer driver");
MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_LICENSE("GPL");
