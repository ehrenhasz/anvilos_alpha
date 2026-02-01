
 

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mfd/twl.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

 
#include <linux/mfd/twl4030-audio.h>

 
#define TWL4030_PMBR1_REG		0x0D
 
#define TWL4030_GPIO6_PWM0_MUTE(value)	((value & 0x03) << 2)

#define TWL4030_CACHEREGNUM	(TWL4030_REG_MISC_SET_2 + 1)

struct twl4030_board_params {
	unsigned int digimic_delay;  
	unsigned int ramp_delay_value;
	unsigned int offset_cncl_path;
	unsigned int hs_extmute:1;
	int hs_extmute_gpio;
};

 
struct twl4030_priv {
	unsigned int codec_powered;

	 
	unsigned int apll_enabled;

	struct snd_pcm_substream *master_substream;
	struct snd_pcm_substream *slave_substream;

	unsigned int configured;
	unsigned int rate;
	unsigned int sample_bits;
	unsigned int channels;

	unsigned int sysclk;

	 
	u8 hsl_enabled, hsr_enabled;
	u8 earpiece_enabled;
	u8 predrivel_enabled, predriver_enabled;
	u8 carkitl_enabled, carkitr_enabled;
	u8 ctl_cache[TWL4030_REG_PRECKR_CTL - TWL4030_REG_EAR_CTL + 1];

	struct twl4030_board_params *board_params;
};

static void tw4030_init_ctl_cache(struct twl4030_priv *twl4030)
{
	int i;
	u8 byte;

	for (i = TWL4030_REG_EAR_CTL; i <= TWL4030_REG_PRECKR_CTL; i++) {
		twl_i2c_read_u8(TWL4030_MODULE_AUDIO_VOICE, &byte, i);
		twl4030->ctl_cache[i - TWL4030_REG_EAR_CTL] = byte;
	}
}

static unsigned int twl4030_read(struct snd_soc_component *component, unsigned int reg)
{
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	u8 value = 0;

	if (reg >= TWL4030_CACHEREGNUM)
		return -EIO;

	switch (reg) {
	case TWL4030_REG_EAR_CTL:
	case TWL4030_REG_PREDL_CTL:
	case TWL4030_REG_PREDR_CTL:
	case TWL4030_REG_PRECKL_CTL:
	case TWL4030_REG_PRECKR_CTL:
	case TWL4030_REG_HS_GAIN_SET:
		value = twl4030->ctl_cache[reg - TWL4030_REG_EAR_CTL];
		break;
	default:
		twl_i2c_read_u8(TWL4030_MODULE_AUDIO_VOICE, &value, reg);
		break;
	}

	return value;
}

static bool twl4030_can_write_to_chip(struct twl4030_priv *twl4030,
				      unsigned int reg)
{
	bool write_to_reg = false;

	 
	switch (reg) {
	case TWL4030_REG_EAR_CTL:
		if (twl4030->earpiece_enabled)
			write_to_reg = true;
		break;
	case TWL4030_REG_PREDL_CTL:
		if (twl4030->predrivel_enabled)
			write_to_reg = true;
		break;
	case TWL4030_REG_PREDR_CTL:
		if (twl4030->predriver_enabled)
			write_to_reg = true;
		break;
	case TWL4030_REG_PRECKL_CTL:
		if (twl4030->carkitl_enabled)
			write_to_reg = true;
		break;
	case TWL4030_REG_PRECKR_CTL:
		if (twl4030->carkitr_enabled)
			write_to_reg = true;
		break;
	case TWL4030_REG_HS_GAIN_SET:
		if (twl4030->hsl_enabled || twl4030->hsr_enabled)
			write_to_reg = true;
		break;
	default:
		 
		write_to_reg = true;
		break;
	}

	return write_to_reg;
}

static int twl4030_write(struct snd_soc_component *component, unsigned int reg,
			 unsigned int value)
{
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);

	 
	switch (reg) {
	case TWL4030_REG_EAR_CTL:
	case TWL4030_REG_PREDL_CTL:
	case TWL4030_REG_PREDR_CTL:
	case TWL4030_REG_PRECKL_CTL:
	case TWL4030_REG_PRECKR_CTL:
	case TWL4030_REG_HS_GAIN_SET:
		twl4030->ctl_cache[reg - TWL4030_REG_EAR_CTL] = value;
		break;
	default:
		break;
	}

	if (twl4030_can_write_to_chip(twl4030, reg))
		return twl_i2c_write_u8(TWL4030_MODULE_AUDIO_VOICE, value, reg);

	return 0;
}

static inline void twl4030_wait_ms(int time)
{
	if (time < 60) {
		time *= 1000;
		usleep_range(time, time + 500);
	} else {
		msleep(time);
	}
}

static void twl4030_codec_enable(struct snd_soc_component *component, int enable)
{
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	int mode;

	if (enable == twl4030->codec_powered)
		return;

	if (enable)
		mode = twl4030_audio_enable_resource(TWL4030_AUDIO_RES_POWER);
	else
		mode = twl4030_audio_disable_resource(TWL4030_AUDIO_RES_POWER);

	if (mode >= 0)
		twl4030->codec_powered = enable;

	 
	 
	udelay(10);
}

static void
twl4030_get_board_param_values(struct twl4030_board_params *board_params,
			       struct device_node *node)
{
	int value;

	of_property_read_u32(node, "ti,digimic_delay", &board_params->digimic_delay);
	of_property_read_u32(node, "ti,ramp_delay_value", &board_params->ramp_delay_value);
	of_property_read_u32(node, "ti,offset_cncl_path", &board_params->offset_cncl_path);
	if (!of_property_read_u32(node, "ti,hs_extmute", &value))
		board_params->hs_extmute = value;

	board_params->hs_extmute_gpio = of_get_named_gpio(node, "ti,hs_extmute_gpio", 0);
	if (gpio_is_valid(board_params->hs_extmute_gpio))
		board_params->hs_extmute = 1;
}

static struct twl4030_board_params*
twl4030_get_board_params(struct snd_soc_component *component)
{
	struct twl4030_board_params *board_params = NULL;
	struct device_node *twl4030_codec_node = NULL;

	twl4030_codec_node = of_get_child_by_name(component->dev->parent->of_node,
						  "codec");

	if (twl4030_codec_node) {
		board_params = devm_kzalloc(component->dev,
					    sizeof(struct twl4030_board_params),
					    GFP_KERNEL);
		if (!board_params) {
			of_node_put(twl4030_codec_node);
			return NULL;
		}
		twl4030_get_board_param_values(board_params, twl4030_codec_node);
		of_node_put(twl4030_codec_node);
	}

	return board_params;
}

static void twl4030_init_chip(struct snd_soc_component *component)
{
	struct twl4030_board_params *board_params;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	u8 reg, byte;
	int i = 0;

	board_params = twl4030_get_board_params(component);

	if (board_params && board_params->hs_extmute) {
		if (gpio_is_valid(board_params->hs_extmute_gpio)) {
			int ret;

			if (!board_params->hs_extmute_gpio)
				dev_warn(component->dev,
					"Extmute GPIO is 0 is this correct?\n");

			ret = gpio_request_one(board_params->hs_extmute_gpio,
					       GPIOF_OUT_INIT_LOW,
					       "hs_extmute");
			if (ret) {
				dev_err(component->dev,
					"Failed to get hs_extmute GPIO\n");
				board_params->hs_extmute_gpio = -1;
			}
		} else {
			u8 pin_mux;

			 
			twl_i2c_read_u8(TWL4030_MODULE_INTBR, &pin_mux,
					TWL4030_PMBR1_REG);
			pin_mux &= ~TWL4030_GPIO6_PWM0_MUTE(0x03);
			pin_mux |= TWL4030_GPIO6_PWM0_MUTE(0x02);
			twl_i2c_write_u8(TWL4030_MODULE_INTBR, pin_mux,
					 TWL4030_PMBR1_REG);
		}
	}

	 
	tw4030_init_ctl_cache(twl4030);

	 
	reg = twl4030_read(component, TWL4030_REG_MISC_SET_1);
	twl4030_write(component, TWL4030_REG_MISC_SET_1,
		      reg | TWL4030_SMOOTH_ANAVOL_EN);

	twl4030_write(component, TWL4030_REG_OPTION,
		      TWL4030_ATXL1_EN | TWL4030_ATXR1_EN |
		      TWL4030_ARXL2_EN | TWL4030_ARXR2_EN);

	 
	twl4030_write(component, TWL4030_REG_ARXR2_APGA_CTL, 0x32);

	 
	if (!board_params)
		return;

	twl4030->board_params = board_params;

	reg = twl4030_read(component, TWL4030_REG_HS_POPN_SET);
	reg &= ~TWL4030_RAMP_DELAY;
	reg |= (board_params->ramp_delay_value << 2);
	twl4030_write(component, TWL4030_REG_HS_POPN_SET, reg);

	 
	twl4030_codec_enable(component, 1);

	reg = twl4030_read(component, TWL4030_REG_ANAMICL);
	reg &= ~TWL4030_OFFSET_CNCL_SEL;
	reg |= board_params->offset_cncl_path;
	twl4030_write(component, TWL4030_REG_ANAMICL,
		      reg | TWL4030_CNCL_OFFSET_START);

	 
	msleep(20);
	do {
		usleep_range(1000, 2000);
		twl_set_regcache_bypass(TWL4030_MODULE_AUDIO_VOICE, true);
		twl_i2c_read_u8(TWL4030_MODULE_AUDIO_VOICE, &byte,
				TWL4030_REG_ANAMICL);
		twl_set_regcache_bypass(TWL4030_MODULE_AUDIO_VOICE, false);
	} while ((i++ < 100) &&
		 ((byte & TWL4030_CNCL_OFFSET_START) ==
		  TWL4030_CNCL_OFFSET_START));

	twl4030_codec_enable(component, 0);
}

