 
#ifndef __USBAUDIO_CARD_H
#define __USBAUDIO_CARD_H

#define MAX_NR_RATES	1024
#define MAX_PACKS	6		 
#define MAX_PACKS_HS	(MAX_PACKS * 8)	 
#define MAX_URBS	12
#define SYNC_URBS	4	 
#define MAX_QUEUE	18	 

struct audioformat {
	struct list_head list;
	u64 formats;			 
	unsigned int channels;		 
	unsigned int fmt_type;		 
	unsigned int fmt_bits;		 
	unsigned int frame_size;	 
	unsigned char iface;		 
	unsigned char altsetting;	 
	unsigned char ep_idx;		 
	unsigned char altset_idx;	 
	unsigned char attributes;	 
	unsigned char endpoint;		 
	unsigned char ep_attr;		 
	bool implicit_fb;		 
	unsigned char sync_ep;		 
	unsigned char sync_iface;	 
	unsigned char sync_altsetting;	 
	unsigned char sync_ep_idx;	 
	unsigned char datainterval;	 
	unsigned char protocol;		 
	unsigned int maxpacksize;	 
	unsigned int rates;		 
	unsigned int rate_min, rate_max;	 
	unsigned int nr_rates;		 
	unsigned int *rate_table;	 
	unsigned char clock;		 
	struct snd_pcm_chmap_elem *chmap;  
	bool dsd_dop;			 
	bool dsd_bitrev;		 
	bool dsd_raw;			 
};

struct snd_usb_substream;
struct snd_usb_iface_ref;
struct snd_usb_clock_ref;
struct snd_usb_endpoint;
struct snd_usb_power_domain;

struct snd_urb_ctx {
	struct urb *urb;
	unsigned int buffer_size;	 
	struct snd_usb_substream *subs;
	struct snd_usb_endpoint *ep;
	int index;	 
	int packets;	 
	int queued;	 
	int packet_size[MAX_PACKS_HS];  
	struct list_head ready_list;
};

struct snd_usb_endpoint {
	struct snd_usb_audio *chip;
	struct snd_usb_iface_ref *iface_ref;
	struct snd_usb_clock_ref *clock_ref;

	int opened;		 
	atomic_t running;	 
	int ep_num;		 
	int type;		 

	unsigned char iface;		 
	unsigned char altsetting;	 
	unsigned char ep_idx;		 

	atomic_t state;		 

	int (*prepare_data_urb) (struct snd_usb_substream *subs,
				 struct urb *urb,
				 bool in_stream_lock);
	void (*retire_data_urb) (struct snd_usb_substream *subs,
				 struct urb *urb);

	struct snd_usb_substream *data_subs;
	struct snd_usb_endpoint *sync_source;
	struct snd_usb_endpoint *sync_sink;

	struct snd_urb_ctx urb[MAX_URBS];

	struct snd_usb_packet_info {
		uint32_t packet_size[MAX_PACKS_HS];
		int packets;
	} next_packet[MAX_URBS];
	unsigned int next_packet_head;	 
	unsigned int next_packet_queued;  
	struct list_head ready_playback_urbs;  

	unsigned int nurbs;		 
	unsigned long active_mask;	 
	unsigned long unlink_mask;	 
	atomic_t submitted_urbs;	 
	char *syncbuf;			 
	dma_addr_t sync_dma;		 

	unsigned int pipe;		 
	unsigned int packsize[2];	 
	unsigned int sample_rem;	 
	unsigned int sample_accum;	 
	unsigned int pps;		 
	unsigned int freqn;		 
	unsigned int freqm;		 
	int	   freqshift;		 
	unsigned int freqmax;		 
	unsigned int phase;		 
	unsigned int maxpacksize;	 
	unsigned int maxframesize;       
	unsigned int max_urb_frames;	 
	unsigned int curpacksize;	 
	unsigned int curframesize;       
	unsigned int syncmaxsize;	 
	unsigned int fill_max:1;	 
	unsigned int tenor_fb_quirk:1;	 
	unsigned int datainterval;       
	unsigned int syncinterval;	 
	unsigned char silence_value;
	unsigned int stride;
	int skip_packets;		 
	bool implicit_fb_sync;		 
	bool lowlatency_playback;	 
	bool need_setup;		 
	bool need_prepare;		 
	bool fixed_rate;		 

	 
	const struct audioformat *cur_audiofmt;
	unsigned int cur_rate;
	snd_pcm_format_t cur_format;
	unsigned int cur_channels;
	unsigned int cur_frame_bytes;
	unsigned int cur_period_frames;
	unsigned int cur_period_bytes;
	unsigned int cur_buffer_periods;

	spinlock_t lock;
	struct list_head list;
};

struct media_ctl;

struct snd_usb_substream {
	struct snd_usb_stream *stream;
	struct usb_device *dev;
	struct snd_pcm_substream *pcm_substream;
	int direction;	 
	int endpoint;	 
	const struct audioformat *cur_audiofmt;	 
	struct snd_usb_power_domain *str_pd;	 
	unsigned int channels_max;	 
	unsigned int txfr_quirk:1;	 
	unsigned int tx_length_quirk:1;	 
	unsigned int fmt_type;		 
	unsigned int pkt_offset_adj;	 
	unsigned int stream_offset_adj;	 

	unsigned int running: 1;	 
	unsigned int period_elapsed_pending;	 

	unsigned int buffer_bytes;	 
	unsigned int inflight_bytes;	 
	unsigned int hwptr_done;	 
	unsigned int transfer_done;	 
	unsigned int frame_limit;	 

	 
	unsigned int ep_num;		 
	struct snd_usb_endpoint *data_endpoint;
	struct snd_usb_endpoint *sync_endpoint;
	unsigned long flags;
	unsigned int speed;		 

	u64 formats;			 
	unsigned int num_formats;		 
	struct list_head fmt_list;	 
	spinlock_t lock;

	unsigned int last_frame_number;	 

	struct {
		int marker;
		int channel;
		int byte_idx;
	} dsd_dop;

	bool trigger_tstamp_pending_update;  
	bool lowlatency_playback;	 
	struct media_ctl *media_ctl;
};

struct snd_usb_stream {
	struct snd_usb_audio *chip;
	struct snd_pcm *pcm;
	int pcm_index;
	unsigned int fmt_type;		 
	struct snd_usb_substream substream[2];
	struct list_head list;
};

#endif  
