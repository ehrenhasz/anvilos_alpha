
 

#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/audio-v2.h>

#include <sound/core.h>
#include <sound/control.h>

#include "usbaudio.h"
#include "mixer.h"
#include "helper.h"

#include "mixer_us16x08.h"

 
static const char route_msg[] = {
	0x61,
	0x02,
	0x03,  
	0x62,
	0x02,
	0x01,  
	0x41,
	0x01,
	0x61,
	0x02,
	0x01,
	0x62,
	0x02,
	0x01,  
	0x42,
	0x01,
	0x43,
	0x01,
	0x00,
	0x00
};

static const char mix_init_msg1[] = {
	0x71, 0x01, 0x00, 0x00
};

static const char mix_init_msg2[] = {
	0x62, 0x02, 0x00, 0x61, 0x02, 0x04, 0xb1, 0x01, 0x00, 0x00
};

static const char mix_msg_in[] = {
	 
	0x61, 0x02, 0x04, 0x62, 0x02, 0x01,
	0x81,  
	0x02,  
	0x00,  
	0x00,
	0x00
};

static const char mix_msg_out[] = {
	 
	0x61, 0x02, 0x02, 0x62, 0x02, 0x01,
	0x81,  
	0x02,  
	0x00,  
	0x00,
	0x00
};

static const char bypass_msg_out[] = {
	0x45,
	0x02,
	0x01,  
	0x00,
	0x00
};

static const char bus_msg_out[] = {
	0x44,
	0x02,
	0x01,  
	0x00,
	0x00
};

static const char comp_msg[] = {
	 
	0x61, 0x02, 0x04, 0x62, 0x02, 0x01,
	0x91,
	0x02,
	0xf0,  
	0x92,
	0x02,
	0x0a,  
	0x93,
	0x02,
	0x02,  
	0x94,
	0x02,
	0x01,  
	0x95,
	0x02,
	0x03,  
	0x96,
	0x02,
	0x01,
	0x97,
	0x02,
	0x01,  
	0x00,
	0x00
};

static const char eqs_msq[] = {
	 
	0x61, 0x02, 0x04, 0x62, 0x02, 0x01,
	0x51,  
	0x02,
	0x04,  
	0x52,
	0x02,
	0x0c,  
	0x53,
	0x02,
	0x0f,  
	0x54,
	0x02,
	0x02,  
	0x55,
	0x02,
	0x01,  
	0x00,
	0x00
};

 
static const char ratio_map[] = {
	0x0a, 0x0b, 0x0d, 0x0f, 0x11, 0x14, 0x19, 0x1e,
	0x23, 0x28, 0x32, 0x3c, 0x50, 0xa0, 0xff
};

 
static const char *const route_names[] = {
	"Master Left", "Master Right", "Output 1", "Output 2", "Output 3",
	"Output 4", "Output 5", "Output 6", "Output 7", "Output 8",
};

static int snd_us16x08_recv_urb(struct snd_usb_audio *chip,
	unsigned char *buf, int size)
{

	mutex_lock(&chip->mutex);
	snd_usb_ctl_msg(chip->dev,
		usb_rcvctrlpipe(chip->dev, 0),
		SND_US16X08_URB_METER_REQUEST,
		SND_US16X08_URB_METER_REQUESTTYPE, 0, 0, buf, size);
	mutex_unlock(&chip->mutex);
	return 0;
}

 
static int snd_us16x08_send_urb(struct snd_usb_audio *chip, char *buf, int size)
{
	return snd_usb_ctl_msg(chip->dev, usb_sndctrlpipe(chip->dev, 0),
			SND_US16X08_URB_REQUEST, SND_US16X08_URB_REQUESTTYPE,
			0, 0, buf, size);
}

static int snd_us16x08_route_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	return snd_ctl_enum_info(uinfo, 1, 10, route_names);
}

static int snd_us16x08_route_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	int index = ucontrol->id.index;

	 
	ucontrol->value.enumerated.item[0] = elem->cache_val[index];

	return 0;
}

static int snd_us16x08_route_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_usb_audio *chip = elem->head.mixer->chip;
	int index = ucontrol->id.index;
	char buf[sizeof(route_msg)];
	int val, val_org, err;

	 
	val = ucontrol->value.enumerated.item[0];

	 
	if (val < 0 || val > 9)
		return -EINVAL;

	 
	memcpy(buf, route_msg, sizeof(route_msg));

	if (val < 2) {
		 
		val_org = val;
		buf[2] = 0x02;
	} else {
		 
		buf[2] = 0x03;
		val_org = val - 2;
	}

	 
	buf[5] = (unsigned char) (val_org & 0x0f) + 1;
	 
	buf[13] = index + 1;

	err = snd_us16x08_send_urb(chip, buf, sizeof(route_msg));

	if (err > 0) {
		elem->cached |= 1 << index;
		elem->cache_val[index] = val;
	} else {
		usb_audio_dbg(chip, "Failed to set routing, err:%d\n", err);
	}

	return err > 0 ? 1 : 0;
}

