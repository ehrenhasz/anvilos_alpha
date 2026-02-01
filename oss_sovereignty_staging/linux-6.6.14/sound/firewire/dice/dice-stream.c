
 

#include "dice.h"

#define	READY_TIMEOUT_MS	200
#define NOTIFICATION_TIMEOUT_MS	100

struct reg_params {
	unsigned int count;
	unsigned int size;
};

const unsigned int snd_dice_rates[SND_DICE_RATES_COUNT] = {
	 
	[0] =  32000,
	[1] =  44100,
	[2] =  48000,
	 
	[3] =  88200,
	[4] =  96000,
	 
	[5] = 176400,
	[6] = 192000,
};

int snd_dice_stream_get_rate_mode(struct snd_dice *dice, unsigned int rate,
				  enum snd_dice_rate_mode *mode)
{
	 
	static const enum snd_dice_rate_mode modes[] = {
		[0] = SND_DICE_RATE_MODE_LOW,
		[1] = SND_DICE_RATE_MODE_LOW,
		[2] = SND_DICE_RATE_MODE_LOW,
		[3] = SND_DICE_RATE_MODE_MIDDLE,
		[4] = SND_DICE_RATE_MODE_MIDDLE,
		[5] = SND_DICE_RATE_MODE_HIGH,
		[6] = SND_DICE_RATE_MODE_HIGH,
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(snd_dice_rates); i++) {
		if (!(dice->clock_caps & BIT(i)))
			continue;
		if (snd_dice_rates[i] != rate)
			continue;

		*mode = modes[i];
		return 0;
	}

	return -EINVAL;
}

static int select_clock(struct snd_dice *dice, unsigned int rate)
{
	__be32 reg, new;
	u32 data;
	int i;
	int err;

	err = snd_dice_transaction_read_global(dice, GLOBAL_CLOCK_SELECT,
					       &reg, sizeof(reg));
	if (err < 0)
		return err;

	data = be32_to_cpu(reg);

	data &= ~CLOCK_RATE_MASK;
	for (i = 0; i < ARRAY_SIZE(snd_dice_rates); ++i) {
		if (snd_dice_rates[i] == rate)
			break;
	}
	if (i == ARRAY_SIZE(snd_dice_rates))
		return -EINVAL;
	data |= i << CLOCK_RATE_SHIFT;

	if (completion_done(&dice->clock_accepted))
		reinit_completion(&dice->clock_accepted);

	new = cpu_to_be32(data);
	err = snd_dice_transaction_write_global(dice, GLOBAL_CLOCK_SELECT,
						&new, sizeof(new));
	if (err < 0)
		return err;

	if (wait_for_completion_timeout(&dice->clock_accepted,
			msecs_to_jiffies(NOTIFICATION_TIMEOUT_MS)) == 0) {
		if (reg != new)
			return -ETIMEDOUT;
	}

	return 0;
}

static int get_register_params(struct snd_dice *dice,
			       struct reg_params *tx_params,
			       struct reg_params *rx_params)
{
	__be32 reg[2];
	int err;

	err = snd_dice_transaction_read_tx(dice, TX_NUMBER, reg, sizeof(reg));
	if (err < 0)
		return err;
	tx_params->count =
			min_t(unsigned int, be32_to_cpu(reg[0]), MAX_STREAMS);
	tx_params->size = be32_to_cpu(reg[1]) * 4;

	err = snd_dice_transaction_read_rx(dice, RX_NUMBER, reg, sizeof(reg));
	if (err < 0)
		return err;
	rx_params->count =
			min_t(unsigned int, be32_to_cpu(reg[0]), MAX_STREAMS);
	rx_params->size = be32_to_cpu(reg[1]) * 4;

	return 0;
}

static void release_resources(struct snd_dice *dice)
{
	int i;

	for (i = 0; i < MAX_STREAMS; ++i) {
		fw_iso_resources_free(&dice->tx_resources[i]);
		fw_iso_resources_free(&dice->rx_resources[i]);
	}
}

