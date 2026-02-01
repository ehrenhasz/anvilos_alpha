
 

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

 
#define BCM2835_I2S_CS_A_REG		0x00
#define BCM2835_I2S_FIFO_A_REG		0x04
#define BCM2835_I2S_MODE_A_REG		0x08
#define BCM2835_I2S_RXC_A_REG		0x0c
#define BCM2835_I2S_TXC_A_REG		0x10
#define BCM2835_I2S_DREQ_A_REG		0x14
#define BCM2835_I2S_INTEN_A_REG	0x18
#define BCM2835_I2S_INTSTC_A_REG	0x1c
#define BCM2835_I2S_GRAY_REG		0x20

 
#define BCM2835_I2S_STBY		BIT(25)
#define BCM2835_I2S_SYNC		BIT(24)
#define BCM2835_I2S_RXSEX		BIT(23)
#define BCM2835_I2S_RXF		BIT(22)
#define BCM2835_I2S_TXE		BIT(21)
#define BCM2835_I2S_RXD		BIT(20)
#define BCM2835_I2S_TXD		BIT(19)
#define BCM2835_I2S_RXR		BIT(18)
#define BCM2835_I2S_TXW		BIT(17)
#define BCM2835_I2S_CS_RXERR		BIT(16)
#define BCM2835_I2S_CS_TXERR		BIT(15)
#define BCM2835_I2S_RXSYNC		BIT(14)
#define BCM2835_I2S_TXSYNC		BIT(13)
#define BCM2835_I2S_DMAEN		BIT(9)
#define BCM2835_I2S_RXTHR(v)		((v) << 7)
#define BCM2835_I2S_TXTHR(v)		((v) << 5)
#define BCM2835_I2S_RXCLR		BIT(4)
#define BCM2835_I2S_TXCLR		BIT(3)
#define BCM2835_I2S_TXON		BIT(2)
#define BCM2835_I2S_RXON		BIT(1)
#define BCM2835_I2S_EN			(1)

#define BCM2835_I2S_CLKDIS		BIT(28)
#define BCM2835_I2S_PDMN		BIT(27)
#define BCM2835_I2S_PDME		BIT(26)
#define BCM2835_I2S_FRXP		BIT(25)
#define BCM2835_I2S_FTXP		BIT(24)
#define BCM2835_I2S_CLKM		BIT(23)
#define BCM2835_I2S_CLKI		BIT(22)
#define BCM2835_I2S_FSM		BIT(21)
#define BCM2835_I2S_FSI		BIT(20)
#define BCM2835_I2S_FLEN(v)		((v) << 10)
#define BCM2835_I2S_FSLEN(v)		(v)

#define BCM2835_I2S_CHWEX		BIT(15)
#define BCM2835_I2S_CHEN		BIT(14)
#define BCM2835_I2S_CHPOS(v)		((v) << 4)
#define BCM2835_I2S_CHWID(v)		(v)
#define BCM2835_I2S_CH1(v)		((v) << 16)
#define BCM2835_I2S_CH2(v)		(v)
#define BCM2835_I2S_CH1_POS(v)		BCM2835_I2S_CH1(BCM2835_I2S_CHPOS(v))
#define BCM2835_I2S_CH2_POS(v)		BCM2835_I2S_CH2(BCM2835_I2S_CHPOS(v))

#define BCM2835_I2S_TX_PANIC(v)	((v) << 24)
#define BCM2835_I2S_RX_PANIC(v)	((v) << 16)
#define BCM2835_I2S_TX(v)		((v) << 8)
#define BCM2835_I2S_RX(v)		(v)

#define BCM2835_I2S_INT_RXERR		BIT(3)
#define BCM2835_I2S_INT_TXERR		BIT(2)
#define BCM2835_I2S_INT_RXR		BIT(1)
#define BCM2835_I2S_INT_TXW		BIT(0)

 
#define BCM2835_I2S_MAX_FRAME_LENGTH	1024

 
struct bcm2835_i2s_dev {
	struct device				*dev;
	struct snd_dmaengine_dai_dma_data	dma_data[2];
	unsigned int				fmt;
	unsigned int				tdm_slots;
	unsigned int				rx_mask;
	unsigned int				tx_mask;
	unsigned int				slot_width;
	unsigned int				frame_length;

	struct regmap				*i2s_regmap;
	struct clk				*clk;
	bool					clk_prepared;
	int					clk_rate;
};

