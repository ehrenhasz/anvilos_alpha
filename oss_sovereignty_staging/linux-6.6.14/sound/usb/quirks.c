
 

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/usb/midi.h>
#include <linux/bits.h>

#include <sound/control.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/pcm.h>

#include "usbaudio.h"
#include "card.h"
#include "mixer.h"
#include "mixer_quirks.h"
#include "midi.h"
#include "midi2.h"
#include "quirks.h"
#include "helper.h"
#include "endpoint.h"
#include "pcm.h"
#include "clock.h"
#include "stream.h"

 
static int create_composite_quirk(struct snd_usb_audio *chip,
				  struct usb_interface *iface,
				  struct usb_driver *driver,
				  const struct snd_usb_audio_quirk *quirk_comp)
{
	int probed_ifnum = get_iface_desc(iface->altsetting)->bInterfaceNumber;
	const struct snd_usb_audio_quirk *quirk;
	int err;

	for (quirk = quirk_comp->data; quirk->ifnum >= 0; ++quirk) {
		iface = usb_ifnum_to_if(chip->dev, quirk->ifnum);
		if (!iface)
			continue;
		if (quirk->ifnum != probed_ifnum &&
		    usb_interface_claimed(iface))
			continue;
		err = snd_usb_create_quirk(chip, iface, driver, quirk);
		if (err < 0)
			return err;
	}

	for (quirk = quirk_comp->data; quirk->ifnum >= 0; ++quirk) {
		iface = usb_ifnum_to_if(chip->dev, quirk->ifnum);
		if (!iface)
			continue;
		if (quirk->ifnum != probed_ifnum &&
		    !usb_interface_claimed(iface)) {
			err = usb_driver_claim_interface(driver, iface,
							 USB_AUDIO_IFACE_UNUSED);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

static int ignore_interface_quirk(struct snd_usb_audio *chip,
				  struct usb_interface *iface,
				  struct usb_driver *driver,
				  const struct snd_usb_audio_quirk *quirk)
{
	return 0;
}


static int create_any_midi_quirk(struct snd_usb_audio *chip,
				 struct usb_interface *intf,
				 struct usb_driver *driver,
				 const struct snd_usb_audio_quirk *quirk)
{
	return snd_usb_midi_v2_create(chip, intf, quirk, 0);
}

 
static int create_standard_audio_quirk(struct snd_usb_audio *chip,
				       struct usb_interface *iface,
				       struct usb_driver *driver,
				       const struct snd_usb_audio_quirk *quirk)
{
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	int err;

	alts = &iface->altsetting[0];
	altsd = get_iface_desc(alts);
	err = snd_usb_parse_audio_interface(chip, altsd->bInterfaceNumber);
	if (err < 0) {
		usb_audio_err(chip, "cannot setup if %d: error %d\n",
			   altsd->bInterfaceNumber, err);
		return err;
	}
	 
	usb_set_interface(chip->dev, altsd->bInterfaceNumber, 0);
	return 0;
}

 
static int add_audio_stream_from_fixed_fmt(struct snd_usb_audio *chip,
					   struct audioformat *fp)
{
	int stream, err;

	stream = (fp->endpoint & USB_DIR_IN) ?
		SNDRV_PCM_STREAM_CAPTURE : SNDRV_PCM_STREAM_PLAYBACK;

	snd_usb_audioformat_set_sync_ep(chip, fp);

	err = snd_usb_add_audio_stream(chip, stream, fp);
	if (err < 0)
		return err;

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

	return 0;
}

 
static int create_fixed_stream_quirk(struct snd_usb_audio *chip,
				     struct usb_interface *iface,
				     struct usb_driver *driver,
				     const struct snd_usb_audio_quirk *quirk)
{
	struct audioformat *fp;
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	unsigned *rate_table = NULL;
	int err;

	fp = kmemdup(quirk->data, sizeof(*fp), GFP_KERNEL);
	if (!fp)
		return -ENOMEM;

	INIT_LIST_HEAD(&fp->list);
	if (fp->nr_rates > MAX_NR_RATES) {
		kfree(fp);
		return -EINVAL;
	}
	if (fp->nr_rates > 0) {
		rate_table = kmemdup(fp->rate_table,
				     sizeof(int) * fp->nr_rates, GFP_KERNEL);
		if (!rate_table) {
			kfree(fp);
			return -ENOMEM;
		}
		fp->rate_table = rate_table;
	}

	if (fp->iface != get_iface_desc(&iface->altsetting[0])->bInterfaceNumber ||
	    fp->altset_idx >= iface->num_altsetting) {
		err = -EINVAL;
		goto error;
	}
	alts = &iface->altsetting[fp->altset_idx];
	altsd = get_iface_desc(alts);
	if (altsd->bNumEndpoints <= fp->ep_idx) {
		err = -EINVAL;
		goto error;
	}

	fp->protocol = altsd->bInterfaceProtocol;

	if (fp->datainterval == 0)
		fp->datainterval = snd_usb_parse_datainterval(chip, alts);
	if (fp->maxpacksize == 0)
		fp->maxpacksize = le16_to_cpu(get_endpoint(alts, fp->ep_idx)->wMaxPacketSize);
	if (!fp->fmt_type)
		fp->fmt_type = UAC_FORMAT_TYPE_I;

	err = add_audio_stream_from_fixed_fmt(chip, fp);
	if (err < 0)
		goto error;

	usb_set_interface(chip->dev, fp->iface, 0);
	snd_usb_init_pitch(chip, fp);
	snd_usb_init_sample_rate(chip, fp, fp->rate_max);
	return 0;

 error:
	list_del(&fp->list);  
	kfree(fp);
	kfree(rate_table);
	return err;
}

static int create_auto_pcm_quirk(struct snd_usb_audio *chip,
				 struct usb_interface *iface,
				 struct usb_driver *driver)
{
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	struct usb_endpoint_descriptor *epd;
	struct uac1_as_header_descriptor *ashd;
	struct uac_format_type_i_discrete_descriptor *fmtd;

	 

	 
	if (iface->num_altsetting < 2)
		return -ENODEV;
	alts = &iface->altsetting[1];
	altsd = get_iface_desc(alts);

	 
	if (altsd->bNumEndpoints < 1)
		return -ENODEV;
	epd = get_endpoint(alts, 0);
	if (!usb_endpoint_xfer_isoc(epd))
		return -ENODEV;

	 
	ashd = snd_usb_find_csint_desc(alts->extra, alts->extralen, NULL,
				       UAC_AS_GENERAL);
	fmtd = snd_usb_find_csint_desc(alts->extra, alts->extralen, NULL,
				       UAC_FORMAT_TYPE);
	if (!ashd || ashd->bLength < 7 ||
	    !fmtd || fmtd->bLength < 8)
		return -ENODEV;

	return create_standard_audio_quirk(chip, iface, driver, NULL);
}

static int create_yamaha_midi_quirk(struct snd_usb_audio *chip,
				    struct usb_interface *iface,
				    struct usb_driver *driver,
				    struct usb_host_interface *alts)
{
	static const struct snd_usb_audio_quirk yamaha_midi_quirk = {
		.type = QUIRK_MIDI_YAMAHA
	};
	struct usb_midi_in_jack_descriptor *injd;
	struct usb_midi_out_jack_descriptor *outjd;

	 
	injd = snd_usb_find_csint_desc(alts->extra, alts->extralen,
				       NULL, USB_MS_MIDI_IN_JACK);
	outjd = snd_usb_find_csint_desc(alts->extra, alts->extralen,
					NULL, USB_MS_MIDI_OUT_JACK);
	if (!injd && !outjd)
		return -ENODEV;
	if ((injd && !snd_usb_validate_midi_desc(injd)) ||
	    (outjd && !snd_usb_validate_midi_desc(outjd)))
		return -ENODEV;
	if (injd && (injd->bLength < 5 ||
		     (injd->bJackType != USB_MS_EMBEDDED &&
		      injd->bJackType != USB_MS_EXTERNAL)))
		return -ENODEV;
	if (outjd && (outjd->bLength < 6 ||
		      (outjd->bJackType != USB_MS_EMBEDDED &&
		       outjd->bJackType != USB_MS_EXTERNAL)))
		return -ENODEV;
	return create_any_midi_quirk(chip, iface, driver, &yamaha_midi_quirk);
}

static int create_roland_midi_quirk(struct snd_usb_audio *chip,
				    struct usb_interface *iface,
				    struct usb_driver *driver,
				    struct usb_host_interface *alts)
{
	static const struct snd_usb_audio_quirk roland_midi_quirk = {
		.type = QUIRK_MIDI_ROLAND
	};
	u8 *roland_desc = NULL;

	 
	for (;;) {
		roland_desc = snd_usb_find_csint_desc(alts->extra,
						      alts->extralen,
						      roland_desc, 0xf1);
		if (!roland_desc)
			return -ENODEV;
		if (roland_desc[0] < 6 || roland_desc[3] != 2)
			continue;
		return create_any_midi_quirk(chip, iface, driver,
					     &roland_midi_quirk);
	}
}

static int create_std_midi_quirk(struct snd_usb_audio *chip,
				 struct usb_interface *iface,
				 struct usb_driver *driver,
				 struct usb_host_interface *alts)
{
	struct usb_ms_header_descriptor *mshd;
	struct usb_ms_endpoint_descriptor *msepd;

	 
	mshd = (struct usb_ms_header_descriptor *)alts->extra;
	if (alts->extralen < 7 ||
	    mshd->bLength < 7 ||
	    mshd->bDescriptorType != USB_DT_CS_INTERFACE ||
	    mshd->bDescriptorSubtype != USB_MS_HEADER)
		return -ENODEV;
	 
	msepd = (struct usb_ms_endpoint_descriptor *)alts->endpoint[0].extra;
	if (alts->endpoint[0].extralen < 4 ||
	    msepd->bLength < 4 ||
	    msepd->bDescriptorType != USB_DT_CS_ENDPOINT ||
	    msepd->bDescriptorSubtype != UAC_MS_GENERAL ||
	    msepd->bNumEmbMIDIJack < 1 ||
	    msepd->bNumEmbMIDIJack > 16)
		return -ENODEV;

	return create_any_midi_quirk(chip, iface, driver, NULL);
}

static int create_auto_midi_quirk(struct snd_usb_audio *chip,
				  struct usb_interface *iface,
				  struct usb_driver *driver)
{
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	struct usb_endpoint_descriptor *epd;
	int err;

	alts = &iface->altsetting[0];
	altsd = get_iface_desc(alts);

	 
	if (altsd->bNumEndpoints < 1)
		return -ENODEV;
	epd = get_endpoint(alts, 0);
	if (!usb_endpoint_xfer_bulk(epd) &&
	    !usb_endpoint_xfer_int(epd))
		return -ENODEV;

	switch (USB_ID_VENDOR(chip->usb_id)) {
	case 0x0499:  
		err = create_yamaha_midi_quirk(chip, iface, driver, alts);
		if (err != -ENODEV)
			return err;
		break;
	case 0x0582:  
		err = create_roland_midi_quirk(chip, iface, driver, alts);
		if (err != -ENODEV)
			return err;
		break;
	}

	return create_std_midi_quirk(chip, iface, driver, alts);
}

static int create_autodetect_quirk(struct snd_usb_audio *chip,
				   struct usb_interface *iface,
				   struct usb_driver *driver,
				   const struct snd_usb_audio_quirk *quirk)
{
	int err;

	err = create_auto_pcm_quirk(chip, iface, driver);
	if (err == -ENODEV)
		err = create_auto_midi_quirk(chip, iface, driver);
	return err;
}

 
static int create_uaxx_quirk(struct snd_usb_audio *chip,
			     struct usb_interface *iface,
			     struct usb_driver *driver,
			     const struct snd_usb_audio_quirk *quirk)
{
	static const struct audioformat ua_format = {
		.formats = SNDRV_PCM_FMTBIT_S24_3LE,
		.channels = 2,
		.fmt_type = UAC_FORMAT_TYPE_I,
		.altsetting = 1,
		.altset_idx = 1,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
	};
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	struct audioformat *fp;
	int err;

	 
	if (iface->num_altsetting < 2)
		return -ENXIO;
	alts = &iface->altsetting[1];
	altsd = get_iface_desc(alts);

	if (altsd->bNumEndpoints == 2) {
		static const struct snd_usb_midi_endpoint_info ua700_ep = {
			.out_cables = 0x0003,
			.in_cables  = 0x0003
		};
		static const struct snd_usb_audio_quirk ua700_quirk = {
			.type = QUIRK_MIDI_FIXED_ENDPOINT,
			.data = &ua700_ep
		};
		static const struct snd_usb_midi_endpoint_info uaxx_ep = {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		};
		static const struct snd_usb_audio_quirk uaxx_quirk = {
			.type = QUIRK_MIDI_FIXED_ENDPOINT,
			.data = &uaxx_ep
		};
		const struct snd_usb_audio_quirk *quirk =
			chip->usb_id == USB_ID(0x0582, 0x002b)
			? &ua700_quirk : &uaxx_quirk;
		return __snd_usbmidi_create(chip->card, iface,
					    &chip->midi_list, quirk,
					    chip->usb_id,
					    &chip->num_rawmidis);
	}

	if (altsd->bNumEndpoints != 1)
		return -ENXIO;

	fp = kmemdup(&ua_format, sizeof(*fp), GFP_KERNEL);
	if (!fp)
		return -ENOMEM;

	fp->iface = altsd->bInterfaceNumber;
	fp->endpoint = get_endpoint(alts, 0)->bEndpointAddress;
	fp->ep_attr = get_endpoint(alts, 0)->bmAttributes;
	fp->datainterval = 0;
	fp->maxpacksize = le16_to_cpu(get_endpoint(alts, 0)->wMaxPacketSize);
	INIT_LIST_HEAD(&fp->list);

	switch (fp->maxpacksize) {
	case 0x120:
		fp->rate_max = fp->rate_min = 44100;
		break;
	case 0x138:
	case 0x140:
		fp->rate_max = fp->rate_min = 48000;
		break;
	case 0x258:
	case 0x260:
		fp->rate_max = fp->rate_min = 96000;
		break;
	default:
		usb_audio_err(chip, "unknown sample rate\n");
		kfree(fp);
		return -ENXIO;
	}

	err = add_audio_stream_from_fixed_fmt(chip, fp);
	if (err < 0) {
		list_del(&fp->list);  
		kfree(fp);
		return err;
	}
	usb_set_interface(chip->dev, fp->iface, 0);
	return 0;
}

 
static int create_standard_mixer_quirk(struct snd_usb_audio *chip,
				       struct usb_interface *iface,
				       struct usb_driver *driver,
				       const struct snd_usb_audio_quirk *quirk)
{
	if (quirk->ifnum < 0)
		return 0;

	return snd_usb_create_mixer(chip, quirk->ifnum);
}

 
int snd_usb_create_quirk(struct snd_usb_audio *chip,
			 struct usb_interface *iface,
			 struct usb_driver *driver,
			 const struct snd_usb_audio_quirk *quirk)
{
	typedef int (*quirk_func_t)(struct snd_usb_audio *,
				    struct usb_interface *,
				    struct usb_driver *,
				    const struct snd_usb_audio_quirk *);
	static const quirk_func_t quirk_funcs[] = {
		[QUIRK_IGNORE_INTERFACE] = ignore_interface_quirk,
		[QUIRK_COMPOSITE] = create_composite_quirk,
		[QUIRK_AUTODETECT] = create_autodetect_quirk,
		[QUIRK_MIDI_STANDARD_INTERFACE] = create_any_midi_quirk,
		[QUIRK_MIDI_FIXED_ENDPOINT] = create_any_midi_quirk,
		[QUIRK_MIDI_YAMAHA] = create_any_midi_quirk,
		[QUIRK_MIDI_ROLAND] = create_any_midi_quirk,
		[QUIRK_MIDI_MIDIMAN] = create_any_midi_quirk,
		[QUIRK_MIDI_NOVATION] = create_any_midi_quirk,
		[QUIRK_MIDI_RAW_BYTES] = create_any_midi_quirk,
		[QUIRK_MIDI_EMAGIC] = create_any_midi_quirk,
		[QUIRK_MIDI_CME] = create_any_midi_quirk,
		[QUIRK_MIDI_AKAI] = create_any_midi_quirk,
		[QUIRK_MIDI_FTDI] = create_any_midi_quirk,
		[QUIRK_MIDI_CH345] = create_any_midi_quirk,
		[QUIRK_AUDIO_STANDARD_INTERFACE] = create_standard_audio_quirk,
		[QUIRK_AUDIO_FIXED_ENDPOINT] = create_fixed_stream_quirk,
		[QUIRK_AUDIO_EDIROL_UAXX] = create_uaxx_quirk,
		[QUIRK_AUDIO_STANDARD_MIXER] = create_standard_mixer_quirk,
	};

	if (quirk->type < QUIRK_TYPE_COUNT) {
		return quirk_funcs[quirk->type](chip, iface, driver, quirk);
	} else {
		usb_audio_err(chip, "invalid quirk type %d\n", quirk->type);
		return -ENXIO;
	}
}

 

#define EXTIGY_FIRMWARE_SIZE_OLD 794
#define EXTIGY_FIRMWARE_SIZE_NEW 483

static int snd_usb_extigy_boot_quirk(struct usb_device *dev, struct usb_interface *intf)
{
	struct usb_host_config *config = dev->actconfig;
	int err;

	if (le16_to_cpu(get_cfg_desc(config)->wTotalLength) == EXTIGY_FIRMWARE_SIZE_OLD ||
	    le16_to_cpu(get_cfg_desc(config)->wTotalLength) == EXTIGY_FIRMWARE_SIZE_NEW) {
		dev_dbg(&dev->dev, "sending Extigy boot sequence...\n");
		 
		err = snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev,0),
				      0x10, 0x43, 0x0001, 0x000a, NULL, 0);
		if (err < 0)
			dev_dbg(&dev->dev, "error sending boot message: %d\n", err);
		err = usb_get_descriptor(dev, USB_DT_DEVICE, 0,
				&dev->descriptor, sizeof(dev->descriptor));
		config = dev->actconfig;
		if (err < 0)
			dev_dbg(&dev->dev, "error usb_get_descriptor: %d\n", err);
		err = usb_reset_configuration(dev);
		if (err < 0)
			dev_dbg(&dev->dev, "error usb_reset_configuration: %d\n", err);
		dev_dbg(&dev->dev, "extigy_boot: new boot length = %d\n",
			    le16_to_cpu(get_cfg_desc(config)->wTotalLength));
		return -ENODEV;  
	}
	return 0;
}

static int snd_usb_audigy2nx_boot_quirk(struct usb_device *dev)
{
	u8 buf = 1;

	snd_usb_ctl_msg(dev, usb_rcvctrlpipe(dev, 0), 0x2a,
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_OTHER,
			0, 0, &buf, 1);
	if (buf == 0) {
		snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0), 0x29,
				USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_OTHER,
				1, 2000, NULL, 0);
		return -ENODEV;
	}
	return 0;
}

