
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/time.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "mxs-saif.h"

#define MXS_SET_ADDR	0x4
#define MXS_CLR_ADDR	0x8

static struct mxs_saif *mxs_saif[2];

 

static int mxs_saif_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
			int clk_id, unsigned int freq, int dir)
{
	struct mxs_saif *saif = snd_soc_dai_get_drvdata(cpu_dai);

	switch (clk_id) {
	case MXS_SAIF_MCLK:
		saif->mclk = freq;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

 
static inline struct mxs_saif *mxs_saif_get_master(struct mxs_saif * saif)
{
	return mxs_saif[saif->master_id];
}

 
static int mxs_saif_set_clk(struct mxs_saif *saif,
				  unsigned int mclk,
				  unsigned int rate)
{
	u32 scr;
	int ret;
	struct mxs_saif *master_saif;

	dev_dbg(saif->dev, "mclk %d rate %d\n", mclk, rate);

	 
	master_saif = mxs_saif_get_master(saif);
	if (!master_saif)
		return -EINVAL;

	dev_dbg(saif->dev, "master saif%d\n", master_saif->id);

	 
	if (master_saif->ongoing && rate != master_saif->cur_rate) {
		dev_err(saif->dev,
			"can not change clock, master saif%d(rate %d) is ongoing\n",
			master_saif->id, master_saif->cur_rate);
		return -EINVAL;
	}

	scr = __raw_readl(master_saif->base + SAIF_CTRL);
	scr &= ~BM_SAIF_CTRL_BITCLK_MULT_RATE;
	scr &= ~BM_SAIF_CTRL_BITCLK_BASE_RATE;

	 
	ret = clk_prepare_enable(master_saif->clk);
	if (ret)
		return ret;

	if (master_saif->mclk_in_use) {
		switch (mclk / rate) {
		case 32:
		case 64:
		case 128:
		case 256:
		case 512:
			scr &= ~BM_SAIF_CTRL_BITCLK_BASE_RATE;
			ret = clk_set_rate(master_saif->clk, 512 * rate);
			break;
		case 48:
		case 96:
		case 192:
		case 384:
			scr |= BM_SAIF_CTRL_BITCLK_BASE_RATE;
			ret = clk_set_rate(master_saif->clk, 384 * rate);
			break;
		default:
			 
			clk_disable_unprepare(master_saif->clk);
			return -EINVAL;
		}
	} else {
		ret = clk_set_rate(master_saif->clk, 512 * rate);
		scr &= ~BM_SAIF_CTRL_BITCLK_BASE_RATE;
	}

	clk_disable_unprepare(master_saif->clk);

	if (ret)
		return ret;

	master_saif->cur_rate = rate;

	if (!master_saif->mclk_in_use) {
		__raw_writel(scr, master_saif->base + SAIF_CTRL);
		return 0;
	}

	 
	switch (mclk / rate) {
	case 32:
		scr |= BF_SAIF_CTRL_BITCLK_MULT_RATE(4);
		break;
	case 64:
		scr |= BF_SAIF_CTRL_BITCLK_MULT_RATE(3);
		break;
	case 128:
		scr |= BF_SAIF_CTRL_BITCLK_MULT_RATE(2);
		break;
	case 256:
		scr |= BF_SAIF_CTRL_BITCLK_MULT_RATE(1);
		break;
	case 512:
		scr |= BF_SAIF_CTRL_BITCLK_MULT_RATE(0);
		break;
	case 48:
		scr |= BF_SAIF_CTRL_BITCLK_MULT_RATE(3);
		break;
	case 96:
		scr |= BF_SAIF_CTRL_BITCLK_MULT_RATE(2);
		break;
	case 192:
		scr |= BF_SAIF_CTRL_BITCLK_MULT_RATE(1);
		break;
	case 384:
		scr |= BF_SAIF_CTRL_BITCLK_MULT_RATE(0);
		break;
	default:
		return -EINVAL;
	}

	__raw_writel(scr, master_saif->base + SAIF_CTRL);

	return 0;
}

 
int mxs_saif_put_mclk(unsigned int saif_id)
{
	struct mxs_saif *saif = mxs_saif[saif_id];
	u32 stat;

	if (!saif)
		return -EINVAL;

	stat = __raw_readl(saif->base + SAIF_STAT);
	if (stat & BM_SAIF_STAT_BUSY) {
		dev_err(saif->dev, "error: busy\n");
		return -EBUSY;
	}

	clk_disable_unprepare(saif->clk);

	 
	__raw_writel(BM_SAIF_CTRL_CLKGATE,
		saif->base + SAIF_CTRL + MXS_SET_ADDR);
	__raw_writel(BM_SAIF_CTRL_RUN,
		saif->base + SAIF_CTRL + MXS_CLR_ADDR);

	saif->mclk_in_use = 0;
	return 0;
}
EXPORT_SYMBOL_GPL(mxs_saif_put_mclk);

 
int mxs_saif_get_mclk(unsigned int saif_id, unsigned int mclk,
					unsigned int rate)
{
	struct mxs_saif *saif = mxs_saif[saif_id];
	u32 stat;
	int ret;
	struct mxs_saif *master_saif;

	if (!saif)
		return -EINVAL;

	 
	__raw_writel(BM_SAIF_CTRL_SFTRST,
		saif->base + SAIF_CTRL + MXS_CLR_ADDR);

	 
	__raw_writel(BM_SAIF_CTRL_CLKGATE,
		saif->base + SAIF_CTRL + MXS_CLR_ADDR);

	master_saif = mxs_saif_get_master(saif);
	if (saif != master_saif) {
		dev_err(saif->dev, "can not get mclk from a non-master saif\n");
		return -EINVAL;
	}

	stat = __raw_readl(saif->base + SAIF_STAT);
	if (stat & BM_SAIF_STAT_BUSY) {
		dev_err(saif->dev, "error: busy\n");
		return -EBUSY;
	}

	saif->mclk_in_use = 1;
	ret = mxs_saif_set_clk(saif, mclk, rate);
	if (ret)
		return ret;

	ret = clk_prepare_enable(saif->clk);
	if (ret)
		return ret;

	 
	__raw_writel(BM_SAIF_CTRL_RUN,
		saif->base + SAIF_CTRL + MXS_SET_ADDR);

	return 0;
}
EXPORT_SYMBOL_GPL(mxs_saif_get_mclk);

 
static int mxs_saif_set_dai_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	u32 scr, stat;
	u32 scr0;
	struct mxs_saif *saif = snd_soc_dai_get_drvdata(cpu_dai);

	stat = __raw_readl(saif->base + SAIF_STAT);
	if (stat & BM_SAIF_STAT_BUSY) {
		dev_err(cpu_dai->dev, "error: busy\n");
		return -EBUSY;
	}

	 
	if (saif->id != saif->master_id) {
		__raw_writel(BM_SAIF_CTRL_SFTRST,
			saif->base + SAIF_CTRL + MXS_CLR_ADDR);
		__raw_writel(BM_SAIF_CTRL_CLKGATE,
			saif->base + SAIF_CTRL + MXS_CLR_ADDR);
	}

	scr0 = __raw_readl(saif->base + SAIF_CTRL);
	scr0 = scr0 & ~BM_SAIF_CTRL_BITCLK_EDGE & ~BM_SAIF_CTRL_LRCLK_POLARITY \
		& ~BM_SAIF_CTRL_JUSTIFY & ~BM_SAIF_CTRL_DELAY;
	scr = 0;

	 
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		 
		scr |= BM_SAIF_CTRL_DELAY;
		scr &= ~BM_SAIF_CTRL_LRCLK_POLARITY;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		 
		scr &= ~BM_SAIF_CTRL_DELAY;
		scr &= ~BM_SAIF_CTRL_LRCLK_POLARITY;
		scr &= ~BM_SAIF_CTRL_JUSTIFY;
		break;
	default:
		return -EINVAL;
	}

	 
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_IB_IF:
		scr |= BM_SAIF_CTRL_BITCLK_EDGE;
		scr |= BM_SAIF_CTRL_LRCLK_POLARITY;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		scr |= BM_SAIF_CTRL_BITCLK_EDGE;
		scr &= ~BM_SAIF_CTRL_LRCLK_POLARITY;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		scr &= ~BM_SAIF_CTRL_BITCLK_EDGE;
		scr |= BM_SAIF_CTRL_LRCLK_POLARITY;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		scr &= ~BM_SAIF_CTRL_BITCLK_EDGE;
		scr &= ~BM_SAIF_CTRL_LRCLK_POLARITY;
		break;
	}

	 
	switch (fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
	case SND_SOC_DAIFMT_BP_FP:
		if (saif->id == saif->master_id)
			scr &= ~BM_SAIF_CTRL_SLAVE_MODE;
		else
			scr |= BM_SAIF_CTRL_SLAVE_MODE;

		__raw_writel(scr | scr0, saif->base + SAIF_CTRL);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mxs_saif_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *cpu_dai)
{
	struct mxs_saif *saif = snd_soc_dai_get_drvdata(cpu_dai);
	int ret;

	 
	saif->fifo_underrun = 0;
	saif->fifo_overrun = 0;

	 
	__raw_writel(BM_SAIF_CTRL_SFTRST,
		saif->base + SAIF_CTRL + MXS_CLR_ADDR);

	 
	__raw_writel(BM_SAIF_CTRL_CLKGATE,
		saif->base + SAIF_CTRL + MXS_CLR_ADDR);

	ret = clk_prepare(saif->clk);
	if (ret)
		return ret;

	return 0;
}

