
 

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mfd/wm97xx.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/ac97_codec.h>
#include <sound/ac97/codec.h>
#include <sound/ac97/compat.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>

#include "wm9713.h"

#define WM9713_VENDOR_ID 0x574d4c13
#define WM9713_VENDOR_ID_MASK 0xffffffff

struct wm9713_priv {
	struct snd_ac97 *ac97;
	u32 pll_in;  
	unsigned int hp_mixer[2];
	struct mutex lock;
	struct wm97xx_platform_data *mfd_pdata;
};

#define HPL_MIXER 0
#define HPR_MIXER 1

static const char *wm9713_mic_mixer[] = {"Stereo", "Mic 1", "Mic 2", "Mute"};
static const char *wm9713_rec_mux[] = {"Stereo", "Left", "Right", "Mute"};
static const char *wm9713_rec_src[] =
	{"Mic 1", "Mic 2", "Line", "Mono In", "Headphone", "Speaker",
	"Mono Out", "Zh"};
static const char *wm9713_rec_gain[] = {"+1.5dB Steps", "+0.75dB Steps"};
static const char *wm9713_alc_select[] = {"None", "Left", "Right", "Stereo"};
static const char *wm9713_mono_pga[] = {"Vmid", "Zh", "Mono", "Inv"};
static const char *wm9713_spk_pga[] =
	{"Vmid", "Zh", "Headphone", "Speaker", "Inv", "Headphone Vmid",
	"Speaker Vmid", "Inv Vmid"};
static const char *wm9713_hp_pga[] = {"Vmid", "Zh", "Headphone",
	"Headphone Vmid"};
static const char *wm9713_out3_pga[] = {"Vmid", "Zh", "Inv 1", "Inv 1 Vmid"};
static const char *wm9713_out4_pga[] = {"Vmid", "Zh", "Inv 2", "Inv 2 Vmid"};
static const char *wm9713_dac_inv[] =
	{"Off", "Mono", "Speaker", "Left Headphone", "Right Headphone",
	"Headphone Mono", "NC", "Vmid"};
static const char *wm9713_bass[] = {"Linear Control", "Adaptive Boost"};
static const char *wm9713_ng_type[] = {"Constant Gain", "Mute"};
static const char *wm9713_mic_select[] = {"Mic 1", "Mic 2 A", "Mic 2 B"};
static const char *wm9713_micb_select[] = {"MPB", "MPA"};

static const struct soc_enum wm9713_enum[] = {
SOC_ENUM_SINGLE(AC97_LINE, 3, 4, wm9713_mic_mixer),  
SOC_ENUM_SINGLE(AC97_VIDEO, 14, 4, wm9713_rec_mux),  
SOC_ENUM_SINGLE(AC97_VIDEO, 9, 4, wm9713_rec_mux),   
SOC_ENUM_SINGLE(AC97_VIDEO, 3, 8, wm9713_rec_src),   
SOC_ENUM_SINGLE(AC97_VIDEO, 0, 8, wm9713_rec_src),   
SOC_ENUM_DOUBLE(AC97_CD, 14, 6, 2, wm9713_rec_gain),  
SOC_ENUM_SINGLE(AC97_PCI_SVID, 14, 4, wm9713_alc_select),  
SOC_ENUM_SINGLE(AC97_REC_GAIN, 14, 4, wm9713_mono_pga),  
SOC_ENUM_SINGLE(AC97_REC_GAIN, 11, 8, wm9713_spk_pga),  
SOC_ENUM_SINGLE(AC97_REC_GAIN, 8, 8, wm9713_spk_pga),  
SOC_ENUM_SINGLE(AC97_REC_GAIN, 6, 3, wm9713_hp_pga),  
SOC_ENUM_SINGLE(AC97_REC_GAIN, 4, 3, wm9713_hp_pga),  
SOC_ENUM_SINGLE(AC97_REC_GAIN, 2, 4, wm9713_out3_pga),  
SOC_ENUM_SINGLE(AC97_REC_GAIN, 0, 4, wm9713_out4_pga),  
SOC_ENUM_SINGLE(AC97_REC_GAIN_MIC, 13, 8, wm9713_dac_inv),  
SOC_ENUM_SINGLE(AC97_REC_GAIN_MIC, 10, 8, wm9713_dac_inv),  
SOC_ENUM_SINGLE(AC97_GENERAL_PURPOSE, 15, 2, wm9713_bass),  
SOC_ENUM_SINGLE(AC97_PCI_SVID, 5, 2, wm9713_ng_type),  
SOC_ENUM_SINGLE(AC97_3D_CONTROL, 12, 3, wm9713_mic_select),  
SOC_ENUM_SINGLE_VIRT(2, wm9713_micb_select),  
};

static const DECLARE_TLV_DB_SCALE(out_tlv, -4650, 150, 0);
static const DECLARE_TLV_DB_SCALE(main_tlv, -3450, 150, 0);
static const DECLARE_TLV_DB_SCALE(misc_tlv, -1500, 300, 0);
static const  DECLARE_TLV_DB_RANGE(mic_tlv,
	0, 2, TLV_DB_SCALE_ITEM(1200, 600, 0),
	3, 3, TLV_DB_SCALE_ITEM(3000, 0, 0)
);