static void bcm2835_i2s_start_clock(struct bcm2835_i2s_dev *dev)
{
	unsigned int provider = dev->fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK;

	if (dev->clk_prepared)
		return;

	switch (provider) {
	case SND_SOC_DAIFMT_BP_FP:
	case SND_SOC_DAIFMT_BP_FC:
		clk_prepare_enable(dev->clk);
		dev->clk_prepared = true;
		break;
	default:
		break;
	}
}

static void bcm2835_i2s_stop_clock(struct bcm2835_i2s_dev *dev)
{
	if (dev->clk_prepared)
		clk_disable_unprepare(dev->clk);
	dev->clk_prepared = false;
}

static void bcm2835_i2s_clear_fifos(struct bcm2835_i2s_dev *dev,
				    bool tx, bool rx)
{
	int timeout = 1000;
	uint32_t syncval;
	uint32_t csreg;
	uint32_t i2s_active_state;
	bool clk_was_prepared;
	uint32_t off;
	uint32_t clr;

	off =  tx ? BCM2835_I2S_TXON : 0;
	off |= rx ? BCM2835_I2S_RXON : 0;

	clr =  tx ? BCM2835_I2S_TXCLR : 0;
	clr |= rx ? BCM2835_I2S_RXCLR : 0;

	 
	regmap_read(dev->i2s_regmap, BCM2835_I2S_CS_A_REG, &csreg);
	i2s_active_state = csreg & (BCM2835_I2S_RXON | BCM2835_I2S_TXON);

	 
	clk_was_prepared = dev->clk_prepared;
	if (!clk_was_prepared)
		bcm2835_i2s_start_clock(dev);

	 
	regmap_update_bits(dev->i2s_regmap, BCM2835_I2S_CS_A_REG, off, 0);

	 
	regmap_update_bits(dev->i2s_regmap, BCM2835_I2S_CS_A_REG, clr, clr);

	 

	 
	regmap_read(dev->i2s_regmap, BCM2835_I2S_CS_A_REG, &syncval);
	syncval &= BCM2835_I2S_SYNC;

	regmap_update_bits(dev->i2s_regmap, BCM2835_I2S_CS_A_REG,
			BCM2835_I2S_SYNC, ~syncval);

	 
	while (--timeout) {
		regmap_read(dev->i2s_regmap, BCM2835_I2S_CS_A_REG, &csreg);
		if ((csreg & BCM2835_I2S_SYNC) != syncval)
			break;
	}

	if (!timeout)
		dev_err(dev->dev, "I2S SYNC error!\n");

	 
	if (!clk_was_prepared)
		bcm2835_i2s_stop_clock(dev);

	 
	regmap_update_bits(dev->i2s_regmap, BCM2835_I2S_CS_A_REG,
			BCM2835_I2S_RXON | BCM2835_I2S_TXON, i2s_active_state);
}

static int bcm2835_i2s_set_dai_fmt(struct snd_soc_dai *dai,
				      unsigned int fmt)
{
	struct bcm2835_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	dev->fmt = fmt;
	return 0;
}

static int bcm2835_i2s_set_dai_bclk_ratio(struct snd_soc_dai *dai,
				      unsigned int ratio)
{
	struct bcm2835_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);

	if (!ratio) {
		dev->tdm_slots = 0;
		return 0;
	}

	if (ratio > BCM2835_I2S_MAX_FRAME_LENGTH)
		return -EINVAL;

	dev->tdm_slots = 2;
	dev->rx_mask = 0x03;
	dev->tx_mask = 0x03;
	dev->slot_width = ratio / 2;
	dev->frame_length = ratio;

	return 0;
}

