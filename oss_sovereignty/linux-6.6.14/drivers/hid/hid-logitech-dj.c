
 



#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/usb.h>  
#include <asm/unaligned.h>
#include "hid-ids.h"

#define DJ_MAX_PAIRED_DEVICES			7
#define DJ_MAX_NUMBER_NOTIFS			8
#define DJ_RECEIVER_INDEX			0
#define DJ_DEVICE_INDEX_MIN			1
#define DJ_DEVICE_INDEX_MAX			7

#define DJREPORT_SHORT_LENGTH			15
#define DJREPORT_LONG_LENGTH			32

#define REPORT_ID_DJ_SHORT			0x20
#define REPORT_ID_DJ_LONG			0x21

#define REPORT_ID_HIDPP_SHORT			0x10
#define REPORT_ID_HIDPP_LONG			0x11
#define REPORT_ID_HIDPP_VERY_LONG		0x12

#define HIDPP_REPORT_SHORT_LENGTH		7
#define HIDPP_REPORT_LONG_LENGTH		20

#define HIDPP_RECEIVER_INDEX			0xff

#define REPORT_TYPE_RFREPORT_FIRST		0x01
#define REPORT_TYPE_RFREPORT_LAST		0x1F

 
#define REPORT_TYPE_CMD_SWITCH			0x80
#define CMD_SWITCH_PARAM_DEVBITFIELD		0x00
#define CMD_SWITCH_PARAM_TIMEOUT_SECONDS	0x01
#define TIMEOUT_NO_KEEPALIVE			0x00

 
#define REPORT_TYPE_CMD_GET_PAIRED_DEVICES	0x81

 
#define REPORT_TYPE_NOTIF_DEVICE_PAIRED		0x41
#define SPFUNCTION_MORE_NOTIF_EXPECTED		0x01
#define SPFUNCTION_DEVICE_LIST_EMPTY		0x02
#define DEVICE_PAIRED_PARAM_SPFUNCTION		0x00
#define DEVICE_PAIRED_PARAM_EQUAD_ID_LSB	0x01
#define DEVICE_PAIRED_PARAM_EQUAD_ID_MSB	0x02
#define DEVICE_PAIRED_RF_REPORT_TYPE		0x03

 
#define REPORT_TYPE_NOTIF_DEVICE_UNPAIRED	0x40

 
#define REPORT_TYPE_NOTIF_CONNECTION_STATUS	0x42
#define CONNECTION_STATUS_PARAM_STATUS		0x00
#define STATUS_LINKLOSS				0x01

 
#define REPORT_TYPE_NOTIF_ERROR			0x7F
#define NOTIF_ERROR_PARAM_ETYPE			0x00
#define ETYPE_KEEPALIVE_TIMEOUT			0x01

 
#define REPORT_TYPE_KEYBOARD			0x01
#define REPORT_TYPE_MOUSE			0x02
#define REPORT_TYPE_CONSUMER_CONTROL		0x03
#define REPORT_TYPE_SYSTEM_CONTROL		0x04
#define REPORT_TYPE_MEDIA_CENTER		0x08
#define REPORT_TYPE_LEDS			0x0E

 
#define STD_KEYBOARD				BIT(1)
#define STD_MOUSE				BIT(2)
#define MULTIMEDIA				BIT(3)
#define POWER_KEYS				BIT(4)
#define KBD_MOUSE				BIT(5)
#define MEDIA_CENTER				BIT(8)
#define KBD_LEDS				BIT(14)
 
#define HIDPP					BIT_ULL(63)

 
#define REPORT_TYPE_NOTIF_DEVICE_CONNECTED	0x41
#define HIDPP_PARAM_PROTO_TYPE			0x00
#define HIDPP_PARAM_DEVICE_INFO			0x01
#define HIDPP_PARAM_EQUAD_LSB			0x02
#define HIDPP_PARAM_EQUAD_MSB			0x03
#define HIDPP_PARAM_27MHZ_DEVID			0x03
#define HIDPP_DEVICE_TYPE_MASK			GENMASK(3, 0)
#define HIDPP_LINK_STATUS_MASK			BIT(6)
#define HIDPP_MANUFACTURER_MASK			BIT(7)
#define HIDPP_27MHZ_SECURE_MASK			BIT(7)

#define HIDPP_DEVICE_TYPE_KEYBOARD		1
#define HIDPP_DEVICE_TYPE_MOUSE			2

#define HIDPP_SET_REGISTER			0x80
#define HIDPP_GET_LONG_REGISTER			0x83
#define HIDPP_REG_CONNECTION_STATE		0x02
#define HIDPP_REG_PAIRING_INFORMATION		0xB5
#define HIDPP_PAIRING_INFORMATION		0x20
#define HIDPP_FAKE_DEVICE_ARRIVAL		0x02

enum recvr_type {
	recvr_type_dj,
	recvr_type_hidpp,
	recvr_type_gaming_hidpp,
	recvr_type_mouse_only,
	recvr_type_27mhz,
	recvr_type_bluetooth,
	recvr_type_dinovo,
};

struct dj_report {
	u8 report_id;
	u8 device_index;
	u8 report_type;
	u8 report_params[DJREPORT_SHORT_LENGTH - 3];
};

struct hidpp_event {
	u8 report_id;
	u8 device_index;
	u8 sub_id;
	u8 params[HIDPP_REPORT_LONG_LENGTH - 3U];
} __packed;

struct dj_receiver_dev {
	struct hid_device *mouse;
	struct hid_device *keyboard;
	struct hid_device *hidpp;
	struct dj_device *paired_dj_devices[DJ_MAX_PAIRED_DEVICES +
					    DJ_DEVICE_INDEX_MIN];
	struct list_head list;
	struct kref kref;
	struct work_struct work;
	struct kfifo notif_fifo;
	unsigned long last_query;  
	bool ready;
	enum recvr_type type;
	unsigned int unnumbered_application;
	spinlock_t lock;
};

struct dj_device {
	struct hid_device *hdev;
	struct dj_receiver_dev *dj_receiver_dev;
	u64 reports_supported;
	u8 device_index;
};

#define WORKITEM_TYPE_EMPTY	0
#define WORKITEM_TYPE_PAIRED	1
#define WORKITEM_TYPE_UNPAIRED	2
#define WORKITEM_TYPE_UNKNOWN	255

