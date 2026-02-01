
 

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/int_log.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/acpi.h>
#include <linux/math64.h>
#include <linux/semaphore.h>

#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>


#include "nau8825.h"


#define NUVOTON_CODEC_DAI "nau8825-hifi"

#define NAU_FREF_MAX 13500000
#define NAU_FVCO_MAX 124000000
#define NAU_FVCO_MIN 90000000

 
#define GAIN_AUGMENT 22500
#define SIDETONE_BASE 207000

 
#define CLK_DA_AD_MAX 6144000

static int nau8825_configure_sysclk(struct nau8825 *nau8825,
		int clk_id, unsigned int freq);
static bool nau8825_is_jack_inserted(struct regmap *regmap);

struct nau8825_fll {
	int mclk_src;
	int ratio;
	int fll_frac;
	int fll_frac_num;
	int fll_int;
	int clk_ref_div;
};

struct nau8825_fll_attr {
	unsigned int param;
	unsigned int val;
};

 
static const struct nau8825_fll_attr mclk_src_scaling[] = {
	{ 1, 0x0 },
	{ 2, 0x2 },
	{ 4, 0x3 },
	{ 8, 0x4 },
	{ 16, 0x5 },
	{ 32, 0x6 },
	{ 3, 0x7 },
	{ 6, 0xa },
	{ 12, 0xb },
	{ 24, 0xc },
	{ 48, 0xd },
	{ 96, 0xe },
	{ 5, 0xf },
};

 
static const struct nau8825_fll_attr fll_ratio[] = {
	{ 512000, 0x01 },
	{ 256000, 0x02 },
	{ 128000, 0x04 },
	{ 64000, 0x08 },
	{ 32000, 0x10 },
	{ 8000, 0x20 },
	{ 4000, 0x40 },
};

static const struct nau8825_fll_attr fll_pre_scalar[] = {
	{ 1, 0x0 },
	{ 2, 0x1 },
	{ 4, 0x2 },
	{ 8, 0x3 },
};

 
struct nau8825_osr_attr {
	unsigned int osr;
	unsigned int clk_src;
};

static const struct nau8825_osr_attr osr_dac_sel[] = {
	{ 64, 2 },	 
	{ 256, 0 },	 
	{ 128, 1 },	 
	{ 0, 0 },
	{ 32, 3 },	 
};

static const struct nau8825_osr_attr osr_adc_sel[] = {
	{ 32, 3 },	 
	{ 64, 2 },	 
	{ 128, 1 },	 
	{ 256, 0 },	 
};

static const struct reg_default nau8825_reg_defaults[] = {
	{ NAU8825_REG_ENA_CTRL, 0x00ff },
	{ NAU8825_REG_IIC_ADDR_SET, 0x0 },
	{ NAU8825_REG_CLK_DIVIDER, 0x0050 },
	{ NAU8825_REG_FLL1, 0x0 },
	{ NAU8825_REG_FLL2, 0x3126 },
	{ NAU8825_REG_FLL3, 0x0008 },
	{ NAU8825_REG_FLL4, 0x0010 },
	{ NAU8825_REG_FLL5, 0x0 },
	{ NAU8825_REG_FLL6, 0x6000 },
	{ NAU8825_REG_FLL_VCO_RSV, 0xf13c },
	{ NAU8825_REG_HSD_CTRL, 0x000c },
	{ NAU8825_REG_JACK_DET_CTRL, 0x0 },
	{ NAU8825_REG_INTERRUPT_MASK, 0x0 },
	{ NAU8825_REG_INTERRUPT_DIS_CTRL, 0xffff },
	{ NAU8825_REG_SAR_CTRL, 0x0015 },
	{ NAU8825_REG_KEYDET_CTRL, 0x0110 },
	{ NAU8825_REG_VDET_THRESHOLD_1, 0x0 },
	{ NAU8825_REG_VDET_THRESHOLD_2, 0x0 },
	{ NAU8825_REG_VDET_THRESHOLD_3, 0x0 },
	{ NAU8825_REG_VDET_THRESHOLD_4, 0x0 },
	{ NAU8825_REG_GPIO34_CTRL, 0x0 },
	{ NAU8825_REG_GPIO12_CTRL, 0x0 },
	{ NAU8825_REG_TDM_CTRL, 0x0 },
	{ NAU8825_REG_I2S_PCM_CTRL1, 0x000b },
	{ NAU8825_REG_I2S_PCM_CTRL2, 0x8010 },
	{ NAU8825_REG_LEFT_TIME_SLOT, 0x0 },
	{ NAU8825_REG_RIGHT_TIME_SLOT, 0x0 },
	{ NAU8825_REG_BIQ_CTRL, 0x0 },
	{ NAU8825_REG_BIQ_COF1, 0x0 },
	{ NAU8825_REG_BIQ_COF2, 0x0 },
	{ NAU8825_REG_BIQ_COF3, 0x0 },
	{ NAU8825_REG_BIQ_COF4, 0x0 },
	{ NAU8825_REG_BIQ_COF5, 0x0 },
	{ NAU8825_REG_BIQ_COF6, 0x0 },
	{ NAU8825_REG_BIQ_COF7, 0x0 },
	{ NAU8825_REG_BIQ_COF8, 0x0 },
	{ NAU8825_REG_BIQ_COF9, 0x0 },
	{ NAU8825_REG_BIQ_COF10, 0x0 },
	{ NAU8825_REG_ADC_RATE, 0x0010 },
	{ NAU8825_REG_DAC_CTRL1, 0x0001 },
	{ NAU8825_REG_DAC_CTRL2, 0x0 },
	{ NAU8825_REG_DAC_DGAIN_CTRL, 0x0 },
	{ NAU8825_REG_ADC_DGAIN_CTRL, 0x00cf },
	{ NAU8825_REG_MUTE_CTRL, 0x0 },
	{ NAU8825_REG_HSVOL_CTRL, 0x0 },
	{ NAU8825_REG_DACL_CTRL, 0x02cf },
	{ NAU8825_REG_DACR_CTRL, 0x00cf },
	{ NAU8825_REG_ADC_DRC_KNEE_IP12, 0x1486 },
	{ NAU8825_REG_ADC_DRC_KNEE_IP34, 0x0f12 },
	{ NAU8825_REG_ADC_DRC_SLOPES, 0x25ff },
	{ NAU8825_REG_ADC_DRC_ATKDCY, 0x3457 },
	{ NAU8825_REG_DAC_DRC_KNEE_IP12, 0x1486 },
	{ NAU8825_REG_DAC_DRC_KNEE_IP34, 0x0f12 },
	{ NAU8825_REG_DAC_DRC_SLOPES, 0x25f9 },
	{ NAU8825_REG_DAC_DRC_ATKDCY, 0x3457 },
	{ NAU8825_REG_IMM_MODE_CTRL, 0x0 },
	{ NAU8825_REG_CLASSG_CTRL, 0x0 },
	{ NAU8825_REG_OPT_EFUSE_CTRL, 0x0 },
	{ NAU8825_REG_MISC_CTRL, 0x0 },
	{ NAU8825_REG_FLL2_LOWER, 0x0 },
	{ NAU8825_REG_FLL2_UPPER, 0x0 },
	{ NAU8825_REG_BIAS_ADJ, 0x0 },
	{ NAU8825_REG_TRIM_SETTINGS, 0x0 },
	{ NAU8825_REG_ANALOG_CONTROL_1, 0x0 },
	{ NAU8825_REG_ANALOG_CONTROL_2, 0x0 },
	{ NAU8825_REG_ANALOG_ADC_1, 0x0011 },
	{ NAU8825_REG_ANALOG_ADC_2, 0x0020 },
	{ NAU8825_REG_RDAC, 0x0008 },
	{ NAU8825_REG_MIC_BIAS, 0x0006 },
	{ NAU8825_REG_BOOST, 0x0 },
	{ NAU8825_REG_FEPGA, 0x0 },
	{ NAU8825_REG_POWER_UP_CONTROL, 0x0 },
	{ NAU8825_REG_CHARGE_PUMP, 0x0 },
};

 
static struct reg_default nau8825_xtalk_baktab[] = {
	{ NAU8825_REG_ADC_DGAIN_CTRL, 0x00cf },
	{ NAU8825_REG_HSVOL_CTRL, 0 },
	{ NAU8825_REG_DACL_CTRL, 0x00cf },
	{ NAU8825_REG_DACR_CTRL, 0x02cf },
};

 
static const struct reg_sequence nau8825_regmap_patch[] = {
	{ NAU8825_REG_FLL2, 0x0000 },
	{ NAU8825_REG_FLL4, 0x8010 },
	{ NAU8825_REG_FLL_VCO_RSV, 0x0bc0 },
	{ NAU8825_REG_INTERRUPT_MASK, 0x0800 },
	{ NAU8825_REG_DACL_CTRL, 0x00cf },
	{ NAU8825_REG_DACR_CTRL, 0x02cf },
	{ NAU8825_REG_OPT_EFUSE_CTRL, 0x0400 },
	{ NAU8825_REG_FLL2_LOWER, 0x26e9 },
	{ NAU8825_REG_FLL2_UPPER, 0x0031 },
	{ NAU8825_REG_ANALOG_CONTROL_2, 0x0020 },
	{ NAU8825_REG_ANALOG_ADC_2, 0x0220 },
	{ NAU8825_REG_MIC_BIAS, 0x0046 },
};

 
static int nau8825_sema_acquire(struct nau8825 *nau8825, long timeout)
{
	int ret;

	if (timeout) {
		ret = down_timeout(&nau8825->xtalk_sem, timeout);
		if (ret < 0)
			dev_warn(nau8825->dev, "Acquire semaphore timeout\n");
	} else {
		ret = down_trylock(&nau8825->xtalk_sem);
		if (ret)
			dev_warn(nau8825->dev, "Acquire semaphore fail\n");
	}

	return ret;
}

 
static inline void nau8825_sema_release(struct nau8825 *nau8825)
{
	up(&nau8825->xtalk_sem);
}

 
static inline void nau8825_sema_reset(struct nau8825 *nau8825)
{
	nau8825->xtalk_sem.count = 1;
}

 
static void nau8825_hpvol_ramp(struct nau8825 *nau8825,
	unsigned int vol_from, unsigned int vol_to, unsigned int step)
{
	unsigned int value, volume, ramp_up, from, to;

	if (vol_from == vol_to || step == 0) {
		return;
	} else if (vol_from < vol_to) {
		ramp_up = true;
		from = vol_from;
		to = vol_to;
	} else {
		ramp_up = false;
		from = vol_to;
		to = vol_from;
	}
	 
	if (to > NAU8825_HP_VOL_MIN)
		to = NAU8825_HP_VOL_MIN;

	for (volume = from; volume < to; volume += step) {
		if (ramp_up)
			value = volume;
		else
			value = to - volume + from;
		regmap_update_bits(nau8825->regmap, NAU8825_REG_HSVOL_CTRL,
			NAU8825_HPL_VOL_MASK | NAU8825_HPR_VOL_MASK,
			(value << NAU8825_HPL_VOL_SFT) | value);
		usleep_range(10000, 10500);
	}
	if (ramp_up)
		value = to;
	else
		value = from;
	regmap_update_bits(nau8825->regmap, NAU8825_REG_HSVOL_CTRL,
		NAU8825_HPL_VOL_MASK | NAU8825_HPR_VOL_MASK,
		(value << NAU8825_HPL_VOL_SFT) | value);
}

 
static u32 nau8825_intlog10_dec3(u32 value)
{
	return intlog10(value) / ((1 << 24) / 1000);
}

 
static u32 nau8825_xtalk_sidetone(u32 sig_org, u32 sig_cros)
{
	u32 gain, sidetone;

	if (WARN_ON(sig_org == 0 || sig_cros == 0))
		return 0;

	sig_org = nau8825_intlog10_dec3(sig_org);
	sig_cros = nau8825_intlog10_dec3(sig_cros);
	if (sig_org >= sig_cros)
		gain = (sig_org - sig_cros) * 20 + GAIN_AUGMENT;
	else
		gain = (sig_cros - sig_org) * 20 + GAIN_AUGMENT;
	sidetone = SIDETONE_BASE - gain * 2;
	sidetone /= 1000;

	return sidetone;
}

static int nau8825_xtalk_baktab_index_by_reg(unsigned int reg)
{
	int index;

	for (index = 0; index < ARRAY_SIZE(nau8825_xtalk_baktab); index++)
		if (nau8825_xtalk_baktab[index].reg == reg)
			return index;
	return -EINVAL;
}

static void nau8825_xtalk_backup(struct nau8825 *nau8825)
{
	int i;

	if (nau8825->xtalk_baktab_initialized)
		return;

	 
	for (i = 0; i < ARRAY_SIZE(nau8825_xtalk_baktab); i++)
		regmap_read(nau8825->regmap, nau8825_xtalk_baktab[i].reg,
				&nau8825_xtalk_baktab[i].def);

	nau8825->xtalk_baktab_initialized = true;
}

static void nau8825_xtalk_restore(struct nau8825 *nau8825, bool cause_cancel)
{
	int i, volume;

	if (!nau8825->xtalk_baktab_initialized)
		return;

	 
	for (i = 0; i < ARRAY_SIZE(nau8825_xtalk_baktab); i++) {
		if (!cause_cancel && nau8825_xtalk_baktab[i].reg ==
			NAU8825_REG_HSVOL_CTRL) {
			 
			volume = nau8825_xtalk_baktab[i].def &
				NAU8825_HPR_VOL_MASK;
			nau8825_hpvol_ramp(nau8825, 0, volume, 3);
			continue;
		}
		regmap_write(nau8825->regmap, nau8825_xtalk_baktab[i].reg,
				nau8825_xtalk_baktab[i].def);
	}

	nau8825->xtalk_baktab_initialized = false;
}

