









#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <linux/bitops.h>

#define soc_component_ret(dai, ret) _soc_component_ret(dai, __func__, ret, -1)
#define soc_component_ret_reg_rw(dai, ret, reg) _soc_component_ret(dai, __func__, ret, reg)
static inline int _soc_component_ret(struct snd_soc_component *component,
				     const char *func, int ret, int reg)
{
	 
	if (ret >= 0)
		return ret;

	 
	switch (ret) {
	case -EPROBE_DEFER:
	case -ENOTSUPP:
		break;
	default:
		if (reg == -1)
			dev_err(component->dev,
				"ASoC: error at %s on %s: %d\n",
				func, component->name, ret);
		else
			dev_err(component->dev,
				"ASoC: error at %s on %s for register: [0x%08x] %d\n",
				func, component->name, reg, ret);
	}

	return ret;
}

static inline int soc_component_field_shift(struct snd_soc_component *component,
					    unsigned int mask)
{
	if (!mask) {
		dev_err(component->dev,	"ASoC: error field mask is zero for %s\n",
			component->name);
		return 0;
	}

	return (ffs(mask) - 1);
}

 
#define soc_component_mark_push(component, substream, tgt)	((component)->mark_##tgt = substream)
#define soc_component_mark_pop(component, substream, tgt)	((component)->mark_##tgt = NULL)
#define soc_component_mark_match(component, substream, tgt)	((component)->mark_##tgt == substream)

void snd_soc_component_set_aux(struct snd_soc_component *component,
			       struct snd_soc_aux_dev *aux)
{
	component->init = (aux) ? aux->init : NULL;
}

int snd_soc_component_init(struct snd_soc_component *component)
{
	int ret = 0;

	if (component->init)
		ret = component->init(component);

	return soc_component_ret(component, ret);
}

 
int snd_soc_component_set_sysclk(struct snd_soc_component *component,
				 int clk_id, int source, unsigned int freq,
				 int dir)
{
	int ret = -ENOTSUPP;

	if (component->driver->set_sysclk)
		ret = component->driver->set_sysclk(component, clk_id, source,
						     freq, dir);

	return soc_component_ret(component, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_component_set_sysclk);

 
int snd_soc_component_set_pll(struct snd_soc_component *component, int pll_id,
			      int source, unsigned int freq_in,
			      unsigned int freq_out)
{
	int ret = -EINVAL;

	if (component->driver->set_pll)
		ret = component->driver->set_pll(component, pll_id, source,
						  freq_in, freq_out);

	return soc_component_ret(component, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_component_set_pll);

void snd_soc_component_seq_notifier(struct snd_soc_component *component,
				    enum snd_soc_dapm_type type, int subseq)
{
	if (component->driver->seq_notifier)
		component->driver->seq_notifier(component, type, subseq);
}

int snd_soc_component_stream_event(struct snd_soc_component *component,
				   int event)
{
	int ret = 0;

	if (component->driver->stream_event)
		ret = component->driver->stream_event(component, event);

	return soc_component_ret(component, ret);
}

int snd_soc_component_set_bias_level(struct snd_soc_component *component,
				     enum snd_soc_bias_level level)
{
	int ret = 0;

	if (component->driver->set_bias_level)
		ret = component->driver->set_bias_level(component, level);

	return soc_component_ret(component, ret);
}

int snd_soc_component_enable_pin(struct snd_soc_component *component,
				 const char *pin)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	return snd_soc_dapm_enable_pin(dapm, pin);
}
EXPORT_SYMBOL_GPL(snd_soc_component_enable_pin);

int snd_soc_component_enable_pin_unlocked(struct snd_soc_component *component,
					  const char *pin)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	return snd_soc_dapm_enable_pin_unlocked(dapm, pin);
}
EXPORT_SYMBOL_GPL(snd_soc_component_enable_pin_unlocked);

int snd_soc_component_disable_pin(struct snd_soc_component *component,
				  const char *pin)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	return snd_soc_dapm_disable_pin(dapm, pin);
}
EXPORT_SYMBOL_GPL(snd_soc_component_disable_pin);

int snd_soc_component_disable_pin_unlocked(struct snd_soc_component *component,
					   const char *pin)
{
	struct snd_soc_dapm_context *dapm = 
		snd_soc_component_get_dapm(component);
	return snd_soc_dapm_disable_pin_unlocked(dapm, pin);
}
EXPORT_SYMBOL_GPL(snd_soc_component_disable_pin_unlocked);

