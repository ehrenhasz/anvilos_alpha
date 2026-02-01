
 

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iio/buffer.h>
#include <linux/iio/common/cros_ec_sensors_core.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_data/cros_ec_sensorhub.h>
#include <linux/platform_device.h>

 
#define CROS_EC_FIFO_SIZE (2048 * 2 / 3)

static int cros_ec_get_host_cmd_version_mask(struct cros_ec_device *ec_dev,
					     u16 cmd_offset, u16 cmd, u32 *mask)
{
	int ret;
	struct {
		struct cros_ec_command msg;
		union {
			struct ec_params_get_cmd_versions params;
			struct ec_response_get_cmd_versions resp;
		};
	} __packed buf = {
		.msg = {
			.command = EC_CMD_GET_CMD_VERSIONS + cmd_offset,
			.insize = sizeof(struct ec_response_get_cmd_versions),
			.outsize = sizeof(struct ec_params_get_cmd_versions)
			},
		.params = {.cmd = cmd}
	};

	ret = cros_ec_cmd_xfer_status(ec_dev, &buf.msg);
	if (ret >= 0)
		*mask = buf.resp.version_mask;
	return ret;
}

static void get_default_min_max_freq(enum motionsensor_type type,
				     u32 *min_freq,
				     u32 *max_freq,
				     u32 *max_fifo_events)
{
	 
	*max_fifo_events = CROS_EC_FIFO_SIZE;

	switch (type) {
	case MOTIONSENSE_TYPE_ACCEL:
		*min_freq = 12500;
		*max_freq = 100000;
		break;
	case MOTIONSENSE_TYPE_GYRO:
		*min_freq = 25000;
		*max_freq = 100000;
		break;
	case MOTIONSENSE_TYPE_MAG:
		*min_freq = 5000;
		*max_freq = 25000;
		break;
	case MOTIONSENSE_TYPE_PROX:
	case MOTIONSENSE_TYPE_LIGHT:
		*min_freq = 100;
		*max_freq = 50000;
		break;
	case MOTIONSENSE_TYPE_BARO:
		*min_freq = 250;
		*max_freq = 20000;
		break;
	case MOTIONSENSE_TYPE_ACTIVITY:
	default:
		*min_freq = 0;
		*max_freq = 0;
		break;
	}
}

static int cros_ec_sensor_set_ec_rate(struct cros_ec_sensors_core_state *st,
				      int rate)
{
	int ret;

	if (rate > U16_MAX)
		rate = U16_MAX;

	mutex_lock(&st->cmd_lock);
	st->param.cmd = MOTIONSENSE_CMD_EC_RATE;
	st->param.ec_rate.data = rate;
	ret = cros_ec_motion_send_host_cmd(st, 0);
	mutex_unlock(&st->cmd_lock);
	return ret;
}

static ssize_t cros_ec_sensor_set_report_latency(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	int integer, fract, ret;
	int latency;

	ret = iio_str_to_fixpoint(buf, 100000, &integer, &fract);
	if (ret)
		return ret;

	 
	latency = integer * 1000 + fract / 1000;
	ret = cros_ec_sensor_set_ec_rate(st, latency);
	if (ret < 0)
		return ret;

	return len;
}

static ssize_t cros_ec_sensor_get_report_latency(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	int latency, ret;

	mutex_lock(&st->cmd_lock);
	st->param.cmd = MOTIONSENSE_CMD_EC_RATE;
	st->param.ec_rate.data = EC_MOTION_SENSE_NO_VALUE;

	ret = cros_ec_motion_send_host_cmd(st, 0);
	latency = st->resp->ec_rate.ret;
	mutex_unlock(&st->cmd_lock);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d.%06u\n",
		       latency / 1000,
		       (latency % 1000) * 1000);
}

static IIO_DEVICE_ATTR(hwfifo_timeout, 0644,
		       cros_ec_sensor_get_report_latency,
		       cros_ec_sensor_set_report_latency, 0);

static ssize_t hwfifo_watermark_max_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);

	return sprintf(buf, "%d\n", st->fifo_max_event_count);
}

static IIO_DEVICE_ATTR_RO(hwfifo_watermark_max, 0);