static int snd_usb_fasttrackpro_boot_quirk(struct usb_device *dev)
{
	int err;

	if (dev->actconfig->desc.bConfigurationValue == 1) {
		dev_info(&dev->dev,
			   "Fast Track Pro switching to config #2\n");
		 
		err = usb_driver_set_configuration(dev, 2);
		if (err < 0)
			dev_dbg(&dev->dev,
				"error usb_driver_set_configuration: %d\n",
				err);
		 
		return -ENODEV;
	} else
		dev_info(&dev->dev, "Fast Track Pro config OK\n");

	return 0;
}

 
static int snd_usb_cm106_write_int_reg(struct usb_device *dev, int reg, u16 value)
{
	u8 buf[4];
	buf[0] = 0x20;
	buf[1] = value & 0xff;
	buf[2] = (value >> 8) & 0xff;
	buf[3] = reg;
	return snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0), USB_REQ_SET_CONFIGURATION,
			       USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_ENDPOINT,
			       0, 0, &buf, 4);
}

static int snd_usb_cm106_boot_quirk(struct usb_device *dev)
{
	 
	return snd_usb_cm106_write_int_reg(dev, 2, 0x8004);
}

 
#define CM6206_REG0_DMA_MASTER BIT(15)
#define CM6206_REG0_SPDIFO_RATE_48K (2 << 12)
#define CM6206_REG0_SPDIFO_RATE_96K (7 << 12)
 
