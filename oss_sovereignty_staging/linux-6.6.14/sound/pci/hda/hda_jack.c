
 

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/jack.h>
#include <sound/hda_codec.h>
#include "hda_local.h"
#include "hda_auto_parser.h"
#include "hda_jack.h"

 
bool is_jack_detectable(struct hda_codec *codec, hda_nid_t nid)
{
	if (codec->no_jack_detect)
		return false;
	if (!(snd_hda_query_pin_caps(codec, nid) & AC_PINCAP_PRES_DETECT))
		return false;
	if (get_defcfg_misc(snd_hda_codec_get_pincfg(codec, nid)) &
	     AC_DEFCFG_MISC_NO_PRESENCE)
		return false;
	if (!(get_wcaps(codec, nid) & AC_WCAP_UNSOL_CAP) &&
	    !codec->jackpoll_interval)
		return false;
	return true;
}
EXPORT_SYMBOL_GPL(is_jack_detectable);

 
static u32 read_pin_sense(struct hda_codec *codec, hda_nid_t nid, int dev_id)
{
	u32 pincap;
	u32 val;

	if (!codec->no_trigger_sense) {
		pincap = snd_hda_query_pin_caps(codec, nid);
		if (pincap & AC_PINCAP_TRIG_REQ)  
			snd_hda_codec_read(codec, nid, 0,
					AC_VERB_SET_PIN_SENSE, 0);
	}
	val = snd_hda_codec_read(codec, nid, 0,
				  AC_VERB_GET_PIN_SENSE, dev_id);
	if (codec->inv_jack_detect)
		val ^= AC_PINSENSE_PRESENCE;
	return val;
}

 
struct hda_jack_tbl *
snd_hda_jack_tbl_get_mst(struct hda_codec *codec, hda_nid_t nid, int dev_id)
{
	struct hda_jack_tbl *jack = codec->jacktbl.list;
	int i;

	if (!nid || !jack)
		return NULL;
	for (i = 0; i < codec->jacktbl.used; i++, jack++)
		if (jack->nid == nid && jack->dev_id == dev_id)
			return jack;
	return NULL;
}
EXPORT_SYMBOL_GPL(snd_hda_jack_tbl_get_mst);

 
struct hda_jack_tbl *
snd_hda_jack_tbl_get_from_tag(struct hda_codec *codec,
			      unsigned char tag, int dev_id)
{
	struct hda_jack_tbl *jack = codec->jacktbl.list;
	int i;

	if (!tag || !jack)
		return NULL;
	for (i = 0; i < codec->jacktbl.used; i++, jack++)
		if (jack->tag == tag && jack->dev_id == dev_id)
			return jack;
	return NULL;
}
EXPORT_SYMBOL_GPL(snd_hda_jack_tbl_get_from_tag);

static struct hda_jack_tbl *
any_jack_tbl_get_from_nid(struct hda_codec *codec, hda_nid_t nid)
{
	struct hda_jack_tbl *jack = codec->jacktbl.list;
	int i;

	if (!nid || !jack)
		return NULL;
	for (i = 0; i < codec->jacktbl.used; i++, jack++)
		if (jack->nid == nid)
			return jack;
	return NULL;
}

 
static struct hda_jack_tbl *
snd_hda_jack_tbl_new(struct hda_codec *codec, hda_nid_t nid, int dev_id)
{
	struct hda_jack_tbl *jack =
		snd_hda_jack_tbl_get_mst(codec, nid, dev_id);
	struct hda_jack_tbl *existing_nid_jack =
		any_jack_tbl_get_from_nid(codec, nid);

	WARN_ON(dev_id != 0 && !codec->dp_mst);

	if (jack)
		return jack;
	jack = snd_array_new(&codec->jacktbl);
	if (!jack)
		return NULL;
	jack->nid = nid;
	jack->dev_id = dev_id;
	jack->jack_dirty = 1;
	if (existing_nid_jack) {
		jack->tag = existing_nid_jack->tag;

		 
		jack->jack_detect = existing_nid_jack->jack_detect;
	} else {
		jack->tag = codec->jacktbl.used;
	}

	return jack;
}