struct dj_workitem {
	u8 type;		 
	u8 device_index;
	u8 device_type;
	u8 quad_id_msb;
	u8 quad_id_lsb;
	u64 reports_supported;
};

 
static const char kbd_descriptor[] = {
	0x05, 0x01,		 
	0x09, 0x06,		 
	0xA1, 0x01,		 
	0x85, 0x01,		 
	0x95, 0x08,		 
	0x75, 0x01,		 
	0x15, 0x00,		 
	0x25, 0x01,		 
	0x05, 0x07,		 
	0x19, 0xE0,		 
	0x29, 0xE7,		 
	0x81, 0x02,		 
	0x95, 0x06,		 
	0x75, 0x08,		 
	0x15, 0x00,		 
	0x26, 0xFF, 0x00,	 
	0x05, 0x07,		 
	0x19, 0x00,		 
	0x2A, 0xFF, 0x00,	 
	0x81, 0x00,		 
	0x85, 0x0e,		 
	0x05, 0x08,		 
	0x95, 0x05,		 
	0x75, 0x01,		 
	0x15, 0x00,		 
	0x25, 0x01,		 
	0x19, 0x01,		 
	0x29, 0x05,		 
	0x91, 0x02,		 
	0x95, 0x01,		 
	0x75, 0x03,		 
	0x91, 0x01,		 
	0xC0
};

 
static const char mse_descriptor[] = {
	0x05, 0x01,		 
	0x09, 0x02,		 
	0xA1, 0x01,		 
	0x85, 0x02,		 
	0x09, 0x01,		 
	0xA1, 0x00,		 
	0x05, 0x09,		 
	0x19, 0x01,		 
	0x29, 0x10,		 
	0x15, 0x00,		 
	0x25, 0x01,		 
	0x95, 0x10,		 
	0x75, 0x01,		 
	0x81, 0x02,		 
	0x05, 0x01,		 
	0x16, 0x01, 0xF8,	 
	0x26, 0xFF, 0x07,	 
	0x75, 0x0C,		 
	0x95, 0x02,		 
	0x09, 0x30,		 
	0x09, 0x31,		 
	0x81, 0x06,		 
	0x15, 0x81,		 
	0x25, 0x7F,		 
	0x75, 0x08,		 
	0x95, 0x01,		 
	0x09, 0x38,		 
	0x81, 0x06,		 
	0x05, 0x0C,		 
	0x0A, 0x38, 0x02,	 
	0x95, 0x01,		 
	0x81, 0x06,		 
	0xC0,			 
	0xC0,			 
};

 
static const char mse_27mhz_descriptor[] = {
	0x05, 0x01,		 
	0x09, 0x02,		 
	0xA1, 0x01,		 
	0x85, 0x02,		 
	0x09, 0x01,		 
	0xA1, 0x00,		 
	0x05, 0x09,		 
	0x19, 0x01,		 
	0x29, 0x08,		 
	0x15, 0x00,		 
	0x25, 0x01,		 
	0x95, 0x08,		 
	0x75, 0x01,		 
	0x81, 0x02,		 
	0x05, 0x01,		 
	0x16, 0x01, 0xF8,	 
	0x26, 0xFF, 0x07,	 
	0x75, 0x0C,		 
	0x95, 0x02,		 
	0x09, 0x30,		 
	0x09, 0x31,		 
	0x81, 0x06,		 
	0x15, 0x81,		 
	0x25, 0x7F,		 
	0x75, 0x08,		 
	0x95, 0x01,		 
	0x09, 0x38,		 
	0x81, 0x06,		 
	0x05, 0x0C,		 
	0x0A, 0x38, 0x02,	 
	0x95, 0x01,		 
	0x81, 0x06,		 
	0xC0,			 
	0xC0,			 
};

 
static const char mse_bluetooth_descriptor[] = {
	0x05, 0x01,		 
	0x09, 0x02,		 
	0xA1, 0x01,		 
	0x85, 0x02,		 
	0x09, 0x01,		 
	0xA1, 0x00,		 
	0x05, 0x09,		 
	0x19, 0x01,		 
	0x29, 0x08,		 
	0x15, 0x00,		 
	0x25, 0x01,		 
	0x95, 0x08,		 
	0x75, 0x01,		 
	0x81, 0x02,		 
	0x05, 0x01,		 
	0x16, 0x01, 0xF8,	 
	0x26, 0xFF, 0x07,	 
	0x75, 0x0C,		 
	0x95, 0x02,		 
	0x09, 0x30,		 
	0x09, 0x31,		 
	0x81, 0x06,		 
	0x15, 0x81,		 
	0x25, 0x7F,		 
	0x75, 0x08,		 
	0x95, 0x01,		 
	0x09, 0x38,		 
	0x81, 0x06,		 
	0x05, 0x0C,		 
	0x0A, 0x38, 0x02,	 
	0x15, 0xF9,		 
	0x25, 0x07,		 
	0x75, 0x04,		 
	0x95, 0x01,		 
	0x81, 0x06,		 
	0x05, 0x09,		 
	0x19, 0x09,		 
	0x29, 0x0C,		 
	0x15, 0x00,		 
	0x25, 0x01,		 
	0x75, 0x01,		 
	0x95, 0x04,		 
	0x81, 0x02,		 
	0xC0,			 
	0xC0,			 
};

 
static const char mse5_bluetooth_descriptor[] = {
	0x05, 0x01,		 
	0x09, 0x02,		 
	0xa1, 0x01,		 
	0x85, 0x05,		 
	0x09, 0x01,		 
	0xa1, 0x00,		 
	0x05, 0x09,		 
	0x19, 0x01,		 
	0x29, 0x08,		 
	0x15, 0x00,		 
	0x25, 0x01,		 
	0x95, 0x08,		 
	0x75, 0x01,		 
	0x81, 0x02,		 
	0x05, 0x01,		 
	0x16, 0x01, 0xf8,	 
	0x26, 0xff, 0x07,	 
	0x75, 0x0c,		 
	0x95, 0x02,		 
	0x09, 0x30,		 
	0x09, 0x31,		 
	0x81, 0x06,		 
	0x15, 0x81,		 
	0x25, 0x7f,		 
	0x75, 0x08,		 
	0x95, 0x01,		 
	0x09, 0x38,		 
	0x81, 0x06,		 
	0x05, 0x0c,		 
	0x0a, 0x38, 0x02,	 
	0x15, 0x81,		 
	0x25, 0x7f,		 
	0x75, 0x08,		 
	0x95, 0x01,		 
	0x81, 0x06,		 
	0xc0,			 
	0xc0,			 
};

 
static const char mse_high_res_descriptor[] = {
	0x05, 0x01,		 
	0x09, 0x02,		 
	0xA1, 0x01,		 
	0x85, 0x02,		 
	0x09, 0x01,		 
	0xA1, 0x00,		 
	0x05, 0x09,		 
	0x19, 0x01,		 
	0x29, 0x10,		 
	0x15, 0x00,		 
	0x25, 0x01,		 
	0x95, 0x10,		 
	0x75, 0x01,		 
	0x81, 0x02,		 
	0x05, 0x01,		 
	0x16, 0x01, 0x80,	 
	0x26, 0xFF, 0x7F,	 
	0x75, 0x10,		 
	0x95, 0x02,		 
	0x09, 0x30,		 
	0x09, 0x31,		 
	0x81, 0x06,		 
	0x15, 0x81,		 
	0x25, 0x7F,		 
	0x75, 0x08,		 
	0x95, 0x01,		 
	0x09, 0x38,		 
	0x81, 0x06,		 
	0x05, 0x0C,		 
	0x0A, 0x38, 0x02,	 
	0x95, 0x01,		 
	0x81, 0x06,		 
	0xC0,			 
	0xC0,			 
};

 
static const char consumer_descriptor[] = {
	0x05, 0x0C,		 
	0x09, 0x01,		 
	0xA1, 0x01,		 
	0x85, 0x03,		 
	0x75, 0x10,		 
	0x95, 0x02,		 
	0x15, 0x01,		 
	0x26, 0xFF, 0x02,	 
	0x19, 0x01,		 
	0x2A, 0xFF, 0x02,	 
	0x81, 0x00,		 
	0xC0,			 
};				 

 
static const char syscontrol_descriptor[] = {
	0x05, 0x01,		 
	0x09, 0x80,		 
	0xA1, 0x01,		 
	0x85, 0x04,		 
	0x75, 0x02,		 
	0x95, 0x01,		 
	0x15, 0x01,		 
	0x25, 0x03,		 
	0x09, 0x82,		 
	0x09, 0x81,		 
	0x09, 0x83,		 
	0x81, 0x60,		 
	0x75, 0x06,		 
	0x81, 0x03,		 
	0xC0,			 
};

 
static const char media_descriptor[] = {
	0x06, 0xbc, 0xff,	 
	0x09, 0x88,		 
	0xa1, 0x01,		 
	0x85, 0x08,		 
	0x19, 0x01,		 
	0x29, 0xff,		 
	0x15, 0x01,		 
	0x26, 0xff, 0x00,	 
	0x75, 0x08,		 
	0x95, 0x01,		 
	0x81, 0x00,		 
	0xc0,			 
};				 

 
static const char hidpp_descriptor[] = {
	0x06, 0x00, 0xff,	 
	0x09, 0x01,		 
	0xa1, 0x01,		 
	0x85, 0x10,		 
	0x75, 0x08,		 
	0x95, 0x06,		 
	0x15, 0x00,		 
	0x26, 0xff, 0x00,	 
	0x09, 0x01,		 
	0x81, 0x00,		 
	0x09, 0x01,		 
	0x91, 0x00,		 
	0xc0,			 
	0x06, 0x00, 0xff,	 
	0x09, 0x02,		 
	0xa1, 0x01,		 
	0x85, 0x11,		 
	0x75, 0x08,		 
	0x95, 0x13,		 
	0x15, 0x00,		 
	0x26, 0xff, 0x00,	 
	0x09, 0x02,		 
	0x81, 0x00,		 
	0x09, 0x02,		 
	0x91, 0x00,		 
	0xc0,			 
	0x06, 0x00, 0xff,	 
	0x09, 0x04,		 
	0xa1, 0x01,		 
	0x85, 0x20,		 
	0x75, 0x08,		 
	0x95, 0x0e,		 
	0x15, 0x00,		 
	0x26, 0xff, 0x00,	 
	0x09, 0x41,		 
	0x81, 0x00,		 
	0x09, 0x41,		 
	0x91, 0x00,		 
	0x85, 0x21,		 
	0x95, 0x1f,		 
	0x15, 0x00,		 
	0x26, 0xff, 0x00,	 
	0x09, 0x42,		 
	0x81, 0x00,		 
	0x09, 0x42,		 
	0x91, 0x00,		 
	0xc0,			 
};

 
#define MAX_REPORT_SIZE 8

 
#define MAX_RDESC_SIZE				\
	(sizeof(kbd_descriptor) +		\
	 sizeof(mse_bluetooth_descriptor) +	\
	 sizeof(mse5_bluetooth_descriptor) +	\
	 sizeof(consumer_descriptor) +		\
	 sizeof(syscontrol_descriptor) +	\
	 sizeof(media_descriptor) +	\
	 sizeof(hidpp_descriptor))

 