int snd_soc_component_nc_pin(struct snd_soc_component *component,
			     const char *pin)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	return snd_soc_dapm_nc_pin(dapm, pin);
}
EXPORT_SYMBOL_GPL(snd_soc_component_nc_pin);

int snd_soc_component_nc_pin_unlocked(struct snd_soc_component *component,
				      const char *pin)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	return snd_soc_dapm_nc_pin_unlocked(dapm, pin);
}
EXPORT_SYMBOL_GPL(snd_soc_component_nc_pin_unlocked);

int snd_soc_component_get_pin_status(struct snd_soc_component *component,
				     const char *pin)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	return snd_soc_dapm_get_pin_status(dapm, pin);
}
EXPORT_SYMBOL_GPL(snd_soc_component_get_pin_status);

int snd_soc_component_force_enable_pin(struct snd_soc_component *component,
				       const char *pin)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	return snd_soc_dapm_force_enable_pin(dapm, pin);
}
EXPORT_SYMBOL_GPL(snd_soc_component_force_enable_pin);

int snd_soc_component_force_enable_pin_unlocked(
	struct snd_soc_component *component,
	const char *pin)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	return snd_soc_dapm_force_enable_pin_unlocked(dapm, pin);
}
EXPORT_SYMBOL_GPL(snd_soc_component_force_enable_pin_unlocked);

int snd_soc_component_notify_control(struct snd_soc_component *component,
				     const char * const ctl)
{
	char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
	struct snd_kcontrol *kctl;

	 
	if (component->name_prefix)
		snprintf(name, ARRAY_SIZE(name), "%s %s", component->name_prefix, ctl);
	else
		snprintf(name, ARRAY_SIZE(name), "%s", ctl);

	kctl = snd_soc_card_get_kcontrol(component->card, name);
	if (!kctl)
		return soc_component_ret(component, -EINVAL);

	snd_ctl_notify(component->card->snd_card,
		       SNDRV_CTL_EVENT_MASK_VALUE, &kctl->id);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_component_notify_control);

 
int snd_soc_component_set_jack(struct snd_soc_component *component,
			       struct snd_soc_jack *jack, void *data)
{
	int ret = -ENOTSUPP;

	if (component->driver->set_jack)
		ret = component->driver->set_jack(component, jack, data);

	return soc_component_ret(component, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_component_set_jack);

 
int snd_soc_component_get_jack_type(
	struct snd_soc_component *component)
{
	int ret = -ENOTSUPP;

	if (component->driver->get_jack_type)
		ret = component->driver->get_jack_type(component);

	return soc_component_ret(component, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_component_get_jack_type);

int snd_soc_component_module_get(struct snd_soc_component *component,
				 void *mark, int upon_open)
{
	int ret = 0;

	if (component->driver->module_get_upon_open == !!upon_open &&
	    !try_module_get(component->dev->driver->owner))
		ret = -ENODEV;

	 
	if (ret == 0)
		soc_component_mark_push(component, mark, module);

	return soc_component_ret(component, ret);
}

void snd_soc_component_module_put(struct snd_soc_component *component,
				  void *mark, int upon_open, int rollback)
{
	if (rollback && !soc_component_mark_match(component, mark, module))
		return;

	if (component->driver->module_get_upon_open == !!upon_open)
		module_put(component->dev->driver->owner);

	 
	soc_component_mark_pop(component, mark, module);
}

int snd_soc_component_open(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (component->driver->open)
		ret = component->driver->open(component, substream);

	 
	if (ret == 0)
		soc_component_mark_push(component, substream, open);

	return soc_component_ret(component, ret);
}

int snd_soc_component_close(struct snd_soc_component *component,
			    struct snd_pcm_substream *substream,
			    int rollback)
{
	int ret = 0;

	if (rollback && !soc_component_mark_match(component, substream, open))
		return 0;

	if (component->driver->close)
		ret = component->driver->close(component, substream);

	 
	soc_component_mark_pop(component, substream, open);

	return soc_component_ret(component, ret);
}

void snd_soc_component_suspend(struct snd_soc_component *component)
{
	if (component->driver->suspend)
		component->driver->suspend(component);
	component->suspended = 1;
}

void snd_soc_component_resume(struct snd_soc_component *component)
{
	if (component->driver->resume)
		component->driver->resume(component);
	component->suspended = 0;
}

int snd_soc_component_is_suspended(struct snd_soc_component *component)
{
	return component->suspended;
}

int snd_soc_component_probe(struct snd_soc_component *component)
{
	int ret = 0;

	if (component->driver->probe)
		ret = component->driver->probe(component);

	return soc_component_ret(component, ret);
}

void snd_soc_component_remove(struct snd_soc_component *component)
{
	if (component->driver->remove)
		component->driver->remove(component);
}

int snd_soc_component_of_xlate_dai_id(struct snd_soc_component *component,
				      struct device_node *ep)
{
	int ret = -ENOTSUPP;

