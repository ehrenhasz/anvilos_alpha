












 

#include <sound/hdaudio_ext.h>
#include <sound/hda_register.h>
#include <sound/sof.h>
#include <trace/events/sof_intel.h>
#include "../ops.h"
#include "../sof-audio.h"
#include "hda.h"

#define HDA_LTRP_GB_VALUE_US	95

static inline const char *hda_hstream_direction_str(struct hdac_stream *hstream)
{
	if (hstream->direction == SNDRV_PCM_STREAM_PLAYBACK)
		return "Playback";
	else
		return "Capture";
}

static char *hda_hstream_dbg_get_stream_info_str(struct hdac_stream *hstream)
{
	struct snd_soc_pcm_runtime *rtd;

	if (hstream->substream)
		rtd = asoc_substream_to_rtd(hstream->substream);
	else if (hstream->cstream)
		rtd = hstream->cstream->private_data;
	else
		 
		return kasprintf(GFP_KERNEL, "-- (%s, stream_tag: %u)",
				 hda_hstream_direction_str(hstream),
				 hstream->stream_tag);

	return kasprintf(GFP_KERNEL, "dai_link \"%s\" (%s, stream_tag: %u)",
			 rtd->dai_link->name, hda_hstream_direction_str(hstream),
			 hstream->stream_tag);
}

 
static int hda_setup_bdle(struct snd_sof_dev *sdev,
			  struct snd_dma_buffer *dmab,
			  struct hdac_stream *hstream,
			  struct sof_intel_dsp_bdl **bdlp,
			  int offset, int size, int ioc)
{
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct sof_intel_dsp_bdl *bdl = *bdlp;

	while (size > 0) {
		dma_addr_t addr;
		int chunk;

		if (hstream->frags >= HDA_DSP_MAX_BDL_ENTRIES) {
			dev_err(sdev->dev, "error: stream frags exceeded\n");
			return -EINVAL;
		}

		addr = snd_sgbuf_get_addr(dmab, offset);
		 
		bdl->addr_l = cpu_to_le32(lower_32_bits(addr));
		bdl->addr_h = cpu_to_le32(upper_32_bits(addr));
		 
		chunk = snd_sgbuf_get_chunk_size(dmab, offset, size);
		 
		if (bus->align_bdle_4k) {
			u32 remain = 0x1000 - (offset & 0xfff);

			if (chunk > remain)
				chunk = remain;
		}
		bdl->size = cpu_to_le32(chunk);
		 
		size -= chunk;
		bdl->ioc = (size || !ioc) ? 0 : cpu_to_le32(0x01);
		bdl++;
		hstream->frags++;
		offset += chunk;
	}

	*bdlp = bdl;
	return offset;
}

 
int hda_dsp_stream_setup_bdl(struct snd_sof_dev *sdev,
			     struct snd_dma_buffer *dmab,
			     struct hdac_stream *hstream)
{
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	struct sof_intel_dsp_bdl *bdl;
	int i, offset, period_bytes, periods;
	int remain, ioc;

	period_bytes = hstream->period_bytes;
	dev_dbg(sdev->dev, "period_bytes:0x%x\n", period_bytes);
	if (!period_bytes)
		period_bytes = hstream->bufsize;

	periods = hstream->bufsize / period_bytes;

	dev_dbg(sdev->dev, "periods:%d\n", periods);

	remain = hstream->bufsize % period_bytes;
	if (remain)
		periods++;

	 
	bdl = (struct sof_intel_dsp_bdl *)hstream->bdl.area;
	offset = 0;
	hstream->frags = 0;

	 
	ioc = hda->no_ipc_position ?
	      !hstream->no_period_wakeup : 0;

	for (i = 0; i < periods; i++) {
		if (i == (periods - 1) && remain)
			 
			offset = hda_setup_bdle(sdev, dmab,
						hstream, &bdl, offset,
						remain, 0);
		else
			offset = hda_setup_bdle(sdev, dmab,
						hstream, &bdl, offset,
						period_bytes, ioc);
	}

