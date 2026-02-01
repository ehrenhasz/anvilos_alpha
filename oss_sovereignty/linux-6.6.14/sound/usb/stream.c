
 


#include <linux/init.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/usb/audio-v3.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include <sound/tlv.h>

#include "usbaudio.h"
#include "card.h"
#include "proc.h"
#include "quirks.h"
#include "endpoint.h"
#include "pcm.h"
#include "helper.h"
#include "format.h"
#include "clock.h"
#include "stream.h"
#include "power.h"
#include "media.h"

static void audioformat_free(struct audioformat *fp)
{
	list_del(&fp->list);  
	kfree(fp->rate_table);
	kfree(fp->chmap);
	kfree(fp);
}

 
static void free_substream(struct snd_usb_substream *subs)
{
	struct audioformat *fp, *n;

	if (!subs->num_formats)
		return;  
	list_for_each_entry_safe(fp, n, &subs->fmt_list, list)
		audioformat_free(fp);
	kfree(subs->str_pd);
	snd_media_stream_delete(subs);
}


 
static void snd_usb_audio_stream_free(struct snd_usb_stream *stream)
{
	free_substream(&stream->substream[0]);
	free_substream(&stream->substream[1]);
	list_del(&stream->list);
	kfree(stream);
}

static void snd_usb_audio_pcm_free(struct snd_pcm *pcm)
{
	struct snd_usb_stream *stream = pcm->private_data;
	if (stream) {
		stream->pcm = NULL;
		snd_usb_audio_stream_free(stream);
	}
}

 

static void snd_usb_init_substream(struct snd_usb_stream *as,
				   int stream,
				   struct audioformat *fp,
				   struct snd_usb_power_domain *pd)
{
	struct snd_usb_substream *subs = &as->substream[stream];

	INIT_LIST_HEAD(&subs->fmt_list);
	spin_lock_init(&subs->lock);

	subs->stream = as;
	subs->direction = stream;
	subs->dev = as->chip->dev;
	subs->txfr_quirk = !!(as->chip->quirk_flags & QUIRK_FLAG_ALIGN_TRANSFER);
	subs->tx_length_quirk = !!(as->chip->quirk_flags & QUIRK_FLAG_TX_LENGTH);
	subs->speed = snd_usb_get_speed(subs->dev);
	subs->pkt_offset_adj = 0;
	subs->stream_offset_adj = 0;

	snd_usb_set_pcm_ops(as->pcm, stream);

	list_add_tail(&fp->list, &subs->fmt_list);
	subs->formats |= fp->formats;
	subs->num_formats++;
	subs->fmt_type = fp->fmt_type;
	subs->ep_num = fp->endpoint;
	if (fp->channels > subs->channels_max)
		subs->channels_max = fp->channels;

	if (pd) {
		subs->str_pd = pd;
		 
		snd_usb_power_domain_set(subs->stream->chip, pd,
					 UAC3_PD_STATE_D1);
	}

	snd_usb_preallocate_buffer(subs);
}

 
static int usb_chmap_ctl_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
	struct snd_usb_substream *subs = info->private_data;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = subs->channels_max;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = SNDRV_CHMAP_LAST;
	return 0;
}

 
static bool have_dup_chmap(struct snd_usb_substream *subs,
			   struct audioformat *fp)
{
	struct audioformat *prev = fp;

	list_for_each_entry_continue_reverse(prev, &subs->fmt_list, list) {
		if (prev->chmap &&
		    !memcmp(prev->chmap, fp->chmap, sizeof(*fp->chmap)))
			return true;
	}
	return false;
}

static int usb_chmap_ctl_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			     unsigned int size, unsigned int __user *tlv)
{
	struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
	struct snd_usb_substream *subs = info->private_data;
	struct audioformat *fp;
	unsigned int __user *dst;
	int count = 0;

	if (size < 8)
		return -ENOMEM;
	if (put_user(SNDRV_CTL_TLVT_CONTAINER, tlv))
		return -EFAULT;
	size -= 8;
	dst = tlv + 2;
	list_for_each_entry(fp, &subs->fmt_list, list) {
		int i, ch_bytes;

		if (!fp->chmap)
			continue;
		if (have_dup_chmap(subs, fp))
			continue;
		 
		ch_bytes = fp->chmap->channels * 4;
		if (size < 8 + ch_bytes)
			return -ENOMEM;
		if (put_user(SNDRV_CTL_TLVT_CHMAP_FIXED, dst) ||
		    put_user(ch_bytes, dst + 1))
			return -EFAULT;
		dst += 2;
		for (i = 0; i < fp->chmap->channels; i++, dst++) {
			if (put_user(fp->chmap->map[i], dst))
				return -EFAULT;
		}

		count += 8 + ch_bytes;
		size -= 8 + ch_bytes;
	}
	if (put_user(count, tlv + 1))
		return -EFAULT;
	return 0;
}

