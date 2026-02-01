


























#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/dma/imx-dma.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

#include "fsl_ssi.h"
#include "imx-pcm.h"

 
#define RX 0
#define TX 1

 
#ifdef __BIG_ENDIAN
#define FSLSSI_I2S_FORMATS \
	(SNDRV_PCM_FMTBIT_S8 | \
	 SNDRV_PCM_FMTBIT_S16_BE | \
	 SNDRV_PCM_FMTBIT_S18_3BE | \
	 SNDRV_PCM_FMTBIT_S20_3BE | \
	 SNDRV_PCM_FMTBIT_S24_3BE | \
	 SNDRV_PCM_FMTBIT_S24_BE)
#else
#define FSLSSI_I2S_FORMATS \
	(SNDRV_PCM_FMTBIT_S8 | \
	 SNDRV_PCM_FMTBIT_S16_LE | \
	 SNDRV_PCM_FMTBIT_S18_3LE | \
	 SNDRV_PCM_FMTBIT_S20_3LE | \
	 SNDRV_PCM_FMTBIT_S24_3LE | \
	 SNDRV_PCM_FMTBIT_S24_LE)
#endif

 
#define FSLSSI_AC97_DAIFMT \
	(SND_SOC_DAIFMT_AC97 | \
	 SND_SOC_DAIFMT_BC_FP | \
	 SND_SOC_DAIFMT_NB_NF)

#define FSLSSI_SIER_DBG_RX_FLAGS \
	(SSI_SIER_RFF0_EN | \
	 SSI_SIER_RLS_EN | \
	 SSI_SIER_RFS_EN | \
	 SSI_SIER_ROE0_EN | \
	 SSI_SIER_RFRC_EN)
#define FSLSSI_SIER_DBG_TX_FLAGS \
	(SSI_SIER_TFE0_EN | \
	 SSI_SIER_TLS_EN | \
	 SSI_SIER_TFS_EN | \
	 SSI_SIER_TUE0_EN | \
	 SSI_SIER_TFRC_EN)

enum fsl_ssi_type {
	FSL_SSI_MCP8610,
	FSL_SSI_MX21,
	FSL_SSI_MX35,
	FSL_SSI_MX51,
};

struct fsl_ssi_regvals {
	u32 sier;
	u32 srcr;
	u32 stcr;
	u32 scr;
};

static bool fsl_ssi_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case REG_SSI_SACCEN:
	case REG_SSI_SACCDIS:
		return false;
	default:
		return true;
	}
}

static bool fsl_ssi_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case REG_SSI_STX0:
	case REG_SSI_STX1:
	case REG_SSI_SRX0:
	case REG_SSI_SRX1:
	case REG_SSI_SISR:
	case REG_SSI_SFCSR:
	case REG_SSI_SACNT:
	case REG_SSI_SACADD:
	case REG_SSI_SACDAT:
	case REG_SSI_SATAG:
	case REG_SSI_SACCST:
	case REG_SSI_SOR:
		return true;
	default:
		return false;
	}
}

static bool fsl_ssi_precious_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case REG_SSI_SRX0:
	case REG_SSI_SRX1:
	case REG_SSI_SISR:
	case REG_SSI_SACADD:
	case REG_SSI_SACDAT:
	case REG_SSI_SATAG:
		return true;
	default:
		return false;
	}
}

static bool fsl_ssi_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case REG_SSI_SRX0:
	case REG_SSI_SRX1:
	case REG_SSI_SACCST:
		return false;
	default:
		return true;
	}
}

static const struct regmap_config fsl_ssi_regconfig = {
	.max_register = REG_SSI_SACCDIS,
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.num_reg_defaults_raw = REG_SSI_SACCDIS / sizeof(uint32_t) + 1,
	.readable_reg = fsl_ssi_readable_reg,
	.volatile_reg = fsl_ssi_volatile_reg,
	.precious_reg = fsl_ssi_precious_reg,
	.writeable_reg = fsl_ssi_writeable_reg,
	.cache_type = REGCACHE_FLAT,
};

struct fsl_ssi_soc_data {
	bool imx;
	bool imx21regs;  
	bool offline_config;
	u32 sisr_write_mask;
};

 
struct fsl_ssi {
	struct regmap *regs;
	int irq;
	struct snd_soc_dai_driver cpu_dai_drv;

	unsigned int dai_fmt;
	u8 streams;
	u8 i2s_net;
	bool synchronous;
	bool use_dma;
	bool use_dual_fifo;
	bool use_dyna_fifo;
	bool has_ipg_clk_name;
	unsigned int fifo_depth;
	unsigned int slot_width;
	unsigned int slots;
	struct fsl_ssi_regvals regvals[2];

	struct clk *clk;
	struct clk *baudclk;
	unsigned int baudclk_streams;

	u32 regcache_sfcsr;
	u32 regcache_sacnt;

	struct snd_dmaengine_dai_dma_data dma_params_tx;
	struct snd_dmaengine_dai_dma_data dma_params_rx;
	dma_addr_t ssi_phys;

	struct imx_pcm_fiq_params fiq_params;

	struct platform_device *card_pdev;
	char card_name[32];
	u32 card_idx;

	struct fsl_ssi_dbg dbg_stats;

	const struct fsl_ssi_soc_data *soc;
	struct device *dev;

	u32 fifo_watermark;
	u32 dma_maxburst;

	struct mutex ac97_reg_lock;
	struct sdma_peripheral_config audio_config[2];
};

 

static struct fsl_ssi_soc_data fsl_ssi_mpc8610 = {
	.imx = false,
	.offline_config = true,
	.sisr_write_mask = SSI_SISR_RFRC | SSI_SISR_TFRC |
			   SSI_SISR_ROE0 | SSI_SISR_ROE1 |
			   SSI_SISR_TUE0 | SSI_SISR_TUE1,
};

