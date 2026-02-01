

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>

#include "cygnus-ssp.h"

 

#define INTH_R5F_STATUS_OFFSET     0x040
#define INTH_R5F_CLEAR_OFFSET      0x048
#define INTH_R5F_MASK_SET_OFFSET   0x050
#define INTH_R5F_MASK_CLEAR_OFFSET 0x054

#define BF_REARM_FREE_MARK_OFFSET 0x344
#define BF_REARM_FULL_MARK_OFFSET 0x348

 
 
#define SRC_RBUF_0_RDADDR_OFFSET 0x500
#define SRC_RBUF_1_RDADDR_OFFSET 0x518
#define SRC_RBUF_2_RDADDR_OFFSET 0x530
#define SRC_RBUF_3_RDADDR_OFFSET 0x548
#define SRC_RBUF_4_RDADDR_OFFSET 0x560
#define SRC_RBUF_5_RDADDR_OFFSET 0x578
#define SRC_RBUF_6_RDADDR_OFFSET 0x590

 
#define SRC_RBUF_0_WRADDR_OFFSET 0x504
#define SRC_RBUF_1_WRADDR_OFFSET 0x51c
#define SRC_RBUF_2_WRADDR_OFFSET 0x534
#define SRC_RBUF_3_WRADDR_OFFSET 0x54c
#define SRC_RBUF_4_WRADDR_OFFSET 0x564
#define SRC_RBUF_5_WRADDR_OFFSET 0x57c
#define SRC_RBUF_6_WRADDR_OFFSET 0x594

 
#define SRC_RBUF_0_BASEADDR_OFFSET 0x508
#define SRC_RBUF_1_BASEADDR_OFFSET 0x520
#define SRC_RBUF_2_BASEADDR_OFFSET 0x538
#define SRC_RBUF_3_BASEADDR_OFFSET 0x550
#define SRC_RBUF_4_BASEADDR_OFFSET 0x568
#define SRC_RBUF_5_BASEADDR_OFFSET 0x580
#define SRC_RBUF_6_BASEADDR_OFFSET 0x598

 
#define SRC_RBUF_0_ENDADDR_OFFSET 0x50c
#define SRC_RBUF_1_ENDADDR_OFFSET 0x524
#define SRC_RBUF_2_ENDADDR_OFFSET 0x53c
#define SRC_RBUF_3_ENDADDR_OFFSET 0x554
#define SRC_RBUF_4_ENDADDR_OFFSET 0x56c
#define SRC_RBUF_5_ENDADDR_OFFSET 0x584
#define SRC_RBUF_6_ENDADDR_OFFSET 0x59c

 
#define SRC_RBUF_0_FREE_MARK_OFFSET 0x510
#define SRC_RBUF_1_FREE_MARK_OFFSET 0x528
#define SRC_RBUF_2_FREE_MARK_OFFSET 0x540
#define SRC_RBUF_3_FREE_MARK_OFFSET 0x558
#define SRC_RBUF_4_FREE_MARK_OFFSET 0x570
#define SRC_RBUF_5_FREE_MARK_OFFSET 0x588
#define SRC_RBUF_6_FREE_MARK_OFFSET 0x5a0

 
#define DST_RBUF_0_RDADDR_OFFSET 0x5c0
#define DST_RBUF_1_RDADDR_OFFSET 0x5d8
#define DST_RBUF_2_RDADDR_OFFSET 0x5f0
#define DST_RBUF_3_RDADDR_OFFSET 0x608
#define DST_RBUF_4_RDADDR_OFFSET 0x620
#define DST_RBUF_5_RDADDR_OFFSET 0x638

 
#define DST_RBUF_0_WRADDR_OFFSET 0x5c4
#define DST_RBUF_1_WRADDR_OFFSET 0x5dc
#define DST_RBUF_2_WRADDR_OFFSET 0x5f4
#define DST_RBUF_3_WRADDR_OFFSET 0x60c
#define DST_RBUF_4_WRADDR_OFFSET 0x624
#define DST_RBUF_5_WRADDR_OFFSET 0x63c

 
#define DST_RBUF_0_BASEADDR_OFFSET 0x5c8
#define DST_RBUF_1_BASEADDR_OFFSET 0x5e0
#define DST_RBUF_2_BASEADDR_OFFSET 0x5f8
#define DST_RBUF_3_BASEADDR_OFFSET 0x610
#define DST_RBUF_4_BASEADDR_OFFSET 0x628
#define DST_RBUF_5_BASEADDR_OFFSET 0x640

 
#define DST_RBUF_0_ENDADDR_OFFSET 0x5cc
#define DST_RBUF_1_ENDADDR_OFFSET 0x5e4
#define DST_RBUF_2_ENDADDR_OFFSET 0x5fc
#define DST_RBUF_3_ENDADDR_OFFSET 0x614
#define DST_RBUF_4_ENDADDR_OFFSET 0x62c
#define DST_RBUF_5_ENDADDR_OFFSET 0x644

 
#define DST_RBUF_0_FULL_MARK_OFFSET 0x5d0
#define DST_RBUF_1_FULL_MARK_OFFSET 0x5e8
#define DST_RBUF_2_FULL_MARK_OFFSET 0x600
#define DST_RBUF_3_FULL_MARK_OFFSET 0x618
#define DST_RBUF_4_FULL_MARK_OFFSET 0x630
#define DST_RBUF_5_FULL_MARK_OFFSET 0x648
 

 
 
