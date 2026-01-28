#ifndef __U_AUDIO_H
#define __U_AUDIO_H
#include <linux/usb/composite.h>
#include "uac_common.h"
#define FBACK_SLOW_MAX	250
#define FBACK_FAST_MAX 5
struct uac_fu_params {
	int id;			 
	bool mute_present;	 
	bool volume_present;	 
	s16 volume_min;		 
	s16 volume_max;		 
	s16 volume_res;		 
};
struct uac_params {
	int p_chmask;	 
	int p_srates[UAC_MAX_RATES];	 
	int p_ssize;	 
	struct uac_fu_params p_fu;	 
	int c_chmask;	 
	int c_srates[UAC_MAX_RATES];	 
	int c_ssize;	 
	struct uac_fu_params c_fu;	 
	int req_number;  
	int fb_max;	 
};
struct g_audio {
	struct usb_function func;
	struct usb_gadget *gadget;
	struct usb_ep *in_ep;
	struct usb_ep *out_ep;
	struct usb_ep *in_ep_fback;
	unsigned int in_ep_maxpsize;
	unsigned int out_ep_maxpsize;
	int (*notify)(struct g_audio *g_audio, int unit_id, int cs);
	struct snd_uac_chip *uac;
	struct uac_params params;
};
static inline struct g_audio *func_to_g_audio(struct usb_function *f)
{
	return container_of(f, struct g_audio, func);
}
static inline uint num_channels(uint chanmask)
{
	uint num = 0;
	while (chanmask) {
		num += (chanmask & 1);
		chanmask >>= 1;
	}
	return num;
}
int g_audio_setup(struct g_audio *g_audio, const char *pcm_name,
					const char *card_name);
void g_audio_cleanup(struct g_audio *g_audio);
int u_audio_start_capture(struct g_audio *g_audio);
void u_audio_stop_capture(struct g_audio *g_audio);
int u_audio_start_playback(struct g_audio *g_audio);
void u_audio_stop_playback(struct g_audio *g_audio);
int u_audio_get_capture_srate(struct g_audio *audio_dev, u32 *val);
int u_audio_set_capture_srate(struct g_audio *audio_dev, int srate);
int u_audio_get_playback_srate(struct g_audio *audio_dev, u32 *val);
int u_audio_set_playback_srate(struct g_audio *audio_dev, int srate);
int u_audio_get_volume(struct g_audio *g_audio, int playback, s16 *val);
int u_audio_set_volume(struct g_audio *g_audio, int playback, s16 val);
int u_audio_get_mute(struct g_audio *g_audio, int playback, int *val);
int u_audio_set_mute(struct g_audio *g_audio, int playback, int val);
void u_audio_suspend(struct g_audio *g_audio);
#endif  
