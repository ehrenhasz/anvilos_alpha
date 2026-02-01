
 

#include <asm/unaligned.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/tty_flip.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>

 
#define AIRCABLE_VID		0x16CA
#define AIRCABLE_USB_PID	0x1502

 
#define HCI_HEADER_LENGTH	0x4
#define TX_HEADER_0		0x20
#define TX_HEADER_1		0x29
#define RX_HEADER_0		0x00
#define RX_HEADER_1		0x20
#define HCI_COMPLETE_FRAME	64

 
#define THROTTLED		0x01
#define ACTUALLY_THROTTLED	0x02

#define DRIVER_AUTHOR "Naranjo, Manuel Francisco <naranjo.manuel@gmail.com>, Johan Hovold <jhovold@gmail.com>"
#define DRIVER_DESC "AIRcable USB Driver"

 
static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(AIRCABLE_VID, AIRCABLE_USB_PID) },
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

static int aircable_prepare_write_buffer(struct usb_serial_port *port,
						void *dest, size_t size)
{
	int count;
	unsigned char *buf = dest;

	count = kfifo_out_locked(&port->write_fifo, buf + HCI_HEADER_LENGTH,
					size - HCI_HEADER_LENGTH, &port->lock);
	buf[0] = TX_HEADER_0;
	buf[1] = TX_HEADER_1;
	put_unaligned_le16(count, &buf[2]);

	return count + HCI_HEADER_LENGTH;
}

static int aircable_calc_num_ports(struct usb_serial *serial,
					struct usb_serial_endpoints *epds)
{
	 
	if (epds->num_bulk_out == 0) {
		dev_dbg(&serial->interface->dev,
			"ignoring interface with no bulk-out endpoints\n");
		return -ENODEV;
	}

	return 1;
}

static int aircable_process_packet(struct usb_serial_port *port,
		int has_headers, char *packet, int len)
{
	if (has_headers) {
		len -= HCI_HEADER_LENGTH;
		packet += HCI_HEADER_LENGTH;
	}
	if (len <= 0) {
		dev_dbg(&port->dev, "%s - malformed packet\n", __func__);
		return 0;
	}

	tty_insert_flip_string(&port->port, packet, len);

	return len;
}

static void aircable_process_read_urb(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	char *data = urb->transfer_buffer;
	int has_headers;
	int count;
	int len;
	int i;

	has_headers = (urb->actual_length > 2 && data[0] == RX_HEADER_0);

	count = 0;
	for (i = 0; i < urb->actual_length; i += HCI_COMPLETE_FRAME) {
		len = min_t(int, urb->actual_length - i, HCI_COMPLETE_FRAME);
		count += aircable_process_packet(port, has_headers,
								&data[i], len);
	}

	if (count)
		tty_flip_buffer_push(&port->port);
}

static struct usb_serial_driver aircable_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"aircable",
	},
	.id_table = 		id_table,
	.bulk_out_size =	HCI_COMPLETE_FRAME,
	.calc_num_ports =	aircable_calc_num_ports,
	.process_read_urb =	aircable_process_read_urb,
	.prepare_write_buffer =	aircable_prepare_write_buffer,
	.throttle =		usb_serial_generic_throttle,
	.unthrottle =		usb_serial_generic_unthrottle,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&aircable_device, NULL
};

module_usb_serial_driver(serial_drivers, id_table);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
