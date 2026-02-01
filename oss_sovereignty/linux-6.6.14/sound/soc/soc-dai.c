







#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-link.h>

#define soc_dai_ret(dai, ret) _soc_dai_ret(dai, __func__, ret)
static inline int _soc_dai_ret(struct snd_soc_dai *dai,
			       const char *func, int ret)
{
	 
	if (ret >= 0)
		return ret;

	 
	switch (ret) {
	case -EPROBE_DEFER:
	case -ENOTSUPP:
		break;
	default:
		dev_err(dai->dev,
			"ASoC: error at %s on %s: %d\n",
			func, dai->name, ret);
	}

	return ret;
}

 
#define soc_dai_mark_push(dai, substream, tgt)	((dai)->mark_##tgt = substream)
#define soc_dai_mark_pop(dai, substream, tgt)	((dai)->mark_##tgt = NULL)
#define soc_dai_mark_match(dai, substream, tgt)	((dai)->mark_##tgt == substream)

 
int snd_soc_dai_set_sysclk(struct snd_soc_dai *dai, int clk_id,
			   unsigned int freq, int dir)
{
	int ret;

	if (dai->driver->ops &&
	    dai->driver->ops->set_sysclk)
		ret = dai->driver->ops->set_sysclk(dai, clk_id, freq, dir);
	else
		ret = snd_soc_component_set_sysclk(dai->component, clk_id, 0,
						   freq, dir);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_sysclk);

 
int snd_soc_dai_set_clkdiv(struct snd_soc_dai *dai,
			   int div_id, int div)
{
	int ret = -EINVAL;

	if (dai->driver->ops &&
	    dai->driver->ops->set_clkdiv)
		ret = dai->driver->ops->set_clkdiv(dai, div_id, div);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_clkdiv);

 
int snd_soc_dai_set_pll(struct snd_soc_dai *dai, int pll_id, int source,
			unsigned int freq_in, unsigned int freq_out)
{
	int ret;

	if (dai->driver->ops &&
	    dai->driver->ops->set_pll)
		ret = dai->driver->ops->set_pll(dai, pll_id, source,
						freq_in, freq_out);
	else
		ret = snd_soc_component_set_pll(dai->component, pll_id, source,
						freq_in, freq_out);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_pll);

 
int snd_soc_dai_set_bclk_ratio(struct snd_soc_dai *dai, unsigned int ratio)
{
	int ret = -ENOTSUPP;

	if (dai->driver->ops &&
	    dai->driver->ops->set_bclk_ratio)
		ret = dai->driver->ops->set_bclk_ratio(dai, ratio);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_bclk_ratio);

int snd_soc_dai_get_fmt_max_priority(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai;
	int i, max = 0;

	 
	for_each_rtd_dais(rtd, i, dai) {
		if (dai->driver->ops &&
		    dai->driver->ops->num_auto_selectable_formats)
			max = max(max, dai->driver->ops->num_auto_selectable_formats);
		else
			return 0;
	}

	return max;
}

 
u64 snd_soc_dai_get_fmt(struct snd_soc_dai *dai, int priority)
{
	const struct snd_soc_dai_ops *ops = dai->driver->ops;
	u64 fmt = 0;
	int i, max = 0, until = priority;

	 
	if (ops)
		max = ops->num_auto_selectable_formats;

	if (max < until)
		until = max;

	for (i = 0; i < until; i++)
		fmt |= ops->auto_selectable_formats[i];

	return fmt;
}

 
int snd_soc_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	int ret = -ENOTSUPP;

	if (dai->driver->ops && dai->driver->ops->set_fmt)
		ret = dai->driver->ops->set_fmt(dai, fmt);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_fmt);

 
static int snd_soc_xlate_tdm_slot_mask(unsigned int slots,
				       unsigned int *tx_mask,
				       unsigned int *rx_mask)
{
	if (*tx_mask || *rx_mask)
		return 0;

	if (!slots)
		return -EINVAL;

	*tx_mask = (1 << slots) - 1;
	*rx_mask = (1 << slots) - 1;

	return 0;
}

 
int snd_soc_dai_set_tdm_slot(struct snd_soc_dai *dai,
			     unsigned int tx_mask, unsigned int rx_mask,
			     int slots, int slot_width)
{
	int ret = -ENOTSUPP;
	int stream;
	unsigned int *tdm_mask[] = {
		&tx_mask,
		&rx_mask,
	};

	if (dai->driver->ops &&
	    dai->driver->ops->xlate_tdm_slot_mask)
		dai->driver->ops->xlate_tdm_slot_mask(slots,
						      &tx_mask, &rx_mask);
	else
		snd_soc_xlate_tdm_slot_mask(slots, &tx_mask, &rx_mask);

	for_each_pcm_streams(stream)
		snd_soc_dai_tdm_mask_set(dai, stream, *tdm_mask[stream]);

	if (dai->driver->ops &&
	    dai->driver->ops->set_tdm_slot)
		ret = dai->driver->ops->set_tdm_slot(dai, tx_mask, rx_mask,
						      slots, slot_width);
	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_tdm_slot);

 
