 
 

#include "usbtv.h"

int usbtv_set_regs(struct usbtv *usbtv, const u16 regs[][2], int size)
{
	int ret;
	int pipe = usb_sndctrlpipe(usbtv->udev, 0);
	int i;

	for (i = 0; i < size; i++) {
		u16 index = regs[i][0];
		u16 value = regs[i][1];

		ret = usb_control_msg(usbtv->udev, pipe, USBTV_REQUEST_REG,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			value, index, NULL, 0, USB_CTRL_GET_TIMEOUT);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int usbtv_probe(struct usb_interface *intf,
	const struct usb_device_id *id)
{
	int ret;
	int size;
	struct device *dev = &intf->dev;
	struct usbtv *usbtv;
	struct usb_host_endpoint *ep;

	 
	if (intf->num_altsetting != 2)
		return -ENODEV;
	if (intf->altsetting[1].desc.bNumEndpoints != 4)
		return -ENODEV;

	ep = &intf->altsetting[1].endpoint[0];

	 
	size = usb_endpoint_maxp(&ep->desc);
	size = size * usb_endpoint_maxp_mult(&ep->desc);

	 
	usbtv = kzalloc(sizeof(struct usbtv), GFP_KERNEL);
	if (usbtv == NULL)
		return -ENOMEM;
	usbtv->dev = dev;
	usbtv->udev = usb_get_dev(interface_to_usbdev(intf));

	usbtv->iso_size = size;

	usb_set_intfdata(intf, usbtv);

	ret = usbtv_video_init(usbtv);
	if (ret < 0)
		goto usbtv_video_fail;

	ret = usbtv_audio_init(usbtv);
	if (ret < 0)
		goto usbtv_audio_fail;

	 
	v4l2_device_get(&usbtv->v4l2_dev);

	dev_info(dev, "Fushicai USBTV007 Audio-Video Grabber\n");
	return 0;

usbtv_audio_fail:
	 
	v4l2_device_get(&usbtv->v4l2_dev);
	 
	usbtv_video_free(usbtv);

usbtv_video_fail:
	usb_set_intfdata(intf, NULL);
	usb_put_dev(usbtv->udev);
	kfree(usbtv);

	return ret;
}

static void usbtv_disconnect(struct usb_interface *intf)
{
	struct usbtv *usbtv = usb_get_intfdata(intf);

	usb_set_intfdata(intf, NULL);

	if (!usbtv)
		return;

	usbtv_audio_free(usbtv);
	usbtv_video_free(usbtv);

	usb_put_dev(usbtv->udev);
	usbtv->udev = NULL;

	 
	v4l2_device_put(&usbtv->v4l2_dev);
}

static const struct usb_device_id usbtv_id_table[] = {
	{ USB_DEVICE(0x1b71, 0x3002) },
	{ USB_DEVICE(0x1f71, 0x3301) },
	{ USB_DEVICE(0x1f71, 0x3306) },
	{}
};
MODULE_DEVICE_TABLE(usb, usbtv_id_table);

MODULE_AUTHOR("Lubomir Rintel, Federico Simoncelli");
MODULE_DESCRIPTION("Fushicai USBTV007 Audio-Video Grabber Driver");
MODULE_LICENSE("Dual BSD/GPL");

static struct usb_driver usbtv_usb_driver = {
	.name = "usbtv",
	.id_table = usbtv_id_table,
	.probe = usbtv_probe,
	.disconnect = usbtv_disconnect,
};

module_usb_driver(usbtv_usb_driver);