static int usb_chmap_ctl_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
	struct snd_usb_substream *subs = info->private_data;
	struct snd_pcm_chmap_elem *chmap = NULL;
	int i = 0;

	if (subs->cur_audiofmt)
		chmap = subs->cur_audiofmt->chmap;
	if (chmap) {
		for (i = 0; i < chmap->channels; i++)
			ucontrol->value.integer.value[i] = chmap->map[i];
	}
	for (; i < subs->channels_max; i++)
		ucontrol->value.integer.value[i] = 0;
	return 0;
}

 
static int add_chmap(struct snd_pcm *pcm, int stream,
		     struct snd_usb_substream *subs)
{
	struct audioformat *fp;
	struct snd_pcm_chmap *chmap;
	struct snd_kcontrol *kctl;
	int err;

	list_for_each_entry(fp, &subs->fmt_list, list)
		if (fp->chmap)
			goto ok;
	 
	return 0;

 ok:
	err = snd_pcm_add_chmap_ctls(pcm, stream, NULL, 0, 0, &chmap);
	if (err < 0)
		return err;

	 
	chmap->private_data = subs;
	kctl = chmap->kctl;
	kctl->info = usb_chmap_ctl_info;
	kctl->get = usb_chmap_ctl_get;
	kctl->tlv.c = usb_chmap_ctl_tlv;