	return offset;
}

int hda_dsp_stream_spib_config(struct snd_sof_dev *sdev,
			       struct hdac_ext_stream *hext_stream,
			       int enable, u32 size)
{
	struct hdac_stream *hstream = &hext_stream->hstream;
	u32 mask;

	if (!sdev->bar[HDA_DSP_SPIB_BAR]) {
		dev_err(sdev->dev, "error: address of spib capability is NULL\n");
		return -EINVAL;
	}

	mask = (1 << hstream->index);

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_SPIB_BAR,
				SOF_HDA_ADSP_REG_CL_SPBFIFO_SPBFCCTL, mask,
				enable << hstream->index);

	 
	sof_io_write(sdev, hstream->spib_addr, size);

	return 0;
}

 
struct hdac_ext_stream *
hda_dsp_stream_get(struct snd_sof_dev *sdev, int direction, u32 flags)
{
	const struct sof_intel_dsp_desc *chip_info =  get_chip_info(sdev->pdata);
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct sof_intel_hda_stream *hda_stream;
	struct hdac_ext_stream *hext_stream = NULL;
	struct hdac_stream *s;

	spin_lock_irq(&bus->reg_lock);

	 
	list_for_each_entry(s, &bus->stream_list, list) {
		if (s->direction == direction && !s->opened) {
			hext_stream = stream_to_hdac_ext_stream(s);
			hda_stream = container_of(hext_stream,
						  struct sof_intel_hda_stream,
						  hext_stream);
			 
			if (hda_stream->host_reserved)
				continue;

			s->opened = true;
			break;
		}
	}

	spin_unlock_irq(&bus->reg_lock);

	 
	if (!hext_stream) {
		dev_err(sdev->dev, "error: no free %s streams\n",
			direction == SNDRV_PCM_STREAM_PLAYBACK ?
			"playback" : "capture");
		return hext_stream;
	}

	hda_stream->flags = flags;

	 
	if (chip_info->hw_ip_version < SOF_INTEL_ACE_1_0 &&
	    !(flags & SOF_HDA_STREAM_DMI_L1_COMPATIBLE)) {
		snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
					HDA_VS_INTEL_EM2,
					HDA_VS_INTEL_EM2_L1SEN, 0);
		hda->l1_disabled = true;
	}

	return hext_stream;
}

 
int hda_dsp_stream_put(struct snd_sof_dev *sdev, int direction, int stream_tag)
{
	const struct sof_intel_dsp_desc *chip_info =  get_chip_info(sdev->pdata);
	struct sof_intel_hda_dev *hda = sdev->pdata->hw_pdata;
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct sof_intel_hda_stream *hda_stream;
	struct hdac_ext_stream *hext_stream;
	struct hdac_stream *s;
	bool dmi_l1_enable = true;
	bool found = false;

	spin_lock_irq(&bus->reg_lock);

	 
	list_for_each_entry(s, &bus->stream_list, list) {
		hext_stream = stream_to_hdac_ext_stream(s);
		hda_stream = container_of(hext_stream, struct sof_intel_hda_stream, hext_stream);

		if (!s->opened)
			continue;

		if (s->direction == direction && s->stream_tag == stream_tag) {
			s->opened = false;
			found = true;
		} else if (!(hda_stream->flags & SOF_HDA_STREAM_DMI_L1_COMPATIBLE)) {
			dmi_l1_enable = false;
		}
	}

	spin_unlock_irq(&bus->reg_lock);

	 
	if (chip_info->hw_ip_version < SOF_INTEL_ACE_1_0 && dmi_l1_enable) {
		snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, HDA_VS_INTEL_EM2,
					HDA_VS_INTEL_EM2_L1SEN, HDA_VS_INTEL_EM2_L1SEN);
		hda->l1_disabled = false;
	}

	if (!found) {
		dev_err(sdev->dev, "%s: stream_tag %d not opened!\n",
			__func__, stream_tag);
		return -ENODEV;
	}

	return 0;
}

