
 

#include <linux/kernel.h>
#include <linux/ioctl.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>

#define DRIVER_AUTHOR "Bart Hartgers <bart.hartgers+ark3116@gmail.com>"
#define DRIVER_DESC "USB ARK3116 serial/IrDA driver"
#define DRIVER_DEV_DESC "ARK3116 RS232/IrDA"
#define DRIVER_NAME "ark3116"

 
#define ARK_TIMEOUT 1000

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE(0x6547, 0x0232) },
	{ USB_DEVICE(0x18ec, 0x3118) },		 
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

static int is_irda(struct usb_serial *serial)
{
	struct usb_device *dev = serial->dev;
	if (le16_to_cpu(dev->descriptor.idVendor) == 0x18ec &&
			le16_to_cpu(dev->descriptor.idProduct) == 0x3118)
		return 1;
	return 0;
}

struct ark3116_private {
	int			irda;	 

	 
	struct mutex		hw_lock;

	int			quot;	 
	__u32			lcr;	 
	__u32			hcr;	 
	__u32			mcr;	 

	 
	spinlock_t		status_lock;
	__u32			msr;	 
	__u32			lsr;	 
};

static int ark3116_write_reg(struct usb_serial *serial,
			     unsigned reg, __u8 val)
{
	int result;
	  
	result = usb_control_msg(serial->dev,
				 usb_sndctrlpipe(serial->dev, 0),
				 0xfe, 0x40, val, reg,
				 NULL, 0, ARK_TIMEOUT);
	if (result)
		return result;

	return 0;
}

static int ark3116_read_reg(struct usb_serial *serial,
			    unsigned reg, unsigned char *buf)
{
	int result;
	 
	result = usb_control_msg(serial->dev,
				 usb_rcvctrlpipe(serial->dev, 0),
				 0xfe, 0xc0, 0, reg,
				 buf, 1, ARK_TIMEOUT);
	if (result < 1) {
		dev_err(&serial->interface->dev,
				"failed to read register %u: %d\n",
				reg, result);
		if (result >= 0)
			result = -EIO;

		return result;
	}

	return 0;
}

static inline int calc_divisor(int bps)
{
	 
	return (12000000 + 2*bps) / (4*bps);
}

static int ark3116_port_probe(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;
	struct ark3116_private *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	mutex_init(&priv->hw_lock);
	spin_lock_init(&priv->status_lock);

	priv->irda = is_irda(serial);

	usb_set_serial_port_data(port, priv);

	 
	ark3116_write_reg(serial, UART_IER, 0);
	 
	ark3116_write_reg(serial, UART_FCR, 0);
	 
	priv->hcr = 0;
	ark3116_write_reg(serial, 0x8     , 0);
	 
	priv->mcr = 0;
	ark3116_write_reg(serial, UART_MCR, 0);

	if (!(priv->irda)) {
		ark3116_write_reg(serial, 0xb , 0);
	} else {
		ark3116_write_reg(serial, 0xb , 1);
		ark3116_write_reg(serial, 0xc , 0);
		ark3116_write_reg(serial, 0xd , 0x41);
		ark3116_write_reg(serial, 0xa , 1);
	}

	 
	ark3116_write_reg(serial, UART_LCR, UART_LCR_DLAB);

	 
	priv->quot = calc_divisor(9600);
	ark3116_write_reg(serial, UART_DLL, priv->quot & 0xff);
	ark3116_write_reg(serial, UART_DLM, (priv->quot>>8) & 0xff);

	priv->lcr = UART_LCR_WLEN8;
	ark3116_write_reg(serial, UART_LCR, UART_LCR_WLEN8);

	ark3116_write_reg(serial, 0xe, 0);

	if (priv->irda)
		ark3116_write_reg(serial, 0x9, 0);

	dev_info(&port->dev, "using %s mode\n", priv->irda ? "IrDA" : "RS232");

	return 0;
}

static void ark3116_port_remove(struct usb_serial_port *port)
{
	struct ark3116_private *priv = usb_get_serial_port_data(port);

	 
	mutex_destroy(&priv->hw_lock);
	kfree(priv);
}