static void nau8825_xtalk_prepare_dac(struct nau8825 *nau8825)
{
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_ENA_CTRL,
		NAU8825_ENABLE_DACR | NAU8825_ENABLE_DACL |
		NAU8825_ENABLE_ADC | NAU8825_ENABLE_ADC_CLK |
		NAU8825_ENABLE_DAC_CLK, NAU8825_ENABLE_DACR |
		NAU8825_ENABLE_DACL | NAU8825_ENABLE_ADC |
		NAU8825_ENABLE_ADC_CLK | NAU8825_ENABLE_DAC_CLK);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
		NAU8825_JAMNODCLOW | NAU8825_CHANRGE_PUMP_EN,
		NAU8825_JAMNODCLOW | NAU8825_CHANRGE_PUMP_EN);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_RDAC,
		NAU8825_RDAC_EN | NAU8825_RDAC_CLK_EN |
		NAU8825_RDAC_FS_BCLK_ENB,
		NAU8825_RDAC_EN | NAU8825_RDAC_CLK_EN);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_POWER_UP_CONTROL,
		NAU8825_POWERUP_INTEGR_R | NAU8825_POWERUP_INTEGR_L |
		NAU8825_POWERUP_DRV_IN_R | NAU8825_POWERUP_DRV_IN_L,
		NAU8825_POWERUP_INTEGR_R | NAU8825_POWERUP_INTEGR_L |
		NAU8825_POWERUP_DRV_IN_R | NAU8825_POWERUP_DRV_IN_L);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_POWER_UP_CONTROL,
		NAU8825_POWERUP_HP_DRV_R | NAU8825_POWERUP_HP_DRV_L,
		NAU8825_POWERUP_HP_DRV_R | NAU8825_POWERUP_HP_DRV_L);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_HSD_CTRL,
		NAU8825_SPKR_DWN1R | NAU8825_SPKR_DWN1L, 0);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_BOOST,
		NAU8825_HP_BOOST_DIS, NAU8825_HP_BOOST_DIS);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_CLASSG_CTRL,
		NAU8825_CLASSG_LDAC_EN | NAU8825_CLASSG_RDAC_EN,
		NAU8825_CLASSG_LDAC_EN | NAU8825_CLASSG_RDAC_EN);
}

static void nau8825_xtalk_prepare_adc(struct nau8825 *nau8825)
{
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_ANALOG_ADC_2,
		NAU8825_POWERUP_ADCL | NAU8825_ADC_VREFSEL_MASK,
		NAU8825_POWERUP_ADCL | NAU8825_ADC_VREFSEL_VMID_PLUS_0_5DB);
}

static void nau8825_xtalk_clock(struct nau8825 *nau8825)
{
	 
	regmap_write(nau8825->regmap, NAU8825_REG_FLL1, 0x0);
	regmap_write(nau8825->regmap, NAU8825_REG_FLL2, 0x3126);
	regmap_write(nau8825->regmap, NAU8825_REG_FLL3, 0x0008);
	regmap_write(nau8825->regmap, NAU8825_REG_FLL4, 0x0010);
	regmap_write(nau8825->regmap, NAU8825_REG_FLL5, 0x0);
	regmap_write(nau8825->regmap, NAU8825_REG_FLL6, 0x6000);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_CLK_DIVIDER,
		NAU8825_CLK_SRC_MASK, NAU8825_CLK_SRC_VCO);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_FLL6, NAU8825_DCO_EN,
		NAU8825_DCO_EN);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_CLK_DIVIDER,
		NAU8825_CLK_MCLK_SRC_MASK, 0xf);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_FLL1,
		NAU8825_FLL_RATIO_MASK, 0x10);
}

static void nau8825_xtalk_prepare(struct nau8825 *nau8825)
{
	int volume, index;

	 
	nau8825_xtalk_backup(nau8825);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL2,
		NAU8825_I2S_MS_MASK | NAU8825_I2S_LRC_DIV_MASK |
		NAU8825_I2S_BLK_DIV_MASK, NAU8825_I2S_MS_MASTER |
		(0x2 << NAU8825_I2S_LRC_DIV_SFT) | 0x1);
	 
	index = nau8825_xtalk_baktab_index_by_reg(NAU8825_REG_HSVOL_CTRL);
	if (index != -EINVAL) {
		volume = nau8825_xtalk_baktab[index].def &
				NAU8825_HPR_VOL_MASK;
		nau8825_hpvol_ramp(nau8825, volume, 0, 3);
	}
	nau8825_xtalk_clock(nau8825);
	nau8825_xtalk_prepare_dac(nau8825);
	nau8825_xtalk_prepare_adc(nau8825);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_DACL_CTRL,
		NAU8825_DACL_CH_SEL_MASK | NAU8825_DACL_CH_VOL_MASK,
		NAU8825_DACL_CH_SEL_L | 0xab);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_DACR_CTRL,
		NAU8825_DACR_CH_SEL_MASK | NAU8825_DACR_CH_VOL_MASK,
		NAU8825_DACR_CH_SEL_R | 0xab);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_IMM_MODE_CTRL,
		NAU8825_IMM_THD_MASK | NAU8825_IMM_GEN_VOL_MASK |
		NAU8825_IMM_CYC_MASK | NAU8825_IMM_DAC_SRC_MASK,
		(0x9 << NAU8825_IMM_THD_SFT) | NAU8825_IMM_GEN_VOL_1_16th |
		NAU8825_IMM_CYC_8192 | NAU8825_IMM_DAC_SRC_SIN);
	 
	regmap_update_bits(nau8825->regmap,
		NAU8825_REG_INTERRUPT_MASK, NAU8825_IRQ_RMS_EN, 0);
	 
	if (nau8825->sw_id == NAU8825_SOFTWARE_ID_NAU8825)
		regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
				   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL, 0);
	else
		regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
				   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL,
				   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL);
}

static void nau8825_xtalk_clean_dac(struct nau8825 *nau8825)
{
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_BOOST,
		NAU8825_HP_BOOST_DIS, 0);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_HSD_CTRL,
		NAU8825_SPKR_DWN1R | NAU8825_SPKR_DWN1L,
		NAU8825_SPKR_DWN1R | NAU8825_SPKR_DWN1L);
	 
	if (nau8825->sw_id == NAU8825_SOFTWARE_ID_NAU8825)
		regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
				   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL,
				   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL);
	else
		regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
				   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL, 0);

	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_BIAS_ADJ,
		NAU8825_BIAS_HPR_IMP | NAU8825_BIAS_HPL_IMP |
		NAU8825_BIAS_TESTDAC_EN, NAU8825_BIAS_TESTDAC_EN);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_POWER_UP_CONTROL,
		NAU8825_POWERUP_HP_DRV_R | NAU8825_POWERUP_HP_DRV_L, 0);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_POWER_UP_CONTROL,
		NAU8825_POWERUP_INTEGR_R | NAU8825_POWERUP_INTEGR_L |
		NAU8825_POWERUP_DRV_IN_R | NAU8825_POWERUP_DRV_IN_L, 0);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_RDAC,
		NAU8825_RDAC_EN | NAU8825_RDAC_CLK_EN, 0);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
		NAU8825_JAMNODCLOW | NAU8825_CHANRGE_PUMP_EN, 0);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_ENA_CTRL,
		NAU8825_ENABLE_DACR | NAU8825_ENABLE_DACL |
		NAU8825_ENABLE_ADC_CLK | NAU8825_ENABLE_DAC_CLK, 0);
	if (!nau8825->irq)
		regmap_update_bits(nau8825->regmap,
			NAU8825_REG_ENA_CTRL, NAU8825_ENABLE_ADC, 0);
}

static void nau8825_xtalk_clean_adc(struct nau8825 *nau8825)
{
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_ANALOG_ADC_2,
		NAU8825_POWERUP_ADCL | NAU8825_ADC_VREFSEL_MASK, 0);
}

static void nau8825_xtalk_clean(struct nau8825 *nau8825, bool cause_cancel)
{
	 
	nau8825_configure_sysclk(nau8825, NAU8825_CLK_INTERNAL, 0);
	nau8825_xtalk_clean_dac(nau8825);
	nau8825_xtalk_clean_adc(nau8825);
	 
	regmap_write(nau8825->regmap, NAU8825_REG_IMM_MODE_CTRL, 0);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_INTERRUPT_MASK,
		NAU8825_IRQ_RMS_EN, NAU8825_IRQ_RMS_EN);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL2,
		NAU8825_I2S_MS_MASK | NAU8825_I2S_LRC_DIV_MASK |
		NAU8825_I2S_BLK_DIV_MASK, NAU8825_I2S_MS_SLAVE);
	 
	nau8825_xtalk_restore(nau8825, cause_cancel);
}

static void nau8825_xtalk_imm_start(struct nau8825 *nau8825, int vol)
{
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_ADC_DGAIN_CTRL,
				NAU8825_ADC_DIG_VOL_MASK, vol);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_BIAS_ADJ,
		NAU8825_BIAS_TESTDACR_EN | NAU8825_BIAS_TESTDACL_EN,
		NAU8825_BIAS_TESTDACL_EN);
	switch (nau8825->xtalk_state) {
	case NAU8825_XTALK_HPR_R2L:
		 
		regmap_update_bits(nau8825->regmap, NAU8825_REG_BIAS_ADJ,
			NAU8825_BIAS_HPR_IMP | NAU8825_BIAS_HPL_IMP,
			NAU8825_BIAS_HPR_IMP);
		break;
	case NAU8825_XTALK_HPL_R2L:
		 
		regmap_update_bits(nau8825->regmap, NAU8825_REG_BIAS_ADJ,
			NAU8825_BIAS_HPR_IMP | NAU8825_BIAS_HPL_IMP,
			NAU8825_BIAS_HPL_IMP);
		break;
	default:
		break;
	}
	msleep(100);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_IMM_MODE_CTRL,
				NAU8825_IMM_EN, NAU8825_IMM_EN);
}

static void nau8825_xtalk_imm_stop(struct nau8825 *nau8825)
{
	 
	regmap_update_bits(nau8825->regmap,
		NAU8825_REG_IMM_MODE_CTRL, NAU8825_IMM_EN, 0);
}

 
static void nau8825_xtalk_measure(struct nau8825 *nau8825)
{
	u32 sidetone;

	switch (nau8825->xtalk_state) {
	case NAU8825_XTALK_PREPARE:
		 
		nau8825_xtalk_prepare(nau8825);
		msleep(280);
		 
		nau8825->xtalk_state = NAU8825_XTALK_HPR_R2L;
		nau8825_xtalk_imm_start(nau8825, 0x00d2);
		break;
	case NAU8825_XTALK_HPR_R2L:
		 
		regmap_read(nau8825->regmap, NAU8825_REG_IMM_RMS_L,
			&nau8825->imp_rms[NAU8825_XTALK_HPR_R2L]);
		dev_dbg(nau8825->dev, "HPR_R2L imm: %x\n",
			nau8825->imp_rms[NAU8825_XTALK_HPR_R2L]);
		 
		nau8825_xtalk_imm_stop(nau8825);
		 
		nau8825->xtalk_state = NAU8825_XTALK_HPL_R2L;
		nau8825_xtalk_imm_start(nau8825, 0x00ff);
		break;
	case NAU8825_XTALK_HPL_R2L:
		 
		regmap_read(nau8825->regmap, NAU8825_REG_IMM_RMS_L,
			&nau8825->imp_rms[NAU8825_XTALK_HPL_R2L]);
		dev_dbg(nau8825->dev, "HPL_R2L imm: %x\n",
			nau8825->imp_rms[NAU8825_XTALK_HPL_R2L]);
		nau8825_xtalk_imm_stop(nau8825);
		msleep(150);
		nau8825->xtalk_state = NAU8825_XTALK_IMM;
		break;
	case NAU8825_XTALK_IMM:
		 
		sidetone = nau8825_xtalk_sidetone(
			nau8825->imp_rms[NAU8825_XTALK_HPR_R2L],
			nau8825->imp_rms[NAU8825_XTALK_HPL_R2L]);
		dev_dbg(nau8825->dev, "cross talk sidetone: %x\n", sidetone);
		regmap_write(nau8825->regmap, NAU8825_REG_DAC_DGAIN_CTRL,
					(sidetone << 8) | sidetone);
		nau8825_xtalk_clean(nau8825, false);
		nau8825->xtalk_state = NAU8825_XTALK_DONE;
		break;
	default:
		break;
	}
}

static void nau8825_xtalk_work(struct work_struct *work)
{
	struct nau8825 *nau8825 = container_of(
		work, struct nau8825, xtalk_work);

	nau8825_xtalk_measure(nau8825);
	 
	if (nau8825->xtalk_state == NAU8825_XTALK_IMM)
		nau8825_xtalk_measure(nau8825);

	 
	if (nau8825->xtalk_state == NAU8825_XTALK_DONE) {
		snd_soc_jack_report(nau8825->jack, nau8825->xtalk_event,
				nau8825->xtalk_event_mask);
		nau8825_sema_release(nau8825);
		nau8825->xtalk_protect = false;
	}
}

static void nau8825_xtalk_cancel(struct nau8825 *nau8825)
{
	 
	if (nau8825->xtalk_enable && nau8825->xtalk_state !=
		NAU8825_XTALK_DONE) {
		cancel_work_sync(&nau8825->xtalk_work);
		nau8825_xtalk_clean(nau8825, true);
	}
	 
	nau8825_sema_reset(nau8825);
	nau8825->xtalk_state = NAU8825_XTALK_DONE;
	nau8825->xtalk_protect = false;
}