	return 0;
}

 
static struct snd_pcm_chmap_elem *convert_chmap(int channels, unsigned int bits,
						int protocol)
{
	static const unsigned int uac1_maps[] = {
		SNDRV_CHMAP_FL,		 
		SNDRV_CHMAP_FR,		 
		SNDRV_CHMAP_FC,		 
		SNDRV_CHMAP_LFE,	 
		SNDRV_CHMAP_SL,		 
		SNDRV_CHMAP_SR,		 
		SNDRV_CHMAP_FLC,	 
		SNDRV_CHMAP_FRC,	 
		SNDRV_CHMAP_RC,		 
		SNDRV_CHMAP_SL,		 
		SNDRV_CHMAP_SR,		 
		SNDRV_CHMAP_TC,		 
		0  
	};
	static const unsigned int uac2_maps[] = {
		SNDRV_CHMAP_FL,		 
		SNDRV_CHMAP_FR,		 
		SNDRV_CHMAP_FC,		 
		SNDRV_CHMAP_LFE,	 
		SNDRV_CHMAP_RL,		 
		SNDRV_CHMAP_RR,		 
		SNDRV_CHMAP_FLC,	 
		SNDRV_CHMAP_FRC,	 
		SNDRV_CHMAP_RC,		 
		SNDRV_CHMAP_SL,		 
		SNDRV_CHMAP_SR,		 
		SNDRV_CHMAP_TC,		 
		SNDRV_CHMAP_TFL,	 
		SNDRV_CHMAP_TFC,	 
		SNDRV_CHMAP_TFR,	 
		SNDRV_CHMAP_TRL,	 
		SNDRV_CHMAP_TRC,	 
		SNDRV_CHMAP_TRR,	 
		SNDRV_CHMAP_TFLC,	 
		SNDRV_CHMAP_TFRC,	 
		SNDRV_CHMAP_LLFE,	 
		SNDRV_CHMAP_RLFE,	 
		SNDRV_CHMAP_TSL,	 
		SNDRV_CHMAP_TSR,	 
		SNDRV_CHMAP_BC,		 
		SNDRV_CHMAP_RLC,	 
		SNDRV_CHMAP_RRC,	 
		0  
	};
	struct snd_pcm_chmap_elem *chmap;
	const unsigned int *maps;
	int c;

	if (channels > ARRAY_SIZE(chmap->map))
		return NULL;

	chmap = kzalloc(sizeof(*chmap), GFP_KERNEL);
	if (!chmap)
		return NULL;

	maps = protocol == UAC_VERSION_2 ? uac2_maps : uac1_maps;
	chmap->channels = channels;
	c = 0;

	if (bits) {
		for (; bits && *maps; maps++, bits >>= 1)
			if (bits & 1)
				chmap->map[c++] = *maps;
	} else {
		 
		if (channels == 1)
			chmap->map[c++] = SNDRV_CHMAP_MONO;
		else
			for (; c < channels && *maps; maps++)
				chmap->map[c++] = *maps;
	}

	for (; c < channels; c++)
		chmap->map[c] = SNDRV_CHMAP_UNKNOWN;

	return chmap;
}

 
static struct
snd_pcm_chmap_elem *convert_chmap_v3(struct uac3_cluster_header_descriptor
								*cluster)
{
	unsigned int channels = cluster->bNrChannels;
	struct snd_pcm_chmap_elem *chmap;
	void *p = cluster;
	int len, c;

	if (channels > ARRAY_SIZE(chmap->map))
		return NULL;

	chmap = kzalloc(sizeof(*chmap), GFP_KERNEL);
	if (!chmap)
		return NULL;

	len = le16_to_cpu(cluster->wLength);
	c = 0;
	p += sizeof(struct uac3_cluster_header_descriptor);

	while (((p - (void *)cluster) < len) && (c < channels)) {
		struct uac3_cluster_segment_descriptor *cs_desc = p;
		u16 cs_len;
		u8 cs_type;

		cs_len = le16_to_cpu(cs_desc->wLength);
		cs_type = cs_desc->bSegmentType;

		if (cs_type == UAC3_CHANNEL_INFORMATION) {
			struct uac3_cluster_information_segment_descriptor *is = p;
			unsigned char map;

			 
			switch (is->bChRelationship) {
			case UAC3_CH_MONO:
				map = SNDRV_CHMAP_MONO;
				break;
			case UAC3_CH_LEFT:
			case UAC3_CH_FRONT_LEFT:
			case UAC3_CH_HEADPHONE_LEFT:
				map = SNDRV_CHMAP_FL;
				break;
			case UAC3_CH_RIGHT:
			case UAC3_CH_FRONT_RIGHT:
			case UAC3_CH_HEADPHONE_RIGHT:
				map = SNDRV_CHMAP_FR;
				break;
			case UAC3_CH_FRONT_CENTER:
				map = SNDRV_CHMAP_FC;
				break;
			case UAC3_CH_FRONT_LEFT_OF_CENTER:
				map = SNDRV_CHMAP_FLC;
				break;
			case UAC3_CH_FRONT_RIGHT_OF_CENTER:
				map = SNDRV_CHMAP_FRC;
				break;
			case UAC3_CH_SIDE_LEFT:
				map = SNDRV_CHMAP_SL;
				break;
			case UAC3_CH_SIDE_RIGHT:
				map = SNDRV_CHMAP_SR;
				break;
			case UAC3_CH_BACK_LEFT:
				map = SNDRV_CHMAP_RL;
				break;
			case UAC3_CH_BACK_RIGHT:
				map = SNDRV_CHMAP_RR;
				break;
			case UAC3_CH_BACK_CENTER:
				map = SNDRV_CHMAP_RC;
				break;
			case UAC3_CH_BACK_LEFT_OF_CENTER:
				map = SNDRV_CHMAP_RLC;
				break;
			case UAC3_CH_BACK_RIGHT_OF_CENTER:
				map = SNDRV_CHMAP_RRC;
				break;
			case UAC3_CH_TOP_CENTER:
				map = SNDRV_CHMAP_TC;
				break;
			case UAC3_CH_TOP_FRONT_LEFT:
				map = SNDRV_CHMAP_TFL;
				break;
			case UAC3_CH_TOP_FRONT_RIGHT:
				map = SNDRV_CHMAP_TFR;
				break;
			case UAC3_CH_TOP_FRONT_CENTER:
				map = SNDRV_CHMAP_TFC;
				break;
			case UAC3_CH_TOP_FRONT_LOC:
				map = SNDRV_CHMAP_TFLC;
				break;
			case UAC3_CH_TOP_FRONT_ROC:
				map = SNDRV_CHMAP_TFRC;
				break;
			case UAC3_CH_TOP_SIDE_LEFT:
				map = SNDRV_CHMAP_TSL;
				break;
			case UAC3_CH_TOP_SIDE_RIGHT:
				map = SNDRV_CHMAP_TSR;
				break;
			case UAC3_CH_TOP_BACK_LEFT:
				map = SNDRV_CHMAP_TRL;
				break;
			case UAC3_CH_TOP_BACK_RIGHT:
				map = SNDRV_CHMAP_TRR;
				break;
			case UAC3_CH_TOP_BACK_CENTER:
				map = SNDRV_CHMAP_TRC;
				break;
			case UAC3_CH_BOTTOM_CENTER:
				map = SNDRV_CHMAP_BC;
				break;
			case UAC3_CH_LOW_FREQUENCY_EFFECTS:
				map = SNDRV_CHMAP_LFE;
				break;
			case UAC3_CH_LFE_LEFT:
				map = SNDRV_CHMAP_LLFE;
				break;
			case UAC3_CH_LFE_RIGHT:
				map = SNDRV_CHMAP_RLFE;
				break;
			case UAC3_CH_RELATIONSHIP_UNDEFINED:
			default:
				map = SNDRV_CHMAP_UNKNOWN;
				break;
			}
			chmap->map[c++] = map;
		}
		p += cs_len;
	}

	if (channels < c)
		pr_err("%s: channel number mismatch\n", __func__);

	chmap->channels = channels;

	for (; c < channels; c++)
		chmap->map[c] = SNDRV_CHMAP_UNKNOWN;

	return chmap;
}

 
static int __snd_usb_add_audio_stream(struct snd_usb_audio *chip,
				      int stream,
				      struct audioformat *fp,
				      struct snd_usb_power_domain *pd)

{
	struct snd_usb_stream *as;
	struct snd_usb_substream *subs;
	struct snd_pcm *pcm;
	int err;

	list_for_each_entry(as, &chip->pcm_list, list) {
		if (as->fmt_type != fp->fmt_type)
			continue;
		subs = &as->substream[stream];
		if (subs->ep_num == fp->endpoint) {
			list_add_tail(&fp->list, &subs->fmt_list);
			subs->num_formats++;
			subs->formats |= fp->formats;
			return 0;
		}
	}