static void twl4030_apll_enable(struct snd_soc_component *component, int enable)
{
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);

	if (enable) {
		twl4030->apll_enabled++;
		if (twl4030->apll_enabled == 1)
			twl4030_audio_enable_resource(
							TWL4030_AUDIO_RES_APLL);
	} else {
		twl4030->apll_enabled--;
		if (!twl4030->apll_enabled)
			twl4030_audio_disable_resource(
							TWL4030_AUDIO_RES_APLL);
	}
}

 
static const struct snd_kcontrol_new twl4030_dapm_earpiece_controls[] = {
	SOC_DAPM_SINGLE("Voice", TWL4030_REG_EAR_CTL, 0, 1, 0),
	SOC_DAPM_SINGLE("AudioL1", TWL4030_REG_EAR_CTL, 1, 1, 0),
	SOC_DAPM_SINGLE("AudioL2", TWL4030_REG_EAR_CTL, 2, 1, 0),
	SOC_DAPM_SINGLE("AudioR1", TWL4030_REG_EAR_CTL, 3, 1, 0),
};

 
static const struct snd_kcontrol_new twl4030_dapm_predrivel_controls[] = {
	SOC_DAPM_SINGLE("Voice", TWL4030_REG_PREDL_CTL, 0, 1, 0),
	SOC_DAPM_SINGLE("AudioL1", TWL4030_REG_PREDL_CTL, 1, 1, 0),
	SOC_DAPM_SINGLE("AudioL2", TWL4030_REG_PREDL_CTL, 2, 1, 0),
	SOC_DAPM_SINGLE("AudioR2", TWL4030_REG_PREDL_CTL, 3, 1, 0),
};

 
static const struct snd_kcontrol_new twl4030_dapm_predriver_controls[] = {
	SOC_DAPM_SINGLE("Voice", TWL4030_REG_PREDR_CTL, 0, 1, 0),
	SOC_DAPM_SINGLE("AudioR1", TWL4030_REG_PREDR_CTL, 1, 1, 0),
	SOC_DAPM_SINGLE("AudioR2", TWL4030_REG_PREDR_CTL, 2, 1, 0),
	SOC_DAPM_SINGLE("AudioL2", TWL4030_REG_PREDR_CTL, 3, 1, 0),
};

 
static const struct snd_kcontrol_new twl4030_dapm_hsol_controls[] = {
	SOC_DAPM_SINGLE("Voice", TWL4030_REG_HS_SEL, 0, 1, 0),
	SOC_DAPM_SINGLE("AudioL1", TWL4030_REG_HS_SEL, 1, 1, 0),
	SOC_DAPM_SINGLE("AudioL2", TWL4030_REG_HS_SEL, 2, 1, 0),
};

 
static const struct snd_kcontrol_new twl4030_dapm_hsor_controls[] = {
	SOC_DAPM_SINGLE("Voice", TWL4030_REG_HS_SEL, 3, 1, 0),
	SOC_DAPM_SINGLE("AudioR1", TWL4030_REG_HS_SEL, 4, 1, 0),
	SOC_DAPM_SINGLE("AudioR2", TWL4030_REG_HS_SEL, 5, 1, 0),
};

 
static const struct snd_kcontrol_new twl4030_dapm_carkitl_controls[] = {
	SOC_DAPM_SINGLE("Voice", TWL4030_REG_PRECKL_CTL, 0, 1, 0),
	SOC_DAPM_SINGLE("AudioL1", TWL4030_REG_PRECKL_CTL, 1, 1, 0),
	SOC_DAPM_SINGLE("AudioL2", TWL4030_REG_PRECKL_CTL, 2, 1, 0),
};

 
static const struct snd_kcontrol_new twl4030_dapm_carkitr_controls[] = {
	SOC_DAPM_SINGLE("Voice", TWL4030_REG_PRECKR_CTL, 0, 1, 0),
	SOC_DAPM_SINGLE("AudioR1", TWL4030_REG_PRECKR_CTL, 1, 1, 0),
	SOC_DAPM_SINGLE("AudioR2", TWL4030_REG_PRECKR_CTL, 2, 1, 0),
};

 
static const char *twl4030_handsfreel_texts[] =
		{"Voice", "AudioL1", "AudioL2", "AudioR2"};

static SOC_ENUM_SINGLE_DECL(twl4030_handsfreel_enum,
			    TWL4030_REG_HFL_CTL, 0,
			    twl4030_handsfreel_texts);

static const struct snd_kcontrol_new twl4030_dapm_handsfreel_control =
SOC_DAPM_ENUM("Route", twl4030_handsfreel_enum);

 
static const struct snd_kcontrol_new twl4030_dapm_handsfreelmute_control =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

 
static const char *twl4030_handsfreer_texts[] =
		{"Voice", "AudioR1", "AudioR2", "AudioL2"};

static SOC_ENUM_SINGLE_DECL(twl4030_handsfreer_enum,
			    TWL4030_REG_HFR_CTL, 0,
			    twl4030_handsfreer_texts);

static const struct snd_kcontrol_new twl4030_dapm_handsfreer_control =
SOC_DAPM_ENUM("Route", twl4030_handsfreer_enum);

 
static const struct snd_kcontrol_new twl4030_dapm_handsfreermute_control =
	SOC_DAPM_SINGLE_VIRT("Switch", 1);

 
 
static const char *twl4030_vibra_texts[] =
		{"AudioL1", "AudioR1", "AudioL2", "AudioR2"};

static SOC_ENUM_SINGLE_DECL(twl4030_vibra_enum,
			    TWL4030_REG_VIBRA_CTL, 2,
			    twl4030_vibra_texts);

static const struct snd_kcontrol_new twl4030_dapm_vibra_control =
SOC_DAPM_ENUM("Route", twl4030_vibra_enum);

 
static const char *twl4030_vibrapath_texts[] =
		{"Local vibrator", "Audio"};

static SOC_ENUM_SINGLE_DECL(twl4030_vibrapath_enum,
			    TWL4030_REG_VIBRA_CTL, 4,
			    twl4030_vibrapath_texts);

static const struct snd_kcontrol_new twl4030_dapm_vibrapath_control =
SOC_DAPM_ENUM("Route", twl4030_vibrapath_enum);

 
static const struct snd_kcontrol_new twl4030_dapm_analoglmic_controls[] = {
	SOC_DAPM_SINGLE("Main Mic Capture Switch",
			TWL4030_REG_ANAMICL, 0, 1, 0),
	SOC_DAPM_SINGLE("Headset Mic Capture Switch",
			TWL4030_REG_ANAMICL, 1, 1, 0),
	SOC_DAPM_SINGLE("AUXL Capture Switch",
			TWL4030_REG_ANAMICL, 2, 1, 0),
	SOC_DAPM_SINGLE("Carkit Mic Capture Switch",
			TWL4030_REG_ANAMICL, 3, 1, 0),
};

 
static const struct snd_kcontrol_new twl4030_dapm_analogrmic_controls[] = {
	SOC_DAPM_SINGLE("Sub Mic Capture Switch", TWL4030_REG_ANAMICR, 0, 1, 0),
	SOC_DAPM_SINGLE("AUXR Capture Switch", TWL4030_REG_ANAMICR, 2, 1, 0),
};

 
static const char *twl4030_micpathtx1_texts[] =
		{"Analog", "Digimic0"};

static SOC_ENUM_SINGLE_DECL(twl4030_micpathtx1_enum,
			    TWL4030_REG_ADCMICSEL, 0,
			    twl4030_micpathtx1_texts);

static const struct snd_kcontrol_new twl4030_dapm_micpathtx1_control =
SOC_DAPM_ENUM("Route", twl4030_micpathtx1_enum);

 
static const char *twl4030_micpathtx2_texts[] =
		{"Analog", "Digimic1"};

static SOC_ENUM_SINGLE_DECL(twl4030_micpathtx2_enum,
			    TWL4030_REG_ADCMICSEL, 2,
			    twl4030_micpathtx2_texts);

static const struct snd_kcontrol_new twl4030_dapm_micpathtx2_control =
SOC_DAPM_ENUM("Route", twl4030_micpathtx2_enum);

 
static const struct snd_kcontrol_new twl4030_dapm_abypassr1_control =
	SOC_DAPM_SINGLE("Switch", TWL4030_REG_ARXR1_APGA_CTL, 2, 1, 0);

 
static const struct snd_kcontrol_new twl4030_dapm_abypassl1_control =
	SOC_DAPM_SINGLE("Switch", TWL4030_REG_ARXL1_APGA_CTL, 2, 1, 0);

 
static const struct snd_kcontrol_new twl4030_dapm_abypassr2_control =
	SOC_DAPM_SINGLE("Switch", TWL4030_REG_ARXR2_APGA_CTL, 2, 1, 0);

 
static const struct snd_kcontrol_new twl4030_dapm_abypassl2_control =
	SOC_DAPM_SINGLE("Switch", TWL4030_REG_ARXL2_APGA_CTL, 2, 1, 0);

 
static const struct snd_kcontrol_new twl4030_dapm_abypassv_control =
	SOC_DAPM_SINGLE("Switch", TWL4030_REG_VDL_APGA_CTL, 2, 1, 0);

 
