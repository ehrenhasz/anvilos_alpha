
 

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-au1x00/au1xxx_psc.h>

#include "psc.h"

 
#define AC97_RW_RETRIES	5

#define AC97_DIR	\
	(SND_SOC_DAIDIR_PLAYBACK | SND_SOC_DAIDIR_CAPTURE)

#define AC97_RATES	\
	SNDRV_PCM_RATE_8000_48000

#define AC97_FMTS	\
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3BE)

#define AC97PCR_START(stype)	\
	((stype) == SNDRV_PCM_STREAM_PLAYBACK ? PSC_AC97PCR_TS : PSC_AC97PCR_RS)
#define AC97PCR_STOP(stype)	\
	((stype) == SNDRV_PCM_STREAM_PLAYBACK ? PSC_AC97PCR_TP : PSC_AC97PCR_RP)
#define AC97PCR_CLRFIFO(stype)	\
	((stype) == SNDRV_PCM_STREAM_PLAYBACK ? PSC_AC97PCR_TC : PSC_AC97PCR_RC)

#define AC97STAT_BUSY(stype)	\
	((stype) == SNDRV_PCM_STREAM_PLAYBACK ? PSC_AC97STAT_TB : PSC_AC97STAT_RB)

 
static struct au1xpsc_audio_data *au1xpsc_ac97_workdata;

#if 0

 
static inline struct au1xpsc_audio_data *ac97_to_pscdata(struct snd_ac97 *x)
{
	struct snd_soc_card *c = x->bus->card->private_data;
	return snd_soc_dai_get_drvdata(c->asoc_rtd_to_cpu(rtd, 0));
}

#else

#define ac97_to_pscdata(x)	au1xpsc_ac97_workdata

#endif

 
static unsigned short au1xpsc_ac97_read(struct snd_ac97 *ac97,
					unsigned short reg)
{
	struct au1xpsc_audio_data *pscdata = ac97_to_pscdata(ac97);
	unsigned short retry, tmo;
	unsigned long data;

	__raw_writel(PSC_AC97EVNT_CD, AC97_EVNT(pscdata));
	wmb();  

	retry = AC97_RW_RETRIES;
	do {
		mutex_lock(&pscdata->lock);

		__raw_writel(PSC_AC97CDC_RD | PSC_AC97CDC_INDX(reg),
			  AC97_CDC(pscdata));
		wmb();  

		tmo = 20;
		do {
			udelay(21);
			if (__raw_readl(AC97_EVNT(pscdata)) & PSC_AC97EVNT_CD)
				break;
		} while (--tmo);

		data = __raw_readl(AC97_CDC(pscdata));

		__raw_writel(PSC_AC97EVNT_CD, AC97_EVNT(pscdata));
		wmb();  

		mutex_unlock(&pscdata->lock);

		if (reg != ((data >> 16) & 0x7f))
			tmo = 1;	 

	} while (--retry && !tmo);

	return retry ? data & 0xffff : 0xffff;
}

 
static void au1xpsc_ac97_write(struct snd_ac97 *ac97, unsigned short reg,
				unsigned short val)
{
	struct au1xpsc_audio_data *pscdata = ac97_to_pscdata(ac97);
	unsigned int tmo, retry;

	__raw_writel(PSC_AC97EVNT_CD, AC97_EVNT(pscdata));
	wmb();  

	retry = AC97_RW_RETRIES;
	do {
		mutex_lock(&pscdata->lock);

		__raw_writel(PSC_AC97CDC_INDX(reg) | (val & 0xffff),
			  AC97_CDC(pscdata));
		wmb();  

		tmo = 20;
		do {
			udelay(21);
			if (__raw_readl(AC97_EVNT(pscdata)) & PSC_AC97EVNT_CD)
				break;
		} while (--tmo);

		__raw_writel(PSC_AC97EVNT_CD, AC97_EVNT(pscdata));
		wmb();  

		mutex_unlock(&pscdata->lock);
	} while (--retry && !tmo);
}

 
static void au1xpsc_ac97_warm_reset(struct snd_ac97 *ac97)
{
	struct au1xpsc_audio_data *pscdata = ac97_to_pscdata(ac97);

	__raw_writel(PSC_AC97RST_SNC, AC97_RST(pscdata));
	wmb();  
	msleep(10);
	__raw_writel(0, AC97_RST(pscdata));
	wmb();  
}

