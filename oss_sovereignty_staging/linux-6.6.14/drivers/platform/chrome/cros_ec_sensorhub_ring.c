
 

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/iio/iio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_data/cros_ec_commands.h>
#include <linux/platform_data/cros_ec_proto.h>
#include <linux/platform_data/cros_ec_sensorhub.h>
#include <linux/platform_device.h>
#include <linux/sort.h>
#include <linux/slab.h>

#define CREATE_TRACE_POINTS
#include "cros_ec_sensorhub_trace.h"

 
#define M_PRECISION BIT(23)

 
#define TS_HISTORY_THRESHOLD 8

 
#define TS_HISTORY_BORED_US 500000

 
#define FUTURE_TS_ANALYTICS_COUNT_MAX 100

static inline int
cros_sensorhub_send_sample(struct cros_ec_sensorhub *sensorhub,
			   struct cros_ec_sensors_ring_sample *sample)
{
	cros_ec_sensorhub_push_data_cb_t cb;
	int id = sample->sensor_id;
	struct iio_dev *indio_dev;

	if (id >= sensorhub->sensor_num)
		return -EINVAL;

	cb = sensorhub->push_data[id].push_data_cb;
	if (!cb)
		return 0;

	indio_dev = sensorhub->push_data[id].indio_dev;

	if (sample->flag & MOTIONSENSE_SENSOR_FLAG_FLUSH)
		return 0;

	return cb(indio_dev, sample->vector, sample->timestamp);
}

 
int cros_ec_sensorhub_register_push_data(struct cros_ec_sensorhub *sensorhub,
					 u8 sensor_num,
					 struct iio_dev *indio_dev,
					 cros_ec_sensorhub_push_data_cb_t cb)
{
	if (sensor_num >= sensorhub->sensor_num)
		return -EINVAL;
	if (sensorhub->push_data[sensor_num].indio_dev)
		return -EINVAL;

	sensorhub->push_data[sensor_num].indio_dev = indio_dev;
	sensorhub->push_data[sensor_num].push_data_cb = cb;

	return 0;
}
EXPORT_SYMBOL_GPL(cros_ec_sensorhub_register_push_data);

void cros_ec_sensorhub_unregister_push_data(struct cros_ec_sensorhub *sensorhub,
					    u8 sensor_num)
{
	sensorhub->push_data[sensor_num].indio_dev = NULL;
	sensorhub->push_data[sensor_num].push_data_cb = NULL;
}
EXPORT_SYMBOL_GPL(cros_ec_sensorhub_unregister_push_data);

 
int cros_ec_sensorhub_ring_fifo_enable(struct cros_ec_sensorhub *sensorhub,
				       bool on)
{
	int ret, i;

	mutex_lock(&sensorhub->cmd_lock);
	if (sensorhub->tight_timestamps)
		for (i = 0; i < sensorhub->sensor_num; i++)
			sensorhub->batch_state[i].last_len = 0;

	sensorhub->params->cmd = MOTIONSENSE_CMD_FIFO_INT_ENABLE;
	sensorhub->params->fifo_int_enable.enable = on;

	sensorhub->msg->outsize = sizeof(struct ec_params_motion_sense);
	sensorhub->msg->insize = sizeof(struct ec_response_motion_sense);

	ret = cros_ec_cmd_xfer_status(sensorhub->ec->ec_dev, sensorhub->msg);
	mutex_unlock(&sensorhub->cmd_lock);

	 
	if (ret > 0)
		ret = 0;

	return ret;
}

