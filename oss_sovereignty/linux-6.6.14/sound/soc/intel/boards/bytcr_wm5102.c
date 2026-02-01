
 

#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-acpi.h>
#include "../../codecs/wm5102.h"
#include "../atom/sst-atom-controls.h"

#define MCLK_FREQ		25000000

#define WM5102_MAX_SYSCLK_4K	49152000  
#define WM5102_MAX_SYSCLK_11025	45158400  

struct byt_wm5102_private {
	struct snd_soc_jack jack;
	struct clk *mclk;
	struct gpio_desc *spkvdd_en_gpio;
};

static int byt_wm5102_spkvdd_power_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct byt_wm5102_private *priv = snd_soc_card_get_drvdata(card);

	gpiod_set_value_cansleep(priv->spkvdd_en_gpio,
				 !!SND_SOC_DAPM_EVENT_ON(event));

	return 0;
}

static int byt_wm5102_prepare_and_enable_pll1(struct snd_soc_dai *codec_dai, int rate)
{
	struct snd_soc_component *codec_component = codec_dai->component;
	int sr_mult = ((rate % 4000) == 0) ?
		(WM5102_MAX_SYSCLK_4K / rate) :
		(WM5102_MAX_SYSCLK_11025 / rate);
	int ret;

	 
	snd_soc_dai_set_pll(codec_dai, WM5102_FLL1_REFCLK, ARIZONA_FLL_SRC_NONE, 0, 0);
	snd_soc_dai_set_pll(codec_dai, WM5102_FLL1, ARIZONA_FLL_SRC_NONE, 0, 0);

	 
	ret = snd_soc_dai_set_pll(codec_dai, WM5102_FLL1, ARIZONA_CLK_SRC_MCLK1,
				  MCLK_FREQ, rate * sr_mult);
	if (ret) {
		dev_err(codec_component->dev, "Error setting PLL: %d\n", ret);
		return ret;
	}

	ret = snd_soc_component_set_sysclk(codec_component, ARIZONA_CLK_SYSCLK,
					   ARIZONA_CLK_SRC_FLL1, rate * sr_mult,
					   SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(codec_component->dev, "Error setting SYSCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, ARIZONA_CLK_SYSCLK,
				     rate * 512, SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(codec_component->dev, "Error setting clock: %d\n", ret);
		return ret;
	}

	return 0;
}

static int platform_clock_control(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *k, int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct snd_soc_dai *codec_dai;
	struct byt_wm5102_private *priv = snd_soc_card_get_drvdata(card);
	int ret;

	codec_dai = snd_soc_card_get_codec_dai(card, "wm5102-aif1");
	if (!codec_dai) {
		dev_err(card->dev, "Error codec DAI not found\n");
		return -EIO;
	}

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		ret = clk_prepare_enable(priv->mclk);
		if (ret) {
			dev_err(card->dev, "Error enabling MCLK: %d\n", ret);
			return ret;
		}
		ret = byt_wm5102_prepare_and_enable_pll1(codec_dai, 48000);
		if (ret) {
			dev_err(card->dev, "Error setting codec sysclk: %d\n", ret);
			return ret;
		}
	} else {
		 
		snd_soc_dai_set_pll(codec_dai, WM5102_FLL1, ARIZONA_FLL_SRC_NONE, 0, 0);
		clk_disable_unprepare(priv->mclk);
	}

	return 0;
}

static const struct snd_soc_dapm_widget byt_wm5102_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Internal Mic", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
	SND_SOC_DAPM_SUPPLY("Platform Clock", SND_SOC_NOPM, 0, 0,
			    platform_clock_control, SND_SOC_DAPM_PRE_PMU |
			    SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("Speaker VDD", SND_SOC_NOPM, 0, 0,
			    byt_wm5102_spkvdd_power_event,
			    SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMU),
};

static const struct snd_soc_dapm_route byt_wm5102_audio_map[] = {
	{"Headphone", NULL, "Platform Clock"},
	{"Headset Mic", NULL, "Platform Clock"},
	{"Internal Mic", NULL, "Platform Clock"},
	{"Speaker", NULL, "Platform Clock"},
	{"Line Out", NULL, "Platform Clock"},

	{"Speaker", NULL, "SPKOUTLP"},
	{"Speaker", NULL, "SPKOUTLN"},
	{"Speaker", NULL, "SPKOUTRP"},
	{"Speaker", NULL, "SPKOUTRN"},
	{"Speaker", NULL, "Speaker VDD"},

	{"Headphone", NULL, "HPOUT1L"},
	{"Headphone", NULL, "HPOUT1R"},

	{"Internal Mic", NULL, "MICBIAS3"},
	{"IN3L", NULL, "Internal Mic"},

	 
	{"Headset Mic", NULL, "MICBIAS1"},
	{"Headset Mic", NULL, "MICBIAS2"},
	{"IN1L", NULL, "Headset Mic"},

	{"AIF1 Playback", NULL, "ssp0 Tx"},
	{"ssp0 Tx", NULL, "modem_out"},

	{"modem_in", NULL, "ssp0 Rx"},
	{"ssp0 Rx", NULL, "AIF1 Capture"},
};