static struct fsl_ssi_soc_data fsl_ssi_imx21 = {
	.imx = true,
	.imx21regs = true,
	.offline_config = true,
	.sisr_write_mask = 0,
};

static struct fsl_ssi_soc_data fsl_ssi_imx35 = {
	.imx = true,
	.offline_config = true,
	.sisr_write_mask = SSI_SISR_RFRC | SSI_SISR_TFRC |
			   SSI_SISR_ROE0 | SSI_SISR_ROE1 |
			   SSI_SISR_TUE0 | SSI_SISR_TUE1,
};

static struct fsl_ssi_soc_data fsl_ssi_imx51 = {
	.imx = true,
	.offline_config = false,
	.sisr_write_mask = SSI_SISR_ROE0 | SSI_SISR_ROE1 |
			   SSI_SISR_TUE0 | SSI_SISR_TUE1,
};

static const struct of_device_id fsl_ssi_ids[] = {
	{ .compatible = "fsl,mpc8610-ssi", .data = &fsl_ssi_mpc8610 },
	{ .compatible = "fsl,imx51-ssi", .data = &fsl_ssi_imx51 },
	{ .compatible = "fsl,imx35-ssi", .data = &fsl_ssi_imx35 },
	{ .compatible = "fsl,imx21-ssi", .data = &fsl_ssi_imx21 },
	{}
};
MODULE_DEVICE_TABLE(of, fsl_ssi_ids);

static bool fsl_ssi_is_ac97(struct fsl_ssi *ssi)
{
	return (ssi->dai_fmt & SND_SOC_DAIFMT_FORMAT_MASK) ==
		SND_SOC_DAIFMT_AC97;
}

static bool fsl_ssi_is_i2s_clock_provider(struct fsl_ssi *ssi)
{
	return (ssi->dai_fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) ==
		SND_SOC_DAIFMT_BP_FP;
}

static bool fsl_ssi_is_i2s_bc_fp(struct fsl_ssi *ssi)
{
	return (ssi->dai_fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) ==
		SND_SOC_DAIFMT_BC_FP;
}

 
static irqreturn_t fsl_ssi_isr(int irq, void *dev_id)
{
	struct fsl_ssi *ssi = dev_id;
	struct regmap *regs = ssi->regs;
	u32 sisr, sisr2;

	regmap_read(regs, REG_SSI_SISR, &sisr);

	sisr2 = sisr & ssi->soc->sisr_write_mask;
	 
	if (sisr2)
		regmap_write(regs, REG_SSI_SISR, sisr2);

	fsl_ssi_dbg_isr(&ssi->dbg_stats, sisr);

	return IRQ_HANDLED;
}

 
static void fsl_ssi_config_enable(struct fsl_ssi *ssi, bool tx)
{
	struct fsl_ssi_regvals *vals = ssi->regvals;
	int dir = tx ? TX : RX;
	u32 sier, srcr, stcr;

	 
	regmap_update_bits(ssi->regs, REG_SSI_SOR,
			   SSI_SOR_xX_CLR(tx), SSI_SOR_xX_CLR(tx));

	 
	if (ssi->soc->offline_config && ssi->streams)
		goto enable_scr;

	if (ssi->soc->offline_config) {
		 
		srcr = vals[RX].srcr | vals[TX].srcr;
		stcr = vals[RX].stcr | vals[TX].stcr;
		sier = vals[RX].sier | vals[TX].sier;
	} else {
		 
		srcr = vals[dir].srcr;
		stcr = vals[dir].stcr;
		sier = vals[dir].sier;
	}

	 
	regmap_update_bits(ssi->regs, REG_SSI_SRCR, srcr, srcr);
	regmap_update_bits(ssi->regs, REG_SSI_STCR, stcr, stcr);
	regmap_update_bits(ssi->regs, REG_SSI_SIER, sier, sier);

enable_scr:
	 
	if (ssi->use_dma && tx) {
		int try = 100;
		u32 sfcsr;

		 
		regmap_update_bits(ssi->regs, REG_SSI_SCR,
				   SSI_SCR_SSIEN, SSI_SCR_SSIEN);

		 
		do {
			regmap_read(ssi->regs, REG_SSI_SFCSR, &sfcsr);
			if (SSI_SFCSR_TFCNT0(sfcsr))
				break;
		} while (--try);

		 
		if (!SSI_SFCSR_TFCNT0(sfcsr))
			dev_warn(ssi->dev, "Timeout waiting TX FIFO filling\n");
	}
	 
	regmap_update_bits(ssi->regs, REG_SSI_SCR,
			   vals[dir].scr, vals[dir].scr);

	 
	ssi->streams |= BIT(dir);
}

 
#define _ssi_xor_shared_bits(vals, avals, aactive) \
	((vals) ^ ((avals) * (aactive)))

#define ssi_excl_shared_bits(vals, avals, aactive) \
	((vals) & _ssi_xor_shared_bits(vals, avals, aactive))

 
static void fsl_ssi_config_disable(struct fsl_ssi *ssi, bool tx)
{
	struct fsl_ssi_regvals *vals, *avals;
	u32 sier, srcr, stcr, scr;
	int adir = tx ? RX : TX;
	int dir = tx ? TX : RX;
	bool aactive;

	 
	aactive = ssi->streams & BIT(adir);

	vals = &ssi->regvals[dir];

	 
	avals = &ssi->regvals[adir];

	 
	scr = ssi_excl_shared_bits(vals->scr, avals->scr, aactive);

	 
	regmap_update_bits(ssi->regs, REG_SSI_SCR, scr, 0);

	 
	ssi->streams &= ~BIT(dir);

	 
	if (ssi->soc->offline_config && aactive)
		goto fifo_clear;

	if (ssi->soc->offline_config) {
		 
		srcr = vals->srcr | avals->srcr;
		stcr = vals->stcr | avals->stcr;
		sier = vals->sier | avals->sier;
	} else {
		 
		sier = ssi_excl_shared_bits(vals->sier, avals->sier, aactive);
		srcr = ssi_excl_shared_bits(vals->srcr, avals->srcr, aactive);
		stcr = ssi_excl_shared_bits(vals->stcr, avals->stcr, aactive);
	}

	 
	regmap_update_bits(ssi->regs, REG_SSI_SRCR, srcr, 0);
	regmap_update_bits(ssi->regs, REG_SSI_STCR, stcr, 0);
	regmap_update_bits(ssi->regs, REG_SSI_SIER, sier, 0);

fifo_clear:
	 
	regmap_update_bits(ssi->regs, REG_SSI_SOR,
			   SSI_SOR_xX_CLR(tx), SSI_SOR_xX_CLR(tx));
}