static int cros_ec_sensor_ring_median_cmp(const void *pv1, const void *pv2)
{
	s64 v1 = *(s64 *)pv1;
	s64 v2 = *(s64 *)pv2;

	if (v1 > v2)
		return 1;
	else if (v1 < v2)
		return -1;
	else
		return 0;
}

 
static s64 cros_ec_sensor_ring_median(s64 *array, size_t length)
{
	sort(array, length, sizeof(s64), cros_ec_sensor_ring_median_cmp, NULL);
	return array[length / 2];
}

 

 
static void
cros_ec_sensor_ring_ts_filter_update(struct cros_ec_sensors_ts_filter_state
				     *state,
				     s64 b, s64 c)
{
	s64 x, y;
	s64 dx, dy;
	s64 m;  
	s64 *m_history_copy = state->temp_buf;
	s64 *error = state->temp_buf;
	int i;

	 
	x = b;
	 
	y = c - b * 1000;

	dx = (state->x_history[0] + state->x_offset) - x;
	if (dx == 0)
		return;  
	dy = (state->y_history[0] + state->y_offset) - y;
	m = div64_s64(dy * M_PRECISION, dx);

	 
	if (-dx > TS_HISTORY_BORED_US)
		state->history_len = 0;

	 
	for (i = state->history_len - 1; i >= 1; i--) {
		state->x_history[i] = state->x_history[i - 1] + dx;
		state->y_history[i] = state->y_history[i - 1] + dy;

		state->m_history[i] = state->m_history[i - 1];
		 
		m_history_copy[i] = state->m_history[i - 1];
	}

	 
	state->x_offset = x;
	state->y_offset = y;
	state->x_history[0] = 0;
	state->y_history[0] = 0;

	state->m_history[0] = m;
	m_history_copy[0] = m;

	if (state->history_len < CROS_EC_SENSORHUB_TS_HISTORY_SIZE)
		state->history_len++;

	 
	if (state->history_len > TS_HISTORY_THRESHOLD) {
		state->median_m =
		    cros_ec_sensor_ring_median(m_history_copy,
					       state->history_len - 1);

		 
		for (i = 0; i < state->history_len; i++)
			error[i] = state->y_history[i] -
				div_s64(state->median_m * state->x_history[i],
					M_PRECISION);
		state->median_error =
			cros_ec_sensor_ring_median(error, state->history_len);
	} else {
		state->median_m = 0;
		state->median_error = 0;
	}
	trace_cros_ec_sensorhub_filter(state, dx, dy);
}

 
static s64
cros_ec_sensor_ring_ts_filter(struct cros_ec_sensors_ts_filter_state *state,
			      s64 x)
{
	return div_s64(state->median_m * (x - state->x_offset), M_PRECISION)
	       + state->median_error + state->y_offset + x * 1000;
}

 
static void
cros_ec_sensor_ring_fix_overflow(s64 *ts,
				 const s64 overflow_period,
				 struct cros_ec_sensors_ec_overflow_state
				 *state)
{
	s64 adjust;

	*ts += state->offset;
	if (abs(state->last - *ts) > (overflow_period / 2)) {
		adjust = state->last > *ts ? overflow_period : -overflow_period;
		state->offset += adjust;
		*ts += adjust;
	}
	state->last = *ts;
}

static void
cros_ec_sensor_ring_check_for_past_timestamp(struct cros_ec_sensorhub
					     *sensorhub,
					     struct cros_ec_sensors_ring_sample
					     *sample)
{
	const u8 sensor_id = sample->sensor_id;

	 
	if (sensorhub->batch_state[sensor_id].newest_sensor_event >
	    sample->timestamp)
		 
		sample->timestamp =
			sensorhub->batch_state[sensor_id].last_ts;
	else
		sensorhub->batch_state[sensor_id].newest_sensor_event =
			sample->timestamp;
}

 
static bool
cros_ec_sensor_ring_process_event(struct cros_ec_sensorhub *sensorhub,
				const struct ec_response_motion_sense_fifo_info
				*fifo_info,
				const ktime_t fifo_timestamp,
				ktime_t *current_timestamp,
				struct ec_response_motion_sensor_data *in,
				struct cros_ec_sensors_ring_sample *out)
{
	const s64 now = cros_ec_get_time_ns();
	int axis, async_flags;

	 
	async_flags = in->flags &
		(MOTIONSENSE_SENSOR_FLAG_ODR | MOTIONSENSE_SENSOR_FLAG_FLUSH);

