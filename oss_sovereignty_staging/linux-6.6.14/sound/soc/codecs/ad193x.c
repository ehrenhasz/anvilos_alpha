
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "ad193x.h"

 
struct ad193x_priv {
	struct regmap *regmap;
	enum ad193x_type type;
	int sysclk;
};

 
static const char * const ad193x_deemp[] = {"None", "48kHz", "44.1kHz", "32kHz"};

static SOC_ENUM_SINGLE_DECL(ad193x_deemp_enum, AD193X_DAC_CTRL2, 1,
			    ad193x_deemp);

static const DECLARE_TLV_DB_MINMAX(adau193x_tlv, -9563, 0);

static const unsigned int ad193x_sb[] = {32};

static struct snd_pcm_hw_constraint_list constr = {
	.list = ad193x_sb,
	.count = ARRAY_SIZE(ad193x_sb),
};

static const struct snd_kcontrol_new ad193x_snd_controls[] = {
	 
	SOC_DOUBLE_R_TLV("DAC1 Volume", AD193X_DAC_L1_VOL,
			AD193X_DAC_R1_VOL, 0, 0xFF, 1, adau193x_tlv),
	SOC_DOUBLE_R_TLV("DAC2 Volume", AD193X_DAC_L2_VOL,
			AD193X_DAC_R2_VOL, 0, 0xFF, 1, adau193x_tlv),
	SOC_DOUBLE_R_TLV("DAC3 Volume", AD193X_DAC_L3_VOL,
			AD193X_DAC_R3_VOL, 0, 0xFF, 1, adau193x_tlv),
	SOC_DOUBLE_R_TLV("DAC4 Volume", AD193X_DAC_L4_VOL,
			AD193X_DAC_R4_VOL, 0, 0xFF, 1, adau193x_tlv),

	 
	SOC_DOUBLE("DAC1 Switch", AD193X_DAC_CHNL_MUTE, AD193X_DACL1_MUTE,
		AD193X_DACR1_MUTE, 1, 1),
	SOC_DOUBLE("DAC2 Switch", AD193X_DAC_CHNL_MUTE, AD193X_DACL2_MUTE,
		AD193X_DACR2_MUTE, 1, 1),
	SOC_DOUBLE("DAC3 Switch", AD193X_DAC_CHNL_MUTE, AD193X_DACL3_MUTE,
		AD193X_DACR3_MUTE, 1, 1),
	SOC_DOUBLE("DAC4 Switch", AD193X_DAC_CHNL_MUTE, AD193X_DACL4_MUTE,
		AD193X_DACR4_MUTE, 1, 1),

	 
	SOC_ENUM("Playback Deemphasis", ad193x_deemp_enum),
};

static const struct snd_kcontrol_new ad193x_adc_snd_controls[] = {
	 
	SOC_DOUBLE("ADC1 Switch", AD193X_ADC_CTRL0, AD193X_ADCL1_MUTE,
		AD193X_ADCR1_MUTE, 1, 1),
	SOC_DOUBLE("ADC2 Switch", AD193X_ADC_CTRL0, AD193X_ADCL2_MUTE,
		AD193X_ADCR2_MUTE, 1, 1),

	 
	SOC_SINGLE("ADC High Pass Filter Switch", AD193X_ADC_CTRL0,
			AD193X_ADC_HIGHPASS_FILTER, 1, 0),
};

static const struct snd_soc_dapm_widget ad193x_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "Playback", SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("DAC Output", AD193X_DAC_CTRL0, 0, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLL_PWR", AD193X_PLL_CLK_CTRL0, 0, 1, NULL, 0),
	SND_SOC_DAPM_SUPPLY("SYSCLK", AD193X_PLL_CLK_CTRL0, 7, 0, NULL, 0),
	SND_SOC_DAPM_VMID("VMID"),
	SND_SOC_DAPM_OUTPUT("DAC1OUT"),
	SND_SOC_DAPM_OUTPUT("DAC2OUT"),
	SND_SOC_DAPM_OUTPUT("DAC3OUT"),
	SND_SOC_DAPM_OUTPUT("DAC4OUT"),
};