static int snd_us16x08_master_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->count = 1;
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.max = SND_US16X08_KCMAX(kcontrol);
	uinfo->value.integer.min = SND_US16X08_KCMIN(kcontrol);
	uinfo->value.integer.step = SND_US16X08_KCSTEP(kcontrol);
	return 0;
}

static int snd_us16x08_master_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	int index = ucontrol->id.index;

	ucontrol->value.integer.value[0] = elem->cache_val[index];

	return 0;
}

static int snd_us16x08_master_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_usb_audio *chip = elem->head.mixer->chip;
	char buf[sizeof(mix_msg_out)];
	int val, err;
	int index = ucontrol->id.index;

	 
	val = ucontrol->value.integer.value[0];

	 
	if (val < SND_US16X08_KCMIN(kcontrol)
		|| val > SND_US16X08_KCMAX(kcontrol))
		return -EINVAL;

	 
	memcpy(buf, mix_msg_out, sizeof(mix_msg_out));

	buf[8] = val - SND_US16X08_KCBIAS(kcontrol);
	buf[6] = elem->head.id;

	 
	buf[5] = index + 1;
	err = snd_us16x08_send_urb(chip, buf, sizeof(mix_msg_out));

	if (err > 0) {
		elem->cached |= 1 << index;
		elem->cache_val[index] = val;
	} else {
		usb_audio_dbg(chip, "Failed to set master, err:%d\n", err);
	}

	return err > 0 ? 1 : 0;
}

static int snd_us16x08_bus_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_usb_audio *chip = elem->head.mixer->chip;
	char buf[sizeof(mix_msg_out)];
	int val, err = 0;

	val = ucontrol->value.integer.value[0];

	 
	switch (elem->head.id) {
	case SND_US16X08_ID_BYPASS:
		memcpy(buf, bypass_msg_out, sizeof(bypass_msg_out));
		buf[2] = val;
		err = snd_us16x08_send_urb(chip, buf, sizeof(bypass_msg_out));
		break;
	case SND_US16X08_ID_BUSS_OUT:
		memcpy(buf, bus_msg_out, sizeof(bus_msg_out));
		buf[2] = val;
		err = snd_us16x08_send_urb(chip, buf, sizeof(bus_msg_out));
		break;
	case SND_US16X08_ID_MUTE:
		memcpy(buf, mix_msg_out, sizeof(mix_msg_out));
		buf[8] = val;
		buf[6] = elem->head.id;
		buf[5] = 1;
		err = snd_us16x08_send_urb(chip, buf, sizeof(mix_msg_out));
		break;
	}

	if (err > 0) {
		elem->cached |= 1;
		elem->cache_val[0] = val;
	} else {
		usb_audio_dbg(chip, "Failed to set bus parameter, err:%d\n", err);
	}

	return err > 0 ? 1 : 0;
}

static int snd_us16x08_bus_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;

	switch (elem->head.id) {
	case SND_US16X08_ID_BUSS_OUT:
		ucontrol->value.integer.value[0] = elem->cache_val[0];
		break;
	case SND_US16X08_ID_BYPASS:
		ucontrol->value.integer.value[0] = elem->cache_val[0];
		break;
	case SND_US16X08_ID_MUTE:
		ucontrol->value.integer.value[0] = elem->cache_val[0];
		break;
	}

	return 0;
}

 
static int snd_us16x08_channel_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	int index = ucontrol->id.index;

	ucontrol->value.integer.value[0] = elem->cache_val[index];

	return 0;
}

static int snd_us16x08_channel_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_usb_audio *chip = elem->head.mixer->chip;
	char buf[sizeof(mix_msg_in)];
	int val, err;
	int index = ucontrol->id.index;

	val = ucontrol->value.integer.value[0];

	 
	if (val < SND_US16X08_KCMIN(kcontrol)
		|| val > SND_US16X08_KCMAX(kcontrol))
		return -EINVAL;

	 
	memcpy(buf, mix_msg_in, sizeof(mix_msg_in));

	 
	buf[8] = val - SND_US16X08_KCBIAS(kcontrol);
	buf[6] = elem->head.id;
	buf[5] = index + 1;

	err = snd_us16x08_send_urb(chip, buf, sizeof(mix_msg_in));

	if (err > 0) {
		elem->cached |= 1 << index;
		elem->cache_val[index] = val;
	} else {
		usb_audio_dbg(chip, "Failed to set channel, err:%d\n", err);
	}

	return err > 0 ? 1 : 0;
}