static void fsl_ssi_tx_ac97_saccst_setup(struct fsl_ssi *ssi)
{
	struct regmap *regs = ssi->regs;

	 
	if (!ssi->soc->imx21regs) {
		 
		regmap_write(regs, REG_SSI_SACCDIS, 0xff);
		 
		regmap_write(regs, REG_SSI_SACCEN, 0x300);
	}
}

 
static void fsl_ssi_setup_regvals(struct fsl_ssi *ssi)
{
	struct fsl_ssi_regvals *vals = ssi->regvals;

	vals[RX].sier = SSI_SIER_RFF0_EN | FSLSSI_SIER_DBG_RX_FLAGS;
	vals[RX].srcr = SSI_SRCR_RFEN0;
	vals[RX].scr = SSI_SCR_SSIEN | SSI_SCR_RE;
	vals[TX].sier = SSI_SIER_TFE0_EN | FSLSSI_SIER_DBG_TX_FLAGS;
	vals[TX].stcr = SSI_STCR_TFEN0;
	vals[TX].scr = SSI_SCR_SSIEN | SSI_SCR_TE;

	 
	if (fsl_ssi_is_ac97(ssi))
		vals[RX].scr = vals[TX].scr = 0;

	if (ssi->use_dual_fifo) {
		vals[RX].srcr |= SSI_SRCR_RFEN1;
		vals[TX].stcr |= SSI_STCR_TFEN1;
	}

	if (ssi->use_dma) {
		vals[RX].sier |= SSI_SIER_RDMAE;
		vals[TX].sier |= SSI_SIER_TDMAE;
	} else {
		vals[RX].sier |= SSI_SIER_RIE;
		vals[TX].sier |= SSI_SIER_TIE;
	}
}

static void fsl_ssi_setup_ac97(struct fsl_ssi *ssi)
{
	struct regmap *regs = ssi->regs;

	 
	regmap_write(regs, REG_SSI_STCCR, SSI_SxCCR_WL(17) | SSI_SxCCR_DC(13));
	regmap_write(regs, REG_SSI_SRCCR, SSI_SxCCR_WL(17) | SSI_SxCCR_DC(13));

	 
	regmap_write(regs, REG_SSI_SACNT, SSI_SACNT_AC97EN | SSI_SACNT_FV);

	 
	regmap_update_bits(regs, REG_SSI_SCR,
			   SSI_SCR_SSIEN | SSI_SCR_TE | SSI_SCR_RE,
			   SSI_SCR_SSIEN | SSI_SCR_TE | SSI_SCR_RE);

	regmap_write(regs, REG_SSI_SOR, SSI_SOR_WAIT(3));
}

static int fsl_ssi_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct fsl_ssi *ssi = snd_soc_dai_get_drvdata(asoc_rtd_to_cpu(rtd, 0));
	int ret;

	ret = clk_prepare_enable(ssi->clk);
	if (ret)
		return ret;

	 
	if (ssi->use_dual_fifo || ssi->use_dyna_fifo)
		snd_pcm_hw_constraint_step(substream->runtime, 0,
					   SNDRV_PCM_HW_PARAM_PERIOD_SIZE, 2);

	return 0;
}

static void fsl_ssi_shutdown(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct fsl_ssi *ssi = snd_soc_dai_get_drvdata(asoc_rtd_to_cpu(rtd, 0));

	clk_disable_unprepare(ssi->clk);
}

 
static int fsl_ssi_set_bclk(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai,
			    struct snd_pcm_hw_params *hw_params)
{
	bool tx2, tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct fsl_ssi *ssi = snd_soc_dai_get_drvdata(dai);
	struct regmap *regs = ssi->regs;
	u32 pm = 999, div2, psr, stccr, mask, afreq, factor, i;
	unsigned long clkrate, baudrate, tmprate;
	unsigned int channels = params_channels(hw_params);
	unsigned int slot_width = params_width(hw_params);
	unsigned int slots = 2;
	u64 sub, savesub = 100000;
	unsigned int freq;
	bool baudclk_is_used;
	int ret;

	 
	if (ssi->slots)
		slots = ssi->slots;
	if (ssi->slot_width)
		slot_width = ssi->slot_width;

	 
	if (channels == 2 &&
	    (ssi->i2s_net & SSI_SCR_I2S_MODE_MASK) == SSI_SCR_I2S_MODE_MASTER)
		slot_width = 32;

	 
	freq = slots * slot_width * params_rate(hw_params);

	 
	if (IS_ERR(ssi->baudclk))
		return -EINVAL;

	 
	if (freq * 5 > clk_get_rate(ssi->clk)) {
		dev_err(dai->dev, "bitclk > ipgclk / 5\n");
		return -EINVAL;
	}

	baudclk_is_used = ssi->baudclk_streams & ~(BIT(substream->stream));

	 
	psr = 0;
	div2 = 0;

	factor = (div2 + 1) * (7 * psr + 1) * 2;