static void mxs_saif_shutdown(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *cpu_dai)
{
	struct mxs_saif *saif = snd_soc_dai_get_drvdata(cpu_dai);

	clk_unprepare(saif->clk);
}

 
static int mxs_saif_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *cpu_dai)
{
	struct mxs_saif *saif = snd_soc_dai_get_drvdata(cpu_dai);
	struct mxs_saif *master_saif;
	u32 scr, stat;
	int ret;

	master_saif = mxs_saif_get_master(saif);
	if (!master_saif)
		return -EINVAL;

	 
	if (!saif->mclk && saif->mclk_in_use) {
		dev_err(cpu_dai->dev, "set mclk first\n");
		return -EINVAL;
	}

	stat = __raw_readl(saif->base + SAIF_STAT);
	if (!saif->mclk_in_use && (stat & BM_SAIF_STAT_BUSY)) {
		dev_err(cpu_dai->dev, "error: busy\n");
		return -EBUSY;
	}

	 
	ret = mxs_saif_set_clk(saif, saif->mclk, params_rate(params));
	if (ret) {
		dev_err(cpu_dai->dev, "unable to get proper clk\n");
		return ret;
	}

	if (saif != master_saif) {
		 
		ret = clk_enable(saif->clk);
		if (ret)
			return ret;

		ret = clk_set_rate(saif->clk, 24000000);
		clk_disable(saif->clk);
		if (ret)
			return ret;

		ret = clk_prepare(master_saif->clk);
		if (ret)
			return ret;
	}

	scr = __raw_readl(saif->base + SAIF_CTRL);

	scr &= ~BM_SAIF_CTRL_WORD_LENGTH;
	scr &= ~BM_SAIF_CTRL_BITCLK_48XFS_ENABLE;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		scr |= BF_SAIF_CTRL_WORD_LENGTH(0);
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		scr |= BF_SAIF_CTRL_WORD_LENGTH(4);
		scr |= BM_SAIF_CTRL_BITCLK_48XFS_ENABLE;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		scr |= BF_SAIF_CTRL_WORD_LENGTH(8);
		scr |= BM_SAIF_CTRL_BITCLK_48XFS_ENABLE;
		break;
	default:
		return -EINVAL;
	}

	 
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		 
		scr &= ~BM_SAIF_CTRL_READ_MODE;
	} else {
		 
		scr |= BM_SAIF_CTRL_READ_MODE;
	}

	__raw_writel(scr, saif->base + SAIF_CTRL);
	return 0;
}