static const struct snd_kcontrol_new byt_wm5102_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Internal Mic"),
	SOC_DAPM_PIN_SWITCH("Speaker"),
	SOC_DAPM_PIN_SWITCH("Line Out"),
};

static struct snd_soc_jack_pin byt_wm5102_pins[] = {
	{
		.pin	= "Headphone",
		.mask	= SND_JACK_HEADPHONE,
	},
	{
		.pin	= "Headset Mic",
		.mask	= SND_JACK_MICROPHONE,
	},
	{
		.pin	= "Line Out",
		.mask	= SND_JACK_LINEOUT,
	},
};

static int byt_wm5102_init(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_card *card = runtime->card;
	struct byt_wm5102_private *priv = snd_soc_card_get_drvdata(card);
	struct snd_soc_component *component = asoc_rtd_to_codec(runtime, 0)->component;
	int ret, jack_type;

	card->dapm.idle_bias_off = true;

	ret = snd_soc_add_card_controls(card, byt_wm5102_controls,
					ARRAY_SIZE(byt_wm5102_controls));
	if (ret) {
		dev_err(card->dev, "Error adding card controls: %d\n", ret);
		return ret;
	}

	 
	ret = clk_prepare_enable(priv->mclk);
	if (!ret)
		clk_disable_unprepare(priv->mclk);

	ret = clk_set_rate(priv->mclk, MCLK_FREQ);
	if (ret) {
		dev_err(card->dev, "Error setting MCLK rate: %d\n", ret);
		return ret;
	}

	jack_type = ARIZONA_JACK_MASK | SND_JACK_BTN_0 | SND_JACK_BTN_1 |
		    SND_JACK_BTN_2 | SND_JACK_BTN_3;
	ret = snd_soc_card_jack_new_pins(card, "Headset", jack_type,
					 &priv->jack, byt_wm5102_pins,
					 ARRAY_SIZE(byt_wm5102_pins));
	if (ret) {
		dev_err(card->dev, "Error creating jack: %d\n", ret);
		return ret;
	}

	snd_soc_component_set_jack(component, &priv->jack, NULL);

	return 0;
}