	if (chip->card->registered)
		chip->need_delayed_register = true;

	 
	list_for_each_entry(as, &chip->pcm_list, list) {
		if (as->fmt_type != fp->fmt_type)
			continue;
		subs = &as->substream[stream];
		if (subs->ep_num)
			continue;
		err = snd_pcm_new_stream(as->pcm, stream, 1);
		if (err < 0)
			return err;
		snd_usb_init_substream(as, stream, fp, pd);
		return add_chmap(as->pcm, stream, subs);
	}

	 
	as = kzalloc(sizeof(*as), GFP_KERNEL);
	if (!as)
		return -ENOMEM;
	as->pcm_index = chip->pcm_devs;
	as->chip = chip;
	as->fmt_type = fp->fmt_type;
	err = snd_pcm_new(chip->card, "USB Audio", chip->pcm_devs,
			  stream == SNDRV_PCM_STREAM_PLAYBACK ? 1 : 0,
			  stream == SNDRV_PCM_STREAM_PLAYBACK ? 0 : 1,
			  &pcm);
	if (err < 0) {
		kfree(as);
		return err;
	}
	as->pcm = pcm;
	pcm->private_data = as;
	pcm->private_free = snd_usb_audio_pcm_free;
	pcm->info_flags = 0;
	if (chip->pcm_devs > 0)
		sprintf(pcm->name, "USB Audio #%d", chip->pcm_devs);
	else
		strcpy(pcm->name, "USB Audio");

	snd_usb_init_substream(as, stream, fp, pd);

	 
	if (chip->usb_id == USB_ID(0x0763, 0x2003))
		list_add(&as->list, &chip->pcm_list);
	else
		list_add_tail(&as->list, &chip->pcm_list);

	chip->pcm_devs++;

	snd_usb_proc_pcm_format_add(as);

	return add_chmap(pcm, stream, &as->substream[stream]);
}

int snd_usb_add_audio_stream(struct snd_usb_audio *chip,
			     int stream,
			     struct audioformat *fp)
{
	return __snd_usb_add_audio_stream(chip, stream, fp, NULL);
}

static int snd_usb_add_audio_stream_v3(struct snd_usb_audio *chip,
				       int stream,
				       struct audioformat *fp,
				       struct snd_usb_power_domain *pd)
{
	return __snd_usb_add_audio_stream(chip, stream, fp, pd);
}

static int parse_uac_endpoint_attributes(struct snd_usb_audio *chip,
					 struct usb_host_interface *alts,
					 int protocol, int iface_no)
{
	 
	struct uac_iso_endpoint_descriptor *csep;
	struct usb_interface_descriptor *altsd = get_iface_desc(alts);
	int attributes = 0;

	csep = snd_usb_find_desc(alts->endpoint[0].extra, alts->endpoint[0].extralen, NULL, USB_DT_CS_ENDPOINT);

	 
	if (!csep && altsd->bNumEndpoints >= 2)
		csep = snd_usb_find_desc(alts->endpoint[1].extra, alts->endpoint[1].extralen, NULL, USB_DT_CS_ENDPOINT);

	 
	if (!csep)
		csep = snd_usb_find_desc(alts->extra, alts->extralen, NULL, USB_DT_CS_ENDPOINT);

	if (!csep || csep->bLength < 7 ||
	    csep->bDescriptorSubtype != UAC_EP_GENERAL)
		goto error;

	if (protocol == UAC_VERSION_1) {
		attributes = csep->bmAttributes;
	} else if (protocol == UAC_VERSION_2) {
		struct uac2_iso_endpoint_descriptor *csep2 =
			(struct uac2_iso_endpoint_descriptor *) csep;

		if (csep2->bLength < sizeof(*csep2))
			goto error;
		attributes = csep->bmAttributes & UAC_EP_CS_ATTR_FILL_MAX;

		 
		if (csep2->bmControls & UAC2_CONTROL_PITCH)
			attributes |= UAC_EP_CS_ATTR_PITCH_CONTROL;
	} else {  
		struct uac3_iso_endpoint_descriptor *csep3 =
			(struct uac3_iso_endpoint_descriptor *) csep;

		if (csep3->bLength < sizeof(*csep3))
			goto error;
		 
		if (le32_to_cpu(csep3->bmControls) & UAC2_CONTROL_PITCH)
			attributes |= UAC_EP_CS_ATTR_PITCH_CONTROL;
	}

	return attributes;

 error:
	usb_audio_warn(chip,
		       "%u:%d : no or invalid class specific endpoint descriptor\n",
		       iface_no, altsd->bAlternateSetting);
	return 0;
}

 
static void *
snd_usb_find_input_terminal_descriptor(struct usb_host_interface *ctrl_iface,
				       int terminal_id, int protocol)
{
	struct uac2_input_terminal_descriptor *term = NULL;

	while ((term = snd_usb_find_csint_desc(ctrl_iface->extra,
					       ctrl_iface->extralen,
					       term, UAC_INPUT_TERMINAL))) {
		if (!snd_usb_validate_audio_desc(term, protocol))
			continue;
		if (term->bTerminalID == terminal_id)
			return term;
	}

	return NULL;
}

