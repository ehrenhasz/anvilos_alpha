 
 

#ifndef LX6464ES_H
#define LX6464ES_H

#include <linux/spinlock.h>
#include <linux/atomic.h>

#include <sound/core.h>
#include <sound/pcm.h>

#include "lx_core.h"

#define LXP "LX6464ES: "

enum {
    ES_cmd_free         = 0,     
    ES_cmd_processing   = 1,	 
    ES_read_pending     = 2,     
    ES_read_finishing   = 3,     
};

enum lx_stream_status {
	LX_STREAM_STATUS_FREE,
 
	LX_STREAM_STATUS_SCHEDULE_RUN,
 
	LX_STREAM_STATUS_RUNNING,
	LX_STREAM_STATUS_SCHEDULE_STOP,
 
 
};


struct lx_stream {
	struct snd_pcm_substream  *stream;
	snd_pcm_uframes_t          frame_pos;
	enum lx_stream_status      status;  
	unsigned int               is_capture:1;
};


struct lx6464es {
	struct snd_card        *card;
	struct pci_dev         *pci;
	int			irq;

	u8			mac_address[6];

	struct mutex		lock;         
	struct mutex            setup_mutex;  

	 
	unsigned long		port_plx;	    
	void __iomem           *port_plx_remapped;  
	void __iomem           *port_dsp_bar;       

	 
	struct mutex		msg_lock;           
	struct lx_rmh           rmh;
	u32			irqsrc;

	 
	uint			freq_ratio : 2;
	uint                    playback_mute : 1;
	uint                    hardware_running[2];
	u32                     board_sample_rate;  
	u16                     pcm_granularity;    

	 
	struct snd_dma_buffer   capture_dma_buf;
	struct snd_dma_buffer   playback_dma_buf;

	 
	struct snd_pcm         *pcm;

	 
	struct lx_stream        capture_stream;
	struct lx_stream        playback_stream;
};


#endif  
