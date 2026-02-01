
 

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/delay.h>

#include <linux/iio/buffer.h>
#include <linux/iio/common/inv_sensors_timestamp.h>
#include <linux/iio/iio.h>

#include "inv_icm42600.h"
#include "inv_icm42600_buffer.h"

 
#define INV_ICM42600_FIFO_HEADER_MSG		BIT(7)
#define INV_ICM42600_FIFO_HEADER_ACCEL		BIT(6)
#define INV_ICM42600_FIFO_HEADER_GYRO		BIT(5)
#define INV_ICM42600_FIFO_HEADER_TMST_FSYNC	GENMASK(3, 2)
#define INV_ICM42600_FIFO_HEADER_ODR_ACCEL	BIT(1)
#define INV_ICM42600_FIFO_HEADER_ODR_GYRO	BIT(0)

struct inv_icm42600_fifo_1sensor_packet {
	uint8_t header;
	struct inv_icm42600_fifo_sensor_data data;
	int8_t temp;
} __packed;
#define INV_ICM42600_FIFO_1SENSOR_PACKET_SIZE		8

struct inv_icm42600_fifo_2sensors_packet {
	uint8_t header;
	struct inv_icm42600_fifo_sensor_data accel;
	struct inv_icm42600_fifo_sensor_data gyro;
	int8_t temp;
	__be16 timestamp;
} __packed;
#define INV_ICM42600_FIFO_2SENSORS_PACKET_SIZE		16

ssize_t inv_icm42600_fifo_decode_packet(const void *packet, const void **accel,
					const void **gyro, const int8_t **temp,
					const void **timestamp, unsigned int *odr)
{
	const struct inv_icm42600_fifo_1sensor_packet *pack1 = packet;
	const struct inv_icm42600_fifo_2sensors_packet *pack2 = packet;
	uint8_t header = *((const uint8_t *)packet);

	 
	if (header & INV_ICM42600_FIFO_HEADER_MSG) {
		*accel = NULL;
		*gyro = NULL;
		*temp = NULL;
		*timestamp = NULL;
		*odr = 0;
		return 0;
	}

	 
	*odr = 0;
	if (header & INV_ICM42600_FIFO_HEADER_ODR_GYRO)
		*odr |= INV_ICM42600_SENSOR_GYRO;
	if (header & INV_ICM42600_FIFO_HEADER_ODR_ACCEL)
		*odr |= INV_ICM42600_SENSOR_ACCEL;

	 
	if ((header & INV_ICM42600_FIFO_HEADER_ACCEL) &&
	    (header & INV_ICM42600_FIFO_HEADER_GYRO)) {
		*accel = &pack2->accel;
		*gyro = &pack2->gyro;
		*temp = &pack2->temp;
		*timestamp = &pack2->timestamp;
		return INV_ICM42600_FIFO_2SENSORS_PACKET_SIZE;
	}

	 
	if (header & INV_ICM42600_FIFO_HEADER_ACCEL) {
		*accel = &pack1->data;
		*gyro = NULL;
		*temp = &pack1->temp;
		*timestamp = NULL;
		return INV_ICM42600_FIFO_1SENSOR_PACKET_SIZE;
	}

	 
	if (header & INV_ICM42600_FIFO_HEADER_GYRO) {
		*accel = NULL;
		*gyro = &pack1->data;
		*temp = &pack1->temp;
		*timestamp = NULL;
		return INV_ICM42600_FIFO_1SENSOR_PACKET_SIZE;
	}

	 
	return -EINVAL;
}

void inv_icm42600_buffer_update_fifo_period(struct inv_icm42600_state *st)
{
	uint32_t period_gyro, period_accel, period;

	if (st->fifo.en & INV_ICM42600_SENSOR_GYRO)
		period_gyro = inv_icm42600_odr_to_period(st->conf.gyro.odr);
	else
		period_gyro = U32_MAX;

	if (st->fifo.en & INV_ICM42600_SENSOR_ACCEL)
		period_accel = inv_icm42600_odr_to_period(st->conf.accel.odr);
	else
		period_accel = U32_MAX;

	if (period_gyro <= period_accel)
		period = period_gyro;
	else
		period = period_accel;

	st->fifo.period = period;
}

