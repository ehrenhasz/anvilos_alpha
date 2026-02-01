




#include <linux/init.h>
#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>
#include <linux/usb/audio-v3.h>
#include <linux/usb/midi.h>
#include "usbaudio.h"
#include "helper.h"

struct usb_desc_validator {
	unsigned char protocol;
	unsigned char type;
	bool (*func)(const void *p, const struct usb_desc_validator *v);
	size_t size;
};

#define UAC_VERSION_ALL		(unsigned char)(-1)

 
static bool validate_uac1_header(const void *p,
				 const struct usb_desc_validator *v)
{
	const struct uac1_ac_header_descriptor *d = p;

	return d->bLength >= sizeof(*d) &&
		d->bLength >= sizeof(*d) + d->bInCollection;
}

 
static bool validate_mixer_unit(const void *p,
				const struct usb_desc_validator *v)
{
	const struct uac_mixer_unit_descriptor *d = p;
	size_t len;

	if (d->bLength < sizeof(*d) || !d->bNrInPins)
		return false;
	len = sizeof(*d) + d->bNrInPins;
	 
	switch (v->protocol) {
	case UAC_VERSION_1:
	default:
		len += 2 + 1;  
		 
		len += 1;  
		break;
	case UAC_VERSION_2:
		len += 4 + 1;  
		 
		len += 1 + 1;  
		break;
	case UAC_VERSION_3:
		len += 2;  
		 
		break;
	}
	return d->bLength >= len;
}

 
static bool validate_processing_unit(const void *p,
				     const struct usb_desc_validator *v)
{
	const struct uac_processing_unit_descriptor *d = p;
	const unsigned char *hdr = p;
	size_t len, m;

	if (d->bLength < sizeof(*d))
		return false;
	len = sizeof(*d) + d->bNrInPins;
	if (d->bLength < len)
		return false;
	switch (v->protocol) {
	case UAC_VERSION_1:
	default:
		 
		len += 1 + 2 + 1;
		if (d->bLength < len + 1)  
			return false;
		m = hdr[len];
		len += 1 + m + 1;  
		break;
	case UAC_VERSION_2:
		 
		len += 1 + 4 + 1;
		if (v->type == UAC2_PROCESSING_UNIT_V2)
			len += 2;  
		else
			len += 1;  
		len += 1;  
		break;
	case UAC_VERSION_3:
		 
		len += 2 + 4;
		break;
	}
	if (d->bLength < len)
		return false;

	switch (v->protocol) {
	case UAC_VERSION_1:
	default:
		if (v->type == UAC1_EXTENSION_UNIT)
			return true;  
		switch (le16_to_cpu(d->wProcessType)) {
		case UAC_PROCESS_UP_DOWNMIX:
		case UAC_PROCESS_DOLBY_PROLOGIC:
			if (d->bLength < len + 1)  
				return false;
			m = hdr[len];
			len += 1 + m * 2;  
			break;
		default:
			break;
		}
		break;
	case UAC_VERSION_2:
		if (v->type == UAC2_EXTENSION_UNIT_V2)
			return true;  
		switch (le16_to_cpu(d->wProcessType)) {
		case UAC2_PROCESS_UP_DOWNMIX:
		case UAC2_PROCESS_DOLBY_PROLOCIC:  
			if (d->bLength < len + 1)  
				return false;
			m = hdr[len];
			len += 1 + m * 4;  
			break;
		default:
			break;
		}
		break;
	case UAC_VERSION_3:
		if (v->type == UAC3_EXTENSION_UNIT) {
			len += 2;  
			break;
		}
		switch (le16_to_cpu(d->wProcessType)) {
		case UAC3_PROCESS_UP_DOWNMIX:
			if (d->bLength < len + 1)  
				return false;
			m = hdr[len];
			len += 1 + m * 2;  
			break;
		case UAC3_PROCESS_MULTI_FUNCTION:
			len += 2 + 4;  
			break;
		default:
			break;
		}
		break;
	}
	if (d->bLength < len)
		return false;

	return true;
}

 
static bool validate_selector_unit(const void *p,
				   const struct usb_desc_validator *v)
{
	const struct uac_selector_unit_descriptor *d = p;
	size_t len;

	if (d->bLength < sizeof(*d))
		return false;
	len = sizeof(*d) + d->bNrInPins;
	switch (v->protocol) {
	case UAC_VERSION_1:
	default:
		len += 1;  
		break;
	case UAC_VERSION_2:
		len += 1 + 1;  
		break;
	case UAC_VERSION_3:
		len += 4 + 2;  
		break;
	}
	return d->bLength >= len;
}

