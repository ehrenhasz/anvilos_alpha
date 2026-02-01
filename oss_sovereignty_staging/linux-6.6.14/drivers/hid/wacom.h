 
 

#ifndef WACOM_H
#define WACOM_H

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/hid.h>
#include <linux/kfifo.h>
#include <linux/leds.h>
#include <linux/usb/input.h>
#include <linux/power_supply.h>
#include <linux/timer.h>
#include <asm/unaligned.h>

 
#define DRIVER_VERSION "v2.00"
#define DRIVER_AUTHOR "Vojtech Pavlik <vojtech@ucw.cz>"
#define DRIVER_DESC "USB Wacom tablet driver"

#define USB_VENDOR_ID_WACOM	0x056a
#define USB_VENDOR_ID_LENOVO	0x17ef

enum wacom_worker {
	WACOM_WORKER_WIRELESS,
	WACOM_WORKER_BATTERY,
	WACOM_WORKER_REMOTE,
	WACOM_WORKER_MODE_CHANGE,
};

struct wacom;

struct wacom_led {
	struct led_classdev cdev;
	struct led_trigger trigger;
	struct wacom *wacom;
	unsigned int group;
	unsigned int id;
	u8 llv;
	u8 hlv;
	bool held;
};

struct wacom_group_leds {
	u8 select;  
	struct wacom_led *leds;
	unsigned int count;
	struct device *dev;
};

struct wacom_battery {
	struct wacom *wacom;
	struct power_supply_desc bat_desc;
	struct power_supply *battery;
	char bat_name[WACOM_NAME_MAX];
	int bat_status;
	int battery_capacity;
	int bat_charging;
	int bat_connected;
	int ps_connected;
};

struct wacom_remote {
	spinlock_t remote_lock;
	struct kfifo remote_fifo;
	struct kobject *remote_dir;
	struct {
		struct attribute_group group;
		u32 serial;
		struct input_dev *input;
		bool registered;
		struct wacom_battery battery;
		ktime_t active_time;
	} remotes[WACOM_MAX_REMOTES];
};

struct wacom {
	struct usb_device *usbdev;
	struct usb_interface *intf;
	struct wacom_wac wacom_wac;
	struct hid_device *hdev;
	struct mutex lock;
	struct work_struct wireless_work;
	struct work_struct battery_work;
	struct work_struct remote_work;
	struct delayed_work init_work;
	struct wacom_remote *remote;
	struct work_struct mode_change_work;
	struct timer_list idleprox_timer;
	bool generic_has_leds;
	struct wacom_leds {
		struct wacom_group_leds *groups;
		unsigned int count;
		u8 llv;        
		u8 hlv;        
		u8 img_lum;    
		u8 max_llv;    
		u8 max_hlv;    
	} led;
	struct wacom_battery battery;
	bool resources;
};

static inline void wacom_schedule_work(struct wacom_wac *wacom_wac,
				       enum wacom_worker which)
{
	struct wacom *wacom = container_of(wacom_wac, struct wacom, wacom_wac);

	switch (which) {
	case WACOM_WORKER_WIRELESS:
		schedule_work(&wacom->wireless_work);
		break;
	case WACOM_WORKER_BATTERY:
		schedule_work(&wacom->battery_work);
		break;
	case WACOM_WORKER_REMOTE:
		schedule_work(&wacom->remote_work);
		break;
	case WACOM_WORKER_MODE_CHANGE:
		schedule_work(&wacom->mode_change_work);
		break;
	}
}

 
static inline __u32 wacom_s32tou(s32 value, __u8 n)
{
	switch (n) {
	case 8:  return ((__u8)value);
	case 16: return ((__u16)value);
	case 32: return ((__u32)value);
	}
	return value & (1 << (n - 1)) ? value & (~(~0U << n)) : value;
}

extern const struct hid_device_id wacom_ids[];

void wacom_wac_irq(struct wacom_wac *wacom_wac, size_t len);
void wacom_setup_device_quirks(struct wacom *wacom);
int wacom_setup_pen_input_capabilities(struct input_dev *input_dev,
				   struct wacom_wac *wacom_wac);
int wacom_setup_touch_input_capabilities(struct input_dev *input_dev,
				   struct wacom_wac *wacom_wac);
int wacom_setup_pad_input_capabilities(struct input_dev *input_dev,
				       struct wacom_wac *wacom_wac);
void wacom_wac_usage_mapping(struct hid_device *hdev,
		struct hid_field *field, struct hid_usage *usage);
void wacom_wac_event(struct hid_device *hdev, struct hid_field *field,
		struct hid_usage *usage, __s32 value);
void wacom_wac_report(struct hid_device *hdev, struct hid_report *report);
void wacom_battery_work(struct work_struct *work);
enum led_brightness wacom_leds_brightness_get(struct wacom_led *led);
struct wacom_led *wacom_led_find(struct wacom *wacom, unsigned int group,
				 unsigned int id);
struct wacom_led *wacom_led_next(struct wacom *wacom, struct wacom_led *cur);
int wacom_equivalent_usage(int usage);
int wacom_initialize_leds(struct wacom *wacom);
void wacom_idleprox_timeout(struct timer_list *list);
#endif