static const struct snd_kcontrol_new wm9713_snd_ac97_controls[] = {
SOC_DOUBLE_TLV("Speaker Playback Volume", AC97_MASTER, 8, 0, 31, 1, out_tlv),
SOC_DOUBLE("Speaker Playback Switch", AC97_MASTER, 15, 7, 1, 1),
SOC_DOUBLE_TLV("Headphone Playback Volume", AC97_HEADPHONE, 8, 0, 31, 1,
	       out_tlv),
SOC_DOUBLE("Headphone Playback Switch", AC97_HEADPHONE, 15, 7, 1, 1),
SOC_DOUBLE_TLV("Line In Volume", AC97_PC_BEEP, 8, 0, 31, 1, main_tlv),
SOC_DOUBLE_TLV("PCM Playback Volume", AC97_PHONE, 8, 0, 31, 1, main_tlv),
SOC_SINGLE_TLV("Mic 1 Volume", AC97_MIC, 8, 31, 1, main_tlv),
SOC_SINGLE_TLV("Mic 2 Volume", AC97_MIC, 0, 31, 1, main_tlv),
SOC_SINGLE_TLV("Mic 1 Preamp Volume", AC97_3D_CONTROL, 10, 3, 0, mic_tlv),
SOC_SINGLE_TLV("Mic 2 Preamp Volume", AC97_3D_CONTROL, 12, 3, 0, mic_tlv),

SOC_SINGLE("Mic Boost (+20dB) Switch", AC97_LINE, 5, 1, 0),
SOC_SINGLE("Mic Headphone Mixer Volume", AC97_LINE, 0, 7, 1),

SOC_SINGLE("Capture Switch", AC97_CD, 15, 1, 1),
SOC_ENUM("Capture Volume Steps", wm9713_enum[5]),
SOC_DOUBLE("Capture Volume", AC97_CD, 8, 0, 31, 0),
SOC_SINGLE("Capture ZC Switch", AC97_CD, 7, 1, 0),

SOC_SINGLE_TLV("Capture to Headphone Volume", AC97_VIDEO, 11, 7, 1, misc_tlv),
SOC_SINGLE("Capture to Mono Boost (+20dB) Switch", AC97_VIDEO, 8, 1, 0),
SOC_SINGLE("Capture ADC Boost (+20dB) Switch", AC97_VIDEO, 6, 1, 0),

SOC_SINGLE("ALC Target Volume", AC97_CODEC_CLASS_REV, 12, 15, 0),
SOC_SINGLE("ALC Hold Time", AC97_CODEC_CLASS_REV, 8, 15, 0),
SOC_SINGLE("ALC Decay Time", AC97_CODEC_CLASS_REV, 4, 15, 0),
SOC_SINGLE("ALC Attack Time", AC97_CODEC_CLASS_REV, 0, 15, 0),
SOC_ENUM("ALC Function", wm9713_enum[6]),
SOC_SINGLE("ALC Max Volume", AC97_PCI_SVID, 11, 7, 0),
SOC_SINGLE("ALC ZC Timeout", AC97_PCI_SVID, 9, 3, 0),
SOC_SINGLE("ALC ZC Switch", AC97_PCI_SVID, 8, 1, 0),
SOC_SINGLE("ALC NG Switch", AC97_PCI_SVID, 7, 1, 0),
SOC_ENUM("ALC NG Type", wm9713_enum[17]),
SOC_SINGLE("ALC NG Threshold", AC97_PCI_SVID, 0, 31, 0),

SOC_DOUBLE("Speaker Playback ZC Switch", AC97_MASTER, 14, 6, 1, 0),
SOC_DOUBLE("Headphone Playback ZC Switch", AC97_HEADPHONE, 14, 6, 1, 0),

SOC_SINGLE("Out4 Playback Switch", AC97_MASTER_MONO, 15, 1, 1),
SOC_SINGLE("Out4 Playback ZC Switch", AC97_MASTER_MONO, 14, 1, 0),
SOC_SINGLE_TLV("Out4 Playback Volume", AC97_MASTER_MONO, 8, 31, 1, out_tlv),

SOC_SINGLE("Out3 Playback Switch", AC97_MASTER_MONO, 7, 1, 1),
SOC_SINGLE("Out3 Playback ZC Switch", AC97_MASTER_MONO, 6, 1, 0),
SOC_SINGLE_TLV("Out3 Playback Volume", AC97_MASTER_MONO, 0, 31, 1, out_tlv),

SOC_SINGLE_TLV("Mono Capture Volume", AC97_MASTER_TONE, 8, 31, 1, main_tlv),
SOC_SINGLE("Mono Playback Switch", AC97_MASTER_TONE, 7, 1, 1),
SOC_SINGLE("Mono Playback ZC Switch", AC97_MASTER_TONE, 6, 1, 0),
SOC_SINGLE_TLV("Mono Playback Volume", AC97_MASTER_TONE, 0, 31, 1, out_tlv),

SOC_SINGLE_TLV("Headphone Mixer Beep Playback Volume", AC97_AUX, 12, 7, 1,
	       misc_tlv),
SOC_SINGLE_TLV("Speaker Mixer Beep Playback Volume", AC97_AUX, 8, 7, 1,
	       misc_tlv),
SOC_SINGLE_TLV("Mono Mixer Beep Playback Volume", AC97_AUX, 4, 7, 1, misc_tlv),

SOC_SINGLE_TLV("Voice Playback Headphone Volume", AC97_PCM, 12, 7, 1,
	       misc_tlv),
SOC_SINGLE("Voice Playback Master Volume", AC97_PCM, 8, 7, 1),
SOC_SINGLE("Voice Playback Mono Volume", AC97_PCM, 4, 7, 1),

SOC_SINGLE_TLV("Headphone Mixer Aux Playback Volume", AC97_REC_SEL, 12, 7, 1,
	       misc_tlv),

SOC_SINGLE_TLV("Speaker Mixer Voice Playback Volume", AC97_PCM, 8, 7, 1,
	       misc_tlv),
SOC_SINGLE_TLV("Speaker Mixer Aux Playback Volume", AC97_REC_SEL, 8, 7, 1,
	       misc_tlv),

SOC_SINGLE_TLV("Mono Mixer Voice Playback Volume", AC97_PCM, 4, 7, 1,
	       misc_tlv),
SOC_SINGLE_TLV("Mono Mixer Aux Playback Volume", AC97_REC_SEL, 4, 7, 1,
	       misc_tlv),

SOC_SINGLE("Aux Playback Headphone Volume", AC97_REC_SEL, 12, 7, 1),
SOC_SINGLE("Aux Playback Master Volume", AC97_REC_SEL, 8, 7, 1),

SOC_ENUM("Bass Control", wm9713_enum[16]),
SOC_SINGLE("Bass Cut-off Switch", AC97_GENERAL_PURPOSE, 12, 1, 1),
SOC_SINGLE("Tone Cut-off Switch", AC97_GENERAL_PURPOSE, 4, 1, 1),
SOC_SINGLE("Playback Attenuate (-6dB) Switch", AC97_GENERAL_PURPOSE, 6, 1, 0),
SOC_SINGLE("Bass Volume", AC97_GENERAL_PURPOSE, 8, 15, 1),
SOC_SINGLE("Tone Volume", AC97_GENERAL_PURPOSE, 0, 15, 1),

SOC_SINGLE("3D Upper Cut-off Switch", AC97_REC_GAIN_MIC, 5, 1, 0),
SOC_SINGLE("3D Lower Cut-off Switch", AC97_REC_GAIN_MIC, 4, 1, 0),
SOC_SINGLE("3D Depth", AC97_REC_GAIN_MIC, 0, 15, 1),
};

static int wm9713_voice_shutdown(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);

	if (WARN_ON(event != SND_SOC_DAPM_PRE_PMD))
		return -EINVAL;

	 
	snd_soc_component_update_bits(component, AC97_HANDSET_RATE, 0x0f00, 0x0200);
	schedule_timeout_interruptible(msecs_to_jiffies(1));
	snd_soc_component_update_bits(component, AC97_HANDSET_RATE, 0x0f00, 0x0f00);
	snd_soc_component_update_bits(component, AC97_EXTENDED_MID, 0x1000, 0x1000);

	return 0;
}

