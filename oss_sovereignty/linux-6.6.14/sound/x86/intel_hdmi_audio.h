 

#ifndef _INTEL_HDMI_AUDIO_H_
#define _INTEL_HDMI_AUDIO_H_

#include "intel_hdmi_lpe_audio.h"

#define MAX_PB_STREAMS		1
#define MAX_CAP_STREAMS		0
#define BYTES_PER_WORD		0x4
#define INTEL_HAD		"HdmiLpeAudio"

 
enum cea_speaker_placement {
	FL  = (1 <<  0),         
	FC  = (1 <<  1),         
	FR  = (1 <<  2),         
	FLC = (1 <<  3),         
	FRC = (1 <<  4),         
	RL  = (1 <<  5),         
	RC  = (1 <<  6),         
	RR  = (1 <<  7),         
	RLC = (1 <<  8),         
	RRC = (1 <<  9),         
	LFE = (1 << 10),         
};

struct cea_channel_speaker_allocation {
	int ca_index;
	int speakers[8];

	 
	int channels;
	int spk_mask;
};

struct channel_map_table {
	unsigned char map;               
	unsigned char cea_slot;          
	int spk_mask;                    
};

struct pcm_stream_info {
	struct snd_pcm_substream *substream;
	int substream_refcount;
};

 
struct snd_intelhad {
	struct snd_intelhad_card *card_ctx;
	bool		connected;
	struct		pcm_stream_info stream_info;
	unsigned char	eld[HDMI_MAX_ELD_BYTES];
	bool dp_output;
	unsigned int	aes_bits;
	spinlock_t had_spinlock;
	struct device *dev;
	struct snd_pcm_chmap *chmap;
	int tmds_clock_speed;
	int link_rate;
	int port;  
	int pipe;  

	 
	unsigned int bd_head;
	 
	unsigned int pcmbuf_head;	 
	unsigned int pcmbuf_filled;	 

	unsigned int num_bds;		 
	unsigned int period_bytes;	 

	 
	union aud_cfg aud_config;	 
	struct work_struct hdmi_audio_wq;
	struct mutex mutex;  
	struct snd_jack *jack;
};

struct snd_intelhad_card {
	struct snd_card	*card;
	struct device *dev;

	 
	int irq;
	void __iomem *mmio_start;
	int num_pipes;
	int num_ports;
	struct snd_intelhad pcm_ctx[3];  
};

#endif  
