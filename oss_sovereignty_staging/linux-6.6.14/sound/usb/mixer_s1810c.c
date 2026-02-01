
 

#include <linux/usb.h>
#include <linux/usb/audio-v2.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/control.h>

#include "usbaudio.h"
#include "mixer.h"
#include "mixer_quirks.h"
#include "helper.h"
#include "mixer_s1810c.h"

#define SC1810C_CMD_REQ	160
#define SC1810C_CMD_REQTYPE \
	(USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_OUT)
#define SC1810C_CMD_F1		0x50617269
#define SC1810C_CMD_F2		0x14

 
struct s1810c_ctl_packet {
	u32 a;
	u32 b;
	u32 fixed1;
	u32 fixed2;
	u32 c;
	u32 d;
	u32 e;
};

#define SC1810C_CTL_LINE_SW	0
#define SC1810C_CTL_MUTE_SW	1
#define SC1810C_CTL_AB_SW	3
#define SC1810C_CTL_48V_SW	4

#define SC1810C_SET_STATE_REQ	161
#define SC1810C_SET_STATE_REQTYPE SC1810C_CMD_REQTYPE
#define SC1810C_SET_STATE_F1	0x64656D73
#define SC1810C_SET_STATE_F2	0xF4

#define SC1810C_GET_STATE_REQ	162
#define SC1810C_GET_STATE_REQTYPE \
	(USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN)
#define SC1810C_GET_STATE_F1	SC1810C_SET_STATE_F1
#define SC1810C_GET_STATE_F2	SC1810C_SET_STATE_F2

#define SC1810C_STATE_F1_IDX	2
#define SC1810C_STATE_F2_IDX	3

 
struct s1810c_state_packet {
	u32 fields[63];
};

#define SC1810C_STATE_48V_SW	58
#define SC1810C_STATE_LINE_SW	59
#define SC1810C_STATE_MUTE_SW	60
#define SC1810C_STATE_AB_SW	62

struct s1810_mixer_state {
	uint16_t seqnum;
	struct mutex usb_mutex;
	struct mutex data_mutex;
};