static const unsigned int wm9713_mixer_mute_regs[] = {
	AC97_PC_BEEP,
	AC97_MASTER_TONE,
	AC97_PHONE,
	AC97_REC_SEL,
	AC97_PCM,
	AC97_AUX,
};

 
static int wm9713_hp_mixer_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *component = snd_soc_dapm_to_component(dapm);
	struct wm9713_priv *wm9713 = snd_soc_component_get_drvdata(component);
	unsigned int val = ucontrol->value.integer.value[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int mixer, mask, shift, old;
	struct snd_soc_dapm_update update = {};
	bool change;

	mixer = mc->shift >> 8;
	shift = mc->shift & 0xff;
	mask = (1 << shift);

	mutex_lock(&wm9713->lock);
	old = wm9713->hp_mixer[mixer];
	if (ucontrol->value.integer.value[0])
		wm9713->hp_mixer[mixer] |= mask;
	else
		wm9713->hp_mixer[mixer] &= ~mask;

	change = old != wm9713->hp_mixer[mixer];
	if (change) {
		update.kcontrol = kcontrol;
		update.reg = wm9713_mixer_mute_regs[shift];
		update.mask = 0x8000;
		if ((wm9713->hp_mixer[0] & mask) ||
		    (wm9713->hp_mixer[1] & mask))
			update.val = 0x0;
		else
			update.val = 0x8000;

		snd_soc_dapm_mixer_update_power(dapm, kcontrol, val,
			&update);
	}

	mutex_unlock(&wm9713->lock);

	return change;
}

static int wm9713_hp_mixer_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kcontrol);
	struct snd_soc_component *component = snd_soc_dapm_to_component(dapm);
	struct wm9713_priv *wm9713 = snd_soc_component_get_drvdata(component);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int mixer, shift;

	mixer = mc->shift >> 8;
	shift = mc->shift & 0xff;

	ucontrol->value.integer.value[0] =
		(wm9713->hp_mixer[mixer] >> shift) & 1;

	return 0;
}

#define WM9713_HP_MIXER_CTRL(xname, xmixer, xshift) { \
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_volsw, \
	.get = wm9713_hp_mixer_get, .put = wm9713_hp_mixer_put, \
	.private_value = SOC_DOUBLE_VALUE(SND_SOC_NOPM, \
		xshift, xmixer, 1, 0, 0) \
}

 
static const struct snd_kcontrol_new wm9713_hpl_mixer_controls[] = {
WM9713_HP_MIXER_CTRL("Beep Playback Switch", HPL_MIXER, 5),
WM9713_HP_MIXER_CTRL("Voice Playback Switch", HPL_MIXER, 4),
WM9713_HP_MIXER_CTRL("Aux Playback Switch", HPL_MIXER, 3),
WM9713_HP_MIXER_CTRL("PCM Playback Switch", HPL_MIXER, 2),
WM9713_HP_MIXER_CTRL("MonoIn Playback Switch", HPL_MIXER, 1),
WM9713_HP_MIXER_CTRL("Bypass Playback Switch", HPL_MIXER, 0),
};

 
static const struct snd_kcontrol_new wm9713_hpr_mixer_controls[] = {
WM9713_HP_MIXER_CTRL("Beep Playback Switch", HPR_MIXER, 5),
WM9713_HP_MIXER_CTRL("Voice Playback Switch", HPR_MIXER, 4),
WM9713_HP_MIXER_CTRL("Aux Playback Switch", HPR_MIXER, 3),
WM9713_HP_MIXER_CTRL("PCM Playback Switch", HPR_MIXER, 2),
WM9713_HP_MIXER_CTRL("MonoIn Playback Switch", HPR_MIXER, 1),
WM9713_HP_MIXER_CTRL("Bypass Playback Switch", HPR_MIXER, 0),
};

 
static const struct snd_kcontrol_new wm9713_hp_rec_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[1]);

 
static const struct snd_kcontrol_new wm9713_hp_mic_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[0]);

 
static const struct snd_kcontrol_new wm9713_speaker_mixer_controls[] = {
SOC_DAPM_SINGLE("Beep Playback Switch", AC97_AUX, 11, 1, 1),
SOC_DAPM_SINGLE("Voice Playback Switch", AC97_PCM, 11, 1, 1),
SOC_DAPM_SINGLE("Aux Playback Switch", AC97_REC_SEL, 11, 1, 1),
SOC_DAPM_SINGLE("PCM Playback Switch", AC97_PHONE, 14, 1, 1),
SOC_DAPM_SINGLE("MonoIn Playback Switch", AC97_MASTER_TONE, 14, 1, 1),
SOC_DAPM_SINGLE("Bypass Playback Switch", AC97_PC_BEEP, 14, 1, 1),
};

 
static const struct snd_kcontrol_new wm9713_mono_mixer_controls[] = {
SOC_DAPM_SINGLE("Beep Playback Switch", AC97_AUX, 7, 1, 1),
SOC_DAPM_SINGLE("Voice Playback Switch", AC97_PCM, 7, 1, 1),
SOC_DAPM_SINGLE("Aux Playback Switch", AC97_REC_SEL, 7, 1, 1),
SOC_DAPM_SINGLE("PCM Playback Switch", AC97_PHONE, 13, 1, 1),
SOC_DAPM_SINGLE("MonoIn Playback Switch", AC97_MASTER_TONE, 13, 1, 1),
SOC_DAPM_SINGLE("Bypass Playback Switch", AC97_PC_BEEP, 13, 1, 1),
SOC_DAPM_SINGLE("Mic 1 Sidetone Switch", AC97_LINE, 7, 1, 1),
SOC_DAPM_SINGLE("Mic 2 Sidetone Switch", AC97_LINE, 6, 1, 1),
};

 
static const struct snd_kcontrol_new wm9713_mono_mic_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[2]);

 
static const struct snd_kcontrol_new wm9713_mono_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[7]);

 
static const struct snd_kcontrol_new wm9713_hp_spkl_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[8]);

 
static const struct snd_kcontrol_new wm9713_hp_spkr_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[9]);

 
static const struct snd_kcontrol_new wm9713_hpl_out_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[10]);

 
static const struct snd_kcontrol_new wm9713_hpr_out_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[11]);

 
static const struct snd_kcontrol_new wm9713_out3_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[12]);

 
static const struct snd_kcontrol_new wm9713_out4_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[13]);

 
static const struct snd_kcontrol_new wm9713_dac_inv1_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[14]);

 
static const struct snd_kcontrol_new wm9713_dac_inv2_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[15]);

 
static const struct snd_kcontrol_new wm9713_rec_srcl_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[3]);

 
static const struct snd_kcontrol_new wm9713_rec_srcr_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[4]);

 
static const struct snd_kcontrol_new wm9713_mic_sel_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[18]);

 
static const struct snd_kcontrol_new wm9713_micb_sel_mux_controls =
SOC_DAPM_ENUM("Route", wm9713_enum[19]);

