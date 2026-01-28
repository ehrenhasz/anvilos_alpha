#ifndef USB6FIRE_CHIP_H
#define USB6FIRE_CHIP_H
#include "common.h"
struct sfire_chip {
	struct usb_device *dev;
	struct snd_card *card;
	int intf_count;  
	int regidx;  
	bool shutdown;
	struct midi_runtime *midi;
	struct pcm_runtime *pcm;
	struct control_runtime *control;
	struct comm_runtime *comm;
};
#endif  
