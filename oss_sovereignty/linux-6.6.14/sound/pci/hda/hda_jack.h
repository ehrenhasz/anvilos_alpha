 
 

#ifndef __SOUND_HDA_JACK_H
#define __SOUND_HDA_JACK_H

#include <linux/err.h>
#include <sound/jack.h>

struct auto_pin_cfg;
struct hda_jack_tbl;
struct hda_jack_callback;

typedef void (*hda_jack_callback_fn) (struct hda_codec *, struct hda_jack_callback *);

struct hda_jack_callback {
	hda_nid_t nid;
	int dev_id;
	hda_jack_callback_fn func;
	unsigned int private_data;	 
	unsigned int unsol_res;		 
	struct hda_jack_tbl *jack;	 
	struct hda_jack_callback *next;
};

struct hda_jack_tbl {
	hda_nid_t nid;
	int dev_id;
	unsigned char tag;		 
	struct hda_jack_callback *callback;
	 
	unsigned int pin_sense;		 
	unsigned int jack_detect:1;	 
	unsigned int jack_dirty:1;	 
	unsigned int phantom_jack:1;     
	unsigned int block_report:1;     
	hda_nid_t gating_jack;		 
	hda_nid_t gated_jack;		 
	hda_nid_t key_report_jack;	 
	int type;
	int button_state;
	struct snd_jack *jack;
};

struct hda_jack_keymap {
	enum snd_jack_types type;
	int key;
};

struct hda_jack_tbl *
snd_hda_jack_tbl_get_mst(struct hda_codec *codec, hda_nid_t nid, int dev_id);

 
static inline struct hda_jack_tbl *
snd_hda_jack_tbl_get(struct hda_codec *codec, hda_nid_t nid)
{
	return snd_hda_jack_tbl_get_mst(codec, nid, 0);
}

struct hda_jack_tbl *
snd_hda_jack_tbl_get_from_tag(struct hda_codec *codec,
			      unsigned char tag, int dev_id);

void snd_hda_jack_tbl_disconnect(struct hda_codec *codec);
void snd_hda_jack_tbl_clear(struct hda_codec *codec);

void snd_hda_jack_set_dirty_all(struct hda_codec *codec);

int snd_hda_jack_detect_enable(struct hda_codec *codec, hda_nid_t nid,
			       int dev_id);

struct hda_jack_callback *
snd_hda_jack_detect_enable_callback_mst(struct hda_codec *codec, hda_nid_t nid,
					int dev_id, hda_jack_callback_fn func);

 
static inline struct hda_jack_callback *
snd_hda_jack_detect_enable_callback(struct hda_codec *codec, hda_nid_t nid,
				    hda_jack_callback_fn cb)
{
	return snd_hda_jack_detect_enable_callback_mst(codec, nid, 0, cb);
}

int snd_hda_jack_set_gating_jack(struct hda_codec *codec, hda_nid_t gated_nid,
				 hda_nid_t gating_nid);

int snd_hda_jack_bind_keymap(struct hda_codec *codec, hda_nid_t key_nid,
			     const struct hda_jack_keymap *keymap,
			     hda_nid_t jack_nid);

void snd_hda_jack_set_button_state(struct hda_codec *codec, hda_nid_t jack_nid,
				   int button_state);

u32 snd_hda_jack_pin_sense(struct hda_codec *codec, hda_nid_t nid, int dev_id);

 
enum {
	HDA_JACK_NOT_PRESENT, HDA_JACK_PRESENT, HDA_JACK_PHANTOM,
};

int snd_hda_jack_detect_state_mst(struct hda_codec *codec, hda_nid_t nid,
				  int dev_id);

 
static inline int
snd_hda_jack_detect_state(struct hda_codec *codec, hda_nid_t nid)
{
	return snd_hda_jack_detect_state_mst(codec, nid, 0);
}

 
static inline bool
snd_hda_jack_detect_mst(struct hda_codec *codec, hda_nid_t nid, int dev_id)
{
	return snd_hda_jack_detect_state_mst(codec, nid, dev_id) !=
			HDA_JACK_NOT_PRESENT;
}

 
static inline bool
snd_hda_jack_detect(struct hda_codec *codec, hda_nid_t nid)
{
	return snd_hda_jack_detect_mst(codec, nid, 0);
}

bool is_jack_detectable(struct hda_codec *codec, hda_nid_t nid);

int snd_hda_jack_add_kctl_mst(struct hda_codec *codec, hda_nid_t nid,
			      int dev_id, const char *name, bool phantom_jack,
			      int type, const struct hda_jack_keymap *keymap);

 
static inline int
snd_hda_jack_add_kctl(struct hda_codec *codec, hda_nid_t nid,
		      const char *name, bool phantom_jack,
		      int type, const struct hda_jack_keymap *keymap)
{
	return snd_hda_jack_add_kctl_mst(codec, nid, 0,
					 name, phantom_jack, type, keymap);
}

int snd_hda_jack_add_kctls(struct hda_codec *codec,
			   const struct auto_pin_cfg *cfg);

void snd_hda_jack_report_sync(struct hda_codec *codec);

void snd_hda_jack_unsol_event(struct hda_codec *codec, unsigned int res);

void snd_hda_jack_poll_all(struct hda_codec *codec);

#endif  