static int hda_dsp_stream_reset(struct snd_sof_dev *sdev, struct hdac_stream *hstream)
{
	int sd_offset = SOF_STREAM_SD_OFFSET(hstream);
	int timeout = HDA_DSP_STREAM_RESET_TIMEOUT;
	u32 val;

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, sd_offset, SOF_STREAM_SD_OFFSET_CRST,
				SOF_STREAM_SD_OFFSET_CRST);
	do {
		val = snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR, sd_offset);
		if (val & SOF_STREAM_SD_OFFSET_CRST)
			break;
	} while (--timeout);
	if (timeout == 0) {
		dev_err(sdev->dev, "timeout waiting for stream reset\n");
		return -ETIMEDOUT;
	}

	timeout = HDA_DSP_STREAM_RESET_TIMEOUT;

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, sd_offset, SOF_STREAM_SD_OFFSET_CRST, 0x0);

	 
	udelay(3);
	do {
		val = snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR, sd_offset);
		if ((val & SOF_STREAM_SD_OFFSET_CRST) == 0)
			break;
	} while (--timeout);
	if (timeout == 0) {
		dev_err(sdev->dev, "timeout waiting for stream to exit reset\n");
		return -ETIMEDOUT;
	}

	return 0;
}

int hda_dsp_stream_trigger(struct snd_sof_dev *sdev,
			   struct hdac_ext_stream *hext_stream, int cmd)
{
	struct hdac_stream *hstream = &hext_stream->hstream;
	int sd_offset = SOF_STREAM_SD_OFFSET(hstream);
	u32 dma_start = SOF_HDA_SD_CTL_DMA_START;
	int ret = 0;
	u32 run;

	 
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (!sdev->dspless_mode_selected)
			break;
		fallthrough;
	case SNDRV_PCM_TRIGGER_START:
		if (hstream->running)
			break;

		snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, SOF_HDA_INTCTL,
					1 << hstream->index,
					1 << hstream->index);

		snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
					sd_offset,
					SOF_HDA_SD_CTL_DMA_START |
					SOF_HDA_CL_DMA_SD_INT_MASK,
					SOF_HDA_SD_CTL_DMA_START |
					SOF_HDA_CL_DMA_SD_INT_MASK);

		ret = snd_sof_dsp_read_poll_timeout(sdev,
					HDA_DSP_HDA_BAR,
					sd_offset, run,
					((run &	dma_start) == dma_start),
					HDA_DSP_REG_POLL_INTERVAL_US,
					HDA_DSP_STREAM_RUN_TIMEOUT);

		if (ret >= 0)
			hstream->running = true;

		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (!sdev->dspless_mode_selected)
			break;
		fallthrough;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
		snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
					sd_offset,
					SOF_HDA_SD_CTL_DMA_START |
					SOF_HDA_CL_DMA_SD_INT_MASK, 0x0);

		ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_HDA_BAR,
						sd_offset, run,
						!(run &	dma_start),
						HDA_DSP_REG_POLL_INTERVAL_US,
						HDA_DSP_STREAM_RUN_TIMEOUT);

		if (ret >= 0) {
			snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
					  sd_offset + SOF_HDA_ADSP_REG_SD_STS,
					  SOF_HDA_CL_DMA_SD_INT_MASK);

			hstream->running = false;
			snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
						SOF_HDA_INTCTL,
						1 << hstream->index, 0x0);
		}
		break;
	default:
		dev_err(sdev->dev, "error: unknown command: %d\n", cmd);
		return -EINVAL;
	}

	if (ret < 0) {
		char *stream_name = hda_hstream_dbg_get_stream_info_str(hstream);

		dev_err(sdev->dev,
			"%s: cmd %d on %s: timeout on STREAM_SD_OFFSET read\n",
			__func__, cmd, stream_name ? stream_name : "unknown stream");
		kfree(stream_name);
	}

	return ret;
}

 
int hda_dsp_iccmax_stream_hw_params(struct snd_sof_dev *sdev, struct hdac_ext_stream *hext_stream,
				    struct snd_dma_buffer *dmab,
				    struct snd_pcm_hw_params *params)
{
	struct hdac_stream *hstream = &hext_stream->hstream;
	int sd_offset = SOF_STREAM_SD_OFFSET(hstream);
	int ret;
	u32 mask = 0x1 << hstream->index;

