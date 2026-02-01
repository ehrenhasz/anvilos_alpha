













#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/atomic.h>
#include <linux/workqueue.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>

 
#define REG_PAGE		0x00
#define REG_DEVICE_CTRL_1	0x02
#define REG_DEVICE_CTRL_2	0x03
#define REG_SIG_CH_CTRL		0x28
#define REG_SAP_CTRL_1		0x33
#define REG_FS_MON		0x37
#define REG_BCK_MON		0x38
#define REG_CLKDET_STATUS	0x39
#define REG_VOL_CTL		0x4c
#define REG_AGAIN		0x54
#define REG_ADR_PIN_CTRL	0x60
#define REG_ADR_PIN_CONFIG	0x61
#define REG_CHAN_FAULT		0x70
#define REG_GLOBAL_FAULT1	0x71
#define REG_GLOBAL_FAULT2	0x72
#define REG_FAULT		0x78
#define REG_BOOK		0x7f

 
#define DCTRL2_MODE_DEEP_SLEEP	0x00
#define DCTRL2_MODE_SLEEP	0x01
#define DCTRL2_MODE_HIZ		0x02
#define DCTRL2_MODE_PLAY	0x03

#define DCTRL2_MUTE		0x08
#define DCTRL2_DIS_DSP		0x10

 
static const uint8_t dsp_cfg_preboot[] = {
	0x00, 0x00, 0x7f, 0x00, 0x03, 0x02, 0x01, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x7f, 0x00, 0x03, 0x02,
};

static const uint32_t tas5805m_volume[] = {
	0x0000001B,   0x0000001E,  
	0x00000021,   0x00000025,  
	0x0000002A,   0x0000002F,  
	0x00000035,   0x0000003B,  
	0x00000043,   0x0000004B,  
	0x00000054,   0x0000005E,  
	0x0000006A,   0x00000076,  
	0x00000085,   0x00000095,  
	0x000000A7,   0x000000BC,  
	0x000000D3,   0x000000EC,  
	0x00000109,   0x0000012A,  
	0x0000014E,   0x00000177,  
	0x000001A4,   0x000001D8,  
	0x00000211,   0x00000252,  
	0x0000029A,   0x000002EC,  
	0x00000347,   0x000003AD,  
	0x00000420,   0x000004A1,  
	0x00000532,   0x000005D4,  
	0x0000068A,   0x00000756,  
	0x0000083B,   0x0000093C,  
	0x00000A5D,   0x00000BA0,  
	0x00000D0C,   0x00000EA3,  
	0x0000106C,   0x0000126D,  
	0x000014AD,   0x00001733,  
	0x00001A07,   0x00001D34,  
	0x000020C5,   0x000024C4,  
	0x00002941,   0x00002E49,  
	0x000033EF,   0x00003A45,  
	0x00004161,   0x0000495C,  
	0x0000524F,   0x00005C5A,  
	0x0000679F,   0x00007444,  
	0x00008274,   0x0000925F,  
	0x0000A43B,   0x0000B845,  
	0x0000CEC1,   0x0000E7FB,  
	0x00010449,   0x0001240C,  
	0x000147AE,   0x00016FAA,  
	0x00019C86,   0x0001CEDC,  
	0x00020756,   0x000246B5,  
	0x00028DCF,   0x0002DD96,  
	0x00033718,   0x00039B87,  
	0x00040C37,   0x00048AA7,  
	0x00051884,   0x0005B7B1,  
	0x00066A4A,   0x000732AE,  
	0x00081385,   0x00090FCC,  
	0x000A2ADB,   0x000B6873,  
	0x000CCCCD,   0x000E5CA1,  
	0x00101D3F,   0x0012149A,  
	0x00144961,   0x0016C311,  
	0x00198A13,   0x001CA7D7,  
	0x002026F3,   0x00241347,  
	0x00287A27,   0x002D6A86,  
	0x0032F52D,   0x00392CEE,  
	0x004026E7,   0x0047FACD,  
	0x0050C336,   0x005A9DF8,  
	0x0065AC8C,   0x00721483,  
	0x00800000,   0x008F9E4D,  
	0x00A12478,   0x00B4CE08,  
	0x00CADDC8,   0x00E39EA9,  
	0x00FF64C1,   0x011E8E6A,  
	0x0141857F,   0x0168C0C6,  
	0x0194C584,   0x01C62940,  
	0x01FD93C2,   0x023BC148,  
	0x02818508,   0x02CFCC01,  
	0x0327A01A,   0x038A2BAD,  
	0x03F8BD7A,   0x0474CD1B,  
	0x05000000,   0x059C2F02,  
	0x064B6CAE,   0x07100C4D,  
	0x07ECA9CD,   0x08E43299,  
	0x09F9EF8E,   0x0B319025,  
	0x0C8F36F2,   0x0E1787B8,  
	0x0FCFB725,   0x11BD9C84,  
	0x13E7C594,   0x16558CCB,  
	0x190F3254,   0x1C1DF80E,  
	0x1F8C4107,   0x2365B4BF,  
	0x27B766C2,   0x2C900313,  
	0x32000000,   0x3819D612,  
	0x3EF23ECA,   0x46A07B07,  
	0x4F3EA203,   0x58E9F9F9,  
	0x63C35B8E,   0x6FEFA16D,  
	0x7D982575,  
};

