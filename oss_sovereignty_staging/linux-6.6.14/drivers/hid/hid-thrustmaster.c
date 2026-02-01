
 
#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/module.h>

 
static const u8 setup_0[] = { 0x42, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const u8 setup_1[] = { 0x0a, 0x04, 0x90, 0x03, 0x00, 0x00, 0x00, 0x00 };
static const u8 setup_2[] = { 0x0a, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00 };
static const u8 setup_3[] = { 0x0a, 0x04, 0x12, 0x10, 0x00, 0x00, 0x00, 0x00 };
static const u8 setup_4[] = { 0x0a, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00 };
static const u8 *const setup_arr[] = { setup_0, setup_1, setup_2, setup_3, setup_4 };
static const unsigned int setup_arr_sizes[] = {
	ARRAY_SIZE(setup_0),
	ARRAY_SIZE(setup_1),
	ARRAY_SIZE(setup_2),
	ARRAY_SIZE(setup_3),
	ARRAY_SIZE(setup_4)
};
 
struct tm_wheel_info {
	uint16_t wheel_type;

	 
	uint16_t switch_value;

	char const *const wheel_name;
};

 
static const struct tm_wheel_info tm_wheels_infos[] = {
	{0x0306, 0x0006, "Thrustmaster T150RS"},
	{0x0200, 0x0005, "Thrustmaster T300RS (Missing Attachment)"},
	{0x0206, 0x0005, "Thrustmaster T300RS"},
	{0x0209, 0x0005, "Thrustmaster T300RS (Open Wheel Attachment)"},
	{0x020a, 0x0005, "Thrustmaster T300RS (Sparco R383 Mod)"},
	{0x0204, 0x0005, "Thrustmaster T300 Ferrari Alcantara Edition"},
	{0x0002, 0x0002, "Thrustmaster T500RS"}
	
};

static const uint8_t tm_wheels_infos_length = 7;

 
struct __packed tm_wheel_response
{
	 
	uint16_t type;

	union {
		struct __packed {
			uint16_t field0;
			uint16_t field1;
			 
			uint16_t model;

			uint16_t field2;
			uint16_t field3;
			uint16_t field4;
			uint16_t field5;
		} a;
		struct __packed {
			uint16_t field0;
			uint16_t field1;
			uint16_t model;
		} b;
	} data;
};

struct tm_wheel {
	struct usb_device *usb_dev;
	struct urb *urb;

	struct usb_ctrlrequest *model_request;
	struct tm_wheel_response *response;

	struct usb_ctrlrequest *change_request;
};

 
static const struct usb_ctrlrequest model_request = {
	.bRequestType = 0xc1,
	.bRequest = 73,
	.wValue = 0,
	.wIndex = 0,
	.wLength = cpu_to_le16(0x0010)
};

static const struct usb_ctrlrequest change_request = {
	.bRequestType = 0x41,
	.bRequest = 83,
	.wValue = 0, 
	.wIndex = 0,
	.wLength = 0
};

 
static void thrustmaster_interrupts(struct hid_device *hdev)
{
	int ret, trans, i, b_ep;
	u8 *send_buf = kmalloc(256, GFP_KERNEL);
	struct usb_host_endpoint *ep;
	struct device *dev = &hdev->dev;
	struct usb_interface *usbif = to_usb_interface(dev->parent);
	struct usb_device *usbdev = interface_to_usbdev(usbif);

	if (!send_buf) {
		hid_err(hdev, "failed allocating send buffer\n");
		return;
	}

	if (usbif->cur_altsetting->desc.bNumEndpoints < 2) {
		kfree(send_buf);
		hid_err(hdev, "Wrong number of endpoints?\n");
		return;
	}

	ep = &usbif->cur_altsetting->endpoint[1];
	b_ep = ep->desc.bEndpointAddress;

	for (i = 0; i < ARRAY_SIZE(setup_arr); ++i) {
		memcpy(send_buf, setup_arr[i], setup_arr_sizes[i]);

		ret = usb_interrupt_msg(usbdev,
			usb_sndintpipe(usbdev, b_ep),
			send_buf,
			setup_arr_sizes[i],
			&trans,
			USB_CTRL_SET_TIMEOUT);

		if (ret) {
			hid_err(hdev, "setup data couldn't be sent\n");
			kfree(send_buf);
			return;
		}
	}

	kfree(send_buf);
}

static void thrustmaster_change_handler(struct urb *urb)
{
	struct hid_device *hdev = urb->context;

	 
	if (urb->status == 0 || urb->status == -EPROTO || urb->status == -EPIPE)
		hid_info(hdev, "Success?! The wheel should have been initialized!\n");
	else
		hid_warn(hdev, "URB to change wheel mode seems to have failed with error %d\n", urb->status);
}

 
static void thrustmaster_model_handler(struct urb *urb)
{
	struct hid_device *hdev = urb->context;
	struct tm_wheel *tm_wheel = hid_get_drvdata(hdev);
	uint16_t model = 0;
	int i, ret;
	const struct tm_wheel_info *twi = NULL;

	if (urb->status) {
		hid_err(hdev, "URB to get model id failed with error %d\n", urb->status);
		return;
	}

	if (tm_wheel->response->type == cpu_to_le16(0x49))
		model = le16_to_cpu(tm_wheel->response->data.a.model);
	else if (tm_wheel->response->type == cpu_to_le16(0x47))
		model = le16_to_cpu(tm_wheel->response->data.b.model);
	else {
		hid_err(hdev, "Unknown packet type 0x%x, unable to proceed further with wheel init\n", tm_wheel->response->type);
		return;
	}

	for (i = 0; i < tm_wheels_infos_length && !twi; i++)
		if (tm_wheels_infos[i].wheel_type == model)
			twi = tm_wheels_infos + i;

	if (twi)
		hid_info(hdev, "Wheel with model id 0x%x is a %s\n", model, twi->wheel_name);
	else {
		hid_err(hdev, "Unknown wheel's model id 0x%x, unable to proceed further with wheel init\n", model);
		return;
	}

	tm_wheel->change_request->wValue = cpu_to_le16(twi->switch_value);
	usb_fill_control_urb(
		tm_wheel->urb,
		tm_wheel->usb_dev,
		usb_sndctrlpipe(tm_wheel->usb_dev, 0),
		(char *)tm_wheel->change_request,
		NULL, 0,  
		thrustmaster_change_handler,
		hdev
	);

	ret = usb_submit_urb(tm_wheel->urb, GFP_ATOMIC);
	if (ret)
		hid_err(hdev, "Error %d while submitting the change URB. I am unable to initialize this wheel...\n", ret);
}

static void thrustmaster_remove(struct hid_device *hdev)
{
	struct tm_wheel *tm_wheel = hid_get_drvdata(hdev);

	usb_kill_urb(tm_wheel->urb);

	kfree(tm_wheel->change_request);
	kfree(tm_wheel->response);
	kfree(tm_wheel->model_request);
	usb_free_urb(tm_wheel->urb);
	kfree(tm_wheel);

	hid_hw_stop(hdev);
}

 
static int thrustmaster_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret = 0;
	struct tm_wheel *tm_wheel = NULL;

	if (!hid_is_usb(hdev))
		return -EINVAL;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed with error %d\n", ret);
		goto error0;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT & ~HID_CONNECT_FF);
	if (ret) {
		hid_err(hdev, "hw start failed with error %d\n", ret);
		goto error0;
	}

	 
	tm_wheel = kzalloc(sizeof(struct tm_wheel), GFP_KERNEL);
	if (!tm_wheel) {
		ret = -ENOMEM;
		goto error1;
	}

	tm_wheel->urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!tm_wheel->urb) {
		ret = -ENOMEM;
		goto error2;
	}

	tm_wheel->model_request = kmemdup(&model_request,
					  sizeof(struct usb_ctrlrequest),
					  GFP_KERNEL);
	if (!tm_wheel->model_request) {
		ret = -ENOMEM;
		goto error3;
	}

	tm_wheel->response = kzalloc(sizeof(struct tm_wheel_response), GFP_KERNEL);
	if (!tm_wheel->response) {
		ret = -ENOMEM;
		goto error4;
	}

	tm_wheel->change_request = kmemdup(&change_request,
					   sizeof(struct usb_ctrlrequest),
					   GFP_KERNEL);
	if (!tm_wheel->change_request) {
		ret = -ENOMEM;
		goto error5;
	}

	tm_wheel->usb_dev = interface_to_usbdev(to_usb_interface(hdev->dev.parent));
	hid_set_drvdata(hdev, tm_wheel);

	thrustmaster_interrupts(hdev);

	usb_fill_control_urb(
		tm_wheel->urb,
		tm_wheel->usb_dev,
		usb_rcvctrlpipe(tm_wheel->usb_dev, 0),
		(char *)tm_wheel->model_request,
		tm_wheel->response,
		sizeof(struct tm_wheel_response),
		thrustmaster_model_handler,
		hdev
	);

	ret = usb_submit_urb(tm_wheel->urb, GFP_ATOMIC);
	if (ret) {
		hid_err(hdev, "Error %d while submitting the URB. I am unable to initialize this wheel...\n", ret);
		goto error6;
	}

	return ret;

error6: kfree(tm_wheel->change_request);
error5: kfree(tm_wheel->response);
error4: kfree(tm_wheel->model_request);
error3: usb_free_urb(tm_wheel->urb);
error2: kfree(tm_wheel);
error1: hid_hw_stop(hdev);
error0:
	return ret;
}

static const struct hid_device_id thrustmaster_devices[] = {
	{ HID_USB_DEVICE(0x044f, 0xb65d)},
	{}
};

MODULE_DEVICE_TABLE(hid, thrustmaster_devices);

static struct hid_driver thrustmaster_driver = {
	.name = "hid-thrustmaster",
	.id_table = thrustmaster_devices,
	.probe = thrustmaster_probe,
	.remove = thrustmaster_remove,
};

module_hid_driver(thrustmaster_driver);

MODULE_AUTHOR("Dario Pagani <dario.pagani.146+linuxk@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver to initialize some steering wheel joysticks from Thrustmaster");

