 
 

 

#ifndef PCM_H
#define PCM_H

#include <sound/pcm.h>

#include "driver.h"

 
#define LINE6_ISO_PACKETS	1

 
#define LINE6_ISO_INTERVAL	1

#define LINE6_IMPULSE_DEFAULT_PERIOD 100

 
#define get_substream(line6pcm, stream)	\
		(line6pcm->pcm->streams[stream].substream)

 

 
enum {
	LINE6_STREAM_PCM,
	LINE6_STREAM_MONITOR,
	LINE6_STREAM_IMPULSE,
	LINE6_STREAM_CAPTURE_HELPER,
};

 
enum {
	LINE6_FLAG_PAUSE_PLAYBACK,
	LINE6_FLAG_PREPARED,
};

struct line6_pcm_properties {
	struct snd_pcm_hardware playback_hw, capture_hw;
	struct snd_pcm_hw_constraint_ratdens rates;
	int bytes_per_channel;
};

struct line6_pcm_stream {
	 
	struct urb **urbs;

	 
	unsigned char *buffer;

	 
	snd_pcm_uframes_t pos;

	 
	unsigned bytes;

	 
	unsigned count;

	 
	unsigned period;

	 
	snd_pcm_uframes_t pos_done;

	 
	unsigned long active_urbs;

	 
	unsigned long unlink_urbs;

	 
	spinlock_t lock;

	 
	unsigned long opened;

	 
	unsigned long running;

	int last_frame;
};

struct snd_line6_pcm {
	 
	struct usb_line6 *line6;

	 
	struct line6_pcm_properties *properties;

	 
	struct snd_pcm *pcm;

	 
	struct mutex state_mutex;

	 
	struct line6_pcm_stream in;
	struct line6_pcm_stream out;

	 
	unsigned char *prev_fbuf;

	 
	int prev_fsize;

	 
	int max_packet_size_in;
	int max_packet_size_out;

	 
	int volume_playback[2];

	 
	int volume_monitor;

	 
	int impulse_volume;

	 
	int impulse_period;

	 
	int impulse_count;

	 
	unsigned long flags;
};

extern int line6_init_pcm(struct usb_line6 *line6,
			  struct line6_pcm_properties *properties);
extern int snd_line6_trigger(struct snd_pcm_substream *substream, int cmd);
extern int snd_line6_prepare(struct snd_pcm_substream *substream);
extern int snd_line6_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *hw_params);
extern int snd_line6_hw_free(struct snd_pcm_substream *substream);
extern snd_pcm_uframes_t snd_line6_pointer(struct snd_pcm_substream *substream);
extern void line6_pcm_disconnect(struct snd_line6_pcm *line6pcm);
extern int line6_pcm_acquire(struct snd_line6_pcm *line6pcm, int type,
			       bool start);
extern void line6_pcm_release(struct snd_line6_pcm *line6pcm, int type);

#endif