static const struct snd_soc_dapm_widget wm9713_dapm_widgets[] = {
SND_SOC_DAPM_MUX("Capture Headphone Mux", SND_SOC_NOPM, 0, 0,
	&wm9713_hp_rec_mux_controls),
SND_SOC_DAPM_MUX("Sidetone Mux", SND_SOC_NOPM, 0, 0,
	&wm9713_hp_mic_mux_controls),
SND_SOC_DAPM_MUX("Capture Mono Mux", SND_SOC_NOPM, 0, 0,
	&wm9713_mono_mic_mux_controls),
SND_SOC_DAPM_MUX("Mono Out Mux", SND_SOC_NOPM, 0, 0,
	&wm9713_mono_mux_controls),
SND_SOC_DAPM_MUX("Left Speaker Out Mux", SND_SOC_NOPM, 0, 0,
	&wm9713_hp_spkl_mux_controls),
SND_SOC_DAPM_MUX("Right Speaker Out Mux", SND_SOC_NOPM, 0, 0,
	&wm9713_hp_spkr_mux_controls),
SND_SOC_DAPM_MUX("Left Headphone Out Mux", SND_SOC_NOPM, 0, 0,
	&wm9713_hpl_out_mux_controls),
SND_SOC_DAPM_MUX("Right Headphone Out Mux", SND_SOC_NOPM, 0, 0,
	&wm9713_hpr_out_mux_controls),
SND_SOC_DAPM_MUX("Out 3 Mux", SND_SOC_NOPM, 0, 0,
	&wm9713_out3_mux_controls),
SND_SOC_DAPM_MUX("Out 4 Mux", SND_SOC_NOPM, 0, 0,
	&wm9713_out4_mux_controls),
SND_SOC_DAPM_MUX("DAC Inv Mux 1", SND_SOC_NOPM, 0, 0,
	&wm9713_dac_inv1_mux_controls),
SND_SOC_DAPM_MUX("DAC Inv Mux 2", SND_SOC_NOPM, 0, 0,
	&wm9713_dac_inv2_mux_controls),
SND_SOC_DAPM_MUX("Left Capture Source", SND_SOC_NOPM, 0, 0,
	&wm9713_rec_srcl_mux_controls),
SND_SOC_DAPM_MUX("Right Capture Source", SND_SOC_NOPM, 0, 0,
	&wm9713_rec_srcr_mux_controls),
SND_SOC_DAPM_MUX("Mic A Source", SND_SOC_NOPM, 0, 0,
	&wm9713_mic_sel_mux_controls),
SND_SOC_DAPM_MUX("Mic B Source", SND_SOC_NOPM, 0, 0,
	&wm9713_micb_sel_mux_controls),
SND_SOC_DAPM_MIXER("Left HP Mixer", AC97_EXTENDED_MID, 3, 1,
	&wm9713_hpl_mixer_controls[0], ARRAY_SIZE(wm9713_hpl_mixer_controls)),
SND_SOC_DAPM_MIXER("Right HP Mixer", AC97_EXTENDED_MID, 2, 1,
	&wm9713_hpr_mixer_controls[0], ARRAY_SIZE(wm9713_hpr_mixer_controls)),
SND_SOC_DAPM_MIXER("Mono Mixer", AC97_EXTENDED_MID, 0, 1,
	&wm9713_mono_mixer_controls[0], ARRAY_SIZE(wm9713_mono_mixer_controls)),
SND_SOC_DAPM_MIXER("Speaker Mixer", AC97_EXTENDED_MID, 1, 1,
	&wm9713_speaker_mixer_controls[0],
	ARRAY_SIZE(wm9713_speaker_mixer_controls)),
SND_SOC_DAPM_DAC("Left DAC", "Left HiFi Playback", AC97_EXTENDED_MID, 7, 1),
SND_SOC_DAPM_DAC("Right DAC", "Right HiFi Playback", AC97_EXTENDED_MID, 6, 1),
SND_SOC_DAPM_MIXER("AC97 Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_MIXER("HP Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_MIXER("Line Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_MIXER("Capture Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_DAC_E("Voice DAC", "Voice Playback", AC97_EXTENDED_MID, 12, 1,
		   wm9713_voice_shutdown, SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_DAC("Aux DAC", "Aux Playback", AC97_EXTENDED_MID, 11, 1),
SND_SOC_DAPM_PGA("Left ADC", AC97_EXTENDED_MID, 5, 1, NULL, 0),
SND_SOC_DAPM_PGA("Right ADC", AC97_EXTENDED_MID, 4, 1, NULL, 0),
SND_SOC_DAPM_ADC("Left HiFi ADC", "Left HiFi Capture", SND_SOC_NOPM, 0, 0),
SND_SOC_DAPM_ADC("Right HiFi ADC", "Right HiFi Capture", SND_SOC_NOPM, 0, 0),
SND_SOC_DAPM_ADC("Left Voice ADC", "Left Voice Capture", SND_SOC_NOPM, 0, 0),
SND_SOC_DAPM_ADC("Right Voice ADC", "Right Voice Capture", SND_SOC_NOPM, 0, 0),
SND_SOC_DAPM_PGA("Left Headphone", AC97_EXTENDED_MSTATUS, 10, 1, NULL, 0),
SND_SOC_DAPM_PGA("Right Headphone", AC97_EXTENDED_MSTATUS, 9, 1, NULL, 0),
SND_SOC_DAPM_PGA("Left Speaker", AC97_EXTENDED_MSTATUS, 8, 1, NULL, 0),
SND_SOC_DAPM_PGA("Right Speaker", AC97_EXTENDED_MSTATUS, 7, 1, NULL, 0),
SND_SOC_DAPM_PGA("Out 3", AC97_EXTENDED_MSTATUS, 11, 1, NULL, 0),
SND_SOC_DAPM_PGA("Out 4", AC97_EXTENDED_MSTATUS, 12, 1, NULL, 0),
SND_SOC_DAPM_PGA("Mono Out", AC97_EXTENDED_MSTATUS, 13, 1, NULL, 0),
SND_SOC_DAPM_PGA("Left Line In", AC97_EXTENDED_MSTATUS, 6, 1, NULL, 0),
SND_SOC_DAPM_PGA("Right Line In", AC97_EXTENDED_MSTATUS, 5, 1, NULL, 0),
SND_SOC_DAPM_PGA("Mono In", AC97_EXTENDED_MSTATUS, 4, 1, NULL, 0),
SND_SOC_DAPM_PGA("Mic A PGA", AC97_EXTENDED_MSTATUS, 3, 1, NULL, 0),
SND_SOC_DAPM_PGA("Mic B PGA", AC97_EXTENDED_MSTATUS, 2, 1, NULL, 0),
SND_SOC_DAPM_PGA("Mic A Pre Amp", AC97_EXTENDED_MSTATUS, 1, 1, NULL, 0),
SND_SOC_DAPM_PGA("Mic B Pre Amp", AC97_EXTENDED_MSTATUS, 0, 1, NULL, 0),
SND_SOC_DAPM_MICBIAS("Mic Bias", AC97_EXTENDED_MSTATUS, 14, 1),
SND_SOC_DAPM_OUTPUT("MONO"),
SND_SOC_DAPM_OUTPUT("HPL"),
SND_SOC_DAPM_OUTPUT("HPR"),
SND_SOC_DAPM_OUTPUT("SPKL"),
SND_SOC_DAPM_OUTPUT("SPKR"),
SND_SOC_DAPM_OUTPUT("OUT3"),
SND_SOC_DAPM_OUTPUT("OUT4"),
SND_SOC_DAPM_INPUT("LINEL"),
SND_SOC_DAPM_INPUT("LINER"),
SND_SOC_DAPM_INPUT("MONOIN"),
SND_SOC_DAPM_INPUT("PCBEEP"),
SND_SOC_DAPM_INPUT("MIC1"),
SND_SOC_DAPM_INPUT("MIC2A"),
SND_SOC_DAPM_INPUT("MIC2B"),
SND_SOC_DAPM_VMID("VMID"),
};

static const struct snd_soc_dapm_route wm9713_audio_map[] = {
	 
	{"Left HP Mixer", "Beep Playback Switch",    "PCBEEP"},
	{"Left HP Mixer", "Voice Playback Switch",   "Voice DAC"},
	{"Left HP Mixer", "Aux Playback Switch",     "Aux DAC"},
	{"Left HP Mixer", "Bypass Playback Switch",  "Left Line In"},
	{"Left HP Mixer", "PCM Playback Switch",     "Left DAC"},
	{"Left HP Mixer", "MonoIn Playback Switch",  "Mono In"},
	{"Left HP Mixer", NULL,  "Capture Headphone Mux"},

	 
	{"Right HP Mixer", "Beep Playback Switch",    "PCBEEP"},
	{"Right HP Mixer", "Voice Playback Switch",   "Voice DAC"},
	{"Right HP Mixer", "Aux Playback Switch",     "Aux DAC"},
	{"Right HP Mixer", "Bypass Playback Switch",  "Right Line In"},
	{"Right HP Mixer", "PCM Playback Switch",     "Right DAC"},
	{"Right HP Mixer", "MonoIn Playback Switch",  "Mono In"},
	{"Right HP Mixer", NULL,  "Capture Headphone Mux"},

	 
	{"AC97 Mixer", NULL, "Left DAC"},
	{"AC97 Mixer", NULL, "Right DAC"},
	{"Line Mixer", NULL, "Right Line In"},
	{"Line Mixer", NULL, "Left Line In"},
	{"HP Mixer", NULL, "Left HP Mixer"},
	{"HP Mixer", NULL, "Right HP Mixer"},
	{"Capture Mixer", NULL, "Left Capture Source"},
	{"Capture Mixer", NULL, "Right Capture Source"},

	 
	{"Speaker Mixer", "Beep Playback Switch",    "PCBEEP"},
	{"Speaker Mixer", "Voice Playback Switch",   "Voice DAC"},
	{"Speaker Mixer", "Aux Playback Switch",     "Aux DAC"},
	{"Speaker Mixer", "Bypass Playback Switch",  "Line Mixer"},
	{"Speaker Mixer", "PCM Playback Switch",     "AC97 Mixer"},
	{"Speaker Mixer", "MonoIn Playback Switch",  "Mono In"},

	 
	{"Mono Mixer", "Beep Playback Switch",    "PCBEEP"},
	{"Mono Mixer", "Voice Playback Switch",   "Voice DAC"},
	{"Mono Mixer", "Aux Playback Switch",     "Aux DAC"},
	{"Mono Mixer", "Bypass Playback Switch",  "Line Mixer"},
	{"Mono Mixer", "PCM Playback Switch",     "AC97 Mixer"},
	{"Mono Mixer", "Mic 1 Sidetone Switch", "Mic A PGA"},
	{"Mono Mixer", "Mic 2 Sidetone Switch", "Mic B PGA"},
	{"Mono Mixer", NULL,  "Capture Mono Mux"},

	 
	{"DAC Inv Mux 1", "Mono", "Mono Mixer"},
	{"DAC Inv Mux 1", "Speaker", "Speaker Mixer"},
	{"DAC Inv Mux 1", "Left Headphone", "Left HP Mixer"},
	{"DAC Inv Mux 1", "Right Headphone", "Right HP Mixer"},
	{"DAC Inv Mux 1", "Headphone Mono", "HP Mixer"},

	 
	{"DAC Inv Mux 2", "Mono", "Mono Mixer"},
	{"DAC Inv Mux 2", "Speaker", "Speaker Mixer"},
	{"DAC Inv Mux 2", "Left Headphone", "Left HP Mixer"},
	{"DAC Inv Mux 2", "Right Headphone", "Right HP Mixer"},
	{"DAC Inv Mux 2", "Headphone Mono", "HP Mixer"},

	 
	{"Left Headphone Out Mux", "Headphone", "Left HP Mixer"},

	 
	{"Right Headphone Out Mux", "Headphone", "Right HP Mixer"},

	 
	{"Left Speaker Out Mux", "Headphone", "Left HP Mixer"},
	{"Left Speaker Out Mux", "Speaker", "Speaker Mixer"},
	{"Left Speaker Out Mux", "Inv", "DAC Inv Mux 1"},

	 
	{"Right Speaker Out Mux", "Headphone", "Right HP Mixer"},
	{"Right Speaker Out Mux", "Speaker", "Speaker Mixer"},
	{"Right Speaker Out Mux", "Inv", "DAC Inv Mux 2"},

	 
	{"Mono Out Mux", "Mono", "Mono Mixer"},
	{"Mono Out Mux", "Inv", "DAC Inv Mux 1"},

	 
	{"Out 3 Mux", "Inv 1", "DAC Inv Mux 1"},

	 
	{"Out 4 Mux", "Inv 2", "DAC Inv Mux 2"},

	 
	{"HPL", NULL, "Left Headphone"},
	{"Left Headphone", NULL, "Left Headphone Out Mux"},
	{"HPR", NULL, "Right Headphone"},
	{"Right Headphone", NULL, "Right Headphone Out Mux"},
	{"OUT3", NULL, "Out 3"},
	{"Out 3", NULL, "Out 3 Mux"},
	{"OUT4", NULL, "Out 4"},
	{"Out 4", NULL, "Out 4 Mux"},
	{"SPKL", NULL, "Left Speaker"},
	{"Left Speaker", NULL, "Left Speaker Out Mux"},
	{"SPKR", NULL, "Right Speaker"},
	{"Right Speaker", NULL, "Right Speaker Out Mux"},
	{"MONO", NULL, "Mono Out"},
	{"Mono Out", NULL, "Mono Out Mux"},

	 
	{"Left Line In", NULL, "LINEL"},
	{"Right Line In", NULL, "LINER"},
	{"Mono In", NULL, "MONOIN"},
	{"Mic A PGA", NULL, "Mic A Pre Amp"},
	{"Mic B PGA", NULL, "Mic B Pre Amp"},

	 
	{"Left Capture Source", "Mic 1", "Mic A Pre Amp"},
	{"Left Capture Source", "Mic 2", "Mic B Pre Amp"},
	{"Left Capture Source", "Line", "LINEL"},
	{"Left Capture Source", "Mono In", "MONOIN"},
	{"Left Capture Source", "Headphone", "Left HP Mixer"},
	{"Left Capture Source", "Speaker", "Speaker Mixer"},
	{"Left Capture Source", "Mono Out", "Mono Mixer"},

	 
	{"Right Capture Source", "Mic 1", "Mic A Pre Amp"},
	{"Right Capture Source", "Mic 2", "Mic B Pre Amp"},
	{"Right Capture Source", "Line", "LINER"},
	{"Right Capture Source", "Mono In", "MONOIN"},
	{"Right Capture Source", "Headphone", "Right HP Mixer"},
	{"Right Capture Source", "Speaker", "Speaker Mixer"},
	{"Right Capture Source", "Mono Out", "Mono Mixer"},

	 
	{"Left ADC", NULL, "Left Capture Source"},
	{"Left Voice ADC", NULL, "Left ADC"},
	{"Left HiFi ADC", NULL, "Left ADC"},

	 
	{"Right ADC", NULL, "Right Capture Source"},
	{"Right Voice ADC", NULL, "Right ADC"},
	{"Right HiFi ADC", NULL, "Right ADC"},

	 
	{"Mic A Pre Amp", NULL, "Mic A Source"},
	{"Mic A Source", "Mic 1", "MIC1"},
	{"Mic A Source", "Mic 2 A", "MIC2A"},
	{"Mic A Source", "Mic 2 B", "Mic B Source"},
	{"Mic B Pre Amp", "MPB", "Mic B Source"},
	{"Mic B Source", NULL, "MIC2B"},

	 
	{"Capture Headphone Mux", "Stereo", "Capture Mixer"},
	{"Capture Headphone Mux", "Left", "Left Capture Source"},
	{"Capture Headphone Mux", "Right", "Right Capture Source"},

	 
	{"Capture Mono Mux", "Stereo", "Capture Mixer"},
	{"Capture Mono Mux", "Left", "Left Capture Source"},
	{"Capture Mono Mux", "Right", "Right Capture Source"},
};

static bool wm9713_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AC97_RESET ... AC97_PCM_SURR_DAC_RATE:
	case AC97_PCM_LR_ADC_RATE:
	case AC97_CENTER_LFE_MASTER:
	case AC97_SPDIF ... AC97_LINE1_LEVEL:
	case AC97_GPIO_CFG ... 0x5c:
	case AC97_CODEC_CLASS_REV ... AC97_PCI_SID:
	case 0x74 ... AC97_VENDOR_ID2:
		return true;
	default:
		return false;
	}
}

static bool wm9713_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case AC97_VENDOR_ID1:
	case AC97_VENDOR_ID2:
		return false;
	default:
		return wm9713_readable_reg(dev, reg);
	}
}

