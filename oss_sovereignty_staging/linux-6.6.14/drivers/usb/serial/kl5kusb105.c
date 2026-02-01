
 

 

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/unaligned.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include "kl5kusb105.h"

#define DRIVER_AUTHOR "Utz-Uwe Haus <haus@uuhaus.de>, Johan Hovold <jhovold@gmail.com>"
#define DRIVER_DESC "KLSI KL5KUSB105 chipset USB->Serial Converter driver"


 
static int klsi_105_port_probe(struct usb_serial_port *port);
static void klsi_105_port_remove(struct usb_serial_port *port);
static int  klsi_105_open(struct tty_struct *tty, struct usb_serial_port *port);
static void klsi_105_close(struct usb_serial_port *port);
static void klsi_105_set_termios(struct tty_struct *tty,
				 struct usb_serial_port *port,
				 const struct ktermios *old_termios);
static int  klsi_105_tiocmget(struct tty_struct *tty);
static void klsi_105_process_read_urb(struct urb *urb);
static int klsi_105_prepare_write_buffer(struct usb_serial_port *port,
						void *dest, size_t size);

 
static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(PALMCONNECT_VID, PALMCONNECT_PID) },
	{ }		 
};

MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_serial_driver kl5kusb105d_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"kl5kusb105d",
	},
	.description =		"KL5KUSB105D / PalmConnect",
	.id_table =		id_table,
	.num_ports =		1,
	.bulk_out_size =	64,
	.open =			klsi_105_open,
	.close =		klsi_105_close,
	.set_termios =		klsi_105_set_termios,
	.tiocmget =		klsi_105_tiocmget,
	.port_probe =		klsi_105_port_probe,
	.port_remove =		klsi_105_port_remove,
	.throttle =		usb_serial_generic_throttle,
	.unthrottle =		usb_serial_generic_unthrottle,
	.process_read_urb =	klsi_105_process_read_urb,
	.prepare_write_buffer =	klsi_105_prepare_write_buffer,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&kl5kusb105d_device, NULL
};

struct klsi_105_port_settings {
	u8	pktlen;		 
	u8	baudrate;
	u8	databits;
	u8	unknown1;
	u8	unknown2;
};

struct klsi_105_private {
	struct klsi_105_port_settings	cfg;
	unsigned long			line_state;  
	spinlock_t			lock;
};


 


#define KLSI_TIMEOUT	 5000  

static int klsi_105_chg_port_settings(struct usb_serial_port *port,
				      struct klsi_105_port_settings *settings)
{
	int rc;

	rc = usb_control_msg_send(port->serial->dev,
				  0,
				  KL5KUSB105A_SIO_SET_DATA,
				  USB_TYPE_VENDOR | USB_DIR_OUT |
				  USB_RECIP_INTERFACE,
				  0,  
				  0,  
				  settings,
				  sizeof(struct klsi_105_port_settings),
				  KLSI_TIMEOUT,
				  GFP_KERNEL);
	if (rc)
		dev_err(&port->dev,
			"Change port settings failed (error = %d)\n", rc);

	dev_dbg(&port->dev,
		"pktlen %u, baudrate 0x%02x, databits %u, u1 %u, u2 %u\n",
		settings->pktlen, settings->baudrate, settings->databits,
		settings->unknown1, settings->unknown2);

	return rc;
}

 
static int klsi_105_get_line_state(struct usb_serial_port *port,
				   unsigned long *state)
{
	u16 status;
	int rc;

	rc = usb_control_msg_recv(port->serial->dev, 0,
				  KL5KUSB105A_SIO_POLL,
				  USB_TYPE_VENDOR | USB_DIR_IN,
				  0,  
				  0,  
				  &status, sizeof(status),
				  10000,
				  GFP_KERNEL);
	if (rc) {
		dev_err(&port->dev, "reading line status failed: %d\n", rc);
		return rc;
	}

	le16_to_cpus(&status);

	dev_dbg(&port->dev, "read status %04x\n", status);

	*state = ((status & KL5KUSB105A_DSR) ? TIOCM_DSR : 0) |
		 ((status & KL5KUSB105A_CTS) ? TIOCM_CTS : 0);

	return 0;
}


 

static int klsi_105_port_probe(struct usb_serial_port *port)
{
	struct klsi_105_private *priv;

	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	 
	priv->cfg.pktlen    = 5;
	priv->cfg.baudrate  = kl5kusb105a_sio_b9600;
	priv->cfg.databits  = kl5kusb105a_dtb_8;
	priv->cfg.unknown1  = 0;
	priv->cfg.unknown2  = 1;

	priv->line_state    = 0;

	spin_lock_init(&priv->lock);

	usb_set_serial_port_data(port, priv);

	return 0;
}