#define CM6206_REG0_SPDIFO_CAT_CODE_GENERAL (0 << 4)
#define CM6206_REG0_SPDIFO_EMPHASIS_CD BIT(3)
#define CM6206_REG0_SPDIFO_COPYRIGHT_NA BIT(2)
#define CM6206_REG0_SPDIFO_NON_AUDIO BIT(1)
#define CM6206_REG0_SPDIFO_PRO_FORMAT BIT(0)

#define CM6206_REG1_TEST_SEL_CLK BIT(14)
#define CM6206_REG1_PLLBIN_EN BIT(13)
#define CM6206_REG1_SOFT_MUTE_EN BIT(12)
#define CM6206_REG1_GPIO4_OUT BIT(11)
#define CM6206_REG1_GPIO4_OE BIT(10)
#define CM6206_REG1_GPIO3_OUT BIT(9)
#define CM6206_REG1_GPIO3_OE BIT(8)
#define CM6206_REG1_GPIO2_OUT BIT(7)
#define CM6206_REG1_GPIO2_OE BIT(6)
#define CM6206_REG1_GPIO1_OUT BIT(5)
#define CM6206_REG1_GPIO1_OE BIT(4)
#define CM6206_REG1_SPDIFO_INVALID BIT(3)
#define CM6206_REG1_SPDIF_LOOP_EN BIT(2)
#define CM6206_REG1_SPDIFO_DIS BIT(1)
#define CM6206_REG1_SPDIFI_MIX BIT(0)

#define CM6206_REG2_DRIVER_ON BIT(15)
#define CM6206_REG2_HEADP_SEL_SIDE_CHANNELS (0 << 13)
#define CM6206_REG2_HEADP_SEL_SURROUND_CHANNELS (1 << 13)
#define CM6206_REG2_HEADP_SEL_CENTER_SUBW (2 << 13)
#define CM6206_REG2_HEADP_SEL_FRONT_CHANNELS (3 << 13)
#define CM6206_REG2_MUTE_HEADPHONE_RIGHT BIT(12)
#define CM6206_REG2_MUTE_HEADPHONE_LEFT BIT(11)
#define CM6206_REG2_MUTE_REAR_SURROUND_RIGHT BIT(10)
#define CM6206_REG2_MUTE_REAR_SURROUND_LEFT BIT(9)
#define CM6206_REG2_MUTE_SIDE_SURROUND_RIGHT BIT(8)
#define CM6206_REG2_MUTE_SIDE_SURROUND_LEFT BIT(7)
#define CM6206_REG2_MUTE_SUBWOOFER BIT(6)
#define CM6206_REG2_MUTE_CENTER BIT(5)
#define CM6206_REG2_MUTE_RIGHT_FRONT BIT(3)
#define CM6206_REG2_MUTE_LEFT_FRONT BIT(3)
#define CM6206_REG2_EN_BTL BIT(2)
#define CM6206_REG2_MCUCLKSEL_1_5_MHZ (0)
#define CM6206_REG2_MCUCLKSEL_3_MHZ (1)
#define CM6206_REG2_MCUCLKSEL_6_MHZ (2)
#define CM6206_REG2_MCUCLKSEL_12_MHZ (3)

 
#define CM6206_REG3_FLYSPEED_DEFAULT (2 << 11)
#define CM6206_REG3_VRAP25EN BIT(10)
#define CM6206_REG3_MSEL1 BIT(9)
#define CM6206_REG3_SPDIFI_RATE_44_1K BIT(0 << 7)
#define CM6206_REG3_SPDIFI_RATE_48K BIT(2 << 7)
#define CM6206_REG3_SPDIFI_RATE_32K BIT(3 << 7)
#define CM6206_REG3_PINSEL BIT(6)
#define CM6206_REG3_FOE BIT(5)
#define CM6206_REG3_ROE BIT(4)
#define CM6206_REG3_CBOE BIT(3)
#define CM6206_REG3_LOSE BIT(2)
#define CM6206_REG3_HPOE BIT(1)
#define CM6206_REG3_SPDIFI_CANREC BIT(0)

#define CM6206_REG5_DA_RSTN BIT(13)
#define CM6206_REG5_AD_RSTN BIT(12)
#define CM6206_REG5_SPDIFO_AD2SPDO BIT(12)
#define CM6206_REG5_SPDIFO_SEL_FRONT (0 << 9)
#define CM6206_REG5_SPDIFO_SEL_SIDE_SUR (1 << 9)
#define CM6206_REG5_SPDIFO_SEL_CEN_LFE (2 << 9)
#define CM6206_REG5_SPDIFO_SEL_REAR_SUR (3 << 9)
#define CM6206_REG5_CODECM BIT(8)
#define CM6206_REG5_EN_HPF BIT(7)
#define CM6206_REG5_T_SEL_DSDA4 BIT(6)
#define CM6206_REG5_T_SEL_DSDA3 BIT(5)
#define CM6206_REG5_T_SEL_DSDA2 BIT(4)
#define CM6206_REG5_T_SEL_DSDA1 BIT(3)
#define CM6206_REG5_T_SEL_DSDAD_NORMAL 0
#define CM6206_REG5_T_SEL_DSDAD_FRONT 4
#define CM6206_REG5_T_SEL_DSDAD_S_SURROUND 5
#define CM6206_REG5_T_SEL_DSDAD_CEN_LFE 6
#define CM6206_REG5_T_SEL_DSDAD_R_SURROUND 7