static void stop_streams(struct snd_dice *dice, enum amdtp_stream_direction dir,
			 struct reg_params *params)
{
	__be32 reg;
	unsigned int i;

	for (i = 0; i < params->count; i++) {
		reg = cpu_to_be32((u32)-1);
		if (dir == AMDTP_IN_STREAM) {
			snd_dice_transaction_write_tx(dice,
					params->size * i + TX_ISOCHRONOUS,
					&reg, sizeof(reg));
		} else {
			snd_dice_transaction_write_rx(dice,
					params->size * i + RX_ISOCHRONOUS,
					&reg, sizeof(reg));
		}
	}
}

static int keep_resources(struct snd_dice *dice, struct amdtp_stream *stream,
			  struct fw_iso_resources *resources, unsigned int rate,
			  unsigned int pcm_chs, unsigned int midi_ports)
{
	bool double_pcm_frames;
	unsigned int i;
	int err;

	
	
	
	
	
	
	
	
	double_pcm_frames = (rate > 96000 && !dice->disable_double_pcm_frames);
	if (double_pcm_frames) {
		rate /= 2;
		pcm_chs *= 2;
	}

	err = amdtp_am824_set_parameters(stream, rate, pcm_chs, midi_ports,
					 double_pcm_frames);
	if (err < 0)
		return err;

	if (double_pcm_frames) {
		pcm_chs /= 2;

		for (i = 0; i < pcm_chs; i++) {
			amdtp_am824_set_pcm_position(stream, i, i * 2);
			amdtp_am824_set_pcm_position(stream, i + pcm_chs,
						     i * 2 + 1);
		}
	}

	return fw_iso_resources_allocate(resources,
				amdtp_stream_get_max_payload(stream),
				fw_parent_device(dice->unit)->max_speed);
}

static int keep_dual_resources(struct snd_dice *dice, unsigned int rate,
			       enum amdtp_stream_direction dir,
			       struct reg_params *params)
{
	enum snd_dice_rate_mode mode;
	int i;
	int err;

	err = snd_dice_stream_get_rate_mode(dice, rate, &mode);
	if (err < 0)
		return err;

	for (i = 0; i < params->count; ++i) {
		__be32 reg[2];
		struct amdtp_stream *stream;
		struct fw_iso_resources *resources;
		unsigned int pcm_cache;
		unsigned int pcm_chs;
		unsigned int midi_ports;

		if (dir == AMDTP_IN_STREAM) {
			stream = &dice->tx_stream[i];
			resources = &dice->tx_resources[i];

			pcm_cache = dice->tx_pcm_chs[i][mode];
			err = snd_dice_transaction_read_tx(dice,
					params->size * i + TX_NUMBER_AUDIO,
					reg, sizeof(reg));
		} else {
			stream = &dice->rx_stream[i];
			resources = &dice->rx_resources[i];

			pcm_cache = dice->rx_pcm_chs[i][mode];
			err = snd_dice_transaction_read_rx(dice,
					params->size * i + RX_NUMBER_AUDIO,
					reg, sizeof(reg));
		}
		if (err < 0)
			return err;
		pcm_chs = be32_to_cpu(reg[0]);
		midi_ports = be32_to_cpu(reg[1]);

		
		if (pcm_chs != pcm_cache) {
			dev_info(&dice->unit->device,
				 "cache mismatch: pcm: %u:%u, midi: %u\n",
				 pcm_chs, pcm_cache, midi_ports);
			return -EPROTO;
		}

		err = keep_resources(dice, stream, resources, rate, pcm_chs,
				     midi_ports);
		if (err < 0)
			return err;
	}

	return 0;
}

static void finish_session(struct snd_dice *dice, struct reg_params *tx_params,
			   struct reg_params *rx_params)
{
	stop_streams(dice, AMDTP_IN_STREAM, tx_params);
	stop_streams(dice, AMDTP_OUT_STREAM, rx_params);

	snd_dice_transaction_clear_enable(dice);
}