	for (i = 0; i < 255; i++) {
		tmprate = freq * factor * (i + 1);

		if (baudclk_is_used)
			clkrate = clk_get_rate(ssi->baudclk);
		else
			clkrate = clk_round_rate(ssi->baudclk, tmprate);

		clkrate /= factor;
		afreq = clkrate / (i + 1);

		if (freq == afreq)
			sub = 0;
		else if (freq / afreq == 1)
			sub = freq - afreq;
		else if (afreq / freq == 1)
			sub = afreq - freq;
		else
			continue;

		 
		sub *= 100000;
		do_div(sub, freq);

		if (sub < savesub && !(i == 0)) {
			baudrate = tmprate;
			savesub = sub;
			pm = i;
		}

		 
		if (savesub == 0)
			break;
	}

	 
	if (pm == 999) {
		dev_err(dai->dev, "failed to handle the required sysclk\n");
		return -EINVAL;
	}

	stccr = SSI_SxCCR_PM(pm + 1);
	mask = SSI_SxCCR_PM_MASK | SSI_SxCCR_DIV2 | SSI_SxCCR_PSR;

	 
	tx2 = tx || ssi->synchronous;
	regmap_update_bits(regs, REG_SSI_SxCCR(tx2), mask, stccr);

	if (!baudclk_is_used) {
		ret = clk_set_rate(ssi->baudclk, baudrate);
		if (ret) {
			dev_err(dai->dev, "failed to set baudclk rate\n");
			return -EINVAL;
		}
	}

	return 0;
}

 
static int fsl_ssi_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params,
			     struct snd_soc_dai *dai)
{
	bool tx2, tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct fsl_ssi *ssi = snd_soc_dai_get_drvdata(dai);
	struct fsl_ssi_regvals *vals = ssi->regvals;
	struct regmap *regs = ssi->regs;
	unsigned int channels = params_channels(hw_params);
	unsigned int sample_size = params_width(hw_params);
	u32 wl = SSI_SxCCR_WL(sample_size);
	int ret;

	if (fsl_ssi_is_i2s_clock_provider(ssi)) {
		ret = fsl_ssi_set_bclk(substream, dai, hw_params);
		if (ret)
			return ret;

		 
		if (!(ssi->baudclk_streams & BIT(substream->stream))) {
			ret = clk_prepare_enable(ssi->baudclk);
			if (ret)
				return ret;

			ssi->baudclk_streams |= BIT(substream->stream);
		}
	}

	 
	if (ssi->streams && ssi->synchronous)
		return 0;

	if (!fsl_ssi_is_ac97(ssi)) {
		 
		u8 i2s_net = ssi->i2s_net;

		 
		if (fsl_ssi_is_i2s_bc_fp(ssi) && sample_size == 16)
			i2s_net = SSI_SCR_I2S_MODE_NORMAL | SSI_SCR_NET;

		 
		if (channels == 1)
			i2s_net = SSI_SCR_I2S_MODE_NORMAL;

		regmap_update_bits(regs, REG_SSI_SCR,
				   SSI_SCR_I2S_NET_MASK, i2s_net);
	}

	 
	tx2 = tx || ssi->synchronous;
	regmap_update_bits(regs, REG_SSI_SxCCR(tx2), SSI_SxCCR_WL_MASK, wl);

	if (ssi->use_dyna_fifo) {
		if (channels == 1) {
			ssi->audio_config[0].n_fifos_dst = 1;
			ssi->audio_config[1].n_fifos_src = 1;
			vals[RX].srcr &= ~SSI_SRCR_RFEN1;
			vals[TX].stcr &= ~SSI_STCR_TFEN1;
			vals[RX].scr  &= ~SSI_SCR_TCH_EN;
			vals[TX].scr  &= ~SSI_SCR_TCH_EN;
		} else {
			ssi->audio_config[0].n_fifos_dst = 2;
			ssi->audio_config[1].n_fifos_src = 2;
			vals[RX].srcr |= SSI_SRCR_RFEN1;
			vals[TX].stcr |= SSI_STCR_TFEN1;
			vals[RX].scr  |= SSI_SCR_TCH_EN;
			vals[TX].scr  |= SSI_SCR_TCH_EN;
		}
		ssi->dma_params_tx.peripheral_config = &ssi->audio_config[0];
		ssi->dma_params_tx.peripheral_size = sizeof(ssi->audio_config[0]);
		ssi->dma_params_rx.peripheral_config = &ssi->audio_config[1];
		ssi->dma_params_rx.peripheral_size = sizeof(ssi->audio_config[1]);
	}

	return 0;
}

static int fsl_ssi_hw_free(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct fsl_ssi *ssi = snd_soc_dai_get_drvdata(asoc_rtd_to_cpu(rtd, 0));

	if (fsl_ssi_is_i2s_clock_provider(ssi) &&
	    ssi->baudclk_streams & BIT(substream->stream)) {
		clk_disable_unprepare(ssi->baudclk);
		ssi->baudclk_streams &= ~BIT(substream->stream);
	}

	return 0;
}