#define TAS5805M_VOLUME_MAX	((int)ARRAY_SIZE(tas5805m_volume) - 1)
#define TAS5805M_VOLUME_MIN	0

struct tas5805m_priv {
	struct i2c_client		*i2c;
	struct regulator		*pvdd;
	struct gpio_desc		*gpio_pdn_n;

	uint8_t				*dsp_cfg_data;
	int				dsp_cfg_len;

	struct regmap			*regmap;

	int				vol[2];
	bool				is_powered;
	bool				is_muted;

	struct work_struct		work;
	struct mutex			lock;
};

static void set_dsp_scale(struct regmap *rm, int offset, int vol)
{
	uint8_t v[4];
	uint32_t x = tas5805m_volume[vol];
	int i;

	for (i = 0; i < 4; i++) {
		v[3 - i] = x;
		x >>= 8;
	}

	regmap_bulk_write(rm, offset, v, ARRAY_SIZE(v));
}

static void tas5805m_refresh(struct tas5805m_priv *tas5805m)
{
	struct regmap *rm = tas5805m->regmap;

	dev_dbg(&tas5805m->i2c->dev, "refresh: is_muted=%d, vol=%d/%d\n",
		tas5805m->is_muted, tas5805m->vol[0], tas5805m->vol[1]);

	regmap_write(rm, REG_PAGE, 0x00);
	regmap_write(rm, REG_BOOK, 0x8c);
	regmap_write(rm, REG_PAGE, 0x2a);

	 
	set_dsp_scale(rm, 0x24, tas5805m->vol[0]);
	set_dsp_scale(rm, 0x28, tas5805m->vol[1]);

	regmap_write(rm, REG_PAGE, 0x00);
	regmap_write(rm, REG_BOOK, 0x00);

	 
	regmap_write(rm, REG_DEVICE_CTRL_2,
		(tas5805m->is_muted ? DCTRL2_MUTE : 0) |
		DCTRL2_MODE_PLAY);
}

static int tas5805m_vol_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;

	uinfo->value.integer.min = TAS5805M_VOLUME_MIN;
	uinfo->value.integer.max = TAS5805M_VOLUME_MAX;
	return 0;
}

static int tas5805m_vol_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tas5805m_priv *tas5805m =
		snd_soc_component_get_drvdata(component);

	mutex_lock(&tas5805m->lock);
	ucontrol->value.integer.value[0] = tas5805m->vol[0];
	ucontrol->value.integer.value[1] = tas5805m->vol[1];
	mutex_unlock(&tas5805m->lock);

	return 0;
}

static inline int volume_is_valid(int v)
{
	return (v >= TAS5805M_VOLUME_MIN) && (v <= TAS5805M_VOLUME_MAX);
}

static int tas5805m_vol_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct tas5805m_priv *tas5805m =
		snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (!(volume_is_valid(ucontrol->value.integer.value[0]) &&
	      volume_is_valid(ucontrol->value.integer.value[1])))
		return -EINVAL;

	mutex_lock(&tas5805m->lock);
	if (tas5805m->vol[0] != ucontrol->value.integer.value[0] ||
	    tas5805m->vol[1] != ucontrol->value.integer.value[1]) {
		tas5805m->vol[0] = ucontrol->value.integer.value[0];
		tas5805m->vol[1] = ucontrol->value.integer.value[1];
		dev_dbg(component->dev, "set vol=%d/%d (is_powered=%d)\n",
			tas5805m->vol[0], tas5805m->vol[1],
			tas5805m->is_powered);
		if (tas5805m->is_powered)
			tas5805m_refresh(tas5805m);
		ret = 1;
	}
	mutex_unlock(&tas5805m->lock);

	return ret;
}

static const struct snd_kcontrol_new tas5805m_snd_controls[] = {
	{
		.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name	= "Master Playback Volume",
		.access	= SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			  SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info	= tas5805m_vol_info,
		.get	= tas5805m_vol_get,
		.put	= tas5805m_vol_put,
	},
};