static int snd_usb_cm6206_boot_quirk(struct usb_device *dev)
{
	int err  = 0, reg;
	int val[] = {
		 
		CM6206_REG0_SPDIFO_RATE_48K |
		CM6206_REG0_SPDIFO_COPYRIGHT_NA,
		 
		CM6206_REG1_PLLBIN_EN |
		CM6206_REG1_SOFT_MUTE_EN,
		 
		CM6206_REG2_DRIVER_ON |
		CM6206_REG2_HEADP_SEL_FRONT_CHANNELS |
		CM6206_REG2_MUTE_HEADPHONE_RIGHT |
		CM6206_REG2_MUTE_HEADPHONE_LEFT,
		 
		CM6206_REG3_FLYSPEED_DEFAULT |
		CM6206_REG3_VRAP25EN |
		CM6206_REG3_FOE |
		CM6206_REG3_ROE |
		CM6206_REG3_CBOE |
		CM6206_REG3_LOSE |
		CM6206_REG3_HPOE |
		CM6206_REG3_SPDIFI_CANREC,
		 
		0x0000,
		 
		CM6206_REG5_DA_RSTN |
		CM6206_REG5_AD_RSTN };

	for (reg = 0; reg < ARRAY_SIZE(val); reg++) {
		err = snd_usb_cm106_write_int_reg(dev, reg, val[reg]);
		if (err < 0)
			return err;
	}

	return err;
}

 
static int snd_usb_gamecon780_boot_quirk(struct usb_device *dev)
{
	 
	u8 buf[2] = { 0x74, 0xe3 };
	return snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0), UAC_SET_CUR,
			USB_RECIP_INTERFACE | USB_TYPE_CLASS | USB_DIR_OUT,
			UAC_FU_VOLUME << 8, 9 << 8, buf, 2);
}

 
static int snd_usb_novation_boot_quirk(struct usb_device *dev)
{
	 
	usb_set_interface(dev, 0, 1);
	return 0;
}

 
static int snd_usb_accessmusic_boot_quirk(struct usb_device *dev)
{
	int err, actual_length;
	 
	static const u8 seq[] = { 0x4e, 0x73, 0x52, 0x01 };
	void *buf;

	if (usb_pipe_type_check(dev, usb_sndintpipe(dev, 0x05)))
		return -EINVAL;
	buf = kmemdup(seq, ARRAY_SIZE(seq), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	err = usb_interrupt_msg(dev, usb_sndintpipe(dev, 0x05), buf,
			ARRAY_SIZE(seq), &actual_length, 1000);
	kfree(buf);
	if (err < 0)
		return err;

	return 0;
}

 

static int snd_usb_nativeinstruments_boot_quirk(struct usb_device *dev)
{
	int ret;

	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				  0xaf, USB_TYPE_VENDOR | USB_RECIP_DEVICE,
				  1, 0, NULL, 0, 1000);

	if (ret < 0)
		return ret;

	usb_reset_device(dev);

	 
	return -EAGAIN;
}

static void mbox2_setup_48_24_magic(struct usb_device *dev)
{
	u8 srate[3];
	u8 temp[12];

	 
	srate[0] = 0x80;
	srate[1] = 0xbb;
	srate[2] = 0x00;

	 
	snd_usb_ctl_msg(dev, usb_rcvctrlpipe(dev, 0),
		0x01, 0x22, 0x0100, 0x0085, &temp, 0x0003);
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
		0x81, 0xa2, 0x0100, 0x0085, &srate, 0x0003);
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
		0x81, 0xa2, 0x0100, 0x0086, &srate, 0x0003);
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
		0x81, 0xa2, 0x0100, 0x0003, &srate, 0x0003);
	return;
}

 

#define MBOX2_FIRMWARE_SIZE    646
#define MBOX2_BOOT_LOADING     0x01  
#define MBOX2_BOOT_READY       0x02  

static int snd_usb_mbox2_boot_quirk(struct usb_device *dev)
{
	struct usb_host_config *config = dev->actconfig;
	int err;
	u8 bootresponse[0x12];
	int fwsize;
	int count;

	fwsize = le16_to_cpu(get_cfg_desc(config)->wTotalLength);

	if (fwsize != MBOX2_FIRMWARE_SIZE) {
		dev_err(&dev->dev, "Invalid firmware size=%d.\n", fwsize);
		return -ENODEV;
	}

	dev_dbg(&dev->dev, "Sending Digidesign Mbox 2 boot sequence...\n");

	count = 0;
	bootresponse[0] = MBOX2_BOOT_LOADING;
	while ((bootresponse[0] == MBOX2_BOOT_LOADING) && (count < 10)) {
		msleep(500);  
		snd_usb_ctl_msg(dev, usb_rcvctrlpipe(dev, 0),
			 
			0x85, 0xc0, 0x0001, 0x0000, &bootresponse, 0x0012);
		if (bootresponse[0] == MBOX2_BOOT_READY)
			break;
		dev_dbg(&dev->dev, "device not ready, resending boot sequence...\n");
		count++;
	}

	if (bootresponse[0] != MBOX2_BOOT_READY) {
		dev_err(&dev->dev, "Unknown bootresponse=%d, or timed out, ignoring device.\n", bootresponse[0]);
		return -ENODEV;
	}

	dev_dbg(&dev->dev, "device initialised!\n");

	err = usb_get_descriptor(dev, USB_DT_DEVICE, 0,
		&dev->descriptor, sizeof(dev->descriptor));
	config = dev->actconfig;
	if (err < 0)
		dev_dbg(&dev->dev, "error usb_get_descriptor: %d\n", err);

	err = usb_reset_configuration(dev);
	if (err < 0)
		dev_dbg(&dev->dev, "error usb_reset_configuration: %d\n", err);
	dev_dbg(&dev->dev, "mbox2_boot: new boot length = %d\n",
		le16_to_cpu(get_cfg_desc(config)->wTotalLength));

	mbox2_setup_48_24_magic(dev);

	dev_info(&dev->dev, "Digidesign Mbox 2: 24bit 48kHz");

	return 0;  
}

static int snd_usb_axefx3_boot_quirk(struct usb_device *dev)
{
	int err;

	dev_dbg(&dev->dev, "Waiting for Axe-Fx III to boot up...\n");

	 
	err = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				USB_REQ_SET_INTERFACE, USB_RECIP_INTERFACE,
				1, 1, NULL, 0, 120000);
	if (err < 0) {
		dev_err(&dev->dev,
			"failed waiting for Axe-Fx III to boot: %d\n", err);
		return err;
	}

	dev_dbg(&dev->dev, "Axe-Fx III is now ready\n");

	err = usb_set_interface(dev, 1, 0);
	if (err < 0)
		dev_dbg(&dev->dev,
			"error stopping Axe-Fx III interface: %d\n", err);

	return 0;
}

static void mbox3_setup_48_24_magic(struct usb_device *dev)
{
	 
	 
	 


	 
	u8 com_buff[4] = {0x80, 0xbb, 0x00, 0x00};

	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			0x01, 0x21, 0x0100, 0x0001, &com_buff, 4);  
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			0x01, 0x21, 0x0100, 0x8101, &com_buff, 4);

	 
	 
	 
	com_buff[0] = 0x00;
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
		0x01, 0x21, 0x0003, 0x2001, &com_buff, 1);

	 
	com_buff[0] = 0x01;
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0100, 0x8001, &com_buff, 1);

	 
	com_buff[0] = 0x00;
	com_buff[1] = 0x80;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0110, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0111, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0114, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0115, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0118, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0119, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x011c, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x011d, 0x4001, &com_buff, 2);

	 
	com_buff[0] = 0x00;
	com_buff[1] = 0x00;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0100, 0x4001, &com_buff, 2);
	com_buff[0] = 0x00;
	com_buff[1] = 0x80;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0101, 0x4001, &com_buff, 2);
	com_buff[0] = 0x00;
	com_buff[1] = 0x80;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0104, 0x4001, &com_buff, 2);
	com_buff[0] = 0x00;
	com_buff[1] = 0x00;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0105, 0x4001, &com_buff, 2);

	com_buff[0] = 0x00;
	com_buff[1] = 0x80;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0108, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0109, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x010c, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x010d, 0x4001, &com_buff, 2);

	 
	com_buff[0] = 0x00;
	com_buff[1] = 0x80;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0120, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0121, 0x4001, &com_buff, 2);

	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0100, 0x4201, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0101, 0x4201, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0102, 0x4201, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0103, 0x4201, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0104, 0x4201, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0105, 0x4201, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0106, 0x4201, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0107, 0x4201, &com_buff, 2);

	 
	com_buff[0] = 0x02;
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			3, 0x21, 0x0000, 0x2001, &com_buff, 1);

	 
	com_buff[0] = 0x00;
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			3, 0x21, 0x0002, 0x2001, &com_buff, 1);

	 
	com_buff[0] = 0x00;
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			3, 0x21, 0x0001, 0x2001, &com_buff, 1);

	 
	com_buff[0] = 0x00;
	com_buff[1] = 0x80;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0112, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0113, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0116, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0117, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x011a, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x011b, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x011e, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x011f, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0102, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0103, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0106, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0107, 0x4001, &com_buff, 2);

	com_buff[0] = 0x00;
	com_buff[1] = 0x00;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x010a, 0x4001, &com_buff, 2);

	com_buff[0] = 0x00;
	com_buff[1] = 0x80;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x010b, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x010e, 0x4001, &com_buff, 2);

	com_buff[0] = 0x00;
	com_buff[1] = 0x00;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x010f, 0x4001, &com_buff, 2);

	com_buff[0] = 0x00;
	com_buff[1] = 0x80;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0122, 0x4001, &com_buff, 2);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0123, 0x4001, &com_buff, 2);

	 
	 
	 
	 
	 
	 
	 
	 
	 
	com_buff[0] = 0x00;
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0200, 0x4301, &com_buff, 1);	 
	 


	 
	 
	 
	com_buff[0] = 0x00;
	com_buff[1] = 0x00;
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0400, 0x4301, &com_buff, 2);

	 
	 
	 
	com_buff[0] = 0x00;
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0500, 0x4301, &com_buff, 1);
	 
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			1, 0x21, 0x0300, 0x4301, &com_buff, 1);

	 
	 
	 
	 
	 
	com_buff[0] = 0x05;
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			3, 0x21, 0x0005, 0x2001, &com_buff, 1);

	 
	com_buff[0] = 0x00;
	snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			3, 0x21, 0x0004, 0x2001, &com_buff, 1);
}