static bool nau8825_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case NAU8825_REG_ENA_CTRL ... NAU8825_REG_FLL_VCO_RSV:
	case NAU8825_REG_HSD_CTRL ... NAU8825_REG_JACK_DET_CTRL:
	case NAU8825_REG_INTERRUPT_MASK ... NAU8825_REG_KEYDET_CTRL:
	case NAU8825_REG_VDET_THRESHOLD_1 ... NAU8825_REG_DACR_CTRL:
	case NAU8825_REG_ADC_DRC_KNEE_IP12 ... NAU8825_REG_ADC_DRC_ATKDCY:
	case NAU8825_REG_DAC_DRC_KNEE_IP12 ... NAU8825_REG_DAC_DRC_ATKDCY:
	case NAU8825_REG_IMM_MODE_CTRL ... NAU8825_REG_IMM_RMS_R:
	case NAU8825_REG_CLASSG_CTRL ... NAU8825_REG_OPT_EFUSE_CTRL:
	case NAU8825_REG_MISC_CTRL:
	case NAU8825_REG_I2C_DEVICE_ID ... NAU8825_REG_FLL2_UPPER:
	case NAU8825_REG_BIAS_ADJ:
	case NAU8825_REG_TRIM_SETTINGS ... NAU8825_REG_ANALOG_CONTROL_2:
	case NAU8825_REG_ANALOG_ADC_1 ... NAU8825_REG_MIC_BIAS:
	case NAU8825_REG_BOOST ... NAU8825_REG_FEPGA:
	case NAU8825_REG_POWER_UP_CONTROL ... NAU8825_REG_GENERAL_STATUS:
		return true;
	default:
		return false;
	}

}

static bool nau8825_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case NAU8825_REG_RESET ... NAU8825_REG_FLL_VCO_RSV:
	case NAU8825_REG_HSD_CTRL ... NAU8825_REG_JACK_DET_CTRL:
	case NAU8825_REG_INTERRUPT_MASK:
	case NAU8825_REG_INT_CLR_KEY_STATUS ... NAU8825_REG_KEYDET_CTRL:
	case NAU8825_REG_VDET_THRESHOLD_1 ... NAU8825_REG_DACR_CTRL:
	case NAU8825_REG_ADC_DRC_KNEE_IP12 ... NAU8825_REG_ADC_DRC_ATKDCY:
	case NAU8825_REG_DAC_DRC_KNEE_IP12 ... NAU8825_REG_DAC_DRC_ATKDCY:
	case NAU8825_REG_IMM_MODE_CTRL:
	case NAU8825_REG_CLASSG_CTRL ... NAU8825_REG_OPT_EFUSE_CTRL:
	case NAU8825_REG_MISC_CTRL:
	case NAU8825_REG_FLL2_LOWER ... NAU8825_REG_FLL2_UPPER:
	case NAU8825_REG_BIAS_ADJ:
	case NAU8825_REG_TRIM_SETTINGS ... NAU8825_REG_ANALOG_CONTROL_2:
	case NAU8825_REG_ANALOG_ADC_1 ... NAU8825_REG_MIC_BIAS:
	case NAU8825_REG_BOOST ... NAU8825_REG_FEPGA:
	case NAU8825_REG_POWER_UP_CONTROL ... NAU8825_REG_CHARGE_PUMP:
		return true;
	default:
		return false;
	}
}

static bool nau8825_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case NAU8825_REG_RESET:
	case NAU8825_REG_IRQ_STATUS:
	case NAU8825_REG_INT_CLR_KEY_STATUS:
	case NAU8825_REG_IMM_RMS_L:
	case NAU8825_REG_IMM_RMS_R:
	case NAU8825_REG_I2C_DEVICE_ID:
	case NAU8825_REG_SARDOUT_RAM_STATUS:
	case NAU8825_REG_CHARGE_PUMP_INPUT_READ:
	case NAU8825_REG_GENERAL_STATUS:
	case NAU8825_REG_BIQ_CTRL ... NAU8825_REG_BIQ_COF10:
		return true;
	default:
		return false;
	}
}

static int nau8825_fepga_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_update_bits(nau8825->regmap, NAU8825_REG_FEPGA,
				   NAU8825_ACDC_CTRL_MASK,
				   NAU8825_ACDC_VREF_MICP | NAU8825_ACDC_VREF_MICN);
		regmap_update_bits(nau8825->regmap, NAU8825_REG_BOOST,
				   NAU8825_DISCHRG_EN, NAU8825_DISCHRG_EN);
		msleep(40);
		regmap_update_bits(nau8825->regmap, NAU8825_REG_BOOST,
				   NAU8825_DISCHRG_EN, 0);
		regmap_update_bits(nau8825->regmap, NAU8825_REG_FEPGA,
				   NAU8825_ACDC_CTRL_MASK, 0);
		break;
	default:
		break;
	}

	return 0;
}

static int nau8825_adc_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(nau8825->adc_delay);
		regmap_update_bits(nau8825->regmap, NAU8825_REG_ENA_CTRL,
			NAU8825_ENABLE_ADC, NAU8825_ENABLE_ADC);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!nau8825->irq)
			regmap_update_bits(nau8825->regmap,
				NAU8825_REG_ENA_CTRL, NAU8825_ENABLE_ADC, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int nau8825_pump_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		 
		msleep(10);
		regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
			NAU8825_JAMNODCLOW, NAU8825_JAMNODCLOW);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
			NAU8825_JAMNODCLOW, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int nau8825_output_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		 
		regmap_update_bits(nau8825->regmap, NAU8825_REG_BIAS_ADJ,
			NAU8825_BIAS_TESTDAC_EN, 0);
		if (nau8825->sw_id == NAU8825_SOFTWARE_ID_NAU8825)
			regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
					   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL, 0);
		else
			regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
					   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL,
					   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(nau8825->regmap, NAU8825_REG_BIAS_ADJ,
			NAU8825_BIAS_TESTDAC_EN, NAU8825_BIAS_TESTDAC_EN);
		if (nau8825->sw_id == NAU8825_SOFTWARE_ID_NAU8825)
			regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
					   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL,
					   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL);
		else
			regmap_update_bits(nau8825->regmap, NAU8825_REG_CHARGE_PUMP,
					   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL, 0);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int system_clock_control(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *k, int  event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);
	struct regmap *regmap = nau8825->regmap;

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		dev_dbg(nau8825->dev, "system clock control : POWER OFF\n");
		 
		if (nau8825_is_jack_inserted(regmap)) {
			nau8825_configure_sysclk(nau8825,
						 NAU8825_CLK_INTERNAL, 0);
		} else {
			nau8825_configure_sysclk(nau8825, NAU8825_CLK_DIS, 0);
		}
	}

	return 0;
}

static int nau8825_biq_coeff_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;

	if (!component->regmap)
		return -EINVAL;

	regmap_raw_read(component->regmap, NAU8825_REG_BIQ_COF1,
		ucontrol->value.bytes.data, params->max);
	return 0;
}

static int nau8825_biq_coeff_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	void *data;

	if (!component->regmap)
		return -EINVAL;

	data = kmemdup(ucontrol->value.bytes.data,
		params->max, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	regmap_update_bits(component->regmap, NAU8825_REG_BIQ_CTRL,
		NAU8825_BIQ_WRT_EN, 0);
	regmap_raw_write(component->regmap, NAU8825_REG_BIQ_COF1,
		data, params->max);
	regmap_update_bits(component->regmap, NAU8825_REG_BIQ_CTRL,
		NAU8825_BIQ_WRT_EN, NAU8825_BIQ_WRT_EN);

	kfree(data);
	return 0;
}

static const char * const nau8825_biq_path[] = {
	"ADC", "DAC"
};

static const struct soc_enum nau8825_biq_path_enum =
	SOC_ENUM_SINGLE(NAU8825_REG_BIQ_CTRL, NAU8825_BIQ_PATH_SFT,
		ARRAY_SIZE(nau8825_biq_path), nau8825_biq_path);

static const char * const nau8825_adc_decimation[] = {
	"32", "64", "128", "256"
};

static const struct soc_enum nau8825_adc_decimation_enum =
	SOC_ENUM_SINGLE(NAU8825_REG_ADC_RATE, NAU8825_ADC_SYNC_DOWN_SFT,
		ARRAY_SIZE(nau8825_adc_decimation), nau8825_adc_decimation);

static const char * const nau8825_dac_oversampl[] = {
	"64", "256", "128", "", "32"
};

static const struct soc_enum nau8825_dac_oversampl_enum =
	SOC_ENUM_SINGLE(NAU8825_REG_DAC_CTRL1, NAU8825_DAC_OVERSAMPLE_SFT,
		ARRAY_SIZE(nau8825_dac_oversampl), nau8825_dac_oversampl);

static const DECLARE_TLV_DB_MINMAX_MUTE(adc_vol_tlv, -10300, 2400);
static const DECLARE_TLV_DB_MINMAX_MUTE(sidetone_vol_tlv, -4200, 0);
static const DECLARE_TLV_DB_MINMAX(dac_vol_tlv, -5400, 0);
static const DECLARE_TLV_DB_MINMAX(fepga_gain_tlv, -100, 3600);
static const DECLARE_TLV_DB_MINMAX_MUTE(crosstalk_vol_tlv, -9600, 2400);

static const struct snd_kcontrol_new nau8825_controls[] = {
	SOC_SINGLE_TLV("Mic Volume", NAU8825_REG_ADC_DGAIN_CTRL,
		0, 0xff, 0, adc_vol_tlv),
	SOC_DOUBLE_TLV("Headphone Bypass Volume", NAU8825_REG_ADC_DGAIN_CTRL,
		12, 8, 0x0f, 0, sidetone_vol_tlv),
	SOC_DOUBLE_TLV("Headphone Volume", NAU8825_REG_HSVOL_CTRL,
		6, 0, 0x3f, 1, dac_vol_tlv),
	SOC_SINGLE_TLV("Frontend PGA Volume", NAU8825_REG_POWER_UP_CONTROL,
		8, 37, 0, fepga_gain_tlv),
	SOC_DOUBLE_TLV("Headphone Crosstalk Volume", NAU8825_REG_DAC_DGAIN_CTRL,
		0, 8, 0xff, 0, crosstalk_vol_tlv),

	SOC_ENUM("ADC Decimation Rate", nau8825_adc_decimation_enum),
	SOC_ENUM("DAC Oversampling Rate", nau8825_dac_oversampl_enum),
	 
	SOC_ENUM("BIQ Path Select", nau8825_biq_path_enum),
	SND_SOC_BYTES_EXT("BIQ Coefficients", 20,
		  nau8825_biq_coeff_get, nau8825_biq_coeff_put),
};

 
static const char * const nau8825_dac_src[] = {
	"DACL", "DACR",
};

static SOC_ENUM_SINGLE_DECL(
	nau8825_dacl_enum, NAU8825_REG_DACL_CTRL,
	NAU8825_DACL_CH_SEL_SFT, nau8825_dac_src);

static SOC_ENUM_SINGLE_DECL(
	nau8825_dacr_enum, NAU8825_REG_DACR_CTRL,
	NAU8825_DACR_CH_SEL_SFT, nau8825_dac_src);

static const struct snd_kcontrol_new nau8825_dacl_mux =
	SOC_DAPM_ENUM("DACL Source", nau8825_dacl_enum);

static const struct snd_kcontrol_new nau8825_dacr_mux =
	SOC_DAPM_ENUM("DACR Source", nau8825_dacr_enum);