static int bcm2835_i2s_set_dai_tdm_slot(struct snd_soc_dai *dai,
	unsigned int tx_mask, unsigned int rx_mask,
	int slots, int width)
{
	struct bcm2835_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);

	if (slots) {
		if (slots < 0 || width < 0)
			return -EINVAL;

		 
		rx_mask &= GENMASK(slots - 1, 0);
		tx_mask &= GENMASK(slots - 1, 0);

		 
		if (hweight_long((unsigned long) rx_mask) != 2
		    || hweight_long((unsigned long) tx_mask) != 2)
			return -EINVAL;

		if (slots * width > BCM2835_I2S_MAX_FRAME_LENGTH)
			return -EINVAL;
	}

	dev->tdm_slots = slots;

	dev->rx_mask = rx_mask;
	dev->tx_mask = tx_mask;
	dev->slot_width = width;
	dev->frame_length = slots * width;

	return 0;
}

 
static int bcm2835_i2s_convert_slot(unsigned int slot, unsigned int odd_offset)
{
	if (!odd_offset)
		return slot;

	if (slot & 1)
		return (slot >> 1) + odd_offset;

	return slot >> 1;
}

 
static void bcm2835_i2s_calc_channel_pos(
	unsigned int *ch1_pos, unsigned int *ch2_pos,
	unsigned int mask, unsigned int width,
	unsigned int bit_offset, unsigned int odd_offset)
{
	*ch1_pos = bcm2835_i2s_convert_slot((ffs(mask) - 1), odd_offset)
			* width + bit_offset;
	*ch2_pos = bcm2835_i2s_convert_slot((fls(mask) - 1), odd_offset)
			* width + bit_offset;
}