static int snd_us16x08_mix_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->count = 1;
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.max = SND_US16X08_KCMAX(kcontrol);
	uinfo->value.integer.min = SND_US16X08_KCMIN(kcontrol);
	uinfo->value.integer.step = SND_US16X08_KCSTEP(kcontrol);
	return 0;
}

static int snd_us16x08_comp_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_us16x08_comp_store *store = elem->private_data;
	int index = ucontrol->id.index;
	int val_idx = COMP_STORE_IDX(elem->head.id);

	ucontrol->value.integer.value[0] = store->val[val_idx][index];

	return 0;
}

static int snd_us16x08_comp_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_usb_audio *chip = elem->head.mixer->chip;
	struct snd_us16x08_comp_store *store = elem->private_data;
	int index = ucontrol->id.index;
	char buf[sizeof(comp_msg)];
	int val_idx, val;
	int err;

	val = ucontrol->value.integer.value[0];

	 
	if (val < SND_US16X08_KCMIN(kcontrol)
		|| val > SND_US16X08_KCMAX(kcontrol))
		return -EINVAL;

	 
	val_idx = elem->head.id - SND_US16X08_ID_COMP_BASE;

	store->val[val_idx][index] = ucontrol->value.integer.value[0];

	 
	memcpy(buf, comp_msg, sizeof(comp_msg));

	 
	buf[8] = store->val[
		COMP_STORE_IDX(SND_US16X08_ID_COMP_THRESHOLD)][index]
		- SND_US16X08_COMP_THRESHOLD_BIAS;
	buf[11] = ratio_map[store->val[
		COMP_STORE_IDX(SND_US16X08_ID_COMP_RATIO)][index]];
	buf[14] = store->val[COMP_STORE_IDX(SND_US16X08_ID_COMP_ATTACK)][index]
		+ SND_US16X08_COMP_ATTACK_BIAS;
	buf[17] = store->val[COMP_STORE_IDX(SND_US16X08_ID_COMP_RELEASE)][index]
		+ SND_US16X08_COMP_RELEASE_BIAS;
	buf[20] = store->val[COMP_STORE_IDX(SND_US16X08_ID_COMP_GAIN)][index];
	buf[26] = store->val[COMP_STORE_IDX(SND_US16X08_ID_COMP_SWITCH)][index];

	 
	buf[5] = index + 1;

	err = snd_us16x08_send_urb(chip, buf, sizeof(comp_msg));

	if (err > 0) {
		elem->cached |= 1 << index;
		elem->cache_val[index] = val;
	} else {
		usb_audio_dbg(chip, "Failed to set compressor, err:%d\n", err);
	}

	return 1;
}

static int snd_us16x08_eqswitch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int val;
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_us16x08_eq_store *store = elem->private_data;
	int index = ucontrol->id.index;

	 
	val = store->val[EQ_STORE_BAND_IDX(elem->head.id)]
		[EQ_STORE_PARAM_IDX(elem->head.id)][index];
	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int snd_us16x08_eqswitch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_usb_audio *chip = elem->head.mixer->chip;
	struct snd_us16x08_eq_store *store = elem->private_data;
	int index = ucontrol->id.index;
	char buf[sizeof(eqs_msq)];
	int val, err = 0;
	int b_idx;

	 
	val = ucontrol->value.integer.value[0] + SND_US16X08_KCBIAS(kcontrol);

	 
	memcpy(buf, eqs_msq, sizeof(eqs_msq));

	 
	buf[5] = index + 1;
	for (b_idx = 0; b_idx < SND_US16X08_ID_EQ_BAND_COUNT; b_idx++) {
		 
		buf[20] = val;
		buf[17] = store->val[b_idx][2][index];
		buf[14] = store->val[b_idx][1][index];
		buf[11] = store->val[b_idx][0][index];
		buf[8] = b_idx + 1;
		err = snd_us16x08_send_urb(chip, buf, sizeof(eqs_msq));
		if (err < 0)
			break;
		store->val[b_idx][3][index] = val;
		msleep(15);
	}

	if (err > 0) {
		elem->cached |= 1 << index;
		elem->cache_val[index] = val;
	} else {
		usb_audio_dbg(chip, "Failed to set eq switch, err:%d\n", err);
	}

	return 1;
}