static const DECLARE_TLV_DB_RANGE(twl4030_dapm_dbypass_tlv,
	0, 1, TLV_DB_SCALE_ITEM(-3000, 600, 1),
	2, 3, TLV_DB_SCALE_ITEM(-2400, 0, 0),
	4, 7, TLV_DB_SCALE_ITEM(-1800, 600, 0)
);

 
static const struct snd_kcontrol_new twl4030_dapm_dbypassl_control =
	SOC_DAPM_SINGLE_TLV("Volume",
			TWL4030_REG_ATX2ARXPGA, 3, 7, 0,
			twl4030_dapm_dbypass_tlv);

 
static const struct snd_kcontrol_new twl4030_dapm_dbypassr_control =
	SOC_DAPM_SINGLE_TLV("Volume",
			TWL4030_REG_ATX2ARXPGA, 0, 7, 0,
			twl4030_dapm_dbypass_tlv);

 
static DECLARE_TLV_DB_SCALE(twl4030_dapm_dbypassv_tlv, -5100, 100, 1);

 
static const struct snd_kcontrol_new twl4030_dapm_dbypassv_control =
	SOC_DAPM_SINGLE_TLV("Volume",
			TWL4030_REG_VSTPGA, 0, 0x29, 0,
			twl4030_dapm_dbypassv_tlv);

 
#define TWL4030_OUTPUT_PGA(pin_name, reg, mask)				\
static int pin_name##pga_event(struct snd_soc_dapm_widget *w,		\
			       struct snd_kcontrol *kcontrol, int event) \
{									\
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);	\
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component); \
									\
	switch (event) {						\
	case SND_SOC_DAPM_POST_PMU:					\
		twl4030->pin_name##_enabled = 1;			\
		twl4030_write(component, reg, twl4030_read(component, reg));	\
		break;							\
	case SND_SOC_DAPM_POST_PMD:					\
		twl4030->pin_name##_enabled = 0;			\
		twl_i2c_write_u8(TWL4030_MODULE_AUDIO_VOICE, 0, reg);	\
		break;							\
	}								\
	return 0;							\
}

TWL4030_OUTPUT_PGA(earpiece, TWL4030_REG_EAR_CTL, TWL4030_EAR_GAIN);
TWL4030_OUTPUT_PGA(predrivel, TWL4030_REG_PREDL_CTL, TWL4030_PREDL_GAIN);
TWL4030_OUTPUT_PGA(predriver, TWL4030_REG_PREDR_CTL, TWL4030_PREDR_GAIN);
TWL4030_OUTPUT_PGA(carkitl, TWL4030_REG_PRECKL_CTL, TWL4030_PRECKL_GAIN);
TWL4030_OUTPUT_PGA(carkitr, TWL4030_REG_PRECKR_CTL, TWL4030_PRECKR_GAIN);

static void handsfree_ramp(struct snd_soc_component *component, int reg, int ramp)
{
	unsigned char hs_ctl;

	hs_ctl = twl4030_read(component, reg);

	if (ramp) {
		 
		hs_ctl |= TWL4030_HF_CTL_REF_EN;
		twl4030_write(component, reg, hs_ctl);
		udelay(10);
		hs_ctl |= TWL4030_HF_CTL_RAMP_EN;
		twl4030_write(component, reg, hs_ctl);
		udelay(40);
		hs_ctl |= TWL4030_HF_CTL_LOOP_EN;
		hs_ctl |= TWL4030_HF_CTL_HB_EN;
		twl4030_write(component, reg, hs_ctl);
	} else {
		 
		hs_ctl &= ~TWL4030_HF_CTL_LOOP_EN;
		hs_ctl &= ~TWL4030_HF_CTL_HB_EN;
		twl4030_write(component, reg, hs_ctl);
		hs_ctl &= ~TWL4030_HF_CTL_RAMP_EN;
		twl4030_write(component, reg, hs_ctl);
		udelay(40);
		hs_ctl &= ~TWL4030_HF_CTL_REF_EN;
		twl4030_write(component, reg, hs_ctl);
	}
}

static int handsfreelpga_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		handsfree_ramp(component, TWL4030_REG_HFL_CTL, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		handsfree_ramp(component, TWL4030_REG_HFL_CTL, 0);
		break;
	}
	return 0;
}

static int handsfreerpga_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		handsfree_ramp(component, TWL4030_REG_HFR_CTL, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		handsfree_ramp(component, TWL4030_REG_HFR_CTL, 0);
		break;
	}
	return 0;
}

static int vibramux_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	twl4030_write(component, TWL4030_REG_VIBRA_SET, 0xff);
	return 0;
}

static int apll_event(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		twl4030_apll_enable(component, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		twl4030_apll_enable(component, 0);
		break;
	}
	return 0;
}

static int aif_event(struct snd_soc_dapm_widget *w,
		     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	u8 audio_if;

	audio_if = twl4030_read(component, TWL4030_REG_AUDIO_IF);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		 
		 
		twl4030_apll_enable(component, 1);

		twl4030_write(component, TWL4030_REG_AUDIO_IF,
			      audio_if | TWL4030_AIF_EN);
		break;
	case SND_SOC_DAPM_POST_PMD:
		 
		twl4030_write(component, TWL4030_REG_AUDIO_IF,
			      audio_if &  ~TWL4030_AIF_EN);
		twl4030_apll_enable(component, 0);
		break;
	}
	return 0;
}

static void headset_ramp(struct snd_soc_component *component, int ramp)
{
	unsigned char hs_gain, hs_pop;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	struct twl4030_board_params *board_params = twl4030->board_params;
	 
	static const unsigned int ramp_base[] = {
		524288, 1048576, 2097152, 4194304,
		8388608, 16777216, 33554432, 67108864
	};
	unsigned int delay;

	hs_gain = twl4030_read(component, TWL4030_REG_HS_GAIN_SET);
	hs_pop = twl4030_read(component, TWL4030_REG_HS_POPN_SET);
	delay = (ramp_base[(hs_pop & TWL4030_RAMP_DELAY) >> 2] /
		twl4030->sysclk) + 1;

	 
	if (board_params && board_params->hs_extmute) {
		if (gpio_is_valid(board_params->hs_extmute_gpio)) {
			gpio_set_value(board_params->hs_extmute_gpio, 1);
		} else {
			hs_pop |= TWL4030_EXTMUTE;
			twl4030_write(component, TWL4030_REG_HS_POPN_SET, hs_pop);
		}
	}

	if (ramp) {
		 
		hs_pop |= TWL4030_VMID_EN;
		twl4030_write(component, TWL4030_REG_HS_POPN_SET, hs_pop);
		 
		twl_i2c_write_u8(TWL4030_MODULE_AUDIO_VOICE, hs_gain,
				 TWL4030_REG_HS_GAIN_SET);
		hs_pop |= TWL4030_RAMP_EN;
		twl4030_write(component, TWL4030_REG_HS_POPN_SET, hs_pop);
		 
		twl4030_wait_ms(delay);
	} else {
		 
		hs_pop &= ~TWL4030_RAMP_EN;
		twl4030_write(component, TWL4030_REG_HS_POPN_SET, hs_pop);
		 
		twl4030_wait_ms(delay);
		 
		twl_i2c_write_u8(TWL4030_MODULE_AUDIO_VOICE, hs_gain & (~0x0f),
				 TWL4030_REG_HS_GAIN_SET);

		hs_pop &= ~TWL4030_VMID_EN;
		twl4030_write(component, TWL4030_REG_HS_POPN_SET, hs_pop);
	}

	 
	if (board_params && board_params->hs_extmute) {
		if (gpio_is_valid(board_params->hs_extmute_gpio)) {
			gpio_set_value(board_params->hs_extmute_gpio, 0);
		} else {
			hs_pop &= ~TWL4030_EXTMUTE;
			twl4030_write(component, TWL4030_REG_HS_POPN_SET, hs_pop);
		}
	}
}

static int headsetlpga_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		 
		if (!twl4030->hsr_enabled)
			headset_ramp(component, 1);

		twl4030->hsl_enabled = 1;
		break;
	case SND_SOC_DAPM_POST_PMD:
		 
		if (!twl4030->hsr_enabled)
			headset_ramp(component, 0);

		twl4030->hsl_enabled = 0;
		break;
	}
	return 0;
}

static int headsetrpga_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		 
		if (!twl4030->hsl_enabled)
			headset_ramp(component, 1);

		twl4030->hsr_enabled = 1;
		break;
	case SND_SOC_DAPM_POST_PMD:
		 
		if (!twl4030->hsl_enabled)
			headset_ramp(component, 0);

		twl4030->hsr_enabled = 0;
		break;
	}
	return 0;
}

static int digimic_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	struct twl4030_board_params *board_params = twl4030->board_params;

	if (board_params && board_params->digimic_delay)
		twl4030_wait_ms(board_params->digimic_delay);
	return 0;
}

 
static int snd_soc_get_volsw_twl4030(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int max = mc->max;
	int mask = (1 << fls(max)) - 1;

	ucontrol->value.integer.value[0] =
		(twl4030_read(component, reg) >> shift) & mask;
	if (ucontrol->value.integer.value[0])
		ucontrol->value.integer.value[0] =
			max + 1 - ucontrol->value.integer.value[0];

	if (shift != rshift) {
		ucontrol->value.integer.value[1] =
			(twl4030_read(component, reg) >> rshift) & mask;
		if (ucontrol->value.integer.value[1])
			ucontrol->value.integer.value[1] =
				max + 1 - ucontrol->value.integer.value[1];
	}

	return 0;
}

static int snd_soc_put_volsw_twl4030(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	unsigned int rshift = mc->rshift;
	int max = mc->max;
	int mask = (1 << fls(max)) - 1;
	unsigned short val, val2, val_mask;

	val = (ucontrol->value.integer.value[0] & mask);

	val_mask = mask << shift;
	if (val)
		val = max + 1 - val;
	val = val << shift;
	if (shift != rshift) {
		val2 = (ucontrol->value.integer.value[1] & mask);
		val_mask |= mask << rshift;
		if (val2)
			val2 = max + 1 - val2;
		val |= val2 << rshift;
	}
	return snd_soc_component_update_bits(component, reg, val_mask, val);
}