static const struct reg_default wm9713_reg_defaults[] = {
	{ 0x02, 0x8080 },	 
	{ 0x04, 0x8080 },	 
	{ 0x06, 0x8080 },	 
	{ 0x08, 0xc880 },	 
	{ 0x0a, 0xe808 },	 
	{ 0x0c, 0xe808 },	 
	{ 0x0e, 0x0808 },	 
	{ 0x10, 0x00da },	 
	{ 0x12, 0x8000 },	 
	{ 0x14, 0xd600 },	 
	{ 0x16, 0xaaa0 },	 
	{ 0x18, 0xaaa0 },	 
	{ 0x1a, 0xaaa0 },	 
	{ 0x1c, 0x0000 },	 
	{ 0x1e, 0x0000 },	 
	{ 0x20, 0x0f0f },	 
	{ 0x22, 0x0040 },	 
	{ 0x24, 0x0000 },	 
	{ 0x26, 0x7f00 },	 
	{ 0x28, 0x0405 },	 
	{ 0x2a, 0x0410 },	 
	{ 0x2c, 0xbb80 },	 
	{ 0x2e, 0xbb80 },	 
	{ 0x32, 0xbb80 },	 
	{ 0x36, 0x4523 },	 
	{ 0x3a, 0x2000 },	 
	{ 0x3c, 0xfdff },	 
	{ 0x3e, 0xffff },	 
	{ 0x40, 0x0000 },	 
	{ 0x42, 0x0000 },	 
	{ 0x44, 0x0080 },	 
	{ 0x46, 0x0000 },	 
	{ 0x4c, 0xfffe },	 
	{ 0x4e, 0xffff },	 
	{ 0x50, 0x0000 },	 
	{ 0x52, 0x0000 },	 
				 
	{ 0x56, 0xfffe },	 
	{ 0x58, 0x4000 },	 
	{ 0x5a, 0x0000 },	 
	{ 0x5c, 0x0000 },	 
	{ 0x60, 0xb032 },	 
	{ 0x62, 0x3e00 },	 
	{ 0x64, 0x0000 },	 
	{ 0x74, 0x0000 },	 
	{ 0x76, 0x0006 },	 
	{ 0x78, 0x0001 },	 
	{ 0x7a, 0x0000 },	 
};

