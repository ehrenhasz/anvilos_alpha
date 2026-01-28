#ifndef __USBIP_STUB_H
#define __USBIP_STUB_H
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/wait.h>
#define STUB_BUSID_OTHER 0
#define STUB_BUSID_REMOV 1
#define STUB_BUSID_ADDED 2
#define STUB_BUSID_ALLOC 3
struct stub_device {
	struct usb_device *udev;
	struct usbip_device ud;
	__u32 devid;
	spinlock_t priv_lock;
	struct list_head priv_init;
	struct list_head priv_tx;
	struct list_head priv_free;
	struct list_head unlink_tx;
	struct list_head unlink_free;
	wait_queue_head_t tx_waitq;
};
struct stub_priv {
	unsigned long seqnum;
	struct list_head list;
	struct stub_device *sdev;
	struct urb **urbs;
	struct scatterlist *sgl;
	int num_urbs;
	int completed_urbs;
	int urb_status;
	int unlinking;
};
struct stub_unlink {
	unsigned long seqnum;
	struct list_head list;
	__u32 status;
};
#define BUSID_SIZE 32
struct bus_id_priv {
	char name[BUSID_SIZE];
	char status;
	int interf_count;
	struct stub_device *sdev;
	struct usb_device *udev;
	char shutdown_busid;
	spinlock_t busid_lock;
};
extern struct kmem_cache *stub_priv_cache;
extern struct usb_device_driver stub_driver;
struct bus_id_priv *get_busid_priv(const char *busid);
void put_busid_priv(struct bus_id_priv *bid);
int del_match_busid(char *busid);
void stub_free_priv_and_urb(struct stub_priv *priv);
void stub_device_cleanup_urbs(struct stub_device *sdev);
int stub_rx_loop(void *data);
void stub_enqueue_ret_unlink(struct stub_device *sdev, __u32 seqnum,
			     __u32 status);
void stub_complete(struct urb *urb);
int stub_tx_loop(void *data);
#endif  