static void au1xpsc_ac97_cold_reset(struct snd_ac97 *ac97)
{
	struct au1xpsc_audio_data *pscdata = ac97_to_pscdata(ac97);
	int i;

	 
	__raw_writel(0, AC97_CFG(au1xpsc_ac97_workdata));
	wmb();  
	__raw_writel(PSC_CTRL_DISABLE, PSC_CTRL(pscdata));
	wmb();  

	 
	__raw_writel(PSC_AC97RST_RST, AC97_RST(pscdata));
	wmb();  
	msleep(500);
	__raw_writel(0, AC97_RST(pscdata));
	wmb();  

	 
	__raw_writel(PSC_CTRL_ENABLE, PSC_CTRL(pscdata));
	wmb();  

	 
	i = 1000;
	while (!((__raw_readl(AC97_STAT(pscdata)) & PSC_AC97STAT_SR)) && (--i))
		msleep(1);

	if (i == 0) {
		printk(KERN_ERR "au1xpsc-ac97: PSC not ready!\n");
		return;
	}

	 
	__raw_writel(pscdata->cfg | PSC_AC97CFG_DE_ENABLE, AC97_CFG(pscdata));
	wmb();  

	 
	i = 1000;
	while (!((__raw_readl(AC97_STAT(pscdata)) & PSC_AC97STAT_DR)) && (--i))
		msleep(1);
	if (i == 0)
		printk(KERN_ERR "au1xpsc-ac97: AC97 ctrl not ready\n");
}

 
static struct snd_ac97_bus_ops psc_ac97_ops = {
	.read		= au1xpsc_ac97_read,
	.write		= au1xpsc_ac97_write,
	.reset		= au1xpsc_ac97_cold_reset,
	.warm_reset	= au1xpsc_ac97_warm_reset,
};

static int au1xpsc_ac97_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct au1xpsc_audio_data *pscdata = snd_soc_dai_get_drvdata(dai);
	unsigned long r, ro, stat;
	int chans, t, stype = substream->stream;

	chans = params_channels(params);

	r = ro = __raw_readl(AC97_CFG(pscdata));
	stat = __raw_readl(AC97_STAT(pscdata));

	 
	if (stat & (PSC_AC97STAT_TB | PSC_AC97STAT_RB)) {
		 
		if ((PSC_AC97CFG_GET_LEN(r) != params->msbits) ||
		    (pscdata->rate != params_rate(params)))
			return -EINVAL;
	} else {

		 
		r &= ~PSC_AC97CFG_LEN_MASK;
		r |= PSC_AC97CFG_SET_LEN(params->msbits);

		 
		if (stype == SNDRV_PCM_STREAM_PLAYBACK) {
			r &= ~PSC_AC97CFG_TXSLOT_MASK;
			r |= PSC_AC97CFG_TXSLOT_ENA(3);
			r |= PSC_AC97CFG_TXSLOT_ENA(4);
		} else {
			r &= ~PSC_AC97CFG_RXSLOT_MASK;
			r |= PSC_AC97CFG_RXSLOT_ENA(3);
			r |= PSC_AC97CFG_RXSLOT_ENA(4);
		}

		 
		if (!(r ^ ro))
			goto out;

		 
		mutex_lock(&pscdata->lock);

		 
		__raw_writel(r & ~PSC_AC97CFG_DE_ENABLE, AC97_CFG(pscdata));
		wmb();  

		 
		t = 100;
		while ((__raw_readl(AC97_STAT(pscdata)) & PSC_AC97STAT_DR) && --t)
			msleep(1);

		if (!t)
			printk(KERN_ERR "PSC-AC97: can't disable!\n");

		 
		__raw_writel(r, AC97_CFG(pscdata));
		wmb();  

		 
		__raw_writel(r | PSC_AC97CFG_DE_ENABLE, AC97_CFG(pscdata));
		wmb();  

		 
		t = 100;
		while ((!(__raw_readl(AC97_STAT(pscdata)) & PSC_AC97STAT_DR)) && --t)
			msleep(1);

		if (!t)
			printk(KERN_ERR "PSC-AC97: can't enable!\n");

		mutex_unlock(&pscdata->lock);

		pscdata->cfg = r;
		pscdata->rate = params_rate(params);
	}

out:
	return 0;
}

static int au1xpsc_ac97_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct au1xpsc_audio_data *pscdata = snd_soc_dai_get_drvdata(dai);
	int ret, stype = substream->stream;

	ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		__raw_writel(AC97PCR_CLRFIFO(stype), AC97_PCR(pscdata));
		wmb();  
		__raw_writel(AC97PCR_START(stype), AC97_PCR(pscdata));
		wmb();  
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		__raw_writel(AC97PCR_STOP(stype), AC97_PCR(pscdata));
		wmb();  

		while (__raw_readl(AC97_STAT(pscdata)) & AC97STAT_BUSY(stype))
			asm volatile ("nop");

		__raw_writel(AC97PCR_CLRFIFO(stype), AC97_PCR(pscdata));
		wmb();  

		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int au1xpsc_ac97_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct au1xpsc_audio_data *pscdata = snd_soc_dai_get_drvdata(dai);
	snd_soc_dai_set_dma_data(dai, substream, &pscdata->dmaids[0]);
	return 0;
}

static int au1xpsc_ac97_probe(struct snd_soc_dai *dai)
{
	return au1xpsc_ac97_workdata ? 0 : -ENODEV;
}

static const struct snd_soc_dai_ops au1xpsc_ac97_dai_ops = {
	.probe		= au1xpsc_ac97_probe,
	.startup	= au1xpsc_ac97_startup,
	.trigger	= au1xpsc_ac97_trigger,
	.hw_params	= au1xpsc_ac97_hw_params,
};