static int mxs_saif_prepare(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *cpu_dai)
{
	struct mxs_saif *saif = snd_soc_dai_get_drvdata(cpu_dai);

	 
	__raw_writel(BM_SAIF_CTRL_FIFO_ERROR_IRQ_EN,
		saif->base + SAIF_CTRL + MXS_SET_ADDR);

	return 0;
}

static int mxs_saif_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *cpu_dai)
{
	struct mxs_saif *saif = snd_soc_dai_get_drvdata(cpu_dai);
	struct mxs_saif *master_saif;
	u32 delay;
	int ret;

	master_saif = mxs_saif_get_master(saif);
	if (!master_saif)
		return -EINVAL;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (saif->state == MXS_SAIF_STATE_RUNNING)
			return 0;

		dev_dbg(cpu_dai->dev, "start\n");

		ret = clk_enable(master_saif->clk);
		if (ret) {
			dev_err(saif->dev, "Failed to enable master clock\n");
			return ret;
		}

		 
		if (saif != master_saif) {
			ret = clk_enable(saif->clk);
			if (ret) {
				dev_err(saif->dev, "Failed to enable master clock\n");
				clk_disable(master_saif->clk);
				return ret;
			}

			__raw_writel(BM_SAIF_CTRL_RUN,
				saif->base + SAIF_CTRL + MXS_SET_ADDR);
		}

		if (!master_saif->mclk_in_use)
			__raw_writel(BM_SAIF_CTRL_RUN,
				master_saif->base + SAIF_CTRL + MXS_SET_ADDR);

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			 
			__raw_writel(0, saif->base + SAIF_DATA);
			__raw_writel(0, saif->base + SAIF_DATA);
		} else {
			 
			__raw_readl(saif->base + SAIF_DATA);
			__raw_readl(saif->base + SAIF_DATA);
		}

		master_saif->ongoing = 1;
		saif->state = MXS_SAIF_STATE_RUNNING;

		dev_dbg(saif->dev, "CTRL 0x%x STAT 0x%x\n",
			__raw_readl(saif->base + SAIF_CTRL),
			__raw_readl(saif->base + SAIF_STAT));

		dev_dbg(master_saif->dev, "CTRL 0x%x STAT 0x%x\n",
			__raw_readl(master_saif->base + SAIF_CTRL),
			__raw_readl(master_saif->base + SAIF_STAT));
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (saif->state == MXS_SAIF_STATE_STOPPED)
			return 0;

		dev_dbg(cpu_dai->dev, "stop\n");

		 
		delay = USEC_PER_SEC / master_saif->cur_rate;

		if (!master_saif->mclk_in_use) {
			__raw_writel(BM_SAIF_CTRL_RUN,
				master_saif->base + SAIF_CTRL + MXS_CLR_ADDR);
			udelay(delay);
		}
		clk_disable(master_saif->clk);

		if (saif != master_saif) {
			__raw_writel(BM_SAIF_CTRL_RUN,
				saif->base + SAIF_CTRL + MXS_CLR_ADDR);
			udelay(delay);
			clk_disable(saif->clk);
		}

		master_saif->ongoing = 0;
		saif->state = MXS_SAIF_STATE_STOPPED;

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define MXS_SAIF_RATES		SNDRV_PCM_RATE_8000_192000
#define MXS_SAIF_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