#define MBOX3_DESCRIPTOR_SIZE	464

static int snd_usb_mbox3_boot_quirk(struct usb_device *dev)
{
	struct usb_host_config *config = dev->actconfig;
	int err;
	int descriptor_size;

	descriptor_size = le16_to_cpu(get_cfg_desc(config)->wTotalLength);

	if (descriptor_size != MBOX3_DESCRIPTOR_SIZE) {
		dev_err(&dev->dev, "Invalid descriptor size=%d.\n", descriptor_size);
		return -ENODEV;
	}

	dev_dbg(&dev->dev, "device initialised!\n");

	err = usb_get_descriptor(dev, USB_DT_DEVICE, 0,
		&dev->descriptor, sizeof(dev->descriptor));
	config = dev->actconfig;
	if (err < 0)
		dev_dbg(&dev->dev, "error usb_get_descriptor: %d\n", err);

	err = usb_reset_configuration(dev);
	if (err < 0)
		dev_dbg(&dev->dev, "error usb_reset_configuration: %d\n", err);
	dev_dbg(&dev->dev, "mbox3_boot: new boot length = %d\n",
		le16_to_cpu(get_cfg_desc(config)->wTotalLength));

	mbox3_setup_48_24_magic(dev);
	dev_info(&dev->dev, "Digidesign Mbox 3: 24bit 48kHz");

	return 0;  
}

#define MICROBOOK_BUF_SIZE 128

static int snd_usb_motu_microbookii_communicate(struct usb_device *dev, u8 *buf,
						int buf_size, int *length)
{
	int err, actual_length;

	if (usb_pipe_type_check(dev, usb_sndintpipe(dev, 0x01)))
		return -EINVAL;
	err = usb_interrupt_msg(dev, usb_sndintpipe(dev, 0x01), buf, *length,
				&actual_length, 1000);
	if (err < 0)
		return err;

	print_hex_dump(KERN_DEBUG, "MicroBookII snd: ", DUMP_PREFIX_NONE, 16, 1,
		       buf, actual_length, false);

	memset(buf, 0, buf_size);

	if (usb_pipe_type_check(dev, usb_rcvintpipe(dev, 0x82)))
		return -EINVAL;
	err = usb_interrupt_msg(dev, usb_rcvintpipe(dev, 0x82), buf, buf_size,
				&actual_length, 1000);
	if (err < 0)
		return err;

	print_hex_dump(KERN_DEBUG, "MicroBookII rcv: ", DUMP_PREFIX_NONE, 16, 1,
		       buf, actual_length, false);

	*length = actual_length;
	return 0;
}

