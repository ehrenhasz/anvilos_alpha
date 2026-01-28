#ifndef __VBOXGUEST_CORE_H__
#define __VBOXGUEST_CORE_H__
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/vboxguest.h>
#include "vmmdev.h"
#define VBG_IOCTL_VMMDEV_REQUEST_BIG_ALT _IOC(_IOC_READ | _IOC_WRITE, 'V', 3, 0)
#define VBG_IOCTL_LOG_ALT(s)             _IOC(_IOC_READ | _IOC_WRITE, 'V', 9, s)
struct vbg_session;
struct vbg_mem_balloon {
	struct work_struct work;
	struct vmmdev_memballoon_info *get_req;
	struct vmmdev_memballoon_change *change_req;
	u32 chunks;
	u32 max_chunks;
	struct page ***pages;
};
struct vbg_bit_usage_tracker {
	u32 per_bit_usage[32];
	u32 mask;
};
struct vbg_dev {
	struct device *dev;
	u16 io_port;
	struct vmmdev_memory *mmio;
	char host_version[64];
	unsigned int host_features;
	struct page *guest_mappings_dummy_page;
	void *guest_mappings;
	spinlock_t event_spinlock;
	struct vmmdev_events *ack_events_req;
	wait_queue_head_t event_wq;
	u32 pending_events;
	wait_queue_head_t hgcm_wq;
	struct vmmdev_hgcm_cancel2 *cancel_req;
	struct mutex cancel_req_mutex;
	struct vmmdev_mouse_status *mouse_status_req;
	struct input_dev *input;
	struct vbg_mem_balloon mem_balloon;
	struct mutex session_mutex;
	u32 fixed_events;
	struct vbg_bit_usage_tracker event_filter_tracker;
	u32 event_filter_host;
	u32 acquire_mode_guest_caps;
	u32 acquired_guest_caps;
	struct vbg_bit_usage_tracker set_guest_caps_tracker;
	u32 guest_caps_host;
	struct timer_list heartbeat_timer;
	int heartbeat_interval_ms;
	struct vmmdev_request_header *guest_heartbeat_req;
	struct miscdevice misc_device;
	struct miscdevice misc_device_user;
};
struct vbg_session {
	struct vbg_dev *gdev;
	u32 hgcm_client_ids[64];
	u32 event_filter;
	u32 acquired_guest_caps;
	u32 set_guest_caps;
	u32 requestor;
	bool cancel_waiters;
};
int  vbg_core_init(struct vbg_dev *gdev, u32 fixed_events);
void vbg_core_exit(struct vbg_dev *gdev);
struct vbg_session *vbg_core_open_session(struct vbg_dev *gdev, u32 requestor);
void vbg_core_close_session(struct vbg_session *session);
int  vbg_core_ioctl(struct vbg_session *session, unsigned int req, void *data);
int  vbg_core_set_mouse_status(struct vbg_dev *gdev, u32 features);
irqreturn_t vbg_core_isr(int irq, void *dev_id);
void vbg_linux_mouse_event(struct vbg_dev *gdev);
void *vbg_req_alloc(size_t len, enum vmmdev_request_type req_type,
		    u32 requestor);
void vbg_req_free(void *req, size_t len);
int vbg_req_perform(struct vbg_dev *gdev, void *req);
int vbg_hgcm_call32(
	struct vbg_dev *gdev, u32 requestor, u32 client_id, u32 function,
	u32 timeout_ms, struct vmmdev_hgcm_function_parameter32 *parm32,
	u32 parm_count, int *vbox_status);
#endif