static const struct snd_soc_dai_ops mxs_saif_dai_ops = {
	.startup = mxs_saif_startup,
	.shutdown = mxs_saif_shutdown,
	.trigger = mxs_saif_trigger,
	.prepare = mxs_saif_prepare,
	.hw_params = mxs_saif_hw_params,
	.set_sysclk = mxs_saif_set_dai_sysclk,
	.set_fmt = mxs_saif_set_dai_fmt,
};

static struct snd_soc_dai_driver mxs_saif_dai = {
	.name = "mxs-saif",
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = MXS_SAIF_RATES,
		.formats = MXS_SAIF_FORMATS,
	},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = MXS_SAIF_RATES,
		.formats = MXS_SAIF_FORMATS,
	},
	.ops = &mxs_saif_dai_ops,
};

static const struct snd_soc_component_driver mxs_saif_component = {
	.name			= "mxs-saif",
	.legacy_dai_naming	= 1,
};

static irqreturn_t mxs_saif_irq(int irq, void *dev_id)
{
	struct mxs_saif *saif = dev_id;
	unsigned int stat;

	stat = __raw_readl(saif->base + SAIF_STAT);
	if (!(stat & (BM_SAIF_STAT_FIFO_UNDERFLOW_IRQ |
			BM_SAIF_STAT_FIFO_OVERFLOW_IRQ)))
		return IRQ_NONE;

	if (stat & BM_SAIF_STAT_FIFO_UNDERFLOW_IRQ) {
		dev_dbg(saif->dev, "underrun!!! %d\n", ++saif->fifo_underrun);
		__raw_writel(BM_SAIF_STAT_FIFO_UNDERFLOW_IRQ,
				saif->base + SAIF_STAT + MXS_CLR_ADDR);
	}

	if (stat & BM_SAIF_STAT_FIFO_OVERFLOW_IRQ) {
		dev_dbg(saif->dev, "overrun!!! %d\n", ++saif->fifo_overrun);
		__raw_writel(BM_SAIF_STAT_FIFO_OVERFLOW_IRQ,
				saif->base + SAIF_STAT + MXS_CLR_ADDR);
	}

	dev_dbg(saif->dev, "SAIF_CTRL %x SAIF_STAT %x\n",
	       __raw_readl(saif->base + SAIF_CTRL),
	       __raw_readl(saif->base + SAIF_STAT));

	return IRQ_HANDLED;
}