int snd_soc_dai_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)
{
	int ret = -ENOTSUPP;

	if (dai->driver->ops &&
	    dai->driver->ops->set_channel_map)
		ret = dai->driver->ops->set_channel_map(dai, tx_num, tx_slot,
							rx_num, rx_slot);
	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_channel_map);

 
int snd_soc_dai_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot)
{
	int ret = -ENOTSUPP;

	if (dai->driver->ops &&
	    dai->driver->ops->get_channel_map)
		ret = dai->driver->ops->get_channel_map(dai, tx_num, tx_slot,
							rx_num, rx_slot);
	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_get_channel_map);

 
int snd_soc_dai_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	int ret = -EINVAL;

	if (dai->driver->ops &&
	    dai->driver->ops->set_tristate)
		ret = dai->driver->ops->set_tristate(dai, tristate);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_set_tristate);

 
int snd_soc_dai_digital_mute(struct snd_soc_dai *dai, int mute,
			     int direction)
{
	int ret = -ENOTSUPP;

	 
	if (dai->driver->ops &&
	    dai->driver->ops->mute_stream &&
	    (direction == SNDRV_PCM_STREAM_PLAYBACK ||
	     !dai->driver->ops->no_capture_mute))
		ret = dai->driver->ops->mute_stream(dai, mute, direction);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_digital_mute);

int snd_soc_dai_hw_params(struct snd_soc_dai *dai,
			  struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params)
{
	int ret = 0;

	if (dai->driver->ops &&
	    dai->driver->ops->hw_params)
		ret = dai->driver->ops->hw_params(substream, params, dai);

	 
	if (ret == 0)
		soc_dai_mark_push(dai, substream, hw_params);

	return soc_dai_ret(dai, ret);
}

void snd_soc_dai_hw_free(struct snd_soc_dai *dai,
			 struct snd_pcm_substream *substream,
			 int rollback)
{
	if (rollback && !soc_dai_mark_match(dai, substream, hw_params))
		return;

	if (dai->driver->ops &&
	    dai->driver->ops->hw_free)
		dai->driver->ops->hw_free(substream, dai);

	 
	soc_dai_mark_pop(dai, substream, hw_params);
}

int snd_soc_dai_startup(struct snd_soc_dai *dai,
			struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (!snd_soc_dai_stream_valid(dai, substream->stream))
		return 0;

	if (dai->driver->ops &&
	    dai->driver->ops->startup)
		ret = dai->driver->ops->startup(substream, dai);

	 
	if (ret == 0)
		soc_dai_mark_push(dai, substream, startup);

	return soc_dai_ret(dai, ret);
}

void snd_soc_dai_shutdown(struct snd_soc_dai *dai,
			  struct snd_pcm_substream *substream,
			  int rollback)
{
	if (!snd_soc_dai_stream_valid(dai, substream->stream))
		return;

	if (rollback && !soc_dai_mark_match(dai, substream, startup))
		return;

	if (dai->driver->ops &&
	    dai->driver->ops->shutdown)
		dai->driver->ops->shutdown(substream, dai);

	 
	soc_dai_mark_pop(dai, substream, startup);
}

int snd_soc_dai_compress_new(struct snd_soc_dai *dai,
			     struct snd_soc_pcm_runtime *rtd, int num)
{
	int ret = -ENOTSUPP;
	if (dai->driver->ops &&
	    dai->driver->ops->compress_new)
		ret = dai->driver->ops->compress_new(rtd, num);
	return soc_dai_ret(dai, ret);
}

 
bool snd_soc_dai_stream_valid(struct snd_soc_dai *dai, int dir)
{
	struct snd_soc_pcm_stream *stream = snd_soc_dai_get_pcm_stream(dai, dir);

	 
	return stream->channels_min;
}

 
void snd_soc_dai_link_set_capabilities(struct snd_soc_dai_link *dai_link)
{
	bool supported[SNDRV_PCM_STREAM_LAST + 1];
	int direction;

	for_each_pcm_streams(direction) {
		struct snd_soc_dai_link_component *cpu;
		struct snd_soc_dai_link_component *codec;
		struct snd_soc_dai *dai;
		bool supported_cpu = false;
		bool supported_codec = false;
		int i;

		for_each_link_cpus(dai_link, i, cpu) {
			dai = snd_soc_find_dai_with_mutex(cpu);
			if (dai && snd_soc_dai_stream_valid(dai, direction)) {
				supported_cpu = true;
				break;
			}
		}
		for_each_link_codecs(dai_link, i, codec) {
			dai = snd_soc_find_dai_with_mutex(codec);
			if (dai && snd_soc_dai_stream_valid(dai, direction)) {
				supported_codec = true;
				break;
			}
		}
		supported[direction] = supported_cpu && supported_codec;
	}

	dai_link->dpcm_playback = supported[SNDRV_PCM_STREAM_PLAYBACK];
	dai_link->dpcm_capture  = supported[SNDRV_PCM_STREAM_CAPTURE];
}
EXPORT_SYMBOL_GPL(snd_soc_dai_link_set_capabilities);