	if (in->flags & MOTIONSENSE_SENSOR_FLAG_TIMESTAMP && !async_flags) {
		s64 a = in->timestamp;
		s64 b = fifo_info->timestamp;
		s64 c = fifo_timestamp;

		cros_ec_sensor_ring_fix_overflow(&a, 1LL << 32,
					  &sensorhub->overflow_a);
		cros_ec_sensor_ring_fix_overflow(&b, 1LL << 32,
					  &sensorhub->overflow_b);

		if (sensorhub->tight_timestamps) {
			cros_ec_sensor_ring_ts_filter_update(
					&sensorhub->filter, b, c);
			*current_timestamp = cros_ec_sensor_ring_ts_filter(
					&sensorhub->filter, a);
		} else {
			s64 new_timestamp;

			 
			new_timestamp = c - b * 1000 + a * 1000;
			 
			if (new_timestamp - *current_timestamp > 0)
				*current_timestamp = new_timestamp;
		}
		trace_cros_ec_sensorhub_timestamp(in->timestamp,
						  fifo_info->timestamp,
						  fifo_timestamp,
						  *current_timestamp,
						  now);
	}

	if (in->flags & MOTIONSENSE_SENSOR_FLAG_ODR) {
		if (sensorhub->tight_timestamps) {
			sensorhub->batch_state[in->sensor_num].last_len = 0;
			sensorhub->batch_state[in->sensor_num].penul_len = 0;
		}
		 
		return false;
	}

	if (in->flags & MOTIONSENSE_SENSOR_FLAG_FLUSH) {
		out->sensor_id = in->sensor_num;
		out->timestamp = *current_timestamp;
		out->flag = in->flags;
		if (sensorhub->tight_timestamps)
			sensorhub->batch_state[out->sensor_id].last_len = 0;
		 
		return true;
	}

	if (in->flags & MOTIONSENSE_SENSOR_FLAG_TIMESTAMP)
		 
		return false;

	 
	out->sensor_id = in->sensor_num;
	trace_cros_ec_sensorhub_data(in->sensor_num,
				     fifo_info->timestamp,
				     fifo_timestamp,
				     *current_timestamp,
				     now);

	if (*current_timestamp - now > 0) {
		 
		sensorhub->future_timestamp_total_ns +=
			*current_timestamp - now;
		if (++sensorhub->future_timestamp_count ==
				FUTURE_TS_ANALYTICS_COUNT_MAX) {
			s64 avg = div_s64(sensorhub->future_timestamp_total_ns,
					sensorhub->future_timestamp_count);
			dev_warn_ratelimited(sensorhub->dev,
					     "100 timestamps in the future, %lldns shaved on average\n",
					     avg);
			sensorhub->future_timestamp_count = 0;
			sensorhub->future_timestamp_total_ns = 0;
		}
		out->timestamp = now;
	} else {
		out->timestamp = *current_timestamp;
	}

	out->flag = in->flags;
	for (axis = 0; axis < 3; axis++)
		out->vector[axis] = in->data[axis];

	if (sensorhub->tight_timestamps)
		cros_ec_sensor_ring_check_for_past_timestamp(sensorhub, out);
	return true;
}

 
static void
cros_ec_sensor_ring_spread_add(struct cros_ec_sensorhub *sensorhub,
			       unsigned long sensor_mask,
			       struct cros_ec_sensors_ring_sample *last_out)
{
	struct cros_ec_sensors_ring_sample *batch_start, *next_batch_start;
	int id;