#define NUMBER_OF_HID_REPORTS 32
static const u8 hid_reportid_size_map[NUMBER_OF_HID_REPORTS] = {
	[1] = 8,		 
	[2] = 8,		 
	[3] = 5,		 
	[4] = 2,		 
	[8] = 2,		 
};


#define LOGITECH_DJ_INTERFACE_NUMBER 0x02

static const struct hid_ll_driver logi_dj_ll_driver;

static int logi_dj_recv_query_paired_devices(struct dj_receiver_dev *djrcv_dev);
static void delayedwork_callback(struct work_struct *work);

static LIST_HEAD(dj_hdev_list);
static DEFINE_MUTEX(dj_hdev_list_lock);

static bool recvr_type_is_bluetooth(enum recvr_type type)
{
	return type == recvr_type_bluetooth || type == recvr_type_dinovo;
}

 
static struct dj_receiver_dev *dj_find_receiver_dev(struct hid_device *hdev,
						    enum recvr_type type)
{
	struct dj_receiver_dev *djrcv_dev;
	char sep;

	 
	sep = recvr_type_is_bluetooth(type) ? '.' : '/';

	 
	list_for_each_entry(djrcv_dev, &dj_hdev_list, list) {
		if (djrcv_dev->mouse &&
		    hid_compare_device_paths(hdev, djrcv_dev->mouse, sep)) {
			kref_get(&djrcv_dev->kref);
			return djrcv_dev;
		}
		if (djrcv_dev->keyboard &&
		    hid_compare_device_paths(hdev, djrcv_dev->keyboard, sep)) {
			kref_get(&djrcv_dev->kref);
			return djrcv_dev;
		}
		if (djrcv_dev->hidpp &&
		    hid_compare_device_paths(hdev, djrcv_dev->hidpp, sep)) {
			kref_get(&djrcv_dev->kref);
			return djrcv_dev;
		}
	}

	return NULL;
}

static void dj_release_receiver_dev(struct kref *kref)
{
	struct dj_receiver_dev *djrcv_dev = container_of(kref, struct dj_receiver_dev, kref);

	list_del(&djrcv_dev->list);
	kfifo_free(&djrcv_dev->notif_fifo);
	kfree(djrcv_dev);
}

static void dj_put_receiver_dev(struct hid_device *hdev)
{
	struct dj_receiver_dev *djrcv_dev = hid_get_drvdata(hdev);

	mutex_lock(&dj_hdev_list_lock);

	if (djrcv_dev->mouse == hdev)
		djrcv_dev->mouse = NULL;
	if (djrcv_dev->keyboard == hdev)
		djrcv_dev->keyboard = NULL;
	if (djrcv_dev->hidpp == hdev)
		djrcv_dev->hidpp = NULL;

	kref_put(&djrcv_dev->kref, dj_release_receiver_dev);

	mutex_unlock(&dj_hdev_list_lock);
}

static struct dj_receiver_dev *dj_get_receiver_dev(struct hid_device *hdev,
						   enum recvr_type type,
						   unsigned int application,
						   bool is_hidpp)
{
	struct dj_receiver_dev *djrcv_dev;

	mutex_lock(&dj_hdev_list_lock);

	djrcv_dev = dj_find_receiver_dev(hdev, type);
	if (!djrcv_dev) {
		djrcv_dev = kzalloc(sizeof(*djrcv_dev), GFP_KERNEL);
		if (!djrcv_dev)
			goto out;

		INIT_WORK(&djrcv_dev->work, delayedwork_callback);
		spin_lock_init(&djrcv_dev->lock);
		if (kfifo_alloc(&djrcv_dev->notif_fifo,
			    DJ_MAX_NUMBER_NOTIFS * sizeof(struct dj_workitem),
			    GFP_KERNEL)) {
			kfree(djrcv_dev);
			djrcv_dev = NULL;
			goto out;
		}
		kref_init(&djrcv_dev->kref);
		list_add_tail(&djrcv_dev->list, &dj_hdev_list);
		djrcv_dev->last_query = jiffies;
		djrcv_dev->type = type;
	}

	if (application == HID_GD_KEYBOARD)
		djrcv_dev->keyboard = hdev;
	if (application == HID_GD_MOUSE)
		djrcv_dev->mouse = hdev;
	if (is_hidpp)
		djrcv_dev->hidpp = hdev;

	hid_set_drvdata(hdev, djrcv_dev);
out:
	mutex_unlock(&dj_hdev_list_lock);
	return djrcv_dev;
}

static void logi_dj_recv_destroy_djhid_device(struct dj_receiver_dev *djrcv_dev,
					      struct dj_workitem *workitem)
{
	 
	struct dj_device *dj_dev;
	unsigned long flags;

	spin_lock_irqsave(&djrcv_dev->lock, flags);
	dj_dev = djrcv_dev->paired_dj_devices[workitem->device_index];
	djrcv_dev->paired_dj_devices[workitem->device_index] = NULL;
	spin_unlock_irqrestore(&djrcv_dev->lock, flags);

	if (dj_dev != NULL) {
		hid_destroy_device(dj_dev->hdev);
		kfree(dj_dev);
	} else {
		hid_err(djrcv_dev->hidpp, "%s: can't destroy a NULL device\n",
			__func__);
	}
}

static void logi_dj_recv_add_djhid_device(struct dj_receiver_dev *djrcv_dev,
					  struct dj_workitem *workitem)
{
	 
	struct hid_device *djrcv_hdev = djrcv_dev->hidpp;
	struct hid_device *dj_hiddev;
	struct dj_device *dj_dev;
	u8 device_index = workitem->device_index;
	unsigned long flags;

	 
	unsigned char tmpstr[3];

	 
	if (djrcv_dev->paired_dj_devices[device_index]) {
		 
		dbg_hid("%s: device is already known\n", __func__);
		return;
	}

	dj_hiddev = hid_allocate_device();
	if (IS_ERR(dj_hiddev)) {
		hid_err(djrcv_hdev, "%s: hid_allocate_dev failed\n", __func__);
		return;
	}

	dj_hiddev->ll_driver = &logi_dj_ll_driver;

	dj_hiddev->dev.parent = &djrcv_hdev->dev;
	dj_hiddev->bus = BUS_USB;
	dj_hiddev->vendor = djrcv_hdev->vendor;
	dj_hiddev->product = (workitem->quad_id_msb << 8) |
			      workitem->quad_id_lsb;
	if (workitem->device_type) {
		const char *type_str = "Device";

		switch (workitem->device_type) {
		case 0x01: type_str = "Keyboard";	break;
		case 0x02: type_str = "Mouse";		break;
		case 0x03: type_str = "Numpad";		break;
		case 0x04: type_str = "Presenter";	break;
		case 0x07: type_str = "Remote Control";	break;
		case 0x08: type_str = "Trackball";	break;
		case 0x09: type_str = "Touchpad";	break;
		}
		snprintf(dj_hiddev->name, sizeof(dj_hiddev->name),
			"Logitech Wireless %s PID:%04x",
			type_str, dj_hiddev->product);
	} else {
		snprintf(dj_hiddev->name, sizeof(dj_hiddev->name),
			"Logitech Wireless Device PID:%04x",
			dj_hiddev->product);
	}

	if (djrcv_dev->type == recvr_type_27mhz)
		dj_hiddev->group = HID_GROUP_LOGITECH_27MHZ_DEVICE;
	else
		dj_hiddev->group = HID_GROUP_LOGITECH_DJ_DEVICE;

	memcpy(dj_hiddev->phys, djrcv_hdev->phys, sizeof(djrcv_hdev->phys));
	snprintf(tmpstr, sizeof(tmpstr), ":%d", device_index);
	strlcat(dj_hiddev->phys, tmpstr, sizeof(dj_hiddev->phys));

	dj_dev = kzalloc(sizeof(struct dj_device), GFP_KERNEL);

	if (!dj_dev) {
		hid_err(djrcv_hdev, "%s: failed allocating dj_dev\n", __func__);
		goto dj_device_allocate_fail;
	}