static int snd_us16x08_eq_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int val;
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_us16x08_eq_store *store = elem->private_data;
	int index = ucontrol->id.index;
	int b_idx = EQ_STORE_BAND_IDX(elem->head.id) - 1;
	int p_idx = EQ_STORE_PARAM_IDX(elem->head.id);

	val = store->val[b_idx][p_idx][index];

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int snd_us16x08_eq_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_usb_audio *chip = elem->head.mixer->chip;
	struct snd_us16x08_eq_store *store = elem->private_data;
	int index = ucontrol->id.index;
	char buf[sizeof(eqs_msq)];
	int val, err;
	int b_idx = EQ_STORE_BAND_IDX(elem->head.id) - 1;
	int p_idx = EQ_STORE_PARAM_IDX(elem->head.id);

	val = ucontrol->value.integer.value[0];

	 
	if (val < SND_US16X08_KCMIN(kcontrol)
		|| val > SND_US16X08_KCMAX(kcontrol))
		return -EINVAL;

	 
	memcpy(buf, eqs_msq, sizeof(eqs_msq));

	store->val[b_idx][p_idx][index] = val;
	buf[20] = store->val[b_idx][3][index];
	buf[17] = store->val[b_idx][2][index];
	buf[14] = store->val[b_idx][1][index];
	buf[11] = store->val[b_idx][0][index];

	 
	buf[5] = index + 1;

	 
	buf[8] = b_idx + 1;

	err = snd_us16x08_send_urb(chip, buf, sizeof(eqs_msq));

	if (err > 0) {
		 
		elem->cached |= 1 << index;
		elem->cache_val[index] = val;
	} else {
		usb_audio_dbg(chip, "Failed to set eq param, err:%d\n", err);
	}

	return 1;
}

static int snd_us16x08_meter_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->count = 34;
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.max = 0x7FFF;
	uinfo->value.integer.min = 0;

	return 0;
}

 
static int snd_get_meter_comp_index(struct snd_us16x08_meter_store *store)
{
	int ret;

	 
	if (store->comp_active_index) {
		 
		if (store->comp_active_index & 0x20) {
			 
			if (store->comp_index -
				store->comp_active_index > 1)
				store->comp_index =
				store->comp_active_index;

			ret = store->comp_index++ & 0x1F;
		} else {
			 
			ret = store->comp_active_index;
		}
	} else {
		 
		while (store->comp_index <= SND_US16X08_MAX_CHANNELS
			&& !store->comp_store->val[
			COMP_STORE_IDX(SND_US16X08_ID_COMP_SWITCH)]
			[store->comp_index - 1]) {
			store->comp_index++;
		}
		ret = store->comp_index++;
		if (store->comp_index > SND_US16X08_MAX_CHANNELS)
			store->comp_index = 1;
	}
	return ret;
}

 
static void get_meter_levels_from_urb(int s,
	struct snd_us16x08_meter_store *store,
	u8 *meter_urb)
{
	int val = MUC2(meter_urb, s) + (MUC3(meter_urb, s) << 8);

	if (MUA0(meter_urb, s) == 0x61 && MUA1(meter_urb, s) == 0x02 &&
		MUA2(meter_urb, s) == 0x04 && MUB0(meter_urb, s) == 0x62) {
		if (MUC0(meter_urb, s) == 0x72)
			store->meter_level[MUB2(meter_urb, s) - 1] = val;
		if (MUC0(meter_urb, s) == 0xb2)
			store->comp_level[MUB2(meter_urb, s) - 1] = val;
	}
	if (MUA0(meter_urb, s) == 0x61 && MUA1(meter_urb, s) == 0x02 &&
		MUA2(meter_urb, s) == 0x02 && MUB0(meter_urb, s) == 0x62)
		store->master_level[MUB2(meter_urb, s) - 1] = val;
}

 
static int snd_us16x08_meter_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int i, set;
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_usb_audio *chip = elem->head.mixer->chip;
	struct snd_us16x08_meter_store *store = elem->private_data;
	u8 meter_urb[64];

	switch (kcontrol->private_value) {
	case 0: {
		char tmp[sizeof(mix_init_msg1)];

		memcpy(tmp, mix_init_msg1, sizeof(mix_init_msg1));
		snd_us16x08_send_urb(chip, tmp, 4);
		snd_us16x08_recv_urb(chip, meter_urb,
			sizeof(meter_urb));
		kcontrol->private_value++;
		break;
	}
	case 1:
		snd_us16x08_recv_urb(chip, meter_urb,
			sizeof(meter_urb));
		kcontrol->private_value++;
		break;
	case 2:
		snd_us16x08_recv_urb(chip, meter_urb,
			sizeof(meter_urb));
		kcontrol->private_value++;
		break;
	case 3: {
		char tmp[sizeof(mix_init_msg2)];

		memcpy(tmp, mix_init_msg2, sizeof(mix_init_msg2));
		tmp[2] = snd_get_meter_comp_index(store);
		snd_us16x08_send_urb(chip, tmp, 10);
		snd_us16x08_recv_urb(chip, meter_urb,
			sizeof(meter_urb));
		kcontrol->private_value = 0;
		break;
	}
	}

	for (set = 0; set < 6; set++)
		get_meter_levels_from_urb(set, store, meter_urb);

	for (i = 0; i < SND_US16X08_MAX_CHANNELS; i++) {
		ucontrol->value.integer.value[i] =
			store ? store->meter_level[i] : 0;
	}

	ucontrol->value.integer.value[i++] = store ? store->master_level[0] : 0;
	ucontrol->value.integer.value[i++] = store ? store->master_level[1] : 0;

	for (i = 2; i < SND_US16X08_MAX_CHANNELS + 2; i++)
		ucontrol->value.integer.value[i + SND_US16X08_MAX_CHANNELS] =
		store ? store->comp_level[i - 2] : 0;

	return 1;
}