	for_each_set_bit(id, &sensor_mask, sensorhub->sensor_num) {
		for (batch_start = sensorhub->ring; batch_start < last_out;
		     batch_start = next_batch_start) {
			 
			int batch_len, sample_idx;
			struct cros_ec_sensors_ring_sample *batch_end =
				batch_start;
			struct cros_ec_sensors_ring_sample *s;
			s64 batch_timestamp = batch_start->timestamp;
			s64 sample_period;

			 
			if (batch_start->sensor_id != id) {
				next_batch_start = batch_start + 1;
				continue;
			}

			 
			if (batch_start->flag & MOTIONSENSE_SENSOR_FLAG_FLUSH) {
				cros_sensorhub_send_sample(sensorhub,
							   batch_start);
				next_batch_start = batch_start + 1;
				continue;
			}

			if (batch_start->timestamp <=
			    sensorhub->batch_state[id].last_ts) {
				batch_timestamp =
					sensorhub->batch_state[id].last_ts;
				batch_len = sensorhub->batch_state[id].last_len;

				sample_idx = batch_len;

				sensorhub->batch_state[id].last_ts =
				  sensorhub->batch_state[id].penul_ts;
				sensorhub->batch_state[id].last_len =
				  sensorhub->batch_state[id].penul_len;
			} else {
				 
				sample_idx = 1;
				batch_len = 1;
				cros_sensorhub_send_sample(sensorhub,
							   batch_start);
				batch_start++;
			}

			 
			for (s = batch_start; s < last_out; s++) {
				if (s->sensor_id != id)
					 
					continue;
				if (s->timestamp != batch_timestamp)
					 
					break;
				if (s->flag & MOTIONSENSE_SENSOR_FLAG_FLUSH)
					 
					break;
				batch_end = s;
				batch_len++;
			}

			if (batch_len == 1)
				goto done_with_this_batch;

			 
			if (sensorhub->batch_state[id].last_len == 0) {
				dev_warn(sensorhub->dev, "Sensor %d: lost %d samples when spreading\n",
					 id, batch_len - 1);
				goto done_with_this_batch;
				 
			}

			sample_period = div_s64(batch_timestamp -
				sensorhub->batch_state[id].last_ts,
				sensorhub->batch_state[id].last_len);
			dev_dbg(sensorhub->dev,
				"Adjusting %d samples, sensor %d last_batch @%lld (%d samples) batch_timestamp=%lld => period=%lld\n",
				batch_len, id,
				sensorhub->batch_state[id].last_ts,
				sensorhub->batch_state[id].last_len,
				batch_timestamp,
				sample_period);

			 
			for (s = batch_start; s <= batch_end; s++) {
				if (s->sensor_id != id)
					 
					continue;

				s->timestamp = batch_timestamp +
					sample_period * sample_idx;
				sample_idx++;

				cros_sensorhub_send_sample(sensorhub, s);
			}

done_with_this_batch:
			sensorhub->batch_state[id].penul_ts =
				sensorhub->batch_state[id].last_ts;
			sensorhub->batch_state[id].penul_len =
				sensorhub->batch_state[id].last_len;

			sensorhub->batch_state[id].last_ts =
				batch_timestamp;
			sensorhub->batch_state[id].last_len = batch_len;

			next_batch_start = batch_end + 1;
		}
	}
}

 
static void
cros_ec_sensor_ring_spread_add_legacy(struct cros_ec_sensorhub *sensorhub,
				      unsigned long sensor_mask,
				      s64 current_timestamp,
				      struct cros_ec_sensors_ring_sample
				      *last_out)
{
	struct cros_ec_sensors_ring_sample *out;
	int i;

	for_each_set_bit(i, &sensor_mask, sensorhub->sensor_num) {
		s64 timestamp;
		int count = 0;
		s64 time_period;

		for (out = sensorhub->ring; out < last_out; out++) {
			if (out->sensor_id != i)
				continue;

			 
			timestamp = out->timestamp;
			out++;
			count = 1;
			break;
		}
		for (; out < last_out; out++) {
			 
			if (out->sensor_id != i)
				continue;
			count++;
		}
		if (count == 0)
			continue;

		 
		time_period = div_s64(current_timestamp - timestamp, count);

		for (out = sensorhub->ring; out < last_out; out++) {
			if (out->sensor_id != i)
				continue;
			timestamp += time_period;
			out->timestamp = timestamp;
		}
	}

	 
	for (out = sensorhub->ring; out < last_out; out++)
		cros_sensorhub_send_sample(sensorhub, out);
}

 
static void cros_ec_sensorhub_ring_handler(struct cros_ec_sensorhub *sensorhub)
{
	struct ec_response_motion_sense_fifo_info *fifo_info =
		sensorhub->fifo_info;
	struct cros_ec_dev *ec = sensorhub->ec;
	ktime_t fifo_timestamp, current_timestamp;
	int i, j, number_data, ret;
	unsigned long sensor_mask = 0;
	struct ec_response_motion_sensor_data *in;
	struct cros_ec_sensors_ring_sample *out, *last_out;

	mutex_lock(&sensorhub->cmd_lock);

	 
	if (fifo_info->total_lost) {
		int fifo_info_length =
			sizeof(struct ec_response_motion_sense_fifo_info) +
			sizeof(u16) * sensorhub->sensor_num;

		 
		sensorhub->params->cmd = MOTIONSENSE_CMD_FIFO_INFO;
		sensorhub->msg->outsize = 1;
		sensorhub->msg->insize = fifo_info_length;

		if (cros_ec_cmd_xfer_status(ec->ec_dev, sensorhub->msg) < 0)
			goto error;

		memcpy(fifo_info, &sensorhub->resp->fifo_info,
		       fifo_info_length);

		 
		fifo_timestamp = cros_ec_get_time_ns();
	} else {
		fifo_timestamp = sensorhub->fifo_timestamp[
			CROS_EC_SENSOR_NEW_TS];
	}

	if (fifo_info->count > sensorhub->fifo_size ||
	    fifo_info->size != sensorhub->fifo_size) {
		dev_warn(sensorhub->dev,
			 "Mismatch EC data: count %d, size %d - expected %d\n",
			 fifo_info->count, fifo_info->size,
			 sensorhub->fifo_size);
		goto error;
	}

	 
	current_timestamp = sensorhub->fifo_timestamp[CROS_EC_SENSOR_LAST_TS];
	out = sensorhub->ring;
	for (i = 0; i < fifo_info->count; i += number_data) {
		sensorhub->params->cmd = MOTIONSENSE_CMD_FIFO_READ;
		sensorhub->params->fifo_read.max_data_vector =
			fifo_info->count - i;
		sensorhub->msg->outsize =
			sizeof(struct ec_params_motion_sense);
		sensorhub->msg->insize =
			sizeof(sensorhub->resp->fifo_read) +
			sensorhub->params->fifo_read.max_data_vector *
			  sizeof(struct ec_response_motion_sensor_data);
		ret = cros_ec_cmd_xfer_status(ec->ec_dev, sensorhub->msg);
		if (ret < 0) {
			dev_warn(sensorhub->dev, "Fifo error: %d\n", ret);
			break;
		}
		number_data = sensorhub->resp->fifo_read.number_data;
		if (number_data == 0) {
			dev_dbg(sensorhub->dev, "Unexpected empty FIFO\n");
			break;
		}
		if (number_data > fifo_info->count - i) {
			dev_warn(sensorhub->dev,
				 "Invalid EC data: too many entry received: %d, expected %d\n",
				 number_data, fifo_info->count - i);
			break;
		}
		if (out + number_data >
		    sensorhub->ring + fifo_info->count) {
			dev_warn(sensorhub->dev,
				 "Too many samples: %d (%zd data) to %d entries for expected %d entries\n",
				 i, out - sensorhub->ring, i + number_data,
				 fifo_info->count);
			break;
		}

		for (in = sensorhub->resp->fifo_read.data, j = 0;
		     j < number_data; j++, in++) {
			if (cros_ec_sensor_ring_process_event(
						sensorhub, fifo_info,
						fifo_timestamp,
						&current_timestamp,
						in, out)) {
				sensor_mask |= BIT(in->sensor_num);
				out++;
			}
		}
	}
	mutex_unlock(&sensorhub->cmd_lock);
	last_out = out;

	if (out == sensorhub->ring)
		 
		goto ring_handler_end;

	 
	if (!sensorhub->tight_timestamps &&
	    (last_out - 1)->timestamp == current_timestamp)
		current_timestamp = fifo_timestamp;

	 
	if (fifo_info->total_lost)
		for (i = 0; i < sensorhub->sensor_num; i++) {
			if (fifo_info->lost[i]) {
				dev_warn_ratelimited(sensorhub->dev,
						     "Sensor %d: lost: %d out of %d\n",
						     i, fifo_info->lost[i],
						     fifo_info->total_lost);
				if (sensorhub->tight_timestamps)
					sensorhub->batch_state[i].last_len = 0;
			}
		}

	 
	if (sensorhub->tight_timestamps)
		cros_ec_sensor_ring_spread_add(sensorhub, sensor_mask,
					       last_out);
	else
		cros_ec_sensor_ring_spread_add_legacy(sensorhub, sensor_mask,
						      current_timestamp,
						      last_out);

ring_handler_end:
	sensorhub->fifo_timestamp[CROS_EC_SENSOR_LAST_TS] = current_timestamp;
	return;

error:
	mutex_unlock(&sensorhub->cmd_lock);
}