	if (!hext_stream) {
		dev_err(sdev->dev, "error: no stream available\n");
		return -ENODEV;
	}

	if (!dmab) {
		dev_err(sdev->dev, "error: no dma buffer allocated!\n");
		return -ENODEV;
	}

	if (hstream->posbuf)
		*hstream->posbuf = 0;

	 
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_SD_BDLPL,
			  0x0);
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_SD_BDLPU,
			  0x0);

	hstream->frags = 0;

	ret = hda_dsp_stream_setup_bdl(sdev, dmab, hstream);
	if (ret < 0) {
		dev_err(sdev->dev, "error: set up of BDL failed\n");
		return ret;
	}

	 
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_SD_BDLPL,
			  (u32)hstream->bdl.addr);
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_SD_BDLPU,
			  upper_32_bits(hstream->bdl.addr));

	 
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_SD_CBL,
			  hstream->bufsize);

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
				sd_offset + SOF_HDA_ADSP_REG_SD_LVI,
				0xffff, (hstream->frags - 1));

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
				mask, mask);

	 
	snd_sof_dsp_update8(sdev, HDA_DSP_HDA_BAR, HDA_VS_INTEL_LTRP,
			    HDA_VS_INTEL_LTRP_GB_MASK, HDA_LTRP_GB_VALUE_US);

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, sd_offset,
				SOF_HDA_SD_CTL_DMA_START, SOF_HDA_SD_CTL_DMA_START);

	return 0;
}

 
int hda_dsp_stream_hw_params(struct snd_sof_dev *sdev,
			     struct hdac_ext_stream *hext_stream,
			     struct snd_dma_buffer *dmab,
			     struct snd_pcm_hw_params *params)
{
	const struct sof_intel_dsp_desc *chip = get_chip_info(sdev->pdata);
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct hdac_stream *hstream;
	int sd_offset, ret;
	u32 dma_start = SOF_HDA_SD_CTL_DMA_START;
	u32 mask;
	u32 run;

	if (!hext_stream) {
		dev_err(sdev->dev, "error: no stream available\n");
		return -ENODEV;
	}

	if (!dmab) {
		dev_err(sdev->dev, "error: no dma buffer allocated!\n");
		return -ENODEV;
	}