static int mxs_saif_mclk_init(struct platform_device *pdev)
{
	struct mxs_saif *saif = platform_get_drvdata(pdev);
	struct device_node *np = pdev->dev.of_node;
	struct clk *clk;
	int ret;

	clk = clk_register_divider(&pdev->dev, "mxs_saif_mclk",
				   __clk_get_name(saif->clk), 0,
				   saif->base + SAIF_CTRL,
				   BP_SAIF_CTRL_BITCLK_MULT_RATE, 3,
				   0, NULL);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		if (ret == -EEXIST)
			return 0;
		dev_err(&pdev->dev, "failed to register mclk: %d\n", ret);
		return PTR_ERR(clk);
	}

	ret = of_clk_add_provider(np, of_clk_src_simple_get, clk);
	if (ret)
		return ret;

	return 0;
}

static int mxs_saif_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct mxs_saif *saif;
	int irq, ret;
	struct device_node *master;

	saif = devm_kzalloc(&pdev->dev, sizeof(*saif), GFP_KERNEL);
	if (!saif)
		return -ENOMEM;

	ret = of_alias_get_id(np, "saif");
	if (ret < 0)
		return ret;
	else
		saif->id = ret;

	if (saif->id >= ARRAY_SIZE(mxs_saif)) {
		dev_err(&pdev->dev, "get wrong saif id\n");
		return -EINVAL;
	}

	 
	master = of_parse_phandle(np, "fsl,saif-master", 0);
	if (!master) {
		saif->master_id = saif->id;
	} else {
		ret = of_alias_get_id(master, "saif");
		of_node_put(master);
		if (ret < 0)
			return ret;
		else
			saif->master_id = ret;

		if (saif->master_id >= ARRAY_SIZE(mxs_saif)) {
			dev_err(&pdev->dev, "get wrong master id\n");
			return -EINVAL;
		}
	}

	mxs_saif[saif->id] = saif;

	saif->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(saif->clk)) {
		ret = PTR_ERR(saif->clk);
		dev_err(&pdev->dev, "Cannot get the clock: %d\n",
			ret);
		return ret;
	}

	saif->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(saif->base))
		return PTR_ERR(saif->base);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	saif->dev = &pdev->dev;
	ret = devm_request_irq(&pdev->dev, irq, mxs_saif_irq, 0,
			       dev_name(&pdev->dev), saif);
	if (ret) {
		dev_err(&pdev->dev, "failed to request irq\n");
		return ret;
	}

	platform_set_drvdata(pdev, saif);

	 
	if (saif->id == 0) {
		ret = mxs_saif_mclk_init(pdev);
		if (ret)
			dev_warn(&pdev->dev, "failed to init clocks\n");
	}

	ret = devm_snd_soc_register_component(&pdev->dev, &mxs_saif_component,
					      &mxs_saif_dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "register DAI failed\n");
		return ret;
	}

	ret = mxs_pcm_platform_register(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "register PCM failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id mxs_saif_dt_ids[] = {
	{ .compatible = "fsl,imx28-saif", },
	{   }
};
MODULE_DEVICE_TABLE(of, mxs_saif_dt_ids);

static struct platform_driver mxs_saif_driver = {
	.probe = mxs_saif_probe,

	.driver = {
		.name = "mxs-saif",
		.of_match_table = mxs_saif_dt_ids,
	},
};

module_platform_driver(mxs_saif_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("MXS ASoC SAIF driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mxs-saif");