	dj_dev->reports_supported = workitem->reports_supported;
	dj_dev->hdev = dj_hiddev;
	dj_dev->dj_receiver_dev = djrcv_dev;
	dj_dev->device_index = device_index;
	dj_hiddev->driver_data = dj_dev;

	spin_lock_irqsave(&djrcv_dev->lock, flags);
	djrcv_dev->paired_dj_devices[device_index] = dj_dev;
	spin_unlock_irqrestore(&djrcv_dev->lock, flags);

	if (hid_add_device(dj_hiddev)) {
		hid_err(djrcv_hdev, "%s: failed adding dj_device\n", __func__);
		goto hid_add_device_fail;
	}

	return;

hid_add_device_fail:
	spin_lock_irqsave(&djrcv_dev->lock, flags);
	djrcv_dev->paired_dj_devices[device_index] = NULL;
	spin_unlock_irqrestore(&djrcv_dev->lock, flags);
	kfree(dj_dev);
dj_device_allocate_fail:
	hid_destroy_device(dj_hiddev);
}

static void delayedwork_callback(struct work_struct *work)
{
	struct dj_receiver_dev *djrcv_dev =
		container_of(work, struct dj_receiver_dev, work);

	struct dj_workitem workitem;
	unsigned long flags;
	int count;
	int retval;

	dbg_hid("%s\n", __func__);

	spin_lock_irqsave(&djrcv_dev->lock, flags);

	 
	if (!djrcv_dev->ready) {
		pr_warn("%s: delayedwork queued before hidpp interface was enumerated\n",
			__func__);
		spin_unlock_irqrestore(&djrcv_dev->lock, flags);
		return;
	}

	count = kfifo_out(&djrcv_dev->notif_fifo, &workitem, sizeof(workitem));

	if (count != sizeof(workitem)) {
		spin_unlock_irqrestore(&djrcv_dev->lock, flags);
		return;
	}

	if (!kfifo_is_empty(&djrcv_dev->notif_fifo))
		schedule_work(&djrcv_dev->work);

	spin_unlock_irqrestore(&djrcv_dev->lock, flags);

	switch (workitem.type) {
	case WORKITEM_TYPE_PAIRED:
		logi_dj_recv_add_djhid_device(djrcv_dev, &workitem);
		break;
	case WORKITEM_TYPE_UNPAIRED:
		logi_dj_recv_destroy_djhid_device(djrcv_dev, &workitem);
		break;
	case WORKITEM_TYPE_UNKNOWN:
		retval = logi_dj_recv_query_paired_devices(djrcv_dev);
		if (retval) {
			hid_err(djrcv_dev->hidpp, "%s: logi_dj_recv_query_paired_devices error: %d\n",
				__func__, retval);
		}
		break;
	case WORKITEM_TYPE_EMPTY:
		dbg_hid("%s: device list is empty\n", __func__);
		break;
	}
}

 
static void logi_dj_recv_queue_unknown_work(struct dj_receiver_dev *djrcv_dev)
{
	struct dj_workitem workitem = { .type = WORKITEM_TYPE_UNKNOWN };

	 
	if (time_before(jiffies, djrcv_dev->last_query + HZ / 2))
		return;

	kfifo_in(&djrcv_dev->notif_fifo, &workitem, sizeof(workitem));
	schedule_work(&djrcv_dev->work);
}

static void logi_dj_recv_queue_notification(struct dj_receiver_dev *djrcv_dev,
					   struct dj_report *dj_report)
{
	 
	struct dj_workitem workitem = {
		.device_index = dj_report->device_index,
	};

	switch (dj_report->report_type) {
	case REPORT_TYPE_NOTIF_DEVICE_PAIRED:
		workitem.type = WORKITEM_TYPE_PAIRED;
		if (dj_report->report_params[DEVICE_PAIRED_PARAM_SPFUNCTION] &
		    SPFUNCTION_DEVICE_LIST_EMPTY) {
			workitem.type = WORKITEM_TYPE_EMPTY;
			break;
		}
		fallthrough;
	case REPORT_TYPE_NOTIF_DEVICE_UNPAIRED:
		workitem.quad_id_msb =
			dj_report->report_params[DEVICE_PAIRED_PARAM_EQUAD_ID_MSB];
		workitem.quad_id_lsb =
			dj_report->report_params[DEVICE_PAIRED_PARAM_EQUAD_ID_LSB];
		workitem.reports_supported = get_unaligned_le32(
						dj_report->report_params +
						DEVICE_PAIRED_RF_REPORT_TYPE);
		workitem.reports_supported |= HIDPP;
		if (dj_report->report_type == REPORT_TYPE_NOTIF_DEVICE_UNPAIRED)
			workitem.type = WORKITEM_TYPE_UNPAIRED;
		break;
	default:
		logi_dj_recv_queue_unknown_work(djrcv_dev);
		return;
	}

	kfifo_in(&djrcv_dev->notif_fifo, &workitem, sizeof(workitem));
	schedule_work(&djrcv_dev->work);
}

 
static const u16 kbd_builtin_touchpad_ids[] = {
	0xb309,  
	0xb30c,  
};

static void logi_hidpp_dev_conn_notif_equad(struct hid_device *hdev,
					    struct hidpp_event *hidpp_report,
					    struct dj_workitem *workitem)
{
	struct dj_receiver_dev *djrcv_dev = hid_get_drvdata(hdev);
	int i, id;

	workitem->type = WORKITEM_TYPE_PAIRED;
	workitem->device_type = hidpp_report->params[HIDPP_PARAM_DEVICE_INFO] &
				HIDPP_DEVICE_TYPE_MASK;
	workitem->quad_id_msb = hidpp_report->params[HIDPP_PARAM_EQUAD_MSB];
	workitem->quad_id_lsb = hidpp_report->params[HIDPP_PARAM_EQUAD_LSB];
	switch (workitem->device_type) {
	case REPORT_TYPE_KEYBOARD:
		workitem->reports_supported |= STD_KEYBOARD | MULTIMEDIA |
					       POWER_KEYS | MEDIA_CENTER |
					       HIDPP;
		id = (workitem->quad_id_msb << 8) | workitem->quad_id_lsb;
		for (i = 0; i < ARRAY_SIZE(kbd_builtin_touchpad_ids); i++) {
			if (id == kbd_builtin_touchpad_ids[i]) {
				if (djrcv_dev->type == recvr_type_dinovo)
					workitem->reports_supported |= KBD_MOUSE;
				else
					workitem->reports_supported |= STD_MOUSE;
				break;
			}
		}
		break;
	case REPORT_TYPE_MOUSE:
		workitem->reports_supported |= STD_MOUSE | HIDPP;
		if (djrcv_dev->type == recvr_type_mouse_only)
			workitem->reports_supported |= MULTIMEDIA;
		break;
	}
}

static void logi_hidpp_dev_conn_notif_27mhz(struct hid_device *hdev,
					    struct hidpp_event *hidpp_report,
					    struct dj_workitem *workitem)
{
	workitem->type = WORKITEM_TYPE_PAIRED;
	workitem->quad_id_lsb = hidpp_report->params[HIDPP_PARAM_27MHZ_DEVID];
	switch (hidpp_report->device_index) {
	case 1:  
	case 2:  
		workitem->device_type = HIDPP_DEVICE_TYPE_MOUSE;
		workitem->reports_supported |= STD_MOUSE | HIDPP;
		break;
	case 3:  
		if (hidpp_report->params[HIDPP_PARAM_DEVICE_INFO] & HIDPP_27MHZ_SECURE_MASK) {
			hid_info(hdev, "Keyboard connection is encrypted\n");
		} else {
			hid_warn(hdev, "Keyboard events are send over the air in plain-text / unencrypted\n");
			hid_warn(hdev, "See: https://gitlab.freedesktop.org/jwrdegoede/logitech-27mhz-keyboard-encryption-setup/\n");
		}
		fallthrough;
	case 4:  
		workitem->device_type = HIDPP_DEVICE_TYPE_KEYBOARD;
		workitem->reports_supported |= STD_KEYBOARD | MULTIMEDIA |
					       POWER_KEYS | HIDPP;
		break;
	default:
		hid_warn(hdev, "%s: unexpected device-index %d", __func__,
			 hidpp_report->device_index);
	}
}

static void logi_hidpp_recv_queue_notif(struct hid_device *hdev,
					struct hidpp_event *hidpp_report)
{
	 
