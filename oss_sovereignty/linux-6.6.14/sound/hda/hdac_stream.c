
 

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/clocksource.h>
#include <sound/compress_driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/hdaudio.h>
#include <sound/hda_register.h>
#include "trace.h"

 

 
int snd_hdac_get_stream_stripe_ctl(struct hdac_bus *bus,
				   struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int channels = runtime->channels,
		     rate = runtime->rate,
		     bits_per_sample = runtime->sample_bits,
		     max_sdo_lines, value, sdo_line;

	 
	max_sdo_lines = snd_hdac_chip_readl(bus, GCAP) & AZX_GCAP_NSDO;

	 
	for (sdo_line = max_sdo_lines; sdo_line > 0; sdo_line >>= 1) {
		if (rate > 48000)
			value = (channels * bits_per_sample *
					(rate / 48000)) / sdo_line;
		else
			value = (channels * bits_per_sample) / sdo_line;

		if (value >= bus->sdo_limit)
			break;
	}

	 
	return sdo_line >> 1;
}
EXPORT_SYMBOL_GPL(snd_hdac_get_stream_stripe_ctl);

 
void snd_hdac_stream_init(struct hdac_bus *bus, struct hdac_stream *azx_dev,
			  int idx, int direction, int tag)
{
	azx_dev->bus = bus;
	 
	azx_dev->sd_addr = bus->remap_addr + (0x20 * idx + 0x80);
	 
	azx_dev->sd_int_sta_mask = 1 << idx;
	azx_dev->index = idx;
	azx_dev->direction = direction;
	azx_dev->stream_tag = tag;
	snd_hdac_dsp_lock_init(azx_dev);
	list_add_tail(&azx_dev->list, &bus->stream_list);

	if (bus->spbcap) {
		azx_dev->spib_addr = bus->spbcap + AZX_SPB_BASE +
					AZX_SPB_INTERVAL * idx +
					AZX_SPB_SPIB;

		azx_dev->fifo_addr = bus->spbcap + AZX_SPB_BASE +
					AZX_SPB_INTERVAL * idx +
					AZX_SPB_MAXFIFO;
	}

	if (bus->drsmcap)
		azx_dev->dpibr_addr = bus->drsmcap + AZX_DRSM_BASE +
					AZX_DRSM_INTERVAL * idx;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_init);

 
void snd_hdac_stream_start(struct hdac_stream *azx_dev)
{
	struct hdac_bus *bus = azx_dev->bus;
	int stripe_ctl;

	trace_snd_hdac_stream_start(bus, azx_dev);

	azx_dev->start_wallclk = snd_hdac_chip_readl(bus, WALLCLK);

	 
	snd_hdac_chip_updatel(bus, INTCTL,
			      1 << azx_dev->index,
			      1 << azx_dev->index);
	 
	if (azx_dev->stripe) {
		if (azx_dev->substream)
			stripe_ctl = snd_hdac_get_stream_stripe_ctl(bus, azx_dev->substream);
		else
			stripe_ctl = 0;
		snd_hdac_stream_updateb(azx_dev, SD_CTL_3B, SD_CTL_STRIPE_MASK,
					stripe_ctl);
	}
	 
	if (bus->access_sdnctl_in_dword)
		snd_hdac_stream_updatel(azx_dev, SD_CTL,
				0, SD_CTL_DMA_START | SD_INT_MASK);
	else
		snd_hdac_stream_updateb(azx_dev, SD_CTL,
				0, SD_CTL_DMA_START | SD_INT_MASK);
	azx_dev->running = true;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_start);

 