#define ESR0_STATUS_OFFSET 0x900
#define ESR1_STATUS_OFFSET 0x918
#define ESR2_STATUS_OFFSET 0x930
#define ESR3_STATUS_OFFSET 0x948
#define ESR4_STATUS_OFFSET 0x960

 
#define ESR0_STATUS_CLR_OFFSET 0x908
#define ESR1_STATUS_CLR_OFFSET 0x920
#define ESR2_STATUS_CLR_OFFSET 0x938
#define ESR3_STATUS_CLR_OFFSET 0x950
#define ESR4_STATUS_CLR_OFFSET 0x968

 
#define ESR0_MASK_STATUS_OFFSET 0x90c
#define ESR1_MASK_STATUS_OFFSET 0x924
#define ESR2_MASK_STATUS_OFFSET 0x93c
#define ESR3_MASK_STATUS_OFFSET 0x954
#define ESR4_MASK_STATUS_OFFSET 0x96c

 
#define ESR0_MASK_SET_OFFSET 0x910
#define ESR1_MASK_SET_OFFSET 0x928
#define ESR2_MASK_SET_OFFSET 0x940
#define ESR3_MASK_SET_OFFSET 0x958
#define ESR4_MASK_SET_OFFSET 0x970

 
#define ESR0_MASK_CLR_OFFSET 0x914
#define ESR1_MASK_CLR_OFFSET 0x92c
#define ESR2_MASK_CLR_OFFSET 0x944
#define ESR3_MASK_CLR_OFFSET 0x95c
#define ESR4_MASK_CLR_OFFSET 0x974
 

#define R5F_ESR0_SHIFT  0     
#define R5F_ESR1_SHIFT  1     
#define R5F_ESR2_SHIFT  2     
#define R5F_ESR3_SHIFT  3     
#define R5F_ESR4_SHIFT  4     


 
#define ANY_PLAYBACK_IRQ  (BIT(R5F_ESR0_SHIFT) | \
			   BIT(R5F_ESR1_SHIFT) | \
			   BIT(R5F_ESR3_SHIFT))

 
#define ANY_CAPTURE_IRQ   (BIT(R5F_ESR2_SHIFT) | BIT(R5F_ESR4_SHIFT))

 
#define PERIOD_BYTES_MIN 0x100