static int byt_wm5102_codec_fixup(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
							  SNDRV_PCM_HW_PARAM_CHANNELS);
	int ret;

	 
	rate->min = 48000;
	rate->max = 48000;
	channels->min = 2;
	channels->max = 2;

	 
	params_set_format(params, SNDRV_PCM_FORMAT_S16_LE);

	 
	ret = snd_soc_dai_set_fmt(asoc_rtd_to_cpu(rtd, 0),
				  SND_SOC_DAIFMT_I2S     |
				  SND_SOC_DAIFMT_NB_NF   |
				  SND_SOC_DAIFMT_BP_FP);
	if (ret) {
		dev_err(rtd->dev, "Error setting format to I2S: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_tdm_slot(asoc_rtd_to_cpu(rtd, 0), 0x3, 0x3, 2, 16);
	if (ret) {
		dev_err(rtd->dev, "Error setting I2S config: %d\n", ret);
		return ret;
	}

	return 0;
}

static int byt_wm5102_aif1_startup(struct snd_pcm_substream *substream)
{
	return snd_pcm_hw_constraint_single(substream->runtime,
					    SNDRV_PCM_HW_PARAM_RATE, 48000);
}

static const struct snd_soc_ops byt_wm5102_aif1_ops = {
	.startup = byt_wm5102_aif1_startup,
};

SND_SOC_DAILINK_DEF(dummy,
	DAILINK_COMP_ARRAY(COMP_DUMMY()));

SND_SOC_DAILINK_DEF(media,
	DAILINK_COMP_ARRAY(COMP_CPU("media-cpu-dai")));

SND_SOC_DAILINK_DEF(deepbuffer,
	DAILINK_COMP_ARRAY(COMP_CPU("deepbuffer-cpu-dai")));

SND_SOC_DAILINK_DEF(ssp0_port,
	DAILINK_COMP_ARRAY(COMP_CPU("ssp0-port")));

SND_SOC_DAILINK_DEF(ssp0_codec,
	DAILINK_COMP_ARRAY(COMP_CODEC(
	 
	"wm5102-codec",
	"wm5102-aif1")));

SND_SOC_DAILINK_DEF(platform,
	DAILINK_COMP_ARRAY(COMP_PLATFORM("sst-mfld-platform")));

static struct snd_soc_dai_link byt_wm5102_dais[] = {
	[MERR_DPCM_AUDIO] = {
		.name = "Baytrail Audio Port",
		.stream_name = "Baytrail Audio",
		.nonatomic = true,
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ops = &byt_wm5102_aif1_ops,
		SND_SOC_DAILINK_REG(media, dummy, platform),

	},
	[MERR_DPCM_DEEP_BUFFER] = {
		.name = "Deep-Buffer Audio Port",
		.stream_name = "Deep-Buffer Audio",
		.nonatomic = true,
		.dynamic = 1,
		.dpcm_playback = 1,
		.ops = &byt_wm5102_aif1_ops,
		SND_SOC_DAILINK_REG(deepbuffer, dummy, platform),
	},
		 
	{
		 
		.name = "SSP2-Codec",
		.id = 0,
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF
						| SND_SOC_DAIFMT_CBC_CFC,
		.be_hw_params_fixup = byt_wm5102_codec_fixup,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = byt_wm5102_init,
		SND_SOC_DAILINK_REG(ssp0_port, ssp0_codec, platform),
	},
};

 
#define SOF_CARD_NAME "bytcht wm5102"  
#define SOF_DRIVER_NAME "SOF"

#define CARD_NAME "bytcr-wm5102"
#define DRIVER_NAME NULL  

 
static struct snd_soc_card byt_wm5102_card = {
	.owner = THIS_MODULE,
	.dai_link = byt_wm5102_dais,
	.num_links = ARRAY_SIZE(byt_wm5102_dais),
	.dapm_widgets = byt_wm5102_widgets,
	.num_dapm_widgets = ARRAY_SIZE(byt_wm5102_widgets),
	.dapm_routes = byt_wm5102_audio_map,
	.num_dapm_routes = ARRAY_SIZE(byt_wm5102_audio_map),
	.fully_routed = true,
};

static int snd_byt_wm5102_mc_probe(struct platform_device *pdev)
{
	char codec_name[SND_ACPI_I2C_ID_LEN];
	struct device *dev = &pdev->dev;
	struct byt_wm5102_private *priv;
	struct snd_soc_acpi_mach *mach;
	const char *platform_name;
	struct acpi_device *adev;
	struct device *codec_dev;
	bool sof_parent;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	 
	priv->mclk = devm_clk_get(dev, "pmc_plt_clk_3");
	if (IS_ERR(priv->mclk))
		return dev_err_probe(dev, PTR_ERR(priv->mclk), "getting pmc_plt_clk_3\n");

	 
	mach = dev->platform_data;
	adev = acpi_dev_get_first_match_dev(mach->id, NULL, -1);
	if (!adev) {
		dev_err(dev, "Error cannot find acpi-dev for codec\n");
		return -ENOENT;
	}
	snprintf(codec_name, sizeof(codec_name), "spi-%s", acpi_dev_name(adev));

	codec_dev = bus_find_device_by_name(&spi_bus_type, NULL, codec_name);
	acpi_dev_put(adev);
	if (!codec_dev)
		return -EPROBE_DEFER;

	 
	priv->spkvdd_en_gpio = gpiod_get(codec_dev, "wlf,spkvdd-ena", GPIOD_OUT_LOW);
	put_device(codec_dev);

	if (IS_ERR(priv->spkvdd_en_gpio)) {
		ret = PTR_ERR(priv->spkvdd_en_gpio);
		 
		if (ret == -ENOENT)
			ret = -EPROBE_DEFER;

		return dev_err_probe(dev, ret, "getting spkvdd-GPIO\n");
	}

	 
	byt_wm5102_card.dev = dev;
	platform_name = mach->mach_params.platform;
	ret = snd_soc_fixup_dai_links_platform_name(&byt_wm5102_card, platform_name);
	if (ret)
		goto out_put_gpio;

	 
	sof_parent = snd_soc_acpi_sof_parent(dev);
	if (sof_parent) {
		byt_wm5102_card.name = SOF_CARD_NAME;
		byt_wm5102_card.driver_name = SOF_DRIVER_NAME;
		dev->driver->pm = &snd_soc_pm_ops;
	} else {
		byt_wm5102_card.name = CARD_NAME;
		byt_wm5102_card.driver_name = DRIVER_NAME;
	}

	snd_soc_card_set_drvdata(&byt_wm5102_card, priv);
	ret = devm_snd_soc_register_card(dev, &byt_wm5102_card);
	if (ret) {
		dev_err_probe(dev, ret, "registering card\n");
		goto out_put_gpio;
	}

	platform_set_drvdata(pdev, &byt_wm5102_card);
	return 0;

out_put_gpio:
	gpiod_put(priv->spkvdd_en_gpio);
	return ret;
}

static void snd_byt_wm5102_mc_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct byt_wm5102_private *priv = snd_soc_card_get_drvdata(card);

	gpiod_put(priv->spkvdd_en_gpio);
}

static struct platform_driver snd_byt_wm5102_mc_driver = {
	.driver = {
		.name = "bytcr_wm5102",
	},
	.probe = snd_byt_wm5102_mc_probe,
	.remove_new = snd_byt_wm5102_mc_remove,
};

module_platform_driver(snd_byt_wm5102_mc_driver);

MODULE_DESCRIPTION("ASoC Baytrail with WM5102 codec machine driver");
MODULE_AUTHOR("Hans de Goede <hdegoede@redhat.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:bytcr_wm5102");