static int _fsl_ssi_set_dai_fmt(struct fsl_ssi *ssi, unsigned int fmt)
{
	u32 strcr = 0, scr = 0, stcr, srcr, mask;
	unsigned int slots;

	ssi->dai_fmt = fmt;

	 
	scr |= SSI_SCR_SYNC_TX_FS;

	 
	strcr |= SSI_STCR_TXBIT0;

	 
	ssi->i2s_net = SSI_SCR_NET;
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		switch (fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
		case SND_SOC_DAIFMT_BP_FP:
			if (IS_ERR(ssi->baudclk)) {
				dev_err(ssi->dev,
					"missing baudclk for master mode\n");
				return -EINVAL;
			}
			fallthrough;
		case SND_SOC_DAIFMT_BC_FP:
			ssi->i2s_net |= SSI_SCR_I2S_MODE_MASTER;
			break;
		case SND_SOC_DAIFMT_BC_FC:
			ssi->i2s_net |= SSI_SCR_I2S_MODE_SLAVE;
			break;
		default:
			return -EINVAL;
		}

		slots = ssi->slots ? : 2;
		regmap_update_bits(ssi->regs, REG_SSI_STCCR,
				   SSI_SxCCR_DC_MASK, SSI_SxCCR_DC(slots));
		regmap_update_bits(ssi->regs, REG_SSI_SRCCR,
				   SSI_SxCCR_DC_MASK, SSI_SxCCR_DC(slots));

		 
		strcr |= SSI_STCR_TFSI | SSI_STCR_TSCKP | SSI_STCR_TEFS;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		 
		strcr |= SSI_STCR_TSCKP;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		 
		strcr |= SSI_STCR_TFSL | SSI_STCR_TSCKP | SSI_STCR_TEFS;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		 
		strcr |= SSI_STCR_TFSL | SSI_STCR_TSCKP;
		break;
	case SND_SOC_DAIFMT_AC97:
		 
		strcr |= SSI_STCR_TEFS;
		break;
	default:
		return -EINVAL;
	}

	scr |= ssi->i2s_net;

	 
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		 
		break;
	case SND_SOC_DAIFMT_IB_NF:
		 
		strcr ^= SSI_STCR_TSCKP;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		 
		strcr ^= SSI_STCR_TFSI;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		 
		strcr ^= SSI_STCR_TSCKP;
		strcr ^= SSI_STCR_TFSI;
		break;
	default:
		return -EINVAL;
	}

	 
	switch (fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
	case SND_SOC_DAIFMT_BP_FP:
		 
		strcr |= SSI_STCR_TFDIR | SSI_STCR_TXDIR;
		scr |= SSI_SCR_SYS_CLK_EN;
		break;
	case SND_SOC_DAIFMT_BC_FC:
		 
		break;
	case SND_SOC_DAIFMT_BC_FP:
		 
		strcr |= SSI_STCR_TFDIR;
		break;
	default:
		return -EINVAL;
	}

	stcr = strcr;
	srcr = strcr;

	 
	if (ssi->synchronous || fsl_ssi_is_ac97(ssi)) {
		srcr &= ~SSI_SRCR_RXDIR;
		scr |= SSI_SCR_SYN;
	}

	mask = SSI_STCR_TFDIR | SSI_STCR_TXDIR | SSI_STCR_TSCKP |
	       SSI_STCR_TFSL | SSI_STCR_TFSI | SSI_STCR_TEFS | SSI_STCR_TXBIT0;

	regmap_update_bits(ssi->regs, REG_SSI_STCR, mask, stcr);
	regmap_update_bits(ssi->regs, REG_SSI_SRCR, mask, srcr);

	mask = SSI_SCR_SYNC_TX_FS | SSI_SCR_I2S_MODE_MASK |
	       SSI_SCR_SYS_CLK_EN | SSI_SCR_SYN;
	regmap_update_bits(ssi->regs, REG_SSI_SCR, mask, scr);

	return 0;
}

 
static int fsl_ssi_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct fsl_ssi *ssi = snd_soc_dai_get_drvdata(dai);

	 
	if (fsl_ssi_is_ac97(ssi))
		return 0;

	return _fsl_ssi_set_dai_fmt(ssi, fmt);
}

 
static int fsl_ssi_set_dai_tdm_slot(struct snd_soc_dai *dai, u32 tx_mask,
				    u32 rx_mask, int slots, int slot_width)
{
	struct fsl_ssi *ssi = snd_soc_dai_get_drvdata(dai);
	struct regmap *regs = ssi->regs;
	u32 val;

	 
	if (slot_width & 1 || slot_width < 8 || slot_width > 24) {
		dev_err(dai->dev, "invalid slot width: %d\n", slot_width);
		return -EINVAL;
	}

	 
	if (ssi->i2s_net && slots < 2) {
		dev_err(dai->dev, "slot number should be >= 2 in I2S or NET\n");
		return -EINVAL;
	}

	regmap_update_bits(regs, REG_SSI_STCCR,
			   SSI_SxCCR_DC_MASK, SSI_SxCCR_DC(slots));
	regmap_update_bits(regs, REG_SSI_SRCCR,
			   SSI_SxCCR_DC_MASK, SSI_SxCCR_DC(slots));

	 
	regmap_read(regs, REG_SSI_SCR, &val);
	 
	regmap_update_bits(regs, REG_SSI_SCR, SSI_SCR_SSIEN, SSI_SCR_SSIEN);

	regmap_write(regs, REG_SSI_STMSK, ~tx_mask);
	regmap_write(regs, REG_SSI_SRMSK, ~rx_mask);

	 
	regmap_update_bits(regs, REG_SSI_SCR, SSI_SCR_SSIEN, val);

	ssi->slot_width = slot_width;
	ssi->slots = slots;

	return 0;
}

 
static int fsl_ssi_trigger(struct snd_pcm_substream *substream, int cmd,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct fsl_ssi *ssi = snd_soc_dai_get_drvdata(asoc_rtd_to_cpu(rtd, 0));
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		 
		if (tx && fsl_ssi_is_ac97(ssi))
			fsl_ssi_tx_ac97_saccst_setup(ssi);
		fsl_ssi_config_enable(ssi, tx);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		fsl_ssi_config_disable(ssi, tx);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int fsl_ssi_dai_probe(struct snd_soc_dai *dai)
{
	struct fsl_ssi *ssi = snd_soc_dai_get_drvdata(dai);

	if (ssi->soc->imx && ssi->use_dma)
		snd_soc_dai_init_dma_data(dai, &ssi->dma_params_tx,
					  &ssi->dma_params_rx);

	return 0;
}

static const struct snd_soc_dai_ops fsl_ssi_dai_ops = {
	.probe = fsl_ssi_dai_probe,
	.startup = fsl_ssi_startup,
	.shutdown = fsl_ssi_shutdown,
	.hw_params = fsl_ssi_hw_params,
	.hw_free = fsl_ssi_hw_free,
	.set_fmt = fsl_ssi_set_dai_fmt,
	.set_tdm_slot = fsl_ssi_set_dai_tdm_slot,
	.trigger = fsl_ssi_trigger,
};

static struct snd_soc_dai_driver fsl_ssi_dai_template = {
	.playback = {
		.stream_name = "CPU-Playback",
		.channels_min = 1,
		.channels_max = 32,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.formats = FSLSSI_I2S_FORMATS,
	},
	.capture = {
		.stream_name = "CPU-Capture",
		.channels_min = 1,
		.channels_max = 32,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.formats = FSLSSI_I2S_FORMATS,
	},
	.ops = &fsl_ssi_dai_ops,
};

static const struct snd_soc_component_driver fsl_ssi_component = {
	.name = "fsl-ssi",
	.legacy_dai_naming = 1,
};

static struct snd_soc_dai_driver fsl_ssi_ac97_dai = {
	.symmetric_channels = 1,
	.playback = {
		.stream_name = "CPU AC97 Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16 | SNDRV_PCM_FMTBIT_S20,
	},
	.capture = {
		.stream_name = "CPU AC97 Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_48000,
		 
		.formats = SNDRV_PCM_FMTBIT_S20,
	},
	.ops = &fsl_ssi_dai_ops,
};

static struct fsl_ssi *fsl_ac97_data;

static void fsl_ssi_ac97_write(struct snd_ac97 *ac97, unsigned short reg,
			       unsigned short val)
{
	struct regmap *regs = fsl_ac97_data->regs;
	unsigned int lreg;
	unsigned int lval;
	int ret;

	if (reg > 0x7f)
		return;

	mutex_lock(&fsl_ac97_data->ac97_reg_lock);

	ret = clk_prepare_enable(fsl_ac97_data->clk);
	if (ret) {
		pr_err("ac97 write clk_prepare_enable failed: %d\n",
			ret);
		goto ret_unlock;
	}

	lreg = reg <<  12;
	regmap_write(regs, REG_SSI_SACADD, lreg);

	lval = val << 4;
	regmap_write(regs, REG_SSI_SACDAT, lval);

	regmap_update_bits(regs, REG_SSI_SACNT,
			   SSI_SACNT_RDWR_MASK, SSI_SACNT_WR);
	udelay(100);

	clk_disable_unprepare(fsl_ac97_data->clk);

ret_unlock:
	mutex_unlock(&fsl_ac97_data->ac97_reg_lock);
}

static unsigned short fsl_ssi_ac97_read(struct snd_ac97 *ac97,
					unsigned short reg)
{
	struct regmap *regs = fsl_ac97_data->regs;
	unsigned short val = 0;
	u32 reg_val;
	unsigned int lreg;
	int ret;

	mutex_lock(&fsl_ac97_data->ac97_reg_lock);

	ret = clk_prepare_enable(fsl_ac97_data->clk);
	if (ret) {
		pr_err("ac97 read clk_prepare_enable failed: %d\n", ret);
		goto ret_unlock;
	}

	lreg = (reg & 0x7f) <<  12;
	regmap_write(regs, REG_SSI_SACADD, lreg);
	regmap_update_bits(regs, REG_SSI_SACNT,
			   SSI_SACNT_RDWR_MASK, SSI_SACNT_RD);

	udelay(100);

	regmap_read(regs, REG_SSI_SACDAT, &reg_val);
	val = (reg_val >> 4) & 0xffff;

	clk_disable_unprepare(fsl_ac97_data->clk);

ret_unlock:
	mutex_unlock(&fsl_ac97_data->ac97_reg_lock);
	return val;
}

static struct snd_ac97_bus_ops fsl_ssi_ac97_ops = {
	.read = fsl_ssi_ac97_read,
	.write = fsl_ssi_ac97_write,
};

 
static int fsl_ssi_hw_init(struct fsl_ssi *ssi)
{
	u32 wm = ssi->fifo_watermark;

	 
	fsl_ssi_setup_regvals(ssi);

	 
	regmap_write(ssi->regs, REG_SSI_SFCSR,
		     SSI_SFCSR_TFWM0(wm) | SSI_SFCSR_RFWM0(wm) |
		     SSI_SFCSR_TFWM1(wm) | SSI_SFCSR_RFWM1(wm));

	 
	if (ssi->use_dual_fifo)
		regmap_update_bits(ssi->regs, REG_SSI_SCR,
				   SSI_SCR_TCH_EN, SSI_SCR_TCH_EN);

	 
	if (fsl_ssi_is_ac97(ssi)) {
		_fsl_ssi_set_dai_fmt(ssi, ssi->dai_fmt);
		fsl_ssi_setup_ac97(ssi);
	}

	return 0;
}

 
static void fsl_ssi_hw_clean(struct fsl_ssi *ssi)
{
	 
	if (fsl_ssi_is_ac97(ssi)) {
		 
		regmap_update_bits(ssi->regs, REG_SSI_SCR,
				   SSI_SCR_TE | SSI_SCR_RE, 0);
		 
		regmap_write(ssi->regs, REG_SSI_SACNT, 0);
		 
		regmap_write(ssi->regs, REG_SSI_SOR, 0);
		 
		regmap_update_bits(ssi->regs, REG_SSI_SCR, SSI_SCR_SSIEN, 0);
	}
}

 
static void make_lowercase(char *s)
{
	if (!s)
		return;
	for (; *s; s++)
		*s = tolower(*s);
}

static int fsl_ssi_imx_probe(struct platform_device *pdev,
			     struct fsl_ssi *ssi, void __iomem *iomem)
{
	struct device *dev = &pdev->dev;
	int ret;

	 
	if (ssi->has_ipg_clk_name)
		ssi->clk = devm_clk_get(dev, "ipg");
	else
		ssi->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ssi->clk)) {
		ret = PTR_ERR(ssi->clk);
		dev_err(dev, "failed to get clock: %d\n", ret);
		return ret;
	}

	 
	if (!ssi->has_ipg_clk_name) {
		ret = clk_prepare_enable(ssi->clk);
		if (ret) {
			dev_err(dev, "clk_prepare_enable failed: %d\n", ret);
			return ret;
		}
	}

	 
	ssi->baudclk = devm_clk_get(dev, "baud");
	if (IS_ERR(ssi->baudclk))
		dev_dbg(dev, "failed to get baud clock: %ld\n",
			 PTR_ERR(ssi->baudclk));

	ssi->dma_params_tx.maxburst = ssi->dma_maxburst;
	ssi->dma_params_rx.maxburst = ssi->dma_maxburst;
	ssi->dma_params_tx.addr = ssi->ssi_phys + REG_SSI_STX0;
	ssi->dma_params_rx.addr = ssi->ssi_phys + REG_SSI_SRX0;

	 
	if (ssi->use_dual_fifo || ssi->use_dyna_fifo) {
		ssi->dma_params_tx.maxburst &= ~0x1;
		ssi->dma_params_rx.maxburst &= ~0x1;
	}

	if (!ssi->use_dma) {
		 
		ssi->fiq_params.irq = ssi->irq;
		ssi->fiq_params.base = iomem;
		ssi->fiq_params.dma_params_rx = &ssi->dma_params_rx;
		ssi->fiq_params.dma_params_tx = &ssi->dma_params_tx;

		ret = imx_pcm_fiq_init(pdev, &ssi->fiq_params);
		if (ret)
			goto error_pcm;
	} else {
		ret = imx_pcm_dma_init(pdev);
		if (ret)
			goto error_pcm;
	}

	return 0;

