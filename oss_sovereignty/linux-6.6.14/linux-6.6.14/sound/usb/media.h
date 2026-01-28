#ifndef __MEDIA_H
#ifdef CONFIG_SND_USB_AUDIO_USE_MEDIA_CONTROLLER
#include <linux/media.h>
#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/media-dev-allocator.h>
#include <sound/asound.h>
struct media_ctl {
	struct media_device *media_dev;
	struct media_entity media_entity;
	struct media_intf_devnode *intf_devnode;
	struct media_link *intf_link;
	struct media_pad media_pad;
	struct media_pipeline media_pipe;
};
#define MEDIA_MIXER_PAD_MAX    (SNDRV_PCM_STREAM_LAST + 2)
struct media_mixer_ctl {
	struct media_device *media_dev;
	struct media_entity media_entity;
	struct media_intf_devnode *intf_devnode;
	struct media_link *intf_link;
	struct media_pad media_pad[MEDIA_MIXER_PAD_MAX];
	struct media_pipeline media_pipe;
};
int snd_media_device_create(struct snd_usb_audio *chip,
			    struct usb_interface *iface);
void snd_media_device_delete(struct snd_usb_audio *chip);
int snd_media_stream_init(struct snd_usb_substream *subs, struct snd_pcm *pcm,
			  int stream);
void snd_media_stream_delete(struct snd_usb_substream *subs);
int snd_media_start_pipeline(struct snd_usb_substream *subs);
void snd_media_stop_pipeline(struct snd_usb_substream *subs);
#else
static inline int snd_media_device_create(struct snd_usb_audio *chip,
					  struct usb_interface *iface)
						{ return 0; }
static inline void snd_media_device_delete(struct snd_usb_audio *chip) { }
static inline int snd_media_stream_init(struct snd_usb_substream *subs,
					struct snd_pcm *pcm, int stream)
						{ return 0; }
static inline void snd_media_stream_delete(struct snd_usb_substream *subs) { }
static inline int snd_media_start_pipeline(struct snd_usb_substream *subs)
					{ return 0; }
static inline void snd_media_stop_pipeline(struct snd_usb_substream *subs) { }
#endif
#endif  