void snd_hda_jack_tbl_disconnect(struct hda_codec *codec)
{
	struct hda_jack_tbl *jack = codec->jacktbl.list;
	int i;

	for (i = 0; i < codec->jacktbl.used; i++, jack++) {
		if (!codec->bus->shutdown && jack->jack)
			snd_device_disconnect(codec->card, jack->jack);
	}
}

void snd_hda_jack_tbl_clear(struct hda_codec *codec)
{
	struct hda_jack_tbl *jack = codec->jacktbl.list;
	int i;

	for (i = 0; i < codec->jacktbl.used; i++, jack++) {
		struct hda_jack_callback *cb, *next;

		 
		if (!codec->bus->shutdown && jack->jack)
			snd_device_free(codec->card, jack->jack);

		for (cb = jack->callback; cb; cb = next) {
			next = cb->next;
			kfree(cb);
		}
	}
	snd_array_free(&codec->jacktbl);
}

#define get_jack_plug_state(sense) !!(sense & AC_PINSENSE_PRESENCE)

 
static void jack_detect_update(struct hda_codec *codec,
			       struct hda_jack_tbl *jack)
{
	if (!jack->jack_dirty)
		return;

	if (jack->phantom_jack)
		jack->pin_sense = AC_PINSENSE_PRESENCE;
	else
		jack->pin_sense = read_pin_sense(codec, jack->nid,
						 jack->dev_id);

	 
	if (jack->gating_jack &&
	    !snd_hda_jack_detect_mst(codec, jack->gating_jack, jack->dev_id))
		jack->pin_sense &= ~AC_PINSENSE_PRESENCE;

	jack->jack_dirty = 0;

	 
	if (jack->gated_jack) {
		struct hda_jack_tbl *gated =
			snd_hda_jack_tbl_get_mst(codec, jack->gated_jack,
						 jack->dev_id);
		if (gated) {
			gated->jack_dirty = 1;
			jack_detect_update(codec, gated);
		}
	}
}

 
void snd_hda_jack_set_dirty_all(struct hda_codec *codec)
{
	struct hda_jack_tbl *jack = codec->jacktbl.list;
	int i;

	for (i = 0; i < codec->jacktbl.used; i++, jack++)
		if (jack->nid)
			jack->jack_dirty = 1;
}
EXPORT_SYMBOL_GPL(snd_hda_jack_set_dirty_all);

 
u32 snd_hda_jack_pin_sense(struct hda_codec *codec, hda_nid_t nid, int dev_id)
{
	struct hda_jack_tbl *jack =
		snd_hda_jack_tbl_get_mst(codec, nid, dev_id);
	if (jack) {
		jack_detect_update(codec, jack);
		return jack->pin_sense;
	}
	return read_pin_sense(codec, nid, dev_id);
}
EXPORT_SYMBOL_GPL(snd_hda_jack_pin_sense);

 
int snd_hda_jack_detect_state_mst(struct hda_codec *codec,
				  hda_nid_t nid, int dev_id)
{
	struct hda_jack_tbl *jack =
		snd_hda_jack_tbl_get_mst(codec, nid, dev_id);
	if (jack && jack->phantom_jack)
		return HDA_JACK_PHANTOM;
	else if (snd_hda_jack_pin_sense(codec, nid, dev_id) &
		 AC_PINSENSE_PRESENCE)
		return HDA_JACK_PRESENT;
	else
		return HDA_JACK_NOT_PRESENT;
}
EXPORT_SYMBOL_GPL(snd_hda_jack_detect_state_mst);