int inv_icm42600_buffer_set_fifo_en(struct inv_icm42600_state *st,
				    unsigned int fifo_en)
{
	unsigned int mask, val;
	int ret;

	 
	mask = INV_ICM42600_FIFO_CONFIG1_TMST_FSYNC_EN |
		INV_ICM42600_FIFO_CONFIG1_TEMP_EN |
		INV_ICM42600_FIFO_CONFIG1_GYRO_EN |
		INV_ICM42600_FIFO_CONFIG1_ACCEL_EN;

	val = 0;
	if (fifo_en & INV_ICM42600_SENSOR_GYRO)
		val |= INV_ICM42600_FIFO_CONFIG1_GYRO_EN;
	if (fifo_en & INV_ICM42600_SENSOR_ACCEL)
		val |= INV_ICM42600_FIFO_CONFIG1_ACCEL_EN;
	if (fifo_en & INV_ICM42600_SENSOR_TEMP)
		val |= INV_ICM42600_FIFO_CONFIG1_TEMP_EN;

	ret = regmap_update_bits(st->map, INV_ICM42600_REG_FIFO_CONFIG1, mask, val);
	if (ret)
		return ret;

	st->fifo.en = fifo_en;
	inv_icm42600_buffer_update_fifo_period(st);

	return 0;
}

static size_t inv_icm42600_get_packet_size(unsigned int fifo_en)
{
	size_t packet_size;

	if ((fifo_en & INV_ICM42600_SENSOR_GYRO) &&
	    (fifo_en & INV_ICM42600_SENSOR_ACCEL))
		packet_size = INV_ICM42600_FIFO_2SENSORS_PACKET_SIZE;
	else
		packet_size = INV_ICM42600_FIFO_1SENSOR_PACKET_SIZE;

	return packet_size;
}

static unsigned int inv_icm42600_wm_truncate(unsigned int watermark,
					     size_t packet_size)
{
	size_t wm_size;
	unsigned int wm;

	wm_size = watermark * packet_size;
	if (wm_size > INV_ICM42600_FIFO_WATERMARK_MAX)
		wm_size = INV_ICM42600_FIFO_WATERMARK_MAX;

	wm = wm_size / packet_size;

	return wm;
}

 
int inv_icm42600_buffer_update_watermark(struct inv_icm42600_state *st)
{
	size_t packet_size, wm_size;
	unsigned int wm_gyro, wm_accel, watermark;
	uint32_t period_gyro, period_accel, period;
	uint32_t latency_gyro, latency_accel, latency;
	bool restore;
	__le16 raw_wm;
	int ret;

	packet_size = inv_icm42600_get_packet_size(st->fifo.en);

	 
	wm_gyro = inv_icm42600_wm_truncate(st->fifo.watermark.gyro, packet_size);
	wm_accel = inv_icm42600_wm_truncate(st->fifo.watermark.accel, packet_size);
	 
	period_gyro = inv_icm42600_odr_to_period(st->conf.gyro.odr) / 1000UL;
	period_accel = inv_icm42600_odr_to_period(st->conf.accel.odr) / 1000UL;
	latency_gyro = period_gyro * wm_gyro;
	latency_accel = period_accel * wm_accel;

	 
	if (latency_gyro == 0) {
		watermark = wm_accel;
	} else if (latency_accel == 0) {
		watermark = wm_gyro;
	} else {
		 
		if (latency_gyro <= latency_accel)
			latency = latency_gyro - (latency_accel % latency_gyro);
		else
			latency = latency_accel - (latency_gyro % latency_accel);
		 
		if (period_gyro <= period_accel)
			period = period_gyro;
		else
			period = period_accel;
		 
		watermark = latency / period;
		if (watermark < 1)
			watermark = 1;
	}

	 
	wm_size = watermark * packet_size;

	 
	ret = regmap_update_bits_check(st->map, INV_ICM42600_REG_INT_SOURCE0,
				       INV_ICM42600_INT_SOURCE0_FIFO_THS_INT1_EN,
				       0, &restore);
	if (ret)
		return ret;

	raw_wm = INV_ICM42600_FIFO_WATERMARK_VAL(wm_size);
	memcpy(st->buffer, &raw_wm, sizeof(raw_wm));
	ret = regmap_bulk_write(st->map, INV_ICM42600_REG_FIFO_WATERMARK,
				st->buffer, sizeof(raw_wm));
	if (ret)
		return ret;

	 
	if (restore) {
		ret = regmap_update_bits(st->map, INV_ICM42600_REG_INT_SOURCE0,
					 INV_ICM42600_INT_SOURCE0_FIFO_THS_INT1_EN,
					 INV_ICM42600_INT_SOURCE0_FIFO_THS_INT1_EN);
		if (ret)
			return ret;
	}

	return 0;
}