error_pcm:
	if (!ssi->has_ipg_clk_name)
		clk_disable_unprepare(ssi->clk);

	return ret;
}

static void fsl_ssi_imx_clean(struct platform_device *pdev, struct fsl_ssi *ssi)
{
	if (!ssi->use_dma)
		imx_pcm_fiq_exit(pdev);
	if (!ssi->has_ipg_clk_name)
		clk_disable_unprepare(ssi->clk);
}

static int fsl_ssi_probe_from_dt(struct fsl_ssi *ssi)
{
	struct device *dev = ssi->dev;
	struct device_node *np = dev->of_node;
	const char *p, *sprop;
	const __be32 *iprop;
	u32 dmas[4];
	int ret;

	ret = of_property_match_string(np, "clock-names", "ipg");
	 
	ssi->has_ipg_clk_name = ret >= 0;

	 
	sprop = of_get_property(np, "fsl,mode", NULL);
	if (sprop && !strcmp(sprop, "ac97-slave")) {
		ssi->dai_fmt = FSLSSI_AC97_DAIFMT;

		ret = of_property_read_u32(np, "cell-index", &ssi->card_idx);
		if (ret) {
			dev_err(dev, "failed to get SSI index property\n");
			return -EINVAL;
		}
		strcpy(ssi->card_name, "ac97-codec");
	} else if (!of_property_read_bool(np, "fsl,ssi-asynchronous")) {
		 
		ssi->synchronous = true;
	}

	 
	ssi->use_dma = !of_property_read_bool(np, "fsl,fiq-stream-filter");

	 
	iprop = of_get_property(np, "fsl,fifo-depth", NULL);
	if (iprop)
		ssi->fifo_depth = be32_to_cpup(iprop);
	else
		ssi->fifo_depth = 8;

	 
	ret = of_property_read_u32_array(np, "dmas", dmas, 4);
	if (ssi->use_dma && !ret && dmas[2] == IMX_DMATYPE_SSI_DUAL)
		ssi->use_dual_fifo = true;

	if (ssi->use_dma && !ret && dmas[2] == IMX_DMATYPE_MULTI_SAI)
		ssi->use_dyna_fifo = true;
	 
	if (!ssi->card_name[0] && of_get_property(np, "codec-handle", NULL)) {
		struct device_node *root = of_find_node_by_path("/");

		sprop = of_get_property(root, "compatible", NULL);
		of_node_put(root);
		 
		p = strrchr(sprop, ',');
		if (p)
			sprop = p + 1;
		snprintf(ssi->card_name, sizeof(ssi->card_name),
			 "snd-soc-%s", sprop);
		make_lowercase(ssi->card_name);
		ssi->card_idx = 0;
	}

	return 0;
}

