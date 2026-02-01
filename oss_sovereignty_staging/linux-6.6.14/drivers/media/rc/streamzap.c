
 

#include <linux/device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <media/rc-core.h>

#define DRIVER_NAME	"streamzap"
#define DRIVER_DESC	"Streamzap Remote Control driver"

#define USB_STREAMZAP_VENDOR_ID		0x0e9c
#define USB_STREAMZAP_PRODUCT_ID	0x0000

 
static const struct usb_device_id streamzap_table[] = {
	 
	{ USB_DEVICE(USB_STREAMZAP_VENDOR_ID, USB_STREAMZAP_PRODUCT_ID) },
	 
	{ }
};

MODULE_DEVICE_TABLE(usb, streamzap_table);

#define SZ_PULSE_MASK 0xf0
#define SZ_SPACE_MASK 0x0f
#define SZ_TIMEOUT    0xff
#define SZ_RESOLUTION 256

 
#define SZ_BUF_LEN 128

enum StreamzapDecoderState {
	PulseSpace,
	FullPulse,
	FullSpace,
	IgnorePulse
};

 
struct streamzap_ir {
	 
	struct rc_dev *rdev;

	 
	struct device *dev;

	 
	struct urb		*urb_in;

	 
	unsigned char		*buf_in;
	dma_addr_t		dma_in;
	unsigned int		buf_in_len;

	 
	enum StreamzapDecoderState decoder_state;

	char			phys[64];
};


 
static int streamzap_probe(struct usb_interface *interface,
			   const struct usb_device_id *id);
static void streamzap_disconnect(struct usb_interface *interface);
static void streamzap_callback(struct urb *urb);
static int streamzap_suspend(struct usb_interface *intf, pm_message_t message);
static int streamzap_resume(struct usb_interface *intf);

 
static struct usb_driver streamzap_driver = {
	.name =		DRIVER_NAME,
	.probe =	streamzap_probe,
	.disconnect =	streamzap_disconnect,
	.suspend =	streamzap_suspend,
	.resume =	streamzap_resume,
	.id_table =	streamzap_table,
};

static void sz_push(struct streamzap_ir *sz, struct ir_raw_event rawir)
{
	dev_dbg(sz->dev, "Storing %s with duration %u us\n",
		(rawir.pulse ? "pulse" : "space"), rawir.duration);
	ir_raw_event_store_with_filter(sz->rdev, &rawir);
}

static void sz_push_full_pulse(struct streamzap_ir *sz,
			       unsigned char value)
{
	struct ir_raw_event rawir = {
		.pulse = true,
		.duration = value * SZ_RESOLUTION + SZ_RESOLUTION / 2,
	};

	sz_push(sz, rawir);
}

static void sz_push_half_pulse(struct streamzap_ir *sz,
			       unsigned char value)
{
	sz_push_full_pulse(sz, (value & SZ_PULSE_MASK) >> 4);
}

static void sz_push_full_space(struct streamzap_ir *sz,
			       unsigned char value)
{
	struct ir_raw_event rawir = {
		.pulse = false,
		.duration = value * SZ_RESOLUTION + SZ_RESOLUTION / 2,
	};

	sz_push(sz, rawir);
}

static void sz_push_half_space(struct streamzap_ir *sz,
			       unsigned long value)
{
	sz_push_full_space(sz, value & SZ_SPACE_MASK);
}

 
static void streamzap_callback(struct urb *urb)
{
	struct streamzap_ir *sz;
	unsigned int i;
	int len;

	if (!urb)
		return;

	sz = urb->context;
	len = urb->actual_length;

	switch (urb->status) {
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		 
		dev_err(sz->dev, "urb terminated, status: %d\n", urb->status);
		return;
	default:
		break;
	}

	dev_dbg(sz->dev, "%s: received urb, len %d\n", __func__, len);
	for (i = 0; i < len; i++) {
		dev_dbg(sz->dev, "sz->buf_in[%d]: %x\n",
			i, (unsigned char)sz->buf_in[i]);
		switch (sz->decoder_state) {
		case PulseSpace:
			if ((sz->buf_in[i] & SZ_PULSE_MASK) ==
				SZ_PULSE_MASK) {
				sz->decoder_state = FullPulse;
				continue;
			} else if ((sz->buf_in[i] & SZ_SPACE_MASK)
					== SZ_SPACE_MASK) {
				sz_push_half_pulse(sz, sz->buf_in[i]);
				sz->decoder_state = FullSpace;
				continue;
			} else {
				sz_push_half_pulse(sz, sz->buf_in[i]);
				sz_push_half_space(sz, sz->buf_in[i]);
			}
			break;
		case FullPulse:
			sz_push_full_pulse(sz, sz->buf_in[i]);
			sz->decoder_state = IgnorePulse;
			break;
		case FullSpace:
			if (sz->buf_in[i] == SZ_TIMEOUT) {
				struct ir_raw_event rawir = {
					.pulse = false,
					.duration = sz->rdev->timeout
				};
				sz_push(sz, rawir);
			} else {
				sz_push_full_space(sz, sz->buf_in[i]);
			}
			sz->decoder_state = PulseSpace;
			break;
		case IgnorePulse:
			if ((sz->buf_in[i] & SZ_SPACE_MASK) ==
				SZ_SPACE_MASK) {
				sz->decoder_state = FullSpace;
				continue;
			}
			sz_push_half_space(sz, sz->buf_in[i]);
			sz->decoder_state = PulseSpace;
			break;
		}
	}

	ir_raw_event_handle(sz->rdev);
	usb_submit_urb(urb, GFP_ATOMIC);
}