static bool validate_uac1_feature_unit(const void *p,
				       const struct usb_desc_validator *v)
{
	const struct uac_feature_unit_descriptor *d = p;

	if (d->bLength < sizeof(*d) || !d->bControlSize)
		return false;
	 
	return d->bLength >= sizeof(*d) + d->bControlSize + 1;
}

static bool validate_uac2_feature_unit(const void *p,
				       const struct usb_desc_validator *v)
{
	const struct uac2_feature_unit_descriptor *d = p;

	if (d->bLength < sizeof(*d))
		return false;
	 
	return d->bLength >= sizeof(*d) + 4 + 1;
}

static bool validate_uac3_feature_unit(const void *p,
				       const struct usb_desc_validator *v)
{
	const struct uac3_feature_unit_descriptor *d = p;

	if (d->bLength < sizeof(*d))
		return false;
	 
	return d->bLength >= sizeof(*d) + 4 + 2;
}

static bool validate_midi_out_jack(const void *p,
				   const struct usb_desc_validator *v)
{
	const struct usb_midi_out_jack_descriptor *d = p;

	return d->bLength >= sizeof(*d) &&
		d->bLength >= sizeof(*d) + d->bNrInputPins * 2;
}

#define FIXED(p, t, s) { .protocol = (p), .type = (t), .size = sizeof(s) }
#define FUNC(p, t, f) { .protocol = (p), .type = (t), .func = (f) }