	if (component->driver->of_xlate_dai_id)
		ret = component->driver->of_xlate_dai_id(component, ep);

	return soc_component_ret(component, ret);
}

int snd_soc_component_of_xlate_dai_name(struct snd_soc_component *component,
					const struct of_phandle_args *args,
					const char **dai_name)
{
	if (component->driver->of_xlate_dai_name)
		return component->driver->of_xlate_dai_name(component,
							    args, dai_name);
	 
	return -ENOTSUPP;
}

void snd_soc_component_setup_regmap(struct snd_soc_component *component)
{
	int val_bytes = regmap_get_val_bytes(component->regmap);

	 
	if (val_bytes > 0)
		component->val_bytes = val_bytes;
}

#ifdef CONFIG_REGMAP

 
void snd_soc_component_init_regmap(struct snd_soc_component *component,
				   struct regmap *regmap)
{
	component->regmap = regmap;
	snd_soc_component_setup_regmap(component);
}
EXPORT_SYMBOL_GPL(snd_soc_component_init_regmap);

 
void snd_soc_component_exit_regmap(struct snd_soc_component *component)
{
	regmap_exit(component->regmap);
	component->regmap = NULL;
}
EXPORT_SYMBOL_GPL(snd_soc_component_exit_regmap);

#endif

int snd_soc_component_compr_open(struct snd_soc_component *component,
				 struct snd_compr_stream *cstream)
{
	int ret = 0;

	if (component->driver->compress_ops &&
	    component->driver->compress_ops->open)
		ret = component->driver->compress_ops->open(component, cstream);

	 
	if (ret == 0)
		soc_component_mark_push(component, cstream, compr_open);

	return soc_component_ret(component, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_open);

void snd_soc_component_compr_free(struct snd_soc_component *component,
				  struct snd_compr_stream *cstream,
				  int rollback)
{
	if (rollback && !soc_component_mark_match(component, cstream, compr_open))
		return;

	if (component->driver->compress_ops &&
	    component->driver->compress_ops->free)
		component->driver->compress_ops->free(component, cstream);

	 
	soc_component_mark_pop(component, cstream, compr_open);
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_free);

int snd_soc_component_compr_trigger(struct snd_compr_stream *cstream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->compress_ops &&
		    component->driver->compress_ops->trigger) {
			ret = component->driver->compress_ops->trigger(
				component, cstream, cmd);
			if (ret < 0)
				return soc_component_ret(component, ret);
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_trigger);

int snd_soc_component_compr_set_params(struct snd_compr_stream *cstream,
				       struct snd_compr_params *params)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->compress_ops &&
		    component->driver->compress_ops->set_params) {
			ret = component->driver->compress_ops->set_params(
				component, cstream, params);
			if (ret < 0)
				return soc_component_ret(component, ret);
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_set_params);

int snd_soc_component_compr_get_params(struct snd_compr_stream *cstream,
				       struct snd_codec *params)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->compress_ops &&
		    component->driver->compress_ops->get_params) {
			ret = component->driver->compress_ops->get_params(
				component, cstream, params);
			return soc_component_ret(component, ret);
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_get_params);

int snd_soc_component_compr_get_caps(struct snd_compr_stream *cstream,
				     struct snd_compr_caps *caps)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component;
	int i, ret = 0;

	snd_soc_dpcm_mutex_lock(rtd);

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->compress_ops &&
		    component->driver->compress_ops->get_caps) {
			ret = component->driver->compress_ops->get_caps(
				component, cstream, caps);
			break;
		}
	}

	snd_soc_dpcm_mutex_unlock(rtd);

	return soc_component_ret(component, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_get_caps);

int snd_soc_component_compr_get_codec_caps(struct snd_compr_stream *cstream,
					   struct snd_compr_codec_caps *codec)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component;
	int i, ret = 0;

	snd_soc_dpcm_mutex_lock(rtd);

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->compress_ops &&
		    component->driver->compress_ops->get_codec_caps) {
			ret = component->driver->compress_ops->get_codec_caps(
				component, cstream, codec);
			break;
		}
	}

	snd_soc_dpcm_mutex_unlock(rtd);

	return soc_component_ret(component, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_get_codec_caps);