void snd_soc_dai_action(struct snd_soc_dai *dai,
			int stream, int action)
{
	 
	dai->stream[stream].active	+= action;

	 
	dai->component->active		+= action;
}
EXPORT_SYMBOL_GPL(snd_soc_dai_action);

int snd_soc_dai_active(struct snd_soc_dai *dai)
{
	int stream, active;

	active = 0;
	for_each_pcm_streams(stream)
		active += dai->stream[stream].active;

	return active;
}
EXPORT_SYMBOL_GPL(snd_soc_dai_active);

int snd_soc_pcm_dai_probe(struct snd_soc_pcm_runtime *rtd, int order)
{
	struct snd_soc_dai *dai;
	int i;

	for_each_rtd_dais(rtd, i, dai) {
		if (dai->probed)
			continue;

		if (dai->driver->ops) {
			if (dai->driver->ops->probe_order != order)
				continue;

			if (dai->driver->ops->probe) {
				int ret = dai->driver->ops->probe(dai);

				if (ret < 0)
					return soc_dai_ret(dai, ret);
			}
		}
		dai->probed = 1;
	}

	return 0;
}

int snd_soc_pcm_dai_remove(struct snd_soc_pcm_runtime *rtd, int order)
{
	struct snd_soc_dai *dai;
	int i, r, ret = 0;

	for_each_rtd_dais(rtd, i, dai) {
		if (!dai->probed)
			continue;

		if (dai->driver->ops) {
			if (dai->driver->ops->remove_order != order)
				continue;

			if (dai->driver->ops->remove) {
				r = dai->driver->ops->remove(dai);
				if (r < 0)
					ret = r;  
			}
		}
		dai->probed = 0;
	}

	return ret;
}

int snd_soc_pcm_dai_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai;
	int i;

	for_each_rtd_dais(rtd, i, dai) {
		if (dai->driver->ops &&
		    dai->driver->ops->pcm_new) {
			int ret = dai->driver->ops->pcm_new(rtd, dai);
			if (ret < 0)
				return soc_dai_ret(dai, ret);
		}
	}

	return 0;
}

int snd_soc_pcm_dai_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai;
	int i, ret;

	for_each_rtd_dais(rtd, i, dai) {
		if (!snd_soc_dai_stream_valid(dai, substream->stream))
			continue;
		if (dai->driver->ops &&
		    dai->driver->ops->prepare) {
			ret = dai->driver->ops->prepare(substream, dai);
			if (ret < 0)
				return soc_dai_ret(dai, ret);
		}
	}

	return 0;
}

static int soc_dai_trigger(struct snd_soc_dai *dai,
			   struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;

	if (!snd_soc_dai_stream_valid(dai, substream->stream))
		return 0;

	if (dai->driver->ops &&
	    dai->driver->ops->trigger)
		ret = dai->driver->ops->trigger(substream, cmd, dai);

	return soc_dai_ret(dai, ret);
}

int snd_soc_pcm_dai_trigger(struct snd_pcm_substream *substream,
			    int cmd, int rollback)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai;
	int i, r, ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		for_each_rtd_dais(rtd, i, dai) {
			ret = soc_dai_trigger(dai, substream, cmd);
			if (ret < 0)
				break;

			if (dai->driver->ops && dai->driver->ops->mute_unmute_on_trigger)
				snd_soc_dai_digital_mute(dai, 0, substream->stream);

			soc_dai_mark_push(dai, substream, trigger);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		for_each_rtd_dais(rtd, i, dai) {
			if (rollback && !soc_dai_mark_match(dai, substream, trigger))
				continue;

			if (dai->driver->ops && dai->driver->ops->mute_unmute_on_trigger)
				snd_soc_dai_digital_mute(dai, 1, substream->stream);

			r = soc_dai_trigger(dai, substream, cmd);
			if (r < 0)
				ret = r;  
			soc_dai_mark_pop(dai, substream, trigger);
		}
	}

	return ret;
}