static const struct snd_pcm_hardware cygnus_pcm_hw = {
	.info = SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_INTERLEAVED,
	.formats = SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S32_LE,

	 
	.period_bytes_min = PERIOD_BYTES_MIN,
	.period_bytes_max = 0x10000,

	 
	.periods_min = 2,
	.periods_max = 8,

	 
	.buffer_bytes_max = 4 * 0x8000,
};

static u64 cygnus_dma_dmamask = DMA_BIT_MASK(32);

static struct cygnus_aio_port *cygnus_dai_get_dma_data(
				struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_runtime = asoc_substream_to_rtd(substream);

	return snd_soc_dai_get_dma_data(asoc_rtd_to_cpu(soc_runtime, 0), substream);
}

static void ringbuf_set_initial(void __iomem *audio_io,
		struct ringbuf_regs *p_rbuf,
		bool is_playback,
		u32 start,
		u32 periodsize,
		u32 bufsize)
{
	u32 initial_rd;
	u32 initial_wr;
	u32 end;
	u32 fmark_val;  

	p_rbuf->period_bytes = periodsize;
	p_rbuf->buf_size = bufsize;

	if (is_playback) {
		 
		initial_rd = start;
		initial_wr = initial_rd ^ BIT(31);
	} else {
		 
		initial_wr = start;
		initial_rd = initial_wr;
	}

	end = start + bufsize - 1;

	 
	fmark_val = periodsize - PERIOD_BYTES_MIN;

	writel(start, audio_io + p_rbuf->baseaddr);
	writel(end, audio_io + p_rbuf->endaddr);
	writel(fmark_val, audio_io + p_rbuf->fmark);
	writel(initial_rd, audio_io + p_rbuf->rdaddr);
	writel(initial_wr, audio_io + p_rbuf->wraddr);
}

static int configure_ringbuf_regs(struct snd_pcm_substream *substream)
{
	struct cygnus_aio_port *aio;
	struct ringbuf_regs *p_rbuf;
	int status = 0;

	aio = cygnus_dai_get_dma_data(substream);

	 
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_rbuf = &aio->play_rb_regs;

		switch (aio->portnum) {
		case 0:
			*p_rbuf = RINGBUF_REG_PLAYBACK(0);
			break;
		case 1:
			*p_rbuf = RINGBUF_REG_PLAYBACK(2);
			break;
		case 2:
			*p_rbuf = RINGBUF_REG_PLAYBACK(4);
			break;
		case 3:  
			*p_rbuf = RINGBUF_REG_PLAYBACK(6);
			break;
		default:
			status = -EINVAL;
		}
	} else {
		p_rbuf = &aio->capture_rb_regs;

		switch (aio->portnum) {
		case 0:
			*p_rbuf = RINGBUF_REG_CAPTURE(0);
			break;
		case 1:
			*p_rbuf = RINGBUF_REG_CAPTURE(2);
			break;
		case 2:
			*p_rbuf = RINGBUF_REG_CAPTURE(4);
			break;
		default:
			status = -EINVAL;
		}
	}

	return status;
}

static struct ringbuf_regs *get_ringbuf(struct snd_pcm_substream *substream)
{
	struct cygnus_aio_port *aio;
	struct ringbuf_regs *p_rbuf = NULL;