static void *
snd_usb_find_output_terminal_descriptor(struct usb_host_interface *ctrl_iface,
					int terminal_id, int protocol)
{
	 
	struct uac2_output_terminal_descriptor *term = NULL;

	while ((term = snd_usb_find_csint_desc(ctrl_iface->extra,
					       ctrl_iface->extralen,
					       term, UAC_OUTPUT_TERMINAL))) {
		if (!snd_usb_validate_audio_desc(term, protocol))
			continue;
		if (term->bTerminalID == terminal_id)
			return term;
	}

	return NULL;
}

static struct audioformat *
audio_format_alloc_init(struct snd_usb_audio *chip,
		       struct usb_host_interface *alts,
		       int protocol, int iface_no, int altset_idx,
		       int altno, int num_channels, int clock)
{
	struct audioformat *fp;

	fp = kzalloc(sizeof(*fp), GFP_KERNEL);
	if (!fp)
		return NULL;

	fp->iface = iface_no;
	fp->altsetting = altno;
	fp->altset_idx = altset_idx;
	fp->endpoint = get_endpoint(alts, 0)->bEndpointAddress;
	fp->ep_attr = get_endpoint(alts, 0)->bmAttributes;
	fp->datainterval = snd_usb_parse_datainterval(chip, alts);
	fp->protocol = protocol;
	fp->maxpacksize = le16_to_cpu(get_endpoint(alts, 0)->wMaxPacketSize);
	fp->channels = num_channels;
	if (snd_usb_get_speed(chip->dev) == USB_SPEED_HIGH)
		fp->maxpacksize = (((fp->maxpacksize >> 11) & 3) + 1)
				* (fp->maxpacksize & 0x7ff);
	fp->clock = clock;
	INIT_LIST_HEAD(&fp->list);

	return fp;
}

static struct audioformat *
snd_usb_get_audioformat_uac12(struct snd_usb_audio *chip,
			      struct usb_host_interface *alts,
			      int protocol, int iface_no, int altset_idx,
			      int altno, int stream, int bm_quirk)
{
	struct usb_device *dev = chip->dev;
	struct uac_format_type_i_continuous_descriptor *fmt;
	unsigned int num_channels = 0, chconfig = 0;
	struct audioformat *fp;
	int clock = 0;
	u64 format;

	 
	if (protocol == UAC_VERSION_1) {
		struct uac1_as_header_descriptor *as =
			snd_usb_find_csint_desc(alts->extra, alts->extralen,
						NULL, UAC_AS_GENERAL);
		struct uac_input_terminal_descriptor *iterm;

		if (!as) {
			dev_err(&dev->dev,
				"%u:%d : UAC_AS_GENERAL descriptor not found\n",
				iface_no, altno);
			return NULL;
		}

		if (as->bLength < sizeof(*as)) {
			dev_err(&dev->dev,
				"%u:%d : invalid UAC_AS_GENERAL desc\n",
				iface_no, altno);
			return NULL;
		}

		format = le16_to_cpu(as->wFormatTag);  

		iterm = snd_usb_find_input_terminal_descriptor(chip->ctrl_intf,
							       as->bTerminalLink,
							       protocol);
		if (iterm) {
			num_channels = iterm->bNrChannels;
			chconfig = le16_to_cpu(iterm->wChannelConfig);
		}
	} else {  
		struct uac2_input_terminal_descriptor *input_term;
		struct uac2_output_terminal_descriptor *output_term;
		struct uac2_as_header_descriptor *as =
			snd_usb_find_csint_desc(alts->extra, alts->extralen,
						NULL, UAC_AS_GENERAL);

		if (!as) {
			dev_err(&dev->dev,
				"%u:%d : UAC_AS_GENERAL descriptor not found\n",
				iface_no, altno);
			return NULL;
		}

		if (as->bLength < sizeof(*as)) {
			dev_err(&dev->dev,
				"%u:%d : invalid UAC_AS_GENERAL desc\n",
				iface_no, altno);
			return NULL;
		}

		num_channels = as->bNrChannels;
		format = le32_to_cpu(as->bmFormats);
		chconfig = le32_to_cpu(as->bmChannelConfig);

		 
		input_term = snd_usb_find_input_terminal_descriptor(chip->ctrl_intf,
								    as->bTerminalLink,
								    protocol);
		if (input_term) {
			clock = input_term->bCSourceID;
			if (!chconfig && (num_channels == input_term->bNrChannels))
				chconfig = le32_to_cpu(input_term->bmChannelConfig);
			goto found_clock;
		}

		output_term = snd_usb_find_output_terminal_descriptor(chip->ctrl_intf,
								      as->bTerminalLink,
								      protocol);
		if (output_term) {
			clock = output_term->bCSourceID;
			goto found_clock;
		}

		dev_err(&dev->dev,
			"%u:%d : bogus bTerminalLink %d\n",
			iface_no, altno, as->bTerminalLink);
		return NULL;
	}

found_clock:
	 
	fmt = snd_usb_find_csint_desc(alts->extra, alts->extralen,
				      NULL, UAC_FORMAT_TYPE);
	if (!fmt) {
		dev_err(&dev->dev,
			"%u:%d : no UAC_FORMAT_TYPE desc\n",
			iface_no, altno);
		return NULL;
	}
	if (((protocol == UAC_VERSION_1) && (fmt->bLength < 8))
			|| ((protocol == UAC_VERSION_2) &&
					(fmt->bLength < 6))) {
		dev_err(&dev->dev,
			"%u:%d : invalid UAC_FORMAT_TYPE desc\n",
			iface_no, altno);
		return NULL;
	}

	 
	if (bm_quirk && fmt->bNrChannels == 1 && fmt->bSubframeSize == 2)
		return NULL;

	fp = audio_format_alloc_init(chip, alts, protocol, iface_no,
				     altset_idx, altno, num_channels, clock);
	if (!fp)
		return ERR_PTR(-ENOMEM);

	fp->attributes = parse_uac_endpoint_attributes(chip, alts, protocol,
						       iface_no);

	 
	snd_usb_audioformat_attributes_quirk(chip, fp, stream);

	 
	if (snd_usb_parse_audio_format(chip, fp, format,
					fmt, stream) < 0) {
		audioformat_free(fp);
		return NULL;
	}

	 
	if (fp->channels != num_channels)
		chconfig = 0;

	fp->chmap = convert_chmap(fp->channels, chconfig, protocol);

	return fp;
}