static void send_cfg(struct regmap *rm,
		     const uint8_t *s, unsigned int len)
{
	unsigned int i;

	for (i = 0; i + 1 < len; i += 2)
		regmap_write(rm, s[i], s[i + 1]);
}

 
static int tas5805m_trigger(struct snd_pcm_substream *substream, int cmd,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct tas5805m_priv *tas5805m =
		snd_soc_component_get_drvdata(component);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dev_dbg(component->dev, "clock start\n");
		schedule_work(&tas5805m->work);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static void do_work(struct work_struct *work)
{
	struct tas5805m_priv *tas5805m =
	       container_of(work, struct tas5805m_priv, work);
	struct regmap *rm = tas5805m->regmap;

	dev_dbg(&tas5805m->i2c->dev, "DSP startup\n");

	mutex_lock(&tas5805m->lock);
	 
	usleep_range(5000, 10000);
	send_cfg(rm, dsp_cfg_preboot, ARRAY_SIZE(dsp_cfg_preboot));
	usleep_range(5000, 15000);
	send_cfg(rm, tas5805m->dsp_cfg_data, tas5805m->dsp_cfg_len);

	tas5805m->is_powered = true;
	tas5805m_refresh(tas5805m);
	mutex_unlock(&tas5805m->lock);
}

static int tas5805m_dac_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct tas5805m_priv *tas5805m =
		snd_soc_component_get_drvdata(component);
	struct regmap *rm = tas5805m->regmap;

	if (event & SND_SOC_DAPM_PRE_PMD) {
		unsigned int chan, global1, global2;

		dev_dbg(component->dev, "DSP shutdown\n");
		cancel_work_sync(&tas5805m->work);

		mutex_lock(&tas5805m->lock);
		if (tas5805m->is_powered) {
			tas5805m->is_powered = false;

			regmap_write(rm, REG_PAGE, 0x00);
			regmap_write(rm, REG_BOOK, 0x00);

			regmap_read(rm, REG_CHAN_FAULT, &chan);
			regmap_read(rm, REG_GLOBAL_FAULT1, &global1);
			regmap_read(rm, REG_GLOBAL_FAULT2, &global2);

			dev_dbg(component->dev, "fault regs: CHAN=%02x, "
				"GLOBAL1=%02x, GLOBAL2=%02x\n",
				chan, global1, global2);

			regmap_write(rm, REG_DEVICE_CTRL_2, DCTRL2_MODE_HIZ);
		}
		mutex_unlock(&tas5805m->lock);
	}

	return 0;
}

static const struct snd_soc_dapm_route tas5805m_audio_map[] = {
	{ "DAC", NULL, "DAC IN" },
	{ "OUT", NULL, "DAC" },
};

static const struct snd_soc_dapm_widget tas5805m_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DAC IN", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC_E("DAC", NULL, SND_SOC_NOPM, 0, 0,
		tas5805m_dac_event, SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("OUT")
};