	aio = cygnus_dai_get_dma_data(substream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		p_rbuf = &aio->play_rb_regs;
	else
		p_rbuf = &aio->capture_rb_regs;

	return p_rbuf;
}

static void enable_intr(struct snd_pcm_substream *substream)
{
	struct cygnus_aio_port *aio;
	u32 clear_mask;

	aio = cygnus_dai_get_dma_data(substream);

	 
	clear_mask = BIT(aio->portnum);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		 
		writel(clear_mask, aio->cygaud->audio + ESR0_STATUS_CLR_OFFSET);
		writel(clear_mask, aio->cygaud->audio + ESR1_STATUS_CLR_OFFSET);
		writel(clear_mask, aio->cygaud->audio + ESR3_STATUS_CLR_OFFSET);
		 
		writel(clear_mask, aio->cygaud->audio + ESR0_MASK_CLR_OFFSET);
		writel(clear_mask, aio->cygaud->audio + ESR1_MASK_CLR_OFFSET);
		writel(clear_mask, aio->cygaud->audio + ESR3_MASK_CLR_OFFSET);

		writel(ANY_PLAYBACK_IRQ,
			aio->cygaud->audio + INTH_R5F_MASK_CLEAR_OFFSET);
	} else {
		writel(clear_mask, aio->cygaud->audio + ESR2_STATUS_CLR_OFFSET);
		writel(clear_mask, aio->cygaud->audio + ESR4_STATUS_CLR_OFFSET);
		writel(clear_mask, aio->cygaud->audio + ESR2_MASK_CLR_OFFSET);
		writel(clear_mask, aio->cygaud->audio + ESR4_MASK_CLR_OFFSET);

		writel(ANY_CAPTURE_IRQ,
			aio->cygaud->audio + INTH_R5F_MASK_CLEAR_OFFSET);
	}

}

static void disable_intr(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct cygnus_aio_port *aio;
	u32 set_mask;

	aio = cygnus_dai_get_dma_data(substream);

	dev_dbg(asoc_rtd_to_cpu(rtd, 0)->dev, "%s on port %d\n", __func__, aio->portnum);

	 
	set_mask = BIT(aio->portnum);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		 
		writel(set_mask, aio->cygaud->audio + ESR0_MASK_SET_OFFSET);
		writel(set_mask, aio->cygaud->audio + ESR1_MASK_SET_OFFSET);
		writel(set_mask, aio->cygaud->audio + ESR3_MASK_SET_OFFSET);
	} else {
		writel(set_mask, aio->cygaud->audio + ESR2_MASK_SET_OFFSET);
		writel(set_mask, aio->cygaud->audio + ESR4_MASK_SET_OFFSET);
	}

}