static int
snd_s1810c_send_ctl_packet(struct usb_device *dev, u32 a,
			   u32 b, u32 c, u32 d, u32 e)
{
	struct s1810c_ctl_packet pkt = { 0 };
	int ret = 0;

	pkt.fixed1 = SC1810C_CMD_F1;
	pkt.fixed2 = SC1810C_CMD_F2;

	pkt.a = a;
	pkt.b = b;
	pkt.c = c;
	pkt.d = d;
	 
	pkt.e = (c == 4) ? 0 : e;

	ret = snd_usb_ctl_msg(dev, usb_sndctrlpipe(dev, 0),
			      SC1810C_CMD_REQ,
			      SC1810C_CMD_REQTYPE, 0, 0, &pkt, sizeof(pkt));
	if (ret < 0) {
		dev_warn(&dev->dev, "could not send ctl packet\n");
		return ret;
	}
	return 0;
}

 
static int
snd_sc1810c_get_status_field(struct usb_device *dev,
			     u32 *field, int field_idx, uint16_t *seqnum)
{
	struct s1810c_state_packet pkt_out = { { 0 } };
	struct s1810c_state_packet pkt_in = { { 0 } };
	int ret = 0;

	pkt_out.fields[SC1810C_STATE_F1_IDX] = SC1810C_SET_STATE_F1;
	pkt_out.fields[SC1810C_STATE_F2_IDX] = SC1810C_SET_STATE_F2;
	ret = snd_usb_ctl_msg(dev, usb_rcvctrlpipe(dev, 0),
			      SC1810C_SET_STATE_REQ,
			      SC1810C_SET_STATE_REQTYPE,
			      (*seqnum), 0, &pkt_out, sizeof(pkt_out));
	if (ret < 0) {
		dev_warn(&dev->dev, "could not send state packet (%d)\n", ret);
		return ret;
	}

	ret = snd_usb_ctl_msg(dev, usb_rcvctrlpipe(dev, 0),
			      SC1810C_GET_STATE_REQ,
			      SC1810C_GET_STATE_REQTYPE,
			      (*seqnum), 0, &pkt_in, sizeof(pkt_in));
	if (ret < 0) {
		dev_warn(&dev->dev, "could not get state field %u (%d)\n",
			 field_idx, ret);
		return ret;
	}

	(*field) = pkt_in.fields[field_idx];
	(*seqnum)++;
	return 0;
}

 
static int snd_s1810c_init_mixer_maps(struct snd_usb_audio *chip)
{
	u32 a, b, c, e, n, off;
	struct usb_device *dev = chip->dev;

	 
	a = 0x64;
	e = 0xbc;
	for (n = 0; n < 2; n++) {
		off = n * 18;
		for (b = off; b < 18 + off; b++) {
			 
			for (c = 0; c <= 8; c++) {
				snd_s1810c_send_ctl_packet(dev, a, b, c, 0, e);
				snd_s1810c_send_ctl_packet(dev, a, b, c, 1, e);
			}
			 
			snd_s1810c_send_ctl_packet(dev, a, b, 0, 0, e);
			snd_s1810c_send_ctl_packet(dev, a, b, 0, 1, e);
		}
		 
		e = 0xb53bf0;
	}

	 
	a = 0x65;
	e = 0x01000000;
	for (b = 1; b < 3; b++) {
		snd_s1810c_send_ctl_packet(dev, a, b, 0, 0, e);
		snd_s1810c_send_ctl_packet(dev, a, b, 0, 1, e);
	}
	snd_s1810c_send_ctl_packet(dev, a, 0, 0, 0, e);
	snd_s1810c_send_ctl_packet(dev, a, 0, 0, 1, e);

	 
	a = 0x64;
	e = 0xbc;
	c = 3;
	for (n = 0; n < 2; n++) {
		off = n * 18;
		for (b = off; b < 18 + off; b++) {
			snd_s1810c_send_ctl_packet(dev, a, b, c, 0, e);
			snd_s1810c_send_ctl_packet(dev, a, b, c, 1, e);
		}
		e = 0xb53bf0;
	}

	 
	a = 0x65;
	e = 0x01000000;
	snd_s1810c_send_ctl_packet(dev, a, 3, 0, 0, e);
	snd_s1810c_send_ctl_packet(dev, a, 3, 0, 1, e);

	 
	a = 0x65;
	e = 0x01000000;
	for (b = 0; b < 4; b++) {
		snd_s1810c_send_ctl_packet(dev, a, b, 0, 0, e);
		snd_s1810c_send_ctl_packet(dev, a, b, 0, 1, e);
	}

	 
	a = 0x64;
	e = 0x01000000;
	for (c = 0; c < 4; c++) {
		for (b = 0; b < 36; b++) {
			if ((c == 0 && b == 18) ||	 
			    (c == 1 && b == 20) ||	 
			    (c == 2 && b == 22) ||	 
			    (c == 3 && b == 24)) {	 
				 
				snd_s1810c_send_ctl_packet(dev, a, b, c, 0, e);
				snd_s1810c_send_ctl_packet(dev, a, b, c, 1, 0);
				b++;
				 
				snd_s1810c_send_ctl_packet(dev, a, b, c, 0, 0);
				snd_s1810c_send_ctl_packet(dev, a, b, c, 1, e);
			} else {
				 
				snd_s1810c_send_ctl_packet(dev, a, b, c, 0, 0);
				snd_s1810c_send_ctl_packet(dev, a, b, c, 1, 0);
			}
		}
	}

	 
	a = 0x64;
	e = 0xbc;
	c = 3;
	for (n = 0; n < 2; n++) {
		off = n * 18;
		for (b = off; b < 18 + off; b++) {
			snd_s1810c_send_ctl_packet(dev, a, b, c, 0, e);
			snd_s1810c_send_ctl_packet(dev, a, b, c, 1, e);
		}
		e = 0xb53bf0;
	}

	 
	a = 0x65;
	e = 0x01000000;
	snd_s1810c_send_ctl_packet(dev, a, 3, 0, 0, e);
	snd_s1810c_send_ctl_packet(dev, a, 3, 0, 1, e);

	 
	snd_s1810c_send_ctl_packet(dev, a, 3, 0, 0, e);
	snd_s1810c_send_ctl_packet(dev, a, 3, 0, 1, e);

	return 0;
}

 
static int
snd_s1810c_get_switch_state(struct usb_mixer_interface *mixer,
			    struct snd_kcontrol *kctl, u32 *state)
{
	struct snd_usb_audio *chip = mixer->chip;
	struct s1810_mixer_state *private = mixer->private_data;
	u32 field = 0;
	u32 ctl_idx = (u32) (kctl->private_value & 0xFF);
	int ret = 0;