static int inv_icm42600_buffer_preenable(struct iio_dev *indio_dev)
{
	struct inv_icm42600_state *st = iio_device_get_drvdata(indio_dev);
	struct device *dev = regmap_get_device(st->map);
	struct inv_sensors_timestamp *ts = iio_priv(indio_dev);

	pm_runtime_get_sync(dev);

	mutex_lock(&st->lock);
	inv_sensors_timestamp_reset(ts);
	mutex_unlock(&st->lock);

	return 0;
}

 
static int inv_icm42600_buffer_postenable(struct iio_dev *indio_dev)
{
	struct inv_icm42600_state *st = iio_device_get_drvdata(indio_dev);
	int ret;

	mutex_lock(&st->lock);

	 
	if (st->fifo.on) {
		ret = 0;
		goto out_on;
	}

	 
	ret = regmap_update_bits(st->map, INV_ICM42600_REG_INT_SOURCE0,
				 INV_ICM42600_INT_SOURCE0_FIFO_THS_INT1_EN,
				 INV_ICM42600_INT_SOURCE0_FIFO_THS_INT1_EN);
	if (ret)
		goto out_unlock;

	 
	ret = regmap_write(st->map, INV_ICM42600_REG_SIGNAL_PATH_RESET,
			   INV_ICM42600_SIGNAL_PATH_RESET_FIFO_FLUSH);
	if (ret)
		goto out_unlock;

	 
	ret = regmap_write(st->map, INV_ICM42600_REG_FIFO_CONFIG,
			   INV_ICM42600_FIFO_CONFIG_STREAM);
	if (ret)
		goto out_unlock;

	 
	ret = regmap_bulk_read(st->map, INV_ICM42600_REG_FIFO_COUNT, st->buffer, 2);
	if (ret)
		goto out_unlock;

out_on:
	 
	st->fifo.on++;
out_unlock:
	mutex_unlock(&st->lock);
	return ret;
}

static int inv_icm42600_buffer_predisable(struct iio_dev *indio_dev)
{
	struct inv_icm42600_state *st = iio_device_get_drvdata(indio_dev);
	int ret;

	mutex_lock(&st->lock);

	 
	if (st->fifo.on > 1) {
		ret = 0;
		goto out_off;
	}

	 
	ret = regmap_write(st->map, INV_ICM42600_REG_FIFO_CONFIG,
			   INV_ICM42600_FIFO_CONFIG_BYPASS);
	if (ret)
		goto out_unlock;

	 
	ret = regmap_write(st->map, INV_ICM42600_REG_SIGNAL_PATH_RESET,
			   INV_ICM42600_SIGNAL_PATH_RESET_FIFO_FLUSH);
	if (ret)
		goto out_unlock;

	 
	ret = regmap_update_bits(st->map, INV_ICM42600_REG_INT_SOURCE0,
				 INV_ICM42600_INT_SOURCE0_FIFO_THS_INT1_EN, 0);
	if (ret)
		goto out_unlock;

out_off:
	 
	st->fifo.on--;
out_unlock:
	mutex_unlock(&st->lock);
	return ret;
}