static int snd_us16x08_meter_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *elem = kcontrol->private_data;
	struct snd_us16x08_meter_store *store = elem->private_data;
	int val;

	val = ucontrol->value.integer.value[0];

	 
	if (val < 0 || val >= SND_US16X08_MAX_CHANNELS)
		return -EINVAL;

	store->comp_active_index = val;
	store->comp_index = val;

	return 1;
}

static const struct snd_kcontrol_new snd_us16x08_ch_boolean_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_switch_info,
	.get = snd_us16x08_channel_get,
	.put = snd_us16x08_channel_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0, 1)
};

static const struct snd_kcontrol_new snd_us16x08_ch_int_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_channel_get,
	.put = snd_us16x08_channel_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_FADER_BIAS, 1, 0, 133)
};

static const struct snd_kcontrol_new snd_us16x08_pan_int_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_channel_get,
	.put = snd_us16x08_channel_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_FADER_BIAS, 1, 0, 255)
};

static const struct snd_kcontrol_new snd_us16x08_master_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 1,
	.info = snd_us16x08_master_info,
	.get = snd_us16x08_master_get,
	.put = snd_us16x08_master_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_FADER_BIAS, 1, 0, 133)
};

static const struct snd_kcontrol_new snd_us16x08_route_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 8,
	.info = snd_us16x08_route_info,
	.get = snd_us16x08_route_get,
	.put = snd_us16x08_route_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0, 9)
};

static const struct snd_kcontrol_new snd_us16x08_bus_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 1,
	.info = snd_us16x08_switch_info,
	.get = snd_us16x08_bus_get,
	.put = snd_us16x08_bus_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0, 1)
};

static const struct snd_kcontrol_new snd_us16x08_compswitch_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_switch_info,
	.get = snd_us16x08_comp_get,
	.put = snd_us16x08_comp_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0, 1)
};

static const struct snd_kcontrol_new snd_us16x08_comp_threshold_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_comp_get,
	.put = snd_us16x08_comp_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_COMP_THRESHOLD_BIAS, 1,
	0, 0x20)
};

static const struct snd_kcontrol_new snd_us16x08_comp_ratio_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_comp_get,
	.put = snd_us16x08_comp_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0,
	sizeof(ratio_map) - 1),  
};

static const struct snd_kcontrol_new snd_us16x08_comp_gain_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_comp_get,
	.put = snd_us16x08_comp_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0, 0x14)
};

static const struct snd_kcontrol_new snd_us16x08_comp_attack_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_comp_get,
	.put = snd_us16x08_comp_put,
	.private_value =
	SND_US16X08_KCSET(SND_US16X08_COMP_ATTACK_BIAS, 1, 0, 0xc6),
};

static const struct snd_kcontrol_new snd_us16x08_comp_release_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_comp_get,
	.put = snd_us16x08_comp_put,
	.private_value =
	SND_US16X08_KCSET(SND_US16X08_COMP_RELEASE_BIAS, 1, 0, 0x63),
};

static const struct snd_kcontrol_new snd_us16x08_eq_gain_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_eq_get,
	.put = snd_us16x08_eq_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0, 24),
};

static const struct snd_kcontrol_new snd_us16x08_eq_low_freq_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_eq_get,
	.put = snd_us16x08_eq_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0, 0x1F),
};

static const struct snd_kcontrol_new snd_us16x08_eq_mid_freq_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_eq_get,
	.put = snd_us16x08_eq_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0, 0x3F)
};

static const struct snd_kcontrol_new snd_us16x08_eq_mid_width_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_eq_get,
	.put = snd_us16x08_eq_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0, 0x06)
};

static const struct snd_kcontrol_new snd_us16x08_eq_high_freq_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_mix_info,
	.get = snd_us16x08_eq_get,
	.put = snd_us16x08_eq_put,
	.private_value =
	SND_US16X08_KCSET(SND_US16X08_EQ_HIGHFREQ_BIAS, 1, 0, 0x1F)
};