static struct hda_jack_callback *
find_callback_from_list(struct hda_jack_tbl *jack,
			hda_jack_callback_fn func)
{
	struct hda_jack_callback *cb;

	if (!func)
		return NULL;

	for (cb = jack->callback; cb; cb = cb->next) {
		if (cb->func == func)
			return cb;
	}

	return NULL;
}

 
struct hda_jack_callback *
snd_hda_jack_detect_enable_callback_mst(struct hda_codec *codec, hda_nid_t nid,
					int dev_id, hda_jack_callback_fn func)
{
	struct hda_jack_tbl *jack;
	struct hda_jack_callback *callback = NULL;
	int err;

	jack = snd_hda_jack_tbl_new(codec, nid, dev_id);
	if (!jack)
		return ERR_PTR(-ENOMEM);

	callback = find_callback_from_list(jack, func);

	if (func && !callback) {
		callback = kzalloc(sizeof(*callback), GFP_KERNEL);
		if (!callback)
			return ERR_PTR(-ENOMEM);
		callback->func = func;
		callback->nid = jack->nid;
		callback->dev_id = jack->dev_id;
		callback->next = jack->callback;
		jack->callback = callback;
	}

	if (jack->jack_detect)
		return callback;  
	jack->jack_detect = 1;
	if (codec->jackpoll_interval > 0)
		return callback;  
	err = snd_hda_codec_write_cache(codec, nid, 0,
					 AC_VERB_SET_UNSOLICITED_ENABLE,
					 AC_USRSP_EN | jack->tag);
	if (err < 0)
		return ERR_PTR(err);
	return callback;
}
EXPORT_SYMBOL_GPL(snd_hda_jack_detect_enable_callback_mst);

 
int snd_hda_jack_detect_enable(struct hda_codec *codec, hda_nid_t nid,
			       int dev_id)
{
	return PTR_ERR_OR_ZERO(snd_hda_jack_detect_enable_callback_mst(codec,
								       nid,
								       dev_id,
								       NULL));
}
EXPORT_SYMBOL_GPL(snd_hda_jack_detect_enable);

 
int snd_hda_jack_set_gating_jack(struct hda_codec *codec, hda_nid_t gated_nid,
				 hda_nid_t gating_nid)
{
	struct hda_jack_tbl *gated = snd_hda_jack_tbl_new(codec, gated_nid, 0);
	struct hda_jack_tbl *gating =
		snd_hda_jack_tbl_new(codec, gating_nid, 0);

	WARN_ON(codec->dp_mst);

	if (!gated || !gating)
		return -EINVAL;

	gated->gating_jack = gating_nid;
	gating->gated_jack = gated_nid;

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hda_jack_set_gating_jack);

 
int snd_hda_jack_bind_keymap(struct hda_codec *codec, hda_nid_t key_nid,
			     const struct hda_jack_keymap *keymap,
			     hda_nid_t jack_nid)
{
	const struct hda_jack_keymap *map;
	struct hda_jack_tbl *key_gen = snd_hda_jack_tbl_get(codec, key_nid);
	struct hda_jack_tbl *report_to = snd_hda_jack_tbl_get(codec, jack_nid);

	WARN_ON(codec->dp_mst);

	if (!key_gen || !report_to || !report_to->jack)
		return -EINVAL;

	key_gen->key_report_jack = jack_nid;

	if (keymap)
		for (map = keymap; map->type; map++)
			snd_jack_set_key(report_to->jack, map->type, map->key);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hda_jack_bind_keymap);

 
void snd_hda_jack_set_button_state(struct hda_codec *codec, hda_nid_t jack_nid,
				   int button_state)
{
	struct hda_jack_tbl *jack = snd_hda_jack_tbl_get(codec, jack_nid);

	if (!jack)
		return;

	if (jack->key_report_jack) {
		struct hda_jack_tbl *report_to =
			snd_hda_jack_tbl_get(codec, jack->key_report_jack);

		if (report_to) {
			report_to->button_state = button_state;
			return;
		}
	}

	jack->button_state = button_state;
}
EXPORT_SYMBOL_GPL(snd_hda_jack_set_button_state);

 
void snd_hda_jack_report_sync(struct hda_codec *codec)
{
	struct hda_jack_tbl *jack;
	int i, state;

	 
	jack = codec->jacktbl.list;
	for (i = 0; i < codec->jacktbl.used; i++, jack++)
		if (jack->nid)
			jack_detect_update(codec, jack);

	 
	jack = codec->jacktbl.list;
	for (i = 0; i < codec->jacktbl.used; i++, jack++)
		if (jack->nid) {
			if (!jack->jack || jack->block_report)
				continue;
			state = jack->button_state;
			if (get_jack_plug_state(jack->pin_sense))
				state |= jack->type;
			snd_jack_report(jack->jack, state);
			if (jack->button_state) {
				snd_jack_report(jack->jack,
						state & ~jack->button_state);
				jack->button_state = 0;  
			}
		}
}
EXPORT_SYMBOL_GPL(snd_hda_jack_report_sync);

 
static int get_input_jack_type(struct hda_codec *codec, hda_nid_t nid)
{
	unsigned int def_conf = snd_hda_codec_get_pincfg(codec, nid);
	switch (get_defcfg_device(def_conf)) {
	case AC_JACK_LINE_OUT:
	case AC_JACK_SPEAKER:
		return SND_JACK_LINEOUT;
	case AC_JACK_HP_OUT:
		return SND_JACK_HEADPHONE;
	case AC_JACK_SPDIF_OUT:
	case AC_JACK_DIG_OTHER_OUT:
		return SND_JACK_AVOUT;
	case AC_JACK_MIC_IN:
		return SND_JACK_MICROPHONE;
	default:
		return SND_JACK_LINEIN;
	}
}