static struct audioformat *
snd_usb_get_audioformat_uac3(struct snd_usb_audio *chip,
			     struct usb_host_interface *alts,
			     struct snd_usb_power_domain **pd_out,
			     int iface_no, int altset_idx,
			     int altno, int stream)
{
	struct usb_device *dev = chip->dev;
	struct uac3_input_terminal_descriptor *input_term;
	struct uac3_output_terminal_descriptor *output_term;
	struct uac3_cluster_header_descriptor *cluster;
	struct uac3_as_header_descriptor *as = NULL;
	struct uac3_hc_descriptor_header hc_header;
	struct snd_pcm_chmap_elem *chmap;
	struct snd_usb_power_domain *pd;
	unsigned char badd_profile;
	u64 badd_formats = 0;
	unsigned int num_channels;
	struct audioformat *fp;
	u16 cluster_id, wLength;
	int clock = 0;
	int err;

	badd_profile = chip->badd_profile;

	if (badd_profile >= UAC3_FUNCTION_SUBCLASS_GENERIC_IO) {
		unsigned int maxpacksize =
			le16_to_cpu(get_endpoint(alts, 0)->wMaxPacketSize);

		switch (maxpacksize) {
		default:
			dev_err(&dev->dev,
				"%u:%d : incorrect wMaxPacketSize for BADD profile\n",
				iface_no, altno);
			return NULL;
		case UAC3_BADD_EP_MAXPSIZE_SYNC_MONO_16:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_MONO_16:
			badd_formats = SNDRV_PCM_FMTBIT_S16_LE;
			num_channels = 1;
			break;
		case UAC3_BADD_EP_MAXPSIZE_SYNC_MONO_24:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_MONO_24:
			badd_formats = SNDRV_PCM_FMTBIT_S24_3LE;
			num_channels = 1;
			break;
		case UAC3_BADD_EP_MAXPSIZE_SYNC_STEREO_16:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_STEREO_16:
			badd_formats = SNDRV_PCM_FMTBIT_S16_LE;
			num_channels = 2;
			break;
		case UAC3_BADD_EP_MAXPSIZE_SYNC_STEREO_24:
		case UAC3_BADD_EP_MAXPSIZE_ASYNC_STEREO_24:
			badd_formats = SNDRV_PCM_FMTBIT_S24_3LE;
			num_channels = 2;
			break;
		}

		chmap = kzalloc(sizeof(*chmap), GFP_KERNEL);
		if (!chmap)
			return ERR_PTR(-ENOMEM);

		if (num_channels == 1) {
			chmap->map[0] = SNDRV_CHMAP_MONO;
		} else {
			chmap->map[0] = SNDRV_CHMAP_FL;
			chmap->map[1] = SNDRV_CHMAP_FR;
		}

		chmap->channels = num_channels;
		clock = UAC3_BADD_CS_ID9;
		goto found_clock;
	}

	as = snd_usb_find_csint_desc(alts->extra, alts->extralen,
				     NULL, UAC_AS_GENERAL);
	if (!as) {
		dev_err(&dev->dev,
			"%u:%d : UAC_AS_GENERAL descriptor not found\n",
			iface_no, altno);
		return NULL;
	}

	if (as->bLength < sizeof(*as)) {
		dev_err(&dev->dev,
			"%u:%d : invalid UAC_AS_GENERAL desc\n",
			iface_no, altno);
		return NULL;
	}

	cluster_id = le16_to_cpu(as->wClusterDescrID);
	if (!cluster_id) {
		dev_err(&dev->dev,
			"%u:%d : no cluster descriptor\n",
			iface_no, altno);
		return NULL;
	}

	 
	err = snd_usb_ctl_msg(chip->dev,
			usb_rcvctrlpipe(chip->dev, 0),
			UAC3_CS_REQ_HIGH_CAPABILITY_DESCRIPTOR,
			USB_RECIP_INTERFACE | USB_TYPE_CLASS | USB_DIR_IN,
			cluster_id,
			snd_usb_ctrl_intf(chip),
			&hc_header, sizeof(hc_header));
	if (err < 0)
		return ERR_PTR(err);
	else if (err != sizeof(hc_header)) {
		dev_err(&dev->dev,
			"%u:%d : can't get High Capability descriptor\n",
			iface_no, altno);
		return ERR_PTR(-EIO);
	}

	 
	wLength = le16_to_cpu(hc_header.wLength);
	cluster = kzalloc(wLength, GFP_KERNEL);
	if (!cluster)
		return ERR_PTR(-ENOMEM);
	err = snd_usb_ctl_msg(chip->dev,
			usb_rcvctrlpipe(chip->dev, 0),
			UAC3_CS_REQ_HIGH_CAPABILITY_DESCRIPTOR,
			USB_RECIP_INTERFACE | USB_TYPE_CLASS | USB_DIR_IN,
			cluster_id,
			snd_usb_ctrl_intf(chip),
			cluster, wLength);
	if (err < 0) {
		kfree(cluster);
		return ERR_PTR(err);
	} else if (err != wLength) {
		dev_err(&dev->dev,
			"%u:%d : can't get Cluster Descriptor\n",
			iface_no, altno);
		kfree(cluster);
		return ERR_PTR(-EIO);
	}

	num_channels = cluster->bNrChannels;
	chmap = convert_chmap_v3(cluster);
	kfree(cluster);

	 
	input_term = snd_usb_find_input_terminal_descriptor(chip->ctrl_intf,
							    as->bTerminalLink,
							    UAC_VERSION_3);
	if (input_term) {
		clock = input_term->bCSourceID;
		goto found_clock;
	}

	output_term = snd_usb_find_output_terminal_descriptor(chip->ctrl_intf,
							      as->bTerminalLink,
							      UAC_VERSION_3);
	if (output_term) {
		clock = output_term->bCSourceID;
		goto found_clock;
	}

	dev_err(&dev->dev, "%u:%d : bogus bTerminalLink %d\n",
			iface_no, altno, as->bTerminalLink);
	kfree(chmap);
	return NULL;

found_clock:
	fp = audio_format_alloc_init(chip, alts, UAC_VERSION_3, iface_no,
				     altset_idx, altno, num_channels, clock);
	if (!fp) {
		kfree(chmap);
		return ERR_PTR(-ENOMEM);
	}

	fp->chmap = chmap;

	if (badd_profile >= UAC3_FUNCTION_SUBCLASS_GENERIC_IO) {
		fp->attributes = 0;  

		fp->fmt_type = UAC_FORMAT_TYPE_I;
		fp->formats = badd_formats;

		fp->nr_rates = 0;	 
		fp->rate_min = UAC3_BADD_SAMPLING_RATE;
		fp->rate_max = UAC3_BADD_SAMPLING_RATE;
		fp->rates = SNDRV_PCM_RATE_CONTINUOUS;

		pd = kzalloc(sizeof(*pd), GFP_KERNEL);
		if (!pd) {
			audioformat_free(fp);
			return NULL;
		}
		pd->pd_id = (stream == SNDRV_PCM_STREAM_PLAYBACK) ?
					UAC3_BADD_PD_ID10 : UAC3_BADD_PD_ID11;
		pd->pd_d1d0_rec = UAC3_BADD_PD_RECOVER_D1D0;
		pd->pd_d2d0_rec = UAC3_BADD_PD_RECOVER_D2D0;

	} else {
		fp->attributes = parse_uac_endpoint_attributes(chip, alts,
							       UAC_VERSION_3,
							       iface_no);

		pd = snd_usb_find_power_domain(chip->ctrl_intf,
					       as->bTerminalLink);

		 
		if (snd_usb_parse_audio_format_v3(chip, fp, as, stream) < 0) {
			kfree(pd);
			audioformat_free(fp);
			return NULL;
		}
	}

	if (pd)
		*pd_out = pd;

	return fp;
}