int snd_soc_component_compr_ack(struct snd_compr_stream *cstream, size_t bytes)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->compress_ops &&
		    component->driver->compress_ops->ack) {
			ret = component->driver->compress_ops->ack(
				component, cstream, bytes);
			if (ret < 0)
				return soc_component_ret(component, ret);
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_ack);

int snd_soc_component_compr_pointer(struct snd_compr_stream *cstream,
				    struct snd_compr_tstamp *tstamp)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->compress_ops &&
		    component->driver->compress_ops->pointer) {
			ret = component->driver->compress_ops->pointer(
				component, cstream, tstamp);
			return soc_component_ret(component, ret);
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_pointer);

int snd_soc_component_compr_copy(struct snd_compr_stream *cstream,
				 char __user *buf, size_t count)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component;
	int i, ret = 0;

	snd_soc_dpcm_mutex_lock(rtd);

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->compress_ops &&
		    component->driver->compress_ops->copy) {
			ret = component->driver->compress_ops->copy(
				component, cstream, buf, count);
			break;
		}
	}

	snd_soc_dpcm_mutex_unlock(rtd);

	return soc_component_ret(component, ret);
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_copy);

int snd_soc_component_compr_set_metadata(struct snd_compr_stream *cstream,
					 struct snd_compr_metadata *metadata)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->compress_ops &&
		    component->driver->compress_ops->set_metadata) {
			ret = component->driver->compress_ops->set_metadata(
				component, cstream, metadata);
			if (ret < 0)
				return soc_component_ret(component, ret);
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_set_metadata);

int snd_soc_component_compr_get_metadata(struct snd_compr_stream *cstream,
					 struct snd_compr_metadata *metadata)
{
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->compress_ops &&
		    component->driver->compress_ops->get_metadata) {
			ret = component->driver->compress_ops->get_metadata(
				component, cstream, metadata);
			return soc_component_ret(component, ret);
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_soc_component_compr_get_metadata);

static unsigned int soc_component_read_no_lock(
	struct snd_soc_component *component,
	unsigned int reg)
{
	int ret;
	unsigned int val = 0;

	if (component->regmap)
		ret = regmap_read(component->regmap, reg, &val);
	else if (component->driver->read) {
		ret = 0;
		val = component->driver->read(component, reg);
	}
	else
		ret = -EIO;

	if (ret < 0)
		return soc_component_ret_reg_rw(component, ret, reg);

	return val;
}

 
unsigned int snd_soc_component_read(struct snd_soc_component *component,
				    unsigned int reg)
{
	unsigned int val;

	mutex_lock(&component->io_mutex);
	val = soc_component_read_no_lock(component, reg);
	mutex_unlock(&component->io_mutex);

	return val;
}
EXPORT_SYMBOL_GPL(snd_soc_component_read);