static void hda_free_jack_priv(struct snd_jack *jack)
{
	struct hda_jack_tbl *jacks = jack->private_data;
	jacks->nid = 0;
	jacks->jack = NULL;
}

 
int snd_hda_jack_add_kctl_mst(struct hda_codec *codec, hda_nid_t nid,
			      int dev_id, const char *name, bool phantom_jack,
			      int type, const struct hda_jack_keymap *keymap)
{
	struct hda_jack_tbl *jack;
	const struct hda_jack_keymap *map;
	int err, state, buttons;

	jack = snd_hda_jack_tbl_new(codec, nid, dev_id);
	if (!jack)
		return 0;
	if (jack->jack)
		return 0;  

	if (!type)
		type = get_input_jack_type(codec, nid);

	buttons = 0;
	if (keymap) {
		for (map = keymap; map->type; map++)
			buttons |= map->type;
	}

	err = snd_jack_new(codec->card, name, type | buttons,
			   &jack->jack, true, phantom_jack);
	if (err < 0)
		return err;

	jack->phantom_jack = !!phantom_jack;
	jack->type = type;
	jack->button_state = 0;
	jack->jack->private_data = jack;
	jack->jack->private_free = hda_free_jack_priv;
	if (keymap) {
		for (map = keymap; map->type; map++)
			snd_jack_set_key(jack->jack, map->type, map->key);
	}

	state = snd_hda_jack_detect_mst(codec, nid, dev_id);
	snd_jack_report(jack->jack, state ? jack->type : 0);

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hda_jack_add_kctl_mst);

static int add_jack_kctl(struct hda_codec *codec, hda_nid_t nid,
			 const struct auto_pin_cfg *cfg,
			 const char *base_name)
{
	unsigned int def_conf, conn;
	char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
	int err;
	bool phantom_jack;

	WARN_ON(codec->dp_mst);

	if (!nid)
		return 0;
	def_conf = snd_hda_codec_get_pincfg(codec, nid);
	conn = get_defcfg_connect(def_conf);
	if (conn == AC_JACK_PORT_NONE)
		return 0;
	phantom_jack = (conn != AC_JACK_PORT_COMPLEX) ||
		       !is_jack_detectable(codec, nid);

	if (base_name)
		strscpy(name, base_name, sizeof(name));
	else
		snd_hda_get_pin_label(codec, nid, cfg, name, sizeof(name), NULL);
	if (phantom_jack)
		 
		strncat(name, " Phantom", sizeof(name) - strlen(name) - 1);
	err = snd_hda_jack_add_kctl(codec, nid, name, phantom_jack, 0, NULL);
	if (err < 0)
		return err;

	if (!phantom_jack)
		return snd_hda_jack_detect_enable(codec, nid, 0);
	return 0;
}

 
int snd_hda_jack_add_kctls(struct hda_codec *codec,
			   const struct auto_pin_cfg *cfg)
{
	const hda_nid_t *p;
	int i, err;

	for (i = 0; i < cfg->num_inputs; i++) {
		 
		if (cfg->inputs[i].is_headphone_mic) {
			if (auto_cfg_hp_outs(cfg) == 1)
				err = add_jack_kctl(codec, auto_cfg_hp_pins(cfg)[0],
						    cfg, "Headphone Mic");
			else
				err = add_jack_kctl(codec, cfg->inputs[i].pin,
						    cfg, "Headphone Mic");
		} else
			err = add_jack_kctl(codec, cfg->inputs[i].pin, cfg,
					    NULL);
		if (err < 0)
			return err;
	}

	for (i = 0, p = cfg->line_out_pins; i < cfg->line_outs; i++, p++) {
		err = add_jack_kctl(codec, *p, cfg, NULL);
		if (err < 0)
			return err;
	}
	for (i = 0, p = cfg->hp_pins; i < cfg->hp_outs; i++, p++) {
		if (*p == *cfg->line_out_pins)  
			break;
		err = add_jack_kctl(codec, *p, cfg, NULL);
		if (err < 0)
			return err;
	}
	for (i = 0, p = cfg->speaker_pins; i < cfg->speaker_outs; i++, p++) {
		if (*p == *cfg->line_out_pins)  
			break;
		err = add_jack_kctl(codec, *p, cfg, NULL);
		if (err < 0)
			return err;
	}
	for (i = 0, p = cfg->dig_out_pins; i < cfg->dig_outs; i++, p++) {
		err = add_jack_kctl(codec, *p, cfg, NULL);
		if (err < 0)
			return err;
	}
	err = add_jack_kctl(codec, cfg->dig_in_pin, cfg, NULL);
	if (err < 0)
		return err;
	err = add_jack_kctl(codec, cfg->mono_out_pin, cfg, NULL);
	if (err < 0)
		return err;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_hda_jack_add_kctls);