static const struct snd_soc_dapm_widget ad193x_adc_widgets[] = {
	SND_SOC_DAPM_ADC("ADC", "Capture", SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_SUPPLY("ADC_PWR", AD193X_ADC_CTRL0, 0, 1, NULL, 0),
	SND_SOC_DAPM_INPUT("ADC1IN"),
	SND_SOC_DAPM_INPUT("ADC2IN"),
};

static int ad193x_check_pll(struct snd_soc_dapm_widget *source,
			    struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(source->dapm);
	struct ad193x_priv *ad193x = snd_soc_component_get_drvdata(component);

	return !!ad193x->sysclk;
}

static const struct snd_soc_dapm_route audio_paths[] = {
	{ "DAC", NULL, "SYSCLK" },
	{ "DAC Output", NULL, "DAC" },
	{ "DAC Output", NULL, "VMID" },
	{ "DAC1OUT", NULL, "DAC Output" },
	{ "DAC2OUT", NULL, "DAC Output" },
	{ "DAC3OUT", NULL, "DAC Output" },
	{ "DAC4OUT", NULL, "DAC Output" },
	{ "SYSCLK", NULL, "PLL_PWR", &ad193x_check_pll },
};

static const struct snd_soc_dapm_route ad193x_adc_audio_paths[] = {
	{ "ADC", NULL, "SYSCLK" },
	{ "ADC", NULL, "ADC_PWR" },
	{ "ADC", NULL, "ADC1IN" },
	{ "ADC", NULL, "ADC2IN" },
};

static inline bool ad193x_has_adc(const struct ad193x_priv *ad193x)
{
	switch (ad193x->type) {
	case AD1933:
	case AD1934:
		return false;
	default:
		break;
	}

	return true;
}

 

static int ad193x_mute(struct snd_soc_dai *dai, int mute, int direction)
{
	struct ad193x_priv *ad193x = snd_soc_component_get_drvdata(dai->component);

	if (mute)
		regmap_update_bits(ad193x->regmap, AD193X_DAC_CTRL2,
				    AD193X_DAC_MASTER_MUTE,
				    AD193X_DAC_MASTER_MUTE);
	else
		regmap_update_bits(ad193x->regmap, AD193X_DAC_CTRL2,
				    AD193X_DAC_MASTER_MUTE, 0);

	return 0;
}

static int ad193x_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
			       unsigned int rx_mask, int slots, int width)
{
	struct ad193x_priv *ad193x = snd_soc_component_get_drvdata(dai->component);
	unsigned int channels;

	switch (slots) {
	case 2:
		channels = AD193X_2_CHANNELS;
		break;
	case 4:
		channels = AD193X_4_CHANNELS;
		break;
	case 8:
		channels = AD193X_8_CHANNELS;
		break;
	case 16:
		channels = AD193X_16_CHANNELS;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(ad193x->regmap, AD193X_DAC_CTRL1,
		AD193X_DAC_CHAN_MASK, channels << AD193X_DAC_CHAN_SHFT);
	if (ad193x_has_adc(ad193x))
		regmap_update_bits(ad193x->regmap, AD193X_ADC_CTRL2,
				   AD193X_ADC_CHAN_MASK,
				   channels << AD193X_ADC_CHAN_SHFT);

	return 0;
}

static int ad193x_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct ad193x_priv *ad193x = snd_soc_component_get_drvdata(codec_dai->component);
	unsigned int adc_serfmt = 0;
	unsigned int dac_serfmt = 0;
	unsigned int adc_fmt = 0;
	unsigned int dac_fmt = 0;

	 
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		adc_serfmt |= AD193X_ADC_SERFMT_TDM;
		dac_serfmt |= AD193X_DAC_SERFMT_STEREO;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		adc_serfmt |= AD193X_ADC_SERFMT_AUX;
		dac_serfmt |= AD193X_DAC_SERFMT_TDM;
		break;
	default:
		if (ad193x_has_adc(ad193x))
			return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:  
		break;
	case SND_SOC_DAIFMT_NB_IF:  
		adc_fmt |= AD193X_ADC_LEFT_HIGH;
		dac_fmt |= AD193X_DAC_LEFT_HIGH;
		break;
	case SND_SOC_DAIFMT_IB_NF:  
		adc_fmt |= AD193X_ADC_BCLK_INV;
		dac_fmt |= AD193X_DAC_BCLK_INV;
		break;
	case SND_SOC_DAIFMT_IB_IF:  
		adc_fmt |= AD193X_ADC_LEFT_HIGH;
		adc_fmt |= AD193X_ADC_BCLK_INV;
		dac_fmt |= AD193X_DAC_LEFT_HIGH;
		dac_fmt |= AD193X_DAC_BCLK_INV;
		break;
	default:
		return -EINVAL;
	}

	 
	if (fmt & SND_SOC_DAIFMT_DSP_A)
		dac_fmt ^= AD193X_DAC_LEFT_HIGH;

	switch (fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
	case SND_SOC_DAIFMT_CBP_CFP:
		adc_fmt |= AD193X_ADC_LCR_MASTER;
		adc_fmt |= AD193X_ADC_BCLK_MASTER;
		dac_fmt |= AD193X_DAC_LCR_MASTER;
		dac_fmt |= AD193X_DAC_BCLK_MASTER;
		break;
	case SND_SOC_DAIFMT_CBC_CFP:
		adc_fmt |= AD193X_ADC_LCR_MASTER;
		dac_fmt |= AD193X_DAC_LCR_MASTER;
		break;
	case SND_SOC_DAIFMT_CBP_CFC:
		adc_fmt |= AD193X_ADC_BCLK_MASTER;
		dac_fmt |= AD193X_DAC_BCLK_MASTER;
		break;
	case SND_SOC_DAIFMT_CBC_CFC:
		break;
	default:
		return -EINVAL;
	}

	if (ad193x_has_adc(ad193x)) {
		regmap_update_bits(ad193x->regmap, AD193X_ADC_CTRL1,
				   AD193X_ADC_SERFMT_MASK, adc_serfmt);
		regmap_update_bits(ad193x->regmap, AD193X_ADC_CTRL2,
				   AD193X_ADC_FMT_MASK, adc_fmt);
	}
	regmap_update_bits(ad193x->regmap, AD193X_DAC_CTRL0,
			   AD193X_DAC_SERFMT_MASK, dac_serfmt);
	regmap_update_bits(ad193x->regmap, AD193X_DAC_CTRL1,
		AD193X_DAC_FMT_MASK, dac_fmt);

	return 0;
}