	hstream = &hext_stream->hstream;
	sd_offset = SOF_STREAM_SD_OFFSET(hstream);
	mask = BIT(hstream->index);

	 
	if (!sdev->dspless_mode_selected)
		snd_sof_dsp_update_bits(sdev, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
					mask, mask);

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, sd_offset,
				SOF_HDA_CL_DMA_SD_INT_MASK |
				SOF_HDA_SD_CTL_DMA_START, 0);

	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_HDA_BAR,
					    sd_offset, run,
					    !(run & dma_start),
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_STREAM_RUN_TIMEOUT);

	if (ret < 0) {
		char *stream_name = hda_hstream_dbg_get_stream_info_str(hstream);

		dev_err(sdev->dev,
			"%s: on %s: timeout on STREAM_SD_OFFSET read1\n",
			__func__, stream_name ? stream_name : "unknown stream");
		kfree(stream_name);
		return ret;
	}

	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
				sd_offset + SOF_HDA_ADSP_REG_SD_STS,
				SOF_HDA_CL_DMA_SD_INT_MASK,
				SOF_HDA_CL_DMA_SD_INT_MASK);

	 
	ret = hda_dsp_stream_reset(sdev, hstream);
	if (ret < 0)
		return ret;

	if (hstream->posbuf)
		*hstream->posbuf = 0;

	 
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_SD_BDLPL,
			  0x0);
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_SD_BDLPU,
			  0x0);

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, sd_offset,
				SOF_HDA_CL_DMA_SD_INT_MASK |
				SOF_HDA_SD_CTL_DMA_START, 0);

	ret = snd_sof_dsp_read_poll_timeout(sdev, HDA_DSP_HDA_BAR,
					    sd_offset, run,
					    !(run & dma_start),
					    HDA_DSP_REG_POLL_INTERVAL_US,
					    HDA_DSP_STREAM_RUN_TIMEOUT);

	if (ret < 0) {
		char *stream_name = hda_hstream_dbg_get_stream_info_str(hstream);

		dev_err(sdev->dev,
			"%s: on %s: timeout on STREAM_SD_OFFSET read1\n",
			__func__, stream_name ? stream_name : "unknown stream");
		kfree(stream_name);
		return ret;
	}

	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
				sd_offset + SOF_HDA_ADSP_REG_SD_STS,
				SOF_HDA_CL_DMA_SD_INT_MASK,
				SOF_HDA_CL_DMA_SD_INT_MASK);

	hstream->frags = 0;

	ret = hda_dsp_stream_setup_bdl(sdev, dmab, hstream);
	if (ret < 0) {
		dev_err(sdev->dev, "error: set up of BDL failed\n");
		return ret;
	}

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, sd_offset,
				SOF_HDA_CL_SD_CTL_STREAM_TAG_MASK,
				hstream->stream_tag <<
				SOF_HDA_CL_SD_CTL_STREAM_TAG_SHIFT);

	 
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_SD_CBL,
			  hstream->bufsize);

	 

	if (!sdev->dspless_mode_selected && (chip->quirks & SOF_INTEL_PROCEN_FMT_QUIRK))
		 
		snd_sof_dsp_update_bits(sdev, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
					mask, 0);

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
				sd_offset +
				SOF_HDA_ADSP_REG_SD_FORMAT,
				0xffff, hstream->format_val);

	if (!sdev->dspless_mode_selected && (chip->quirks & SOF_INTEL_PROCEN_FMT_QUIRK))
		 
		snd_sof_dsp_update_bits(sdev, HDA_DSP_PP_BAR, SOF_HDA_REG_PP_PPCTL,
					mask, mask);

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR,
				sd_offset + SOF_HDA_ADSP_REG_SD_LVI,
				0xffff, (hstream->frags - 1));

	 
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_SD_BDLPL,
			  (u32)hstream->bdl.addr);
	snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR,
			  sd_offset + SOF_HDA_ADSP_REG_SD_BDLPU,
			  upper_32_bits(hstream->bdl.addr));

	 
	if (bus->use_posbuf && bus->posbuf.addr &&
	    !(snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPLBASE)
	      & SOF_HDA_ADSP_DPLBASE_ENABLE)) {
		snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPUBASE,
				  upper_32_bits(bus->posbuf.addr));
		snd_sof_dsp_write(sdev, HDA_DSP_HDA_BAR, SOF_HDA_ADSP_DPLBASE,
				  (u32)bus->posbuf.addr |
				  SOF_HDA_ADSP_DPLBASE_ENABLE);
	}

	 
	snd_sof_dsp_update_bits(sdev, HDA_DSP_HDA_BAR, sd_offset,
				SOF_HDA_CL_DMA_SD_INT_MASK,
				SOF_HDA_CL_DMA_SD_INT_MASK);

	 
	if (hstream->direction == SNDRV_PCM_STREAM_PLAYBACK) {
		hstream->fifo_size =
			snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR,
					 sd_offset +
					 SOF_HDA_ADSP_REG_SD_FIFOSIZE);
		hstream->fifo_size &= 0xffff;
		hstream->fifo_size += 1;
	} else {
		hstream->fifo_size = 0;
	}

	return ret;
}