	mutex_lock(&private->usb_mutex);
	ret = snd_sc1810c_get_status_field(chip->dev, &field,
					   ctl_idx, &private->seqnum);
	if (ret < 0)
		goto unlock;

	*state = field;
 unlock:
	mutex_unlock(&private->usb_mutex);
	return ret ? ret : 0;
}

 
static int
snd_s1810c_set_switch_state(struct usb_mixer_interface *mixer,
			    struct snd_kcontrol *kctl)
{
	struct snd_usb_audio *chip = mixer->chip;
	struct s1810_mixer_state *private = mixer->private_data;
	u32 pval = (u32) kctl->private_value;
	u32 ctl_id = (pval >> 8) & 0xFF;
	u32 ctl_val = (pval >> 16) & 0x1;
	int ret = 0;

	mutex_lock(&private->usb_mutex);
	ret = snd_s1810c_send_ctl_packet(chip->dev, 0, 0, 0, ctl_id, ctl_val);
	mutex_unlock(&private->usb_mutex);
	return ret;
}

 

static int
snd_s1810c_switch_get(struct snd_kcontrol *kctl,
		      struct snd_ctl_elem_value *ctl_elem)
{
	struct usb_mixer_elem_list *list = snd_kcontrol_chip(kctl);
	struct usb_mixer_interface *mixer = list->mixer;
	struct s1810_mixer_state *private = mixer->private_data;
	u32 pval = (u32) kctl->private_value;
	u32 ctl_idx = pval & 0xFF;
	u32 state = 0;
	int ret = 0;

	mutex_lock(&private->data_mutex);
	ret = snd_s1810c_get_switch_state(mixer, kctl, &state);
	if (ret < 0)
		goto unlock;

	switch (ctl_idx) {
	case SC1810C_STATE_LINE_SW:
	case SC1810C_STATE_AB_SW:
		ctl_elem->value.enumerated.item[0] = (int)state;
		break;
	default:
		ctl_elem->value.integer.value[0] = (long)state;
	}

 unlock:
	mutex_unlock(&private->data_mutex);
	return (ret < 0) ? ret : 0;
}

static int
snd_s1810c_switch_set(struct snd_kcontrol *kctl,
		      struct snd_ctl_elem_value *ctl_elem)
{
	struct usb_mixer_elem_list *list = snd_kcontrol_chip(kctl);
	struct usb_mixer_interface *mixer = list->mixer;
	struct s1810_mixer_state *private = mixer->private_data;
	u32 pval = (u32) kctl->private_value;
	u32 ctl_idx = pval & 0xFF;
	u32 curval = 0;
	u32 newval = 0;
	int ret = 0;

	mutex_lock(&private->data_mutex);
	ret = snd_s1810c_get_switch_state(mixer, kctl, &curval);
	if (ret < 0)
		goto unlock;

	switch (ctl_idx) {
	case SC1810C_STATE_LINE_SW:
	case SC1810C_STATE_AB_SW:
		newval = (u32) ctl_elem->value.enumerated.item[0];
		break;
	default:
		newval = (u32) ctl_elem->value.integer.value[0];
	}

	if (curval == newval)
		goto unlock;

	kctl->private_value &= ~(0x1 << 16);
	kctl->private_value |= (unsigned int)(newval & 0x1) << 16;
	ret = snd_s1810c_set_switch_state(mixer, kctl);

 unlock:
	mutex_unlock(&private->data_mutex);
	return (ret < 0) ? 0 : 1;
}

static int
snd_s1810c_switch_init(struct usb_mixer_interface *mixer,
		       const struct snd_kcontrol_new *new_kctl)
{
	struct snd_kcontrol *kctl;
	struct usb_mixer_elem_info *elem;

	elem = kzalloc(sizeof(struct usb_mixer_elem_info), GFP_KERNEL);
	if (!elem)
		return -ENOMEM;

	elem->head.mixer = mixer;
	elem->control = 0;
	elem->head.id = 0;
	elem->channels = 1;

	kctl = snd_ctl_new1(new_kctl, elem);
	if (!kctl) {
		kfree(elem);
		return -ENOMEM;
	}
	kctl->private_free = snd_usb_mixer_elem_free;

	return snd_usb_mixer_add_control(&elem->head, kctl);
}