static int __snd_usb_parse_audio_interface(struct snd_usb_audio *chip,
					   int iface_no,
					   bool *has_non_pcm, bool non_pcm)
{
	struct usb_device *dev;
	struct usb_interface *iface;
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	int i, altno, err, stream;
	struct audioformat *fp = NULL;
	struct snd_usb_power_domain *pd = NULL;
	bool set_iface_first;
	int num, protocol;

	dev = chip->dev;

	 
	iface = usb_ifnum_to_if(dev, iface_no);

	num = iface->num_altsetting;

	 
	if (chip->usb_id == USB_ID(0x04fa, 0x4201) && num >= 4)
		num = 4;

	for (i = 0; i < num; i++) {
		alts = &iface->altsetting[i];
		altsd = get_iface_desc(alts);
		protocol = altsd->bInterfaceProtocol;
		 
		if (((altsd->bInterfaceClass != USB_CLASS_AUDIO ||
		      (altsd->bInterfaceSubClass != USB_SUBCLASS_AUDIOSTREAMING &&
		       altsd->bInterfaceSubClass != USB_SUBCLASS_VENDOR_SPEC)) &&
		     altsd->bInterfaceClass != USB_CLASS_VENDOR_SPEC) ||
		    altsd->bNumEndpoints < 1 ||
		    le16_to_cpu(get_endpoint(alts, 0)->wMaxPacketSize) == 0)
			continue;
		 
		if ((get_endpoint(alts, 0)->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) !=
		    USB_ENDPOINT_XFER_ISOC)
			continue;
		 
		stream = (get_endpoint(alts, 0)->bEndpointAddress & USB_DIR_IN) ?
			SNDRV_PCM_STREAM_CAPTURE : SNDRV_PCM_STREAM_PLAYBACK;
		altno = altsd->bAlternateSetting;

		if (snd_usb_apply_interface_quirk(chip, iface_no, altno))
			continue;

		 
		if (USB_ID_VENDOR(chip->usb_id) == 0x0582 &&
		    altsd->bInterfaceClass == USB_CLASS_VENDOR_SPEC &&
		    protocol <= 2)
			protocol = UAC_VERSION_1;

		switch (protocol) {
		default:
			dev_dbg(&dev->dev, "%u:%d: unknown interface protocol %#02x, assuming v1\n",
				iface_no, altno, protocol);
			protocol = UAC_VERSION_1;
			fallthrough;
		case UAC_VERSION_1:
		case UAC_VERSION_2: {
			int bm_quirk = 0;

			 
			if (altno == 2 && num == 3 &&
			    fp && fp->altsetting == 1 && fp->channels == 1 &&
			    fp->formats == SNDRV_PCM_FMTBIT_S16_LE &&
			    protocol == UAC_VERSION_1 &&
			    le16_to_cpu(get_endpoint(alts, 0)->wMaxPacketSize) ==
							fp->maxpacksize * 2)
				bm_quirk = 1;

			fp = snd_usb_get_audioformat_uac12(chip, alts, protocol,
							   iface_no, i, altno,
							   stream, bm_quirk);
			break;
		}
		case UAC_VERSION_3:
			fp = snd_usb_get_audioformat_uac3(chip, alts, &pd,
						iface_no, i, altno, stream);
			break;
		}

		if (!fp)
			continue;
		else if (IS_ERR(fp))
			return PTR_ERR(fp);

		if (fp->fmt_type != UAC_FORMAT_TYPE_I)
			*has_non_pcm = true;
		if ((fp->fmt_type == UAC_FORMAT_TYPE_I) == non_pcm) {
			audioformat_free(fp);
			kfree(pd);
			fp = NULL;
			pd = NULL;
			continue;
		}

		snd_usb_audioformat_set_sync_ep(chip, fp);

		dev_dbg(&dev->dev, "%u:%d: add audio endpoint %#x\n", iface_no, altno, fp->endpoint);
		if (protocol == UAC_VERSION_3)
			err = snd_usb_add_audio_stream_v3(chip, stream, fp, pd);
		else
			err = snd_usb_add_audio_stream(chip, stream, fp);

		if (err < 0) {
			audioformat_free(fp);
			kfree(pd);
			return err;
		}

		 
		err = snd_usb_add_endpoint(chip, fp->endpoint,
					   SND_USB_ENDPOINT_TYPE_DATA);
		if (err < 0)
			return err;

		if (fp->sync_ep) {
			err = snd_usb_add_endpoint(chip, fp->sync_ep,
						   fp->implicit_fb ?
						   SND_USB_ENDPOINT_TYPE_DATA :
						   SND_USB_ENDPOINT_TYPE_SYNC);
			if (err < 0)
				return err;
		}

		set_iface_first = false;
		if (protocol == UAC_VERSION_1 ||
		    (chip->quirk_flags & QUIRK_FLAG_SET_IFACE_FIRST))
			set_iface_first = true;

		 
		usb_set_interface(chip->dev, iface_no, 0);
		if (set_iface_first)
			usb_set_interface(chip->dev, iface_no, altno);
		snd_usb_init_pitch(chip, fp);
		snd_usb_init_sample_rate(chip, fp, fp->rate_max);
		if (!set_iface_first)
			usb_set_interface(chip->dev, iface_no, altno);
	}
	return 0;
}

int snd_usb_parse_audio_interface(struct snd_usb_audio *chip, int iface_no)
{
	int err;
	bool has_non_pcm = false;

	 
	err = __snd_usb_parse_audio_interface(chip, iface_no, &has_non_pcm, false);
	if (err < 0)
		return err;

	if (has_non_pcm) {
		 
		err = __snd_usb_parse_audio_interface(chip, iface_no, &has_non_pcm, true);
		if (err < 0)
			return err;
	}

	return 0;
}