static const struct snd_kcontrol_new snd_us16x08_eq_switch_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 16,
	.info = snd_us16x08_switch_info,
	.get = snd_us16x08_eqswitch_get,
	.put = snd_us16x08_eqswitch_put,
	.private_value = SND_US16X08_KCSET(SND_US16X08_NO_BIAS, 1, 0, 1)
};

static const struct snd_kcontrol_new snd_us16x08_meter_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.count = 1,
	.info = snd_us16x08_meter_info,
	.get = snd_us16x08_meter_get,
	.put = snd_us16x08_meter_put
};

 

 
static struct snd_us16x08_comp_store *snd_us16x08_create_comp_store(void)
{
	int i;
	struct snd_us16x08_comp_store *tmp;

	tmp = kmalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return NULL;

	for (i = 0; i < SND_US16X08_MAX_CHANNELS; i++) {
		tmp->val[COMP_STORE_IDX(SND_US16X08_ID_COMP_THRESHOLD)][i]
			= 0x20;
		tmp->val[COMP_STORE_IDX(SND_US16X08_ID_COMP_RATIO)][i] = 0x00;
		tmp->val[COMP_STORE_IDX(SND_US16X08_ID_COMP_GAIN)][i] = 0x00;
		tmp->val[COMP_STORE_IDX(SND_US16X08_ID_COMP_SWITCH)][i] = 0x00;
		tmp->val[COMP_STORE_IDX(SND_US16X08_ID_COMP_ATTACK)][i] = 0x00;
		tmp->val[COMP_STORE_IDX(SND_US16X08_ID_COMP_RELEASE)][i] = 0x00;
	}
	return tmp;
}

 
static struct snd_us16x08_eq_store *snd_us16x08_create_eq_store(void)
{
	int i, b_idx;
	struct snd_us16x08_eq_store *tmp;

	tmp = kmalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return NULL;

	for (i = 0; i < SND_US16X08_MAX_CHANNELS; i++) {
		for (b_idx = 0; b_idx < SND_US16X08_ID_EQ_BAND_COUNT; b_idx++) {
			tmp->val[b_idx][0][i] = 0x0c;
			tmp->val[b_idx][3][i] = 0x00;
			switch (b_idx) {
			case 0:  
				tmp->val[b_idx][1][i] = 0x05;
				tmp->val[b_idx][2][i] = 0xff;
				break;
			case 1:  
				tmp->val[b_idx][1][i] = 0x0e;
				tmp->val[b_idx][2][i] = 0x02;
				break;
			case 2:  
				tmp->val[b_idx][1][i] = 0x1b;
				tmp->val[b_idx][2][i] = 0x02;
				break;
			case 3:  
				tmp->val[b_idx][1][i] = 0x2f
					- SND_US16X08_EQ_HIGHFREQ_BIAS;
				tmp->val[b_idx][2][i] = 0xff;
				break;
			}
		}
	}
	return tmp;
}

static struct snd_us16x08_meter_store *snd_us16x08_create_meter_store(void)
{
	struct snd_us16x08_meter_store *tmp;

	tmp = kzalloc(sizeof(*tmp), GFP_KERNEL);
	if (!tmp)
		return NULL;
	tmp->comp_index = 1;
	tmp->comp_active_index = 0;
	return tmp;
}

 
static void elem_private_free(struct snd_kcontrol *kctl)
{
	struct usb_mixer_elem_info *elem = kctl->private_data;

	if (elem)
		kfree(elem->private_data);
	kfree(elem);
	kctl->private_data = NULL;
}

static int add_new_ctl(struct usb_mixer_interface *mixer,
	const struct snd_kcontrol_new *ncontrol,
	int index, int val_type, int channels,
	const char *name, void *opt,
	bool do_private_free,
	struct usb_mixer_elem_info **elem_ret)
{
	struct snd_kcontrol *kctl;
	struct usb_mixer_elem_info *elem;
	int err;

	usb_audio_dbg(mixer->chip, "us16x08 add mixer %s\n", name);

	elem = kzalloc(sizeof(*elem), GFP_KERNEL);
	if (!elem)
		return -ENOMEM;

	elem->head.mixer = mixer;
	elem->head.resume = NULL;
	elem->control = 0;
	elem->idx_off = 0;
	elem->head.id = index;
	elem->val_type = val_type;
	elem->channels = channels;
	elem->private_data = opt;

	kctl = snd_ctl_new1(ncontrol, elem);
	if (!kctl) {
		kfree(elem);
		return -ENOMEM;
	}

	if (do_private_free)
		kctl->private_free = elem_private_free;
	else
		kctl->private_free = snd_usb_mixer_elem_free;

	strscpy(kctl->id.name, name, sizeof(kctl->id.name));

	err = snd_usb_mixer_add_control(&elem->head, kctl);
	if (err < 0)
		return err;

	if (elem_ret)
		*elem_ret = elem;