int snd_soc_pcm_dai_bespoke_trigger(struct snd_pcm_substream *substream,
				    int cmd)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai;
	int i, ret;

	for_each_rtd_dais(rtd, i, dai) {
		if (dai->driver->ops &&
		    dai->driver->ops->bespoke_trigger) {
			ret = dai->driver->ops->bespoke_trigger(substream,
								cmd, dai);
			if (ret < 0)
				return soc_dai_ret(dai, ret);
		}
	}

	return 0;
}

void snd_soc_pcm_dai_delay(struct snd_pcm_substream *substream,
			   snd_pcm_sframes_t *cpu_delay,
			   snd_pcm_sframes_t *codec_delay)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *dai;
	int i;

	 

	 
	for_each_rtd_cpu_dais(rtd, i, dai)
		if (dai->driver->ops &&
		    dai->driver->ops->delay)
			*cpu_delay = max(*cpu_delay, dai->driver->ops->delay(substream, dai));

	 
	for_each_rtd_codec_dais(rtd, i, dai)
		if (dai->driver->ops &&
		    dai->driver->ops->delay)
			*codec_delay = max(*codec_delay, dai->driver->ops->delay(substream, dai));
}

int snd_soc_dai_compr_startup(struct snd_soc_dai *dai,
			      struct snd_compr_stream *cstream)
{
	int ret = 0;

	if (dai->driver->cops &&
	    dai->driver->cops->startup)
		ret = dai->driver->cops->startup(cstream, dai);

	 
	if (ret == 0)
		soc_dai_mark_push(dai, cstream, compr_startup);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_compr_startup);

void snd_soc_dai_compr_shutdown(struct snd_soc_dai *dai,
				struct snd_compr_stream *cstream,
				int rollback)
{
	if (rollback && !soc_dai_mark_match(dai, cstream, compr_startup))
		return;

	if (dai->driver->cops &&
	    dai->driver->cops->shutdown)
		dai->driver->cops->shutdown(cstream, dai);

	 
	soc_dai_mark_pop(dai, cstream, compr_startup);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_compr_shutdown);

int snd_soc_dai_compr_trigger(struct snd_soc_dai *dai,
			      struct snd_compr_stream *cstream, int cmd)
{
	int ret = 0;

	if (dai->driver->cops &&
	    dai->driver->cops->trigger)
		ret = dai->driver->cops->trigger(cstream, cmd, dai);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_compr_trigger);

int snd_soc_dai_compr_set_params(struct snd_soc_dai *dai,
				 struct snd_compr_stream *cstream,
				 struct snd_compr_params *params)
{
	int ret = 0;

	if (dai->driver->cops &&
	    dai->driver->cops->set_params)
		ret = dai->driver->cops->set_params(cstream, params, dai);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_compr_set_params);

int snd_soc_dai_compr_get_params(struct snd_soc_dai *dai,
				 struct snd_compr_stream *cstream,
				 struct snd_codec *params)
{
	int ret = 0;

	if (dai->driver->cops &&
	    dai->driver->cops->get_params)
		ret = dai->driver->cops->get_params(cstream, params, dai);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_compr_get_params);

int snd_soc_dai_compr_ack(struct snd_soc_dai *dai,
			  struct snd_compr_stream *cstream,
			  size_t bytes)
{
	int ret = 0;

	if (dai->driver->cops &&
	    dai->driver->cops->ack)
		ret = dai->driver->cops->ack(cstream, bytes, dai);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_compr_ack);

int snd_soc_dai_compr_pointer(struct snd_soc_dai *dai,
			      struct snd_compr_stream *cstream,
			      struct snd_compr_tstamp *tstamp)
{
	int ret = 0;

	if (dai->driver->cops &&
	    dai->driver->cops->pointer)
		ret = dai->driver->cops->pointer(cstream, tstamp, dai);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_compr_pointer);

int snd_soc_dai_compr_set_metadata(struct snd_soc_dai *dai,
				   struct snd_compr_stream *cstream,
				   struct snd_compr_metadata *metadata)
{
	int ret = 0;

	if (dai->driver->cops &&
	    dai->driver->cops->set_metadata)
		ret = dai->driver->cops->set_metadata(cstream, metadata, dai);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_compr_set_metadata);

int snd_soc_dai_compr_get_metadata(struct snd_soc_dai *dai,
				   struct snd_compr_stream *cstream,
				   struct snd_compr_metadata *metadata)
{
	int ret = 0;

	if (dai->driver->cops &&
	    dai->driver->cops->get_metadata)
		ret = dai->driver->cops->get_metadata(cstream, metadata, dai);

	return soc_dai_ret(dai, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_dai_compr_get_metadata);