int snd_dice_stream_reserve_duplex(struct snd_dice *dice, unsigned int rate,
				   unsigned int events_per_period,
				   unsigned int events_per_buffer)
{
	unsigned int curr_rate;
	int err;

	
	err = snd_dice_transaction_get_rate(dice, &curr_rate);
	if (err < 0)
		return err;
	if (rate == 0)
		rate = curr_rate;

	if (dice->substreams_counter == 0 || curr_rate != rate) {
		struct reg_params tx_params, rx_params;

		amdtp_domain_stop(&dice->domain);

		err = get_register_params(dice, &tx_params, &rx_params);
		if (err < 0)
			return err;
		finish_session(dice, &tx_params, &rx_params);

		release_resources(dice);

		
		
		
		err = select_clock(dice, rate);
		if (err < 0)
			return err;

		
		
		err = get_register_params(dice, &tx_params, &rx_params);
		if (err < 0)
			return err;

		err = keep_dual_resources(dice, rate, AMDTP_IN_STREAM,
					  &tx_params);
		if (err < 0)
			goto error;

		err = keep_dual_resources(dice, rate, AMDTP_OUT_STREAM,
					  &rx_params);
		if (err < 0)
			goto error;

		err = amdtp_domain_set_events_per_period(&dice->domain,
					events_per_period, events_per_buffer);
		if (err < 0)
			goto error;
	}

	return 0;
error:
	release_resources(dice);
	return err;
}

static int start_streams(struct snd_dice *dice, enum amdtp_stream_direction dir,
			 unsigned int rate, struct reg_params *params)
{
	unsigned int max_speed = fw_parent_device(dice->unit)->max_speed;
	int i;
	int err;

	for (i = 0; i < params->count; i++) {
		struct amdtp_stream *stream;
		struct fw_iso_resources *resources;
		__be32 reg;

		if (dir == AMDTP_IN_STREAM) {
			stream = dice->tx_stream + i;
			resources = dice->tx_resources + i;
		} else {
			stream = dice->rx_stream + i;
			resources = dice->rx_resources + i;
		}

		reg = cpu_to_be32(resources->channel);
		if (dir == AMDTP_IN_STREAM) {
			err = snd_dice_transaction_write_tx(dice,
					params->size * i + TX_ISOCHRONOUS,
					&reg, sizeof(reg));
		} else {
			err = snd_dice_transaction_write_rx(dice,
					params->size * i + RX_ISOCHRONOUS,
					&reg, sizeof(reg));
		}
		if (err < 0)
			return err;

		if (dir == AMDTP_IN_STREAM) {
			reg = cpu_to_be32(max_speed);
			err = snd_dice_transaction_write_tx(dice,
					params->size * i + TX_SPEED,
					&reg, sizeof(reg));
			if (err < 0)
				return err;
		}

		err = amdtp_domain_add_stream(&dice->domain, stream,
					      resources->channel, max_speed);
		if (err < 0)
			return err;
	}

	return 0;
}

 
int snd_dice_stream_start_duplex(struct snd_dice *dice)
{
	unsigned int generation = dice->rx_resources[0].generation;
	struct reg_params tx_params, rx_params;
	unsigned int i;
	unsigned int rate;
	enum snd_dice_rate_mode mode;
	int err;

	if (dice->substreams_counter == 0)
		return -EIO;

	err = get_register_params(dice, &tx_params, &rx_params);
	if (err < 0)
		return err;

	 
	for (i = 0; i < MAX_STREAMS; ++i) {
		if (amdtp_streaming_error(&dice->tx_stream[i]) ||
		    amdtp_streaming_error(&dice->rx_stream[i])) {
			amdtp_domain_stop(&dice->domain);
			finish_session(dice, &tx_params, &rx_params);
			break;
		}
	}

	if (generation != fw_parent_device(dice->unit)->card->generation) {
		for (i = 0; i < MAX_STREAMS; ++i) {
			if (i < tx_params.count)
				fw_iso_resources_update(dice->tx_resources + i);
			if (i < rx_params.count)
				fw_iso_resources_update(dice->rx_resources + i);
		}
	}

	 
	err = snd_dice_transaction_get_rate(dice, &rate);
	if (err < 0)
		return err;
	err = snd_dice_stream_get_rate_mode(dice, rate, &mode);
	if (err < 0)
		return err;
	for (i = 0; i < MAX_STREAMS; ++i) {
		if (dice->tx_pcm_chs[i][mode] > 0 &&
		    !amdtp_stream_running(&dice->tx_stream[i]))
			break;
		if (dice->rx_pcm_chs[i][mode] > 0 &&
		    !amdtp_stream_running(&dice->rx_stream[i]))
			break;
	}
	if (i < MAX_STREAMS) {
		 
		err = start_streams(dice, AMDTP_IN_STREAM, rate, &tx_params);
		if (err < 0)
			goto error;

		err = start_streams(dice, AMDTP_OUT_STREAM, rate, &rx_params);
		if (err < 0)
			goto error;

		err = snd_dice_transaction_set_enable(dice);
		if (err < 0) {
			dev_err(&dice->unit->device,
				"fail to enable interface\n");
			goto error;
		}

		 
		 
		 
		 
		err = amdtp_domain_start(&dice->domain, 0, true, false);
		if (err < 0)
			goto error;

		if (!amdtp_domain_wait_ready(&dice->domain, READY_TIMEOUT_MS)) {
			err = -ETIMEDOUT;
			goto error;
		}
	}

	return 0;
error:
	amdtp_domain_stop(&dice->domain);
	finish_session(dice, &tx_params, &rx_params);
	return err;
}

 
void snd_dice_stream_stop_duplex(struct snd_dice *dice)
{
	struct reg_params tx_params, rx_params;

	if (dice->substreams_counter == 0) {
		if (get_register_params(dice, &tx_params, &rx_params) >= 0)
			finish_session(dice, &tx_params, &rx_params);

		amdtp_domain_stop(&dice->domain);
		release_resources(dice);
	}
}