static int ad193x_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *component = codec_dai->component;
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(component);
	struct ad193x_priv *ad193x = snd_soc_component_get_drvdata(component);

	if (clk_id == AD193X_SYSCLK_MCLK) {
		 
		if (dir == SND_SOC_CLOCK_OUT || freq != 24576000)
			return -EINVAL;

		regmap_update_bits(ad193x->regmap, AD193X_PLL_CLK_CTRL1,
				   AD193X_PLL_SRC_MASK,
				   AD193X_PLL_DAC_SRC_MCLK |
				   AD193X_PLL_CLK_SRC_MCLK);

		snd_soc_dapm_sync(dapm);
		return 0;
	}
	switch (freq) {
	case 12288000:
	case 18432000:
	case 24576000:
	case 36864000:
		ad193x->sysclk = freq;
		return 0;
	}
	return -EINVAL;
}

static int ad193x_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	int word_len = 0, master_rate = 0;
	struct snd_soc_component *component = dai->component;
	struct ad193x_priv *ad193x = snd_soc_component_get_drvdata(component);
	bool is_playback = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	u8 dacc0;

	dev_dbg(dai->dev, "%s() rate=%u format=%#x width=%u channels=%u\n",
		__func__, params_rate(params), params_format(params),
		params_width(params), params_channels(params));


	 
	switch (params_width(params)) {
	case 16:
		word_len = 3;
		break;
	case 20:
		word_len = 1;
		break;
	case 24:
	case 32:
		word_len = 0;
		break;
	}

	switch (ad193x->sysclk) {
	case 12288000:
		master_rate = AD193X_PLL_INPUT_256;
		break;
	case 18432000:
		master_rate = AD193X_PLL_INPUT_384;
		break;
	case 24576000:
		master_rate = AD193X_PLL_INPUT_512;
		break;
	case 36864000:
		master_rate = AD193X_PLL_INPUT_768;
		break;
	}

	if (is_playback) {
		switch (params_rate(params)) {
		case 48000:
			dacc0 = AD193X_DAC_SR_48;
			break;
		case 96000:
			dacc0 = AD193X_DAC_SR_96;
			break;
		case 192000:
			dacc0 = AD193X_DAC_SR_192;
			break;
		default:
			dev_err(dai->dev, "invalid sampling rate: %d\n", params_rate(params));
			return -EINVAL;
		}

		regmap_update_bits(ad193x->regmap, AD193X_DAC_CTRL0, AD193X_DAC_SR_MASK, dacc0);
	}

	regmap_update_bits(ad193x->regmap, AD193X_PLL_CLK_CTRL0,
			    AD193X_PLL_INPUT_MASK, master_rate);

	regmap_update_bits(ad193x->regmap, AD193X_DAC_CTRL2,
			    AD193X_DAC_WORD_LEN_MASK,
			    word_len << AD193X_DAC_WORD_LEN_SHFT);

	if (ad193x_has_adc(ad193x))
		regmap_update_bits(ad193x->regmap, AD193X_ADC_CTRL1,
				   AD193X_ADC_WORD_LEN_MASK, word_len);

	return 0;
}

