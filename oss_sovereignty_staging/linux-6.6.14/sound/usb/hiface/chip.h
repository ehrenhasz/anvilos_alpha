 
 

#ifndef HIFACE_CHIP_H
#define HIFACE_CHIP_H

#include <linux/usb.h>
#include <sound/core.h>

struct pcm_runtime;

struct hiface_chip {
	struct usb_device *dev;
	struct snd_card *card;
	struct pcm_runtime *pcm;
};
#endif  