static int snd_usb_motu_microbookii_boot_quirk(struct usb_device *dev)
{
	int err, actual_length, poll_attempts = 0;
	static const u8 set_samplerate_seq[] = { 0x00, 0x00, 0x00, 0x00,
						 0x00, 0x00, 0x0b, 0x14,
						 0x00, 0x00, 0x00, 0x01 };
	static const u8 poll_ready_seq[] = { 0x00, 0x04, 0x00, 0x00,
					     0x00, 0x00, 0x0b, 0x18 };
	u8 *buf = kzalloc(MICROBOOK_BUF_SIZE, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;

	dev_info(&dev->dev, "Waiting for MOTU Microbook II to boot up...\n");

	 
	memcpy(buf, set_samplerate_seq, sizeof(set_samplerate_seq));
	actual_length = sizeof(set_samplerate_seq);
	err = snd_usb_motu_microbookii_communicate(dev, buf, MICROBOOK_BUF_SIZE,
						   &actual_length);

	if (err < 0) {
		dev_err(&dev->dev,
			"failed setting the sample rate for Motu MicroBook II: %d\n",
			err);
		goto free_buf;
	}

	 
	while (true) {
		if (++poll_attempts > 100) {
			dev_err(&dev->dev,
				"failed booting Motu MicroBook II: timeout\n");
			err = -ENODEV;
			goto free_buf;
		}

		memset(buf, 0, MICROBOOK_BUF_SIZE);
		memcpy(buf, poll_ready_seq, sizeof(poll_ready_seq));

		actual_length = sizeof(poll_ready_seq);
		err = snd_usb_motu_microbookii_communicate(
			dev, buf, MICROBOOK_BUF_SIZE, &actual_length);
		if (err < 0) {
			dev_err(&dev->dev,
				"failed booting Motu MicroBook II: communication error %d\n",
				err);
			goto free_buf;
		}

		 
		if (actual_length == 12 && buf[actual_length - 1] == 1)
			break;

		msleep(100);
	}

	dev_info(&dev->dev, "MOTU MicroBook II ready\n");

free_buf:
	kfree(buf);
	return err;
}

static int snd_usb_motu_m_series_boot_quirk(struct usb_device *dev)
{
	msleep(4000);

	return 0;
}

 
#define MAUDIO_SET		0x01  
#define MAUDIO_SET_COMPATIBLE	0x80  
#define MAUDIO_SET_DTS		0x02  
#define MAUDIO_SET_96K		0x04  
#define MAUDIO_SET_24B		0x08  
#define MAUDIO_SET_DI		0x10  
#define MAUDIO_SET_MASK		0x1f  
#define MAUDIO_SET_24B_48K_DI	 0x19  
#define MAUDIO_SET_24B_48K_NOTDI 0x09  
#define MAUDIO_SET_16B_48K_DI	 0x11  
#define MAUDIO_SET_16B_48K_NOTDI 0x01  

static int quattro_skip_setting_quirk(struct snd_usb_audio *chip,
				      int iface, int altno)
{
	 
	usb_set_interface(chip->dev, iface, 0);
	if (chip->setup & MAUDIO_SET) {
		if (chip->setup & MAUDIO_SET_COMPATIBLE) {
			if (iface != 1 && iface != 2)
				return 1;  
		} else {
			unsigned int mask;
			if (iface == 1 || iface == 2)
				return 1;  
			if ((chip->setup & MAUDIO_SET_96K) && altno != 1)
				return 1;  
			mask = chip->setup & MAUDIO_SET_MASK;
			if (mask == MAUDIO_SET_24B_48K_DI && altno != 2)
				return 1;  
			if (mask == MAUDIO_SET_24B_48K_NOTDI && altno != 3)
				return 1;  
			if (mask == MAUDIO_SET_16B_48K_NOTDI && altno != 4)
				return 1;  
		}
	}
	usb_audio_dbg(chip,
		    "using altsetting %d for interface %d config %d\n",
		    altno, iface, chip->setup);
	return 0;  
}

static int audiophile_skip_setting_quirk(struct snd_usb_audio *chip,
					 int iface,
					 int altno)
{
	 
	usb_set_interface(chip->dev, iface, 0);

	if (chip->setup & MAUDIO_SET) {
		unsigned int mask;
		if ((chip->setup & MAUDIO_SET_DTS) && altno != 6)
			return 1;  
		if ((chip->setup & MAUDIO_SET_96K) && altno != 1)
			return 1;  
		mask = chip->setup & MAUDIO_SET_MASK;
		if (mask == MAUDIO_SET_24B_48K_DI && altno != 2)
			return 1;  
		if (mask == MAUDIO_SET_24B_48K_NOTDI && altno != 3)
			return 1;  
		if (mask == MAUDIO_SET_16B_48K_DI && altno != 4)
			return 1;  
		if (mask == MAUDIO_SET_16B_48K_NOTDI && altno != 5)
			return 1;  
	}

	return 0;  
}

static int fasttrackpro_skip_setting_quirk(struct snd_usb_audio *chip,
					   int iface, int altno)
{
	 
	usb_set_interface(chip->dev, iface, 0);

	 
	if (chip->setup & (MAUDIO_SET | MAUDIO_SET_24B)) {
		if (chip->setup & MAUDIO_SET_96K) {
			if (altno != 3 && altno != 6)
				return 1;
		} else if (chip->setup & MAUDIO_SET_DI) {
			if (iface == 4)
				return 1;  
			if (altno != 2 && altno != 5)
				return 1;  
		} else {
			if (iface == 5)
				return 1;  
			if (altno != 2 && altno != 5)
				return 1;  
		}
	} else {
		 
		if (altno != 1)
			return 1;
	}

	usb_audio_dbg(chip,
		    "using altsetting %d for interface %d config %d\n",
		    altno, iface, chip->setup);
	return 0;  
}

static int s1810c_skip_setting_quirk(struct snd_usb_audio *chip,
					   int iface, int altno)
{
	 

	 
	if ((chip->setup == 0 || chip->setup > 2) && altno != 2)
		return 1;
	else if (chip->setup == 1 && altno != 1)
		return 1;
	else if (chip->setup == 2 && altno != 3)
		return 1;

	return 0;
}

int snd_usb_apply_interface_quirk(struct snd_usb_audio *chip,
				  int iface,
				  int altno)
{
	 
	if (chip->usb_id == USB_ID(0x0763, 0x2003))
		return audiophile_skip_setting_quirk(chip, iface, altno);
	 
	if (chip->usb_id == USB_ID(0x0763, 0x2001))
		return quattro_skip_setting_quirk(chip, iface, altno);
	 
	if (chip->usb_id == USB_ID(0x0763, 0x2012))
		return fasttrackpro_skip_setting_quirk(chip, iface, altno);
	 
	if (chip->usb_id == USB_ID(0x194f, 0x010c))
		return s1810c_skip_setting_quirk(chip, iface, altno);


	return 0;
}

int snd_usb_apply_boot_quirk(struct usb_device *dev,
			     struct usb_interface *intf,
			     const struct snd_usb_audio_quirk *quirk,
			     unsigned int id)
{
	switch (id) {
	case USB_ID(0x041e, 0x3000):
		 
		 
		return snd_usb_extigy_boot_quirk(dev, intf);

	case USB_ID(0x041e, 0x3020):
		 
		return snd_usb_audigy2nx_boot_quirk(dev);

	case USB_ID(0x10f5, 0x0200):
		 
		return snd_usb_cm106_boot_quirk(dev);

	case USB_ID(0x0d8c, 0x0102):
		 
	case USB_ID(0x0ccd, 0x00b1):  
		return snd_usb_cm6206_boot_quirk(dev);

	case USB_ID(0x0dba, 0x3000):
		 
		return snd_usb_mbox2_boot_quirk(dev);
	case USB_ID(0x0dba, 0x5000):
		 
		return snd_usb_mbox3_boot_quirk(dev);


	case USB_ID(0x1235, 0x0010):  
	case USB_ID(0x1235, 0x0018):  
		return snd_usb_novation_boot_quirk(dev);

	case USB_ID(0x133e, 0x0815):
		 
		return snd_usb_accessmusic_boot_quirk(dev);

	case USB_ID(0x17cc, 0x1000):  
	case USB_ID(0x17cc, 0x1010):  
	case USB_ID(0x17cc, 0x1020):  
		return snd_usb_nativeinstruments_boot_quirk(dev);
	case USB_ID(0x0763, 0x2012):   
		return snd_usb_fasttrackpro_boot_quirk(dev);
	case USB_ID(0x047f, 0xc010):  
		return snd_usb_gamecon780_boot_quirk(dev);
	case USB_ID(0x2466, 0x8010):  
		return snd_usb_axefx3_boot_quirk(dev);
	case USB_ID(0x07fd, 0x0004):  
		 
		if (get_iface_desc(intf->altsetting)->bInterfaceClass ==
		    USB_CLASS_VENDOR_SPEC &&
		    get_iface_desc(intf->altsetting)->bInterfaceNumber < 3)
			return snd_usb_motu_microbookii_boot_quirk(dev);
		break;
	}

	return 0;
}

int snd_usb_apply_boot_quirk_once(struct usb_device *dev,
				  struct usb_interface *intf,
				  const struct snd_usb_audio_quirk *quirk,
				  unsigned int id)
{
	switch (id) {
	case USB_ID(0x07fd, 0x0008):  
		return snd_usb_motu_m_series_boot_quirk(dev);
	}

	return 0;
}

 
int snd_usb_is_big_endian_format(struct snd_usb_audio *chip,
				 const struct audioformat *fp)
{
	 
	switch (chip->usb_id) {
	case USB_ID(0x0763, 0x2001):  
		if (fp->altsetting == 2 || fp->altsetting == 3 ||
			fp->altsetting == 5 || fp->altsetting == 6)
			return 1;
		break;
	case USB_ID(0x0763, 0x2003):  
		if (chip->setup == 0x00 ||
			fp->altsetting == 1 || fp->altsetting == 2 ||
			fp->altsetting == 3)
			return 1;
		break;
	case USB_ID(0x0763, 0x2012):  
		if (fp->altsetting == 2 || fp->altsetting == 3 ||
			fp->altsetting == 5 || fp->altsetting == 6)
			return 1;
		break;
	}
	return 0;
}

 

enum {
	EMU_QUIRK_SR_44100HZ = 0,
	EMU_QUIRK_SR_48000HZ,
	EMU_QUIRK_SR_88200HZ,
	EMU_QUIRK_SR_96000HZ,
	EMU_QUIRK_SR_176400HZ,
	EMU_QUIRK_SR_192000HZ
};

static void set_format_emu_quirk(struct snd_usb_substream *subs,
				 const struct audioformat *fmt)
{
	unsigned char emu_samplerate_id = 0;

	 
	if (subs->direction == SNDRV_PCM_STREAM_PLAYBACK) {
		if (subs->stream->substream[SNDRV_PCM_STREAM_CAPTURE].cur_audiofmt)
			return;
	}

	switch (fmt->rate_min) {
	case 48000:
		emu_samplerate_id = EMU_QUIRK_SR_48000HZ;
		break;
	case 88200:
		emu_samplerate_id = EMU_QUIRK_SR_88200HZ;
		break;
	case 96000:
		emu_samplerate_id = EMU_QUIRK_SR_96000HZ;
		break;
	case 176400:
		emu_samplerate_id = EMU_QUIRK_SR_176400HZ;
		break;
	case 192000:
		emu_samplerate_id = EMU_QUIRK_SR_192000HZ;
		break;
	default:
		emu_samplerate_id = EMU_QUIRK_SR_44100HZ;
		break;
	}
	snd_emuusb_set_samplerate(subs->stream->chip, emu_samplerate_id);
	subs->pkt_offset_adj = (emu_samplerate_id >= EMU_QUIRK_SR_176400HZ) ? 4 : 0;
}

static int pioneer_djm_set_format_quirk(struct snd_usb_substream *subs,
					u16 windex)
{
	unsigned int cur_rate = subs->data_endpoint->cur_rate;
	u8 sr[3];
	
	sr[0] = cur_rate & 0xff;
	sr[1] = (cur_rate >> 8) & 0xff;
	sr[2] = (cur_rate >> 16) & 0xff;
	usb_set_interface(subs->dev, 0, 1);
	
	snd_usb_ctl_msg(subs->stream->chip->dev,
		usb_sndctrlpipe(subs->stream->chip->dev, 0),
		0x01, 0x22, 0x0100, windex, &sr, 0x0003);
	return 0;
}

void snd_usb_set_format_quirk(struct snd_usb_substream *subs,
			      const struct audioformat *fmt)
{
	switch (subs->stream->chip->usb_id) {
	case USB_ID(0x041e, 0x3f02):  
	case USB_ID(0x041e, 0x3f04):  
	case USB_ID(0x041e, 0x3f0a):  
	case USB_ID(0x041e, 0x3f19):  
		set_format_emu_quirk(subs, fmt);
		break;
	case USB_ID(0x534d, 0x0021):  
	case USB_ID(0x534d, 0x2109):  
		subs->stream_offset_adj = 2;
		break;
	case USB_ID(0x2b73, 0x0013):  
		pioneer_djm_set_format_quirk(subs, 0x0082);
		break;
	case USB_ID(0x08e4, 0x017f):  
	case USB_ID(0x08e4, 0x0163):  
		pioneer_djm_set_format_quirk(subs, 0x0086);
		break;
	}
}

int snd_usb_select_mode_quirk(struct snd_usb_audio *chip,
			      const struct audioformat *fmt)
{
	struct usb_device *dev = chip->dev;
	int err;

	if (chip->quirk_flags & QUIRK_FLAG_ITF_USB_DSD_DAC) {
		 
		err = usb_set_interface(dev, fmt->iface, 0);
		if (err < 0)
			return err;

		msleep(20);  

		 
		if (fmt->formats & SNDRV_PCM_FMTBIT_DSD_U32_BE) {
			 
			err = snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0), 0,
					      USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,
					      1, 1, NULL, 0);
			if (err < 0)
				return err;

		} else {
			 
			 
			err = snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0), 0,
					      USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,
					      0, 1, NULL, 0);
			if (err < 0)
				return err;

		}
		msleep(20);
	}
	return 0;
}