static int ad193x_startup(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	return snd_pcm_hw_constraint_list(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
				   &constr);
}

static const struct snd_soc_dai_ops ad193x_dai_ops = {
	.startup = ad193x_startup,
	.hw_params = ad193x_hw_params,
	.mute_stream = ad193x_mute,
	.set_tdm_slot = ad193x_set_tdm_slot,
	.set_sysclk	= ad193x_set_dai_sysclk,
	.set_fmt = ad193x_set_dai_fmt,
	.no_capture_mute = 1,
};

 
static struct snd_soc_dai_driver ad193x_dai = {
	.name = "ad193x-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 4,
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.ops = &ad193x_dai_ops,
};

 
static struct snd_soc_dai_driver ad193x_no_adc_dai = {
	.name = "ad193x-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000,
		.formats = SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.ops = &ad193x_dai_ops,
};

 
static void ad193x_reg_default_init(struct ad193x_priv *ad193x)
{
	static const struct reg_sequence reg_init[] = {
		{  0, 0x99 },	 
		{  1, 0x04 },	 
		{  2, 0x40 },	 
		{  3, 0x00 },	 
		{  4, 0x1A },	 
		{  5, 0x00 },	 
		{  6, 0x00 },	 
		{  7, 0x00 },	 
		{  8, 0x00 },	 
		{  9, 0x00 },	 
		{ 10, 0x00 },	 
		{ 11, 0x00 },	 
		{ 12, 0x00 },	 
		{ 13, 0x00 },	 
	};
	static const struct reg_sequence reg_adc_init[] = {
		{ 14, 0x03 },	 
		{ 15, 0x43 },	 
		{ 16, 0x00 },	 
	};

	regmap_multi_reg_write(ad193x->regmap, reg_init, ARRAY_SIZE(reg_init));

	if (ad193x_has_adc(ad193x)) {
		regmap_multi_reg_write(ad193x->regmap, reg_adc_init,
				       ARRAY_SIZE(reg_adc_init));
	}
}

static int ad193x_component_probe(struct snd_soc_component *component)
{
	struct ad193x_priv *ad193x = snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(component);
	int num, ret;

	 
	ad193x_reg_default_init(ad193x);

	 
	if (ad193x_has_adc(ad193x)) {
		 
		num = ARRAY_SIZE(ad193x_adc_snd_controls);
		ret = snd_soc_add_component_controls(component,
						 ad193x_adc_snd_controls,
						 num);
		if (ret)
			return ret;

		 
		num = ARRAY_SIZE(ad193x_adc_widgets);
		ret = snd_soc_dapm_new_controls(dapm,
						ad193x_adc_widgets,
						num);
		if (ret)
			return ret;

		 
		num = ARRAY_SIZE(ad193x_adc_audio_paths);
		ret = snd_soc_dapm_add_routes(dapm,
					      ad193x_adc_audio_paths,
					      num);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct snd_soc_component_driver soc_component_dev_ad193x = {
	.probe			= ad193x_component_probe,
	.controls		= ad193x_snd_controls,
	.num_controls		= ARRAY_SIZE(ad193x_snd_controls),
	.dapm_widgets		= ad193x_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(ad193x_dapm_widgets),
	.dapm_routes		= audio_paths,
	.num_dapm_routes	= ARRAY_SIZE(audio_paths),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

const struct regmap_config ad193x_regmap_config = {
	.max_register = AD193X_NUM_REGS - 1,
};
EXPORT_SYMBOL_GPL(ad193x_regmap_config);

int ad193x_probe(struct device *dev, struct regmap *regmap,
		 enum ad193x_type type)
{
	struct ad193x_priv *ad193x;

	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ad193x = devm_kzalloc(dev, sizeof(*ad193x), GFP_KERNEL);
	if (ad193x == NULL)
		return -ENOMEM;

	ad193x->regmap = regmap;
	ad193x->type = type;

	dev_set_drvdata(dev, ad193x);

	if (ad193x_has_adc(ad193x))
		return devm_snd_soc_register_component(dev, &soc_component_dev_ad193x,
						       &ad193x_dai, 1);
	return devm_snd_soc_register_component(dev, &soc_component_dev_ad193x,
		&ad193x_no_adc_dai, 1);
}
EXPORT_SYMBOL_GPL(ad193x_probe);

MODULE_DESCRIPTION("ASoC ad193x driver");
MODULE_AUTHOR("Barry Song <21cnbao@gmail.com>");
MODULE_LICENSE("GPL");