static int inv_icm42600_buffer_postdisable(struct iio_dev *indio_dev)
{
	struct inv_icm42600_state *st = iio_device_get_drvdata(indio_dev);
	struct device *dev = regmap_get_device(st->map);
	unsigned int sensor;
	unsigned int *watermark;
	struct inv_icm42600_sensor_conf conf = INV_ICM42600_SENSOR_CONF_INIT;
	unsigned int sleep_temp = 0;
	unsigned int sleep_sensor = 0;
	unsigned int sleep;
	int ret;

	if (indio_dev == st->indio_gyro) {
		sensor = INV_ICM42600_SENSOR_GYRO;
		watermark = &st->fifo.watermark.gyro;
	} else if (indio_dev == st->indio_accel) {
		sensor = INV_ICM42600_SENSOR_ACCEL;
		watermark = &st->fifo.watermark.accel;
	} else {
		return -EINVAL;
	}

	mutex_lock(&st->lock);

	ret = inv_icm42600_buffer_set_fifo_en(st, st->fifo.en & ~sensor);
	if (ret)
		goto out_unlock;

	*watermark = 0;
	ret = inv_icm42600_buffer_update_watermark(st);
	if (ret)
		goto out_unlock;

	conf.mode = INV_ICM42600_SENSOR_MODE_OFF;
	if (sensor == INV_ICM42600_SENSOR_GYRO)
		ret = inv_icm42600_set_gyro_conf(st, &conf, &sleep_sensor);
	else
		ret = inv_icm42600_set_accel_conf(st, &conf, &sleep_sensor);
	if (ret)
		goto out_unlock;

	 
	if (!st->fifo.on)
		ret = inv_icm42600_set_temp_conf(st, false, &sleep_temp);

out_unlock:
	mutex_unlock(&st->lock);

	 
	if (sleep_sensor > sleep_temp)
		sleep = sleep_sensor;
	else
		sleep = sleep_temp;
	if (sleep)
		msleep(sleep);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return ret;
}

const struct iio_buffer_setup_ops inv_icm42600_buffer_ops = {
	.preenable = inv_icm42600_buffer_preenable,
	.postenable = inv_icm42600_buffer_postenable,
	.predisable = inv_icm42600_buffer_predisable,
	.postdisable = inv_icm42600_buffer_postdisable,
};

int inv_icm42600_buffer_fifo_read(struct inv_icm42600_state *st,
				  unsigned int max)
{
	size_t max_count;
	__be16 *raw_fifo_count;
	ssize_t i, size;
	const void *accel, *gyro, *timestamp;
	const int8_t *temp;
	unsigned int odr;
	int ret;

	 
	st->fifo.count = 0;
	st->fifo.nb.gyro = 0;
	st->fifo.nb.accel = 0;
	st->fifo.nb.total = 0;

	 
	if (max == 0)
		max_count = sizeof(st->fifo.data);
	else
		max_count = max * inv_icm42600_get_packet_size(st->fifo.en);

	 
	raw_fifo_count = (__be16 *)st->buffer;
	ret = regmap_bulk_read(st->map, INV_ICM42600_REG_FIFO_COUNT,
			       raw_fifo_count, sizeof(*raw_fifo_count));
	if (ret)
		return ret;
	st->fifo.count = be16_to_cpup(raw_fifo_count);

	 
	if (st->fifo.count == 0)
		return 0;
	if (st->fifo.count > max_count)
		st->fifo.count = max_count;

	 
	ret = regmap_noinc_read(st->map, INV_ICM42600_REG_FIFO_DATA,
				st->fifo.data, st->fifo.count);
	if (ret)
		return ret;

	 
	for (i = 0; i < st->fifo.count; i += size) {
		size = inv_icm42600_fifo_decode_packet(&st->fifo.data[i],
				&accel, &gyro, &temp, &timestamp, &odr);
		if (size <= 0)
			break;
		if (gyro != NULL && inv_icm42600_fifo_is_data_valid(gyro))
			st->fifo.nb.gyro++;
		if (accel != NULL && inv_icm42600_fifo_is_data_valid(accel))
			st->fifo.nb.accel++;
		st->fifo.nb.total++;
	}

	return 0;
}