static const struct snd_soc_component_driver soc_codec_dev_tas5805m = {
	.controls		= tas5805m_snd_controls,
	.num_controls		= ARRAY_SIZE(tas5805m_snd_controls),
	.dapm_widgets		= tas5805m_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(tas5805m_dapm_widgets),
	.dapm_routes		= tas5805m_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(tas5805m_audio_map),
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

static int tas5805m_mute(struct snd_soc_dai *dai, int mute, int direction)
{
	struct snd_soc_component *component = dai->component;
	struct tas5805m_priv *tas5805m =
		snd_soc_component_get_drvdata(component);

	mutex_lock(&tas5805m->lock);
	dev_dbg(component->dev, "set mute=%d (is_powered=%d)\n",
		mute, tas5805m->is_powered);

	tas5805m->is_muted = mute;
	if (tas5805m->is_powered)
		tas5805m_refresh(tas5805m);
	mutex_unlock(&tas5805m->lock);

	return 0;
}

static const struct snd_soc_dai_ops tas5805m_dai_ops = {
	.trigger		= tas5805m_trigger,
	.mute_stream		= tas5805m_mute,
	.no_capture_mute	= 1,
};

static struct snd_soc_dai_driver tas5805m_dai = {
	.name		= "tas5805m-amplifier",
	.playback	= {
		.stream_name	= "Playback",
		.channels_min	= 2,
		.channels_max	= 2,
		.rates		= SNDRV_PCM_RATE_48000,
		.formats	= SNDRV_PCM_FMTBIT_S32_LE,
	},
	.ops		= &tas5805m_dai_ops,
};

static const struct regmap_config tas5805m_regmap = {
	.reg_bits	= 8,
	.val_bits	= 8,

	 
	.cache_type	= REGCACHE_NONE,
};

static int tas5805m_i2c_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct regmap *regmap;
	struct tas5805m_priv *tas5805m;
	char filename[128];
	const char *config_name;
	const struct firmware *fw;
	int ret;

	regmap = devm_regmap_init_i2c(i2c, &tas5805m_regmap);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "unable to allocate register map: %d\n", ret);
		return ret;
	}

	tas5805m = devm_kzalloc(dev, sizeof(struct tas5805m_priv), GFP_KERNEL);
	if (!tas5805m)
		return -ENOMEM;

	tas5805m->i2c = i2c;
	tas5805m->pvdd = devm_regulator_get(dev, "pvdd");
	if (IS_ERR(tas5805m->pvdd)) {
		dev_err(dev, "failed to get pvdd supply: %ld\n",
			PTR_ERR(tas5805m->pvdd));
		return PTR_ERR(tas5805m->pvdd);
	}

	dev_set_drvdata(dev, tas5805m);
	tas5805m->regmap = regmap;
	tas5805m->gpio_pdn_n = devm_gpiod_get(dev, "pdn", GPIOD_OUT_LOW);
	if (IS_ERR(tas5805m->gpio_pdn_n)) {
		dev_err(dev, "error requesting PDN gpio: %ld\n",
			PTR_ERR(tas5805m->gpio_pdn_n));
		return PTR_ERR(tas5805m->gpio_pdn_n);
	}

	 
	if (device_property_read_string(dev, "ti,dsp-config-name",
					&config_name))
		config_name = "default";

	snprintf(filename, sizeof(filename), "tas5805m_dsp_%s.bin",
		 config_name);
	ret = request_firmware(&fw, filename, dev);
	if (ret)
		return ret;

	if ((fw->size < 2) || (fw->size & 1)) {
		dev_err(dev, "firmware is invalid\n");
		release_firmware(fw);
		return -EINVAL;
	}

	tas5805m->dsp_cfg_len = fw->size;
	tas5805m->dsp_cfg_data = devm_kmemdup(dev, fw->data, fw->size, GFP_KERNEL);
	if (!tas5805m->dsp_cfg_data) {
		release_firmware(fw);
		return -ENOMEM;
	}

	release_firmware(fw);

	 
	tas5805m->vol[0] = TAS5805M_VOLUME_MIN;
	tas5805m->vol[1] = TAS5805M_VOLUME_MIN;

	ret = regulator_enable(tas5805m->pvdd);
	if (ret < 0) {
		dev_err(dev, "failed to enable pvdd: %d\n", ret);
		return ret;
	}

	usleep_range(100000, 150000);
	gpiod_set_value(tas5805m->gpio_pdn_n, 1);
	usleep_range(10000, 15000);

	INIT_WORK(&tas5805m->work, do_work);
	mutex_init(&tas5805m->lock);

	 
	ret = snd_soc_register_component(dev, &soc_codec_dev_tas5805m,
					 &tas5805m_dai, 1);
	if (ret < 0) {
		dev_err(dev, "unable to register codec: %d\n", ret);
		gpiod_set_value(tas5805m->gpio_pdn_n, 0);
		regulator_disable(tas5805m->pvdd);
		return ret;
	}

	return 0;
}

static void tas5805m_i2c_remove(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct tas5805m_priv *tas5805m = dev_get_drvdata(dev);

	cancel_work_sync(&tas5805m->work);
	snd_soc_unregister_component(dev);
	gpiod_set_value(tas5805m->gpio_pdn_n, 0);
	usleep_range(10000, 15000);
	regulator_disable(tas5805m->pvdd);
}

static const struct i2c_device_id tas5805m_i2c_id[] = {
	{ "tas5805m", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas5805m_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id tas5805m_of_match[] = {
	{ .compatible = "ti,tas5805m", },
	{ }
};
MODULE_DEVICE_TABLE(of, tas5805m_of_match);
#endif

static struct i2c_driver tas5805m_i2c_driver = {
	.probe		= tas5805m_i2c_probe,
	.remove		= tas5805m_i2c_remove,
	.id_table	= tas5805m_i2c_id,
	.driver		= {
		.name		= "tas5805m",
		.of_match_table = of_match_ptr(tas5805m_of_match),
	},
};

module_i2c_driver(tas5805m_i2c_driver);

MODULE_AUTHOR("Andy Liu <andy-liu@ti.com>");
MODULE_AUTHOR("Daniel Beer <daniel.beer@igorinstitute.com>");
MODULE_DESCRIPTION("TAS5805M Audio Amplifier Driver");
MODULE_LICENSE("GPL v2");