	struct dj_receiver_dev *djrcv_dev = hid_get_drvdata(hdev);
	const char *device_type = "UNKNOWN";
	struct dj_workitem workitem = {
		.type = WORKITEM_TYPE_EMPTY,
		.device_index = hidpp_report->device_index,
	};

	switch (hidpp_report->params[HIDPP_PARAM_PROTO_TYPE]) {
	case 0x01:
		device_type = "Bluetooth";
		 
		logi_hidpp_dev_conn_notif_equad(hdev, hidpp_report, &workitem);
		if (!(hidpp_report->params[HIDPP_PARAM_DEVICE_INFO] &
						HIDPP_MANUFACTURER_MASK)) {
			hid_info(hdev, "Non Logitech device connected on slot %d\n",
				 hidpp_report->device_index);
			workitem.reports_supported &= ~HIDPP;
		}
		break;
	case 0x02:
		device_type = "27 Mhz";
		logi_hidpp_dev_conn_notif_27mhz(hdev, hidpp_report, &workitem);
		break;
	case 0x03:
		device_type = "QUAD or eQUAD";
		logi_hidpp_dev_conn_notif_equad(hdev, hidpp_report, &workitem);
		break;
	case 0x04:
		device_type = "eQUAD step 4 DJ";
		logi_hidpp_dev_conn_notif_equad(hdev, hidpp_report, &workitem);
		break;
	case 0x05:
		device_type = "DFU Lite";
		break;
	case 0x06:
		device_type = "eQUAD step 4 Lite";
		logi_hidpp_dev_conn_notif_equad(hdev, hidpp_report, &workitem);
		break;
	case 0x07:
		device_type = "eQUAD step 4 Gaming";
		logi_hidpp_dev_conn_notif_equad(hdev, hidpp_report, &workitem);
		workitem.reports_supported |= STD_KEYBOARD;
		break;
	case 0x08:
		device_type = "eQUAD step 4 for gamepads";
		break;
	case 0x0a:
		device_type = "eQUAD nano Lite";
		logi_hidpp_dev_conn_notif_equad(hdev, hidpp_report, &workitem);
		break;
	case 0x0c:
		device_type = "eQUAD Lightspeed 1";
		logi_hidpp_dev_conn_notif_equad(hdev, hidpp_report, &workitem);
		workitem.reports_supported |= STD_KEYBOARD;
		break;
	case 0x0d:
		device_type = "eQUAD Lightspeed 1.1";
		logi_hidpp_dev_conn_notif_equad(hdev, hidpp_report, &workitem);
		workitem.reports_supported |= STD_KEYBOARD;
		break;
	case 0x0f:
	case 0x11:
		device_type = "eQUAD Lightspeed 1.2";
		logi_hidpp_dev_conn_notif_equad(hdev, hidpp_report, &workitem);
		workitem.reports_supported |= STD_KEYBOARD;
		break;
	}

	 
	if (hidpp_report->device_index == 7) {
		workitem.reports_supported |= HIDPP;
	}

	if (workitem.type == WORKITEM_TYPE_EMPTY) {
		hid_warn(hdev,
			 "unusable device of type %s (0x%02x) connected on slot %d",
			 device_type,
			 hidpp_report->params[HIDPP_PARAM_PROTO_TYPE],
			 hidpp_report->device_index);
		return;
	}

	hid_info(hdev, "device of type %s (0x%02x) connected on slot %d",
		 device_type, hidpp_report->params[HIDPP_PARAM_PROTO_TYPE],
		 hidpp_report->device_index);

	kfifo_in(&djrcv_dev->notif_fifo, &workitem, sizeof(workitem));
	schedule_work(&djrcv_dev->work);
}

static void logi_dj_recv_forward_null_report(struct dj_receiver_dev *djrcv_dev,
					     struct dj_report *dj_report)
{
	 
	unsigned int i;
	u8 reportbuffer[MAX_REPORT_SIZE];
	struct dj_device *djdev;

	djdev = djrcv_dev->paired_dj_devices[dj_report->device_index];

	memset(reportbuffer, 0, sizeof(reportbuffer));

	for (i = 0; i < NUMBER_OF_HID_REPORTS; i++) {
		if (djdev->reports_supported & (1 << i)) {
			reportbuffer[0] = i;
			if (hid_input_report(djdev->hdev,
					     HID_INPUT_REPORT,
					     reportbuffer,
					     hid_reportid_size_map[i], 1)) {
				dbg_hid("hid_input_report error sending null "
					"report\n");
			}
		}
	}
}

static void logi_dj_recv_forward_dj(struct dj_receiver_dev *djrcv_dev,
				    struct dj_report *dj_report)
{
	 
	struct dj_device *dj_device;

	dj_device = djrcv_dev->paired_dj_devices[dj_report->device_index];

	if ((dj_report->report_type > ARRAY_SIZE(hid_reportid_size_map) - 1) ||
	    (hid_reportid_size_map[dj_report->report_type] == 0)) {
		dbg_hid("invalid report type:%x\n", dj_report->report_type);
		return;
	}

	if (hid_input_report(dj_device->hdev,
			HID_INPUT_REPORT, &dj_report->report_type,
			hid_reportid_size_map[dj_report->report_type], 1)) {
		dbg_hid("hid_input_report error\n");
	}
}

static void logi_dj_recv_forward_report(struct dj_device *dj_dev, u8 *data,
					int size)
{
	 
	if (hid_input_report(dj_dev->hdev, HID_INPUT_REPORT, data, size, 1))
		dbg_hid("hid_input_report error\n");
}

static void logi_dj_recv_forward_input_report(struct hid_device *hdev,
					      u8 *data, int size)
{
	struct dj_receiver_dev *djrcv_dev = hid_get_drvdata(hdev);
	struct dj_device *dj_dev;
	unsigned long flags;
	u8 report = data[0];
	int i;

	if (report > REPORT_TYPE_RFREPORT_LAST) {
		hid_err(hdev, "Unexpected input report number %d\n", report);
		return;
	}

	spin_lock_irqsave(&djrcv_dev->lock, flags);
	for (i = 0; i < (DJ_MAX_PAIRED_DEVICES + DJ_DEVICE_INDEX_MIN); i++) {
		dj_dev = djrcv_dev->paired_dj_devices[i];
		if (dj_dev && (dj_dev->reports_supported & BIT(report))) {
			logi_dj_recv_forward_report(dj_dev, data, size);
			spin_unlock_irqrestore(&djrcv_dev->lock, flags);
			return;
		}
	}

	logi_dj_recv_queue_unknown_work(djrcv_dev);
	spin_unlock_irqrestore(&djrcv_dev->lock, flags);

	dbg_hid("No dj-devs handling input report number %d\n", report);
}

static int logi_dj_recv_send_report(struct dj_receiver_dev *djrcv_dev,
				    struct dj_report *dj_report)
{
	struct hid_device *hdev = djrcv_dev->hidpp;
	struct hid_report *report;
	struct hid_report_enum *output_report_enum;
	u8 *data = (u8 *)(&dj_report->device_index);
	unsigned int i;

	output_report_enum = &hdev->report_enum[HID_OUTPUT_REPORT];
	report = output_report_enum->report_id_hash[REPORT_ID_DJ_SHORT];

