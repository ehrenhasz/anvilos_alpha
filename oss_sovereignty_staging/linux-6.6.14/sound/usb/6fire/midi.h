 
 

#ifndef USB6FIRE_MIDI_H
#define USB6FIRE_MIDI_H

#include "common.h"

struct midi_runtime {
	struct sfire_chip *chip;
	struct snd_rawmidi *instance;

	struct snd_rawmidi_substream *in;
	char in_active;

	spinlock_t in_lock;
	spinlock_t out_lock;
	struct snd_rawmidi_substream *out;
	struct urb out_urb;
	u8 out_serial;  
	u8 *out_buffer;
	int buffer_offset;

	void (*in_received)(struct midi_runtime *rt, u8 *data, int length);
};

int usb6fire_midi_init(struct sfire_chip *chip);
void usb6fire_midi_abort(struct sfire_chip *chip);
void usb6fire_midi_destroy(struct sfire_chip *chip);
#endif  