void snd_usb_endpoint_start_quirk(struct snd_usb_endpoint *ep)
{
	 
	if (USB_ID_VENDOR(ep->chip->usb_id) == 0x23ba &&
	    ep->type == SND_USB_ENDPOINT_TYPE_SYNC)
		ep->skip_packets = 4;

	 
	if ((ep->chip->usb_id == USB_ID(0x0763, 0x2030) ||
	     ep->chip->usb_id == USB_ID(0x0763, 0x2031)) &&
	    ep->type == SND_USB_ENDPOINT_TYPE_DATA)
		ep->skip_packets = 16;

	 
	if ((ep->chip->usb_id == USB_ID(0x0644, 0x8038) ||   
	     ep->chip->usb_id == USB_ID(0x1852, 0x5034)) &&  
	    ep->syncmaxsize == 4)
		ep->tenor_fb_quirk = 1;
}

 
void snd_usb_ctl_msg_quirk(struct usb_device *dev, unsigned int pipe,
			   __u8 request, __u8 requesttype, __u16 value,
			   __u16 index, void *data, __u16 size)
{
	struct snd_usb_audio *chip = dev_get_drvdata(&dev->dev);

	if (!chip || (requesttype & USB_TYPE_MASK) != USB_TYPE_CLASS)
		return;

	if (chip->quirk_flags & QUIRK_FLAG_CTL_MSG_DELAY)
		msleep(20);
	else if (chip->quirk_flags & QUIRK_FLAG_CTL_MSG_DELAY_1M)
		usleep_range(1000, 2000);
	else if (chip->quirk_flags & QUIRK_FLAG_CTL_MSG_DELAY_5M)
		usleep_range(5000, 6000);
}

 
u64 snd_usb_interface_dsd_format_quirks(struct snd_usb_audio *chip,
					struct audioformat *fp,
					unsigned int sample_bytes)
{
	struct usb_interface *iface;

	 
	if (USB_ID_VENDOR(chip->usb_id) == 0x23ba &&
	    USB_ID_PRODUCT(chip->usb_id) < 0x0110) {
		switch (fp->altsetting) {
		case 1:
			fp->dsd_dop = true;
			return SNDRV_PCM_FMTBIT_DSD_U16_LE;
		case 2:
			fp->dsd_bitrev = true;
			return SNDRV_PCM_FMTBIT_DSD_U8;
		case 3:
			fp->dsd_bitrev = true;
			return SNDRV_PCM_FMTBIT_DSD_U16_LE;
		}
	}

	 
	switch (chip->usb_id) {
	case USB_ID(0x139f, 0x5504):  
	case USB_ID(0x20b1, 0x3089):  
	case USB_ID(0x2522, 0x0007):  
	case USB_ID(0x2522, 0x0009):  
	case USB_ID(0x2522, 0x0012):  
	case USB_ID(0x2772, 0x0230):  
		if (fp->altsetting == 2)
			return SNDRV_PCM_FMTBIT_DSD_U32_BE;
		break;

	case USB_ID(0x0d8c, 0x0316):  
	case USB_ID(0x10cb, 0x0103):  
	case USB_ID(0x16d0, 0x06b2):  
	case USB_ID(0x16d0, 0x06b4):  
	case USB_ID(0x16d0, 0x0733):  
	case USB_ID(0x16d0, 0x09d8):  
	case USB_ID(0x16d0, 0x09db):  
	case USB_ID(0x16d0, 0x09dd):  
	case USB_ID(0x1db5, 0x0003):  
	case USB_ID(0x20a0, 0x4143):  
	case USB_ID(0x22e1, 0xca01):  
	case USB_ID(0x249c, 0x9326):  
	case USB_ID(0x2616, 0x0106):  
	case USB_ID(0x2622, 0x0041):  
	case USB_ID(0x278b, 0x5100):  
	case USB_ID(0x27f7, 0x3002):  
	case USB_ID(0x29a2, 0x0086):  
	case USB_ID(0x6b42, 0x0042):  
		if (fp->altsetting == 3)
			return SNDRV_PCM_FMTBIT_DSD_U32_BE;
		break;

	 
	case USB_ID(0x16d0, 0x071a):   
		if (fp->altsetting == 2) {
			switch (le16_to_cpu(chip->dev->descriptor.bcdDevice)) {
			case 0x199:
				return SNDRV_PCM_FMTBIT_DSD_U32_LE;
			case 0x19b:
			case 0x203:
				return SNDRV_PCM_FMTBIT_DSD_U32_BE;
			default:
				break;
			}
		}
		break;
	case USB_ID(0x16d0, 0x0a23):
		if (fp->altsetting == 2)
			return SNDRV_PCM_FMTBIT_DSD_U32_BE;
		break;

	default:
		break;
	}

	 
	if (chip->quirk_flags & QUIRK_FLAG_ITF_USB_DSD_DAC) {
		iface = usb_ifnum_to_if(chip->dev, fp->iface);

		 
		if (fp->altsetting == iface->num_altsetting - 1)
			return SNDRV_PCM_FMTBIT_DSD_U32_BE;
	}

	 
	if ((chip->quirk_flags & QUIRK_FLAG_DSD_RAW) && fp->dsd_raw)
		return SNDRV_PCM_FMTBIT_DSD_U32_BE;

	return 0;
}

void snd_usb_audioformat_attributes_quirk(struct snd_usb_audio *chip,
					  struct audioformat *fp,
					  int stream)
{
	switch (chip->usb_id) {
	case USB_ID(0x0a92, 0x0053):  
		 
		fp->attributes &= ~UAC_EP_CS_ATTR_SAMPLE_RATE;
		break;
	case USB_ID(0x041e, 0x3020):  
	case USB_ID(0x0763, 0x2003):  
		 
		fp->attributes |= UAC_EP_CS_ATTR_SAMPLE_RATE;
		break;
	case USB_ID(0x0763, 0x2001):   
	case USB_ID(0x0763, 0x2012):   
	case USB_ID(0x047f, 0x0ca1):  
	case USB_ID(0x077d, 0x07af):  
	 
		fp->ep_attr &= ~USB_ENDPOINT_SYNCTYPE;
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			fp->ep_attr |= USB_ENDPOINT_SYNC_ADAPTIVE;
		else
			fp->ep_attr |= USB_ENDPOINT_SYNC_SYNC;
		break;
	case USB_ID(0x07fd, 0x0004):   
		 
		fp->attributes &= ~UAC_EP_CS_ATTR_FILL_MAX;
		break;
	case USB_ID(0x1224, 0x2a25):   
		 
		fp->attributes |= UAC_EP_CS_ATTR_FILL_MAX;
		break;
	case USB_ID(0x3511, 0x2b1e):  
		 
		if (stream == SNDRV_PCM_STREAM_CAPTURE)
			fp->attributes &= ~UAC_EP_CS_ATTR_PITCH_CONTROL;
		break;
	}
}

 
struct usb_audio_quirk_flags_table {
	u32 id;
	u32 flags;
};

#define DEVICE_FLG(vid, pid, _flags) \
	{ .id = USB_ID(vid, pid), .flags = (_flags) }
#define VENDOR_FLG(vid, _flags) DEVICE_FLG(vid, 0, _flags)