static int cygnus_pcm_trigger(struct snd_soc_component *component,
			      struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		enable_intr(substream);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		disable_intr(substream);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void cygnus_pcm_period_elapsed(struct snd_pcm_substream *substream)
{
	struct cygnus_aio_port *aio;
	struct ringbuf_regs *p_rbuf = NULL;
	u32 regval;

	aio = cygnus_dai_get_dma_data(substream);

	p_rbuf = get_ringbuf(substream);

	 
	snd_pcm_period_elapsed(substream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		 
		regval = readl(aio->cygaud->audio + p_rbuf->rdaddr);
		regval = regval ^ BIT(31);
		writel(regval, aio->cygaud->audio + p_rbuf->wraddr);
	} else {
		 
		regval = readl(aio->cygaud->audio + p_rbuf->wraddr);
		writel(regval, aio->cygaud->audio + p_rbuf->rdaddr);
	}
}

 
static void handle_playback_irq(struct cygnus_audio *cygaud)
{
	void __iomem *audio_io;
	u32 port;
	u32 esr_status0, esr_status1, esr_status3;

	audio_io = cygaud->audio;

	 
	esr_status0 = readl(audio_io + ESR0_STATUS_OFFSET);
	esr_status0 &= ~readl(audio_io + ESR0_MASK_STATUS_OFFSET);
	esr_status1 = readl(audio_io + ESR1_STATUS_OFFSET);
	esr_status1 &= ~readl(audio_io + ESR1_MASK_STATUS_OFFSET);
	esr_status3 = readl(audio_io + ESR3_STATUS_OFFSET);
	esr_status3 &= ~readl(audio_io + ESR3_MASK_STATUS_OFFSET);

	for (port = 0; port < CYGNUS_MAX_PLAYBACK_PORTS; port++) {
		u32 esrmask = BIT(port);

		 
		if ((esrmask & esr_status1) || (esrmask & esr_status0)) {
			dev_dbg(cygaud->dev,
				"Underrun: esr0=0x%x, esr1=0x%x esr3=0x%x\n",
				esr_status0, esr_status1, esr_status3);
		}

		 
		if (esrmask & esr_status3) {
			struct snd_pcm_substream *playstr;

			playstr = cygaud->portinfo[port].play_stream;
			cygnus_pcm_period_elapsed(playstr);
		}
	}

	 
	writel(esr_status0, audio_io + ESR0_STATUS_CLR_OFFSET);
	writel(esr_status1, audio_io + ESR1_STATUS_CLR_OFFSET);
	writel(esr_status3, audio_io + ESR3_STATUS_CLR_OFFSET);
	 
	writel(esr_status3, audio_io + BF_REARM_FREE_MARK_OFFSET);
}

 
static void handle_capture_irq(struct cygnus_audio *cygaud)
{
	void __iomem *audio_io;
	u32 port;
	u32 esr_status2, esr_status4;

	audio_io = cygaud->audio;

	 
	esr_status2 = readl(audio_io + ESR2_STATUS_OFFSET);
	esr_status2 &= ~readl(audio_io + ESR2_MASK_STATUS_OFFSET);
	esr_status4 = readl(audio_io + ESR4_STATUS_OFFSET);
	esr_status4 &= ~readl(audio_io + ESR4_MASK_STATUS_OFFSET);

	for (port = 0; port < CYGNUS_MAX_CAPTURE_PORTS; port++) {
		u32 esrmask = BIT(port);

		 
		if (esrmask & esr_status2)
			dev_dbg(cygaud->dev,
				"Overflow: esr2=0x%x\n", esr_status2);

		if (esrmask & esr_status4) {
			struct snd_pcm_substream *capstr;

			capstr = cygaud->portinfo[port].capture_stream;
			cygnus_pcm_period_elapsed(capstr);
		}
	}

	writel(esr_status2, audio_io + ESR2_STATUS_CLR_OFFSET);
	writel(esr_status4, audio_io + ESR4_STATUS_CLR_OFFSET);
	 
	writel(esr_status4, audio_io + BF_REARM_FULL_MARK_OFFSET);
}

static irqreturn_t cygnus_dma_irq(int irq, void *data)
{
	u32 r5_status;
	struct cygnus_audio *cygaud = data;

	 
	r5_status = readl(cygaud->audio + INTH_R5F_STATUS_OFFSET);

	if (!(r5_status & (ANY_PLAYBACK_IRQ | ANY_CAPTURE_IRQ)))
		return IRQ_NONE;

	 
	if (ANY_PLAYBACK_IRQ & r5_status) {
		handle_playback_irq(cygaud);
		writel(ANY_PLAYBACK_IRQ & r5_status,
			cygaud->audio + INTH_R5F_CLEAR_OFFSET);
	}

	 
	if (ANY_CAPTURE_IRQ & r5_status) {
		handle_capture_irq(cygaud);
		writel(ANY_CAPTURE_IRQ & r5_status,
			cygaud->audio + INTH_R5F_CLEAR_OFFSET);
	}

	return IRQ_HANDLED;
}

static int cygnus_pcm_open(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct cygnus_aio_port *aio;
	int ret;

	aio = cygnus_dai_get_dma_data(substream);
	if (!aio)
		return -ENODEV;

	dev_dbg(asoc_rtd_to_cpu(rtd, 0)->dev, "%s port %d\n", __func__, aio->portnum);

	snd_soc_set_runtime_hwparams(substream, &cygnus_pcm_hw);

	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, PERIOD_BYTES_MIN);
	if (ret < 0)
		return ret;

	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, PERIOD_BYTES_MIN);
	if (ret < 0)
		return ret;
	 
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aio->play_stream = substream;
	else
		aio->capture_stream = substream;

	return 0;
}