static int fsl_ssi_probe(struct platform_device *pdev)
{
	struct regmap_config regconfig = fsl_ssi_regconfig;
	struct device *dev = &pdev->dev;
	struct fsl_ssi *ssi;
	struct resource *res;
	void __iomem *iomem;
	int ret = 0;

	ssi = devm_kzalloc(dev, sizeof(*ssi), GFP_KERNEL);
	if (!ssi)
		return -ENOMEM;

	ssi->dev = dev;
	ssi->soc = of_device_get_match_data(&pdev->dev);

	 
	ret = fsl_ssi_probe_from_dt(ssi);
	if (ret)
		return ret;

	if (fsl_ssi_is_ac97(ssi)) {
		memcpy(&ssi->cpu_dai_drv, &fsl_ssi_ac97_dai,
		       sizeof(fsl_ssi_ac97_dai));
		fsl_ac97_data = ssi;
	} else {
		memcpy(&ssi->cpu_dai_drv, &fsl_ssi_dai_template,
		       sizeof(fsl_ssi_dai_template));
	}
	ssi->cpu_dai_drv.name = dev_name(dev);

	iomem = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(iomem))
		return PTR_ERR(iomem);
	ssi->ssi_phys = res->start;

	if (ssi->soc->imx21regs) {
		 
		regconfig.max_register = REG_SSI_SRMSK;
		regconfig.num_reg_defaults_raw =
			REG_SSI_SRMSK / sizeof(uint32_t) + 1;
	}

	if (ssi->has_ipg_clk_name)
		ssi->regs = devm_regmap_init_mmio_clk(dev, "ipg", iomem,
						      &regconfig);
	else
		ssi->regs = devm_regmap_init_mmio(dev, iomem, &regconfig);
	if (IS_ERR(ssi->regs)) {
		dev_err(dev, "failed to init register map\n");
		return PTR_ERR(ssi->regs);
	}

	ssi->irq = platform_get_irq(pdev, 0);
	if (ssi->irq < 0)
		return ssi->irq;

	 
	if (ssi->synchronous && !fsl_ssi_is_ac97(ssi)) {
		ssi->cpu_dai_drv.symmetric_rate = 1;
		ssi->cpu_dai_drv.symmetric_channels = 1;
		ssi->cpu_dai_drv.symmetric_sample_bits = 1;
	}

	 
	switch (ssi->fifo_depth) {
	case 15:
		 
		ssi->fifo_watermark = 8;
		ssi->dma_maxburst = 8;
		break;
	case 8:
	default:
		 
		ssi->fifo_watermark = ssi->fifo_depth - 2;
		ssi->dma_maxburst = ssi->fifo_depth - 2;
		break;
	}

	dev_set_drvdata(dev, ssi);

	if (ssi->soc->imx) {
		ret = fsl_ssi_imx_probe(pdev, ssi, iomem);
		if (ret)
			return ret;
	}

	if (fsl_ssi_is_ac97(ssi)) {
		mutex_init(&ssi->ac97_reg_lock);
		ret = snd_soc_set_ac97_ops_of_reset(&fsl_ssi_ac97_ops, pdev);
		if (ret) {
			dev_err(dev, "failed to set AC'97 ops\n");
			goto error_ac97_ops;
		}
	}

	ret = devm_snd_soc_register_component(dev, &fsl_ssi_component,
					      &ssi->cpu_dai_drv, 1);
	if (ret) {
		dev_err(dev, "failed to register DAI: %d\n", ret);
		goto error_asoc_register;
	}

	if (ssi->use_dma) {
		ret = devm_request_irq(dev, ssi->irq, fsl_ssi_isr, 0,
				       dev_name(dev), ssi);
		if (ret < 0) {
			dev_err(dev, "failed to claim irq %u\n", ssi->irq);
			goto error_asoc_register;
		}
	}

	fsl_ssi_debugfs_create(&ssi->dbg_stats, dev);

	 
	fsl_ssi_hw_init(ssi);

	 
	if (ssi->card_name[0]) {
		struct device *parent = dev;
		 
		if (fsl_ssi_is_ac97(ssi))
			parent = NULL;

		ssi->card_pdev = platform_device_register_data(parent,
				ssi->card_name, ssi->card_idx, NULL, 0);
		if (IS_ERR(ssi->card_pdev)) {
			ret = PTR_ERR(ssi->card_pdev);
			dev_err(dev, "failed to register %s: %d\n",
				ssi->card_name, ret);
			goto error_sound_card;
		}
	}

	return 0;