static int
snd_s1810c_line_sw_info(struct snd_kcontrol *kctl,
			struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[2] = {
		"Preamp On (Mic/Inst)",
		"Preamp Off (Line in)"
	};

	return snd_ctl_enum_info(uinfo, 1, ARRAY_SIZE(texts), texts);
}

static const struct snd_kcontrol_new snd_s1810c_line_sw = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Line 1/2 Source Type",
	.info = snd_s1810c_line_sw_info,
	.get = snd_s1810c_switch_get,
	.put = snd_s1810c_switch_set,
	.private_value = (SC1810C_STATE_LINE_SW | SC1810C_CTL_LINE_SW << 8)
};

static const struct snd_kcontrol_new snd_s1810c_mute_sw = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Mute Main Out Switch",
	.info = snd_ctl_boolean_mono_info,
	.get = snd_s1810c_switch_get,
	.put = snd_s1810c_switch_set,
	.private_value = (SC1810C_STATE_MUTE_SW | SC1810C_CTL_MUTE_SW << 8)
};

static const struct snd_kcontrol_new snd_s1810c_48v_sw = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "48V Phantom Power On Mic Inputs Switch",
	.info = snd_ctl_boolean_mono_info,
	.get = snd_s1810c_switch_get,
	.put = snd_s1810c_switch_set,
	.private_value = (SC1810C_STATE_48V_SW | SC1810C_CTL_48V_SW << 8)
};

static int
snd_s1810c_ab_sw_info(struct snd_kcontrol *kctl,
		      struct snd_ctl_elem_info *uinfo)
{
	static const char *const texts[2] = {
		"1/2",
		"3/4"
	};

	return snd_ctl_enum_info(uinfo, 1, ARRAY_SIZE(texts), texts);
}

static const struct snd_kcontrol_new snd_s1810c_ab_sw = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Headphone 1 Source Route",
	.info = snd_s1810c_ab_sw_info,
	.get = snd_s1810c_switch_get,
	.put = snd_s1810c_switch_set,
	.private_value = (SC1810C_STATE_AB_SW | SC1810C_CTL_AB_SW << 8)
};

static void snd_sc1810_mixer_state_free(struct usb_mixer_interface *mixer)
{
	struct s1810_mixer_state *private = mixer->private_data;
	kfree(private);
	mixer->private_data = NULL;
}

 
int snd_sc1810_init_mixer(struct usb_mixer_interface *mixer)
{
	struct s1810_mixer_state *private = NULL;
	struct snd_usb_audio *chip = mixer->chip;
	struct usb_device *dev = chip->dev;
	int ret = 0;

	 
	if (!list_empty(&chip->mixer_list))
		return 0;

	dev_info(&dev->dev,
		 "Presonus Studio 1810c, device_setup: %u\n", chip->setup);
	if (chip->setup == 1)
		dev_info(&dev->dev, "(8out/18in @ 48kHz)\n");
	else if (chip->setup == 2)
		dev_info(&dev->dev, "(6out/8in @ 192kHz)\n");
	else
		dev_info(&dev->dev, "(8out/14in @ 96kHz)\n");

	ret = snd_s1810c_init_mixer_maps(chip);
	if (ret < 0)
		return ret;

	private = kzalloc(sizeof(struct s1810_mixer_state), GFP_KERNEL);
	if (!private)
		return -ENOMEM;

	mutex_init(&private->usb_mutex);
	mutex_init(&private->data_mutex);

	mixer->private_data = private;
	mixer->private_free = snd_sc1810_mixer_state_free;

	private->seqnum = 1;

	ret = snd_s1810c_switch_init(mixer, &snd_s1810c_line_sw);
	if (ret < 0)
		return ret;

	ret = snd_s1810c_switch_init(mixer, &snd_s1810c_mute_sw);
	if (ret < 0)
		return ret;

	ret = snd_s1810c_switch_init(mixer, &snd_s1810c_48v_sw);
	if (ret < 0)
		return ret;

	ret = snd_s1810c_switch_init(mixer, &snd_s1810c_ab_sw);
	if (ret < 0)
		return ret;
	return ret;
}