static const struct snd_soc_dapm_widget nau8825_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_OUT("AIFTX", "Capture", 0, NAU8825_REG_I2S_PCM_CTRL2,
		15, 1),
	SND_SOC_DAPM_AIF_IN("AIFRX", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_SUPPLY("System Clock", SND_SOC_NOPM, 0, 0,
			    system_clock_control, SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("MIC"),
	SND_SOC_DAPM_MICBIAS("MICBIAS", NAU8825_REG_MIC_BIAS, 8, 0),

	SND_SOC_DAPM_PGA_E("Frontend PGA", NAU8825_REG_POWER_UP_CONTROL, 14, 0,
			   NULL, 0, nau8825_fepga_event, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_ADC_E("ADC", NULL, SND_SOC_NOPM, 0, 0,
		nau8825_adc_event, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("ADC Clock", NAU8825_REG_ENA_CTRL, 7, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("ADC Power", NAU8825_REG_ANALOG_ADC_2, 6, 0, NULL,
		0),

	 
	SND_SOC_DAPM_SUPPLY("SAR", NAU8825_REG_SAR_CTRL,
		NAU8825_SAR_ADC_EN_SFT, 0, NULL, 0),

	SND_SOC_DAPM_PGA_S("ADACL", 2, NAU8825_REG_RDAC, 12, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADACR", 2, NAU8825_REG_RDAC, 13, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADACL Clock", 3, NAU8825_REG_RDAC, 8, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("ADACR Clock", 3, NAU8825_REG_RDAC, 9, 0, NULL, 0),

	SND_SOC_DAPM_DAC("DDACR", NULL, NAU8825_REG_ENA_CTRL,
		NAU8825_ENABLE_DACR_SFT, 0),
	SND_SOC_DAPM_DAC("DDACL", NULL, NAU8825_REG_ENA_CTRL,
		NAU8825_ENABLE_DACL_SFT, 0),
	SND_SOC_DAPM_SUPPLY("DDAC Clock", NAU8825_REG_ENA_CTRL, 6, 0, NULL, 0),

	SND_SOC_DAPM_MUX("DACL Mux", SND_SOC_NOPM, 0, 0, &nau8825_dacl_mux),
	SND_SOC_DAPM_MUX("DACR Mux", SND_SOC_NOPM, 0, 0, &nau8825_dacr_mux),

	SND_SOC_DAPM_PGA_S("HP amp L", 0,
		NAU8825_REG_CLASSG_CTRL, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("HP amp R", 0,
		NAU8825_REG_CLASSG_CTRL, 2, 0, NULL, 0),

	SND_SOC_DAPM_PGA_S("Charge Pump", 1, NAU8825_REG_CHARGE_PUMP, 5, 0,
		nau8825_pump_event, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_S("Output Driver R Stage 1", 4,
		NAU8825_REG_POWER_UP_CONTROL, 5, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Output Driver L Stage 1", 4,
		NAU8825_REG_POWER_UP_CONTROL, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Output Driver R Stage 2", 5,
		NAU8825_REG_POWER_UP_CONTROL, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Output Driver L Stage 2", 5,
		NAU8825_REG_POWER_UP_CONTROL, 2, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Output Driver R Stage 3", 6,
		NAU8825_REG_POWER_UP_CONTROL, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA_S("Output Driver L Stage 3", 6,
		NAU8825_REG_POWER_UP_CONTROL, 0, 0, NULL, 0),

	SND_SOC_DAPM_PGA_S("Output DACL", 7,
		SND_SOC_NOPM, 0, 0, nau8825_output_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_S("Output DACR", 7,
		SND_SOC_NOPM, 0, 0, nau8825_output_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),


	 
	SND_SOC_DAPM_PGA_S("HPOL Pulldown", 8,
		NAU8825_REG_HSD_CTRL, 0, 1, NULL, 0),
	SND_SOC_DAPM_PGA_S("HPOR Pulldown", 8,
		NAU8825_REG_HSD_CTRL, 1, 1, NULL, 0),

	 
	SND_SOC_DAPM_PGA_S("HP Boost Driver", 9,
		NAU8825_REG_BOOST, 9, 1, NULL, 0),

	 
	SND_SOC_DAPM_PGA_S("Class G", 10,
		NAU8825_REG_CLASSG_CTRL, 0, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("HPOL"),
	SND_SOC_DAPM_OUTPUT("HPOR"),
};

static const struct snd_soc_dapm_route nau8825_dapm_routes[] = {
	{"Frontend PGA", NULL, "MIC"},
	{"ADC", NULL, "Frontend PGA"},
	{"ADC", NULL, "ADC Clock"},
	{"ADC", NULL, "ADC Power"},
	{"AIFTX", NULL, "ADC"},
	{"AIFTX", NULL, "System Clock"},

	{"AIFRX", NULL, "System Clock"},
	{"DDACL", NULL, "AIFRX"},
	{"DDACR", NULL, "AIFRX"},
	{"DDACL", NULL, "DDAC Clock"},
	{"DDACR", NULL, "DDAC Clock"},
	{"DACL Mux", "DACL", "DDACL"},
	{"DACL Mux", "DACR", "DDACR"},
	{"DACR Mux", "DACL", "DDACL"},
	{"DACR Mux", "DACR", "DDACR"},
	{"HP amp L", NULL, "DACL Mux"},
	{"HP amp R", NULL, "DACR Mux"},
	{"Charge Pump", NULL, "HP amp L"},
	{"Charge Pump", NULL, "HP amp R"},
	{"ADACL", NULL, "Charge Pump"},
	{"ADACR", NULL, "Charge Pump"},
	{"ADACL Clock", NULL, "ADACL"},
	{"ADACR Clock", NULL, "ADACR"},
	{"Output Driver L Stage 1", NULL, "ADACL Clock"},
	{"Output Driver R Stage 1", NULL, "ADACR Clock"},
	{"Output Driver L Stage 2", NULL, "Output Driver L Stage 1"},
	{"Output Driver R Stage 2", NULL, "Output Driver R Stage 1"},
	{"Output Driver L Stage 3", NULL, "Output Driver L Stage 2"},
	{"Output Driver R Stage 3", NULL, "Output Driver R Stage 2"},
	{"Output DACL", NULL, "Output Driver L Stage 3"},
	{"Output DACR", NULL, "Output Driver R Stage 3"},
	{"HPOL Pulldown", NULL, "Output DACL"},
	{"HPOR Pulldown", NULL, "Output DACR"},
	{"HP Boost Driver", NULL, "HPOL Pulldown"},
	{"HP Boost Driver", NULL, "HPOR Pulldown"},
	{"Class G", NULL, "HP Boost Driver"},
	{"HPOL", NULL, "Class G"},
	{"HPOR", NULL, "Class G"},
};

static const struct nau8825_osr_attr *
nau8825_get_osr(struct nau8825 *nau8825, int stream)
{
	unsigned int osr;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		regmap_read(nau8825->regmap,
			    NAU8825_REG_DAC_CTRL1, &osr);
		osr &= NAU8825_DAC_OVERSAMPLE_MASK;
		if (osr >= ARRAY_SIZE(osr_dac_sel))
			return NULL;
		return &osr_dac_sel[osr];
	} else {
		regmap_read(nau8825->regmap,
			    NAU8825_REG_ADC_RATE, &osr);
		osr &= NAU8825_ADC_SYNC_DOWN_MASK;
		if (osr >= ARRAY_SIZE(osr_adc_sel))
			return NULL;
		return &osr_adc_sel[osr];
	}
}

static int nau8825_dai_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);
	const struct nau8825_osr_attr *osr;

	osr = nau8825_get_osr(nau8825, substream->stream);
	if (!osr || !osr->osr)
		return -EINVAL;

	return snd_pcm_hw_constraint_minmax(substream->runtime,
					    SNDRV_PCM_HW_PARAM_RATE,
					    0, CLK_DA_AD_MAX / osr->osr);
}

static int nau8825_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);
	unsigned int val_len = 0, ctrl_val, bclk_fs, bclk_div;
	const struct nau8825_osr_attr *osr;
	int err = -EINVAL;

	nau8825_sema_acquire(nau8825, 3 * HZ);

	 
	osr = nau8825_get_osr(nau8825, substream->stream);
	if (!osr || !osr->osr)
		goto error;
	if (params_rate(params) * osr->osr > CLK_DA_AD_MAX)
		goto error;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		regmap_update_bits(nau8825->regmap, NAU8825_REG_CLK_DIVIDER,
			NAU8825_CLK_DAC_SRC_MASK,
			osr->clk_src << NAU8825_CLK_DAC_SRC_SFT);
	else
		regmap_update_bits(nau8825->regmap, NAU8825_REG_CLK_DIVIDER,
			NAU8825_CLK_ADC_SRC_MASK,
			osr->clk_src << NAU8825_CLK_ADC_SRC_SFT);

	 
	regmap_read(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL2, &ctrl_val);
	if (ctrl_val & NAU8825_I2S_MS_MASTER) {
		 
		bclk_fs = snd_soc_params_to_bclk(params) / params_rate(params);
		if (bclk_fs <= 32)
			bclk_div = 2;
		else if (bclk_fs <= 64)
			bclk_div = 1;
		else if (bclk_fs <= 128)
			bclk_div = 0;
		else
			goto error;
		regmap_update_bits(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL2,
			NAU8825_I2S_LRC_DIV_MASK | NAU8825_I2S_BLK_DIV_MASK,
			((bclk_div + 1) << NAU8825_I2S_LRC_DIV_SFT) | bclk_div);
	}

	switch (params_width(params)) {
	case 16:
		val_len |= NAU8825_I2S_DL_16;
		break;
	case 20:
		val_len |= NAU8825_I2S_DL_20;
		break;
	case 24:
		val_len |= NAU8825_I2S_DL_24;
		break;
	case 32:
		val_len |= NAU8825_I2S_DL_32;
		break;
	default:
		goto error;
	}

	regmap_update_bits(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL1,
		NAU8825_I2S_DL_MASK, val_len);
	err = 0;

 error:
	 
	nau8825_sema_release(nau8825);

	return err;
}

static int nau8825_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);
	unsigned int ctrl1_val = 0, ctrl2_val = 0;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		ctrl2_val |= NAU8825_I2S_MS_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		ctrl1_val |= NAU8825_I2S_BP_INV;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		ctrl1_val |= NAU8825_I2S_DF_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		ctrl1_val |= NAU8825_I2S_DF_LEFT;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		ctrl1_val |= NAU8825_I2S_DF_RIGTH;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		ctrl1_val |= NAU8825_I2S_DF_PCM_AB;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		ctrl1_val |= NAU8825_I2S_DF_PCM_AB;
		ctrl1_val |= NAU8825_I2S_PCMB_EN;
		break;
	default:
		return -EINVAL;
	}

	nau8825_sema_acquire(nau8825, 3 * HZ);

	regmap_update_bits(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL1,
		NAU8825_I2S_DL_MASK | NAU8825_I2S_DF_MASK |
		NAU8825_I2S_BP_MASK | NAU8825_I2S_PCMB_MASK,
		ctrl1_val);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL2,
		NAU8825_I2S_MS_MASK, ctrl2_val);

	 
	nau8825_sema_release(nau8825);

	return 0;
}

 
static int nau8825_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
				unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_component *component = dai->component;
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);
	unsigned int ctrl_val = 0, ctrl_offset = 0, value = 0, dac_s, adc_s;

	if (slots != 4 && slots != 8) {
		dev_err(nau8825->dev, "Only support 4 or 8 slots!\n");
		return -EINVAL;
	}

	 
	if (hweight_long((unsigned long) tx_mask) != 1 ||
	    hweight_long((unsigned long) rx_mask) != 2) {
		dev_err(nau8825->dev,
			"The limitation is 1-channel for ADC, and 2-channel for DAC on TDM mode.\n");
		return -EINVAL;
	}

	if (((tx_mask & 0xf) && (tx_mask & 0xf0)) ||
	    ((rx_mask & 0xf) && (rx_mask & 0xf0)) ||
	    ((tx_mask & 0xf) && (rx_mask & 0xf0)) ||
	    ((rx_mask & 0xf) && (tx_mask & 0xf0))) {
		dev_err(nau8825->dev,
			"Slot assignment of DAC and ADC need to set same interval.\n");
		return -EINVAL;
	}

	 
	if (rx_mask & 0xf0) {
		regmap_update_bits(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL2,
				   NAU8825_I2S_PCM_TS_EN_MASK, NAU8825_I2S_PCM_TS_EN);
		regmap_read(nau8825->regmap, NAU8825_REG_I2S_PCM_CTRL1, &value);
		ctrl_val |= NAU8825_TDM_OFFSET_EN;
		ctrl_offset = 4 * slot_width;
		if (!(value & NAU8825_I2S_PCMB_MASK))
			ctrl_offset += 1;
		dac_s = (rx_mask & 0xf0) >> 4;
		adc_s = fls((tx_mask & 0xf0) >> 4);
	} else {
		dac_s = rx_mask & 0xf;
		adc_s = fls(tx_mask & 0xf);
	}

	ctrl_val |= NAU8825_TDM_MODE;

	switch (dac_s) {
	case 0x3:
		ctrl_val |= 1 << NAU8825_TDM_DACR_RX_SFT;
		break;
	case 0x5:
		ctrl_val |= 2 << NAU8825_TDM_DACR_RX_SFT;
		break;
	case 0x6:
		ctrl_val |= 1 << NAU8825_TDM_DACL_RX_SFT;
		ctrl_val |= 2 << NAU8825_TDM_DACR_RX_SFT;
		break;
	case 0x9:
		ctrl_val |= 3 << NAU8825_TDM_DACR_RX_SFT;
		break;
	case 0xa:
		ctrl_val |= 1 << NAU8825_TDM_DACL_RX_SFT;
		ctrl_val |= 3 << NAU8825_TDM_DACR_RX_SFT;
		break;
	case 0xc:
		ctrl_val |= 2 << NAU8825_TDM_DACL_RX_SFT;
		ctrl_val |= 3 << NAU8825_TDM_DACR_RX_SFT;
		break;
	default:
		return -EINVAL;
	}

	ctrl_val |= adc_s - 1;

	regmap_update_bits(nau8825->regmap, NAU8825_REG_TDM_CTRL,
			   NAU8825_TDM_MODE | NAU8825_TDM_OFFSET_EN |
			   NAU8825_TDM_DACL_RX_MASK | NAU8825_TDM_DACR_RX_MASK |
			   NAU8825_TDM_TX_MASK, ctrl_val);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_LEFT_TIME_SLOT,
			   NAU8825_TSLOT_L0_MASK, ctrl_offset);

	return 0;
}

static const struct snd_soc_dai_ops nau8825_dai_ops = {
	.startup	= nau8825_dai_startup,
	.hw_params	= nau8825_hw_params,
	.set_fmt	= nau8825_set_dai_fmt,
	.set_tdm_slot	= nau8825_set_tdm_slot,
};

#define NAU8825_RATES	SNDRV_PCM_RATE_8000_192000
#define NAU8825_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE \
			 | SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver nau8825_dai = {
	.name = "nau8825-hifi",
	.playback = {
		.stream_name	 = "Playback",
		.channels_min	 = 1,
		.channels_max	 = 2,
		.rates		 = NAU8825_RATES,
		.formats	 = NAU8825_FORMATS,
	},
	.capture = {
		.stream_name	 = "Capture",
		.channels_min	 = 1,
		.channels_max	 = 2,    
		.rates		 = NAU8825_RATES,
		.formats	 = NAU8825_FORMATS,
	},
	.ops = &nau8825_dai_ops,
};

 
int nau8825_enable_jack_detect(struct snd_soc_component *component,
				struct snd_soc_jack *jack)
{
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);
	struct regmap *regmap = nau8825->regmap;

	nau8825->jack = jack;

	if (!nau8825->jack) {
		regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL,
				   NAU8825_HSD_AUTO_MODE | NAU8825_SPKR_DWN1R |
				   NAU8825_SPKR_DWN1L, 0);
		return 0;
	}
	 
	regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL,
		NAU8825_HSD_AUTO_MODE | NAU8825_SPKR_DWN1R | NAU8825_SPKR_DWN1L,
		NAU8825_HSD_AUTO_MODE | NAU8825_SPKR_DWN1R | NAU8825_SPKR_DWN1L);

	return 0;
}
EXPORT_SYMBOL_GPL(nau8825_enable_jack_detect);


