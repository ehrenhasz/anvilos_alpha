 
#ifndef __USBMIDI_H
#define __USBMIDI_H

 
#define MIDI_MAX_ENDPOINTS 2

 
struct snd_usb_midi_endpoint_info {
	int8_t   out_ep;	 
	uint8_t  out_interval;	 
	int8_t   in_ep;
	uint8_t  in_interval;
	uint16_t out_cables;	 
	uint16_t in_cables;	 
	int16_t  assoc_in_jacks[16];
	int16_t  assoc_out_jacks[16];
};

 

 

 

 

 

 

 

 

 

 

 

int __snd_usbmidi_create(struct snd_card *card,
			 struct usb_interface *iface,
			 struct list_head *midi_list,
			 const struct snd_usb_audio_quirk *quirk,
			 unsigned int usb_id,
			 unsigned int *num_rawmidis);

static inline int snd_usbmidi_create(struct snd_card *card,
		       struct usb_interface *iface,
		       struct list_head *midi_list,
		       const struct snd_usb_audio_quirk *quirk)
{
	return __snd_usbmidi_create(card, iface, midi_list, quirk, 0, NULL);
}

void snd_usbmidi_input_stop(struct list_head *p);
void snd_usbmidi_input_start(struct list_head *p);
void snd_usbmidi_disconnect(struct list_head *p);
void snd_usbmidi_suspend(struct list_head *p);
void snd_usbmidi_resume(struct list_head *p);

#endif  