	if (!report) {
		hid_err(hdev, "%s: unable to find dj report\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < DJREPORT_SHORT_LENGTH - 1; i++)
		report->field[0]->value[i] = data[i];

	hid_hw_request(hdev, report, HID_REQ_SET_REPORT);

	return 0;
}

static int logi_dj_recv_query_hidpp_devices(struct dj_receiver_dev *djrcv_dev)
{
	static const u8 template[] = {
		REPORT_ID_HIDPP_SHORT,
		HIDPP_RECEIVER_INDEX,
		HIDPP_SET_REGISTER,
		HIDPP_REG_CONNECTION_STATE,
		HIDPP_FAKE_DEVICE_ARRIVAL,
		0x00, 0x00
	};
	u8 *hidpp_report;
	int retval;

	hidpp_report = kmemdup(template, sizeof(template), GFP_KERNEL);
	if (!hidpp_report)
		return -ENOMEM;

	retval = hid_hw_raw_request(djrcv_dev->hidpp,
				    REPORT_ID_HIDPP_SHORT,
				    hidpp_report, sizeof(template),
				    HID_OUTPUT_REPORT,
				    HID_REQ_SET_REPORT);

	kfree(hidpp_report);
	return (retval < 0) ? retval : 0;
}

static int logi_dj_recv_query_paired_devices(struct dj_receiver_dev *djrcv_dev)
{
	struct dj_report *dj_report;
	int retval;

	djrcv_dev->last_query = jiffies;

	if (djrcv_dev->type != recvr_type_dj)
		return logi_dj_recv_query_hidpp_devices(djrcv_dev);

	dj_report = kzalloc(sizeof(struct dj_report), GFP_KERNEL);
	if (!dj_report)
		return -ENOMEM;
	dj_report->report_id = REPORT_ID_DJ_SHORT;
	dj_report->device_index = HIDPP_RECEIVER_INDEX;
	dj_report->report_type = REPORT_TYPE_CMD_GET_PAIRED_DEVICES;
	retval = logi_dj_recv_send_report(djrcv_dev, dj_report);
	kfree(dj_report);
	return retval;
}


static int logi_dj_recv_switch_to_dj_mode(struct dj_receiver_dev *djrcv_dev,
					  unsigned timeout)
{
	struct hid_device *hdev = djrcv_dev->hidpp;
	struct dj_report *dj_report;
	u8 *buf;
	int retval = 0;

	dj_report = kzalloc(sizeof(struct dj_report), GFP_KERNEL);
	if (!dj_report)
		return -ENOMEM;

	if (djrcv_dev->type == recvr_type_dj) {
		dj_report->report_id = REPORT_ID_DJ_SHORT;
		dj_report->device_index = HIDPP_RECEIVER_INDEX;
		dj_report->report_type = REPORT_TYPE_CMD_SWITCH;
		dj_report->report_params[CMD_SWITCH_PARAM_DEVBITFIELD] = 0x3F;
		dj_report->report_params[CMD_SWITCH_PARAM_TIMEOUT_SECONDS] =
								(u8)timeout;

		retval = logi_dj_recv_send_report(djrcv_dev, dj_report);

		 
		msleep(50);

		if (retval)
			return retval;
	}

	 
	buf = (u8 *)dj_report;

	memset(buf, 0, HIDPP_REPORT_SHORT_LENGTH);

	buf[0] = REPORT_ID_HIDPP_SHORT;
	buf[1] = HIDPP_RECEIVER_INDEX;
	buf[2] = 0x80;
	buf[3] = 0x00;
	buf[4] = 0x00;
	buf[5] = 0x09;
	buf[6] = 0x00;

	retval = hid_hw_raw_request(hdev, REPORT_ID_HIDPP_SHORT, buf,
			HIDPP_REPORT_SHORT_LENGTH, HID_OUTPUT_REPORT,
			HID_REQ_SET_REPORT);

	kfree(dj_report);
	return retval;
}


static int logi_dj_ll_open(struct hid_device *hid)
{
	dbg_hid("%s: %s\n", __func__, hid->phys);
	return 0;

}

static void logi_dj_ll_close(struct hid_device *hid)
{
	dbg_hid("%s: %s\n", __func__, hid->phys);
}

 
static u8 unifying_pairing_query[]  = { REPORT_ID_HIDPP_SHORT,
					HIDPP_RECEIVER_INDEX,
					HIDPP_GET_LONG_REGISTER,
					HIDPP_REG_PAIRING_INFORMATION };
static u8 unifying_pairing_answer[] = { REPORT_ID_HIDPP_LONG,
					HIDPP_RECEIVER_INDEX,
					HIDPP_GET_LONG_REGISTER,
					HIDPP_REG_PAIRING_INFORMATION };

static int logi_dj_ll_raw_request(struct hid_device *hid,
				  unsigned char reportnum, __u8 *buf,
				  size_t count, unsigned char report_type,
				  int reqtype)
{
	struct dj_device *djdev = hid->driver_data;
	struct dj_receiver_dev *djrcv_dev = djdev->dj_receiver_dev;
	u8 *out_buf;
	int ret;

	if ((buf[0] == REPORT_ID_HIDPP_SHORT) ||
	    (buf[0] == REPORT_ID_HIDPP_LONG) ||
	    (buf[0] == REPORT_ID_HIDPP_VERY_LONG)) {
		if (count < 2)
			return -EINVAL;

		 
		if (count == 7 && !memcmp(buf, unifying_pairing_query,
					  sizeof(unifying_pairing_query)))
			buf[4] = (buf[4] & 0xf0) | (djdev->device_index - 1);
		else
			buf[1] = djdev->device_index;
		return hid_hw_raw_request(djrcv_dev->hidpp, reportnum, buf,
				count, report_type, reqtype);
	}

	if (buf[0] != REPORT_TYPE_LEDS)
		return -EINVAL;

	if (djrcv_dev->type != recvr_type_dj && count >= 2) {
		if (!djrcv_dev->keyboard) {
			hid_warn(hid, "Received REPORT_TYPE_LEDS request before the keyboard interface was enumerated\n");
			return 0;
		}
		 
		return hid_hw_raw_request(djrcv_dev->keyboard, 0, buf, count,
					  report_type, reqtype);
	}

	out_buf = kzalloc(DJREPORT_SHORT_LENGTH, GFP_ATOMIC);
	if (!out_buf)
		return -ENOMEM;

	if (count > DJREPORT_SHORT_LENGTH - 2)
		count = DJREPORT_SHORT_LENGTH - 2;

	out_buf[0] = REPORT_ID_DJ_SHORT;
	out_buf[1] = djdev->device_index;
	memcpy(out_buf + 2, buf, count);

	ret = hid_hw_raw_request(djrcv_dev->hidpp, out_buf[0], out_buf,
		DJREPORT_SHORT_LENGTH, report_type, reqtype);

	kfree(out_buf);
	return ret;
}

static void rdcat(char *rdesc, unsigned int *rsize, const char *data, unsigned int size)
{
	memcpy(rdesc + *rsize, data, size);
	*rsize += size;
}

static int logi_dj_ll_parse(struct hid_device *hid)
{
	struct dj_device *djdev = hid->driver_data;
	unsigned int rsize = 0;
	char *rdesc;
	int retval;

	dbg_hid("%s\n", __func__);

	djdev->hdev->version = 0x0111;
	djdev->hdev->country = 0x00;

	rdesc = kmalloc(MAX_RDESC_SIZE, GFP_KERNEL);
	if (!rdesc)
		return -ENOMEM;

	if (djdev->reports_supported & STD_KEYBOARD) {
		dbg_hid("%s: sending a kbd descriptor, reports_supported: %llx\n",
			__func__, djdev->reports_supported);
		rdcat(rdesc, &rsize, kbd_descriptor, sizeof(kbd_descriptor));
	}

	if (djdev->reports_supported & STD_MOUSE) {
		dbg_hid("%s: sending a mouse descriptor, reports_supported: %llx\n",
			__func__, djdev->reports_supported);
		if (djdev->dj_receiver_dev->type == recvr_type_gaming_hidpp ||
		    djdev->dj_receiver_dev->type == recvr_type_mouse_only)
			rdcat(rdesc, &rsize, mse_high_res_descriptor,
			      sizeof(mse_high_res_descriptor));
		else if (djdev->dj_receiver_dev->type == recvr_type_27mhz)
			rdcat(rdesc, &rsize, mse_27mhz_descriptor,
			      sizeof(mse_27mhz_descriptor));
		else if (recvr_type_is_bluetooth(djdev->dj_receiver_dev->type))
			rdcat(rdesc, &rsize, mse_bluetooth_descriptor,
			      sizeof(mse_bluetooth_descriptor));
		else
			rdcat(rdesc, &rsize, mse_descriptor,
			      sizeof(mse_descriptor));
	}

	if (djdev->reports_supported & KBD_MOUSE) {
		dbg_hid("%s: sending a kbd-mouse descriptor, reports_supported: %llx\n",
			__func__, djdev->reports_supported);
		rdcat(rdesc, &rsize, mse5_bluetooth_descriptor,
		      sizeof(mse5_bluetooth_descriptor));
	}

	if (djdev->reports_supported & MULTIMEDIA) {
		dbg_hid("%s: sending a multimedia report descriptor: %llx\n",
			__func__, djdev->reports_supported);
		rdcat(rdesc, &rsize, consumer_descriptor, sizeof(consumer_descriptor));
	}

	if (djdev->reports_supported & POWER_KEYS) {
		dbg_hid("%s: sending a power keys report descriptor: %llx\n",
			__func__, djdev->reports_supported);
		rdcat(rdesc, &rsize, syscontrol_descriptor, sizeof(syscontrol_descriptor));
	}

	if (djdev->reports_supported & MEDIA_CENTER) {
		dbg_hid("%s: sending a media center report descriptor: %llx\n",
			__func__, djdev->reports_supported);
		rdcat(rdesc, &rsize, media_descriptor, sizeof(media_descriptor));
	}

	if (djdev->reports_supported & KBD_LEDS) {
		dbg_hid("%s: need to send kbd leds report descriptor: %llx\n",
			__func__, djdev->reports_supported);
	}

	if (djdev->reports_supported & HIDPP) {
		dbg_hid("%s: sending a HID++ descriptor, reports_supported: %llx\n",
			__func__, djdev->reports_supported);
		rdcat(rdesc, &rsize, hidpp_descriptor,
		      sizeof(hidpp_descriptor));
	}

	retval = hid_parse_report(hid, rdesc, rsize);
	kfree(rdesc);

	return retval;
}

static int logi_dj_ll_start(struct hid_device *hid)
{
	dbg_hid("%s\n", __func__);
	return 0;
}

static void logi_dj_ll_stop(struct hid_device *hid)
{
	dbg_hid("%s\n", __func__);
}

static bool logi_dj_ll_may_wakeup(struct hid_device *hid)
{
	struct dj_device *djdev = hid->driver_data;
	struct dj_receiver_dev *djrcv_dev = djdev->dj_receiver_dev;

	return hid_hw_may_wakeup(djrcv_dev->hidpp);
}

static const struct hid_ll_driver logi_dj_ll_driver = {
	.parse = logi_dj_ll_parse,
	.start = logi_dj_ll_start,
	.stop = logi_dj_ll_stop,
	.open = logi_dj_ll_open,
	.close = logi_dj_ll_close,
	.raw_request = logi_dj_ll_raw_request,
	.may_wakeup = logi_dj_ll_may_wakeup,
};

static int logi_dj_dj_event(struct hid_device *hdev,
			     struct hid_report *report, u8 *data,
			     int size)
{
	struct dj_receiver_dev *djrcv_dev = hid_get_drvdata(hdev);
	struct dj_report *dj_report = (struct dj_report *) data;
	unsigned long flags;

	 

	if ((dj_report->device_index < DJ_DEVICE_INDEX_MIN) ||
	    (dj_report->device_index > DJ_DEVICE_INDEX_MAX)) {
		 
		if (dj_report->device_index != DJ_RECEIVER_INDEX)
			hid_err(hdev, "%s: invalid device index:%d\n",
				__func__, dj_report->device_index);
		return false;
	}

	spin_lock_irqsave(&djrcv_dev->lock, flags);

	if (!djrcv_dev->paired_dj_devices[dj_report->device_index]) {
		 
		logi_dj_recv_queue_notification(djrcv_dev, dj_report);
		goto out;
	}

	switch (dj_report->report_type) {
	case REPORT_TYPE_NOTIF_DEVICE_PAIRED:
		 
		break;
	case REPORT_TYPE_NOTIF_DEVICE_UNPAIRED:
		logi_dj_recv_queue_notification(djrcv_dev, dj_report);
		break;
	case REPORT_TYPE_NOTIF_CONNECTION_STATUS:
		if (dj_report->report_params[CONNECTION_STATUS_PARAM_STATUS] ==
		    STATUS_LINKLOSS) {
			logi_dj_recv_forward_null_report(djrcv_dev, dj_report);
		}
		break;
	default:
		logi_dj_recv_forward_dj(djrcv_dev, dj_report);
	}

out:
	spin_unlock_irqrestore(&djrcv_dev->lock, flags);

	return true;
}

static int logi_dj_hidpp_event(struct hid_device *hdev,
			     struct hid_report *report, u8 *data,
			     int size)
{
	struct dj_receiver_dev *djrcv_dev = hid_get_drvdata(hdev);
	struct hidpp_event *hidpp_report = (struct hidpp_event *) data;
	struct dj_device *dj_dev;
	unsigned long flags;
	u8 device_index = hidpp_report->device_index;

	if (device_index == HIDPP_RECEIVER_INDEX) {
		 
		if (size == HIDPP_REPORT_LONG_LENGTH &&
		    !memcmp(data, unifying_pairing_answer,
			    sizeof(unifying_pairing_answer)))
			device_index = (data[4] & 0x0F) + 1;
		else
			return false;
	}

	 

	if ((device_index < DJ_DEVICE_INDEX_MIN) ||
	    (device_index > DJ_DEVICE_INDEX_MAX)) {
		 
		hid_err(hdev, "%s: invalid device index:%d\n", __func__,
			hidpp_report->device_index);
		return false;
	}

	spin_lock_irqsave(&djrcv_dev->lock, flags);

	dj_dev = djrcv_dev->paired_dj_devices[device_index];

	 
	if (djrcv_dev->type == recvr_type_27mhz && dj_dev &&
	    hidpp_report->sub_id == REPORT_TYPE_NOTIF_DEVICE_CONNECTED &&
	    hidpp_report->params[HIDPP_PARAM_PROTO_TYPE] == 0x02 &&
	    hidpp_report->params[HIDPP_PARAM_27MHZ_DEVID] !=
						dj_dev->hdev->product) {
		struct dj_workitem workitem = {
			.device_index = hidpp_report->device_index,
			.type = WORKITEM_TYPE_UNPAIRED,
		};
		kfifo_in(&djrcv_dev->notif_fifo, &workitem, sizeof(workitem));
		 
		dj_dev = NULL;
	}

	if (dj_dev) {
		logi_dj_recv_forward_report(dj_dev, data, size);
	} else {
		if (hidpp_report->sub_id == REPORT_TYPE_NOTIF_DEVICE_CONNECTED)
			logi_hidpp_recv_queue_notif(hdev, hidpp_report);
		else
			logi_dj_recv_queue_unknown_work(djrcv_dev);
	}

	spin_unlock_irqrestore(&djrcv_dev->lock, flags);

	return false;
}

static int logi_dj_raw_event(struct hid_device *hdev,
			     struct hid_report *report, u8 *data,
			     int size)
{
	struct dj_receiver_dev *djrcv_dev = hid_get_drvdata(hdev);
	dbg_hid("%s, size:%d\n", __func__, size);

	if (!djrcv_dev)
		return 0;

	if (!hdev->report_enum[HID_INPUT_REPORT].numbered) {

		if (djrcv_dev->unnumbered_application == HID_GD_KEYBOARD) {
			 
			data[1] = data[0];
			data[0] = REPORT_TYPE_KEYBOARD;

			logi_dj_recv_forward_input_report(hdev, data, size);

			 
			data[0] = data[1];
			data[1] = 0;
		}
		 
		if (djrcv_dev->unnumbered_application == HID_GD_MOUSE &&
		    size <= 8) {
			u8 mouse_report[9];

			 
			mouse_report[0] = REPORT_TYPE_MOUSE;
			memcpy(mouse_report + 1, data, size);
			logi_dj_recv_forward_input_report(hdev, mouse_report,
							  size + 1);
		}

		return false;
	}

	switch (data[0]) {
	case REPORT_ID_DJ_SHORT:
		if (size != DJREPORT_SHORT_LENGTH) {
			hid_err(hdev, "Short DJ report bad size (%d)", size);
			return false;
		}
		return logi_dj_dj_event(hdev, report, data, size);
	case REPORT_ID_DJ_LONG:
		if (size != DJREPORT_LONG_LENGTH) {
			hid_err(hdev, "Long DJ report bad size (%d)", size);
			return false;
		}
		return logi_dj_dj_event(hdev, report, data, size);
	case REPORT_ID_HIDPP_SHORT:
		if (size != HIDPP_REPORT_SHORT_LENGTH) {
			hid_err(hdev, "Short HID++ report bad size (%d)", size);
			return false;
		}
		return logi_dj_hidpp_event(hdev, report, data, size);
	case REPORT_ID_HIDPP_LONG:
		if (size != HIDPP_REPORT_LONG_LENGTH) {
			hid_err(hdev, "Long HID++ report bad size (%d)", size);
			return false;
		}
		return logi_dj_hidpp_event(hdev, report, data, size);
	}

	logi_dj_recv_forward_input_report(hdev, data, size);

	return false;
}

static int logi_dj_probe(struct hid_device *hdev,
			 const struct hid_device_id *id)
{
	struct hid_report_enum *rep_enum;
	struct hid_report *rep;
	struct dj_receiver_dev *djrcv_dev;
	struct usb_interface *intf;
	unsigned int no_dj_interfaces = 0;
	bool has_hidpp = false;
	unsigned long flags;
	int retval;

	 
	retval = hid_parse(hdev);
	if (retval) {
		hid_err(hdev, "%s: parse failed\n", __func__);
		return retval;
	}

	 
	switch (id->driver_data) {
	case recvr_type_dj:		no_dj_interfaces = 3; break;
	case recvr_type_hidpp:		no_dj_interfaces = 2; break;
	case recvr_type_gaming_hidpp:	no_dj_interfaces = 3; break;
	case recvr_type_mouse_only:	no_dj_interfaces = 2; break;
	case recvr_type_27mhz:		no_dj_interfaces = 2; break;
	case recvr_type_bluetooth:	no_dj_interfaces = 2; break;
	case recvr_type_dinovo:		no_dj_interfaces = 2; break;
	}
	if (hid_is_usb(hdev)) {
		intf = to_usb_interface(hdev->dev.parent);
		if (intf && intf->altsetting->desc.bInterfaceNumber >=
							no_dj_interfaces) {
			hdev->quirks |= HID_QUIRK_INPUT_PER_APP;
			return hid_hw_start(hdev, HID_CONNECT_DEFAULT);
		}
	}

	rep_enum = &hdev->report_enum[HID_INPUT_REPORT];

	 
	if (list_empty(&rep_enum->report_list))
		return -ENODEV;

	 
	list_for_each_entry(rep, &rep_enum->report_list, list) {
		if (rep->application == 0xff000001)
			has_hidpp = true;
	}

	 
	if (!has_hidpp && id->driver_data == recvr_type_dj)
		return -ENODEV;

	 
	rep = list_first_entry(&rep_enum->report_list, struct hid_report, list);
	djrcv_dev = dj_get_receiver_dev(hdev, id->driver_data,
					rep->application, has_hidpp);
	if (!djrcv_dev) {
		hid_err(hdev, "%s: dj_get_receiver_dev failed\n", __func__);
		return -ENOMEM;
	}

	if (!rep_enum->numbered)
		djrcv_dev->unnumbered_application = rep->application;

	 
	retval = hid_hw_start(hdev, HID_CONNECT_HIDRAW|HID_CONNECT_HIDDEV);
	if (retval) {
		hid_err(hdev, "%s: hid_hw_start returned error\n", __func__);
		goto hid_hw_start_fail;
	}

	if (has_hidpp) {
		retval = logi_dj_recv_switch_to_dj_mode(djrcv_dev, 0);
		if (retval < 0) {
			hid_err(hdev, "%s: logi_dj_recv_switch_to_dj_mode returned error:%d\n",
				__func__, retval);
			goto switch_to_dj_mode_fail;
		}
	}

	 
	retval = hid_hw_open(hdev);
	if (retval < 0) {
		hid_err(hdev, "%s: hid_hw_open returned error:%d\n",
			__func__, retval);
		goto llopen_failed;
	}

	 
	hid_device_io_start(hdev);

	if (has_hidpp) {
		spin_lock_irqsave(&djrcv_dev->lock, flags);
		djrcv_dev->ready = true;
		spin_unlock_irqrestore(&djrcv_dev->lock, flags);
		retval = logi_dj_recv_query_paired_devices(djrcv_dev);
		if (retval < 0) {
			hid_err(hdev, "%s: logi_dj_recv_query_paired_devices error:%d\n",
				__func__, retval);
			 
		}
	}

	return 0;

llopen_failed:
switch_to_dj_mode_fail:
	hid_hw_stop(hdev);

hid_hw_start_fail:
	dj_put_receiver_dev(hdev);
	return retval;
}

#ifdef CONFIG_PM
static int logi_dj_reset_resume(struct hid_device *hdev)
{
	int retval;
	struct dj_receiver_dev *djrcv_dev = hid_get_drvdata(hdev);

	if (!djrcv_dev || djrcv_dev->hidpp != hdev)
		return 0;

	retval = logi_dj_recv_switch_to_dj_mode(djrcv_dev, 0);
	if (retval < 0) {
		hid_err(hdev, "%s: logi_dj_recv_switch_to_dj_mode returned error:%d\n",
			__func__, retval);
	}

	return 0;
}
#endif

static void logi_dj_remove(struct hid_device *hdev)
{
	struct dj_receiver_dev *djrcv_dev = hid_get_drvdata(hdev);
	struct dj_device *dj_dev;
	unsigned long flags;
	int i;

	dbg_hid("%s\n", __func__);

	if (!djrcv_dev)
		return hid_hw_stop(hdev);

	 
	spin_lock_irqsave(&djrcv_dev->lock, flags);
	djrcv_dev->ready = false;
	spin_unlock_irqrestore(&djrcv_dev->lock, flags);

	cancel_work_sync(&djrcv_dev->work);

	hid_hw_close(hdev);
	hid_hw_stop(hdev);

	 
	for (i = 0; i < (DJ_MAX_PAIRED_DEVICES + DJ_DEVICE_INDEX_MIN); i++) {
		spin_lock_irqsave(&djrcv_dev->lock, flags);
		dj_dev = djrcv_dev->paired_dj_devices[i];
		djrcv_dev->paired_dj_devices[i] = NULL;
		spin_unlock_irqrestore(&djrcv_dev->lock, flags);
		if (dj_dev != NULL) {
			hid_destroy_device(dj_dev->hdev);
			kfree(dj_dev);
		}
	}

	dj_put_receiver_dev(hdev);
}

static const struct hid_device_id logi_dj_receivers[] = {
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_LOGITECH_UNIFYING_RECEIVER),
	 .driver_data = recvr_type_dj},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_LOGITECH_UNIFYING_RECEIVER_2),
	 .driver_data = recvr_type_dj},

	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
			 USB_DEVICE_ID_LOGITECH_NANO_RECEIVER),
	 .driver_data = recvr_type_mouse_only},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
			 USB_DEVICE_ID_LOGITECH_NANO_RECEIVER_2),
	 .driver_data = recvr_type_hidpp},

	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
			 USB_DEVICE_ID_LOGITECH_G700_RECEIVER),
	 .driver_data = recvr_type_gaming_hidpp},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		0xc537),
	 .driver_data = recvr_type_gaming_hidpp},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_LOGITECH_NANO_RECEIVER_LIGHTSPEED_1),
	 .driver_data = recvr_type_gaming_hidpp},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_LOGITECH_NANO_RECEIVER_POWERPLAY),
	 .driver_data = recvr_type_gaming_hidpp},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_LOGITECH_NANO_RECEIVER_LIGHTSPEED_1_1),
	 .driver_data = recvr_type_gaming_hidpp},

	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH, USB_DEVICE_ID_MX3000_RECEIVER),
	 .driver_data = recvr_type_27mhz},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_S510_RECEIVER_2),
	 .driver_data = recvr_type_27mhz},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_LOGITECH_27MHZ_MOUSE_RECEIVER),
	 .driver_data = recvr_type_27mhz},

	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_MX5000_RECEIVER_KBD_DEV),
	 .driver_data = recvr_type_bluetooth},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_MX5000_RECEIVER_MOUSE_DEV),
	 .driver_data = recvr_type_bluetooth},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_MX5500_RECEIVER_KBD_DEV),
	 .driver_data = recvr_type_bluetooth},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_MX5500_RECEIVER_MOUSE_DEV),
	 .driver_data = recvr_type_bluetooth},

	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_DINOVO_EDGE_RECEIVER_KBD_DEV),
	 .driver_data = recvr_type_dinovo},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_DINOVO_EDGE_RECEIVER_MOUSE_DEV),
	 .driver_data = recvr_type_dinovo},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_DINOVO_MINI_RECEIVER_KBD_DEV),
	 .driver_data = recvr_type_dinovo},
	{  
	  HID_USB_DEVICE(USB_VENDOR_ID_LOGITECH,
		USB_DEVICE_ID_DINOVO_MINI_RECEIVER_MOUSE_DEV),
	 .driver_data = recvr_type_dinovo},
	{}
};

MODULE_DEVICE_TABLE(hid, logi_dj_receivers);

static struct hid_driver logi_djreceiver_driver = {
	.name = "logitech-djreceiver",
	.id_table = logi_dj_receivers,
	.probe = logi_dj_probe,
	.remove = logi_dj_remove,
	.raw_event = logi_dj_raw_event,
#ifdef CONFIG_PM
	.reset_resume = logi_dj_reset_resume,
#endif
};

module_hid_driver(logi_djreceiver_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Logitech");
MODULE_AUTHOR("Nestor Lopez Casado");
MODULE_AUTHOR("nlopezcasad@logitech.com");