static const struct regmap_config wm9713_regmap_config = {
	.reg_bits = 16,
	.reg_stride = 2,
	.val_bits = 16,
	.max_register = 0x7e,
	.cache_type = REGCACHE_MAPLE,

	.reg_defaults = wm9713_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(wm9713_reg_defaults),
	.volatile_reg = regmap_ac97_default_volatile,
	.readable_reg = wm9713_readable_reg,
	.writeable_reg = wm9713_writeable_reg,
};

 
struct _pll_div {
	u32 divsel:1;
	u32 divctl:1;
	u32 lf:1;
	u32 n:4;
	u32 k:24;
};

 
#define FIXED_PLL_SIZE ((1 << 22) * 10)

static void pll_factors(struct snd_soc_component *component,
	struct _pll_div *pll_div, unsigned int source)
{
	u64 Kpart;
	unsigned int K, Ndiv, Nmod, target;

	 
	target = 98304000;

	 
	if (source > 14400000) {
		source >>= 1;
		pll_div->divsel = 1;

		if (source > 14400000) {
			source >>= 1;
			pll_div->divctl = 1;
		} else
			pll_div->divctl = 0;

	} else {
		pll_div->divsel = 0;
		pll_div->divctl = 0;
	}

	 
	if (source < 8192000) {
		pll_div->lf = 1;
		target >>= 2;
	} else
		pll_div->lf = 0;

	Ndiv = target / source;
	if ((Ndiv < 5) || (Ndiv > 12))
		dev_warn(component->dev,
			"WM9713 PLL N value %u out of recommended range!\n",
			Ndiv);

	pll_div->n = Ndiv;
	Nmod = target % source;
	Kpart = FIXED_PLL_SIZE * (long long)Nmod;

	do_div(Kpart, source);

	K = Kpart & 0xFFFFFFFF;

	 
	if ((K % 10) >= 5)
		K += 5;

	 
	K /= 10;

