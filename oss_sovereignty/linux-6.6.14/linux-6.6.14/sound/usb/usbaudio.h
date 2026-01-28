#ifndef __USBAUDIO_H
#define __USBAUDIO_H
#define USB_ID(vendor, product) (((unsigned int)(vendor) << 16) | (product))
#define USB_ID_VENDOR(id) ((id) >> 16)
#define USB_ID_PRODUCT(id) ((u16)(id))
struct media_device;
struct media_intf_devnode;
#define MAX_CARD_INTERFACES	16
struct snd_usb_audio {
	int index;
	struct usb_device *dev;
	struct snd_card *card;
	struct usb_interface *intf[MAX_CARD_INTERFACES];
	u32 usb_id;
	uint16_t quirk_type;
	struct mutex mutex;
	unsigned int system_suspend;
	atomic_t active;
	atomic_t shutdown;
	atomic_t usage_count;
	wait_queue_head_t shutdown_wait;
	unsigned int quirk_flags;
	unsigned int need_delayed_register:1;  
	int num_interfaces;
	int last_iface;
	int num_suspended_intf;
	int sample_rate_read_error;
	int badd_profile;		 
	struct list_head pcm_list;	 
	struct list_head ep_list;	 
	struct list_head iface_ref_list;  
	struct list_head clock_ref_list;  
	int pcm_devs;
	unsigned int num_rawmidis;	 
	struct list_head midi_list;	 
	struct list_head midi_v2_list;	 
	struct list_head mixer_list;	 
	int setup;			 
	bool generic_implicit_fb;	 
	bool autoclock;			 
	bool lowlatency;		 
	struct usb_host_interface *ctrl_intf;	 
	struct media_device *media_dev;
	struct media_intf_devnode *ctl_intf_media_devnode;
};
#define USB_AUDIO_IFACE_UNUSED	((void *)-1L)
#define usb_audio_err(chip, fmt, args...) \
	dev_err(&(chip)->dev->dev, fmt, ##args)
#define usb_audio_err_ratelimited(chip, fmt, args...) \
	dev_err_ratelimited(&(chip)->dev->dev, fmt, ##args)
#define usb_audio_warn(chip, fmt, args...) \
	dev_warn(&(chip)->dev->dev, fmt, ##args)
#define usb_audio_info(chip, fmt, args...) \
	dev_info(&(chip)->dev->dev, fmt, ##args)
#define usb_audio_dbg(chip, fmt, args...) \
	dev_dbg(&(chip)->dev->dev, fmt, ##args)
#define QUIRK_NODEV_INTERFACE		-3	 
#define QUIRK_NO_INTERFACE		-2
#define QUIRK_ANY_INTERFACE		-1
enum quirk_type {
	QUIRK_IGNORE_INTERFACE,
	QUIRK_COMPOSITE,
	QUIRK_AUTODETECT,
	QUIRK_MIDI_STANDARD_INTERFACE,
	QUIRK_MIDI_FIXED_ENDPOINT,
	QUIRK_MIDI_YAMAHA,
	QUIRK_MIDI_ROLAND,
	QUIRK_MIDI_MIDIMAN,
	QUIRK_MIDI_NOVATION,
	QUIRK_MIDI_RAW_BYTES,
	QUIRK_MIDI_EMAGIC,
	QUIRK_MIDI_CME,
	QUIRK_MIDI_AKAI,
	QUIRK_MIDI_US122L,
	QUIRK_MIDI_FTDI,
	QUIRK_MIDI_CH345,
	QUIRK_AUDIO_STANDARD_INTERFACE,
	QUIRK_AUDIO_FIXED_ENDPOINT,
	QUIRK_AUDIO_EDIROL_UAXX,
	QUIRK_AUDIO_STANDARD_MIXER,
	QUIRK_TYPE_COUNT
};
struct snd_usb_audio_quirk {
	const char *vendor_name;
	const char *product_name;
	int16_t ifnum;
	uint16_t type;
	const void *data;
};
#define combine_word(s)    ((*(s)) | ((unsigned int)(s)[1] << 8))
#define combine_triple(s)  (combine_word(s) | ((unsigned int)(s)[2] << 16))
#define combine_quad(s)    (combine_triple(s) | ((unsigned int)(s)[3] << 24))
int snd_usb_lock_shutdown(struct snd_usb_audio *chip);
void snd_usb_unlock_shutdown(struct snd_usb_audio *chip);
extern bool snd_usb_use_vmalloc;
extern bool snd_usb_skip_validation;
#define QUIRK_FLAG_GET_SAMPLE_RATE	(1U << 0)
#define QUIRK_FLAG_SHARE_MEDIA_DEVICE	(1U << 1)
#define QUIRK_FLAG_ALIGN_TRANSFER	(1U << 2)
#define QUIRK_FLAG_TX_LENGTH		(1U << 3)
#define QUIRK_FLAG_PLAYBACK_FIRST	(1U << 4)
#define QUIRK_FLAG_SKIP_CLOCK_SELECTOR	(1U << 5)
#define QUIRK_FLAG_IGNORE_CLOCK_SOURCE	(1U << 6)
#define QUIRK_FLAG_ITF_USB_DSD_DAC	(1U << 7)
#define QUIRK_FLAG_CTL_MSG_DELAY	(1U << 8)
#define QUIRK_FLAG_CTL_MSG_DELAY_1M	(1U << 9)
#define QUIRK_FLAG_CTL_MSG_DELAY_5M	(1U << 10)
#define QUIRK_FLAG_IFACE_DELAY		(1U << 11)
#define QUIRK_FLAG_VALIDATE_RATES	(1U << 12)
#define QUIRK_FLAG_DISABLE_AUTOSUSPEND	(1U << 13)
#define QUIRK_FLAG_IGNORE_CTL_ERROR	(1U << 14)
#define QUIRK_FLAG_DSD_RAW		(1U << 15)
#define QUIRK_FLAG_SET_IFACE_FIRST	(1U << 16)
#define QUIRK_FLAG_GENERIC_IMPLICIT_FB	(1U << 17)
#define QUIRK_FLAG_SKIP_IMPLICIT_FB	(1U << 18)
#define QUIRK_FLAG_IFACE_SKIP_CLOSE	(1U << 19)
#define QUIRK_FLAG_FORCE_IFACE_RESET	(1U << 20)
#define QUIRK_FLAG_FIXED_RATE		(1U << 21)
#endif  