static int bcm2835_i2s_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct bcm2835_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	unsigned int data_length, data_delay, framesync_length;
	unsigned int slots, slot_width, odd_slot_offset;
	int frame_length, bclk_rate;
	unsigned int rx_mask, tx_mask;
	unsigned int rx_ch1_pos, rx_ch2_pos, tx_ch1_pos, tx_ch2_pos;
	unsigned int mode, format;
	bool bit_clock_provider = false;
	bool frame_sync_provider = false;
	bool frame_start_falling_edge = false;
	uint32_t csreg;
	int ret = 0;

	 
	regmap_read(dev->i2s_regmap, BCM2835_I2S_CS_A_REG, &csreg);

	if (csreg & (BCM2835_I2S_TXON | BCM2835_I2S_RXON))
		return 0;

	data_length = params_width(params);
	data_delay = 0;
	odd_slot_offset = 0;
	mode = 0;

	if (dev->tdm_slots) {
		slots = dev->tdm_slots;
		slot_width = dev->slot_width;
		frame_length = dev->frame_length;
		rx_mask = dev->rx_mask;
		tx_mask = dev->tx_mask;
		bclk_rate = dev->frame_length * params_rate(params);
	} else {
		slots = 2;
		slot_width = params_width(params);
		rx_mask = 0x03;
		tx_mask = 0x03;

		frame_length = snd_soc_params_to_frame_size(params);
		if (frame_length < 0)
			return frame_length;

		bclk_rate = snd_soc_params_to_bclk(params);
		if (bclk_rate < 0)
			return bclk_rate;
	}

	 
	if (data_length > slot_width)
		return -EINVAL;

	 
	switch (dev->fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
	case SND_SOC_DAIFMT_BP_FP:
	case SND_SOC_DAIFMT_BP_FC:
		bit_clock_provider = true;
		break;
	case SND_SOC_DAIFMT_BC_FP:
	case SND_SOC_DAIFMT_BC_FC:
		bit_clock_provider = false;
		break;
	default:
		return -EINVAL;
	}

	 
	switch (dev->fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
	case SND_SOC_DAIFMT_BP_FP:
	case SND_SOC_DAIFMT_BC_FP:
		frame_sync_provider = true;
		break;
	case SND_SOC_DAIFMT_BP_FC:
	case SND_SOC_DAIFMT_BC_FC:
		frame_sync_provider = false;
		break;
	default:
		return -EINVAL;
	}

	 
	if (bit_clock_provider &&
	    (!dev->clk_prepared || dev->clk_rate != bclk_rate)) {
		if (dev->clk_prepared)
			bcm2835_i2s_stop_clock(dev);

		if (dev->clk_rate != bclk_rate) {
			ret = clk_set_rate(dev->clk, bclk_rate);
			if (ret)
				return ret;
			dev->clk_rate = bclk_rate;
		}

		bcm2835_i2s_start_clock(dev);
	}

	 
	format = BCM2835_I2S_CHEN;

	if (data_length >= 24)
		format |= BCM2835_I2S_CHWEX;

	format |= BCM2835_I2S_CHWID((data_length-8)&0xf);

	 
	format = BCM2835_I2S_CH1(format) | BCM2835_I2S_CH2(format);

	switch (dev->fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		 
		if (slots & 1)
			return -EINVAL;

		 
		odd_slot_offset = slots >> 1;

		 
		data_delay = 1;

		 
		framesync_length = frame_length / 2;
		frame_start_falling_edge = true;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		if (slots & 1)
			return -EINVAL;

		odd_slot_offset = slots >> 1;
		data_delay = 0;
		framesync_length = frame_length / 2;
		frame_start_falling_edge = false;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		if (slots & 1)
			return -EINVAL;

		 
		if (frame_length & 1)
			return -EINVAL;

		odd_slot_offset = slots >> 1;
		data_delay = slot_width - data_length;
		framesync_length = frame_length / 2;
		frame_start_falling_edge = false;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		data_delay = 1;
		framesync_length = 1;
		frame_start_falling_edge = false;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		data_delay = 0;
		framesync_length = 1;
		frame_start_falling_edge = false;
		break;
	default:
		return -EINVAL;
	}

	bcm2835_i2s_calc_channel_pos(&rx_ch1_pos, &rx_ch2_pos,
		rx_mask, slot_width, data_delay, odd_slot_offset);
	bcm2835_i2s_calc_channel_pos(&tx_ch1_pos, &tx_ch2_pos,
		tx_mask, slot_width, data_delay, odd_slot_offset);

	 
	if ((!rx_ch1_pos || !tx_ch1_pos) && !frame_sync_provider)
		dev_warn(dev->dev,
			"Unstable consumer config detected, L/R may be swapped");

	 
	regmap_write(dev->i2s_regmap, BCM2835_I2S_RXC_A_REG, 
		  format
		| BCM2835_I2S_CH1_POS(rx_ch1_pos)
		| BCM2835_I2S_CH2_POS(rx_ch2_pos));
	regmap_write(dev->i2s_regmap, BCM2835_I2S_TXC_A_REG, 
		  format
		| BCM2835_I2S_CH1_POS(tx_ch1_pos)
		| BCM2835_I2S_CH2_POS(tx_ch2_pos));

	 

	if (data_length <= 16) {
		 
		mode |= BCM2835_I2S_FTXP | BCM2835_I2S_FRXP;
	}

	mode |= BCM2835_I2S_FLEN(frame_length - 1);
	mode |= BCM2835_I2S_FSLEN(framesync_length);

	 
	if (!bit_clock_provider)
		mode |= BCM2835_I2S_CLKM;

	 
	if (!frame_sync_provider)
		mode |= BCM2835_I2S_FSM;

	 
        switch (dev->fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
	case SND_SOC_DAIFMT_NB_IF:
		mode |= BCM2835_I2S_CLKI;
		break;
	case SND_SOC_DAIFMT_IB_NF:
	case SND_SOC_DAIFMT_IB_IF:
		break;
	default:
		return -EINVAL;
	}

	 
	switch (dev->fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
	case SND_SOC_DAIFMT_IB_NF:
		if (frame_start_falling_edge)
			mode |= BCM2835_I2S_FSI;
		break;
	case SND_SOC_DAIFMT_NB_IF:
	case SND_SOC_DAIFMT_IB_IF:
		if (!frame_start_falling_edge)
			mode |= BCM2835_I2S_FSI;
		break;
	default:
		return -EINVAL;
	}

	regmap_write(dev->i2s_regmap, BCM2835_I2S_MODE_A_REG, mode);

	 
	regmap_update_bits(dev->i2s_regmap, BCM2835_I2S_CS_A_REG,
			BCM2835_I2S_RXTHR(1)
			| BCM2835_I2S_TXTHR(1)
			| BCM2835_I2S_DMAEN, 0xffffffff);

	regmap_update_bits(dev->i2s_regmap, BCM2835_I2S_DREQ_A_REG,
			  BCM2835_I2S_TX_PANIC(0x10)
			| BCM2835_I2S_RX_PANIC(0x30)
			| BCM2835_I2S_TX(0x30)
			| BCM2835_I2S_RX(0x20), 0xffffffff);

	 
	bcm2835_i2s_clear_fifos(dev, true, true);

	dev_dbg(dev->dev,
		"slots: %d width: %d rx mask: 0x%02x tx_mask: 0x%02x\n",
		slots, slot_width, rx_mask, tx_mask);

	dev_dbg(dev->dev, "frame len: %d sync len: %d data len: %d\n",
		frame_length, framesync_length, data_length);

	dev_dbg(dev->dev, "rx pos: %d,%d tx pos: %d,%d\n",
		rx_ch1_pos, rx_ch2_pos, tx_ch1_pos, tx_ch2_pos);

	dev_dbg(dev->dev, "sampling rate: %d bclk rate: %d\n",
		params_rate(params), bclk_rate);

	dev_dbg(dev->dev, "CLKM: %d CLKI: %d FSM: %d FSI: %d frame start: %s edge\n",
		!!(mode & BCM2835_I2S_CLKM),
		!!(mode & BCM2835_I2S_CLKI),
		!!(mode & BCM2835_I2S_FSM),
		!!(mode & BCM2835_I2S_FSI),
		(mode & BCM2835_I2S_FSI) ? "falling" : "rising");

	return ret;
}