	pll_div->k = K;
}

 
static int wm9713_set_pll(struct snd_soc_component *component,
	int pll_id, unsigned int freq_in, unsigned int freq_out)
{
	struct wm9713_priv *wm9713 = snd_soc_component_get_drvdata(component);
	u16 reg, reg2;
	struct _pll_div pll_div;

	 
	if (freq_in == 0) {
		 
		snd_soc_component_update_bits(component, AC97_HANDSET_RATE, 0x0080, 0x0080);
		snd_soc_component_update_bits(component, AC97_EXTENDED_MID, 0x0200, 0x0200);
		wm9713->pll_in = 0;
		return 0;
	}

	pll_factors(component, &pll_div, freq_in);

	if (pll_div.k == 0) {
		reg = (pll_div.n << 12) | (pll_div.lf << 11) |
			(pll_div.divsel << 9) | (pll_div.divctl << 8);
		snd_soc_component_write(component, AC97_LINE1_LEVEL, reg);
	} else {
		 
		reg2 = (pll_div.n << 12) | (pll_div.lf << 11) | (1 << 10) |
			(pll_div.divsel << 9) | (pll_div.divctl << 8);

		 
		reg = reg2 | (0x5 << 4) | (pll_div.k >> 20);
		snd_soc_component_write(component, AC97_LINE1_LEVEL, reg);

		 
		reg = reg2 | (0x4 << 4) | ((pll_div.k >> 16) & 0xf);
		snd_soc_component_write(component, AC97_LINE1_LEVEL, reg);

		 
		reg = reg2 | (0x3 << 4) | ((pll_div.k >> 12) & 0xf);
		snd_soc_component_write(component, AC97_LINE1_LEVEL, reg);

		 
		reg = reg2 | (0x2 << 4) | ((pll_div.k >> 8) & 0xf);
		snd_soc_component_write(component, AC97_LINE1_LEVEL, reg);

		 
		reg = reg2 | (0x1 << 4) | ((pll_div.k >> 4) & 0xf);
		snd_soc_component_write(component, AC97_LINE1_LEVEL, reg);

		reg = reg2 | (0x0 << 4) | (pll_div.k & 0xf);  
		snd_soc_component_write(component, AC97_LINE1_LEVEL, reg);
	}

	 
	snd_soc_component_update_bits(component, AC97_EXTENDED_MID, 0x0200, 0x0000);
	snd_soc_component_update_bits(component, AC97_HANDSET_RATE, 0x0080, 0x0000);
	wm9713->pll_in = freq_in;

	 
	schedule_timeout_interruptible(msecs_to_jiffies(10));
	return 0;
}

static int wm9713_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_component *component = codec_dai->component;
	return wm9713_set_pll(component, pll_id, freq_in, freq_out);
}

 
static int wm9713_set_dai_tristate(struct snd_soc_dai *codec_dai,
	int tristate)
{
	struct snd_soc_component *component = codec_dai->component;

	if (tristate)
		snd_soc_component_update_bits(component, AC97_CENTER_LFE_MASTER,
				    0x6000, 0x0000);

	return 0;
}

 
static int wm9713_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
		int div_id, int div)
{
	struct snd_soc_component *component = codec_dai->component;

	switch (div_id) {
	case WM9713_PCMCLK_DIV:
		snd_soc_component_update_bits(component, AC97_HANDSET_RATE, 0x0f00, div);
		break;
	case WM9713_CLKA_MULT:
		snd_soc_component_update_bits(component, AC97_HANDSET_RATE, 0x0002, div);
		break;
	case WM9713_CLKB_MULT:
		snd_soc_component_update_bits(component, AC97_HANDSET_RATE, 0x0004, div);
		break;
	case WM9713_HIFI_DIV:
		snd_soc_component_update_bits(component, AC97_HANDSET_RATE, 0x7000, div);
		break;
	case WM9713_PCMBCLK_DIV:
		snd_soc_component_update_bits(component, AC97_CENTER_LFE_MASTER, 0x0e00, div);
		break;
	case WM9713_PCMCLK_PLL_DIV:
		snd_soc_component_update_bits(component, AC97_LINE1_LEVEL,
				    0x007f, div | 0x60);
		break;
	case WM9713_HIFI_PLL_DIV:
		snd_soc_component_update_bits(component, AC97_LINE1_LEVEL,
				    0x007f, div | 0x70);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int wm9713_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	u16 gpio = snd_soc_component_read(component, AC97_GPIO_CFG) & 0xffc5;
	u16 reg = 0x8000;

	 
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		reg |= 0x4000;
		gpio |= 0x0010;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		reg |= 0x6000;
		gpio |= 0x0018;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		reg |= 0x2000;
		gpio |= 0x001a;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		gpio |= 0x0012;
		break;
	}

	 
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
		reg |= 0x00c0;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		reg |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		reg |= 0x0040;
		break;
	}

	 
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		reg |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		reg |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		reg |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		reg |= 0x0043;
		break;
	}

	snd_soc_component_write(component, AC97_GPIO_CFG, gpio);
	snd_soc_component_write(component, AC97_CENTER_LFE_MASTER, reg);
	return 0;
}

static int wm9713_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	 
	switch (params_width(params)) {
	case 16:
		break;
	case 20:
		snd_soc_component_update_bits(component, AC97_CENTER_LFE_MASTER,
				    0x000c, 0x0004);
		break;
	case 24:
		snd_soc_component_update_bits(component, AC97_CENTER_LFE_MASTER,
				    0x000c, 0x0008);
		break;
	case 32:
		snd_soc_component_update_bits(component, AC97_CENTER_LFE_MASTER,
				    0x000c, 0x000c);
		break;
	}
	return 0;
}

static int ac97_hifi_prepare(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct snd_pcm_runtime *runtime = substream->runtime;
	int reg;

	snd_soc_component_update_bits(component, AC97_EXTENDED_STATUS, 0x0001, 0x0001);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		reg = AC97_PCM_FRONT_DAC_RATE;
	else
		reg = AC97_PCM_LR_ADC_RATE;

	return snd_soc_component_write(component, reg, runtime->rate);
}

static int ac97_aux_prepare(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_soc_component_update_bits(component, AC97_EXTENDED_STATUS, 0x0001, 0x0001);
	snd_soc_component_update_bits(component, AC97_PCI_SID, 0x8000, 0x8000);

	if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK)
		return -ENODEV;

	return snd_soc_component_write(component, AC97_PCM_SURR_DAC_RATE, runtime->rate);
}

#define WM9713_RATES (SNDRV_PCM_RATE_8000  |	\
		      SNDRV_PCM_RATE_11025 |	\
		      SNDRV_PCM_RATE_22050 |	\
		      SNDRV_PCM_RATE_44100 |	\
		      SNDRV_PCM_RATE_48000)

#define WM9713_PCM_RATES (SNDRV_PCM_RATE_8000  |	\
			  SNDRV_PCM_RATE_11025 |	\
			  SNDRV_PCM_RATE_16000 |	\
			  SNDRV_PCM_RATE_22050 |	\
			  SNDRV_PCM_RATE_44100 |	\
			  SNDRV_PCM_RATE_48000)

#define WM9713_PCM_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	 SNDRV_PCM_FMTBIT_S24_LE)

static const struct snd_soc_dai_ops wm9713_dai_ops_hifi = {
	.prepare	= ac97_hifi_prepare,
	.set_clkdiv	= wm9713_set_dai_clkdiv,
	.set_pll	= wm9713_set_dai_pll,
};