static const struct usb_desc_validator audio_validators[] = {
	 
	FUNC(UAC_VERSION_1, UAC_HEADER, validate_uac1_header),
	FIXED(UAC_VERSION_1, UAC_INPUT_TERMINAL,
	      struct uac_input_terminal_descriptor),
	FIXED(UAC_VERSION_1, UAC_OUTPUT_TERMINAL,
	      struct uac1_output_terminal_descriptor),
	FUNC(UAC_VERSION_1, UAC_MIXER_UNIT, validate_mixer_unit),
	FUNC(UAC_VERSION_1, UAC_SELECTOR_UNIT, validate_selector_unit),
	FUNC(UAC_VERSION_1, UAC_FEATURE_UNIT, validate_uac1_feature_unit),
	FUNC(UAC_VERSION_1, UAC1_PROCESSING_UNIT, validate_processing_unit),
	FUNC(UAC_VERSION_1, UAC1_EXTENSION_UNIT, validate_processing_unit),

	 
	FIXED(UAC_VERSION_2, UAC_HEADER, struct uac2_ac_header_descriptor),
	FIXED(UAC_VERSION_2, UAC_INPUT_TERMINAL,
	      struct uac2_input_terminal_descriptor),
	FIXED(UAC_VERSION_2, UAC_OUTPUT_TERMINAL,
	      struct uac2_output_terminal_descriptor),
	FUNC(UAC_VERSION_2, UAC_MIXER_UNIT, validate_mixer_unit),
	FUNC(UAC_VERSION_2, UAC_SELECTOR_UNIT, validate_selector_unit),
	FUNC(UAC_VERSION_2, UAC_FEATURE_UNIT, validate_uac2_feature_unit),
	 
	FUNC(UAC_VERSION_2, UAC2_PROCESSING_UNIT_V2, validate_processing_unit),
	FUNC(UAC_VERSION_2, UAC2_EXTENSION_UNIT_V2, validate_processing_unit),
	FIXED(UAC_VERSION_2, UAC2_CLOCK_SOURCE,
	      struct uac_clock_source_descriptor),
	FUNC(UAC_VERSION_2, UAC2_CLOCK_SELECTOR, validate_selector_unit),
	FIXED(UAC_VERSION_2, UAC2_CLOCK_MULTIPLIER,
	      struct uac_clock_multiplier_descriptor),
	 

	 
	FIXED(UAC_VERSION_2, UAC_HEADER, struct uac3_ac_header_descriptor),
	FIXED(UAC_VERSION_3, UAC_INPUT_TERMINAL,
	      struct uac3_input_terminal_descriptor),
	FIXED(UAC_VERSION_3, UAC_OUTPUT_TERMINAL,
	      struct uac3_output_terminal_descriptor),
	 
	FUNC(UAC_VERSION_3, UAC3_MIXER_UNIT, validate_mixer_unit),
	FUNC(UAC_VERSION_3, UAC3_SELECTOR_UNIT, validate_selector_unit),
	FUNC(UAC_VERSION_3, UAC_FEATURE_UNIT, validate_uac3_feature_unit),
	 
	FUNC(UAC_VERSION_3, UAC3_PROCESSING_UNIT, validate_processing_unit),
	FUNC(UAC_VERSION_3, UAC3_EXTENSION_UNIT, validate_processing_unit),
	FIXED(UAC_VERSION_3, UAC3_CLOCK_SOURCE,
	      struct uac3_clock_source_descriptor),
	FUNC(UAC_VERSION_3, UAC3_CLOCK_SELECTOR, validate_selector_unit),
	FIXED(UAC_VERSION_3, UAC3_CLOCK_MULTIPLIER,
	      struct uac3_clock_multiplier_descriptor),
	 
	 
	{ }  
};

static const struct usb_desc_validator midi_validators[] = {
	FIXED(UAC_VERSION_ALL, USB_MS_HEADER,
	      struct usb_ms_header_descriptor),
	FIXED(UAC_VERSION_ALL, USB_MS_MIDI_IN_JACK,
	      struct usb_midi_in_jack_descriptor),
	FUNC(UAC_VERSION_ALL, USB_MS_MIDI_OUT_JACK,
	     validate_midi_out_jack),
	{ }  
};


 
static bool validate_desc(unsigned char *hdr, int protocol,
			  const struct usb_desc_validator *v)
{
	if (hdr[1] != USB_DT_CS_INTERFACE)
		return true;  

	for (; v->type; v++) {
		if (v->type == hdr[2] &&
		    (v->protocol == UAC_VERSION_ALL ||
		     v->protocol == protocol)) {
			if (v->func)
				return v->func(hdr, v);
			 
			return hdr[0] >= v->size;
		}
	}

	return true;  
}

bool snd_usb_validate_audio_desc(void *p, int protocol)
{
	unsigned char *c = p;
	bool valid;

	valid = validate_desc(p, protocol, audio_validators);
	if (!valid && snd_usb_skip_validation) {
		print_hex_dump(KERN_ERR, "USB-audio: buggy audio desc: ",
			       DUMP_PREFIX_NONE, 16, 1, c, c[0], true);
		valid = true;
	}
	return valid;
}

bool snd_usb_validate_midi_desc(void *p)
{
	unsigned char *c = p;
	bool valid;

	valid = validate_desc(p, UAC_VERSION_1, midi_validators);
	if (!valid && snd_usb_skip_validation) {
		print_hex_dump(KERN_ERR, "USB-audio: buggy midi desc: ",
			       DUMP_PREFIX_NONE, 16, 1, c, c[0], true);
		valid = true;
	}
	return valid;
}