static void ark3116_set_termios(struct tty_struct *tty,
				struct usb_serial_port *port,
				const struct ktermios *old_termios)
{
	struct usb_serial *serial = port->serial;
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	struct ktermios *termios = &tty->termios;
	unsigned int cflag = termios->c_cflag;
	int bps = tty_get_baud_rate(tty);
	int quot;
	__u8 lcr, hcr, eval;

	 
	lcr = UART_LCR_WLEN(tty_get_char_size(cflag));

	if (cflag & CSTOPB)
		lcr |= UART_LCR_STOP;
	if (cflag & PARENB)
		lcr |= UART_LCR_PARITY;
	if (!(cflag & PARODD))
		lcr |= UART_LCR_EPAR;
	if (cflag & CMSPAR)
		lcr |= UART_LCR_SPAR;

	 
	hcr = (cflag & CRTSCTS) ? 0x03 : 0x00;

	 
	dev_dbg(&port->dev, "%s - setting bps to %d\n", __func__, bps);
	eval = 0;
	switch (bps) {
	case 0:
		quot = calc_divisor(9600);
		break;
	default:
		if ((bps < 75) || (bps > 3000000))
			bps = 9600;
		quot = calc_divisor(bps);
		break;
	case 460800:
		eval = 1;
		quot = calc_divisor(bps);
		break;
	case 921600:
		eval = 2;
		quot = calc_divisor(bps);
		break;
	}

	 
	mutex_lock(&priv->hw_lock);

	 
	lcr |= (priv->lcr & UART_LCR_SBC);

	dev_dbg(&port->dev, "%s - setting hcr:0x%02x,lcr:0x%02x,quot:%d\n",
		__func__, hcr, lcr, quot);

	 
	if (priv->hcr != hcr) {
		priv->hcr = hcr;
		ark3116_write_reg(serial, 0x8, hcr);
	}

	 
	if (priv->quot != quot) {
		priv->quot = quot;
		priv->lcr = lcr;  

		 
		ark3116_write_reg(serial, UART_FCR, 0);

		ark3116_write_reg(serial, UART_LCR,
				  lcr|UART_LCR_DLAB);
		ark3116_write_reg(serial, UART_DLL, quot & 0xff);
		ark3116_write_reg(serial, UART_DLM, (quot>>8) & 0xff);

		 
		ark3116_write_reg(serial, UART_LCR, lcr);
		 
		ark3116_write_reg(serial, 0xe, eval);

		 
		ark3116_write_reg(serial, UART_FCR, UART_FCR_DMA_SELECT);
	} else if (priv->lcr != lcr) {
		priv->lcr = lcr;
		ark3116_write_reg(serial, UART_LCR, lcr);
	}

	mutex_unlock(&priv->hw_lock);

	 
	if (I_IXOFF(tty) || I_IXON(tty)) {
		dev_warn(&port->dev,
				"software flow control not implemented\n");
	}

	 
	if (tty_termios_baud_rate(termios))
		tty_termios_encode_baud_rate(termios, bps, bps);
}

static void ark3116_close(struct usb_serial_port *port)
{
	struct usb_serial *serial = port->serial;

	 
	ark3116_write_reg(serial, UART_FCR, 0);

	 
	ark3116_write_reg(serial, UART_IER, 0);

	usb_serial_generic_close(port);

	usb_kill_urb(port->interrupt_in_urb);
}

static int ark3116_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	struct usb_serial *serial = port->serial;
	unsigned char *buf;
	int result;

	buf = kmalloc(1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	result = usb_serial_generic_open(tty, port);
	if (result) {
		dev_dbg(&port->dev,
			"%s - usb_serial_generic_open failed: %d\n",
			__func__, result);
		goto err_free;
	}

	 
	ark3116_read_reg(serial, UART_RX, buf);

	 
	result = ark3116_read_reg(serial, UART_MSR, buf);
	if (result)
		goto err_close;
	priv->msr = *buf;

	 
	result = ark3116_read_reg(serial, UART_LSR, buf);
	if (result)
		goto err_close;
	priv->lsr = *buf;

	result = usb_submit_urb(port->interrupt_in_urb, GFP_KERNEL);
	if (result) {
		dev_err(&port->dev, "submit irq_in urb failed %d\n",
			result);
		goto err_close;
	}

	 
	ark3116_write_reg(port->serial, UART_IER, UART_IER_MSI|UART_IER_RLSI);

	 
	ark3116_write_reg(port->serial, UART_FCR, UART_FCR_DMA_SELECT);

	 
	if (tty)
		ark3116_set_termios(tty, port, NULL);

	kfree(buf);

	return 0;

err_close:
	usb_serial_generic_close(port);
err_free:
	kfree(buf);

	return result;
}