static int soc_component_write_no_lock(
	struct snd_soc_component *component,
	unsigned int reg, unsigned int val)
{
	int ret = -EIO;

	if (component->regmap)
		ret = regmap_write(component->regmap, reg, val);
	else if (component->driver->write)
		ret = component->driver->write(component, reg, val);

	return soc_component_ret_reg_rw(component, ret, reg);
}

 
int snd_soc_component_write(struct snd_soc_component *component,
			    unsigned int reg, unsigned int val)
{
	int ret;

	mutex_lock(&component->io_mutex);
	ret = soc_component_write_no_lock(component, reg, val);
	mutex_unlock(&component->io_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(snd_soc_component_write);

static int snd_soc_component_update_bits_legacy(
	struct snd_soc_component *component, unsigned int reg,
	unsigned int mask, unsigned int val, bool *change)
{
	unsigned int old, new;
	int ret = 0;

	mutex_lock(&component->io_mutex);

	old = soc_component_read_no_lock(component, reg);

	new = (old & ~mask) | (val & mask);
	*change = old != new;
	if (*change)
		ret = soc_component_write_no_lock(component, reg, new);

	mutex_unlock(&component->io_mutex);

	return soc_component_ret_reg_rw(component, ret, reg);
}

 
int snd_soc_component_update_bits(struct snd_soc_component *component,
				  unsigned int reg, unsigned int mask, unsigned int val)
{
	bool change;
	int ret;

	if (component->regmap)
		ret = regmap_update_bits_check(component->regmap, reg, mask,
					       val, &change);
	else
		ret = snd_soc_component_update_bits_legacy(component, reg,
							   mask, val, &change);

	if (ret < 0)
		return soc_component_ret_reg_rw(component, ret, reg);
	return change;
}
EXPORT_SYMBOL_GPL(snd_soc_component_update_bits);

 
int snd_soc_component_update_bits_async(struct snd_soc_component *component,
					unsigned int reg, unsigned int mask, unsigned int val)
{
	bool change;
	int ret;

	if (component->regmap)
		ret = regmap_update_bits_check_async(component->regmap, reg,
						     mask, val, &change);
	else
		ret = snd_soc_component_update_bits_legacy(component, reg,
							   mask, val, &change);

	if (ret < 0)
		return soc_component_ret_reg_rw(component, ret, reg);
	return change;
}
EXPORT_SYMBOL_GPL(snd_soc_component_update_bits_async);

 
unsigned int snd_soc_component_read_field(struct snd_soc_component *component,
					  unsigned int reg, unsigned int mask)
{
	unsigned int val;

	val = snd_soc_component_read(component, reg);

	val = (val & mask) >> soc_component_field_shift(component, mask);

	return val;
}
EXPORT_SYMBOL_GPL(snd_soc_component_read_field);

 
int snd_soc_component_write_field(struct snd_soc_component *component,
				  unsigned int reg, unsigned int mask,
				  unsigned int val)
{

	val = (val << soc_component_field_shift(component, mask)) & mask;

	return snd_soc_component_update_bits(component, reg, mask, val);
}
EXPORT_SYMBOL_GPL(snd_soc_component_write_field);

 
void snd_soc_component_async_complete(struct snd_soc_component *component)
{
	if (component->regmap)
		regmap_async_complete(component->regmap);
}
EXPORT_SYMBOL_GPL(snd_soc_component_async_complete);

 
int snd_soc_component_test_bits(struct snd_soc_component *component,
				unsigned int reg, unsigned int mask, unsigned int value)
{
	unsigned int old, new;

	old = snd_soc_component_read(component, reg);
	new = (old & ~mask) | value;
	return old != new;
}
EXPORT_SYMBOL_GPL(snd_soc_component_test_bits);

int snd_soc_pcm_component_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	int i;

	 
	for_each_rtd_components(rtd, i, component)
		if (component->driver->pointer)
			return component->driver->pointer(component, substream);

	return 0;
}

static bool snd_soc_component_is_codec_on_rtd(struct snd_soc_pcm_runtime *rtd,
					      struct snd_soc_component *component)
{
	struct snd_soc_dai *dai;
	int i;

	for_each_rtd_codec_dais(rtd, i, dai) {
		if (dai->component == component)
			return true;
	}

	return false;
}

void snd_soc_pcm_component_delay(struct snd_pcm_substream *substream,
				 snd_pcm_sframes_t *cpu_delay,
				 snd_pcm_sframes_t *codec_delay)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	snd_pcm_sframes_t delay;
	int i;

	 
	for_each_rtd_components(rtd, i, component) {
		if (!component->driver->delay)
			continue;

		delay = component->driver->delay(component, substream);

		if (snd_soc_component_is_codec_on_rtd(rtd, component))
			*codec_delay = max(*codec_delay, delay);
		else
			*cpu_delay = max(*cpu_delay, delay);
	}
}

int snd_soc_pcm_component_ioctl(struct snd_pcm_substream *substream,
				unsigned int cmd, void *arg)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	int i;

	 
	for_each_rtd_components(rtd, i, component)
		if (component->driver->ioctl)
			return soc_component_ret(
				component,
				component->driver->ioctl(component,
							 substream, cmd, arg));

	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

int snd_soc_pcm_component_sync_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->sync_stop) {
			ret = component->driver->sync_stop(component,
							   substream);
			if (ret < 0)
				return soc_component_ret(component, ret);
		}
	}

	return 0;
}

int snd_soc_pcm_component_copy(struct snd_pcm_substream *substream,
			       int channel, unsigned long pos,
			       struct iov_iter *iter, unsigned long bytes)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	int i;

	 
	for_each_rtd_components(rtd, i, component)
		if (component->driver->copy)
			return soc_component_ret(component,
				component->driver->copy(component, substream,
					channel, pos, iter, bytes));

	return -EINVAL;
}