static bool nau8825_is_jack_inserted(struct regmap *regmap)
{
	bool active_high, is_high;
	int status, jkdet;

	regmap_read(regmap, NAU8825_REG_JACK_DET_CTRL, &jkdet);
	active_high = jkdet & NAU8825_JACK_POLARITY;
	regmap_read(regmap, NAU8825_REG_I2C_DEVICE_ID, &status);
	is_high = status & NAU8825_GPIO2JD1;
	 
	return active_high == is_high;
}

static void nau8825_restart_jack_detection(struct regmap *regmap)
{
	 
	regmap_update_bits(regmap, NAU8825_REG_JACK_DET_CTRL,
		NAU8825_JACK_DET_RESTART, NAU8825_JACK_DET_RESTART);
	regmap_update_bits(regmap, NAU8825_REG_JACK_DET_CTRL,
		NAU8825_JACK_DET_RESTART, 0);
}

static void nau8825_int_status_clear_all(struct regmap *regmap)
{
	int active_irq, clear_irq, i;

	 
	regmap_read(regmap, NAU8825_REG_IRQ_STATUS, &active_irq);
	for (i = 0; i < NAU8825_REG_DATA_LEN; i++) {
		clear_irq = (0x1 << i);
		if (active_irq & clear_irq)
			regmap_write(regmap,
				NAU8825_REG_INT_CLR_KEY_STATUS, clear_irq);
	}
}

static void nau8825_eject_jack(struct nau8825 *nau8825)
{
	struct snd_soc_dapm_context *dapm = nau8825->dapm;
	struct regmap *regmap = nau8825->regmap;

	 
	nau8825_xtalk_cancel(nau8825);

	snd_soc_dapm_disable_pin(dapm, "SAR");
	snd_soc_dapm_disable_pin(dapm, "MICBIAS");
	 
	regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
		NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2, 0);
	 
	regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL, 0xf, 0xf);

	snd_soc_dapm_sync(dapm);

	 
	nau8825_int_status_clear_all(regmap);

	 
	regmap_update_bits(regmap, NAU8825_REG_INTERRUPT_DIS_CTRL,
		NAU8825_IRQ_EJECT_DIS | NAU8825_IRQ_INSERT_DIS,
		NAU8825_IRQ_EJECT_DIS);
	regmap_update_bits(regmap, NAU8825_REG_INTERRUPT_MASK,
		NAU8825_IRQ_OUTPUT_EN | NAU8825_IRQ_EJECT_EN |
		NAU8825_IRQ_HEADSET_COMPLETE_EN | NAU8825_IRQ_INSERT_EN,
		NAU8825_IRQ_OUTPUT_EN | NAU8825_IRQ_EJECT_EN |
		NAU8825_IRQ_HEADSET_COMPLETE_EN);
	regmap_update_bits(regmap, NAU8825_REG_JACK_DET_CTRL,
		NAU8825_JACK_DET_DB_BYPASS, NAU8825_JACK_DET_DB_BYPASS);

	 
	regmap_update_bits(regmap, NAU8825_REG_ENA_CTRL,
		NAU8825_ENABLE_ADC, 0);

	 
	nau8825_configure_sysclk(nau8825, NAU8825_CLK_DIS, 0);
}

 
static void nau8825_setup_auto_irq(struct nau8825 *nau8825)
{
	struct regmap *regmap = nau8825->regmap;

	 
	regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL,
			   NAU8825_HSD_AUTO_MODE, NAU8825_HSD_AUTO_MODE);

	 
	regmap_update_bits(regmap, NAU8825_REG_INTERRUPT_MASK,
		NAU8825_IRQ_HEADSET_COMPLETE_EN | NAU8825_IRQ_EJECT_EN, 0);

	 
	nau8825_configure_sysclk(nau8825, NAU8825_CLK_INTERNAL, 0);
	 
	regmap_update_bits(regmap, NAU8825_REG_CLK_DIVIDER,
			   NAU8825_CLK_MCLK_SRC_MASK, 0);

	 
	regmap_update_bits(regmap, NAU8825_REG_ENA_CTRL,
		NAU8825_ENABLE_ADC, NAU8825_ENABLE_ADC);

	 
	regmap_update_bits(regmap, NAU8825_REG_I2S_PCM_CTRL2,
		NAU8825_I2S_MS_MASK, NAU8825_I2S_MS_MASTER);
	regmap_update_bits(regmap, NAU8825_REG_I2S_PCM_CTRL2,
		NAU8825_I2S_MS_MASK, NAU8825_I2S_MS_SLAVE);

	 
	regmap_update_bits(regmap, NAU8825_REG_JACK_DET_CTRL,
		NAU8825_JACK_DET_DB_BYPASS, 0);

	 
	regmap_write(regmap, NAU8825_REG_INTERRUPT_DIS_CTRL, 0);

	 
	nau8825_restart_jack_detection(regmap);
}

static int nau8825_button_decode(int value)
{
	int buttons = 0;

	 
	if (value & BIT(0))
		buttons |= SND_JACK_BTN_0;
	if (value & BIT(1))
		buttons |= SND_JACK_BTN_1;
	if (value & BIT(2))
		buttons |= SND_JACK_BTN_2;
	if (value & BIT(3))
		buttons |= SND_JACK_BTN_3;
	if (value & BIT(4))
		buttons |= SND_JACK_BTN_4;
	if (value & BIT(5))
		buttons |= SND_JACK_BTN_5;

	return buttons;
}

static int nau8825_high_imped_detection(struct nau8825 *nau8825)
{
	struct regmap *regmap = nau8825->regmap;
	struct snd_soc_dapm_context *dapm = nau8825->dapm;
	unsigned int adc_mg1, adc_mg2;

	 
	regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL,
			   NAU8825_SPKR_ENGND1 | NAU8825_SPKR_ENGND2 | NAU8825_SPKR_DWN1R |
			   NAU8825_SPKR_DWN1L, NAU8825_SPKR_ENGND1 | NAU8825_SPKR_ENGND2);
	regmap_update_bits(regmap, NAU8825_REG_ANALOG_CONTROL_1,
			   NAU8825_TESTDACIN_MASK, NAU8825_TESTDACIN_GND);
	regmap_write(regmap, NAU8825_REG_TRIM_SETTINGS, 0x6);
	regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
			   NAU8825_MICBIAS_LOWNOISE_MASK | NAU8825_MICBIAS_VOLTAGE_MASK,
			   NAU8825_MICBIAS_LOWNOISE_EN);
	regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
			   NAU8825_SAR_INPUT_MASK | NAU8825_SAR_TRACKING_GAIN_MASK |
			   NAU8825_SAR_HV_SEL_MASK | NAU8825_SAR_RES_SEL_MASK |
			   NAU8825_SAR_COMPARE_TIME_MASK | NAU8825_SAR_SAMPLING_TIME_MASK,
			   NAU8825_SAR_HV_SEL_VDDMIC | NAU8825_SAR_RES_SEL_70K);

	snd_soc_dapm_force_enable_pin(dapm, "MICBIAS");
	snd_soc_dapm_force_enable_pin(dapm, "SAR");
	snd_soc_dapm_sync(dapm);

	 
	regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL,
			   NAU8825_SPKR_ENGND1 | NAU8825_SPKR_ENGND2 | NAU8825_SPKR_DWN1R |
			   NAU8825_SPKR_DWN1L, NAU8825_SPKR_ENGND2);
	regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
			   NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2,
			   NAU8825_MICBIAS_JKR2);
	regmap_read(regmap, NAU8825_REG_SARDOUT_RAM_STATUS, &adc_mg1);

	 
	regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
			   NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2, 0);
	regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL,
			   NAU8825_SPKR_ENGND1 | NAU8825_SPKR_ENGND2 | NAU8825_SPKR_DWN1R |
			   NAU8825_SPKR_DWN1L, NAU8825_SPKR_ENGND1 | NAU8825_SPKR_ENGND2 |
			   NAU8825_SPKR_DWN1R | NAU8825_SPKR_DWN1L);
	regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL,
			   NAU8825_SPKR_ENGND1 | NAU8825_SPKR_ENGND2 | NAU8825_SPKR_DWN1R |
			   NAU8825_SPKR_DWN1L, NAU8825_SPKR_ENGND1);
	regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
			   NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2,
			   NAU8825_MICBIAS_JKSLV);
	regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
			   NAU8825_SAR_INPUT_MASK, NAU8825_SAR_INPUT_JKSLV);
	regmap_read(regmap, NAU8825_REG_SARDOUT_RAM_STATUS, &adc_mg2);

	 
	snd_soc_dapm_disable_pin(dapm, "SAR");
	snd_soc_dapm_disable_pin(dapm, "MICBIAS");
	snd_soc_dapm_sync(dapm);

	regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
			   NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_LOWNOISE_MASK |
			   NAU8825_MICBIAS_VOLTAGE_MASK, nau8825->micbias_voltage);
	regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL,
			   NAU8825_SPKR_ENGND1 | NAU8825_SPKR_ENGND2 | NAU8825_SPKR_DWN1R |
			   NAU8825_SPKR_DWN1L, NAU8825_SPKR_ENGND1 | NAU8825_SPKR_ENGND2 |
			   NAU8825_SPKR_DWN1R | NAU8825_SPKR_DWN1L);
	regmap_update_bits(regmap, NAU8825_REG_ANALOG_CONTROL_1,
			   NAU8825_TESTDACIN_MASK, NAU8825_TESTDACIN_GND);
	regmap_write(regmap, NAU8825_REG_TRIM_SETTINGS, 0);
	regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
			   NAU8825_SAR_TRACKING_GAIN_MASK | NAU8825_SAR_HV_SEL_MASK,
			   nau8825->sar_voltage << NAU8825_SAR_TRACKING_GAIN_SFT);
	regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
			   NAU8825_SAR_COMPARE_TIME_MASK | NAU8825_SAR_SAMPLING_TIME_MASK,
			   (nau8825->sar_compare_time << NAU8825_SAR_COMPARE_TIME_SFT) |
			   (nau8825->sar_sampling_time << NAU8825_SAR_SAMPLING_TIME_SFT));
	dev_dbg(nau8825->dev, "adc_mg1:%x, adc_mg2:%x\n", adc_mg1, adc_mg2);

	 
	if (adc_mg1 > adc_mg2) {
		dev_dbg(nau8825->dev, "OMTP (micgnd1) mic connected\n");

		 
		regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL,
				   NAU8825_SPKR_ENGND1 | NAU8825_SPKR_ENGND2,
				   NAU8825_SPKR_ENGND2);
		 
		regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
				   NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2,
				   NAU8825_MICBIAS_JKR2);
		 
		regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
				   NAU8825_SAR_INPUT_MASK,
				   NAU8825_SAR_INPUT_JKR2);
	} else if (adc_mg1 < adc_mg2) {
		dev_dbg(nau8825->dev, "CTIA (micgnd2) mic connected\n");

		 
		regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL,
				   NAU8825_SPKR_ENGND1 | NAU8825_SPKR_ENGND2,
				   NAU8825_SPKR_ENGND1);
		 
		regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
				   NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2,
				   NAU8825_MICBIAS_JKSLV);
		 
		regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
				   NAU8825_SAR_INPUT_MASK,
				   NAU8825_SAR_INPUT_JKSLV);
	} else {
		dev_err(nau8825->dev, "Jack broken.\n");
		return -EINVAL;
	}

	return 0;
}

static int nau8825_jack_insert(struct nau8825 *nau8825)
{
	struct regmap *regmap = nau8825->regmap;
	struct snd_soc_dapm_context *dapm = nau8825->dapm;
	int jack_status_reg, mic_detected;
	int type = 0;

	regmap_read(regmap, NAU8825_REG_GENERAL_STATUS, &jack_status_reg);
	mic_detected = (jack_status_reg >> 10) & 3;
	 
	if (mic_detected == 0x3)
		nau8825->high_imped = true;
	else
		nau8825->high_imped = false;

	switch (mic_detected) {
	case 0:
		 
		type = SND_JACK_HEADPHONE;
		break;
	case 1:
		dev_dbg(nau8825->dev, "OMTP (micgnd1) mic connected\n");
		type = SND_JACK_HEADSET;

		 
		regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL, 3 << 2,
			1 << 2);
		 
		regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
			NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2,
			NAU8825_MICBIAS_JKR2);
		 
		regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
			NAU8825_SAR_INPUT_MASK,
			NAU8825_SAR_INPUT_JKR2);

		snd_soc_dapm_force_enable_pin(dapm, "MICBIAS");
		snd_soc_dapm_force_enable_pin(dapm, "SAR");
		snd_soc_dapm_sync(dapm);
		break;
	case 2:
		dev_dbg(nau8825->dev, "CTIA (micgnd2) mic connected\n");
		type = SND_JACK_HEADSET;

		 
		regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL, 3 << 2,
			2 << 2);
		 
		regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
			NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2,
			NAU8825_MICBIAS_JKSLV);
		 
		regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
			NAU8825_SAR_INPUT_MASK,
			NAU8825_SAR_INPUT_JKSLV);

		snd_soc_dapm_force_enable_pin(dapm, "MICBIAS");
		snd_soc_dapm_force_enable_pin(dapm, "SAR");
		snd_soc_dapm_sync(dapm);
		break;
	case 3:
		 
		dev_warn(nau8825->dev,
			 "Detection failure. Try the manually mechanism for jack type checking.\n");
		if (!nau8825_high_imped_detection(nau8825)) {
			type = SND_JACK_HEADSET;
			snd_soc_dapm_force_enable_pin(dapm, "MICBIAS");
			snd_soc_dapm_force_enable_pin(dapm, "SAR");
			snd_soc_dapm_sync(dapm);
		} else
			type = SND_JACK_HEADPHONE;
		break;
	}

	 
	regmap_update_bits(regmap, NAU8825_REG_CLK_DIVIDER,
			   NAU8825_CLK_MCLK_SRC_MASK, 0xf);

	 
	regmap_update_bits(regmap, NAU8825_REG_HSD_CTRL, NAU8825_HSD_AUTO_MODE, 0);

	 
	return type;
}