static void klsi_105_port_remove(struct usb_serial_port *port)
{
	struct klsi_105_private *priv;

	priv = usb_get_serial_port_data(port);
	kfree(priv);
}

static int  klsi_105_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct klsi_105_private *priv = usb_get_serial_port_data(port);
	int retval = 0;
	int rc;
	unsigned long line_state;
	struct klsi_105_port_settings cfg;
	unsigned long flags;

	 

	cfg.pktlen   = 5;
	cfg.baudrate = kl5kusb105a_sio_b9600;
	cfg.databits = kl5kusb105a_dtb_8;
	cfg.unknown1 = 0;
	cfg.unknown2 = 1;
	klsi_105_chg_port_settings(port, &cfg);

	spin_lock_irqsave(&priv->lock, flags);
	priv->cfg.pktlen   = cfg.pktlen;
	priv->cfg.baudrate = cfg.baudrate;
	priv->cfg.databits = cfg.databits;
	priv->cfg.unknown1 = cfg.unknown1;
	priv->cfg.unknown2 = cfg.unknown2;
	spin_unlock_irqrestore(&priv->lock, flags);

	 
	rc = usb_serial_generic_open(tty, port);
	if (rc)
		return rc;

	rc = usb_control_msg(port->serial->dev,
			     usb_sndctrlpipe(port->serial->dev, 0),
			     KL5KUSB105A_SIO_CONFIGURE,
			     USB_TYPE_VENDOR|USB_DIR_OUT|USB_RECIP_INTERFACE,
			     KL5KUSB105A_SIO_CONFIGURE_READ_ON,
			     0,  
			     NULL,
			     0,
			     KLSI_TIMEOUT);
	if (rc < 0) {
		dev_err(&port->dev, "Enabling read failed (error = %d)\n", rc);
		retval = rc;
		goto err_generic_close;
	} else
		dev_dbg(&port->dev, "%s - enabled reading\n", __func__);

	rc = klsi_105_get_line_state(port, &line_state);
	if (rc < 0) {
		retval = rc;
		goto err_disable_read;
	}

	spin_lock_irqsave(&priv->lock, flags);
	priv->line_state = line_state;
	spin_unlock_irqrestore(&priv->lock, flags);
	dev_dbg(&port->dev, "%s - read line state 0x%lx\n", __func__,
			line_state);

	return 0;

err_disable_read:
	usb_control_msg(port->serial->dev,
			     usb_sndctrlpipe(port->serial->dev, 0),
			     KL5KUSB105A_SIO_CONFIGURE,
			     USB_TYPE_VENDOR | USB_DIR_OUT,
			     KL5KUSB105A_SIO_CONFIGURE_READ_OFF,
			     0,  
			     NULL, 0,
			     KLSI_TIMEOUT);
err_generic_close:
	usb_serial_generic_close(port);

	return retval;
}

static void klsi_105_close(struct usb_serial_port *port)
{
	int rc;

	 
	rc = usb_control_msg(port->serial->dev,
			     usb_sndctrlpipe(port->serial->dev, 0),
			     KL5KUSB105A_SIO_CONFIGURE,
			     USB_TYPE_VENDOR | USB_DIR_OUT,
			     KL5KUSB105A_SIO_CONFIGURE_READ_OFF,
			     0,  
			     NULL, 0,
			     KLSI_TIMEOUT);
	if (rc < 0)
		dev_err(&port->dev, "failed to disable read: %d\n", rc);

	 
	usb_serial_generic_close(port);
}

 
#define KLSI_HDR_LEN		2
static int klsi_105_prepare_write_buffer(struct usb_serial_port *port,
						void *dest, size_t size)
{
	unsigned char *buf = dest;
	int count;

	count = kfifo_out_locked(&port->write_fifo, buf + KLSI_HDR_LEN, size,
								&port->lock);
	put_unaligned_le16(count, buf);

	return count + KLSI_HDR_LEN;
}

 
static void klsi_105_process_read_urb(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	unsigned char *data = urb->transfer_buffer;
	unsigned len;

	 
	if (!urb->actual_length)
		return;

	if (urb->actual_length <= KLSI_HDR_LEN) {
		dev_dbg(&port->dev, "%s - malformed packet\n", __func__);
		return;
	}

	len = get_unaligned_le16(data);
	if (len > urb->actual_length - KLSI_HDR_LEN) {
		dev_dbg(&port->dev, "%s - packet length mismatch\n", __func__);
		len = urb->actual_length - KLSI_HDR_LEN;
	}

	tty_insert_flip_string(&port->port, data + KLSI_HDR_LEN, len);
	tty_flip_buffer_push(&port->port);
}