struct page *snd_soc_pcm_component_page(struct snd_pcm_substream *substream,
					unsigned long offset)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	struct page *page;
	int i;

	 
	for_each_rtd_components(rtd, i, component) {
		if (component->driver->page) {
			page = component->driver->page(component,
						       substream, offset);
			if (page)
				return page;
		}
	}

	return NULL;
}

int snd_soc_pcm_component_mmap(struct snd_pcm_substream *substream,
			       struct vm_area_struct *vma)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	int i;

	 
	for_each_rtd_components(rtd, i, component)
		if (component->driver->mmap)
			return soc_component_ret(
				component,
				component->driver->mmap(component,
							substream, vma));

	return -EINVAL;
}

int snd_soc_pcm_component_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component;
	int ret;
	int i;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->pcm_construct) {
			ret = component->driver->pcm_construct(component, rtd);
			if (ret < 0)
				return soc_component_ret(component, ret);
		}
	}

	return 0;
}

void snd_soc_pcm_component_free(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component;
	int i;

	if (!rtd->pcm)
		return;

	for_each_rtd_components(rtd, i, component)
		if (component->driver->pcm_destruct)
			component->driver->pcm_destruct(component, rtd->pcm);
}

int snd_soc_pcm_component_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->prepare) {
			ret = component->driver->prepare(component, substream);
			if (ret < 0)
				return soc_component_ret(component, ret);
		}
	}

	return 0;
}

int snd_soc_pcm_component_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (component->driver->hw_params) {
			ret = component->driver->hw_params(component,
							   substream, params);
			if (ret < 0)
				return soc_component_ret(component, ret);
		}
		 
		soc_component_mark_push(component, substream, hw_params);
	}

	return 0;
}

void snd_soc_pcm_component_hw_free(struct snd_pcm_substream *substream,
				   int rollback)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	int i, ret;

	for_each_rtd_components(rtd, i, component) {
		if (rollback && !soc_component_mark_match(component, substream, hw_params))
			continue;

		if (component->driver->hw_free) {
			ret = component->driver->hw_free(component, substream);
			if (ret < 0)
				soc_component_ret(component, ret);
		}

		 
		soc_component_mark_pop(component, substream, hw_params);
	}
}

static int soc_component_trigger(struct snd_soc_component *component,
				 struct snd_pcm_substream *substream,
				 int cmd)
{
	int ret = 0;

	if (component->driver->trigger)
		ret = component->driver->trigger(component, substream, cmd);

	return soc_component_ret(component, ret);
}

int snd_soc_pcm_component_trigger(struct snd_pcm_substream *substream,
				  int cmd, int rollback)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	int i, r, ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		for_each_rtd_components(rtd, i, component) {
			ret = soc_component_trigger(component, substream, cmd);
			if (ret < 0)
				break;
			soc_component_mark_push(component, substream, trigger);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		for_each_rtd_components(rtd, i, component) {
			if (rollback && !soc_component_mark_match(component, substream, trigger))
				continue;

			r = soc_component_trigger(component, substream, cmd);
			if (r < 0)
				ret = r;  
			soc_component_mark_pop(component, substream, trigger);
		}
	}

	return ret;
}

int snd_soc_pcm_component_pm_runtime_get(struct snd_soc_pcm_runtime *rtd,
					 void *stream)
{
	struct snd_soc_component *component;
	int i;

	for_each_rtd_components(rtd, i, component) {
		int ret = pm_runtime_get_sync(component->dev);
		if (ret < 0 && ret != -EACCES) {
			pm_runtime_put_noidle(component->dev);
			return soc_component_ret(component, ret);
		}
		 
		soc_component_mark_push(component, stream, pm);
	}

	return 0;
}

void snd_soc_pcm_component_pm_runtime_put(struct snd_soc_pcm_runtime *rtd,
					  void *stream, int rollback)
{
	struct snd_soc_component *component;
	int i;

	for_each_rtd_components(rtd, i, component) {
		if (rollback && !soc_component_mark_match(component, stream, pm))
			continue;

		pm_runtime_mark_last_busy(component->dev);
		pm_runtime_put_autosuspend(component->dev);

		 
		soc_component_mark_pop(component, stream, pm);
	}
}

int snd_soc_pcm_component_ack(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_component *component;
	int i;

	 
	for_each_rtd_components(rtd, i, component)
		if (component->driver->ack)
			return component->driver->ack(component, substream);

	return 0;
}