	return 0;
}

 
static const struct snd_us16x08_control_params eq_controls[] = {
	{  
		.kcontrol_new = &snd_us16x08_eq_switch_ctl,
		.control_id = SND_US16X08_ID_EQENABLE,
		.type = USB_MIXER_BOOLEAN,
		.num_channels = 16,
		.name = "EQ Switch",
	},
	{  
		.kcontrol_new = &snd_us16x08_eq_gain_ctl,
		.control_id = SND_US16X08_ID_EQLOWLEVEL,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "EQ Low Volume",
	},
	{  
		.kcontrol_new = &snd_us16x08_eq_low_freq_ctl,
		.control_id = SND_US16X08_ID_EQLOWFREQ,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "EQ Low Frequency",
	},
	{  
		.kcontrol_new = &snd_us16x08_eq_gain_ctl,
		.control_id = SND_US16X08_ID_EQLOWMIDLEVEL,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "EQ MidLow Volume",
	},
	{  
		.kcontrol_new = &snd_us16x08_eq_mid_freq_ctl,
		.control_id = SND_US16X08_ID_EQLOWMIDFREQ,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "EQ MidLow Frequency",
	},
	{  
		.kcontrol_new = &snd_us16x08_eq_mid_width_ctl,
		.control_id = SND_US16X08_ID_EQLOWMIDWIDTH,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "EQ MidLow Q",
	},
	{  
		.kcontrol_new = &snd_us16x08_eq_gain_ctl,
		.control_id = SND_US16X08_ID_EQHIGHMIDLEVEL,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "EQ MidHigh Volume",
	},
	{  
		.kcontrol_new = &snd_us16x08_eq_mid_freq_ctl,
		.control_id = SND_US16X08_ID_EQHIGHMIDFREQ,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "EQ MidHigh Frequency",
	},
	{  
		.kcontrol_new = &snd_us16x08_eq_mid_width_ctl,
		.control_id = SND_US16X08_ID_EQHIGHMIDWIDTH,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "EQ MidHigh Q",
	},
	{  
		.kcontrol_new = &snd_us16x08_eq_gain_ctl,
		.control_id = SND_US16X08_ID_EQHIGHLEVEL,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "EQ High Volume",
	},
	{  
		.kcontrol_new = &snd_us16x08_eq_high_freq_ctl,
		.control_id = SND_US16X08_ID_EQHIGHFREQ,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "EQ High Frequency",
	},
};

 
static const struct snd_us16x08_control_params comp_controls[] = {
	{  
		.kcontrol_new = &snd_us16x08_compswitch_ctl,
		.control_id = SND_US16X08_ID_COMP_SWITCH,
		.type = USB_MIXER_BOOLEAN,
		.num_channels = 16,
		.name = "Compressor Switch",
	},
	{  
		.kcontrol_new = &snd_us16x08_comp_threshold_ctl,
		.control_id = SND_US16X08_ID_COMP_THRESHOLD,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "Compressor Threshold Volume",
	},
	{  
		.kcontrol_new = &snd_us16x08_comp_ratio_ctl,
		.control_id = SND_US16X08_ID_COMP_RATIO,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "Compressor Ratio",
	},
	{  
		.kcontrol_new = &snd_us16x08_comp_attack_ctl,
		.control_id = SND_US16X08_ID_COMP_ATTACK,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "Compressor Attack",
	},
	{  
		.kcontrol_new = &snd_us16x08_comp_release_ctl,
		.control_id = SND_US16X08_ID_COMP_RELEASE,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "Compressor Release",
	},
	{  
		.kcontrol_new = &snd_us16x08_comp_gain_ctl,
		.control_id = SND_US16X08_ID_COMP_GAIN,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "Compressor Volume",
	},
};

 
static const struct snd_us16x08_control_params channel_controls[] = {
	{  
		.kcontrol_new = &snd_us16x08_ch_boolean_ctl,
		.control_id = SND_US16X08_ID_PHASE,
		.type = USB_MIXER_BOOLEAN,
		.num_channels = 16,
		.name = "Phase Switch",
		.default_val = 0
	},
	{  
		.kcontrol_new = &snd_us16x08_ch_int_ctl,
		.control_id = SND_US16X08_ID_FADER,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "Line Volume",
		.default_val = 127
	},
	{  
		.kcontrol_new = &snd_us16x08_ch_boolean_ctl,
		.control_id = SND_US16X08_ID_MUTE,
		.type = USB_MIXER_BOOLEAN,
		.num_channels = 16,
		.name = "Mute Switch",
		.default_val = 0
	},
	{  
		.kcontrol_new = &snd_us16x08_pan_int_ctl,
		.control_id = SND_US16X08_ID_PAN,
		.type = USB_MIXER_U16,
		.num_channels = 16,
		.name = "Pan Left-Right Volume",
		.default_val = 127
	},
};

 
static const struct snd_us16x08_control_params master_controls[] = {
	{  
		.kcontrol_new = &snd_us16x08_master_ctl,
		.control_id = SND_US16X08_ID_FADER,
		.type = USB_MIXER_U8,
		.num_channels = 16,
		.name = "Master Volume",
		.default_val = 127
	},
	{  
		.kcontrol_new = &snd_us16x08_bus_ctl,
		.control_id = SND_US16X08_ID_BYPASS,
		.type = USB_MIXER_BOOLEAN,
		.num_channels = 16,
		.name = "DSP Bypass Switch",
		.default_val = 0
	},
	{  
		.kcontrol_new = &snd_us16x08_bus_ctl,
		.control_id = SND_US16X08_ID_BUSS_OUT,
		.type = USB_MIXER_BOOLEAN,
		.num_channels = 16,
		.name = "Buss Out Switch",
		.default_val = 0
	},
	{  
		.kcontrol_new = &snd_us16x08_bus_ctl,
		.control_id = SND_US16X08_ID_MUTE,
		.type = USB_MIXER_BOOLEAN,
		.num_channels = 16,
		.name = "Master Mute Switch",
		.default_val = 0
	},

};