static void call_jack_callback(struct hda_codec *codec, unsigned int res,
			       struct hda_jack_tbl *jack)
{
	struct hda_jack_callback *cb;

	for (cb = jack->callback; cb; cb = cb->next) {
		cb->jack = jack;
		cb->unsol_res = res;
		cb->func(codec, cb);
	}
	if (jack->gated_jack) {
		struct hda_jack_tbl *gated =
			snd_hda_jack_tbl_get_mst(codec, jack->gated_jack,
						 jack->dev_id);
		if (gated) {
			for (cb = gated->callback; cb; cb = cb->next) {
				cb->jack = gated;
				cb->unsol_res = res;
				cb->func(codec, cb);
			}
		}
	}
}

 
void snd_hda_jack_unsol_event(struct hda_codec *codec, unsigned int res)
{
	struct hda_jack_tbl *event;
	int tag = (res & AC_UNSOL_RES_TAG) >> AC_UNSOL_RES_TAG_SHIFT;

	if (codec->dp_mst) {
		int dev_entry =
			(res & AC_UNSOL_RES_DE) >> AC_UNSOL_RES_DE_SHIFT;

		event = snd_hda_jack_tbl_get_from_tag(codec, tag, dev_entry);
	} else {
		event = snd_hda_jack_tbl_get_from_tag(codec, tag, 0);
	}
	if (!event)
		return;

	if (event->key_report_jack) {
		struct hda_jack_tbl *report_to =
			snd_hda_jack_tbl_get_mst(codec, event->key_report_jack,
						 event->dev_id);
		if (report_to)
			report_to->jack_dirty = 1;
	} else
		event->jack_dirty = 1;

	call_jack_callback(codec, res, event);
	snd_hda_jack_report_sync(codec);
}
EXPORT_SYMBOL_GPL(snd_hda_jack_unsol_event);

 
void snd_hda_jack_poll_all(struct hda_codec *codec)
{
	struct hda_jack_tbl *jack = codec->jacktbl.list;
	int i, changes = 0;

	for (i = 0; i < codec->jacktbl.used; i++, jack++) {
		unsigned int old_sense;
		if (!jack->nid || !jack->jack_dirty || jack->phantom_jack)
			continue;
		old_sense = get_jack_plug_state(jack->pin_sense);
		jack_detect_update(codec, jack);
		if (old_sense == get_jack_plug_state(jack->pin_sense))
			continue;
		changes = 1;
		call_jack_callback(codec, 0, jack);
	}
	if (changes)
		snd_hda_jack_report_sync(codec);
}
EXPORT_SYMBOL_GPL(snd_hda_jack_poll_all);