#define NAU8825_BUTTONS (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
		SND_JACK_BTN_2 | SND_JACK_BTN_3)

static irqreturn_t nau8825_interrupt(int irq, void *data)
{
	struct nau8825 *nau8825 = (struct nau8825 *)data;
	struct regmap *regmap = nau8825->regmap;
	int active_irq, clear_irq = 0, event = 0, event_mask = 0;

	if (regmap_read(regmap, NAU8825_REG_IRQ_STATUS, &active_irq)) {
		dev_err(nau8825->dev, "failed to read irq status\n");
		return IRQ_NONE;
	}

	if ((active_irq & NAU8825_JACK_EJECTION_IRQ_MASK) ==
		NAU8825_JACK_EJECTION_DETECTED) {

		nau8825_eject_jack(nau8825);
		event_mask |= SND_JACK_HEADSET;
		clear_irq = NAU8825_JACK_EJECTION_IRQ_MASK;
	} else if (active_irq & NAU8825_KEY_SHORT_PRESS_IRQ) {
		int key_status;

		regmap_read(regmap, NAU8825_REG_INT_CLR_KEY_STATUS,
			&key_status);

		 
		nau8825->button_pressed = nau8825_button_decode(
			key_status >> 8);

		event |= nau8825->button_pressed;
		event_mask |= NAU8825_BUTTONS;
		clear_irq = NAU8825_KEY_SHORT_PRESS_IRQ;
	} else if (active_irq & NAU8825_KEY_RELEASE_IRQ) {
		event_mask = NAU8825_BUTTONS;
		clear_irq = NAU8825_KEY_RELEASE_IRQ;
	} else if (active_irq & NAU8825_HEADSET_COMPLETION_IRQ) {
		if (nau8825_is_jack_inserted(regmap)) {
			event |= nau8825_jack_insert(nau8825);
			if (nau8825->xtalk_enable && !nau8825->high_imped) {
				 
				if (!nau8825->xtalk_protect) {
					 
					int ret;
					nau8825->xtalk_protect = true;
					ret = nau8825_sema_acquire(nau8825, 0);
					if (ret)
						nau8825->xtalk_protect = false;
				}
				 
				if (nau8825->xtalk_protect) {
					nau8825->xtalk_state =
						NAU8825_XTALK_PREPARE;
					schedule_work(&nau8825->xtalk_work);
				}
			} else {
				 
				if (nau8825->xtalk_protect) {
					nau8825_sema_release(nau8825);
					nau8825->xtalk_protect = false;
				}
			}
		} else {
			dev_warn(nau8825->dev, "Headset completion IRQ fired but no headset connected\n");
			nau8825_eject_jack(nau8825);
		}

		event_mask |= SND_JACK_HEADSET;
		clear_irq = NAU8825_HEADSET_COMPLETION_IRQ;
		 
		if (nau8825->xtalk_state == NAU8825_XTALK_PREPARE) {
			nau8825->xtalk_event = event;
			nau8825->xtalk_event_mask = event_mask;
		}
	} else if (active_irq & NAU8825_IMPEDANCE_MEAS_IRQ) {
		 
		if (nau8825->xtalk_enable && nau8825->xtalk_protect)
			schedule_work(&nau8825->xtalk_work);
		clear_irq = NAU8825_IMPEDANCE_MEAS_IRQ;
	} else if ((active_irq & NAU8825_JACK_INSERTION_IRQ_MASK) ==
		NAU8825_JACK_INSERTION_DETECTED) {
		 
		if (nau8825_is_jack_inserted(regmap)) {
			 
			regmap_update_bits(regmap,
				NAU8825_REG_INTERRUPT_DIS_CTRL,
				NAU8825_IRQ_INSERT_DIS,
				NAU8825_IRQ_INSERT_DIS);
			regmap_update_bits(regmap, NAU8825_REG_INTERRUPT_MASK,
				NAU8825_IRQ_INSERT_EN, NAU8825_IRQ_INSERT_EN);
			 
			nau8825_setup_auto_irq(nau8825);
		}
	}

	if (!clear_irq)
		clear_irq = active_irq;
	 
	regmap_write(regmap, NAU8825_REG_INT_CLR_KEY_STATUS, clear_irq);

	 
	if (event_mask && nau8825->xtalk_state == NAU8825_XTALK_DONE)
		snd_soc_jack_report(nau8825->jack, event, event_mask);

	return IRQ_HANDLED;
}

static void nau8825_setup_buttons(struct nau8825 *nau8825)
{
	struct regmap *regmap = nau8825->regmap;

	regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
		NAU8825_SAR_TRACKING_GAIN_MASK,
		nau8825->sar_voltage << NAU8825_SAR_TRACKING_GAIN_SFT);
	regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
		NAU8825_SAR_COMPARE_TIME_MASK,
		nau8825->sar_compare_time << NAU8825_SAR_COMPARE_TIME_SFT);
	regmap_update_bits(regmap, NAU8825_REG_SAR_CTRL,
		NAU8825_SAR_SAMPLING_TIME_MASK,
		nau8825->sar_sampling_time << NAU8825_SAR_SAMPLING_TIME_SFT);

	regmap_update_bits(regmap, NAU8825_REG_KEYDET_CTRL,
		NAU8825_KEYDET_LEVELS_NR_MASK,
		(nau8825->sar_threshold_num - 1) << NAU8825_KEYDET_LEVELS_NR_SFT);
	regmap_update_bits(regmap, NAU8825_REG_KEYDET_CTRL,
		NAU8825_KEYDET_HYSTERESIS_MASK,
		nau8825->sar_hysteresis << NAU8825_KEYDET_HYSTERESIS_SFT);
	regmap_update_bits(regmap, NAU8825_REG_KEYDET_CTRL,
		NAU8825_KEYDET_SHORTKEY_DEBOUNCE_MASK,
		nau8825->key_debounce << NAU8825_KEYDET_SHORTKEY_DEBOUNCE_SFT);

	regmap_write(regmap, NAU8825_REG_VDET_THRESHOLD_1,
		(nau8825->sar_threshold[0] << 8) | nau8825->sar_threshold[1]);
	regmap_write(regmap, NAU8825_REG_VDET_THRESHOLD_2,
		(nau8825->sar_threshold[2] << 8) | nau8825->sar_threshold[3]);
	regmap_write(regmap, NAU8825_REG_VDET_THRESHOLD_3,
		(nau8825->sar_threshold[4] << 8) | nau8825->sar_threshold[5]);
	regmap_write(regmap, NAU8825_REG_VDET_THRESHOLD_4,
		(nau8825->sar_threshold[6] << 8) | nau8825->sar_threshold[7]);

	 
	regmap_update_bits(regmap, NAU8825_REG_INTERRUPT_MASK,
		NAU8825_IRQ_KEY_SHORT_PRESS_EN | NAU8825_IRQ_KEY_RELEASE_EN,
		0);
}

static void nau8825_init_regs(struct nau8825 *nau8825)
{
	struct regmap *regmap = nau8825->regmap;

	 
	regmap_write(regmap, NAU8825_REG_IIC_ADDR_SET, 0x0001);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_BIAS_ADJ,
		NAU8825_BIAS_VMID, NAU8825_BIAS_VMID);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_BOOST,
		NAU8825_GLOBAL_BIAS_EN, NAU8825_GLOBAL_BIAS_EN);

	 
	regmap_update_bits(regmap, NAU8825_REG_BIAS_ADJ,
		NAU8825_BIAS_VMID_SEL_MASK,
		nau8825->vref_impedance << NAU8825_BIAS_VMID_SEL_SFT);
	 
	regmap_update_bits(regmap, NAU8825_REG_BOOST,
		NAU8825_PRECHARGE_DIS | NAU8825_HP_BOOST_DIS |
		NAU8825_HP_BOOST_G_DIS | NAU8825_SHORT_SHUTDOWN_EN,
		NAU8825_PRECHARGE_DIS | NAU8825_HP_BOOST_DIS |
		NAU8825_HP_BOOST_G_DIS | NAU8825_SHORT_SHUTDOWN_EN);

	regmap_update_bits(regmap, NAU8825_REG_GPIO12_CTRL,
		NAU8825_JKDET_OUTPUT_EN,
		nau8825->jkdet_enable ? 0 : NAU8825_JKDET_OUTPUT_EN);
	regmap_update_bits(regmap, NAU8825_REG_GPIO12_CTRL,
		NAU8825_JKDET_PULL_EN,
		nau8825->jkdet_pull_enable ? 0 : NAU8825_JKDET_PULL_EN);
	regmap_update_bits(regmap, NAU8825_REG_GPIO12_CTRL,
		NAU8825_JKDET_PULL_UP,
		nau8825->jkdet_pull_up ? NAU8825_JKDET_PULL_UP : 0);
	regmap_update_bits(regmap, NAU8825_REG_JACK_DET_CTRL,
		NAU8825_JACK_POLARITY,
		 
		nau8825->jkdet_polarity ? 0 : NAU8825_JACK_POLARITY);

	regmap_update_bits(regmap, NAU8825_REG_JACK_DET_CTRL,
		NAU8825_JACK_INSERT_DEBOUNCE_MASK,
		nau8825->jack_insert_debounce << NAU8825_JACK_INSERT_DEBOUNCE_SFT);
	regmap_update_bits(regmap, NAU8825_REG_JACK_DET_CTRL,
		NAU8825_JACK_EJECT_DEBOUNCE_MASK,
		nau8825->jack_eject_debounce << NAU8825_JACK_EJECT_DEBOUNCE_SFT);

	 
	regmap_update_bits(regmap, NAU8825_REG_INTERRUPT_MASK,
		NAU8825_IRQ_PIN_PULLUP | NAU8825_IRQ_PIN_PULL_EN,
		NAU8825_IRQ_PIN_PULLUP | NAU8825_IRQ_PIN_PULL_EN);
	 
	regmap_update_bits(regmap, NAU8825_REG_INTERRUPT_MASK, 0x7ff, 0x7ff);

	regmap_update_bits(regmap, NAU8825_REG_MIC_BIAS,
		NAU8825_MICBIAS_VOLTAGE_MASK, nau8825->micbias_voltage);

	if (nau8825->sar_threshold_num)
		nau8825_setup_buttons(nau8825);

	 
	regmap_update_bits(regmap, NAU8825_REG_ADC_RATE,
		NAU8825_ADC_SYNC_DOWN_MASK | NAU8825_ADC_SINC4_EN,
		NAU8825_ADC_SYNC_DOWN_64);
	regmap_update_bits(regmap, NAU8825_REG_DAC_CTRL1,
		NAU8825_DAC_OVERSAMPLE_MASK, NAU8825_DAC_OVERSAMPLE_64);
	 
	if (nau8825->sw_id == NAU8825_SOFTWARE_ID_NAU8825)
		regmap_update_bits(regmap, NAU8825_REG_CHARGE_PUMP,
				   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL,
				   NAU8825_POWER_DOWN_DACR | NAU8825_POWER_DOWN_DACL);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_BIAS_ADJ,
		NAU8825_BIAS_TESTDAC_EN, NAU8825_BIAS_TESTDAC_EN);
	 
	regmap_update_bits(regmap, NAU8825_REG_DAC_CTRL1,
		NAU8825_DAC_CLIP_OFF, NAU8825_DAC_CLIP_OFF);

	 
	regmap_update_bits(regmap, NAU8825_REG_ANALOG_CONTROL_2,
		NAU8825_HP_NON_CLASSG_CURRENT_2xADJ |
		NAU8825_DAC_CAPACITOR_MSB | NAU8825_DAC_CAPACITOR_LSB,
		NAU8825_HP_NON_CLASSG_CURRENT_2xADJ |
		NAU8825_DAC_CAPACITOR_MSB | NAU8825_DAC_CAPACITOR_LSB);
	 
	regmap_update_bits(regmap, NAU8825_REG_CLASSG_CTRL,
		NAU8825_CLASSG_TIMER_MASK,
		0x20 << NAU8825_CLASSG_TIMER_SFT);
	 
	regmap_update_bits(regmap, NAU8825_REG_RDAC,
		NAU8825_RDAC_CLK_DELAY_MASK | NAU8825_RDAC_VREF_MASK,
		(0x2 << NAU8825_RDAC_CLK_DELAY_SFT) |
		(0x3 << NAU8825_RDAC_VREF_SFT));
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_DACL_CTRL,
		NAU8825_DACL_CH_SEL_MASK, NAU8825_DACL_CH_SEL_L);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_DACR_CTRL,
		NAU8825_DACL_CH_SEL_MASK, NAU8825_DACL_CH_SEL_R);
	 
	regmap_update_bits(regmap, NAU8825_REG_LEFT_TIME_SLOT,
		NAU8825_DIS_FS_SHORT_DET, NAU8825_DIS_FS_SHORT_DET);
	 
	regmap_update_bits(regmap, NAU8825_REG_CHARGE_PUMP,
			   NAU8825_ADCOUT_DS_MASK,
			   nau8825->adcout_ds << NAU8825_ADCOUT_DS_SFT);
}