int snd_us16x08_controls_create(struct usb_mixer_interface *mixer)
{
	int i, j;
	int err;
	struct usb_mixer_elem_info *elem;
	struct snd_us16x08_comp_store *comp_store;
	struct snd_us16x08_meter_store *meter_store;
	struct snd_us16x08_eq_store *eq_store;

	 
	if (mixer->hostif->desc.bInterfaceNumber == 3) {

		 
		err = add_new_ctl(mixer, &snd_us16x08_route_ctl,
			SND_US16X08_ID_ROUTE, USB_MIXER_U8, 8, "Line Out Route",
			NULL, false, &elem);
		if (err < 0) {
			usb_audio_dbg(mixer->chip,
				"Failed to create route control, err:%d\n",
				err);
			return err;
		}
		for (i = 0; i < 8; i++)
			elem->cache_val[i] = i < 2 ? i : i + 2;
		elem->cached = 0xff;

		 
		comp_store = snd_us16x08_create_comp_store();
		if (!comp_store)
			return -ENOMEM;

		 
		for (i = 0; i < ARRAY_SIZE(master_controls); i++) {

			err = add_new_ctl(mixer,
				master_controls[i].kcontrol_new,
				master_controls[i].control_id,
				master_controls[i].type,
				master_controls[i].num_channels,
				master_controls[i].name,
				comp_store,
				i == 0,  
				&elem);
			if (err < 0)
				return err;
			elem->cache_val[0] = master_controls[i].default_val;
			elem->cached = 1;
		}

		 
		for (i = 0; i < ARRAY_SIZE(channel_controls); i++) {

			err = add_new_ctl(mixer,
				channel_controls[i].kcontrol_new,
				channel_controls[i].control_id,
				channel_controls[i].type,
				channel_controls[i].num_channels,
				channel_controls[i].name,
				comp_store,
				false, &elem);
			if (err < 0)
				return err;
			for (j = 0; j < SND_US16X08_MAX_CHANNELS; j++) {
				elem->cache_val[j] =
					channel_controls[i].default_val;
			}
			elem->cached = 0xffff;
		}

		 
		eq_store = snd_us16x08_create_eq_store();
		if (!eq_store)
			return -ENOMEM;

		 
		for (i = 0; i < ARRAY_SIZE(eq_controls); i++) {

			err = add_new_ctl(mixer,
				eq_controls[i].kcontrol_new,
				eq_controls[i].control_id,
				eq_controls[i].type,
				eq_controls[i].num_channels,
				eq_controls[i].name,
				eq_store,
				i == 0,  
				NULL);
			if (err < 0)
				return err;
		}

		 
		for (i = 0; i < ARRAY_SIZE(comp_controls); i++) {

			err = add_new_ctl(mixer,
				comp_controls[i].kcontrol_new,
				comp_controls[i].control_id,
				comp_controls[i].type,
				comp_controls[i].num_channels,
				comp_controls[i].name,
				comp_store,
				false, NULL);
			if (err < 0)
				return err;
		}

		 
		meter_store = snd_us16x08_create_meter_store();
		if (!meter_store)
			return -ENOMEM;

		 
		meter_store->comp_store = comp_store;
		err = add_new_ctl(mixer, &snd_us16x08_meter_ctl,
			SND_US16X08_ID_METER, USB_MIXER_U16, 0, "Level Meter",
			meter_store, true, NULL);
		if (err < 0)
			return err;
	}

	return 0;
}

