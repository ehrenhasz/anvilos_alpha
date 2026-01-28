#ifndef __USBIP_VHCI_H
#define __USBIP_VHCI_H
#include <linux/device.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/wait.h>
struct vhci_device {
	struct usb_device *udev;
	__u32 devid;
	enum usb_device_speed speed;
	__u32 rhport;
	struct usbip_device ud;
	spinlock_t priv_lock;
	struct list_head priv_tx;
	struct list_head priv_rx;
	struct list_head unlink_tx;
	struct list_head unlink_rx;
	wait_queue_head_t waitq_tx;
};
struct vhci_priv {
	unsigned long seqnum;
	struct list_head list;
	struct vhci_device *vdev;
	struct urb *urb;
};
struct vhci_unlink {
	unsigned long seqnum;
	struct list_head list;
	unsigned long unlink_seqnum;
};
enum hub_speed {
	HUB_SPEED_HIGH = 0,
	HUB_SPEED_SUPER,
};
#ifdef CONFIG_USBIP_VHCI_HC_PORTS
#define VHCI_HC_PORTS CONFIG_USBIP_VHCI_HC_PORTS
#else
#define VHCI_HC_PORTS 8
#endif
#define VHCI_PORTS	(VHCI_HC_PORTS*2)
#ifdef CONFIG_USBIP_VHCI_NR_HCS
#define VHCI_NR_HCS CONFIG_USBIP_VHCI_NR_HCS
#else
#define VHCI_NR_HCS 1
#endif
#define MAX_STATUS_NAME 16
struct vhci {
	spinlock_t lock;
	struct platform_device *pdev;
	struct vhci_hcd *vhci_hcd_hs;
	struct vhci_hcd *vhci_hcd_ss;
};
struct vhci_hcd {
	struct vhci *vhci;
	u32 port_status[VHCI_HC_PORTS];
	unsigned resuming:1;
	unsigned long re_timeout;
	atomic_t seqnum;
	struct vhci_device vdev[VHCI_HC_PORTS];
};
extern int vhci_num_controllers;
extern struct vhci *vhcis;
extern struct attribute_group vhci_attr_group;
void rh_port_connect(struct vhci_device *vdev, enum usb_device_speed speed);
int vhci_init_attr_group(void);
void vhci_finish_attr_group(void);
struct urb *pickup_urb_and_free_priv(struct vhci_device *vdev, __u32 seqnum);
int vhci_rx_loop(void *data);
int vhci_tx_loop(void *data);
static inline __u32 port_to_rhport(__u32 port)
{
	return port % VHCI_HC_PORTS;
}
static inline int port_to_pdev_nr(__u32 port)
{
	return port / VHCI_PORTS;
}
static inline struct vhci_hcd *hcd_to_vhci_hcd(struct usb_hcd *hcd)
{
	return (struct vhci_hcd *) (hcd->hcd_priv);
}
static inline struct device *hcd_dev(struct usb_hcd *hcd)
{
	return (hcd)->self.controller;
}
static inline const char *hcd_name(struct usb_hcd *hcd)
{
	return (hcd)->self.bus_name;
}
static inline struct usb_hcd *vhci_hcd_to_hcd(struct vhci_hcd *vhci_hcd)
{
	return container_of((void *) vhci_hcd, struct usb_hcd, hcd_priv);
}
static inline struct vhci_hcd *vdev_to_vhci_hcd(struct vhci_device *vdev)
{
	return container_of((void *)(vdev - vdev->rhport), struct vhci_hcd, vdev);
}
#endif  