int hda_dsp_stream_hw_free(struct snd_sof_dev *sdev,
			   struct snd_pcm_substream *substream)
{
	struct hdac_stream *hstream = substream->runtime->private_data;
	struct hdac_ext_stream *hext_stream = container_of(hstream,
							 struct hdac_ext_stream,
							 hstream);
	int ret;

	ret = hda_dsp_stream_reset(sdev, hstream);
	if (ret < 0)
		return ret;

	if (!sdev->dspless_mode_selected) {
		struct hdac_bus *bus = sof_to_bus(sdev);
		u32 mask = BIT(hstream->index);

		spin_lock_irq(&bus->reg_lock);
		 
		if (!hext_stream->link_locked)
			snd_sof_dsp_update_bits(sdev, HDA_DSP_PP_BAR,
						SOF_HDA_REG_PP_PPCTL, mask, 0);
		spin_unlock_irq(&bus->reg_lock);
	}

	hda_dsp_stream_spib_config(sdev, hext_stream, HDA_DSP_SPIB_DISABLE, 0);

	hstream->substream = NULL;

	return 0;
}

bool hda_dsp_check_stream_irq(struct snd_sof_dev *sdev)
{
	struct hdac_bus *bus = sof_to_bus(sdev);
	bool ret = false;
	u32 status;

	 
	spin_lock_irq(&bus->reg_lock);

	status = snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR, SOF_HDA_INTSTS);

	trace_sof_intel_hda_dsp_check_stream_irq(sdev, status);

	 
	if (status != 0xffffffff)
		ret = true;

	spin_unlock_irq(&bus->reg_lock);

	return ret;
}

static void
hda_dsp_compr_bytes_transferred(struct hdac_stream *hstream, int direction)
{
	u64 buffer_size = hstream->bufsize;
	u64 prev_pos, pos, num_bytes;

	div64_u64_rem(hstream->curr_pos, buffer_size, &prev_pos);
	pos = hda_dsp_stream_get_position(hstream, direction, false);

	if (pos < prev_pos)
		num_bytes = (buffer_size - prev_pos) +  pos;
	else
		num_bytes = pos - prev_pos;

	hstream->curr_pos += num_bytes;
}

static bool hda_dsp_stream_check(struct hdac_bus *bus, u32 status)
{
	struct sof_intel_hda_dev *sof_hda = bus_to_sof_hda(bus);
	struct hdac_stream *s;
	bool active = false;
	u32 sd_status;

	list_for_each_entry(s, &bus->stream_list, list) {
		if (status & BIT(s->index) && s->opened) {
			sd_status = readb(s->sd_addr + SOF_HDA_ADSP_REG_SD_STS);

			trace_sof_intel_hda_dsp_stream_status(bus->dev, s, sd_status);

			writeb(sd_status, s->sd_addr + SOF_HDA_ADSP_REG_SD_STS);

			active = true;
			if ((!s->substream && !s->cstream) ||
			    !s->running ||
			    (sd_status & SOF_HDA_CL_DMA_SD_INT_COMPLETE) == 0)
				continue;

			 
			if (s->substream && sof_hda->no_ipc_position) {
				snd_sof_pcm_period_elapsed(s->substream);
			} else if (s->cstream) {
				hda_dsp_compr_bytes_transferred(s, s->cstream->direction);
				snd_compr_fragment_elapsed(s->cstream);
			}
		}
	}

	return active;
}

