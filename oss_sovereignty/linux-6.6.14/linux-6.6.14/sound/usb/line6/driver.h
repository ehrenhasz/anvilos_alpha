#ifndef DRIVER_H
#define DRIVER_H
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <sound/core.h>
#include "midi.h"
#define USB_LOW_INTERVALS_PER_SECOND 1000
#define USB_LOW_ISO_BUFFERS 2
#define USB_HIGH_INTERVALS_PER_SECOND 8000
#define USB_HIGH_ISO_BUFFERS 16
#define LINE6_FALLBACK_INTERVAL 10
#define LINE6_FALLBACK_MAXPACKETSIZE 16
#define LINE6_TIMEOUT 1000
#define LINE6_BUFSIZE_LISTEN 64
#define LINE6_MIDI_MESSAGE_MAXLEN 256
#define LINE6_RAW_MESSAGES_MAXCOUNT_ORDER 7
#define LINE6_RAW_MESSAGES_MAXCOUNT (1 << LINE6_RAW_MESSAGES_MAXCOUNT_ORDER)
#if LINE6_BUFSIZE_LISTEN > 65535
#error "Use dynamic fifo instead"
#endif
#define LINE6_PARAM_CHANGE   0xb0
#define LINE6_PROGRAM_CHANGE 0xc0
#define LINE6_SYSEX_BEGIN    0xf0
#define LINE6_SYSEX_END      0xf7
#define LINE6_RESET          0xff
#define LINE6_CHANNEL_HOST   0x00
#define LINE6_CHANNEL_DEVICE 0x02
#define LINE6_CHANNEL_UNKNOWN 5	 
#define LINE6_CHANNEL_MASK 0x0f
extern const unsigned char line6_midi_id[3];
#define SYSEX_DATA_OFS (sizeof(line6_midi_id) + 3)
#define SYSEX_EXTRA_SIZE (sizeof(line6_midi_id) + 4)
struct line6_properties {
	const char *id;
	const char *name;
	int capabilities;
	int altsetting;
	unsigned int ctrl_if;
	unsigned int ep_ctrl_r;
	unsigned int ep_ctrl_w;
	unsigned int ep_audio_r;
	unsigned int ep_audio_w;
};
enum {
	LINE6_CAP_CONTROL =	1 << 0,
	LINE6_CAP_PCM =		1 << 1,
	LINE6_CAP_HWMON =	1 << 2,
	LINE6_CAP_IN_NEEDS_OUT = 1 << 3,
	LINE6_CAP_CONTROL_MIDI = 1 << 4,
	LINE6_CAP_CONTROL_INFO = 1 << 5,
	LINE6_CAP_HWMON_CTL =	1 << 6,
};
struct usb_line6 {
	struct usb_device *usbdev;
	const struct line6_properties *properties;
	int interval;
	int intervals_per_second;
	int iso_buffers;
	int max_packet_size;
	struct device *ifcdev;
	struct snd_card *card;
	struct snd_line6_pcm *line6pcm;
	struct snd_line6_midi *line6midi;
	struct urb *urb_listen;
	unsigned char *buffer_listen;
	unsigned char *buffer_message;
	int message_length;
	struct {
		struct mutex read_lock;
		wait_queue_head_t wait_queue;
		unsigned int active:1;
		unsigned int nonblock:1;
		STRUCT_KFIFO_REC_2(LINE6_BUFSIZE_LISTEN * LINE6_RAW_MESSAGES_MAXCOUNT)
			fifo;
	} messages;
	struct delayed_work startup_work;
	void (*process_message)(struct usb_line6 *);
	void (*disconnect)(struct usb_line6 *line6);
	void (*startup)(struct usb_line6 *line6);
};
extern char *line6_alloc_sysex_buffer(struct usb_line6 *line6, int code1,
				      int code2, int size);
extern int line6_read_data(struct usb_line6 *line6, unsigned address,
			   void *data, unsigned datalen);
extern int line6_read_serial_number(struct usb_line6 *line6,
				    u32 *serial_number);
extern int line6_send_raw_message(struct usb_line6 *line6,
					const char *buffer, int size);
extern int line6_send_raw_message_async(struct usb_line6 *line6,
					const char *buffer, int size);
extern int line6_send_sysex_message(struct usb_line6 *line6,
				    const char *buffer, int size);
extern int line6_version_request_async(struct usb_line6 *line6);
extern int line6_write_data(struct usb_line6 *line6, unsigned address,
			    void *data, unsigned datalen);
int line6_probe(struct usb_interface *interface,
		const struct usb_device_id *id,
		const char *driver_name,
		const struct line6_properties *properties,
		int (*private_init)(struct usb_line6 *, const struct usb_device_id *id),
		size_t data_size);
void line6_disconnect(struct usb_interface *interface);
#ifdef CONFIG_PM
int line6_suspend(struct usb_interface *interface, pm_message_t message);
int line6_resume(struct usb_interface *interface);
#endif
#endif