static const struct snd_soc_dai_ops wm9713_dai_ops_aux = {
	.prepare	= ac97_aux_prepare,
	.set_clkdiv	= wm9713_set_dai_clkdiv,
	.set_pll	= wm9713_set_dai_pll,
};

static const struct snd_soc_dai_ops wm9713_dai_ops_voice = {
	.hw_params	= wm9713_pcm_hw_params,
	.set_clkdiv	= wm9713_set_dai_clkdiv,
	.set_pll	= wm9713_set_dai_pll,
	.set_fmt	= wm9713_set_dai_fmt,
	.set_tristate	= wm9713_set_dai_tristate,
};

static struct snd_soc_dai_driver wm9713_dai[] = {
{
	.name = "wm9713-hifi",
	.playback = {
		.stream_name = "HiFi Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM9713_RATES,
		.formats = SND_SOC_STD_AC97_FMTS,},
	.capture = {
		.stream_name = "HiFi Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM9713_RATES,
		.formats = SND_SOC_STD_AC97_FMTS,},
	.ops = &wm9713_dai_ops_hifi,
	},
	{
	.name = "wm9713-aux",
	.playback = {
		.stream_name = "Aux Playback",
		.channels_min = 1,
		.channels_max = 1,
		.rates = WM9713_RATES,
		.formats = SND_SOC_STD_AC97_FMTS,},
	.ops = &wm9713_dai_ops_aux,
	},
	{
	.name = "wm9713-voice",
	.playback = {
		.stream_name = "Voice Playback",
		.channels_min = 1,
		.channels_max = 1,
		.rates = WM9713_PCM_RATES,
		.formats = WM9713_PCM_FORMATS,},
	.capture = {
		.stream_name = "Voice Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM9713_PCM_RATES,
		.formats = WM9713_PCM_FORMATS,},
	.ops = &wm9713_dai_ops_voice,
	.symmetric_rate = 1,
	},
};

static int wm9713_set_bias_level(struct snd_soc_component *component,
				 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		 
		snd_soc_component_update_bits(component, AC97_EXTENDED_MID, 0xe400, 0x0000);
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		 
		snd_soc_component_update_bits(component, AC97_EXTENDED_MID, 0xc400, 0x0000);
		snd_soc_component_write(component, AC97_POWERDOWN, 0x0000);
		break;
	case SND_SOC_BIAS_OFF:
		 
		snd_soc_component_write(component, AC97_EXTENDED_MID, 0xffff);
		snd_soc_component_write(component, AC97_EXTENDED_MSTATUS, 0xffff);
		snd_soc_component_write(component, AC97_POWERDOWN, 0xffff);
		break;
	}
	return 0;
}

static int wm9713_soc_suspend(struct snd_soc_component *component)
{
	 
	snd_soc_component_update_bits(component, AC97_EXTENDED_MID, 0x7fff,
				 0x7fff);
	snd_soc_component_write(component, AC97_EXTENDED_MSTATUS, 0xffff);
	snd_soc_component_write(component, AC97_POWERDOWN, 0x6f00);
	snd_soc_component_write(component, AC97_POWERDOWN, 0xffff);

	return 0;
}

static int wm9713_soc_resume(struct snd_soc_component *component)
{
	struct wm9713_priv *wm9713 = snd_soc_component_get_drvdata(component);
	int ret;

	ret = snd_ac97_reset(wm9713->ac97, true, WM9713_VENDOR_ID,
		WM9713_VENDOR_ID_MASK);
	if (ret < 0)
		return ret;

	snd_soc_component_force_bias_level(component, SND_SOC_BIAS_STANDBY);

	 
	if (wm9713->pll_in)
		wm9713_set_pll(component, 0, wm9713->pll_in, 0);

	 
	if (ret == 0) {
		regcache_mark_dirty(component->regmap);
		snd_soc_component_cache_sync(component);
	}

	return ret;
}

static int wm9713_soc_probe(struct snd_soc_component *component)
{
	struct wm9713_priv *wm9713 = snd_soc_component_get_drvdata(component);
	struct regmap *regmap = NULL;

	if (wm9713->mfd_pdata) {
		wm9713->ac97 = wm9713->mfd_pdata->ac97;
		regmap = wm9713->mfd_pdata->regmap;
	} else if (IS_ENABLED(CONFIG_SND_SOC_AC97_BUS)) {
		wm9713->ac97 = snd_soc_new_ac97_component(component, WM9713_VENDOR_ID,
						      WM9713_VENDOR_ID_MASK);
		if (IS_ERR(wm9713->ac97))
			return PTR_ERR(wm9713->ac97);
		regmap = regmap_init_ac97(wm9713->ac97, &wm9713_regmap_config);
		if (IS_ERR(regmap)) {
			snd_soc_free_ac97_component(wm9713->ac97);
			return PTR_ERR(regmap);
		}
	} else {
		return -ENXIO;
	}

	snd_soc_component_init_regmap(component, regmap);

	 
	snd_soc_component_update_bits(component, AC97_CD, 0x7fff, 0x0000);

	return 0;
}

static void wm9713_soc_remove(struct snd_soc_component *component)
{
	struct wm9713_priv *wm9713 = snd_soc_component_get_drvdata(component);

	if (IS_ENABLED(CONFIG_SND_SOC_AC97_BUS) && !wm9713->mfd_pdata) {
		snd_soc_component_exit_regmap(component);
		snd_soc_free_ac97_component(wm9713->ac97);
	}
}

static const struct snd_soc_component_driver soc_component_dev_wm9713 = {
	.probe			= wm9713_soc_probe,
	.remove			= wm9713_soc_remove,
	.suspend		= wm9713_soc_suspend,
	.resume			= wm9713_soc_resume,
	.set_bias_level		= wm9713_set_bias_level,
	.controls		= wm9713_snd_ac97_controls,
	.num_controls		= ARRAY_SIZE(wm9713_snd_ac97_controls),
	.dapm_widgets		= wm9713_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(wm9713_dapm_widgets),
	.dapm_routes		= wm9713_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(wm9713_audio_map),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

static int wm9713_probe(struct platform_device *pdev)
{
	struct wm9713_priv *wm9713;

	wm9713 = devm_kzalloc(&pdev->dev, sizeof(*wm9713), GFP_KERNEL);
	if (wm9713 == NULL)
		return -ENOMEM;

	mutex_init(&wm9713->lock);

	wm9713->mfd_pdata = dev_get_platdata(&pdev->dev);
	platform_set_drvdata(pdev, wm9713);

	return devm_snd_soc_register_component(&pdev->dev,
			&soc_component_dev_wm9713, wm9713_dai, ARRAY_SIZE(wm9713_dai));
}

static struct platform_driver wm9713_codec_driver = {
	.driver = {
			.name = "wm9713-codec",
	},

	.probe = wm9713_probe,
};

module_platform_driver(wm9713_codec_driver);

MODULE_DESCRIPTION("ASoC WM9713/WM9714 driver");
MODULE_AUTHOR("Liam Girdwood");
MODULE_LICENSE("GPL");