static int init_stream(struct snd_dice *dice, enum amdtp_stream_direction dir,
		       unsigned int index)
{
	struct amdtp_stream *stream;
	struct fw_iso_resources *resources;
	int err;

	if (dir == AMDTP_IN_STREAM) {
		stream = &dice->tx_stream[index];
		resources = &dice->tx_resources[index];
	} else {
		stream = &dice->rx_stream[index];
		resources = &dice->rx_resources[index];
	}

	err = fw_iso_resources_init(resources, dice->unit);
	if (err < 0)
		goto end;
	resources->channels_mask = 0x00000000ffffffffuLL;

	err = amdtp_am824_init(stream, dice->unit, dir, CIP_BLOCKING);
	if (err < 0) {
		amdtp_stream_destroy(stream);
		fw_iso_resources_destroy(resources);
	}
end:
	return err;
}

 
static void destroy_stream(struct snd_dice *dice,
			   enum amdtp_stream_direction dir,
			   unsigned int index)
{
	struct amdtp_stream *stream;
	struct fw_iso_resources *resources;

	if (dir == AMDTP_IN_STREAM) {
		stream = &dice->tx_stream[index];
		resources = &dice->tx_resources[index];
	} else {
		stream = &dice->rx_stream[index];
		resources = &dice->rx_resources[index];
	}

	amdtp_stream_destroy(stream);
	fw_iso_resources_destroy(resources);
}

int snd_dice_stream_init_duplex(struct snd_dice *dice)
{
	int i, err;

	for (i = 0; i < MAX_STREAMS; i++) {
		err = init_stream(dice, AMDTP_IN_STREAM, i);
		if (err < 0) {
			for (; i >= 0; i--)
				destroy_stream(dice, AMDTP_IN_STREAM, i);
			goto end;
		}
	}

	for (i = 0; i < MAX_STREAMS; i++) {
		err = init_stream(dice, AMDTP_OUT_STREAM, i);
		if (err < 0) {
			for (; i >= 0; i--)
				destroy_stream(dice, AMDTP_OUT_STREAM, i);
			for (i = 0; i < MAX_STREAMS; i++)
				destroy_stream(dice, AMDTP_IN_STREAM, i);
			goto end;
		}
	}

	err = amdtp_domain_init(&dice->domain);
	if (err < 0) {
		for (i = 0; i < MAX_STREAMS; ++i) {
			destroy_stream(dice, AMDTP_OUT_STREAM, i);
			destroy_stream(dice, AMDTP_IN_STREAM, i);
		}
	}
end:
	return err;
}