error_sound_card:
	fsl_ssi_debugfs_remove(&ssi->dbg_stats);
error_asoc_register:
	if (fsl_ssi_is_ac97(ssi))
		snd_soc_set_ac97_ops(NULL);
error_ac97_ops:
	if (fsl_ssi_is_ac97(ssi))
		mutex_destroy(&ssi->ac97_reg_lock);

	if (ssi->soc->imx)
		fsl_ssi_imx_clean(pdev, ssi);

	return ret;
}

static void fsl_ssi_remove(struct platform_device *pdev)
{
	struct fsl_ssi *ssi = dev_get_drvdata(&pdev->dev);

	fsl_ssi_debugfs_remove(&ssi->dbg_stats);

	if (ssi->card_pdev)
		platform_device_unregister(ssi->card_pdev);

	 
	fsl_ssi_hw_clean(ssi);

	if (ssi->soc->imx)
		fsl_ssi_imx_clean(pdev, ssi);

	if (fsl_ssi_is_ac97(ssi)) {
		snd_soc_set_ac97_ops(NULL);
		mutex_destroy(&ssi->ac97_reg_lock);
	}
}

#ifdef CONFIG_PM_SLEEP
static int fsl_ssi_suspend(struct device *dev)
{
	struct fsl_ssi *ssi = dev_get_drvdata(dev);
	struct regmap *regs = ssi->regs;

	regmap_read(regs, REG_SSI_SFCSR, &ssi->regcache_sfcsr);
	regmap_read(regs, REG_SSI_SACNT, &ssi->regcache_sacnt);

	regcache_cache_only(regs, true);
	regcache_mark_dirty(regs);

	return 0;
}

static int fsl_ssi_resume(struct device *dev)
{
	struct fsl_ssi *ssi = dev_get_drvdata(dev);
	struct regmap *regs = ssi->regs;

	regcache_cache_only(regs, false);

	regmap_update_bits(regs, REG_SSI_SFCSR,
			   SSI_SFCSR_RFWM1_MASK | SSI_SFCSR_TFWM1_MASK |
			   SSI_SFCSR_RFWM0_MASK | SSI_SFCSR_TFWM0_MASK,
			   ssi->regcache_sfcsr);
	regmap_write(regs, REG_SSI_SACNT, ssi->regcache_sacnt);

	return regcache_sync(regs);
}
#endif  

static const struct dev_pm_ops fsl_ssi_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(fsl_ssi_suspend, fsl_ssi_resume)
};

static struct platform_driver fsl_ssi_driver = {
	.driver = {
		.name = "fsl-ssi-dai",
		.of_match_table = fsl_ssi_ids,
		.pm = &fsl_ssi_pm,
	},
	.probe = fsl_ssi_probe,
	.remove_new = fsl_ssi_remove,
};

module_platform_driver(fsl_ssi_driver);

MODULE_ALIAS("platform:fsl-ssi-dai");
MODULE_AUTHOR("Timur Tabi <timur@freescale.com>");
MODULE_DESCRIPTION("Freescale Synchronous Serial Interface (SSI) ASoC Driver");
MODULE_LICENSE("GPL v2");