static const struct snd_soc_dai_driver au1xpsc_ac97_dai_template = {
	.playback = {
		.rates		= AC97_RATES,
		.formats	= AC97_FMTS,
		.channels_min	= 2,
		.channels_max	= 2,
	},
	.capture = {
		.rates		= AC97_RATES,
		.formats	= AC97_FMTS,
		.channels_min	= 2,
		.channels_max	= 2,
	},
	.ops = &au1xpsc_ac97_dai_ops,
};

static const struct snd_soc_component_driver au1xpsc_ac97_component = {
	.name			= "au1xpsc-ac97",
	.legacy_dai_naming	= 1,
};

static int au1xpsc_ac97_drvprobe(struct platform_device *pdev)
{
	int ret;
	struct resource *dmares;
	unsigned long sel;
	struct au1xpsc_audio_data *wd;

	wd = devm_kzalloc(&pdev->dev, sizeof(struct au1xpsc_audio_data),
			  GFP_KERNEL);
	if (!wd)
		return -ENOMEM;

	mutex_init(&wd->lock);

	wd->mmio = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wd->mmio))
		return PTR_ERR(wd->mmio);

	dmares = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!dmares)
		return -EBUSY;
	wd->dmaids[SNDRV_PCM_STREAM_PLAYBACK] = dmares->start;

	dmares = platform_get_resource(pdev, IORESOURCE_DMA, 1);
	if (!dmares)
		return -EBUSY;
	wd->dmaids[SNDRV_PCM_STREAM_CAPTURE] = dmares->start;

	 
	wd->cfg = PSC_AC97CFG_RT_FIFO8 | PSC_AC97CFG_TT_FIFO8 |
		  PSC_AC97CFG_DE_ENABLE;

	 
	sel = __raw_readl(PSC_SEL(wd)) & PSC_SEL_CLK_MASK;
	__raw_writel(PSC_CTRL_DISABLE, PSC_CTRL(wd));
	wmb();  
	__raw_writel(0, PSC_SEL(wd));
	wmb();  
	__raw_writel(PSC_SEL_PS_AC97MODE | sel, PSC_SEL(wd));
	wmb();  

	 
	memcpy(&wd->dai_drv, &au1xpsc_ac97_dai_template,
	       sizeof(struct snd_soc_dai_driver));
	wd->dai_drv.name = dev_name(&pdev->dev);

	platform_set_drvdata(pdev, wd);

	ret = snd_soc_set_ac97_ops(&psc_ac97_ops);
	if (ret)
		return ret;

	ret = snd_soc_register_component(&pdev->dev, &au1xpsc_ac97_component,
					 &wd->dai_drv, 1);
	if (ret)
		return ret;

	au1xpsc_ac97_workdata = wd;
	return 0;
}

static void au1xpsc_ac97_drvremove(struct platform_device *pdev)
{
	struct au1xpsc_audio_data *wd = platform_get_drvdata(pdev);

	snd_soc_unregister_component(&pdev->dev);

	 
	__raw_writel(0, AC97_CFG(wd));
	wmb();  
	__raw_writel(PSC_CTRL_DISABLE, PSC_CTRL(wd));
	wmb();  

	au1xpsc_ac97_workdata = NULL;	 
}

#ifdef CONFIG_PM
static int au1xpsc_ac97_drvsuspend(struct device *dev)
{
	struct au1xpsc_audio_data *wd = dev_get_drvdata(dev);

	 
	wd->pm[0] = __raw_readl(PSC_SEL(wd));

	__raw_writel(0, AC97_CFG(wd));
	wmb();  
	__raw_writel(PSC_CTRL_DISABLE, PSC_CTRL(wd));
	wmb();  

	return 0;
}

static int au1xpsc_ac97_drvresume(struct device *dev)
{
	struct au1xpsc_audio_data *wd = dev_get_drvdata(dev);

	 
	__raw_writel(wd->pm[0] | PSC_SEL_PS_AC97MODE, PSC_SEL(wd));
	wmb();  

	 
	return 0;
}

static const struct dev_pm_ops au1xpscac97_pmops = {
	.suspend	= au1xpsc_ac97_drvsuspend,
	.resume		= au1xpsc_ac97_drvresume,
};

#define AU1XPSCAC97_PMOPS &au1xpscac97_pmops

#else

#define AU1XPSCAC97_PMOPS NULL

#endif

static struct platform_driver au1xpsc_ac97_driver = {
	.driver	= {
		.name	= "au1xpsc_ac97",
		.pm	= AU1XPSCAC97_PMOPS,
	},
	.probe		= au1xpsc_ac97_drvprobe,
	.remove_new	= au1xpsc_ac97_drvremove,
};

module_platform_driver(au1xpsc_ac97_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Au12x0/Au1550 PSC AC97 ALSA ASoC audio driver");
MODULE_AUTHOR("Manuel Lauss");