static const struct iio_dev_attr *cros_ec_sensor_fifo_attributes[] = {
	&iio_dev_attr_hwfifo_timeout,
	&iio_dev_attr_hwfifo_watermark_max,
	NULL,
};

int cros_ec_sensors_push_data(struct iio_dev *indio_dev,
			      s16 *data,
			      s64 timestamp)
{
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	s16 *out;
	s64 delta;
	unsigned int i;

	 
	if (iio_device_claim_buffer_mode(indio_dev) < 0)
		return 0;

	out = (s16 *)st->samples;
	for_each_set_bit(i,
			 indio_dev->active_scan_mask,
			 indio_dev->masklength) {
		*out = data[i];
		out++;
	}

	if (iio_device_get_clock(indio_dev) != CLOCK_BOOTTIME)
		delta = iio_get_time_ns(indio_dev) - cros_ec_get_time_ns();
	else
		delta = 0;

	iio_push_to_buffers_with_timestamp(indio_dev, st->samples,
					   timestamp + delta);

	iio_device_release_buffer_mode(indio_dev);
	return 0;
}
EXPORT_SYMBOL_GPL(cros_ec_sensors_push_data);

static void cros_ec_sensors_core_clean(void *arg)
{
	struct platform_device *pdev = (struct platform_device *)arg;
	struct cros_ec_sensorhub *sensor_hub =
		dev_get_drvdata(pdev->dev.parent);
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	u8 sensor_num = st->param.info.sensor_num;

	cros_ec_sensorhub_unregister_push_data(sensor_hub, sensor_num);
}

 
int cros_ec_sensors_core_init(struct platform_device *pdev,
			      struct iio_dev *indio_dev,
			      bool physical_device,
			      cros_ec_sensors_capture_t trigger_capture)
{
	struct device *dev = &pdev->dev;
	struct cros_ec_sensors_core_state *state = iio_priv(indio_dev);
	struct cros_ec_sensorhub *sensor_hub = dev_get_drvdata(dev->parent);
	struct cros_ec_dev *ec = sensor_hub->ec;
	struct cros_ec_sensor_platform *sensor_platform = dev_get_platdata(dev);
	u32 ver_mask, temp;
	int frequencies[ARRAY_SIZE(state->frequencies) / 2] = { 0 };
	int ret, i;

	platform_set_drvdata(pdev, indio_dev);

	state->ec = ec->ec_dev;
	state->msg = devm_kzalloc(&pdev->dev, sizeof(*state->msg) +
				max((u16)sizeof(struct ec_params_motion_sense),
				state->ec->max_response), GFP_KERNEL);
	if (!state->msg)
		return -ENOMEM;

	state->resp = (struct ec_response_motion_sense *)state->msg->data;

	mutex_init(&state->cmd_lock);

	ret = cros_ec_get_host_cmd_version_mask(state->ec,
						ec->cmd_offset,
						EC_CMD_MOTION_SENSE_CMD,
						&ver_mask);
	if (ret < 0)
		return ret;

	 
	state->msg->version = fls(ver_mask) - 1;
	state->msg->command = EC_CMD_MOTION_SENSE_CMD + ec->cmd_offset;
	state->msg->outsize = sizeof(struct ec_params_motion_sense);

	indio_dev->name = pdev->name;

	if (physical_device) {
		enum motionsensor_location loc;

		state->param.cmd = MOTIONSENSE_CMD_INFO;
		state->param.info.sensor_num = sensor_platform->sensor_num;
		ret = cros_ec_motion_send_host_cmd(state, 0);
		if (ret) {
			dev_warn(dev, "Can not access sensor info\n");
			return ret;
		}
		state->type = state->resp->info.type;
		loc = state->resp->info.location;
		if (loc == MOTIONSENSE_LOC_BASE)
			indio_dev->label = "accel-base";
		else if (loc == MOTIONSENSE_LOC_LID)
			indio_dev->label = "accel-display";
		else if (loc == MOTIONSENSE_LOC_CAMERA)
			indio_dev->label = "accel-camera";

		 
		memset(state->sign, 1, CROS_EC_SENSOR_MAX_AXIS);

		for (i = CROS_EC_SENSOR_X; i < CROS_EC_SENSOR_MAX_AXIS; i++)
			state->calib[i].scale = MOTION_SENSE_DEFAULT_SCALE;

		 
		if (state->msg->version < 3) {
			get_default_min_max_freq(state->resp->info.type,
						 &frequencies[1],
						 &frequencies[2],
						 &state->fifo_max_event_count);
		} else {
			if (state->resp->info_3.max_frequency == 0) {
				get_default_min_max_freq(state->resp->info.type,
							 &frequencies[1],
							 &frequencies[2],
							 &temp);
			} else {
				frequencies[1] = state->resp->info_3.min_frequency;
				frequencies[2] = state->resp->info_3.max_frequency;
			}
			state->fifo_max_event_count = state->resp->info_3.fifo_max_event_count;
		}
		for (i = 0; i < ARRAY_SIZE(frequencies); i++) {
			state->frequencies[2 * i] = frequencies[i] / 1000;
			state->frequencies[2 * i + 1] =
				(frequencies[i] % 1000) * 1000;
		}

		if (cros_ec_check_features(ec, EC_FEATURE_MOTION_SENSE_FIFO)) {
			 
			ret = devm_iio_kfifo_buffer_setup_ext(dev, indio_dev, NULL,
							      cros_ec_sensor_fifo_attributes);
			if (ret)
				return ret;

			 
			ret = iio_device_set_clock(indio_dev, CLOCK_BOOTTIME);
			if (ret)
				return ret;

		} else {
			 
			ret = devm_iio_triggered_buffer_setup(dev, indio_dev,
					NULL, trigger_capture, NULL);
			if (ret)
				return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cros_ec_sensors_core_init);

 
int cros_ec_sensors_core_register(struct device *dev,
				  struct iio_dev *indio_dev,
				  cros_ec_sensorhub_push_data_cb_t push_data)
{
	struct cros_ec_sensor_platform *sensor_platform = dev_get_platdata(dev);
	struct cros_ec_sensorhub *sensor_hub = dev_get_drvdata(dev->parent);
	struct platform_device *pdev = to_platform_device(dev);
	struct cros_ec_dev *ec = sensor_hub->ec;
	int ret;

	ret = devm_iio_device_register(dev, indio_dev);
	if (ret)
		return ret;

	if (!push_data ||
	    !cros_ec_check_features(ec, EC_FEATURE_MOTION_SENSE_FIFO))
		return 0;

	ret = cros_ec_sensorhub_register_push_data(
			sensor_hub, sensor_platform->sensor_num,
			indio_dev, push_data);
	if (ret)
		return ret;

	return devm_add_action_or_reset(
			dev, cros_ec_sensors_core_clean, pdev);
}
EXPORT_SYMBOL_GPL(cros_ec_sensors_core_register);

 
int cros_ec_motion_send_host_cmd(struct cros_ec_sensors_core_state *state,
				 u16 opt_length)
{
	int ret;

	if (opt_length)
		state->msg->insize = min(opt_length, state->ec->max_response);
	else
		state->msg->insize = state->ec->max_response;

	memcpy(state->msg->data, &state->param, sizeof(state->param));

	ret = cros_ec_cmd_xfer_status(state->ec, state->msg);
	if (ret < 0)
		return ret;

	if (ret &&
	    state->resp != (struct ec_response_motion_sense *)state->msg->data)
		memcpy(state->resp, state->msg->data, ret);

	return 0;
}
EXPORT_SYMBOL_GPL(cros_ec_motion_send_host_cmd);

static ssize_t cros_ec_sensors_calibrate(struct iio_dev *indio_dev,
		uintptr_t private, const struct iio_chan_spec *chan,
		const char *buf, size_t len)
{
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	int ret, i;
	bool calibrate;

	ret = kstrtobool(buf, &calibrate);
	if (ret < 0)
		return ret;
	if (!calibrate)
		return -EINVAL;

	mutex_lock(&st->cmd_lock);
	st->param.cmd = MOTIONSENSE_CMD_PERFORM_CALIB;
	ret = cros_ec_motion_send_host_cmd(st, 0);
	if (ret != 0) {
		dev_warn(&indio_dev->dev, "Unable to calibrate sensor\n");
	} else {
		 
		for (i = CROS_EC_SENSOR_X; i < CROS_EC_SENSOR_MAX_AXIS; i++)
			st->calib[i].offset = st->resp->perform_calib.offset[i];
	}
	mutex_unlock(&st->cmd_lock);

	return ret ? ret : len;
}

static ssize_t cros_ec_sensors_id(struct iio_dev *indio_dev,
				  uintptr_t private,
				  const struct iio_chan_spec *chan, char *buf)
{
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", st->param.info.sensor_num);
}

const struct iio_chan_spec_ext_info cros_ec_sensors_ext_info[] = {
	{
		.name = "calibrate",
		.shared = IIO_SHARED_BY_ALL,
		.write = cros_ec_sensors_calibrate
	},
	{
		.name = "id",
		.shared = IIO_SHARED_BY_ALL,
		.read = cros_ec_sensors_id
	},
	{ },
};
EXPORT_SYMBOL_GPL(cros_ec_sensors_ext_info);

 
static unsigned int cros_ec_sensors_idx_to_reg(
					struct cros_ec_sensors_core_state *st,
					unsigned int idx)
{
	 
	if (st->type == MOTIONSENSE_TYPE_ACCEL)
		return EC_MEMMAP_ACC_DATA + sizeof(u16) *
			(1 + idx + st->param.info.sensor_num *
			 CROS_EC_SENSOR_MAX_AXIS);

	return EC_MEMMAP_GYRO_DATA + sizeof(u16) * idx;
}

static int cros_ec_sensors_cmd_read_u8(struct cros_ec_device *ec,
				       unsigned int offset, u8 *dest)
{
	return ec->cmd_readmem(ec, offset, 1, dest);
}

static int cros_ec_sensors_cmd_read_u16(struct cros_ec_device *ec,
					 unsigned int offset, u16 *dest)
{
	__le16 tmp;
	int ret = ec->cmd_readmem(ec, offset, 2, &tmp);

	if (ret >= 0)
		*dest = le16_to_cpu(tmp);

	return ret;
}

 
static int cros_ec_sensors_read_until_not_busy(
					struct cros_ec_sensors_core_state *st)
{
	struct cros_ec_device *ec = st->ec;
	u8 status;
	int ret, attempts = 0;

	ret = cros_ec_sensors_cmd_read_u8(ec, EC_MEMMAP_ACC_STATUS, &status);
	if (ret < 0)
		return ret;

	while (status & EC_MEMMAP_ACC_STATUS_BUSY_BIT) {
		 
		if (attempts++ >= 50)
			return -EIO;

		 
		if (attempts % 5 == 0)
			msleep(25);

		ret = cros_ec_sensors_cmd_read_u8(ec, EC_MEMMAP_ACC_STATUS,
						  &status);
		if (ret < 0)
			return ret;
	}

	return status;
}

 
static int cros_ec_sensors_read_data_unsafe(struct iio_dev *indio_dev,
			 unsigned long scan_mask, s16 *data)
{
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	struct cros_ec_device *ec = st->ec;
	unsigned int i;
	int ret;

	 
	for_each_set_bit(i, &scan_mask, indio_dev->masklength) {
		ret = cros_ec_sensors_cmd_read_u16(ec,
					     cros_ec_sensors_idx_to_reg(st, i),
					     data);
		if (ret < 0)
			return ret;

		*data *= st->sign[i];
		data++;
	}

	return 0;
}

 
int cros_ec_sensors_read_lpc(struct iio_dev *indio_dev,
			     unsigned long scan_mask, s16 *data)
{
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	struct cros_ec_device *ec = st->ec;
	u8 samp_id = 0xff, status = 0;
	int ret, attempts = 0;

	 
	while ((status & (EC_MEMMAP_ACC_STATUS_BUSY_BIT |
			  EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK)) != samp_id) {
		 
		if (attempts++ >= 5)
			return -EIO;

		 
		ret = cros_ec_sensors_read_until_not_busy(st);
		if (ret < 0)
			return ret;

		 
		samp_id = ret & EC_MEMMAP_ACC_STATUS_SAMPLE_ID_MASK;

		 
		ret = cros_ec_sensors_read_data_unsafe(indio_dev, scan_mask,
						       data);
		if (ret < 0)
			return ret;

		 
		ret = cros_ec_sensors_cmd_read_u8(ec, EC_MEMMAP_ACC_STATUS,
						  &status);
		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cros_ec_sensors_read_lpc);

 
int cros_ec_sensors_read_cmd(struct iio_dev *indio_dev,
			     unsigned long scan_mask, s16 *data)
{
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	int ret;
	unsigned int i;

	 
	st->param.cmd = MOTIONSENSE_CMD_DATA;
	ret = cros_ec_motion_send_host_cmd(st, sizeof(st->resp->data));
	if (ret != 0) {
		dev_warn(&indio_dev->dev, "Unable to read sensor data\n");
		return ret;
	}

	for_each_set_bit(i, &scan_mask, indio_dev->masklength) {
		*data = st->resp->data.data[i];
		data++;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cros_ec_sensors_read_cmd);

 
irqreturn_t cros_ec_sensors_capture(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	int ret;

	mutex_lock(&st->cmd_lock);

	 
	memset(st->samples, 0, indio_dev->scan_bytes);

	 
	ret = st->read_ec_sensors_data(indio_dev,
				       *(indio_dev->active_scan_mask),
				       (s16 *)st->samples);
	if (ret < 0)
		goto done;

	iio_push_to_buffers_with_timestamp(indio_dev, st->samples,
					   iio_get_time_ns(indio_dev));

done:
	 
	iio_trigger_notify_done(indio_dev->trig);

	mutex_unlock(&st->cmd_lock);

	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(cros_ec_sensors_capture);

 
int cros_ec_sensors_core_read(struct cros_ec_sensors_core_state *st,
			  struct iio_chan_spec const *chan,
			  int *val, int *val2, long mask)
{
	int ret, frequency;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		st->param.cmd = MOTIONSENSE_CMD_SENSOR_ODR;
		st->param.sensor_odr.data =
			EC_MOTION_SENSE_NO_VALUE;

		ret = cros_ec_motion_send_host_cmd(st, 0);
		if (ret)
			break;

		frequency = st->resp->sensor_odr.ret;
		*val = frequency / 1000;
		*val2 = (frequency % 1000) * 1000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(cros_ec_sensors_core_read);

 
int cros_ec_sensors_core_read_avail(struct iio_dev *indio_dev,
				    struct iio_chan_spec const *chan,
				    const int **vals,
				    int *type,
				    int *length,
				    long mask)
{
	struct cros_ec_sensors_core_state *state = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		*length = ARRAY_SIZE(state->frequencies);
		*vals = (const int *)&state->frequencies;
		*type = IIO_VAL_INT_PLUS_MICRO;
		return IIO_AVAIL_LIST;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(cros_ec_sensors_core_read_avail);

 
int cros_ec_sensors_core_write(struct cros_ec_sensors_core_state *st,
			       struct iio_chan_spec const *chan,
			       int val, int val2, long mask)
{
	int ret, frequency;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		frequency = val * 1000 + val2 / 1000;
		st->param.cmd = MOTIONSENSE_CMD_SENSOR_ODR;
		st->param.sensor_odr.data = frequency;

		 
		st->param.sensor_odr.roundup = 1;

		ret = cros_ec_motion_send_host_cmd(st, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(cros_ec_sensors_core_write);

static int __maybe_unused cros_ec_sensors_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cros_ec_sensors_core_state *st = iio_priv(indio_dev);
	int ret = 0;

	if (st->range_updated) {
		mutex_lock(&st->cmd_lock);
		st->param.cmd = MOTIONSENSE_CMD_SENSOR_RANGE;
		st->param.sensor_range.data = st->curr_range;
		st->param.sensor_range.roundup = 1;
		ret = cros_ec_motion_send_host_cmd(st, 0);
		mutex_unlock(&st->cmd_lock);
	}
	return ret;
}

SIMPLE_DEV_PM_OPS(cros_ec_sensors_pm_ops, NULL, cros_ec_sensors_resume);
EXPORT_SYMBOL_GPL(cros_ec_sensors_pm_ops);

MODULE_DESCRIPTION("ChromeOS EC sensor hub core functions");
MODULE_LICENSE("GPL v2");