static int ark3116_tiocmget(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	__u32 status;
	__u32 ctrl;
	unsigned long flags;

	mutex_lock(&priv->hw_lock);
	ctrl = priv->mcr;
	mutex_unlock(&priv->hw_lock);

	spin_lock_irqsave(&priv->status_lock, flags);
	status = priv->msr;
	spin_unlock_irqrestore(&priv->status_lock, flags);

	return  (status & UART_MSR_DSR  ? TIOCM_DSR  : 0) |
		(status & UART_MSR_CTS  ? TIOCM_CTS  : 0) |
		(status & UART_MSR_RI   ? TIOCM_RI   : 0) |
		(status & UART_MSR_DCD  ? TIOCM_CD   : 0) |
		(ctrl   & UART_MCR_DTR  ? TIOCM_DTR  : 0) |
		(ctrl   & UART_MCR_RTS  ? TIOCM_RTS  : 0) |
		(ctrl   & UART_MCR_OUT1 ? TIOCM_OUT1 : 0) |
		(ctrl   & UART_MCR_OUT2 ? TIOCM_OUT2 : 0);
}

static int ark3116_tiocmset(struct tty_struct *tty,
			unsigned set, unsigned clr)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ark3116_private *priv = usb_get_serial_port_data(port);

	 

	mutex_lock(&priv->hw_lock);

	if (set & TIOCM_RTS)
		priv->mcr |= UART_MCR_RTS;
	if (set & TIOCM_DTR)
		priv->mcr |= UART_MCR_DTR;
	if (set & TIOCM_OUT1)
		priv->mcr |= UART_MCR_OUT1;
	if (set & TIOCM_OUT2)
		priv->mcr |= UART_MCR_OUT2;
	if (clr & TIOCM_RTS)
		priv->mcr &= ~UART_MCR_RTS;
	if (clr & TIOCM_DTR)
		priv->mcr &= ~UART_MCR_DTR;
	if (clr & TIOCM_OUT1)
		priv->mcr &= ~UART_MCR_OUT1;
	if (clr & TIOCM_OUT2)
		priv->mcr &= ~UART_MCR_OUT2;

	ark3116_write_reg(port->serial, UART_MCR, priv->mcr);

	mutex_unlock(&priv->hw_lock);

	return 0;
}

static int ark3116_break_ctl(struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port = tty->driver_data;
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	int ret;

	 
	mutex_lock(&priv->hw_lock);

	if (break_state)
		priv->lcr |= UART_LCR_SBC;
	else
		priv->lcr &= ~UART_LCR_SBC;

	ret = ark3116_write_reg(port->serial, UART_LCR, priv->lcr);

	mutex_unlock(&priv->hw_lock);

	return ret;
}

static void ark3116_update_msr(struct usb_serial_port *port, __u8 msr)
{
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;

	spin_lock_irqsave(&priv->status_lock, flags);
	priv->msr = msr;
	spin_unlock_irqrestore(&priv->status_lock, flags);

	if (msr & UART_MSR_ANY_DELTA) {
		 
		if (msr & UART_MSR_DCTS)
			port->icount.cts++;
		if (msr & UART_MSR_DDSR)
			port->icount.dsr++;
		if (msr & UART_MSR_DDCD)
			port->icount.dcd++;
		if (msr & UART_MSR_TERI)
			port->icount.rng++;
		wake_up_interruptible(&port->port.delta_msr_wait);
	}
}