static int cygnus_pcm_close(struct snd_soc_component *component,
			    struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct cygnus_aio_port *aio;

	aio = cygnus_dai_get_dma_data(substream);

	dev_dbg(asoc_rtd_to_cpu(rtd, 0)->dev, "%s  port %d\n", __func__, aio->portnum);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aio->play_stream = NULL;
	else
		aio->capture_stream = NULL;

	if (!aio->play_stream && !aio->capture_stream)
		dev_dbg(asoc_rtd_to_cpu(rtd, 0)->dev, "freed  port %d\n", aio->portnum);

	return 0;
}

static int cygnus_pcm_prepare(struct snd_soc_component *component,
			      struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct cygnus_aio_port *aio;
	unsigned long bufsize, periodsize;
	bool is_play;
	u32 start;
	struct ringbuf_regs *p_rbuf = NULL;

	aio = cygnus_dai_get_dma_data(substream);
	dev_dbg(asoc_rtd_to_cpu(rtd, 0)->dev, "%s port %d\n", __func__, aio->portnum);

	bufsize = snd_pcm_lib_buffer_bytes(substream);
	periodsize = snd_pcm_lib_period_bytes(substream);

	dev_dbg(asoc_rtd_to_cpu(rtd, 0)->dev, "%s (buf_size %lu) (period_size %lu)\n",
			__func__, bufsize, periodsize);

	configure_ringbuf_regs(substream);

	p_rbuf = get_ringbuf(substream);

	start = runtime->dma_addr;

	is_play = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 1 : 0;

	ringbuf_set_initial(aio->cygaud->audio, p_rbuf, is_play, start,
				periodsize, bufsize);

	return 0;
}

static snd_pcm_uframes_t cygnus_pcm_pointer(struct snd_soc_component *component,
					    struct snd_pcm_substream *substream)
{
	struct cygnus_aio_port *aio;
	unsigned int res = 0, cur = 0, base = 0;
	struct ringbuf_regs *p_rbuf = NULL;

	aio = cygnus_dai_get_dma_data(substream);

	 
	p_rbuf = get_ringbuf(substream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		cur = readl(aio->cygaud->audio + p_rbuf->rdaddr);
	else
		cur = readl(aio->cygaud->audio + p_rbuf->wraddr);

	base = readl(aio->cygaud->audio + p_rbuf->baseaddr);

	 
	res = (cur & 0x7fffffff) - (base & 0x7fffffff);

	return bytes_to_frames(substream->runtime, res);
}

static int cygnus_dma_new(struct snd_soc_component *component,
			  struct snd_soc_pcm_runtime *rtd)
{
	size_t size = cygnus_pcm_hw.buffer_bytes_max;
	struct snd_card *card = rtd->card->snd_card;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &cygnus_dma_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	snd_pcm_set_managed_buffer_all(rtd->pcm, SNDRV_DMA_TYPE_DEV,
				       card->dev, size, size);

	return 0;
}

static struct snd_soc_component_driver cygnus_soc_platform = {
	.open		= cygnus_pcm_open,
	.close		= cygnus_pcm_close,
	.prepare	= cygnus_pcm_prepare,
	.trigger	= cygnus_pcm_trigger,
	.pointer	= cygnus_pcm_pointer,
	.pcm_construct	= cygnus_dma_new,
};

int cygnus_soc_platform_register(struct device *dev,
				 struct cygnus_audio *cygaud)
{
	int rc;

	dev_dbg(dev, "%s Enter\n", __func__);

	rc = devm_request_irq(dev, cygaud->irq_num, cygnus_dma_irq,
				IRQF_SHARED, "cygnus-audio", cygaud);
	if (rc) {
		dev_err(dev, "%s request_irq error %d\n", __func__, rc);
		return rc;
	}

	rc = devm_snd_soc_register_component(dev, &cygnus_soc_platform,
					     NULL, 0);
	if (rc) {
		dev_err(dev, "%s failed\n", __func__);
		return rc;
	}

	return 0;
}

int cygnus_soc_platform_unregister(struct device *dev)
{
	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("Cygnus ASoC PCM module");
