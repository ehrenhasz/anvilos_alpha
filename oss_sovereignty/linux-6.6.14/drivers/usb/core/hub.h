 
 

#include <linux/usb.h>
#include <linux/usb/ch11.h>
#include <linux/usb/hcd.h>
#include "usb.h"

struct usb_hub {
	struct device		*intfdev;	 
	struct usb_device	*hdev;
	struct kref		kref;
	struct urb		*urb;		 

	 
	u8			(*buffer)[8];
	union {
		struct usb_hub_status	hub;
		struct usb_port_status	port;
	}			*status;	 
	struct mutex		status_mutex;	 

	int			error;		 
	int			nerrors;	 

	unsigned long		event_bits[1];	 
	unsigned long		change_bits[1];	 
	unsigned long		removed_bits[1];  
	unsigned long		wakeup_bits[1];	 
	unsigned long		power_bits[1];  
	unsigned long		child_usage_bits[1];  
	unsigned long		warm_reset_bits[1];  
#if USB_MAXCHILDREN > 31  
#error event_bits[] is too short!
#endif

	struct usb_hub_descriptor *descriptor;	 
	struct usb_tt		tt;		 

	unsigned		mA_per_port;	 
#ifdef	CONFIG_PM
	unsigned		wakeup_enabled_descendants;
#endif

	unsigned		limited_power:1;
	unsigned		quiescing:1;
	unsigned		disconnected:1;
	unsigned		in_reset:1;
	unsigned		quirk_disable_autosuspend:1;

	unsigned		quirk_check_port_auto_suspend:1;

	unsigned		has_indicators:1;
	u8			indicator[USB_MAXCHILDREN];
	struct delayed_work	leds;
	struct delayed_work	init_work;
	struct work_struct      events;
	spinlock_t		irq_urb_lock;
	struct timer_list	irq_urb_retry;
	struct usb_port		**ports;
	struct list_head        onboard_hub_devs;
};

 
struct usb_port {
	struct usb_device *child;
	struct device dev;
	struct usb_dev_state *port_owner;
	struct usb_port *peer;
	struct dev_pm_qos_request *req;
	enum usb_port_connect_type connect_type;
	enum usb_device_state state;
	struct kernfs_node *state_kn;
	usb_port_location_t location;
	struct mutex status_lock;
	u32 over_current_count;
	u8 portnum;
	u32 quirks;
	unsigned int early_stop:1;
	unsigned int ignore_event:1;
	unsigned int is_superspeed:1;
	unsigned int usb3_lpm_u1_permit:1;
	unsigned int usb3_lpm_u2_permit:1;
};

#define to_usb_port(_dev) \
	container_of(_dev, struct usb_port, dev)

extern int usb_hub_create_port_device(struct usb_hub *hub,
		int port1);
extern void usb_hub_remove_port_device(struct usb_hub *hub,
		int port1);
extern int usb_hub_set_port_power(struct usb_device *hdev, struct usb_hub *hub,
		int port1, bool set);
extern struct usb_hub *usb_hub_to_struct_hub(struct usb_device *hdev);
extern int hub_port_debounce(struct usb_hub *hub, int port1,
		bool must_be_connected);
extern int usb_clear_port_feature(struct usb_device *hdev,
		int port1, int feature);
extern int usb_hub_port_status(struct usb_hub *hub, int port1,
		u16 *status, u16 *change);
extern int usb_port_is_power_on(struct usb_hub *hub, unsigned int portstatus);

static inline bool hub_is_port_power_switchable(struct usb_hub *hub)
{
	__le16 hcs;

	if (!hub)
		return false;
	hcs = hub->descriptor->wHubCharacteristics;
	return (le16_to_cpu(hcs) & HUB_CHAR_LPSM) < HUB_CHAR_NO_LPSM;
}

static inline int hub_is_superspeed(struct usb_device *hdev)
{
	return hdev->descriptor.bDeviceProtocol == USB_HUB_PR_SS;
}

static inline int hub_is_superspeedplus(struct usb_device *hdev)
{
	return (hdev->descriptor.bDeviceProtocol == USB_HUB_PR_SS &&
		le16_to_cpu(hdev->descriptor.bcdUSB) >= 0x0310 &&
		hdev->bos && hdev->bos->ssp_cap);
}

static inline unsigned hub_power_on_good_delay(struct usb_hub *hub)
{
	unsigned delay = hub->descriptor->bPwrOn2PwrGood * 2;

	if (!hub->hdev->parent)	 
		return delay;
	else  
		return max(delay, 100U);
}

static inline int hub_port_debounce_be_connected(struct usb_hub *hub,
		int port1)
{
	return hub_port_debounce(hub, port1, true);
}

static inline int hub_port_debounce_be_stable(struct usb_hub *hub,
		int port1)
{
	return hub_port_debounce(hub, port1, false);
}