static void snd_hdac_stream_clear(struct hdac_stream *azx_dev)
{
	snd_hdac_stream_updateb(azx_dev, SD_CTL,
				SD_CTL_DMA_START | SD_INT_MASK, 0);
	snd_hdac_stream_writeb(azx_dev, SD_STS, SD_INT_MASK);  
	if (azx_dev->stripe)
		snd_hdac_stream_updateb(azx_dev, SD_CTL_3B, SD_CTL_STRIPE_MASK, 0);
	azx_dev->running = false;
}

 
void snd_hdac_stream_stop(struct hdac_stream *azx_dev)
{
	trace_snd_hdac_stream_stop(azx_dev->bus, azx_dev);

	snd_hdac_stream_clear(azx_dev);
	 
	snd_hdac_chip_updatel(azx_dev->bus, INTCTL, 1 << azx_dev->index, 0);
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_stop);

 
void snd_hdac_stop_streams(struct hdac_bus *bus)
{
	struct hdac_stream *stream;

	list_for_each_entry(stream, &bus->stream_list, list)
		snd_hdac_stream_stop(stream);
}
EXPORT_SYMBOL_GPL(snd_hdac_stop_streams);

 
void snd_hdac_stop_streams_and_chip(struct hdac_bus *bus)
{

	if (bus->chip_init) {
		snd_hdac_stop_streams(bus);
		snd_hdac_bus_stop_chip(bus);
	}
}
EXPORT_SYMBOL_GPL(snd_hdac_stop_streams_and_chip);

 
void snd_hdac_stream_reset(struct hdac_stream *azx_dev)
{
	unsigned char val;
	int dma_run_state;

	snd_hdac_stream_clear(azx_dev);

	dma_run_state = snd_hdac_stream_readb(azx_dev, SD_CTL) & SD_CTL_DMA_START;

	snd_hdac_stream_updateb(azx_dev, SD_CTL, 0, SD_CTL_STREAM_RESET);

	 
	snd_hdac_stream_readb_poll(azx_dev, SD_CTL, val, (val & SD_CTL_STREAM_RESET), 3, 300);

	if (azx_dev->bus->dma_stop_delay && dma_run_state)
		udelay(azx_dev->bus->dma_stop_delay);

	snd_hdac_stream_updateb(azx_dev, SD_CTL, SD_CTL_STREAM_RESET, 0);

	 
	snd_hdac_stream_readb_poll(azx_dev, SD_CTL, val, !(val & SD_CTL_STREAM_RESET), 3, 300);

	 
	if (azx_dev->posbuf)
		*azx_dev->posbuf = 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_reset);

 
int snd_hdac_stream_setup(struct hdac_stream *azx_dev)
{
	struct hdac_bus *bus = azx_dev->bus;
	struct snd_pcm_runtime *runtime;
	unsigned int val;

	if (azx_dev->substream)
		runtime = azx_dev->substream->runtime;
	else
		runtime = NULL;
	 
	snd_hdac_stream_clear(azx_dev);
	 
	val = snd_hdac_stream_readl(azx_dev, SD_CTL);
	val = (val & ~SD_CTL_STREAM_TAG_MASK) |
		(azx_dev->stream_tag << SD_CTL_STREAM_TAG_SHIFT);
	if (!bus->snoop)
		val |= SD_CTL_TRAFFIC_PRIO;
	snd_hdac_stream_writel(azx_dev, SD_CTL, val);

	 
	snd_hdac_stream_writel(azx_dev, SD_CBL, azx_dev->bufsize);

	 
	 
	snd_hdac_stream_writew(azx_dev, SD_FORMAT, azx_dev->format_val);

	 
	snd_hdac_stream_writew(azx_dev, SD_LVI, azx_dev->frags - 1);

	 
	 
	snd_hdac_stream_writel(azx_dev, SD_BDLPL, (u32)azx_dev->bdl.addr);
	 
	snd_hdac_stream_writel(azx_dev, SD_BDLPU,
			       upper_32_bits(azx_dev->bdl.addr));

	 
	if (bus->use_posbuf && bus->posbuf.addr) {
		if (!(snd_hdac_chip_readl(bus, DPLBASE) & AZX_DPLBASE_ENABLE))
			snd_hdac_chip_writel(bus, DPLBASE,
				(u32)bus->posbuf.addr | AZX_DPLBASE_ENABLE);
	}

	 
	snd_hdac_stream_updatel(azx_dev, SD_CTL, 0, SD_INT_MASK);

	azx_dev->fifo_size = snd_hdac_stream_readw(azx_dev, SD_FIFOSIZE) + 1;

	 
	if (runtime && runtime->period_size > 64)
		azx_dev->delay_negative_threshold =
			-frames_to_bytes(runtime, 64);
	else
		azx_dev->delay_negative_threshold = 0;

	 
	if (runtime)
		azx_dev->period_wallclk = (((runtime->period_size * 24000) /
				    runtime->rate) * 1000);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_setup);

 
