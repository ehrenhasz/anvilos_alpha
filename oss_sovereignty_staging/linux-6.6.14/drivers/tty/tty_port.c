
 

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/serdev.h>
#include "tty.h"

static size_t tty_port_default_receive_buf(struct tty_port *port, const u8 *p,
					   const u8 *f, size_t count)
{
	struct tty_struct *tty;
	struct tty_ldisc *ld;

	tty = READ_ONCE(port->itty);
	if (!tty)
		return 0;

	ld = tty_ldisc_ref(tty);
	if (!ld)
		return 0;

	count = tty_ldisc_receive_buf(ld, p, f, count);

	tty_ldisc_deref(ld);

	return count;
}

static void tty_port_default_lookahead_buf(struct tty_port *port, const u8 *p,
					   const u8 *f, size_t count)
{
	struct tty_struct *tty;
	struct tty_ldisc *ld;

	tty = READ_ONCE(port->itty);
	if (!tty)
		return;

	ld = tty_ldisc_ref(tty);
	if (!ld)
		return;

	if (ld->ops->lookahead_buf)
		ld->ops->lookahead_buf(ld->tty, p, f, count);

	tty_ldisc_deref(ld);
}

static void tty_port_default_wakeup(struct tty_port *port)
{
	struct tty_struct *tty = tty_port_tty_get(port);

	if (tty) {
		tty_wakeup(tty);
		tty_kref_put(tty);
	}
}

const struct tty_port_client_operations tty_port_default_client_ops = {
	.receive_buf = tty_port_default_receive_buf,
	.lookahead_buf = tty_port_default_lookahead_buf,
	.write_wakeup = tty_port_default_wakeup,
};
EXPORT_SYMBOL_GPL(tty_port_default_client_ops);

 
void tty_port_init(struct tty_port *port)
{
	memset(port, 0, sizeof(*port));
	tty_buffer_init(port);
	init_waitqueue_head(&port->open_wait);
	init_waitqueue_head(&port->delta_msr_wait);
	mutex_init(&port->mutex);
	mutex_init(&port->buf_mutex);
	spin_lock_init(&port->lock);
	port->close_delay = (50 * HZ) / 100;
	port->closing_wait = (3000 * HZ) / 100;
	port->client_ops = &tty_port_default_client_ops;
	kref_init(&port->kref);
}
EXPORT_SYMBOL(tty_port_init);

 
void tty_port_link_device(struct tty_port *port,
		struct tty_driver *driver, unsigned index)
{
	if (WARN_ON(index >= driver->num))
		return;
	driver->ports[index] = port;
}
EXPORT_SYMBOL_GPL(tty_port_link_device);

 
struct device *tty_port_register_device(struct tty_port *port,
		struct tty_driver *driver, unsigned index,
		struct device *device)
{
	return tty_port_register_device_attr(port, driver, index, device, NULL, NULL);
}
EXPORT_SYMBOL_GPL(tty_port_register_device);

 
struct device *tty_port_register_device_attr(struct tty_port *port,
		struct tty_driver *driver, unsigned index,
		struct device *device, void *drvdata,
		const struct attribute_group **attr_grp)
{
	tty_port_link_device(port, driver, index);
	return tty_register_device_attr(driver, index, device, drvdata,
			attr_grp);
}
EXPORT_SYMBOL_GPL(tty_port_register_device_attr);

 
struct device *tty_port_register_device_attr_serdev(struct tty_port *port,
		struct tty_driver *driver, unsigned index,
		struct device *device, void *drvdata,
		const struct attribute_group **attr_grp)
{
	struct device *dev;

	tty_port_link_device(port, driver, index);

	dev = serdev_tty_port_register(port, device, driver, index);
	if (PTR_ERR(dev) != -ENODEV) {
		 
		return dev;
	}

	return tty_register_device_attr(driver, index, device, drvdata,
			attr_grp);
}
EXPORT_SYMBOL_GPL(tty_port_register_device_attr_serdev);

 
struct device *tty_port_register_device_serdev(struct tty_port *port,
		struct tty_driver *driver, unsigned index,
		struct device *device)
{
	return tty_port_register_device_attr_serdev(port, driver, index,
			device, NULL, NULL);
}
EXPORT_SYMBOL_GPL(tty_port_register_device_serdev);

 
void tty_port_unregister_device(struct tty_port *port,
		struct tty_driver *driver, unsigned index)
{
	int ret;

	ret = serdev_tty_port_unregister(port);
	if (ret == 0)
		return;

	tty_unregister_device(driver, index);
}
EXPORT_SYMBOL_GPL(tty_port_unregister_device);

int tty_port_alloc_xmit_buf(struct tty_port *port)
{
	 
	mutex_lock(&port->buf_mutex);
	if (port->xmit_buf == NULL) {
		port->xmit_buf = (unsigned char *)get_zeroed_page(GFP_KERNEL);
		if (port->xmit_buf)
			kfifo_init(&port->xmit_fifo, port->xmit_buf, PAGE_SIZE);
	}
	mutex_unlock(&port->buf_mutex);
	if (port->xmit_buf == NULL)
		return -ENOMEM;
	return 0;
}
EXPORT_SYMBOL(tty_port_alloc_xmit_buf);