static int snd_soc_get_volsw_r2_twl4030(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int reg2 = mc->rreg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	int mask = (1<<fls(max))-1;

	ucontrol->value.integer.value[0] =
		(twl4030_read(component, reg) >> shift) & mask;
	ucontrol->value.integer.value[1] =
		(twl4030_read(component, reg2) >> shift) & mask;

	if (ucontrol->value.integer.value[0])
		ucontrol->value.integer.value[0] =
			max + 1 - ucontrol->value.integer.value[0];
	if (ucontrol->value.integer.value[1])
		ucontrol->value.integer.value[1] =
			max + 1 - ucontrol->value.integer.value[1];

	return 0;
}

static int snd_soc_put_volsw_r2_twl4030(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	unsigned int reg = mc->reg;
	unsigned int reg2 = mc->rreg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	int mask = (1 << fls(max)) - 1;
	int err;
	unsigned short val, val2, val_mask;

	val_mask = mask << shift;
	val = (ucontrol->value.integer.value[0] & mask);
	val2 = (ucontrol->value.integer.value[1] & mask);

	if (val)
		val = max + 1 - val;
	if (val2)
		val2 = max + 1 - val2;

	val = val << shift;
	val2 = val2 << shift;

	err = snd_soc_component_update_bits(component, reg, val_mask, val);
	if (err < 0)
		return err;

	err = snd_soc_component_update_bits(component, reg2, val_mask, val2);
	return err;
}

 
static const char *twl4030_op_modes_texts[] = {
	"Option 2 (voice/audio)", "Option 1 (audio)"
};

static SOC_ENUM_SINGLE_DECL(twl4030_op_modes_enum,
			    TWL4030_REG_CODEC_MODE, 0,
			    twl4030_op_modes_texts);

static int snd_soc_put_twl4030_opmode_enum_double(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);

	if (twl4030->configured) {
		dev_err(component->dev,
			"operation mode cannot be changed on-the-fly\n");
		return -EBUSY;
	}

	return snd_soc_put_enum_double(kcontrol, ucontrol);
}

 
static DECLARE_TLV_DB_SCALE(digital_fine_tlv, -6300, 100, 1);

 
static DECLARE_TLV_DB_SCALE(digital_coarse_tlv, 0, 600, 0);

 
static DECLARE_TLV_DB_SCALE(digital_voice_downlink_tlv, -3700, 100, 1);

 
static DECLARE_TLV_DB_SCALE(analog_tlv, -2400, 200, 0);

 
static DECLARE_TLV_DB_SCALE(output_tvl, -1200, 600, 1);

 
static DECLARE_TLV_DB_SCALE(output_ear_tvl, -600, 600, 1);

 
static DECLARE_TLV_DB_SCALE(digital_capture_tlv, 0, 100, 0);

 
static DECLARE_TLV_DB_SCALE(input_gain_tlv, 0, 600, 0);

 
static const char *twl4030_avadc_clk_priority_texts[] = {
	"Voice high priority", "HiFi high priority"
};

static SOC_ENUM_SINGLE_DECL(twl4030_avadc_clk_priority_enum,
			    TWL4030_REG_AVADC_CTL, 2,
			    twl4030_avadc_clk_priority_texts);

static const char *twl4030_rampdelay_texts[] = {
	"27/20/14 ms", "55/40/27 ms", "109/81/55 ms", "218/161/109 ms",
	"437/323/218 ms", "874/645/437 ms", "1748/1291/874 ms",
	"3495/2581/1748 ms"
};

static SOC_ENUM_SINGLE_DECL(twl4030_rampdelay_enum,
			    TWL4030_REG_HS_POPN_SET, 2,
			    twl4030_rampdelay_texts);

 
static const char *twl4030_vibradirmode_texts[] = {
	"Vibra H-bridge direction", "Audio data MSB",
};

static SOC_ENUM_SINGLE_DECL(twl4030_vibradirmode_enum,
			    TWL4030_REG_VIBRA_CTL, 5,
			    twl4030_vibradirmode_texts);

 
static const char *twl4030_vibradir_texts[] = {
	"Positive polarity", "Negative polarity",
};

static SOC_ENUM_SINGLE_DECL(twl4030_vibradir_enum,
			    TWL4030_REG_VIBRA_CTL, 1,
			    twl4030_vibradir_texts);

 
static const char *twl4030_digimicswap_texts[] = {
	"Not swapped", "Swapped",
};

static SOC_ENUM_SINGLE_DECL(twl4030_digimicswap_enum,
			    TWL4030_REG_MISC_SET_1, 0,
			    twl4030_digimicswap_texts);

static const struct snd_kcontrol_new twl4030_snd_controls[] = {
	 
	SOC_ENUM_EXT("Codec Operation Mode", twl4030_op_modes_enum,
		snd_soc_get_enum_double,
		snd_soc_put_twl4030_opmode_enum_double),

	 
	SOC_DOUBLE_R_TLV("DAC1 Digital Fine Playback Volume",
		TWL4030_REG_ARXL1PGA, TWL4030_REG_ARXR1PGA,
		0, 0x3f, 0, digital_fine_tlv),
	SOC_DOUBLE_R_TLV("DAC2 Digital Fine Playback Volume",
		TWL4030_REG_ARXL2PGA, TWL4030_REG_ARXR2PGA,
		0, 0x3f, 0, digital_fine_tlv),

	SOC_DOUBLE_R_TLV("DAC1 Digital Coarse Playback Volume",
		TWL4030_REG_ARXL1PGA, TWL4030_REG_ARXR1PGA,
		6, 0x2, 0, digital_coarse_tlv),
	SOC_DOUBLE_R_TLV("DAC2 Digital Coarse Playback Volume",
		TWL4030_REG_ARXL2PGA, TWL4030_REG_ARXR2PGA,
		6, 0x2, 0, digital_coarse_tlv),

	SOC_DOUBLE_R_TLV("DAC1 Analog Playback Volume",
		TWL4030_REG_ARXL1_APGA_CTL, TWL4030_REG_ARXR1_APGA_CTL,
		3, 0x12, 1, analog_tlv),
	SOC_DOUBLE_R_TLV("DAC2 Analog Playback Volume",
		TWL4030_REG_ARXL2_APGA_CTL, TWL4030_REG_ARXR2_APGA_CTL,
		3, 0x12, 1, analog_tlv),
	SOC_DOUBLE_R("DAC1 Analog Playback Switch",
		TWL4030_REG_ARXL1_APGA_CTL, TWL4030_REG_ARXR1_APGA_CTL,
		1, 1, 0),
	SOC_DOUBLE_R("DAC2 Analog Playback Switch",
		TWL4030_REG_ARXL2_APGA_CTL, TWL4030_REG_ARXR2_APGA_CTL,
		1, 1, 0),

	 
	SOC_SINGLE_TLV("DAC Voice Digital Downlink Volume",
		TWL4030_REG_VRXPGA, 0, 0x31, 0, digital_voice_downlink_tlv),

	SOC_SINGLE_TLV("DAC Voice Analog Downlink Volume",
		TWL4030_REG_VDL_APGA_CTL, 3, 0x12, 1, analog_tlv),

	SOC_SINGLE("DAC Voice Analog Downlink Switch",
		TWL4030_REG_VDL_APGA_CTL, 1, 1, 0),

	 
	SOC_DOUBLE_R_EXT_TLV("PreDriv Playback Volume",
		TWL4030_REG_PREDL_CTL, TWL4030_REG_PREDR_CTL,
		4, 3, 0, snd_soc_get_volsw_r2_twl4030,
		snd_soc_put_volsw_r2_twl4030, output_tvl),

	SOC_DOUBLE_EXT_TLV("Headset Playback Volume",
		TWL4030_REG_HS_GAIN_SET, 0, 2, 3, 0, snd_soc_get_volsw_twl4030,
		snd_soc_put_volsw_twl4030, output_tvl),

	SOC_DOUBLE_R_EXT_TLV("Carkit Playback Volume",
		TWL4030_REG_PRECKL_CTL, TWL4030_REG_PRECKR_CTL,
		4, 3, 0, snd_soc_get_volsw_r2_twl4030,
		snd_soc_put_volsw_r2_twl4030, output_tvl),

	SOC_SINGLE_EXT_TLV("Earpiece Playback Volume",
		TWL4030_REG_EAR_CTL, 4, 3, 0, snd_soc_get_volsw_twl4030,
		snd_soc_put_volsw_twl4030, output_ear_tvl),

	 
	SOC_DOUBLE_R_TLV("TX1 Digital Capture Volume",
		TWL4030_REG_ATXL1PGA, TWL4030_REG_ATXR1PGA,
		0, 0x1f, 0, digital_capture_tlv),
	SOC_DOUBLE_R_TLV("TX2 Digital Capture Volume",
		TWL4030_REG_AVTXL2PGA, TWL4030_REG_AVTXR2PGA,
		0, 0x1f, 0, digital_capture_tlv),

	SOC_DOUBLE_TLV("Analog Capture Volume", TWL4030_REG_ANAMIC_GAIN,
		0, 3, 5, 0, input_gain_tlv),

	SOC_ENUM("AVADC Clock Priority", twl4030_avadc_clk_priority_enum),

	SOC_ENUM("HS ramp delay", twl4030_rampdelay_enum),

	SOC_ENUM("Vibra H-bridge mode", twl4030_vibradirmode_enum),
	SOC_ENUM("Vibra H-bridge direction", twl4030_vibradir_enum),

	SOC_ENUM("Digimic LR Swap", twl4030_digimicswap_enum),
};