static const struct regmap_config nau8825_regmap_config = {
	.val_bits = NAU8825_REG_DATA_LEN,
	.reg_bits = NAU8825_REG_ADDR_LEN,

	.max_register = NAU8825_REG_MAX,
	.readable_reg = nau8825_readable_reg,
	.writeable_reg = nau8825_writeable_reg,
	.volatile_reg = nau8825_volatile_reg,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = nau8825_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(nau8825_reg_defaults),
};

static int nau8825_component_probe(struct snd_soc_component *component)
{
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(component);

	nau8825->dapm = dapm;

	return 0;
}

static void nau8825_component_remove(struct snd_soc_component *component)
{
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);

	 
	nau8825_xtalk_cancel(nau8825);
}

 
static int nau8825_calc_fll_param(unsigned int fll_in, unsigned int fs,
		struct nau8825_fll *fll_param)
{
	u64 fvco, fvco_max;
	unsigned int fref, i, fvco_sel;

	 
	for (i = 0; i < ARRAY_SIZE(fll_pre_scalar); i++) {
		fref = fll_in / fll_pre_scalar[i].param;
		if (fref <= NAU_FREF_MAX)
			break;
	}
	if (i == ARRAY_SIZE(fll_pre_scalar))
		return -EINVAL;
	fll_param->clk_ref_div = fll_pre_scalar[i].val;

	 
	for (i = 0; i < ARRAY_SIZE(fll_ratio); i++) {
		if (fref >= fll_ratio[i].param)
			break;
	}
	if (i == ARRAY_SIZE(fll_ratio))
		return -EINVAL;
	fll_param->ratio = fll_ratio[i].val;

	 
	fvco_max = 0;
	fvco_sel = ARRAY_SIZE(mclk_src_scaling);
	for (i = 0; i < ARRAY_SIZE(mclk_src_scaling); i++) {
		fvco = 256ULL * fs * 2 * mclk_src_scaling[i].param;
		if (fvco > NAU_FVCO_MIN && fvco < NAU_FVCO_MAX &&
			fvco_max < fvco) {
			fvco_max = fvco;
			fvco_sel = i;
		}
	}
	if (ARRAY_SIZE(mclk_src_scaling) == fvco_sel)
		return -EINVAL;
	fll_param->mclk_src = mclk_src_scaling[fvco_sel].val;

	 
	fvco = div_u64(fvco_max << fll_param->fll_frac_num, fref * fll_param->ratio);
	fll_param->fll_int = (fvco >> fll_param->fll_frac_num) & 0x3FF;
	if (fll_param->fll_frac_num == 16)
		fll_param->fll_frac = fvco & 0xFFFF;
	else
		fll_param->fll_frac = fvco & 0xFFFFFF;
	return 0;
}

static void nau8825_fll_apply(struct nau8825 *nau8825,
		struct nau8825_fll *fll_param)
{
	regmap_update_bits(nau8825->regmap, NAU8825_REG_CLK_DIVIDER,
		NAU8825_CLK_SRC_MASK | NAU8825_CLK_MCLK_SRC_MASK,
		NAU8825_CLK_SRC_MCLK | fll_param->mclk_src);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_FLL1,
		NAU8825_FLL_RATIO_MASK | NAU8825_ICTRL_LATCH_MASK,
		fll_param->ratio | (0x6 << NAU8825_ICTRL_LATCH_SFT));
	 
	if (fll_param->fll_frac_num == 16)
		regmap_write(nau8825->regmap, NAU8825_REG_FLL2,
			     fll_param->fll_frac);
	else {
		regmap_write(nau8825->regmap, NAU8825_REG_FLL2_LOWER,
			     fll_param->fll_frac & 0xffff);
		regmap_write(nau8825->regmap, NAU8825_REG_FLL2_UPPER,
			     (fll_param->fll_frac >> 16) & 0xff);
	}
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_FLL3,
			NAU8825_FLL_INTEGER_MASK, fll_param->fll_int);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_FLL4,
			NAU8825_FLL_REF_DIV_MASK,
			fll_param->clk_ref_div << NAU8825_FLL_REF_DIV_SFT);
	 
	regmap_update_bits(nau8825->regmap, NAU8825_REG_FLL5,
		NAU8825_FLL_CLK_SW_MASK, NAU8825_FLL_CLK_SW_REF);
	 
	regmap_update_bits(nau8825->regmap,
		NAU8825_REG_FLL6, NAU8825_DCO_EN, 0);
	if (fll_param->fll_frac) {
		 
		regmap_update_bits(nau8825->regmap, NAU8825_REG_FLL5,
			NAU8825_FLL_PDB_DAC_EN | NAU8825_FLL_LOOP_FTR_EN |
			NAU8825_FLL_FTR_SW_MASK,
			NAU8825_FLL_PDB_DAC_EN | NAU8825_FLL_LOOP_FTR_EN |
			NAU8825_FLL_FTR_SW_FILTER);
		regmap_update_bits(nau8825->regmap, NAU8825_REG_FLL6,
			NAU8825_SDM_EN | NAU8825_CUTOFF500,
			NAU8825_SDM_EN | NAU8825_CUTOFF500);
	} else {
		 
		regmap_update_bits(nau8825->regmap, NAU8825_REG_FLL5,
			NAU8825_FLL_PDB_DAC_EN | NAU8825_FLL_LOOP_FTR_EN |
			NAU8825_FLL_FTR_SW_MASK, NAU8825_FLL_FTR_SW_ACCU);
		regmap_update_bits(nau8825->regmap, NAU8825_REG_FLL6,
			NAU8825_SDM_EN | NAU8825_CUTOFF500, 0);
	}
}

 
static int nau8825_set_pll(struct snd_soc_component *component, int pll_id, int source,
		unsigned int freq_in, unsigned int freq_out)
{
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);
	struct nau8825_fll fll_param;
	int ret, fs;

	if (nau8825->sw_id == NAU8825_SOFTWARE_ID_NAU8825)
		fll_param.fll_frac_num = 16;
	else
		fll_param.fll_frac_num = 24;

	fs = freq_out / 256;
	ret = nau8825_calc_fll_param(freq_in, fs, &fll_param);
	if (ret < 0) {
		dev_err(component->dev, "Unsupported input clock %d\n", freq_in);
		return ret;
	}
	dev_dbg(component->dev, "mclk_src=%x ratio=%x fll_frac=%x fll_int=%x clk_ref_div=%x\n",
		fll_param.mclk_src, fll_param.ratio, fll_param.fll_frac,
		fll_param.fll_int, fll_param.clk_ref_div);

	nau8825_fll_apply(nau8825, &fll_param);
	mdelay(2);
	regmap_update_bits(nau8825->regmap, NAU8825_REG_CLK_DIVIDER,
			NAU8825_CLK_SRC_MASK, NAU8825_CLK_SRC_VCO);
	return 0;
}

static int nau8825_mclk_prepare(struct nau8825 *nau8825, unsigned int freq)
{
	int ret;

	nau8825->mclk = devm_clk_get(nau8825->dev, "mclk");
	if (IS_ERR(nau8825->mclk)) {
		dev_info(nau8825->dev, "No 'mclk' clock found, assume MCLK is managed externally");
		return 0;
	}

	if (!nau8825->mclk_freq) {
		ret = clk_prepare_enable(nau8825->mclk);
		if (ret) {
			dev_err(nau8825->dev, "Unable to prepare codec mclk\n");
			return ret;
		}
	}

	if (nau8825->mclk_freq != freq) {
		freq = clk_round_rate(nau8825->mclk, freq);
		ret = clk_set_rate(nau8825->mclk, freq);
		if (ret) {
			dev_err(nau8825->dev, "Unable to set mclk rate\n");
			return ret;
		}
		nau8825->mclk_freq = freq;
	}

	return 0;
}

static void nau8825_configure_mclk_as_sysclk(struct regmap *regmap)
{
	regmap_update_bits(regmap, NAU8825_REG_CLK_DIVIDER,
		NAU8825_CLK_SRC_MASK, NAU8825_CLK_SRC_MCLK);
	regmap_update_bits(regmap, NAU8825_REG_FLL6,
		NAU8825_DCO_EN, 0);
	 
	regmap_update_bits(regmap, NAU8825_REG_FLL1,
		NAU8825_ICTRL_LATCH_MASK, 0);
}

static int nau8825_configure_sysclk(struct nau8825 *nau8825, int clk_id,
	unsigned int freq)
{
	struct regmap *regmap = nau8825->regmap;
	int ret;

	switch (clk_id) {
	case NAU8825_CLK_DIS:
		 
		nau8825_configure_mclk_as_sysclk(regmap);
		if (nau8825->mclk_freq) {
			clk_disable_unprepare(nau8825->mclk);
			nau8825->mclk_freq = 0;
		}

		break;
	case NAU8825_CLK_MCLK:
		 
		nau8825_sema_acquire(nau8825, 3 * HZ);
		nau8825_configure_mclk_as_sysclk(regmap);
		 
		regmap_update_bits(regmap, NAU8825_REG_CLK_DIVIDER,
			NAU8825_CLK_MCLK_SRC_MASK, 0);
		 
		nau8825_sema_release(nau8825);

		ret = nau8825_mclk_prepare(nau8825, freq);
		if (ret)
			return ret;

		break;
	case NAU8825_CLK_INTERNAL:
		if (nau8825_is_jack_inserted(nau8825->regmap)) {
			regmap_update_bits(regmap, NAU8825_REG_FLL6,
				NAU8825_DCO_EN, NAU8825_DCO_EN);
			regmap_update_bits(regmap, NAU8825_REG_CLK_DIVIDER,
				NAU8825_CLK_SRC_MASK, NAU8825_CLK_SRC_VCO);
			 
			regmap_update_bits(regmap, NAU8825_REG_CLK_DIVIDER,
				NAU8825_CLK_MCLK_SRC_MASK, 0xf);
			regmap_update_bits(regmap, NAU8825_REG_FLL1,
				NAU8825_ICTRL_LATCH_MASK |
				NAU8825_FLL_RATIO_MASK, 0x10);
			regmap_update_bits(regmap, NAU8825_REG_FLL6,
				NAU8825_SDM_EN, NAU8825_SDM_EN);
		} else {
			 
			nau8825_configure_mclk_as_sysclk(regmap);
			dev_warn(nau8825->dev, "Disable clock for power saving when no headset connected\n");
		}
		if (nau8825->mclk_freq) {
			clk_disable_unprepare(nau8825->mclk);
			nau8825->mclk_freq = 0;
		}

		break;
	case NAU8825_CLK_FLL_MCLK:
		 
		nau8825_sema_acquire(nau8825, 3 * HZ);
		 
		regmap_update_bits(regmap, NAU8825_REG_FLL3,
			NAU8825_FLL_CLK_SRC_MASK | NAU8825_GAIN_ERR_MASK,
			NAU8825_FLL_CLK_SRC_MCLK | 0);
		 
		nau8825_sema_release(nau8825);

		ret = nau8825_mclk_prepare(nau8825, freq);
		if (ret)
			return ret;

		break;
	case NAU8825_CLK_FLL_BLK:
		 
		nau8825_sema_acquire(nau8825, 3 * HZ);
		 
		regmap_update_bits(regmap, NAU8825_REG_FLL3,
			NAU8825_FLL_CLK_SRC_MASK | NAU8825_GAIN_ERR_MASK,
			NAU8825_FLL_CLK_SRC_BLK |
			(0xf << NAU8825_GAIN_ERR_SFT));
		 
		nau8825_sema_release(nau8825);

		if (nau8825->mclk_freq) {
			clk_disable_unprepare(nau8825->mclk);
			nau8825->mclk_freq = 0;
		}

		break;
	case NAU8825_CLK_FLL_FS:
		 
		nau8825_sema_acquire(nau8825, 3 * HZ);
		 
		regmap_update_bits(regmap, NAU8825_REG_FLL3,
			NAU8825_FLL_CLK_SRC_MASK | NAU8825_GAIN_ERR_MASK,
			NAU8825_FLL_CLK_SRC_FS |
			(0xf << NAU8825_GAIN_ERR_SFT));
		 
		nau8825_sema_release(nau8825);

		if (nau8825->mclk_freq) {
			clk_disable_unprepare(nau8825->mclk);
			nau8825->mclk_freq = 0;
		}

		break;
	default:
		dev_err(nau8825->dev, "Invalid clock id (%d)\n", clk_id);
		return -EINVAL;
	}

	dev_dbg(nau8825->dev, "Sysclk is %dHz and clock id is %d\n", freq,
		clk_id);
	return 0;
}

static int nau8825_set_sysclk(struct snd_soc_component *component, int clk_id,
	int source, unsigned int freq, int dir)
{
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);

	return nau8825_configure_sysclk(nau8825, clk_id, freq);
}

static int nau8825_resume_setup(struct nau8825 *nau8825)
{
	struct regmap *regmap = nau8825->regmap;

	 
	nau8825_configure_sysclk(nau8825, NAU8825_CLK_DIS, 0);

	 
	nau8825_int_status_clear_all(regmap);

	 
	regmap_update_bits(regmap, NAU8825_REG_INTERRUPT_MASK,
		NAU8825_IRQ_OUTPUT_EN | NAU8825_IRQ_HEADSET_COMPLETE_EN |
		NAU8825_IRQ_EJECT_EN | NAU8825_IRQ_INSERT_EN,
		NAU8825_IRQ_OUTPUT_EN | NAU8825_IRQ_HEADSET_COMPLETE_EN);
	regmap_update_bits(regmap, NAU8825_REG_JACK_DET_CTRL,
		NAU8825_JACK_DET_DB_BYPASS, NAU8825_JACK_DET_DB_BYPASS);
	regmap_update_bits(regmap, NAU8825_REG_INTERRUPT_DIS_CTRL,
		NAU8825_IRQ_INSERT_DIS | NAU8825_IRQ_EJECT_DIS, 0);

	return 0;
}