static int bcm2835_i2s_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct bcm2835_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	uint32_t cs_reg;

	 
	regmap_read(dev->i2s_regmap, BCM2835_I2S_CS_A_REG, &cs_reg);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK
			&& !(cs_reg & BCM2835_I2S_TXE))
		bcm2835_i2s_clear_fifos(dev, true, false);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE
			&& (cs_reg & BCM2835_I2S_RXD))
		bcm2835_i2s_clear_fifos(dev, false, true);

	return 0;
}

static void bcm2835_i2s_stop(struct bcm2835_i2s_dev *dev,
		struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	uint32_t mask;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		mask = BCM2835_I2S_RXON;
	else
		mask = BCM2835_I2S_TXON;

	regmap_update_bits(dev->i2s_regmap,
			BCM2835_I2S_CS_A_REG, mask, 0);

	 
	if (!snd_soc_dai_active(dai) && !(dev->fmt & SND_SOC_DAIFMT_CONT))
		bcm2835_i2s_stop_clock(dev);
}

static int bcm2835_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *dai)
{
	struct bcm2835_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);
	uint32_t mask;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		bcm2835_i2s_start_clock(dev);

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			mask = BCM2835_I2S_RXON;
		else
			mask = BCM2835_I2S_TXON;

		regmap_update_bits(dev->i2s_regmap,
				BCM2835_I2S_CS_A_REG, mask, mask);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		bcm2835_i2s_stop(dev, substream, dai);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bcm2835_i2s_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct bcm2835_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);

	if (snd_soc_dai_active(dai))
		return 0;

	 
	bcm2835_i2s_stop_clock(dev);

	 
	regmap_update_bits(dev->i2s_regmap, BCM2835_I2S_CS_A_REG,
			BCM2835_I2S_EN, BCM2835_I2S_EN);

	 
	regmap_update_bits(dev->i2s_regmap, BCM2835_I2S_CS_A_REG,
			BCM2835_I2S_STBY, BCM2835_I2S_STBY);

	return 0;
}

static void bcm2835_i2s_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct bcm2835_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);

	bcm2835_i2s_stop(dev, substream, dai);

	 
	if (snd_soc_dai_active(dai))
		return;

	 
	regmap_update_bits(dev->i2s_regmap, BCM2835_I2S_CS_A_REG,
			BCM2835_I2S_EN, 0);

	 
	bcm2835_i2s_stop_clock(dev);
}

static int bcm2835_i2s_dai_probe(struct snd_soc_dai *dai)
{
	struct bcm2835_i2s_dev *dev = snd_soc_dai_get_drvdata(dai);

	snd_soc_dai_init_dma_data(dai,
				  &dev->dma_data[SNDRV_PCM_STREAM_PLAYBACK],
				  &dev->dma_data[SNDRV_PCM_STREAM_CAPTURE]);

	return 0;
}

static const struct snd_soc_dai_ops bcm2835_i2s_dai_ops = {
	.probe		= bcm2835_i2s_dai_probe,
	.startup	= bcm2835_i2s_startup,
	.shutdown	= bcm2835_i2s_shutdown,
	.prepare	= bcm2835_i2s_prepare,
	.trigger	= bcm2835_i2s_trigger,
	.hw_params	= bcm2835_i2s_hw_params,
	.set_fmt	= bcm2835_i2s_set_dai_fmt,
	.set_bclk_ratio	= bcm2835_i2s_set_dai_bclk_ratio,
	.set_tdm_slot	= bcm2835_i2s_set_dai_tdm_slot,
};