void snd_hdac_stream_cleanup(struct hdac_stream *azx_dev)
{
	snd_hdac_stream_writel(azx_dev, SD_BDLPL, 0);
	snd_hdac_stream_writel(azx_dev, SD_BDLPU, 0);
	snd_hdac_stream_writel(azx_dev, SD_CTL, 0);
	azx_dev->bufsize = 0;
	azx_dev->period_bytes = 0;
	azx_dev->format_val = 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_cleanup);

 
struct hdac_stream *snd_hdac_stream_assign(struct hdac_bus *bus,
					   struct snd_pcm_substream *substream)
{
	struct hdac_stream *azx_dev;
	struct hdac_stream *res = NULL;

	 
	int key = (substream->number << 2) | (substream->stream + 1);

	if (substream->pcm)
		key |= (substream->pcm->device << 16);

	spin_lock_irq(&bus->reg_lock);
	list_for_each_entry(azx_dev, &bus->stream_list, list) {
		if (azx_dev->direction != substream->stream)
			continue;
		if (azx_dev->opened)
			continue;
		if (azx_dev->assigned_key == key) {
			res = azx_dev;
			break;
		}
		if (!res || bus->reverse_assign)
			res = azx_dev;
	}
	if (res) {
		res->opened = 1;
		res->running = 0;
		res->assigned_key = key;
		res->substream = substream;
	}
	spin_unlock_irq(&bus->reg_lock);
	return res;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_assign);

 
void snd_hdac_stream_release_locked(struct hdac_stream *azx_dev)
{
	azx_dev->opened = 0;
	azx_dev->running = 0;
	azx_dev->substream = NULL;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_release_locked);

 