static int cros_ec_sensorhub_event(struct notifier_block *nb,
				   unsigned long queued_during_suspend,
				   void *_notify)
{
	struct cros_ec_sensorhub *sensorhub;
	struct cros_ec_device *ec_dev;

	sensorhub = container_of(nb, struct cros_ec_sensorhub, notifier);
	ec_dev = sensorhub->ec->ec_dev;

	if (ec_dev->event_data.event_type != EC_MKBP_EVENT_SENSOR_FIFO)
		return NOTIFY_DONE;

	if (ec_dev->event_size != sizeof(ec_dev->event_data.data.sensor_fifo)) {
		dev_warn(ec_dev->dev, "Invalid fifo info size\n");
		return NOTIFY_DONE;
	}

	if (queued_during_suspend)
		return NOTIFY_OK;

	memcpy(sensorhub->fifo_info, &ec_dev->event_data.data.sensor_fifo.info,
	       sizeof(*sensorhub->fifo_info));
	sensorhub->fifo_timestamp[CROS_EC_SENSOR_NEW_TS] =
		ec_dev->last_event_time;
	cros_ec_sensorhub_ring_handler(sensorhub);

	return NOTIFY_OK;
}

 
int cros_ec_sensorhub_ring_allocate(struct cros_ec_sensorhub *sensorhub)
{
	int fifo_info_length =
		sizeof(struct ec_response_motion_sense_fifo_info) +
		sizeof(u16) * sensorhub->sensor_num;

	 
	sensorhub->fifo_info = devm_kzalloc(sensorhub->dev, fifo_info_length,
					    GFP_KERNEL);
	if (!sensorhub->fifo_info)
		return -ENOMEM;

	 
	sensorhub->push_data = devm_kcalloc(sensorhub->dev,
			sensorhub->sensor_num,
			sizeof(*sensorhub->push_data),
			GFP_KERNEL);
	if (!sensorhub->push_data)
		return -ENOMEM;

	sensorhub->tight_timestamps = cros_ec_check_features(
			sensorhub->ec,
			EC_FEATURE_MOTION_SENSE_TIGHT_TIMESTAMPS);

	if (sensorhub->tight_timestamps) {
		sensorhub->batch_state = devm_kcalloc(sensorhub->dev,
				sensorhub->sensor_num,
				sizeof(*sensorhub->batch_state),
				GFP_KERNEL);
		if (!sensorhub->batch_state)
			return -ENOMEM;
	}

	return 0;
}

 
int cros_ec_sensorhub_ring_add(struct cros_ec_sensorhub *sensorhub)
{
	struct cros_ec_dev *ec = sensorhub->ec;
	int ret;
	int fifo_info_length =
		sizeof(struct ec_response_motion_sense_fifo_info) +
		sizeof(u16) * sensorhub->sensor_num;

	 
	sensorhub->msg->version = 2;
	sensorhub->params->cmd = MOTIONSENSE_CMD_FIFO_INFO;
	sensorhub->msg->outsize = 1;
	sensorhub->msg->insize = fifo_info_length;

	ret = cros_ec_cmd_xfer_status(ec->ec_dev, sensorhub->msg);
	if (ret < 0)
		return ret;

	 
	sensorhub->fifo_size = sensorhub->resp->fifo_info.size;
	sensorhub->ring = devm_kcalloc(sensorhub->dev, sensorhub->fifo_size,
				       sizeof(*sensorhub->ring), GFP_KERNEL);
	if (!sensorhub->ring)
		return -ENOMEM;

	sensorhub->fifo_timestamp[CROS_EC_SENSOR_LAST_TS] =
		cros_ec_get_time_ns();

	 
	sensorhub->notifier.notifier_call = cros_ec_sensorhub_event;
	ret = blocking_notifier_chain_register(&ec->ec_dev->event_notifier,
					       &sensorhub->notifier);
	if (ret < 0)
		return ret;

	 
	return cros_ec_sensorhub_ring_fifo_enable(sensorhub, true);
}

void cros_ec_sensorhub_ring_remove(void *arg)
{
	struct cros_ec_sensorhub *sensorhub = arg;
	struct cros_ec_device *ec_dev = sensorhub->ec->ec_dev;

	 
	cros_ec_sensorhub_ring_fifo_enable(sensorhub, false);
	blocking_notifier_chain_unregister(&ec_dev->event_notifier,
					   &sensorhub->notifier);
}