static const struct snd_soc_dapm_widget twl4030_dapm_widgets[] = {
	 
	SND_SOC_DAPM_INPUT("MAINMIC"),
	SND_SOC_DAPM_INPUT("HSMIC"),
	SND_SOC_DAPM_INPUT("AUXL"),
	SND_SOC_DAPM_INPUT("CARKITMIC"),
	 
	SND_SOC_DAPM_INPUT("SUBMIC"),
	SND_SOC_DAPM_INPUT("AUXR"),
	 
	SND_SOC_DAPM_INPUT("DIGIMIC0"),
	SND_SOC_DAPM_INPUT("DIGIMIC1"),

	 
	SND_SOC_DAPM_OUTPUT("EARPIECE"),
	SND_SOC_DAPM_OUTPUT("PREDRIVEL"),
	SND_SOC_DAPM_OUTPUT("PREDRIVER"),
	SND_SOC_DAPM_OUTPUT("HSOL"),
	SND_SOC_DAPM_OUTPUT("HSOR"),
	SND_SOC_DAPM_OUTPUT("CARKITL"),
	SND_SOC_DAPM_OUTPUT("CARKITR"),
	SND_SOC_DAPM_OUTPUT("HFL"),
	SND_SOC_DAPM_OUTPUT("HFR"),
	SND_SOC_DAPM_OUTPUT("VIBRA"),

	 
	SND_SOC_DAPM_OUTPUT("Virtual HiFi OUT"),
	SND_SOC_DAPM_INPUT("Virtual HiFi IN"),
	SND_SOC_DAPM_OUTPUT("Virtual Voice OUT"),

	 
	SND_SOC_DAPM_DAC("DAC Right1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC Left1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC Right2", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC Left2", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("DAC Voice", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("VAIFIN", "Voice Playback", 0,
			    TWL4030_REG_VOICE_IF, 6, 0),

	 
	SND_SOC_DAPM_SWITCH("Right1 Analog Loopback", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_abypassr1_control),
	SND_SOC_DAPM_SWITCH("Left1 Analog Loopback", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_abypassl1_control),
	SND_SOC_DAPM_SWITCH("Right2 Analog Loopback", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_abypassr2_control),
	SND_SOC_DAPM_SWITCH("Left2 Analog Loopback", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_abypassl2_control),
	SND_SOC_DAPM_SWITCH("Voice Analog Loopback", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_abypassv_control),

	 
	SND_SOC_DAPM_SUPPLY("FM Loop Enable", TWL4030_REG_MISC_SET_1, 5, 0,
			    NULL, 0),

	 
	SND_SOC_DAPM_SWITCH("Left Digital Loopback", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_dbypassl_control),
	SND_SOC_DAPM_SWITCH("Right Digital Loopback", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_dbypassr_control),
	SND_SOC_DAPM_SWITCH("Voice Digital Loopback", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_dbypassv_control),

	 
	SND_SOC_DAPM_MIXER("Digital R1 Playback Mixer",
			TWL4030_REG_AVDAC_CTL, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Digital L1 Playback Mixer",
			TWL4030_REG_AVDAC_CTL, 1, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Digital R2 Playback Mixer",
			TWL4030_REG_AVDAC_CTL, 2, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Digital L2 Playback Mixer",
			TWL4030_REG_AVDAC_CTL, 3, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Digital Voice Playback Mixer",
			TWL4030_REG_AVDAC_CTL, 4, 0, NULL, 0),

	 
	SND_SOC_DAPM_MIXER("Analog R1 Playback Mixer",
			TWL4030_REG_ARXR1_APGA_CTL, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Analog L1 Playback Mixer",
			TWL4030_REG_ARXL1_APGA_CTL, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Analog R2 Playback Mixer",
			TWL4030_REG_ARXR2_APGA_CTL, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Analog L2 Playback Mixer",
			TWL4030_REG_ARXL2_APGA_CTL, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("Analog Voice Playback Mixer",
			TWL4030_REG_VDL_APGA_CTL, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("APLL Enable", SND_SOC_NOPM, 0, 0, apll_event,
			    SND_SOC_DAPM_PRE_PMU|SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("AIF Enable", SND_SOC_NOPM, 0, 0, aif_event,
			    SND_SOC_DAPM_PRE_PMU|SND_SOC_DAPM_POST_PMD),

	 
	 
	SND_SOC_DAPM_MIXER("Earpiece Mixer", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_earpiece_controls[0],
			ARRAY_SIZE(twl4030_dapm_earpiece_controls)),
	SND_SOC_DAPM_PGA_E("Earpiece PGA", SND_SOC_NOPM,
			0, 0, NULL, 0, earpiecepga_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_POST_PMD),
	 
	SND_SOC_DAPM_MIXER("PredriveL Mixer", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_predrivel_controls[0],
			ARRAY_SIZE(twl4030_dapm_predrivel_controls)),
	SND_SOC_DAPM_PGA_E("PredriveL PGA", SND_SOC_NOPM,
			0, 0, NULL, 0, predrivelpga_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("PredriveR Mixer", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_predriver_controls[0],
			ARRAY_SIZE(twl4030_dapm_predriver_controls)),
	SND_SOC_DAPM_PGA_E("PredriveR PGA", SND_SOC_NOPM,
			0, 0, NULL, 0, predriverpga_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_POST_PMD),
	 
	SND_SOC_DAPM_MIXER("HeadsetL Mixer", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_hsol_controls[0],
			ARRAY_SIZE(twl4030_dapm_hsol_controls)),
	SND_SOC_DAPM_PGA_E("HeadsetL PGA", SND_SOC_NOPM,
			0, 0, NULL, 0, headsetlpga_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("HeadsetR Mixer", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_hsor_controls[0],
			ARRAY_SIZE(twl4030_dapm_hsor_controls)),
	SND_SOC_DAPM_PGA_E("HeadsetR PGA", SND_SOC_NOPM,
			0, 0, NULL, 0, headsetrpga_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_POST_PMD),
	 
	SND_SOC_DAPM_MIXER("CarkitL Mixer", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_carkitl_controls[0],
			ARRAY_SIZE(twl4030_dapm_carkitl_controls)),
	SND_SOC_DAPM_PGA_E("CarkitL PGA", SND_SOC_NOPM,
			0, 0, NULL, 0, carkitlpga_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("CarkitR Mixer", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_carkitr_controls[0],
			ARRAY_SIZE(twl4030_dapm_carkitr_controls)),
	SND_SOC_DAPM_PGA_E("CarkitR PGA", SND_SOC_NOPM,
			0, 0, NULL, 0, carkitrpga_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_POST_PMD),

	 
	 
	SND_SOC_DAPM_MUX("HandsfreeL Mux", SND_SOC_NOPM, 0, 0,
		&twl4030_dapm_handsfreel_control),
	SND_SOC_DAPM_SWITCH("HandsfreeL", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_handsfreelmute_control),
	SND_SOC_DAPM_PGA_E("HandsfreeL PGA", SND_SOC_NOPM,
			0, 0, NULL, 0, handsfreelpga_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX("HandsfreeR Mux", SND_SOC_NOPM, 5, 0,
		&twl4030_dapm_handsfreer_control),
	SND_SOC_DAPM_SWITCH("HandsfreeR", SND_SOC_NOPM, 0, 0,
			&twl4030_dapm_handsfreermute_control),
	SND_SOC_DAPM_PGA_E("HandsfreeR PGA", SND_SOC_NOPM,
			0, 0, NULL, 0, handsfreerpga_event,
			SND_SOC_DAPM_POST_PMU|SND_SOC_DAPM_POST_PMD),
	 
	SND_SOC_DAPM_MUX_E("Vibra Mux", TWL4030_REG_VIBRA_CTL, 0, 0,
			   &twl4030_dapm_vibra_control, vibramux_event,
			   SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MUX("Vibra Route", SND_SOC_NOPM, 0, 0,
		&twl4030_dapm_vibrapath_control),

	 
	SND_SOC_DAPM_ADC("ADC Virtual Left1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC Virtual Right1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC Virtual Left2", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_ADC("ADC Virtual Right2", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT("VAIFOUT", "Voice Capture", 0,
			     TWL4030_REG_VOICE_IF, 5, 0),

	 
	SND_SOC_DAPM_MUX("TX1 Capture Route", SND_SOC_NOPM, 0, 0,
		&twl4030_dapm_micpathtx1_control),
	SND_SOC_DAPM_MUX("TX2 Capture Route", SND_SOC_NOPM, 0, 0,
		&twl4030_dapm_micpathtx2_control),

	 
	SND_SOC_DAPM_MIXER("Analog Left",
		TWL4030_REG_ANAMICL, 4, 0,
		&twl4030_dapm_analoglmic_controls[0],
		ARRAY_SIZE(twl4030_dapm_analoglmic_controls)),
	SND_SOC_DAPM_MIXER("Analog Right",
		TWL4030_REG_ANAMICR, 4, 0,
		&twl4030_dapm_analogrmic_controls[0],
		ARRAY_SIZE(twl4030_dapm_analogrmic_controls)),

	SND_SOC_DAPM_PGA("ADC Physical Left",
		TWL4030_REG_AVADC_CTL, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("ADC Physical Right",
		TWL4030_REG_AVADC_CTL, 1, 0, NULL, 0),

	SND_SOC_DAPM_PGA_E("Digimic0 Enable",
		TWL4030_REG_ADCMICSEL, 1, 0, NULL, 0,
		digimic_event, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("Digimic1 Enable",
		TWL4030_REG_ADCMICSEL, 3, 0, NULL, 0,
		digimic_event, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY("micbias1 select", TWL4030_REG_MICBIAS_CTL, 5, 0,
			    NULL, 0),
	SND_SOC_DAPM_SUPPLY("micbias2 select", TWL4030_REG_MICBIAS_CTL, 6, 0,
			    NULL, 0),

	 
	SND_SOC_DAPM_SUPPLY("Mic Bias 1",
			    TWL4030_REG_MICBIAS_CTL, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Mic Bias 2",
			    TWL4030_REG_MICBIAS_CTL, 1, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Headset Mic Bias",
			    TWL4030_REG_MICBIAS_CTL, 2, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("VIF Enable", TWL4030_REG_VOICE_IF, 0, 0, NULL, 0),
};

static const struct snd_soc_dapm_route intercon[] = {
	 
	{"DAC Right1", NULL, "HiFi Playback"},
	{"DAC Left1", NULL, "HiFi Playback"},
	{"DAC Right2", NULL, "HiFi Playback"},
	{"DAC Left2", NULL, "HiFi Playback"},
	{"DAC Voice", NULL, "VAIFIN"},

	 
	{"HiFi Capture", NULL, "ADC Virtual Left1"},
	{"HiFi Capture", NULL, "ADC Virtual Right1"},
	{"HiFi Capture", NULL, "ADC Virtual Left2"},
	{"HiFi Capture", NULL, "ADC Virtual Right2"},
	{"VAIFOUT", NULL, "ADC Virtual Left2"},
	{"VAIFOUT", NULL, "ADC Virtual Right2"},
	{"VAIFOUT", NULL, "VIF Enable"},

	{"Digital L1 Playback Mixer", NULL, "DAC Left1"},
	{"Digital R1 Playback Mixer", NULL, "DAC Right1"},
	{"Digital L2 Playback Mixer", NULL, "DAC Left2"},
	{"Digital R2 Playback Mixer", NULL, "DAC Right2"},
	{"Digital Voice Playback Mixer", NULL, "DAC Voice"},

	 
	{"Digital Voice Playback Mixer", NULL, "APLL Enable"},

	{"DAC Left1", NULL, "AIF Enable"},
	{"DAC Right1", NULL, "AIF Enable"},
	{"DAC Left2", NULL, "AIF Enable"},
	{"DAC Right1", NULL, "AIF Enable"},
	{"DAC Voice", NULL, "VIF Enable"},

	{"Digital R2 Playback Mixer", NULL, "AIF Enable"},
	{"Digital L2 Playback Mixer", NULL, "AIF Enable"},

	{"Analog L1 Playback Mixer", NULL, "Digital L1 Playback Mixer"},
	{"Analog R1 Playback Mixer", NULL, "Digital R1 Playback Mixer"},
	{"Analog L2 Playback Mixer", NULL, "Digital L2 Playback Mixer"},
	{"Analog R2 Playback Mixer", NULL, "Digital R2 Playback Mixer"},
	{"Analog Voice Playback Mixer", NULL, "Digital Voice Playback Mixer"},

	 
	 
	{"Earpiece Mixer", "Voice", "Analog Voice Playback Mixer"},
	{"Earpiece Mixer", "AudioL1", "Analog L1 Playback Mixer"},
	{"Earpiece Mixer", "AudioL2", "Analog L2 Playback Mixer"},
	{"Earpiece Mixer", "AudioR1", "Analog R1 Playback Mixer"},
	{"Earpiece PGA", NULL, "Earpiece Mixer"},
	 
	{"PredriveL Mixer", "Voice", "Analog Voice Playback Mixer"},
	{"PredriveL Mixer", "AudioL1", "Analog L1 Playback Mixer"},
	{"PredriveL Mixer", "AudioL2", "Analog L2 Playback Mixer"},
	{"PredriveL Mixer", "AudioR2", "Analog R2 Playback Mixer"},
	{"PredriveL PGA", NULL, "PredriveL Mixer"},
	 
	{"PredriveR Mixer", "Voice", "Analog Voice Playback Mixer"},
	{"PredriveR Mixer", "AudioR1", "Analog R1 Playback Mixer"},
	{"PredriveR Mixer", "AudioR2", "Analog R2 Playback Mixer"},
	{"PredriveR Mixer", "AudioL2", "Analog L2 Playback Mixer"},
	{"PredriveR PGA", NULL, "PredriveR Mixer"},
	 
	{"HeadsetL Mixer", "Voice", "Analog Voice Playback Mixer"},
	{"HeadsetL Mixer", "AudioL1", "Analog L1 Playback Mixer"},
	{"HeadsetL Mixer", "AudioL2", "Analog L2 Playback Mixer"},
	{"HeadsetL PGA", NULL, "HeadsetL Mixer"},
	 
	{"HeadsetR Mixer", "Voice", "Analog Voice Playback Mixer"},
	{"HeadsetR Mixer", "AudioR1", "Analog R1 Playback Mixer"},
	{"HeadsetR Mixer", "AudioR2", "Analog R2 Playback Mixer"},
	{"HeadsetR PGA", NULL, "HeadsetR Mixer"},
	 
	{"CarkitL Mixer", "Voice", "Analog Voice Playback Mixer"},
	{"CarkitL Mixer", "AudioL1", "Analog L1 Playback Mixer"},
	{"CarkitL Mixer", "AudioL2", "Analog L2 Playback Mixer"},
	{"CarkitL PGA", NULL, "CarkitL Mixer"},
	 
	{"CarkitR Mixer", "Voice", "Analog Voice Playback Mixer"},
	{"CarkitR Mixer", "AudioR1", "Analog R1 Playback Mixer"},
	{"CarkitR Mixer", "AudioR2", "Analog R2 Playback Mixer"},
	{"CarkitR PGA", NULL, "CarkitR Mixer"},
	 
	{"HandsfreeL Mux", "Voice", "Analog Voice Playback Mixer"},
	{"HandsfreeL Mux", "AudioL1", "Analog L1 Playback Mixer"},
	{"HandsfreeL Mux", "AudioL2", "Analog L2 Playback Mixer"},
	{"HandsfreeL Mux", "AudioR2", "Analog R2 Playback Mixer"},
	{"HandsfreeL", "Switch", "HandsfreeL Mux"},
	{"HandsfreeL PGA", NULL, "HandsfreeL"},
	 
	{"HandsfreeR Mux", "Voice", "Analog Voice Playback Mixer"},
	{"HandsfreeR Mux", "AudioR1", "Analog R1 Playback Mixer"},
	{"HandsfreeR Mux", "AudioR2", "Analog R2 Playback Mixer"},
	{"HandsfreeR Mux", "AudioL2", "Analog L2 Playback Mixer"},
	{"HandsfreeR", "Switch", "HandsfreeR Mux"},
	{"HandsfreeR PGA", NULL, "HandsfreeR"},
	 
	{"Vibra Mux", "AudioL1", "DAC Left1"},
	{"Vibra Mux", "AudioR1", "DAC Right1"},
	{"Vibra Mux", "AudioL2", "DAC Left2"},
	{"Vibra Mux", "AudioR2", "DAC Right2"},

	 
	 
	{"Virtual HiFi OUT", NULL, "DAC Left1"},
	{"Virtual HiFi OUT", NULL, "DAC Right1"},
	{"Virtual HiFi OUT", NULL, "DAC Left2"},
	{"Virtual HiFi OUT", NULL, "DAC Right2"},
	 
	{"Virtual Voice OUT", NULL, "Digital Voice Playback Mixer"},
	 
	{"EARPIECE", NULL, "Earpiece PGA"},
	{"PREDRIVEL", NULL, "PredriveL PGA"},
	{"PREDRIVER", NULL, "PredriveR PGA"},
	{"HSOL", NULL, "HeadsetL PGA"},
	{"HSOR", NULL, "HeadsetR PGA"},
	{"CARKITL", NULL, "CarkitL PGA"},
	{"CARKITR", NULL, "CarkitR PGA"},
	{"HFL", NULL, "HandsfreeL PGA"},
	{"HFR", NULL, "HandsfreeR PGA"},
	{"Vibra Route", "Audio", "Vibra Mux"},
	{"VIBRA", NULL, "Vibra Route"},

	 
	 
	{"ADC Virtual Left1", NULL, "Virtual HiFi IN"},
	{"ADC Virtual Right1", NULL, "Virtual HiFi IN"},
	{"ADC Virtual Left2", NULL, "Virtual HiFi IN"},
	{"ADC Virtual Right2", NULL, "Virtual HiFi IN"},
	 
	{"Analog Left", "Main Mic Capture Switch", "MAINMIC"},
	{"Analog Left", "Headset Mic Capture Switch", "HSMIC"},
	{"Analog Left", "AUXL Capture Switch", "AUXL"},
	{"Analog Left", "Carkit Mic Capture Switch", "CARKITMIC"},

	{"Analog Right", "Sub Mic Capture Switch", "SUBMIC"},
	{"Analog Right", "AUXR Capture Switch", "AUXR"},

	{"ADC Physical Left", NULL, "Analog Left"},
	{"ADC Physical Right", NULL, "Analog Right"},

	{"Digimic0 Enable", NULL, "DIGIMIC0"},
	{"Digimic1 Enable", NULL, "DIGIMIC1"},

	{"DIGIMIC0", NULL, "micbias1 select"},
	{"DIGIMIC1", NULL, "micbias2 select"},

	 
	{"TX1 Capture Route", "Analog", "ADC Physical Left"},
	{"TX1 Capture Route", "Digimic0", "Digimic0 Enable"},
	 
	{"TX1 Capture Route", "Analog", "ADC Physical Right"},
	{"TX1 Capture Route", "Digimic0", "Digimic0 Enable"},
	 
	{"TX2 Capture Route", "Analog", "ADC Physical Left"},
	{"TX2 Capture Route", "Digimic1", "Digimic1 Enable"},
	 
	{"TX2 Capture Route", "Analog", "ADC Physical Right"},
	{"TX2 Capture Route", "Digimic1", "Digimic1 Enable"},

	{"ADC Virtual Left1", NULL, "TX1 Capture Route"},
	{"ADC Virtual Right1", NULL, "TX1 Capture Route"},
	{"ADC Virtual Left2", NULL, "TX2 Capture Route"},
	{"ADC Virtual Right2", NULL, "TX2 Capture Route"},

	{"ADC Virtual Left1", NULL, "AIF Enable"},
	{"ADC Virtual Right1", NULL, "AIF Enable"},
	{"ADC Virtual Left2", NULL, "AIF Enable"},
	{"ADC Virtual Right2", NULL, "AIF Enable"},

	 
	{"Right1 Analog Loopback", "Switch", "Analog Right"},
	{"Left1 Analog Loopback", "Switch", "Analog Left"},
	{"Right2 Analog Loopback", "Switch", "Analog Right"},
	{"Left2 Analog Loopback", "Switch", "Analog Left"},
	{"Voice Analog Loopback", "Switch", "Analog Left"},

	 
	{"Right1 Analog Loopback", NULL, "FM Loop Enable"},
	{"Left1 Analog Loopback", NULL, "FM Loop Enable"},
	{"Right2 Analog Loopback", NULL, "FM Loop Enable"},
	{"Left2 Analog Loopback", NULL, "FM Loop Enable"},
	{"Voice Analog Loopback", NULL, "FM Loop Enable"},

	{"Analog R1 Playback Mixer", NULL, "Right1 Analog Loopback"},
	{"Analog L1 Playback Mixer", NULL, "Left1 Analog Loopback"},
	{"Analog R2 Playback Mixer", NULL, "Right2 Analog Loopback"},
	{"Analog L2 Playback Mixer", NULL, "Left2 Analog Loopback"},
	{"Analog Voice Playback Mixer", NULL, "Voice Analog Loopback"},

	 
	{"Right Digital Loopback", "Volume", "TX1 Capture Route"},
	{"Left Digital Loopback", "Volume", "TX1 Capture Route"},
	{"Voice Digital Loopback", "Volume", "TX2 Capture Route"},

	{"Digital R2 Playback Mixer", NULL, "Right Digital Loopback"},
	{"Digital L2 Playback Mixer", NULL, "Left Digital Loopback"},
	{"Digital Voice Playback Mixer", NULL, "Voice Digital Loopback"},

};

static int twl4030_set_bias_level(struct snd_soc_component *component,
				  enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (snd_soc_component_get_bias_level(component) == SND_SOC_BIAS_OFF)
			twl4030_codec_enable(component, 1);
		break;
	case SND_SOC_BIAS_OFF:
		twl4030_codec_enable(component, 0);
		break;
	}

	return 0;
}

static void twl4030_constraints(struct twl4030_priv *twl4030,
				struct snd_pcm_substream *mst_substream)
{
	struct snd_pcm_substream *slv_substream;

	 
	if (mst_substream == twl4030->master_substream)
		slv_substream = twl4030->slave_substream;
	else if (mst_substream == twl4030->slave_substream)
		slv_substream = twl4030->master_substream;
	else  
		return;

	 
	snd_pcm_hw_constraint_single(slv_substream->runtime,
				SNDRV_PCM_HW_PARAM_RATE,
				twl4030->rate);

	snd_pcm_hw_constraint_single(slv_substream->runtime,
				SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
				twl4030->sample_bits);

	snd_pcm_hw_constraint_single(slv_substream->runtime,
				SNDRV_PCM_HW_PARAM_CHANNELS,
				twl4030->channels);
}

 
static void twl4030_tdm_enable(struct snd_soc_component *component, int direction,
			       int enable)
{
	u8 reg, mask;

	reg = twl4030_read(component, TWL4030_REG_OPTION);

	if (direction == SNDRV_PCM_STREAM_PLAYBACK)
		mask = TWL4030_ARXL1_VRX_EN | TWL4030_ARXR1_EN;
	else
		mask = TWL4030_ATXL2_VTXL_EN | TWL4030_ATXR2_VTXR_EN;

	if (enable)
		reg |= mask;
	else
		reg &= ~mask;

	twl4030_write(component, TWL4030_REG_OPTION, reg);
}

static int twl4030_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);

	if (twl4030->master_substream) {
		twl4030->slave_substream = substream;
		 
		if (twl4030->configured)
			twl4030_constraints(twl4030, twl4030->master_substream);
	} else {
		if (!(twl4030_read(component, TWL4030_REG_CODEC_MODE) &
			TWL4030_OPTION_1)) {
			 
			snd_pcm_hw_constraint_single(substream->runtime,
						     SNDRV_PCM_HW_PARAM_CHANNELS,
						     2);
		}
		twl4030->master_substream = substream;
	}

	return 0;
}

static void twl4030_shutdown(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);

	if (twl4030->master_substream == substream)
		twl4030->master_substream = twl4030->slave_substream;

	twl4030->slave_substream = NULL;

	 
	if (!twl4030->master_substream)
		twl4030->configured = 0;
	 else if (!twl4030->master_substream->runtime->channels)
		twl4030->configured = 0;

	  
	if (substream->runtime->channels == 4)
		twl4030_tdm_enable(component, substream->stream, 0);
}

static int twl4030_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	u8 mode, old_mode, format, old_format;

	  
	if (params_channels(params) == 4) {
		format = twl4030_read(component, TWL4030_REG_AUDIO_IF);
		mode = twl4030_read(component, TWL4030_REG_CODEC_MODE);

		 
		if ((mode & TWL4030_OPTION_1) &&
		    ((format & TWL4030_AIF_FORMAT) == TWL4030_AIF_FORMAT_TDM))
			twl4030_tdm_enable(component, substream->stream, 1);
		else
			return -EINVAL;
	}

	if (twl4030->configured)
		 
		return 0;

	 
	old_mode = twl4030_read(component,
				TWL4030_REG_CODEC_MODE) & ~TWL4030_CODECPDZ;
	mode = old_mode & ~TWL4030_APLL_RATE;

	switch (params_rate(params)) {
	case 8000:
		mode |= TWL4030_APLL_RATE_8000;
		break;
	case 11025:
		mode |= TWL4030_APLL_RATE_11025;
		break;
	case 12000:
		mode |= TWL4030_APLL_RATE_12000;
		break;
	case 16000:
		mode |= TWL4030_APLL_RATE_16000;
		break;
	case 22050:
		mode |= TWL4030_APLL_RATE_22050;
		break;
	case 24000:
		mode |= TWL4030_APLL_RATE_24000;
		break;
	case 32000:
		mode |= TWL4030_APLL_RATE_32000;
		break;
	case 44100:
		mode |= TWL4030_APLL_RATE_44100;
		break;
	case 48000:
		mode |= TWL4030_APLL_RATE_48000;
		break;
	case 96000:
		mode |= TWL4030_APLL_RATE_96000;
		break;
	default:
		dev_err(component->dev, "%s: unknown rate %d\n", __func__,
			params_rate(params));
		return -EINVAL;
	}

	 
	old_format = twl4030_read(component, TWL4030_REG_AUDIO_IF);
	format = old_format;
	format &= ~TWL4030_DATA_WIDTH;
	switch (params_width(params)) {
	case 16:
		format |= TWL4030_DATA_WIDTH_16S_16W;
		break;
	case 32:
		format |= TWL4030_DATA_WIDTH_32S_24W;
		break;
	default:
		dev_err(component->dev, "%s: unsupported bits/sample %d\n",
			__func__, params_width(params));
		return -EINVAL;
	}

	if (format != old_format || mode != old_mode) {
		if (twl4030->codec_powered) {
			 
			twl4030_codec_enable(component, 0);
			twl4030_write(component, TWL4030_REG_CODEC_MODE, mode);
			twl4030_write(component, TWL4030_REG_AUDIO_IF, format);
			twl4030_codec_enable(component, 1);
		} else {
			twl4030_write(component, TWL4030_REG_CODEC_MODE, mode);
			twl4030_write(component, TWL4030_REG_AUDIO_IF, format);
		}
	}

	 
	twl4030->configured = 1;
	twl4030->rate = params_rate(params);
	twl4030->sample_bits = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_SAMPLE_BITS)->min;
	twl4030->channels = params_channels(params);

	 
	if (twl4030->slave_substream)
		twl4030_constraints(twl4030, substream);

	return 0;
}

static int twl4030_set_dai_sysclk(struct snd_soc_dai *codec_dai, int clk_id,
				  unsigned int freq, int dir)
{
	struct snd_soc_component *component = codec_dai->component;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);

	switch (freq) {
	case 19200000:
	case 26000000:
	case 38400000:
		break;
	default:
		dev_err(component->dev, "Unsupported HFCLKIN: %u\n", freq);
		return -EINVAL;
	}

	if ((freq / 1000) != twl4030->sysclk) {
		dev_err(component->dev,
			"Mismatch in HFCLKIN: %u (configured: %u)\n",
			freq, twl4030->sysclk * 1000);
		return -EINVAL;
	}

	return 0;
}

static int twl4030_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	u8 old_format, format;

	 
	old_format = twl4030_read(component, TWL4030_REG_AUDIO_IF);
	format = old_format;

	switch (fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
	case SND_SOC_DAIFMT_CBP_CFP:
		format &= ~(TWL4030_AIF_SLAVE_EN);
		format &= ~(TWL4030_CLK256FS_EN);
		break;
	case SND_SOC_DAIFMT_CBC_CFC:
		format |= TWL4030_AIF_SLAVE_EN;
		format |= TWL4030_CLK256FS_EN;
		break;
	default:
		return -EINVAL;
	}

	 
	format &= ~TWL4030_AIF_FORMAT;
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format |= TWL4030_AIF_FORMAT_CODEC;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		format |= TWL4030_AIF_FORMAT_TDM;
		break;
	default:
		return -EINVAL;
	}

	if (format != old_format) {
		if (twl4030->codec_powered) {
			 
			twl4030_codec_enable(component, 0);
			twl4030_write(component, TWL4030_REG_AUDIO_IF, format);
			twl4030_codec_enable(component, 1);
		} else {
			twl4030_write(component, TWL4030_REG_AUDIO_IF, format);
		}
	}

	return 0;
}

static int twl4030_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_component *component = dai->component;
	u8 reg = twl4030_read(component, TWL4030_REG_AUDIO_IF);

	if (tristate)
		reg |= TWL4030_AIF_TRI_EN;
	else
		reg &= ~TWL4030_AIF_TRI_EN;

	return twl4030_write(component, TWL4030_REG_AUDIO_IF, reg);
}

 
static void twl4030_voice_enable(struct snd_soc_component *component, int direction,
				 int enable)
{
	u8 reg, mask;

	reg = twl4030_read(component, TWL4030_REG_OPTION);

	if (direction == SNDRV_PCM_STREAM_PLAYBACK)
		mask = TWL4030_ARXL1_VRX_EN;
	else
		mask = TWL4030_ATXL2_VTXL_EN | TWL4030_ATXR2_VTXR_EN;

	if (enable)
		reg |= mask;
	else
		reg &= ~mask;

	twl4030_write(component, TWL4030_REG_OPTION, reg);
}

static int twl4030_voice_startup(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	u8 mode;

	 
	if (twl4030->sysclk != 26000) {
		dev_err(component->dev,
			"%s: HFCLKIN is %u KHz, voice interface needs 26MHz\n",
			__func__, twl4030->sysclk);
		return -EINVAL;
	}

	 
	mode = twl4030_read(component, TWL4030_REG_CODEC_MODE)
		& TWL4030_OPT_MODE;

	if (mode != TWL4030_OPTION_2) {
		dev_err(component->dev, "%s: the codec mode is not option2\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

static void twl4030_voice_shutdown(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	 
	twl4030_voice_enable(component, substream->stream, 0);
}

static int twl4030_voice_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	u8 old_mode, mode;

	 
	twl4030_voice_enable(component, substream->stream, 1);

	 
	old_mode = twl4030_read(component,
				TWL4030_REG_CODEC_MODE) & ~TWL4030_CODECPDZ;
	mode = old_mode;

	switch (params_rate(params)) {
	case 8000:
		mode &= ~(TWL4030_SEL_16K);
		break;
	case 16000:
		mode |= TWL4030_SEL_16K;
		break;
	default:
		dev_err(component->dev, "%s: unknown rate %d\n", __func__,
			params_rate(params));
		return -EINVAL;
	}

	if (mode != old_mode) {
		if (twl4030->codec_powered) {
			 
			twl4030_codec_enable(component, 0);
			twl4030_write(component, TWL4030_REG_CODEC_MODE, mode);
			twl4030_codec_enable(component, 1);
		} else {
			twl4030_write(component, TWL4030_REG_CODEC_MODE, mode);
		}
	}

	return 0;
}

static int twl4030_voice_set_dai_sysclk(struct snd_soc_dai *codec_dai,
					int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *component = codec_dai->component;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);

	if (freq != 26000000) {
		dev_err(component->dev,
			"%s: HFCLKIN is %u KHz, voice interface needs 26MHz\n",
			__func__, freq / 1000);
		return -EINVAL;
	}
	if ((freq / 1000) != twl4030->sysclk) {
		dev_err(component->dev,
			"Mismatch in HFCLKIN: %u (configured: %u)\n",
			freq, twl4030->sysclk * 1000);
		return -EINVAL;
	}
	return 0;
}

static int twl4030_voice_set_dai_fmt(struct snd_soc_dai *codec_dai,
				     unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	u8 old_format, format;

	 
	old_format = twl4030_read(component, TWL4030_REG_VOICE_IF);
	format = old_format;

	switch (fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
	case SND_SOC_DAIFMT_CBP_CFP:
		format &= ~(TWL4030_VIF_SLAVE_EN);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		format |= TWL4030_VIF_SLAVE_EN;
		break;
	default:
		return -EINVAL;
	}

	 
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_NF:
		format &= ~(TWL4030_VIF_FORMAT);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		format |= TWL4030_VIF_FORMAT;
		break;
	default:
		return -EINVAL;
	}

	if (format != old_format) {
		if (twl4030->codec_powered) {
			 
			twl4030_codec_enable(component, 0);
			twl4030_write(component, TWL4030_REG_VOICE_IF, format);
			twl4030_codec_enable(component, 1);
		} else {
			twl4030_write(component, TWL4030_REG_VOICE_IF, format);
		}
	}

	return 0;
}

static int twl4030_voice_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct snd_soc_component *component = dai->component;
	u8 reg = twl4030_read(component, TWL4030_REG_VOICE_IF);

	if (tristate)
		reg |= TWL4030_VIF_TRI_EN;
	else
		reg &= ~TWL4030_VIF_TRI_EN;

	return twl4030_write(component, TWL4030_REG_VOICE_IF, reg);
}

#define TWL4030_RATES	 (SNDRV_PCM_RATE_8000_48000)
#define TWL4030_FORMATS	 (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops twl4030_dai_hifi_ops = {
	.startup	= twl4030_startup,
	.shutdown	= twl4030_shutdown,
	.hw_params	= twl4030_hw_params,
	.set_sysclk	= twl4030_set_dai_sysclk,
	.set_fmt	= twl4030_set_dai_fmt,
	.set_tristate	= twl4030_set_tristate,
};

static const struct snd_soc_dai_ops twl4030_dai_voice_ops = {
	.startup	= twl4030_voice_startup,
	.shutdown	= twl4030_voice_shutdown,
	.hw_params	= twl4030_voice_hw_params,
	.set_sysclk	= twl4030_voice_set_dai_sysclk,
	.set_fmt	= twl4030_voice_set_dai_fmt,
	.set_tristate	= twl4030_voice_set_tristate,
};

static struct snd_soc_dai_driver twl4030_dai[] = {
{
	.name = "twl4030-hifi",
	.playback = {
		.stream_name = "HiFi Playback",
		.channels_min = 2,
		.channels_max = 4,
		.rates = TWL4030_RATES | SNDRV_PCM_RATE_96000,
		.formats = TWL4030_FORMATS,
		.sig_bits = 24,},
	.capture = {
		.stream_name = "HiFi Capture",
		.channels_min = 2,
		.channels_max = 4,
		.rates = TWL4030_RATES,
		.formats = TWL4030_FORMATS,
		.sig_bits = 24,},
	.ops = &twl4030_dai_hifi_ops,
},
{
	.name = "twl4030-voice",
	.playback = {
		.stream_name = "Voice Playback",
		.channels_min = 1,
		.channels_max = 1,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.stream_name = "Voice Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = &twl4030_dai_voice_ops,
},
};

static int twl4030_soc_probe(struct snd_soc_component *component)
{
	struct twl4030_priv *twl4030;

	twl4030 = devm_kzalloc(component->dev, sizeof(struct twl4030_priv),
			       GFP_KERNEL);
	if (!twl4030)
		return -ENOMEM;
	snd_soc_component_set_drvdata(component, twl4030);
	 
	twl4030->sysclk = twl4030_audio_get_mclk() / 1000;

	twl4030_init_chip(component);

	return 0;
}

static void twl4030_soc_remove(struct snd_soc_component *component)
{
	struct twl4030_priv *twl4030 = snd_soc_component_get_drvdata(component);
	struct twl4030_board_params *board_params = twl4030->board_params;

	if (board_params && board_params->hs_extmute &&
	    gpio_is_valid(board_params->hs_extmute_gpio))
		gpio_free(board_params->hs_extmute_gpio);
}

static const struct snd_soc_component_driver soc_component_dev_twl4030 = {
	.probe			= twl4030_soc_probe,
	.remove			= twl4030_soc_remove,
	.read			= twl4030_read,
	.write			= twl4030_write,
	.set_bias_level		= twl4030_set_bias_level,
	.controls		= twl4030_snd_controls,
	.num_controls		= ARRAY_SIZE(twl4030_snd_controls),
	.dapm_widgets		= twl4030_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(twl4030_dapm_widgets),
	.dapm_routes		= intercon,
	.num_dapm_routes	= ARRAY_SIZE(intercon),
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

static int twl4030_codec_probe(struct platform_device *pdev)
{
	return devm_snd_soc_register_component(&pdev->dev,
				      &soc_component_dev_twl4030,
				      twl4030_dai, ARRAY_SIZE(twl4030_dai));
}

MODULE_ALIAS("platform:twl4030-codec");

static struct platform_driver twl4030_codec_driver = {
	.probe		= twl4030_codec_probe,
	.driver		= {
		.name	= "twl4030-codec",
	},
};

module_platform_driver(twl4030_codec_driver);

MODULE_DESCRIPTION("ASoC TWL4030 codec driver");
MODULE_AUTHOR("Steve Sakoman");
MODULE_LICENSE("GPL");