void snd_dice_stream_destroy_duplex(struct snd_dice *dice)
{
	unsigned int i;

	for (i = 0; i < MAX_STREAMS; i++) {
		destroy_stream(dice, AMDTP_IN_STREAM, i);
		destroy_stream(dice, AMDTP_OUT_STREAM, i);
	}

	amdtp_domain_destroy(&dice->domain);
}

void snd_dice_stream_update_duplex(struct snd_dice *dice)
{
	struct reg_params tx_params, rx_params;

	 
	dice->global_enabled = false;

	if (get_register_params(dice, &tx_params, &rx_params) == 0) {
		amdtp_domain_stop(&dice->domain);

		stop_streams(dice, AMDTP_IN_STREAM, &tx_params);
		stop_streams(dice, AMDTP_OUT_STREAM, &rx_params);
	}
}

int snd_dice_stream_detect_current_formats(struct snd_dice *dice)
{
	unsigned int rate;
	enum snd_dice_rate_mode mode;
	__be32 reg[2];
	struct reg_params tx_params, rx_params;
	int i;
	int err;

	 
	err = snd_dice_detect_extension_formats(dice);
	if (err >= 0)
		return err;

	 
	err = snd_dice_transaction_get_rate(dice, &rate);
	if (err < 0)
		return err;

	err = snd_dice_stream_get_rate_mode(dice, rate, &mode);
	if (err < 0)
		return err;

	 
	err = select_clock(dice, rate);
	if (err < 0)
		return err;

	err = get_register_params(dice, &tx_params, &rx_params);
	if (err < 0)
		return err;

	for (i = 0; i < tx_params.count; ++i) {
		err = snd_dice_transaction_read_tx(dice,
				tx_params.size * i + TX_NUMBER_AUDIO,
				reg, sizeof(reg));
		if (err < 0)
			return err;
		dice->tx_pcm_chs[i][mode] = be32_to_cpu(reg[0]);
		dice->tx_midi_ports[i] = max_t(unsigned int,
				be32_to_cpu(reg[1]), dice->tx_midi_ports[i]);
	}
	for (i = 0; i < rx_params.count; ++i) {
		err = snd_dice_transaction_read_rx(dice,
				rx_params.size * i + RX_NUMBER_AUDIO,
				reg, sizeof(reg));
		if (err < 0)
			return err;
		dice->rx_pcm_chs[i][mode] = be32_to_cpu(reg[0]);
		dice->rx_midi_ports[i] = max_t(unsigned int,
				be32_to_cpu(reg[1]), dice->rx_midi_ports[i]);
	}

	return 0;
}

static void dice_lock_changed(struct snd_dice *dice)
{
	dice->dev_lock_changed = true;
	wake_up(&dice->hwdep_wait);
}

int snd_dice_stream_lock_try(struct snd_dice *dice)
{
	int err;

	spin_lock_irq(&dice->lock);

	if (dice->dev_lock_count < 0) {
		err = -EBUSY;
		goto out;
	}

	if (dice->dev_lock_count++ == 0)
		dice_lock_changed(dice);
	err = 0;
out:
	spin_unlock_irq(&dice->lock);
	return err;
}

void snd_dice_stream_lock_release(struct snd_dice *dice)
{
	spin_lock_irq(&dice->lock);

	if (WARN_ON(dice->dev_lock_count <= 0))
		goto out;

	if (--dice->dev_lock_count == 0)
		dice_lock_changed(dice);
out:
	spin_unlock_irq(&dice->lock);
}
