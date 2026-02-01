 
 

#ifndef MIDI_H
#define MIDI_H

#include <sound/rawmidi.h>

#include "midibuf.h"

#define MIDI_BUFFER_SIZE 1024

struct snd_line6_midi {
	 
	struct usb_line6 *line6;

	 
	struct snd_rawmidi_substream *substream_receive;

	 
	struct snd_rawmidi_substream *substream_transmit;

	 
	int num_active_send_urbs;

	 
	spinlock_t lock;

	 
	wait_queue_head_t send_wait;

	 
	struct midi_buffer midibuf_in;

	 
	struct midi_buffer midibuf_out;
};

extern int line6_init_midi(struct usb_line6 *line6);
extern void line6_midi_receive(struct usb_line6 *line6, unsigned char *data,
			       int length);

#endif