void snd_hdac_stream_release(struct hdac_stream *azx_dev)
{
	struct hdac_bus *bus = azx_dev->bus;

	spin_lock_irq(&bus->reg_lock);
	snd_hdac_stream_release_locked(azx_dev);
	spin_unlock_irq(&bus->reg_lock);
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_release);

 
struct hdac_stream *snd_hdac_get_stream(struct hdac_bus *bus,
					int dir, int stream_tag)
{
	struct hdac_stream *s;

	list_for_each_entry(s, &bus->stream_list, list) {
		if (s->direction == dir && s->stream_tag == stream_tag)
			return s;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(snd_hdac_get_stream);

 
static int setup_bdle(struct hdac_bus *bus,
		      struct snd_dma_buffer *dmab,
		      struct hdac_stream *azx_dev, __le32 **bdlp,
		      int ofs, int size, int with_ioc)
{
	__le32 *bdl = *bdlp;

	while (size > 0) {
		dma_addr_t addr;
		int chunk;

		if (azx_dev->frags >= AZX_MAX_BDL_ENTRIES)
			return -EINVAL;

		addr = snd_sgbuf_get_addr(dmab, ofs);
		 
		bdl[0] = cpu_to_le32((u32)addr);
		bdl[1] = cpu_to_le32(upper_32_bits(addr));
		 
		chunk = snd_sgbuf_get_chunk_size(dmab, ofs, size);
		 
		if (bus->align_bdle_4k) {
			u32 remain = 0x1000 - (ofs & 0xfff);

			if (chunk > remain)
				chunk = remain;
		}
		bdl[2] = cpu_to_le32(chunk);
		 
		size -= chunk;
		bdl[3] = (size || !with_ioc) ? 0 : cpu_to_le32(0x01);
		bdl += 4;
		azx_dev->frags++;
		ofs += chunk;
	}
	*bdlp = bdl;
	return ofs;
}

 
int snd_hdac_stream_setup_periods(struct hdac_stream *azx_dev)
{
	struct hdac_bus *bus = azx_dev->bus;
	struct snd_pcm_substream *substream = azx_dev->substream;
	struct snd_compr_stream *cstream = azx_dev->cstream;
	struct snd_pcm_runtime *runtime = NULL;
	struct snd_dma_buffer *dmab;
	__le32 *bdl;
	int i, ofs, periods, period_bytes;
	int pos_adj, pos_align;

	if (substream) {
		runtime = substream->runtime;
		dmab = snd_pcm_get_dma_buf(substream);
	} else if (cstream) {
		dmab = snd_pcm_get_dma_buf(cstream);
	} else {
		WARN(1, "No substream or cstream assigned\n");
		return -EINVAL;
	}

	 
	snd_hdac_stream_writel(azx_dev, SD_BDLPL, 0);
	snd_hdac_stream_writel(azx_dev, SD_BDLPU, 0);

	period_bytes = azx_dev->period_bytes;
	periods = azx_dev->bufsize / period_bytes;

	 
	bdl = (__le32 *)azx_dev->bdl.area;
	ofs = 0;
	azx_dev->frags = 0;

	pos_adj = bus->bdl_pos_adj;
	if (runtime && !azx_dev->no_period_wakeup && pos_adj > 0) {
		pos_align = pos_adj;
		pos_adj = DIV_ROUND_UP(pos_adj * runtime->rate, 48000);
		if (!pos_adj)
			pos_adj = pos_align;
		else
			pos_adj = roundup(pos_adj, pos_align);
		pos_adj = frames_to_bytes(runtime, pos_adj);
		if (pos_adj >= period_bytes) {
			dev_warn(bus->dev, "Too big adjustment %d\n",
				 pos_adj);
			pos_adj = 0;
		} else {
			ofs = setup_bdle(bus, dmab, azx_dev,
					 &bdl, ofs, pos_adj, true);
			if (ofs < 0)
				goto error;
		}
	} else
		pos_adj = 0;

	for (i = 0; i < periods; i++) {
		if (i == periods - 1 && pos_adj)
			ofs = setup_bdle(bus, dmab, azx_dev,
					 &bdl, ofs, period_bytes - pos_adj, 0);
		else
			ofs = setup_bdle(bus, dmab, azx_dev,
					 &bdl, ofs, period_bytes,
					 !azx_dev->no_period_wakeup);
		if (ofs < 0)
			goto error;
	}
	return 0;

 error:
	dev_err(bus->dev, "Too many BDL entries: buffer=%d, period=%d\n",
		azx_dev->bufsize, period_bytes);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_setup_periods);

 
int snd_hdac_stream_set_params(struct hdac_stream *azx_dev,
				 unsigned int format_val)
{
	struct snd_pcm_substream *substream = azx_dev->substream;
	struct snd_compr_stream *cstream = azx_dev->cstream;
	unsigned int bufsize, period_bytes;
	unsigned int no_period_wakeup;
	int err;

	if (substream) {
		bufsize = snd_pcm_lib_buffer_bytes(substream);
		period_bytes = snd_pcm_lib_period_bytes(substream);
		no_period_wakeup = substream->runtime->no_period_wakeup;
	} else if (cstream) {
		bufsize = cstream->runtime->buffer_size;
		period_bytes = cstream->runtime->fragment_size;
		no_period_wakeup = 0;
	} else {
		return -EINVAL;
	}

	if (bufsize != azx_dev->bufsize ||
	    period_bytes != azx_dev->period_bytes ||
	    format_val != azx_dev->format_val ||
	    no_period_wakeup != azx_dev->no_period_wakeup) {
		azx_dev->bufsize = bufsize;
		azx_dev->period_bytes = period_bytes;
		azx_dev->format_val = format_val;
		azx_dev->no_period_wakeup = no_period_wakeup;
		err = snd_hdac_stream_setup_periods(azx_dev);
		if (err < 0)
			return err;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_set_params);

static u64 azx_cc_read(const struct cyclecounter *cc)
{
	struct hdac_stream *azx_dev = container_of(cc, struct hdac_stream, cc);

	return snd_hdac_chip_readl(azx_dev->bus, WALLCLK);
}

static void azx_timecounter_init(struct hdac_stream *azx_dev,
				 bool force, u64 last)
{
	struct timecounter *tc = &azx_dev->tc;
	struct cyclecounter *cc = &azx_dev->cc;
	u64 nsec;

	cc->read = azx_cc_read;
	cc->mask = CLOCKSOURCE_MASK(32);

	 
	clocks_calc_mult_shift(&cc->mult, &cc->shift, 24000000,
			       NSEC_PER_SEC, 178);

	nsec = 0;  
	timecounter_init(tc, cc, nsec);
	if (force) {
		 
		tc->cycle_last = last;
	}
}

 
void snd_hdac_stream_timecounter_init(struct hdac_stream *azx_dev,
				      unsigned int streams)
{
	struct hdac_bus *bus = azx_dev->bus;
	struct snd_pcm_runtime *runtime = azx_dev->substream->runtime;
	struct hdac_stream *s;
	bool inited = false;
	u64 cycle_last = 0;
	int i = 0;

	list_for_each_entry(s, &bus->stream_list, list) {
		if (streams & (1 << i)) {
			azx_timecounter_init(s, inited, cycle_last);
			if (!inited) {
				inited = true;
				cycle_last = s->tc.cycle_last;
			}
		}
		i++;
	}

	snd_pcm_gettime(runtime, &runtime->trigger_tstamp);
	runtime->trigger_tstamp_latched = true;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_timecounter_init);

 
void snd_hdac_stream_sync_trigger(struct hdac_stream *azx_dev, bool set,
				  unsigned int streams, unsigned int reg)
{
	struct hdac_bus *bus = azx_dev->bus;
	unsigned int val;

	if (!reg)
		reg = AZX_REG_SSYNC;
	val = _snd_hdac_chip_readl(bus, reg);
	if (set)
		val |= streams;
	else
		val &= ~streams;
	_snd_hdac_chip_writel(bus, reg, val);
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_sync_trigger);

 
void snd_hdac_stream_sync(struct hdac_stream *azx_dev, bool start,
			  unsigned int streams)
{
	struct hdac_bus *bus = azx_dev->bus;
	int i, nwait, timeout;
	struct hdac_stream *s;

	for (timeout = 5000; timeout; timeout--) {
		nwait = 0;
		i = 0;
		list_for_each_entry(s, &bus->stream_list, list) {
			if (!(streams & (1 << i++)))
				continue;

			if (start) {
				 
				if (!(snd_hdac_stream_readb(s, SD_STS) &
				      SD_STS_FIFO_READY))
					nwait++;
			} else {
				 
				if (snd_hdac_stream_readb(s, SD_CTL) &
				    SD_CTL_DMA_START) {
					nwait++;
					 
					if (timeout == 1)
						snd_hdac_stream_reset(s);
				}
			}
		}
		if (!nwait)
			break;
		cpu_relax();
	}
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_sync);

 
void snd_hdac_stream_spbcap_enable(struct hdac_bus *bus,
				   bool enable, int index)
{
	u32 mask = 0;

	if (!bus->spbcap) {
		dev_err(bus->dev, "Address of SPB capability is NULL\n");
		return;
	}

	mask |= (1 << index);

	if (enable)
		snd_hdac_updatel(bus->spbcap, AZX_REG_SPB_SPBFCCTL, mask, mask);
	else
		snd_hdac_updatel(bus->spbcap, AZX_REG_SPB_SPBFCCTL, mask, 0);
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_spbcap_enable);

 
int snd_hdac_stream_set_spib(struct hdac_bus *bus,
			     struct hdac_stream *azx_dev, u32 value)
{
	if (!bus->spbcap) {
		dev_err(bus->dev, "Address of SPB capability is NULL\n");
		return -EINVAL;
	}

	writel(value, azx_dev->spib_addr);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_set_spib);

 
int snd_hdac_stream_get_spbmaxfifo(struct hdac_bus *bus,
				   struct hdac_stream *azx_dev)
{
	if (!bus->spbcap) {
		dev_err(bus->dev, "Address of SPB capability is NULL\n");
		return -EINVAL;
	}

	return readl(azx_dev->fifo_addr);
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_get_spbmaxfifo);

 
void snd_hdac_stream_drsm_enable(struct hdac_bus *bus,
				 bool enable, int index)
{
	u32 mask = 0;

	if (!bus->drsmcap) {
		dev_err(bus->dev, "Address of DRSM capability is NULL\n");
		return;
	}

	mask |= (1 << index);

	if (enable)
		snd_hdac_updatel(bus->drsmcap, AZX_REG_DRSM_CTL, mask, mask);
	else
		snd_hdac_updatel(bus->drsmcap, AZX_REG_DRSM_CTL, mask, 0);
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_drsm_enable);

 
int snd_hdac_stream_wait_drsm(struct hdac_stream *azx_dev)
{
	struct hdac_bus *bus = azx_dev->bus;
	u32 mask, reg;
	int ret;

	mask = 1 << azx_dev->index;

	ret = read_poll_timeout(snd_hdac_reg_readl, reg, !(reg & mask), 250, 2000, false, bus,
				bus->drsmcap + AZX_REG_DRSM_CTL);
	if (ret)
		dev_dbg(bus->dev, "polling RSM 0x%08x failed: %d\n", mask, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_wait_drsm);

 
int snd_hdac_stream_set_dpibr(struct hdac_bus *bus,
			      struct hdac_stream *azx_dev, u32 value)
{
	if (!bus->drsmcap) {
		dev_err(bus->dev, "Address of DRSM capability is NULL\n");
		return -EINVAL;
	}

	writel(value, azx_dev->dpibr_addr);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_set_dpibr);

 
int snd_hdac_stream_set_lpib(struct hdac_stream *azx_dev, u32 value)
{
	snd_hdac_stream_writel(azx_dev, SD_LPIB, value);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_stream_set_lpib);

#ifdef CONFIG_SND_HDA_DSP_LOADER
 
int snd_hdac_dsp_prepare(struct hdac_stream *azx_dev, unsigned int format,
			 unsigned int byte_size, struct snd_dma_buffer *bufp)
{
	struct hdac_bus *bus = azx_dev->bus;
	__le32 *bdl;
	int err;

	snd_hdac_dsp_lock(azx_dev);
	spin_lock_irq(&bus->reg_lock);
	if (azx_dev->running || azx_dev->locked) {
		spin_unlock_irq(&bus->reg_lock);
		err = -EBUSY;
		goto unlock;
	}
	azx_dev->locked = true;
	spin_unlock_irq(&bus->reg_lock);

	err = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV_SG, bus->dev,
				  byte_size, bufp);
	if (err < 0)
		goto err_alloc;

	azx_dev->substream = NULL;
	azx_dev->bufsize = byte_size;
	azx_dev->period_bytes = byte_size;
	azx_dev->format_val = format;

	snd_hdac_stream_reset(azx_dev);

	 
	snd_hdac_stream_writel(azx_dev, SD_BDLPL, 0);
	snd_hdac_stream_writel(azx_dev, SD_BDLPU, 0);

	azx_dev->frags = 0;
	bdl = (__le32 *)azx_dev->bdl.area;
	err = setup_bdle(bus, bufp, azx_dev, &bdl, 0, byte_size, 0);
	if (err < 0)
		goto error;

	snd_hdac_stream_setup(azx_dev);
	snd_hdac_dsp_unlock(azx_dev);
	return azx_dev->stream_tag;

 error:
	snd_dma_free_pages(bufp);
 err_alloc:
	spin_lock_irq(&bus->reg_lock);
	azx_dev->locked = false;
	spin_unlock_irq(&bus->reg_lock);
 unlock:
	snd_hdac_dsp_unlock(azx_dev);
	return err;
}
EXPORT_SYMBOL_GPL(snd_hdac_dsp_prepare);

 
void snd_hdac_dsp_trigger(struct hdac_stream *azx_dev, bool start)
{
	if (start)
		snd_hdac_stream_start(azx_dev);
	else
		snd_hdac_stream_stop(azx_dev);
}
EXPORT_SYMBOL_GPL(snd_hdac_dsp_trigger);

 
void snd_hdac_dsp_cleanup(struct hdac_stream *azx_dev,
			  struct snd_dma_buffer *dmab)
{
	struct hdac_bus *bus = azx_dev->bus;

	if (!dmab->area || !azx_dev->locked)
		return;

	snd_hdac_dsp_lock(azx_dev);
	 
	snd_hdac_stream_writel(azx_dev, SD_BDLPL, 0);
	snd_hdac_stream_writel(azx_dev, SD_BDLPU, 0);
	snd_hdac_stream_writel(azx_dev, SD_CTL, 0);
	azx_dev->bufsize = 0;
	azx_dev->period_bytes = 0;
	azx_dev->format_val = 0;

	snd_dma_free_pages(dmab);
	dmab->area = NULL;

	spin_lock_irq(&bus->reg_lock);
	azx_dev->locked = false;
	spin_unlock_irq(&bus->reg_lock);
	snd_hdac_dsp_unlock(azx_dev);
}
EXPORT_SYMBOL_GPL(snd_hdac_dsp_cleanup);
#endif  