static struct snd_soc_dai_driver bcm2835_i2s_dai = {
	.name	= "bcm2835-i2s",
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates =	SNDRV_PCM_RATE_CONTINUOUS,
		.rate_min =	8000,
		.rate_max =	384000,
		.formats =	SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE
		},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates =	SNDRV_PCM_RATE_CONTINUOUS,
		.rate_min =	8000,
		.rate_max =	384000,
		.formats =	SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE
		},
	.ops = &bcm2835_i2s_dai_ops,
	.symmetric_rate = 1,
	.symmetric_sample_bits = 1,
};

static bool bcm2835_i2s_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case BCM2835_I2S_CS_A_REG:
	case BCM2835_I2S_FIFO_A_REG:
	case BCM2835_I2S_INTSTC_A_REG:
	case BCM2835_I2S_GRAY_REG:
		return true;
	default:
		return false;
	}
}

static bool bcm2835_i2s_precious_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case BCM2835_I2S_FIFO_A_REG:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config bcm2835_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = BCM2835_I2S_GRAY_REG,
	.precious_reg = bcm2835_i2s_precious_reg,
	.volatile_reg = bcm2835_i2s_volatile_reg,
	.cache_type = REGCACHE_RBTREE,
};

static const struct snd_soc_component_driver bcm2835_i2s_component = {
	.name			= "bcm2835-i2s-comp",
	.legacy_dai_naming	= 1,
};

static int bcm2835_i2s_probe(struct platform_device *pdev)
{
	struct bcm2835_i2s_dev *dev;
	int ret;
	void __iomem *base;
	const __be32 *addr;
	dma_addr_t dma_base;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev),
			   GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	 
	dev->clk_prepared = false;
	dev->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(dev->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(dev->clk),
				     "could not get clk\n");

	 
	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	dev->i2s_regmap = devm_regmap_init_mmio(&pdev->dev, base,
				&bcm2835_regmap_config);
	if (IS_ERR(dev->i2s_regmap))
		return PTR_ERR(dev->i2s_regmap);

	 
	addr = of_get_address(pdev->dev.of_node, 0, NULL, NULL);
	if (!addr) {
		dev_err(&pdev->dev, "could not get DMA-register address\n");
		return -EINVAL;
	}
	dma_base = be32_to_cpup(addr);

	dev->dma_data[SNDRV_PCM_STREAM_PLAYBACK].addr =
		dma_base + BCM2835_I2S_FIFO_A_REG;

	dev->dma_data[SNDRV_PCM_STREAM_CAPTURE].addr =
		dma_base + BCM2835_I2S_FIFO_A_REG;

	 
	dev->dma_data[SNDRV_PCM_STREAM_PLAYBACK].addr_width =
		DMA_SLAVE_BUSWIDTH_4_BYTES;
	dev->dma_data[SNDRV_PCM_STREAM_CAPTURE].addr_width =
		DMA_SLAVE_BUSWIDTH_4_BYTES;

	 
	dev->dma_data[SNDRV_PCM_STREAM_PLAYBACK].maxburst = 2;
	dev->dma_data[SNDRV_PCM_STREAM_CAPTURE].maxburst = 2;

	 
	dev->dma_data[SNDRV_PCM_STREAM_PLAYBACK].flags =
		SND_DMAENGINE_PCM_DAI_FLAG_PACK;
	dev->dma_data[SNDRV_PCM_STREAM_CAPTURE].flags =
		SND_DMAENGINE_PCM_DAI_FLAG_PACK;

	 
	dev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, dev);

	ret = devm_snd_soc_register_component(&pdev->dev,
			&bcm2835_i2s_component, &bcm2835_i2s_dai, 1);
	if (ret) {
		dev_err(&pdev->dev, "Could not register DAI: %d\n", ret);
		return ret;
	}

	ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL, 0);
	if (ret) {
		dev_err(&pdev->dev, "Could not register PCM: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct of_device_id bcm2835_i2s_of_match[] = {
	{ .compatible = "brcm,bcm2835-i2s", },
	{},
};

MODULE_DEVICE_TABLE(of, bcm2835_i2s_of_match);

static struct platform_driver bcm2835_i2s_driver = {
	.probe		= bcm2835_i2s_probe,
	.driver		= {
		.name	= "bcm2835-i2s",
		.of_match_table = bcm2835_i2s_of_match,
	},
};

module_platform_driver(bcm2835_i2s_driver);

MODULE_ALIAS("platform:bcm2835-i2s");
MODULE_DESCRIPTION("BCM2835 I2S interface");
MODULE_AUTHOR("Florian Meier <florian.meier@koalo.de>");
MODULE_LICENSE("GPL v2");