static void ark3116_update_lsr(struct usb_serial_port *port, __u8 lsr)
{
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	unsigned long flags;

	spin_lock_irqsave(&priv->status_lock, flags);
	 
	priv->lsr |= lsr;
	spin_unlock_irqrestore(&priv->status_lock, flags);

	if (lsr&UART_LSR_BRK_ERROR_BITS) {
		if (lsr & UART_LSR_BI)
			port->icount.brk++;
		if (lsr & UART_LSR_FE)
			port->icount.frame++;
		if (lsr & UART_LSR_PE)
			port->icount.parity++;
		if (lsr & UART_LSR_OE)
			port->icount.overrun++;
	}
}

static void ark3116_read_int_callback(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	int status = urb->status;
	const __u8 *data = urb->transfer_buffer;
	int result;

	switch (status) {
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		 
		dev_dbg(&port->dev, "%s - urb shutting down with status: %d\n",
			__func__, status);
		return;
	default:
		dev_dbg(&port->dev, "%s - nonzero urb status received: %d\n",
			__func__, status);
		break;
	case 0:  
		 
		if ((urb->actual_length == 4) && (data[0] == 0xe8)) {
			const __u8 id = data[1]&UART_IIR_ID;
			dev_dbg(&port->dev, "%s: iir=%02x\n", __func__, data[1]);
			if (id == UART_IIR_MSI) {
				dev_dbg(&port->dev, "%s: msr=%02x\n",
					__func__, data[3]);
				ark3116_update_msr(port, data[3]);
				break;
			} else if (id == UART_IIR_RLSI) {
				dev_dbg(&port->dev, "%s: lsr=%02x\n",
					__func__, data[2]);
				ark3116_update_lsr(port, data[2]);
				break;
			}
		}
		 
		usb_serial_debug_data(&port->dev, __func__,
				      urb->actual_length,
				      urb->transfer_buffer);
		break;
	}

	result = usb_submit_urb(urb, GFP_ATOMIC);
	if (result)
		dev_err(&port->dev, "failed to resubmit interrupt urb: %d\n",
			result);
}


 
static void ark3116_process_read_urb(struct urb *urb)
{
	struct usb_serial_port *port = urb->context;
	struct ark3116_private *priv = usb_get_serial_port_data(port);
	unsigned char *data = urb->transfer_buffer;
	char tty_flag = TTY_NORMAL;
	unsigned long flags;
	__u32 lsr;

	 
	spin_lock_irqsave(&priv->status_lock, flags);
	lsr = priv->lsr;
	priv->lsr &= ~UART_LSR_BRK_ERROR_BITS;
	spin_unlock_irqrestore(&priv->status_lock, flags);

	if (!urb->actual_length)
		return;

	if (lsr & UART_LSR_BRK_ERROR_BITS) {
		if (lsr & UART_LSR_BI)
			tty_flag = TTY_BREAK;
		else if (lsr & UART_LSR_PE)
			tty_flag = TTY_PARITY;
		else if (lsr & UART_LSR_FE)
			tty_flag = TTY_FRAME;

		 
		if (lsr & UART_LSR_OE)
			tty_insert_flip_char(&port->port, 0, TTY_OVERRUN);
	}
	tty_insert_flip_string_fixed_flag(&port->port, data, tty_flag,
							urb->actual_length);
	tty_flip_buffer_push(&port->port);
}

static struct usb_serial_driver ark3116_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"ark3116",
	},
	.id_table =		id_table,
	.num_ports =		1,
	.num_bulk_in =		1,
	.num_bulk_out =		1,
	.num_interrupt_in =	1,
	.port_probe =		ark3116_port_probe,
	.port_remove =		ark3116_port_remove,
	.set_termios =		ark3116_set_termios,
	.tiocmget =		ark3116_tiocmget,
	.tiocmset =		ark3116_tiocmset,
	.tiocmiwait =		usb_serial_generic_tiocmiwait,
	.get_icount =		usb_serial_generic_get_icount,
	.open =			ark3116_open,
	.close =		ark3116_close,
	.break_ctl = 		ark3116_break_ctl,
	.read_int_callback = 	ark3116_read_int_callback,
	.process_read_urb =	ark3116_process_read_urb,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&ark3116_device, NULL
};

module_usb_serial_driver(serial_drivers, id_table);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

 