void tty_port_free_xmit_buf(struct tty_port *port)
{
	mutex_lock(&port->buf_mutex);
	free_page((unsigned long)port->xmit_buf);
	port->xmit_buf = NULL;
	INIT_KFIFO(port->xmit_fifo);
	mutex_unlock(&port->buf_mutex);
}
EXPORT_SYMBOL(tty_port_free_xmit_buf);

 
void tty_port_destroy(struct tty_port *port)
{
	tty_buffer_cancel_work(port);
	tty_buffer_free_all(port);
}
EXPORT_SYMBOL(tty_port_destroy);

static void tty_port_destructor(struct kref *kref)
{
	struct tty_port *port = container_of(kref, struct tty_port, kref);

	 
	if (WARN_ON(port->itty))
		return;
	free_page((unsigned long)port->xmit_buf);
	tty_port_destroy(port);
	if (port->ops && port->ops->destruct)
		port->ops->destruct(port);
	else
		kfree(port);
}

 
void tty_port_put(struct tty_port *port)
{
	if (port)
		kref_put(&port->kref, tty_port_destructor);
}
EXPORT_SYMBOL(tty_port_put);

 
struct tty_struct *tty_port_tty_get(struct tty_port *port)
{
	unsigned long flags;
	struct tty_struct *tty;

	spin_lock_irqsave(&port->lock, flags);
	tty = tty_kref_get(port->tty);
	spin_unlock_irqrestore(&port->lock, flags);
	return tty;
}
EXPORT_SYMBOL(tty_port_tty_get);

 
void tty_port_tty_set(struct tty_port *port, struct tty_struct *tty)
{
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	tty_kref_put(port->tty);
	port->tty = tty_kref_get(tty);
	spin_unlock_irqrestore(&port->lock, flags);
}
EXPORT_SYMBOL(tty_port_tty_set);

 
static void tty_port_shutdown(struct tty_port *port, struct tty_struct *tty)
{
	mutex_lock(&port->mutex);
	if (port->console)
		goto out;

	if (tty_port_initialized(port)) {
		tty_port_set_initialized(port, false);
		 
		if (tty && C_HUPCL(tty))
			tty_port_lower_dtr_rts(port);

		if (port->ops->shutdown)
			port->ops->shutdown(port);
	}
out:
	mutex_unlock(&port->mutex);
}

 
void tty_port_hangup(struct tty_port *port)
{
	struct tty_struct *tty;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	port->count = 0;
	tty = port->tty;
	if (tty)
		set_bit(TTY_IO_ERROR, &tty->flags);
	port->tty = NULL;
	spin_unlock_irqrestore(&port->lock, flags);
	tty_port_set_active(port, false);
	tty_port_shutdown(port, tty);
	tty_kref_put(tty);
	wake_up_interruptible(&port->open_wait);
	wake_up_interruptible(&port->delta_msr_wait);
}
EXPORT_SYMBOL(tty_port_hangup);

 
void tty_port_tty_hangup(struct tty_port *port, bool check_clocal)
{
	struct tty_struct *tty = tty_port_tty_get(port);

	if (tty && (!check_clocal || !C_CLOCAL(tty)))
		tty_hangup(tty);
	tty_kref_put(tty);
}
EXPORT_SYMBOL_GPL(tty_port_tty_hangup);

 
void tty_port_tty_wakeup(struct tty_port *port)
{
	port->client_ops->write_wakeup(port);
}
EXPORT_SYMBOL_GPL(tty_port_tty_wakeup);

 
bool tty_port_carrier_raised(struct tty_port *port)
{
	if (port->ops->carrier_raised == NULL)
		return true;
	return port->ops->carrier_raised(port);
}
EXPORT_SYMBOL(tty_port_carrier_raised);

 
void tty_port_raise_dtr_rts(struct tty_port *port)
{
	if (port->ops->dtr_rts)
		port->ops->dtr_rts(port, true);
}
EXPORT_SYMBOL(tty_port_raise_dtr_rts);

 
void tty_port_lower_dtr_rts(struct tty_port *port)
{
	if (port->ops->dtr_rts)
		port->ops->dtr_rts(port, false);
}
EXPORT_SYMBOL(tty_port_lower_dtr_rts);

 
int tty_port_block_til_ready(struct tty_port *port,
				struct tty_struct *tty, struct file *filp)
{
	int do_clocal = 0, retval;
	unsigned long flags;
	DEFINE_WAIT(wait);

	 
	if (tty_io_error(tty)) {
		tty_port_set_active(port, true);
		return 0;
	}
	if (filp == NULL || (filp->f_flags & O_NONBLOCK)) {
		 
		if (C_BAUD(tty))
			tty_port_raise_dtr_rts(port);
		tty_port_set_active(port, true);
		return 0;
	}