irqreturn_t hda_dsp_stream_threaded_handler(int irq, void *context)
{
	struct snd_sof_dev *sdev = context;
	struct hdac_bus *bus = sof_to_bus(sdev);
	bool active;
	u32 status;
	int i;

	 
	for (i = 0, active = true; i < 10 && active; i++) {
		spin_lock_irq(&bus->reg_lock);

		status = snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR, SOF_HDA_INTSTS);

		 
		active = hda_dsp_stream_check(bus, status);

		 
		if (status & AZX_INT_CTRL_EN) {
			active |= hda_codec_check_rirb_status(sdev);
		}
		spin_unlock_irq(&bus->reg_lock);
	}

	return IRQ_HANDLED;
}

int hda_dsp_stream_init(struct snd_sof_dev *sdev)
{
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct hdac_ext_stream *hext_stream;
	struct hdac_stream *hstream;
	struct pci_dev *pci = to_pci_dev(sdev->dev);
	struct sof_intel_hda_dev *sof_hda = bus_to_sof_hda(bus);
	int sd_offset;
	int i, num_playback, num_capture, num_total, ret;
	u32 gcap;

	gcap = snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR, SOF_HDA_GCAP);
	dev_dbg(sdev->dev, "hda global caps = 0x%x\n", gcap);

	 
	num_capture = (gcap >> 8) & 0x0f;
	num_playback = (gcap >> 12) & 0x0f;
	num_total = num_playback + num_capture;

	dev_dbg(sdev->dev, "detected %d playback and %d capture streams\n",
		num_playback, num_capture);

	if (num_playback >= SOF_HDA_PLAYBACK_STREAMS) {
		dev_err(sdev->dev, "error: too many playback streams %d\n",
			num_playback);
		return -EINVAL;
	}

	if (num_capture >= SOF_HDA_CAPTURE_STREAMS) {
		dev_err(sdev->dev, "error: too many capture streams %d\n",
			num_playback);
		return -EINVAL;
	}

	 
	ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, &pci->dev,
				  SOF_HDA_DPIB_ENTRY_SIZE * num_total,
				  &bus->posbuf);
	if (ret < 0) {
		dev_err(sdev->dev, "error: posbuffer dma alloc failed\n");
		return -ENOMEM;
	}

	 
	ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, &pci->dev,
				  PAGE_SIZE, &bus->rb);
	if (ret < 0) {
		dev_err(sdev->dev, "error: RB alloc failed\n");
		return -ENOMEM;
	}

	 
	for (i = 0; i < num_total; i++) {
		struct sof_intel_hda_stream *hda_stream;

		hda_stream = devm_kzalloc(sdev->dev, sizeof(*hda_stream),
					  GFP_KERNEL);
		if (!hda_stream)
			return -ENOMEM;

		hda_stream->sdev = sdev;

		hext_stream = &hda_stream->hext_stream;

		if (sdev->bar[HDA_DSP_PP_BAR]) {
			hext_stream->pphc_addr = sdev->bar[HDA_DSP_PP_BAR] +
				SOF_HDA_PPHC_BASE + SOF_HDA_PPHC_INTERVAL * i;

			hext_stream->pplc_addr = sdev->bar[HDA_DSP_PP_BAR] +
				SOF_HDA_PPLC_BASE + SOF_HDA_PPLC_MULTI * num_total +
				SOF_HDA_PPLC_INTERVAL * i;
		}

		hstream = &hext_stream->hstream;

		 
		if (sdev->bar[HDA_DSP_SPIB_BAR]) {
			hstream->spib_addr = sdev->bar[HDA_DSP_SPIB_BAR] +
				SOF_HDA_SPIB_BASE + SOF_HDA_SPIB_INTERVAL * i +
				SOF_HDA_SPIB_SPIB;

			hstream->fifo_addr = sdev->bar[HDA_DSP_SPIB_BAR] +
				SOF_HDA_SPIB_BASE + SOF_HDA_SPIB_INTERVAL * i +
				SOF_HDA_SPIB_MAXFIFO;
		}

		hstream->bus = bus;
		hstream->sd_int_sta_mask = 1 << i;
		hstream->index = i;
		sd_offset = SOF_STREAM_SD_OFFSET(hstream);
		hstream->sd_addr = sdev->bar[HDA_DSP_HDA_BAR] + sd_offset;
		hstream->opened = false;
		hstream->running = false;

		if (i < num_capture) {
			hstream->stream_tag = i + 1;
			hstream->direction = SNDRV_PCM_STREAM_CAPTURE;
		} else {
			hstream->stream_tag = i - num_capture + 1;
			hstream->direction = SNDRV_PCM_STREAM_PLAYBACK;
		}

		 
		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, &pci->dev,
					  HDA_DSP_BDL_SIZE, &hstream->bdl);
		if (ret < 0) {
			dev_err(sdev->dev, "error: stream bdl dma alloc failed\n");
			return -ENOMEM;
		}

		hstream->posbuf = (__le32 *)(bus->posbuf.area +
			(hstream->index) * 8);

		list_add_tail(&hstream->list, &bus->stream_list);
	}

	 
	sof_hda->stream_max = num_total;

	return 0;
}