static const struct usb_audio_quirk_flags_table quirk_flags_table[] = {
	 
	DEVICE_FLG(0x041e, 0x3000,  
		   QUIRK_FLAG_IGNORE_CTL_ERROR),
	DEVICE_FLG(0x041e, 0x4080,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x045e, 0x083c,  
		   QUIRK_FLAG_GET_SAMPLE_RATE | QUIRK_FLAG_CTL_MSG_DELAY |
		   QUIRK_FLAG_DISABLE_AUTOSUSPEND),
	DEVICE_FLG(0x046d, 0x084c,  
		   QUIRK_FLAG_GET_SAMPLE_RATE | QUIRK_FLAG_CTL_MSG_DELAY_1M),
	DEVICE_FLG(0x046d, 0x0991,  
		   QUIRK_FLAG_CTL_MSG_DELAY_1M | QUIRK_FLAG_IGNORE_CTL_ERROR),
	DEVICE_FLG(0x046d, 0x09a4,  
		   QUIRK_FLAG_CTL_MSG_DELAY_1M | QUIRK_FLAG_IGNORE_CTL_ERROR),
	DEVICE_FLG(0x0499, 0x1509,  
		   QUIRK_FLAG_GENERIC_IMPLICIT_FB),
	DEVICE_FLG(0x04d8, 0xfeea,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x04e8, 0xa051,  
		   QUIRK_FLAG_SKIP_CLOCK_SELECTOR | QUIRK_FLAG_CTL_MSG_DELAY_5M),
	DEVICE_FLG(0x054c, 0x0b8c,  
		   QUIRK_FLAG_SET_IFACE_FIRST),
	DEVICE_FLG(0x0556, 0x0014,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x05a3, 0x9420,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x05a7, 0x1020,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x05e1, 0x0408,  
		   QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x05e1, 0x0480,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x0644, 0x8043,  
		   QUIRK_FLAG_ITF_USB_DSD_DAC | QUIRK_FLAG_CTL_MSG_DELAY |
		   QUIRK_FLAG_IFACE_DELAY),
	DEVICE_FLG(0x0644, 0x8044,  
		   QUIRK_FLAG_ITF_USB_DSD_DAC | QUIRK_FLAG_CTL_MSG_DELAY |
		   QUIRK_FLAG_IFACE_DELAY),
	DEVICE_FLG(0x0644, 0x804a,  
		   QUIRK_FLAG_ITF_USB_DSD_DAC | QUIRK_FLAG_CTL_MSG_DELAY |
		   QUIRK_FLAG_IFACE_DELAY),
	DEVICE_FLG(0x0644, 0x805f,  
		   QUIRK_FLAG_FORCE_IFACE_RESET),
	DEVICE_FLG(0x0644, 0x806b,  
		   QUIRK_FLAG_ITF_USB_DSD_DAC | QUIRK_FLAG_CTL_MSG_DELAY |
		   QUIRK_FLAG_IFACE_DELAY),
	DEVICE_FLG(0x06f8, 0xb000,  
		   QUIRK_FLAG_IGNORE_CTL_ERROR),
	DEVICE_FLG(0x06f8, 0xd002,  
		   QUIRK_FLAG_IGNORE_CTL_ERROR),
	DEVICE_FLG(0x0711, 0x5800,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x074d, 0x3553,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x0763, 0x2030,  
		   QUIRK_FLAG_GENERIC_IMPLICIT_FB),
	DEVICE_FLG(0x0763, 0x2031,  
		   QUIRK_FLAG_GENERIC_IMPLICIT_FB),
	DEVICE_FLG(0x08bb, 0x2702,  
		   QUIRK_FLAG_IGNORE_CTL_ERROR),
	DEVICE_FLG(0x0951, 0x16ad,  
		   QUIRK_FLAG_CTL_MSG_DELAY_1M),
	DEVICE_FLG(0x0b0e, 0x0349,  
		   QUIRK_FLAG_CTL_MSG_DELAY_1M),
	DEVICE_FLG(0x0fd9, 0x0008,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x1395, 0x740a,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x1397, 0x0507,  
		   QUIRK_FLAG_PLAYBACK_FIRST | QUIRK_FLAG_GENERIC_IMPLICIT_FB),
	DEVICE_FLG(0x1397, 0x0508,  
		   QUIRK_FLAG_PLAYBACK_FIRST | QUIRK_FLAG_GENERIC_IMPLICIT_FB),
	DEVICE_FLG(0x1397, 0x0509,  
		   QUIRK_FLAG_PLAYBACK_FIRST | QUIRK_FLAG_GENERIC_IMPLICIT_FB),
	DEVICE_FLG(0x13e5, 0x0001,  
		   QUIRK_FLAG_IGNORE_CTL_ERROR),
	DEVICE_FLG(0x154e, 0x1002,  
		   QUIRK_FLAG_ITF_USB_DSD_DAC | QUIRK_FLAG_CTL_MSG_DELAY),
	DEVICE_FLG(0x154e, 0x1003,  
		   QUIRK_FLAG_ITF_USB_DSD_DAC | QUIRK_FLAG_CTL_MSG_DELAY),
	DEVICE_FLG(0x154e, 0x3005,  
		   QUIRK_FLAG_ITF_USB_DSD_DAC | QUIRK_FLAG_CTL_MSG_DELAY),
	DEVICE_FLG(0x154e, 0x3006,  
		   QUIRK_FLAG_ITF_USB_DSD_DAC | QUIRK_FLAG_CTL_MSG_DELAY),
	DEVICE_FLG(0x154e, 0x300b,  
		   QUIRK_FLAG_DSD_RAW),
	DEVICE_FLG(0x154e, 0x500e,  
		   QUIRK_FLAG_IGNORE_CLOCK_SOURCE),
	DEVICE_FLG(0x1686, 0x00dd,  
		   QUIRK_FLAG_TX_LENGTH | QUIRK_FLAG_CTL_MSG_DELAY_1M),
	DEVICE_FLG(0x17aa, 0x1046,  
		   QUIRK_FLAG_DISABLE_AUTOSUSPEND),
	DEVICE_FLG(0x17aa, 0x104d,  
		   QUIRK_FLAG_DISABLE_AUTOSUSPEND),
	DEVICE_FLG(0x1852, 0x5065,  
		   QUIRK_FLAG_ITF_USB_DSD_DAC | QUIRK_FLAG_CTL_MSG_DELAY),
	DEVICE_FLG(0x1901, 0x0191,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x2040, 0x7200,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x7201,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x7210,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x7211,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x7213,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x7217,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x721b,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x721e,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x721f,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x7240,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x7260,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x7270,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x7280,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x7281,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x2040, 0x8200,  
		   QUIRK_FLAG_SHARE_MEDIA_DEVICE | QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x21b4, 0x0081,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x21b4, 0x0230,  
		   QUIRK_FLAG_DSD_RAW),
	DEVICE_FLG(0x21b4, 0x0232,  
		   QUIRK_FLAG_DSD_RAW),
	DEVICE_FLG(0x2522, 0x0007,  
		   QUIRK_FLAG_SET_IFACE_FIRST),
	DEVICE_FLG(0x2708, 0x0002,  
		   QUIRK_FLAG_IGNORE_CTL_ERROR),
	DEVICE_FLG(0x2912, 0x30c8,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x30be, 0x0101,  
		   QUIRK_FLAG_IGNORE_CTL_ERROR),
	DEVICE_FLG(0x413c, 0xa506,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x534d, 0x0021,  
		   QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x534d, 0x2109,  
		   QUIRK_FLAG_ALIGN_TRANSFER),
	DEVICE_FLG(0x1224, 0x2a25,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	DEVICE_FLG(0x2b53, 0x0023,  
		   QUIRK_FLAG_GENERIC_IMPLICIT_FB),
	DEVICE_FLG(0x2b53, 0x0024,  
		   QUIRK_FLAG_GENERIC_IMPLICIT_FB),
	DEVICE_FLG(0x2b53, 0x0031,  
		   QUIRK_FLAG_GENERIC_IMPLICIT_FB),
	DEVICE_FLG(0x0525, 0xa4ad,  
		   QUIRK_FLAG_IFACE_SKIP_CLOSE),
	DEVICE_FLG(0x0ecb, 0x205c,  
		   QUIRK_FLAG_FIXED_RATE),
	DEVICE_FLG(0x0ecb, 0x2069,  
		   QUIRK_FLAG_FIXED_RATE),
	DEVICE_FLG(0x1bcf, 0x2283,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),

	 
	VENDOR_FLG(0x045e,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	VENDOR_FLG(0x046d,  
		   QUIRK_FLAG_CTL_MSG_DELAY_1M),
	VENDOR_FLG(0x047f,  
		   QUIRK_FLAG_GET_SAMPLE_RATE | QUIRK_FLAG_CTL_MSG_DELAY),
	VENDOR_FLG(0x0644,  
		   QUIRK_FLAG_CTL_MSG_DELAY | QUIRK_FLAG_IFACE_DELAY),
	VENDOR_FLG(0x07fd,  
		   QUIRK_FLAG_VALIDATE_RATES),
	VENDOR_FLG(0x1235,  
		   QUIRK_FLAG_VALIDATE_RATES),
	VENDOR_FLG(0x1511,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x152a,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x18d1,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x1de7,  
		   QUIRK_FLAG_GET_SAMPLE_RATE),
	VENDOR_FLG(0x20b1,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x21ed,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x22d9,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x23ba,  
		   QUIRK_FLAG_CTL_MSG_DELAY | QUIRK_FLAG_IFACE_DELAY |
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x25ce,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x278b,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x292b,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x2972,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x2ab6,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x2afd,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x2d87,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x3336,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x3353,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x35f4,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0x3842,  
		   QUIRK_FLAG_DSD_RAW),
	VENDOR_FLG(0xc502,  
		   QUIRK_FLAG_DSD_RAW),

	{}  
};

void snd_usb_init_quirk_flags(struct snd_usb_audio *chip)
{
	const struct usb_audio_quirk_flags_table *p;

	for (p = quirk_flags_table; p->id; p++) {
		if (chip->usb_id == p->id ||
		    (!USB_ID_PRODUCT(p->id) &&
		     USB_ID_VENDOR(chip->usb_id) == USB_ID_VENDOR(p->id))) {
			usb_audio_dbg(chip,
				      "Set quirk_flags 0x%x for device %04x:%04x\n",
				      p->flags, USB_ID_VENDOR(chip->usb_id),
				      USB_ID_PRODUCT(chip->usb_id));
			chip->quirk_flags |= p->flags;
			return;
		}
	}
}