static int nau8825_set_bias_level(struct snd_soc_component *component,
				   enum snd_soc_bias_level level)
{
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);
	int ret;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		if (snd_soc_component_get_bias_level(component) == SND_SOC_BIAS_OFF) {
			if (nau8825->mclk_freq) {
				ret = clk_prepare_enable(nau8825->mclk);
				if (ret) {
					dev_err(nau8825->dev, "Unable to prepare component mclk\n");
					return ret;
				}
			}
			 
			nau8825_resume_setup(nau8825);
		}
		break;

	case SND_SOC_BIAS_OFF:
		 
		 
		regmap_update_bits(nau8825->regmap, NAU8825_REG_MIC_BIAS,
			NAU8825_MICBIAS_JKSLV | NAU8825_MICBIAS_JKR2, 0);
		 
		regmap_update_bits(nau8825->regmap,
			NAU8825_REG_HSD_CTRL, 0xf, 0xf);
		 
		nau8825_xtalk_cancel(nau8825);
		 
		regmap_write(nau8825->regmap,
			NAU8825_REG_INTERRUPT_DIS_CTRL, 0xffff);
		 
		regmap_update_bits(nau8825->regmap, NAU8825_REG_ENA_CTRL,
			NAU8825_ENABLE_ADC, 0);
		if (nau8825->mclk_freq)
			clk_disable_unprepare(nau8825->mclk);
		break;
	}
	return 0;
}

static int __maybe_unused nau8825_suspend(struct snd_soc_component *component)
{
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);

	disable_irq(nau8825->irq);
	snd_soc_component_force_bias_level(component, SND_SOC_BIAS_OFF);
	 
	snd_soc_dapm_disable_pin(nau8825->dapm, "SAR");
	snd_soc_dapm_disable_pin(nau8825->dapm, "MICBIAS");
	snd_soc_dapm_sync(nau8825->dapm);
	regcache_cache_only(nau8825->regmap, true);
	regcache_mark_dirty(nau8825->regmap);

	return 0;
}

static int __maybe_unused nau8825_resume(struct snd_soc_component *component)
{
	struct nau8825 *nau8825 = snd_soc_component_get_drvdata(component);
	int ret;

	regcache_cache_only(nau8825->regmap, false);
	regcache_sync(nau8825->regmap);
	nau8825->xtalk_protect = true;
	ret = nau8825_sema_acquire(nau8825, 0);
	if (ret)
		nau8825->xtalk_protect = false;
	enable_irq(nau8825->irq);

	return 0;
}

static int nau8825_set_jack(struct snd_soc_component *component,
			    struct snd_soc_jack *jack, void *data)
{
	return nau8825_enable_jack_detect(component, jack);
}

static const struct snd_soc_component_driver nau8825_component_driver = {
	.probe			= nau8825_component_probe,
	.remove			= nau8825_component_remove,
	.set_sysclk		= nau8825_set_sysclk,
	.set_pll		= nau8825_set_pll,
	.set_bias_level		= nau8825_set_bias_level,
	.suspend		= nau8825_suspend,
	.resume			= nau8825_resume,
	.controls		= nau8825_controls,
	.num_controls		= ARRAY_SIZE(nau8825_controls),
	.dapm_widgets		= nau8825_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(nau8825_dapm_widgets),
	.dapm_routes		= nau8825_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(nau8825_dapm_routes),
	.set_jack		= nau8825_set_jack,
	.suspend_bias_off	= 1,
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

static void nau8825_reset_chip(struct regmap *regmap)
{
	regmap_write(regmap, NAU8825_REG_RESET, 0x00);
	regmap_write(regmap, NAU8825_REG_RESET, 0x00);
}

static void nau8825_print_device_properties(struct nau8825 *nau8825)
{
	int i;
	struct device *dev = nau8825->dev;

	dev_dbg(dev, "jkdet-enable:         %d\n", nau8825->jkdet_enable);
	dev_dbg(dev, "jkdet-pull-enable:    %d\n", nau8825->jkdet_pull_enable);
	dev_dbg(dev, "jkdet-pull-up:        %d\n", nau8825->jkdet_pull_up);
	dev_dbg(dev, "jkdet-polarity:       %d\n", nau8825->jkdet_polarity);
	dev_dbg(dev, "micbias-voltage:      %d\n", nau8825->micbias_voltage);
	dev_dbg(dev, "vref-impedance:       %d\n", nau8825->vref_impedance);

	dev_dbg(dev, "sar-threshold-num:    %d\n", nau8825->sar_threshold_num);
	for (i = 0; i < nau8825->sar_threshold_num; i++)
		dev_dbg(dev, "sar-threshold[%d]=%d\n", i,
				nau8825->sar_threshold[i]);

	dev_dbg(dev, "sar-hysteresis:       %d\n", nau8825->sar_hysteresis);
	dev_dbg(dev, "sar-voltage:          %d\n", nau8825->sar_voltage);
	dev_dbg(dev, "sar-compare-time:     %d\n", nau8825->sar_compare_time);
	dev_dbg(dev, "sar-sampling-time:    %d\n", nau8825->sar_sampling_time);
	dev_dbg(dev, "short-key-debounce:   %d\n", nau8825->key_debounce);
	dev_dbg(dev, "jack-insert-debounce: %d\n",
			nau8825->jack_insert_debounce);
	dev_dbg(dev, "jack-eject-debounce:  %d\n",
			nau8825->jack_eject_debounce);
	dev_dbg(dev, "crosstalk-enable:     %d\n",
			nau8825->xtalk_enable);
	dev_dbg(dev, "adcout-drive-strong:  %d\n", nau8825->adcout_ds);
	dev_dbg(dev, "adc-delay-ms:         %d\n", nau8825->adc_delay);
}

static int nau8825_read_device_properties(struct device *dev,
	struct nau8825 *nau8825) {
	int ret;

	nau8825->jkdet_enable = device_property_read_bool(dev,
		"nuvoton,jkdet-enable");
	nau8825->jkdet_pull_enable = device_property_read_bool(dev,
		"nuvoton,jkdet-pull-enable");
	nau8825->jkdet_pull_up = device_property_read_bool(dev,
		"nuvoton,jkdet-pull-up");
	ret = device_property_read_u32(dev, "nuvoton,jkdet-polarity",
		&nau8825->jkdet_polarity);
	if (ret)
		nau8825->jkdet_polarity = 1;
	ret = device_property_read_u32(dev, "nuvoton,micbias-voltage",
		&nau8825->micbias_voltage);
	if (ret)
		nau8825->micbias_voltage = 6;
	ret = device_property_read_u32(dev, "nuvoton,vref-impedance",
		&nau8825->vref_impedance);
	if (ret)
		nau8825->vref_impedance = 2;
	ret = device_property_read_u32(dev, "nuvoton,sar-threshold-num",
		&nau8825->sar_threshold_num);
	if (ret)
		nau8825->sar_threshold_num = 4;
	ret = device_property_read_u32_array(dev, "nuvoton,sar-threshold",
		nau8825->sar_threshold, nau8825->sar_threshold_num);
	if (ret) {
		nau8825->sar_threshold[0] = 0x08;
		nau8825->sar_threshold[1] = 0x12;
		nau8825->sar_threshold[2] = 0x26;
		nau8825->sar_threshold[3] = 0x73;
	}
	ret = device_property_read_u32(dev, "nuvoton,sar-hysteresis",
		&nau8825->sar_hysteresis);
	if (ret)
		nau8825->sar_hysteresis = 0;
	ret = device_property_read_u32(dev, "nuvoton,sar-voltage",
		&nau8825->sar_voltage);
	if (ret)
		nau8825->sar_voltage = 6;
	ret = device_property_read_u32(dev, "nuvoton,sar-compare-time",
		&nau8825->sar_compare_time);
	if (ret)
		nau8825->sar_compare_time = 1;
	ret = device_property_read_u32(dev, "nuvoton,sar-sampling-time",
		&nau8825->sar_sampling_time);
	if (ret)
		nau8825->sar_sampling_time = 1;
	ret = device_property_read_u32(dev, "nuvoton,short-key-debounce",
		&nau8825->key_debounce);
	if (ret)
		nau8825->key_debounce = 3;
	ret = device_property_read_u32(dev, "nuvoton,jack-insert-debounce",
		&nau8825->jack_insert_debounce);
	if (ret)
		nau8825->jack_insert_debounce = 7;
	ret = device_property_read_u32(dev, "nuvoton,jack-eject-debounce",
		&nau8825->jack_eject_debounce);
	if (ret)
		nau8825->jack_eject_debounce = 0;
	nau8825->xtalk_enable = device_property_read_bool(dev,
		"nuvoton,crosstalk-enable");
	nau8825->adcout_ds = device_property_read_bool(dev, "nuvoton,adcout-drive-strong");
	ret = device_property_read_u32(dev, "nuvoton,adc-delay-ms", &nau8825->adc_delay);
	if (ret)
		nau8825->adc_delay = 125;
	if (nau8825->adc_delay < 125 || nau8825->adc_delay > 500)
		dev_warn(dev, "Please set the suitable delay time!\n");

	nau8825->mclk = devm_clk_get(dev, "mclk");
	if (PTR_ERR(nau8825->mclk) == -EPROBE_DEFER) {
		return -EPROBE_DEFER;
	} else if (PTR_ERR(nau8825->mclk) == -ENOENT) {
		 
		nau8825->mclk = NULL;
		dev_info(dev, "No 'mclk' clock found, assume MCLK is managed externally");
	} else if (IS_ERR(nau8825->mclk)) {
		return -EINVAL;
	}

	return 0;
}

static int nau8825_setup_irq(struct nau8825 *nau8825)
{
	int ret;

	ret = devm_request_threaded_irq(nau8825->dev, nau8825->irq, NULL,
		nau8825_interrupt, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
		"nau8825", nau8825);

	if (ret) {
		dev_err(nau8825->dev, "Cannot request irq %d (%d)\n",
			nau8825->irq, ret);
		return ret;
	}

	return 0;
}

static int nau8825_i2c_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct nau8825 *nau8825 = dev_get_platdata(&i2c->dev);
	int ret, value;

	if (!nau8825) {
		nau8825 = devm_kzalloc(dev, sizeof(*nau8825), GFP_KERNEL);
		if (!nau8825)
			return -ENOMEM;
		ret = nau8825_read_device_properties(dev, nau8825);
		if (ret)
			return ret;
	}

	i2c_set_clientdata(i2c, nau8825);

	nau8825->regmap = devm_regmap_init_i2c(i2c, &nau8825_regmap_config);
	if (IS_ERR(nau8825->regmap))
		return PTR_ERR(nau8825->regmap);
	nau8825->dev = dev;
	nau8825->irq = i2c->irq;
	 
	nau8825->xtalk_state = NAU8825_XTALK_DONE;
	nau8825->xtalk_protect = false;
	nau8825->xtalk_baktab_initialized = false;
	sema_init(&nau8825->xtalk_sem, 1);
	INIT_WORK(&nau8825->xtalk_work, nau8825_xtalk_work);

	nau8825_print_device_properties(nau8825);

	nau8825_reset_chip(nau8825->regmap);
	ret = regmap_read(nau8825->regmap, NAU8825_REG_I2C_DEVICE_ID, &value);
	if (ret < 0) {
		dev_err(dev, "Failed to read device id from the NAU8825: %d\n",
			ret);
		return ret;
	}
	nau8825->sw_id = value & NAU8825_SOFTWARE_ID_MASK;
	switch (nau8825->sw_id) {
	case NAU8825_SOFTWARE_ID_NAU8825:
		break;
	case NAU8825_SOFTWARE_ID_NAU8825C:
		ret = regmap_register_patch(nau8825->regmap, nau8825_regmap_patch,
					    ARRAY_SIZE(nau8825_regmap_patch));
		if (ret) {
			dev_err(dev, "Failed to register Rev C patch: %d\n", ret);
			return ret;
		}
		break;
	default:
		dev_err(dev, "Not a NAU8825 chip\n");
		return -ENODEV;
	}

	nau8825_init_regs(nau8825);

	if (i2c->irq)
		nau8825_setup_irq(nau8825);

	return devm_snd_soc_register_component(&i2c->dev,
		&nau8825_component_driver,
		&nau8825_dai, 1);
}

static void nau8825_i2c_remove(struct i2c_client *client)
{}

static const struct i2c_device_id nau8825_i2c_ids[] = {
	{ "nau8825", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, nau8825_i2c_ids);

#ifdef CONFIG_OF
static const struct of_device_id nau8825_of_ids[] = {
	{ .compatible = "nuvoton,nau8825", },
	{}
};
MODULE_DEVICE_TABLE(of, nau8825_of_ids);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id nau8825_acpi_match[] = {
	{ "10508825", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, nau8825_acpi_match);
#endif

static struct i2c_driver nau8825_driver = {
	.driver = {
		.name = "nau8825",
		.of_match_table = of_match_ptr(nau8825_of_ids),
		.acpi_match_table = ACPI_PTR(nau8825_acpi_match),
	},
	.probe = nau8825_i2c_probe,
	.remove = nau8825_i2c_remove,
	.id_table = nau8825_i2c_ids,
};
module_i2c_driver(nau8825_driver);

MODULE_DESCRIPTION("ASoC nau8825 driver");
MODULE_AUTHOR("Anatol Pomozov <anatol@chromium.org>");
MODULE_LICENSE("GPL");
