


#ifndef	_USBATM_H_
#define	_USBATM_H_

#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/completion.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/stringify.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/ratelimit.h>



#define usb_err(instance, format, arg...)	\
	dev_err(&(instance)->usb_intf->dev , format , ## arg)
#define usb_info(instance, format, arg...)	\
	dev_info(&(instance)->usb_intf->dev , format , ## arg)
#define usb_warn(instance, format, arg...)	\
	dev_warn(&(instance)->usb_intf->dev , format , ## arg)
#define usb_dbg(instance, format, arg...)	\
	dev_dbg(&(instance)->usb_intf->dev , format , ## arg)


#define atm_printk(level, instance, format, arg...)	\
	printk(level "ATM dev %d: " format ,		\
	(instance)->atm_dev->number , ## arg)

#define atm_err(instance, format, arg...)	\
	atm_printk(KERN_ERR, instance , format , ## arg)
#define atm_info(instance, format, arg...)	\
	atm_printk(KERN_INFO, instance , format , ## arg)
#define atm_warn(instance, format, arg...)	\
	atm_printk(KERN_WARNING, instance , format , ## arg)
#define atm_dbg(instance, format, ...)					\
	pr_debug("ATM dev %d: " format,					\
		 (instance)->atm_dev->number, ##__VA_ARGS__)
#define atm_rldbg(instance, format, ...)				\
	pr_debug_ratelimited("ATM dev %d: " format,			\
			     (instance)->atm_dev->number, ##__VA_ARGS__)



#define UDSL_SKIP_HEAVY_INIT	(1<<0)
#define UDSL_USE_ISOC		(1<<1)
#define UDSL_IGNORE_EILSEQ	(1<<2)




struct usbatm_data;



struct usbatm_driver {
	const char *driver_name;

	
	int (*bind) (struct usbatm_data *, struct usb_interface *,
		     const struct usb_device_id *id);

	
	int (*heavy_init) (struct usbatm_data *, struct usb_interface *);

	
	void (*unbind) (struct usbatm_data *, struct usb_interface *);

	
	int (*atm_start) (struct usbatm_data *, struct atm_dev *);

	
	void (*atm_stop) (struct usbatm_data *, struct atm_dev *);

	int bulk_in;	
	int isoc_in;	
	int bulk_out;	

	unsigned rx_padding;
	unsigned tx_padding;
};

extern int usbatm_usb_probe(struct usb_interface *intf, const struct usb_device_id *id,
		struct usbatm_driver *driver);
extern void usbatm_usb_disconnect(struct usb_interface *intf);


struct usbatm_channel {
	int endpoint;			
	unsigned int stride;		
	unsigned int buf_size;		
	unsigned int packet_size;	
	spinlock_t lock;
	struct list_head list;
	struct tasklet_struct tasklet;
	struct timer_list delay;
	struct usbatm_data *usbatm;
};



struct usbatm_data {
	

	
	struct usbatm_driver *driver;
	void *driver_data;
	char driver_name[16];
	unsigned int flags; 

	
	struct usb_device *usb_dev;
	struct usb_interface *usb_intf;
	char description[64];

	
	struct atm_dev *atm_dev;

	

	struct kref refcount;
	struct mutex serialize;
	int disconnected;

	
	struct task_struct *thread;
	struct completion thread_started;
	struct completion thread_exited;

	
	struct list_head vcc_list;

	struct usbatm_channel rx_channel;
	struct usbatm_channel tx_channel;

	struct sk_buff_head sndqueue;
	struct sk_buff *current_skb;	

	struct usbatm_vcc_data *cached_vcc;
	int cached_vci;
	short cached_vpi;

	unsigned char *cell_buf;	
	unsigned int buf_usage;

	struct urb *urbs[];
};

static inline void *to_usbatm_driver_data(struct usb_interface *intf)
{
	struct usbatm_data *usbatm_instance;

	if (intf == NULL)
		return NULL;

	usbatm_instance = usb_get_intfdata(intf);

	if (usbatm_instance == NULL) 
		return NULL;

	return usbatm_instance->driver_data; 
}

#endif	