	if (C_CLOCAL(tty))
		do_clocal = 1;

	 

	retval = 0;

	 
	spin_lock_irqsave(&port->lock, flags);
	port->count--;
	port->blocked_open++;
	spin_unlock_irqrestore(&port->lock, flags);

	while (1) {
		 
		if (C_BAUD(tty) && tty_port_initialized(port))
			tty_port_raise_dtr_rts(port);

		prepare_to_wait(&port->open_wait, &wait, TASK_INTERRUPTIBLE);
		 
		if (tty_hung_up_p(filp) || !tty_port_initialized(port)) {
			if (port->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;
			break;
		}
		 
		if (do_clocal || tty_port_carrier_raised(port))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		tty_unlock(tty);
		schedule();
		tty_lock(tty);
	}
	finish_wait(&port->open_wait, &wait);

	 
	spin_lock_irqsave(&port->lock, flags);
	if (!tty_hung_up_p(filp))
		port->count++;
	port->blocked_open--;
	spin_unlock_irqrestore(&port->lock, flags);
	if (retval == 0)
		tty_port_set_active(port, true);
	return retval;
}
EXPORT_SYMBOL(tty_port_block_til_ready);

static void tty_port_drain_delay(struct tty_port *port, struct tty_struct *tty)
{
	unsigned int bps = tty_get_baud_rate(tty);
	long timeout;

	if (bps > 1200) {
		timeout = (HZ * 10 * port->drain_delay) / bps;
		timeout = max_t(long, timeout, HZ / 10);
	} else {
		timeout = 2 * HZ;
	}
	schedule_timeout_interruptible(timeout);
}

 
int tty_port_close_start(struct tty_port *port,
				struct tty_struct *tty, struct file *filp)
{
	unsigned long flags;

	if (tty_hung_up_p(filp))
		return 0;

	spin_lock_irqsave(&port->lock, flags);
	if (tty->count == 1 && port->count != 1) {
		tty_warn(tty, "%s: tty->count = 1 port count = %d\n", __func__,
			 port->count);
		port->count = 1;
	}
	if (--port->count < 0) {
		tty_warn(tty, "%s: bad port count (%d)\n", __func__,
			 port->count);
		port->count = 0;
	}

	if (port->count) {
		spin_unlock_irqrestore(&port->lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&port->lock, flags);

	tty->closing = 1;

	if (tty_port_initialized(port)) {
		 
		if (tty->flow.tco_stopped)
			tty_driver_flush_buffer(tty);
		if (port->closing_wait != ASYNC_CLOSING_WAIT_NONE)
			tty_wait_until_sent(tty, port->closing_wait);
		if (port->drain_delay)
			tty_port_drain_delay(port, tty);
	}
	 
	tty_ldisc_flush(tty);

	 
	return 1;
}
EXPORT_SYMBOL(tty_port_close_start);

 
void tty_port_close_end(struct tty_port *port, struct tty_struct *tty)
{
	unsigned long flags;

	tty_ldisc_flush(tty);
	tty->closing = 0;

	spin_lock_irqsave(&port->lock, flags);

	if (port->blocked_open) {
		spin_unlock_irqrestore(&port->lock, flags);
		if (port->close_delay)
			msleep_interruptible(jiffies_to_msecs(port->close_delay));
		spin_lock_irqsave(&port->lock, flags);
		wake_up_interruptible(&port->open_wait);
	}
	spin_unlock_irqrestore(&port->lock, flags);
	tty_port_set_active(port, false);
}
EXPORT_SYMBOL(tty_port_close_end);

 
void tty_port_close(struct tty_port *port, struct tty_struct *tty,
							struct file *filp)
{
	if (tty_port_close_start(port, tty, filp) == 0)
		return;
	tty_port_shutdown(port, tty);
	if (!port->console)
		set_bit(TTY_IO_ERROR, &tty->flags);
	tty_port_close_end(port, tty);
	tty_port_tty_set(port, NULL);
}
EXPORT_SYMBOL(tty_port_close);

 
int tty_port_install(struct tty_port *port, struct tty_driver *driver,
		struct tty_struct *tty)
{
	tty->port = port;
	return tty_standard_install(driver, tty);
}
EXPORT_SYMBOL_GPL(tty_port_install);

 
int tty_port_open(struct tty_port *port, struct tty_struct *tty,
							struct file *filp)
{
	spin_lock_irq(&port->lock);
	++port->count;
	spin_unlock_irq(&port->lock);
	tty_port_tty_set(port, tty);

	 

	mutex_lock(&port->mutex);

	if (!tty_port_initialized(port)) {
		clear_bit(TTY_IO_ERROR, &tty->flags);
		if (port->ops->activate) {
			int retval = port->ops->activate(port, tty);

			if (retval) {
				mutex_unlock(&port->mutex);
				return retval;
			}
		}
		tty_port_set_initialized(port, true);
	}
	mutex_unlock(&port->mutex);
	return tty_port_block_til_ready(port, tty, filp);
}
EXPORT_SYMBOL(tty_port_open);