void hda_dsp_stream_free(struct snd_sof_dev *sdev)
{
	struct hdac_bus *bus = sof_to_bus(sdev);
	struct hdac_stream *s, *_s;
	struct hdac_ext_stream *hext_stream;
	struct sof_intel_hda_stream *hda_stream;

	 
	if (bus->posbuf.area)
		snd_dma_free_pages(&bus->posbuf);

	 
	if (bus->rb.area)
		snd_dma_free_pages(&bus->rb);

	list_for_each_entry_safe(s, _s, &bus->stream_list, list) {
		 

		 
		if (s->bdl.area)
			snd_dma_free_pages(&s->bdl);
		list_del(&s->list);
		hext_stream = stream_to_hdac_ext_stream(s);
		hda_stream = container_of(hext_stream, struct sof_intel_hda_stream,
					  hext_stream);
		devm_kfree(sdev->dev, hda_stream);
	}
}

snd_pcm_uframes_t hda_dsp_stream_get_position(struct hdac_stream *hstream,
					      int direction, bool can_sleep)
{
	struct hdac_ext_stream *hext_stream = stream_to_hdac_ext_stream(hstream);
	struct sof_intel_hda_stream *hda_stream = hstream_to_sof_hda_stream(hext_stream);
	struct snd_sof_dev *sdev = hda_stream->sdev;
	snd_pcm_uframes_t pos;

	switch (sof_hda_position_quirk) {
	case SOF_HDA_POSITION_QUIRK_USE_SKYLAKE_LEGACY:
		 

		 
		if (direction == SNDRV_PCM_STREAM_PLAYBACK) {
			pos = snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR,
					       AZX_REG_VS_SDXDPIB_XBASE +
					       (AZX_REG_VS_SDXDPIB_XINTERVAL *
						hstream->index));
		} else {
			 
			if (can_sleep)
				usleep_range(20, 21);

			snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR,
					 AZX_REG_VS_SDXDPIB_XBASE +
					 (AZX_REG_VS_SDXDPIB_XINTERVAL *
					  hstream->index));
			pos = snd_hdac_stream_get_pos_posbuf(hstream);
		}
		break;
	case SOF_HDA_POSITION_QUIRK_USE_DPIB_REGISTERS:
		 
		pos = snd_sof_dsp_read(sdev, HDA_DSP_HDA_BAR,
				       AZX_REG_VS_SDXDPIB_XBASE +
				       (AZX_REG_VS_SDXDPIB_XINTERVAL *
					hstream->index));
		break;
	case SOF_HDA_POSITION_QUIRK_USE_DPIB_DDR_UPDATE:
		 
		pos = snd_hdac_stream_get_pos_posbuf(hstream);
		break;
	default:
		dev_err_once(sdev->dev, "hda_position_quirk value %d not supported\n",
			     sof_hda_position_quirk);
		pos = 0;
		break;
	}

	if (pos >= hstream->bufsize)
		pos = 0;

	return pos;
}
