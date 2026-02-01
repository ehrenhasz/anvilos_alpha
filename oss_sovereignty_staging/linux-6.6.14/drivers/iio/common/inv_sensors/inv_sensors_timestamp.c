
 

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>

#include <linux/iio/common/inv_sensors_timestamp.h>

 
#define INV_SENSORS_TIMESTAMP_JITTER(_val, _jitter)		\
	(div_s64((_val) * (_jitter), 1000))
#define INV_SENSORS_TIMESTAMP_MIN(_val, _jitter)		\
	(((_val) * (1000 - (_jitter))) / 1000)
#define INV_SENSORS_TIMESTAMP_MAX(_val, _jitter)		\
	(((_val) * (1000 + (_jitter))) / 1000)

 
static void inv_update_acc(struct inv_sensors_timestamp_acc *acc, uint32_t val)
{
	uint64_t sum = 0;
	size_t i;

	acc->values[acc->idx++] = val;
	if (acc->idx >= ARRAY_SIZE(acc->values))
		acc->idx = 0;

	 
	for (i = 0; i < ARRAY_SIZE(acc->values); ++i) {
		if (acc->values[i] == 0)
			break;
		sum += acc->values[i];
	}

	acc->val = div_u64(sum, i);
}

void inv_sensors_timestamp_init(struct inv_sensors_timestamp *ts,
				const struct inv_sensors_timestamp_chip *chip)
{
	memset(ts, 0, sizeof(*ts));

	 
	ts->chip = *chip;
	ts->min_period = INV_SENSORS_TIMESTAMP_MIN(chip->clock_period, chip->jitter);
	ts->max_period = INV_SENSORS_TIMESTAMP_MAX(chip->clock_period, chip->jitter);

	 
	ts->mult = chip->init_period / chip->clock_period;
	ts->period = chip->init_period;

	 
	inv_update_acc(&ts->chip_period, chip->clock_period);
}
EXPORT_SYMBOL_NS_GPL(inv_sensors_timestamp_init, IIO_INV_SENSORS_TIMESTAMP);

int inv_sensors_timestamp_update_odr(struct inv_sensors_timestamp *ts,
				     uint32_t period, bool fifo)
{
	 
	if (fifo && ts->new_mult != 0)
		return -EAGAIN;

	ts->new_mult = period / ts->chip.clock_period;

	return 0;
}
EXPORT_SYMBOL_NS_GPL(inv_sensors_timestamp_update_odr, IIO_INV_SENSORS_TIMESTAMP);

static bool inv_validate_period(struct inv_sensors_timestamp *ts, uint32_t period, uint32_t mult)
{
	uint32_t period_min, period_max;

	 
	period_min = ts->min_period * mult;
	period_max = ts->max_period * mult;
	if (period > period_min && period < period_max)
		return true;
	else
		return false;
}

static bool inv_update_chip_period(struct inv_sensors_timestamp *ts,
				    uint32_t mult, uint32_t period)
{
	uint32_t new_chip_period;

	if (!inv_validate_period(ts, period, mult))
		return false;

	 
	new_chip_period = period / mult;
	inv_update_acc(&ts->chip_period, new_chip_period);
	ts->period = ts->mult * ts->chip_period.val;

	return true;
}

static void inv_align_timestamp_it(struct inv_sensors_timestamp *ts)
{
	int64_t delta, jitter;
	int64_t adjust;

	 
	delta = ts->it.lo - ts->timestamp;

	 
	jitter = INV_SENSORS_TIMESTAMP_JITTER((int64_t)ts->period, ts->chip.jitter);
	if (delta > jitter)
		adjust = jitter;
	else if (delta < -jitter)
		adjust = -jitter;
	else
		adjust = 0;

	ts->timestamp += adjust;
}

void inv_sensors_timestamp_interrupt(struct inv_sensors_timestamp *ts,
				      uint32_t fifo_period, size_t fifo_nb,
				      size_t sensor_nb, int64_t timestamp)
{
	struct inv_sensors_timestamp_interval *it;
	int64_t delta, interval;
	const uint32_t fifo_mult = fifo_period / ts->chip.clock_period;
	uint32_t period = ts->period;
	bool valid = false;

	if (fifo_nb == 0)
		return;

	 
	it = &ts->it;
	it->lo = it->up;
	it->up = timestamp;
	delta = it->up - it->lo;
	if (it->lo != 0) {
		 
		period = div_s64(delta, fifo_nb);
		valid = inv_update_chip_period(ts, fifo_mult, period);
	}

	 
	if (ts->timestamp == 0) {
		 
		interval = (int64_t)ts->period * (int64_t)sensor_nb;
		ts->timestamp = it->up - interval;
		return;
	}

	 
	if (valid)
		inv_align_timestamp_it(ts);
}
EXPORT_SYMBOL_NS_GPL(inv_sensors_timestamp_interrupt, IIO_INV_SENSORS_TIMESTAMP);

void inv_sensors_timestamp_apply_odr(struct inv_sensors_timestamp *ts,
				     uint32_t fifo_period, size_t fifo_nb,
				     unsigned int fifo_no)
{
	int64_t interval;
	uint32_t fifo_mult;

	if (ts->new_mult == 0)
		return;

	 
	ts->mult = ts->new_mult;
	ts->new_mult = 0;
	ts->period = ts->mult * ts->chip_period.val;

	 
	if (ts->timestamp != 0) {
		 
		fifo_mult = fifo_period / ts->chip.clock_period;
		fifo_period = fifo_mult * ts->chip_period.val;
		 
		interval = (int64_t)(fifo_nb - fifo_no) * (int64_t)fifo_period;
		ts->timestamp = ts->it.up - interval;
	}
}
EXPORT_SYMBOL_NS_GPL(inv_sensors_timestamp_apply_odr, IIO_INV_SENSORS_TIMESTAMP);

MODULE_AUTHOR("InvenSense, Inc.");
MODULE_DESCRIPTION("InvenSense sensors timestamp module");
MODULE_LICENSE("GPL");