static struct rc_dev *streamzap_init_rc_dev(struct streamzap_ir *sz,
					    struct usb_device *usbdev)
{
	struct rc_dev *rdev;
	struct device *dev = sz->dev;
	int ret;

	rdev = rc_allocate_device(RC_DRIVER_IR_RAW);
	if (!rdev)
		goto out;

	usb_make_path(usbdev, sz->phys, sizeof(sz->phys));
	strlcat(sz->phys, "/input0", sizeof(sz->phys));

	rdev->device_name = "Streamzap PC Remote Infrared Receiver";
	rdev->input_phys = sz->phys;
	usb_to_input_id(usbdev, &rdev->input_id);
	rdev->dev.parent = dev;
	rdev->priv = sz;
	rdev->allowed_protocols = RC_PROTO_BIT_ALL_IR_DECODER;
	rdev->driver_name = DRIVER_NAME;
	rdev->map_name = RC_MAP_STREAMZAP;
	rdev->rx_resolution = SZ_RESOLUTION;

	ret = rc_register_device(rdev);
	if (ret < 0) {
		dev_err(dev, "remote input device register failed\n");
		goto out;
	}

	return rdev;

out:
	rc_free_device(rdev);
	return NULL;
}

 
static int streamzap_probe(struct usb_interface *intf,
			   const struct usb_device_id *id)
{
	struct usb_device *usbdev = interface_to_usbdev(intf);
	struct usb_endpoint_descriptor *endpoint;
	struct usb_host_interface *iface_host;
	struct streamzap_ir *sz = NULL;
	int retval = -ENOMEM;
	int pipe, maxp;

	 
	sz = kzalloc(sizeof(struct streamzap_ir), GFP_KERNEL);
	if (!sz)
		return -ENOMEM;

	 
	iface_host = intf->cur_altsetting;

	if (iface_host->desc.bNumEndpoints != 1) {
		dev_err(&intf->dev, "%s: Unexpected desc.bNumEndpoints (%d)\n",
			__func__, iface_host->desc.bNumEndpoints);
		retval = -ENODEV;
		goto free_sz;
	}

	endpoint = &iface_host->endpoint[0].desc;
	if (!usb_endpoint_dir_in(endpoint)) {
		dev_err(&intf->dev, "%s: endpoint doesn't match input device 02%02x\n",
			__func__, endpoint->bEndpointAddress);
		retval = -ENODEV;
		goto free_sz;
	}

	if (!usb_endpoint_xfer_int(endpoint)) {
		dev_err(&intf->dev, "%s: endpoint attributes don't match xfer 02%02x\n",
			__func__, endpoint->bmAttributes);
		retval = -ENODEV;
		goto free_sz;
	}

	pipe = usb_rcvintpipe(usbdev, endpoint->bEndpointAddress);
	maxp = usb_maxpacket(usbdev, pipe);

	if (maxp == 0) {
		dev_err(&intf->dev, "%s: endpoint Max Packet Size is 0!?!\n",
			__func__);
		retval = -ENODEV;
		goto free_sz;
	}

	 
	sz->buf_in = usb_alloc_coherent(usbdev, maxp, GFP_ATOMIC, &sz->dma_in);
	if (!sz->buf_in)
		goto free_sz;

	sz->urb_in = usb_alloc_urb(0, GFP_KERNEL);
	if (!sz->urb_in)
		goto free_buf_in;

	sz->dev = &intf->dev;
	sz->buf_in_len = maxp;

	sz->rdev = streamzap_init_rc_dev(sz, usbdev);
	if (!sz->rdev)
		goto rc_dev_fail;

	sz->decoder_state = PulseSpace;
	 
	sz->rdev->timeout = SZ_TIMEOUT * SZ_RESOLUTION;
	#if 0
	 
	 
	sz->min_timeout = SZ_TIMEOUT * SZ_RESOLUTION;
	sz->max_timeout = SZ_TIMEOUT * SZ_RESOLUTION;
	#endif

	 
	usb_fill_int_urb(sz->urb_in, usbdev, pipe, sz->buf_in,
			 maxp, streamzap_callback, sz, endpoint->bInterval);
	sz->urb_in->transfer_dma = sz->dma_in;
	sz->urb_in->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	usb_set_intfdata(intf, sz);

	if (usb_submit_urb(sz->urb_in, GFP_ATOMIC))
		dev_err(sz->dev, "urb submit failed\n");

	return 0;

rc_dev_fail:
	usb_free_urb(sz->urb_in);
free_buf_in:
	usb_free_coherent(usbdev, maxp, sz->buf_in, sz->dma_in);
free_sz:
	kfree(sz);

	return retval;
}

 
static void streamzap_disconnect(struct usb_interface *interface)
{
	struct streamzap_ir *sz = usb_get_intfdata(interface);
	struct usb_device *usbdev = interface_to_usbdev(interface);

	usb_set_intfdata(interface, NULL);

	if (!sz)
		return;

	rc_unregister_device(sz->rdev);
	usb_kill_urb(sz->urb_in);
	usb_free_urb(sz->urb_in);
	usb_free_coherent(usbdev, sz->buf_in_len, sz->buf_in, sz->dma_in);

	kfree(sz);
}

static int streamzap_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct streamzap_ir *sz = usb_get_intfdata(intf);

	usb_kill_urb(sz->urb_in);

	return 0;
}

static int streamzap_resume(struct usb_interface *intf)
{
	struct streamzap_ir *sz = usb_get_intfdata(intf);

	if (usb_submit_urb(sz->urb_in, GFP_NOIO)) {
		dev_err(sz->dev, "Error submitting urb\n");
		return -EIO;
	}

	return 0;
}

module_usb_driver(streamzap_driver);

MODULE_AUTHOR("Jarod Wilson <jarod@wilsonet.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