static void klsi_105_set_termios(struct tty_struct *tty,
				 struct usb_serial_port *port,
				 const struct ktermios *old_termios)
{
	struct klsi_105_private *priv = usb_get_serial_port_data(port);
	struct device *dev = &port->dev;
	unsigned int iflag = tty->termios.c_iflag;
	unsigned int old_iflag = old_termios->c_iflag;
	unsigned int cflag = tty->termios.c_cflag;
	unsigned int old_cflag = old_termios->c_cflag;
	struct klsi_105_port_settings *cfg;
	unsigned long flags;
	speed_t baud;

	cfg = kmalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return;

	 
	spin_lock_irqsave(&priv->lock, flags);

	 
	baud = tty_get_baud_rate(tty);

	switch (baud) {
	case 0:  
		break;
	case 1200:
		priv->cfg.baudrate = kl5kusb105a_sio_b1200;
		break;
	case 2400:
		priv->cfg.baudrate = kl5kusb105a_sio_b2400;
		break;
	case 4800:
		priv->cfg.baudrate = kl5kusb105a_sio_b4800;
		break;
	case 9600:
		priv->cfg.baudrate = kl5kusb105a_sio_b9600;
		break;
	case 19200:
		priv->cfg.baudrate = kl5kusb105a_sio_b19200;
		break;
	case 38400:
		priv->cfg.baudrate = kl5kusb105a_sio_b38400;
		break;
	case 57600:
		priv->cfg.baudrate = kl5kusb105a_sio_b57600;
		break;
	case 115200:
		priv->cfg.baudrate = kl5kusb105a_sio_b115200;
		break;
	default:
		dev_dbg(dev, "unsupported baudrate, using 9600\n");
		priv->cfg.baudrate = kl5kusb105a_sio_b9600;
		baud = 9600;
		break;
	}

	 

	tty_encode_baud_rate(tty, baud, baud);

	if ((cflag & CSIZE) != (old_cflag & CSIZE)) {
		 
		switch (cflag & CSIZE) {
		case CS5:
			dev_dbg(dev, "%s - 5 bits/byte not supported\n", __func__);
			spin_unlock_irqrestore(&priv->lock, flags);
			goto err;
		case CS6:
			dev_dbg(dev, "%s - 6 bits/byte not supported\n", __func__);
			spin_unlock_irqrestore(&priv->lock, flags);
			goto err;
		case CS7:
			priv->cfg.databits = kl5kusb105a_dtb_7;
			break;
		case CS8:
			priv->cfg.databits = kl5kusb105a_dtb_8;
			break;
		default:
			dev_err(dev, "CSIZE was not CS5-CS8, using default of 8\n");
			priv->cfg.databits = kl5kusb105a_dtb_8;
			break;
		}
	}

	 
	if ((cflag & (PARENB|PARODD)) != (old_cflag & (PARENB|PARODD))
	    || (cflag & CSTOPB) != (old_cflag & CSTOPB)) {
		 
		tty->termios.c_cflag &= ~(PARENB|PARODD|CSTOPB);
	}
	 
	if ((iflag & IXOFF) != (old_iflag & IXOFF)
	    || (iflag & IXON) != (old_iflag & IXON)
	    ||  (cflag & CRTSCTS) != (old_cflag & CRTSCTS)) {
		 
		tty->termios.c_cflag &= ~CRTSCTS;
	}
	memcpy(cfg, &priv->cfg, sizeof(*cfg));
	spin_unlock_irqrestore(&priv->lock, flags);

	 
	klsi_105_chg_port_settings(port, cfg);
err:
	kfree(cfg);
}

static int klsi_105_tiocmget(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct klsi_105_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;
	int rc;
	unsigned long line_state;

	rc = klsi_105_get_line_state(port, &line_state);
	if (rc < 0) {
		dev_err(&port->dev,
			"Reading line control failed (error = %d)\n", rc);
		 
		return rc;
	}

	spin_lock_irqsave(&priv->lock, flags);
	priv->line_state = line_state;
	spin_unlock_irqrestore(&priv->lock, flags);
	dev_dbg(&port->dev, "%s - read line state 0x%lx\n", __func__, line_state);
	return (int)line_state;
}

module_usb_serial_driver(serial_drivers, id_table);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