int inv_icm42600_buffer_fifo_parse(struct inv_icm42600_state *st)
{
	struct inv_sensors_timestamp *ts;
	int ret;

	if (st->fifo.nb.total == 0)
		return 0;

	 
	ts = iio_priv(st->indio_gyro);
	inv_sensors_timestamp_interrupt(ts, st->fifo.period, st->fifo.nb.total,
					st->fifo.nb.gyro, st->timestamp.gyro);
	if (st->fifo.nb.gyro > 0) {
		ret = inv_icm42600_gyro_parse_fifo(st->indio_gyro);
		if (ret)
			return ret;
	}

	 
	ts = iio_priv(st->indio_accel);
	inv_sensors_timestamp_interrupt(ts, st->fifo.period, st->fifo.nb.total,
					st->fifo.nb.accel, st->timestamp.accel);
	if (st->fifo.nb.accel > 0) {
		ret = inv_icm42600_accel_parse_fifo(st->indio_accel);
		if (ret)
			return ret;
	}

	return 0;
}

int inv_icm42600_buffer_hwfifo_flush(struct inv_icm42600_state *st,
				     unsigned int count)
{
	struct inv_sensors_timestamp *ts;
	int64_t gyro_ts, accel_ts;
	int ret;

	gyro_ts = iio_get_time_ns(st->indio_gyro);
	accel_ts = iio_get_time_ns(st->indio_accel);

	ret = inv_icm42600_buffer_fifo_read(st, count);
	if (ret)
		return ret;

	if (st->fifo.nb.total == 0)
		return 0;

	if (st->fifo.nb.gyro > 0) {
		ts = iio_priv(st->indio_gyro);
		inv_sensors_timestamp_interrupt(ts, st->fifo.period,
						st->fifo.nb.total, st->fifo.nb.gyro,
						gyro_ts);
		ret = inv_icm42600_gyro_parse_fifo(st->indio_gyro);
		if (ret)
			return ret;
	}

	if (st->fifo.nb.accel > 0) {
		ts = iio_priv(st->indio_accel);
		inv_sensors_timestamp_interrupt(ts, st->fifo.period,
						st->fifo.nb.total, st->fifo.nb.accel,
						accel_ts);
		ret = inv_icm42600_accel_parse_fifo(st->indio_accel);
		if (ret)
			return ret;
	}

	return 0;
}

int inv_icm42600_buffer_init(struct inv_icm42600_state *st)
{
	unsigned int val;
	int ret;

	 
	val = INV_ICM42600_INTF_CONFIG0_FIFO_COUNT_ENDIAN;
	ret = regmap_update_bits(st->map, INV_ICM42600_REG_INTF_CONFIG0,
				 GENMASK(7, 5), val);
	if (ret)
		return ret;

	 
	val = INV_ICM42600_FIFO_CONFIG1_RESUME_PARTIAL_RD |
	      INV_ICM42600_FIFO_CONFIG1_WM_GT_TH;
	return regmap_update_bits(st->map, INV_ICM42600_REG_FIFO_CONFIG1,
				  GENMASK(6, 5) | GENMASK(3, 0), val);
}
