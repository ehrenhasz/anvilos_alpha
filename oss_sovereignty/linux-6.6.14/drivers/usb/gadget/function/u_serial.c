
 

 

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/kstrtox.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>

#include "u_serial.h"


 

 
#define QUEUE_SIZE		16
#define WRITE_BUF_SIZE		8192		 
#define GS_CONSOLE_BUF_SIZE	8192

 
static DEFINE_SPINLOCK(serial_port_lock);

 
struct gs_console {
	struct console		console;
	struct work_struct	work;
	spinlock_t		lock;
	struct usb_request	*req;
	struct kfifo		buf;
	size_t			missed;
};

 
struct gs_port {
	struct tty_port		port;
	spinlock_t		port_lock;	 

	struct gserial		*port_usb;
#ifdef CONFIG_U_SERIAL_CONSOLE
	struct gs_console	*console;
#endif

	u8			port_num;

	struct list_head	read_pool;
	int read_started;
	int read_allocated;
	struct list_head	read_queue;
	unsigned		n_read;
	struct delayed_work	push;

	struct list_head	write_pool;
	int write_started;
	int write_allocated;
	struct kfifo		port_write_buf;
	wait_queue_head_t	drain_wait;	 
	bool                    write_busy;
	wait_queue_head_t	close_wait;
	bool			suspended;	 
	bool			start_delayed;	 

	 
	struct usb_cdc_line_coding port_line_coding;	 
};

static struct portmaster {
	struct mutex	lock;			 
	struct gs_port	*port;
} ports[MAX_U_SERIAL_PORTS];

#define GS_CLOSE_TIMEOUT		15		 



#ifdef VERBOSE_DEBUG
#ifndef pr_vdebug
#define pr_vdebug(fmt, arg...) \
	pr_debug(fmt, ##arg)
#endif  
#else
#ifndef pr_vdebug
#define pr_vdebug(fmt, arg...) \
	({ if (0) pr_debug(fmt, ##arg); })
#endif  
#endif

 

 

 
struct usb_request *
gs_alloc_req(struct usb_ep *ep, unsigned len, gfp_t kmalloc_flags)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, kmalloc_flags);

	if (req != NULL) {
		req->length = len;
		req->buf = kmalloc(len, kmalloc_flags);
		if (req->buf == NULL) {
			usb_ep_free_request(ep, req);
			return NULL;
		}
	}

	return req;
}
EXPORT_SYMBOL_GPL(gs_alloc_req);

 
void gs_free_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}
EXPORT_SYMBOL_GPL(gs_free_req);

 
static unsigned
gs_send_packet(struct gs_port *port, char *packet, unsigned size)
{
	unsigned len;

	len = kfifo_len(&port->port_write_buf);
	if (len < size)
		size = len;
	if (size != 0)
		size = kfifo_out(&port->port_write_buf, packet, size);
	return size;
}

 
static int gs_start_tx(struct gs_port *port)
 
{
	struct list_head	*pool = &port->write_pool;
	struct usb_ep		*in;
	int			status = 0;
	bool			do_tty_wake = false;

	if (!port->port_usb)
		return status;

	in = port->port_usb->in;

	while (!port->write_busy && !list_empty(pool)) {
		struct usb_request	*req;
		int			len;

		if (port->write_started >= QUEUE_SIZE)
			break;

		req = list_entry(pool->next, struct usb_request, list);
		len = gs_send_packet(port, req->buf, in->maxpacket);
		if (len == 0) {
			wake_up_interruptible(&port->drain_wait);
			break;
		}
		do_tty_wake = true;

		req->length = len;
		list_del(&req->list);
		req->zero = kfifo_is_empty(&port->port_write_buf);

		pr_vdebug("ttyGS%d: tx len=%d, %3ph ...\n", port->port_num, len, req->buf);

		 
		port->write_busy = true;
		spin_unlock(&port->port_lock);
		status = usb_ep_queue(in, req, GFP_ATOMIC);
		spin_lock(&port->port_lock);
		port->write_busy = false;

		if (status) {
			pr_debug("%s: %s %s err %d\n",
					__func__, "queue", in->name, status);
			list_add(&req->list, pool);
			break;
		}

		port->write_started++;

		 
		if (!port->port_usb)
			break;
	}

	if (do_tty_wake && port->port.tty)
		tty_wakeup(port->port.tty);
	return status;
}

 
static unsigned gs_start_rx(struct gs_port *port)
 
{
	struct list_head	*pool = &port->read_pool;
	struct usb_ep		*out = port->port_usb->out;

	while (!list_empty(pool)) {
		struct usb_request	*req;
		int			status;
		struct tty_struct	*tty;

		 
		tty = port->port.tty;
		if (!tty)
			break;

		if (port->read_started >= QUEUE_SIZE)
			break;

		req = list_entry(pool->next, struct usb_request, list);
		list_del(&req->list);
		req->length = out->maxpacket;

		 
		spin_unlock(&port->port_lock);
		status = usb_ep_queue(out, req, GFP_ATOMIC);
		spin_lock(&port->port_lock);

		if (status) {
			pr_debug("%s: %s %s err %d\n",
					__func__, "queue", out->name, status);
			list_add(&req->list, pool);
			break;
		}
		port->read_started++;

		 
		if (!port->port_usb)
			break;
	}
	return port->read_started;
}

 
static void gs_rx_push(struct work_struct *work)
{
	struct delayed_work	*w = to_delayed_work(work);
	struct gs_port		*port = container_of(w, struct gs_port, push);
	struct tty_struct	*tty;
	struct list_head	*queue = &port->read_queue;
	bool			disconnect = false;
	bool			do_push = false;

	 
	spin_lock_irq(&port->port_lock);
	tty = port->port.tty;
	while (!list_empty(queue)) {
		struct usb_request	*req;

		req = list_first_entry(queue, struct usb_request, list);

		 
		if (tty && tty_throttled(tty))
			break;

		switch (req->status) {
		case -ESHUTDOWN:
			disconnect = true;
			pr_vdebug("ttyGS%d: shutdown\n", port->port_num);
			break;

		default:
			 
			pr_warn("ttyGS%d: unexpected RX status %d\n",
				port->port_num, req->status);
			fallthrough;
		case 0:
			 
			break;
		}

		 
		if (req->actual && tty) {
			char		*packet = req->buf;
			unsigned	size = req->actual;
			unsigned	n;
			int		count;

			 
			n = port->n_read;
			if (n) {
				packet += n;
				size -= n;
			}

			count = tty_insert_flip_string(&port->port, packet,
					size);
			if (count)
				do_push = true;
			if (count != size) {
				 
				port->n_read += count;
				pr_vdebug("ttyGS%d: rx block %d/%d\n",
					  port->port_num, count, req->actual);
				break;
			}
			port->n_read = 0;
		}

		list_move(&req->list, &port->read_pool);
		port->read_started--;
	}

	 
	if (do_push)
		tty_flip_buffer_push(&port->port);


	 
	if (!list_empty(queue) && !tty_throttled(tty))
		schedule_delayed_work(&port->push, 1);

	 
	if (!disconnect && port->port_usb)
		gs_start_rx(port);

	spin_unlock_irq(&port->port_lock);
}

static void gs_read_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct gs_port	*port = ep->driver_data;

	 
	spin_lock(&port->port_lock);
	list_add_tail(&req->list, &port->read_queue);
	schedule_delayed_work(&port->push, 0);
	spin_unlock(&port->port_lock);
}

static void gs_write_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct gs_port	*port = ep->driver_data;

	spin_lock(&port->port_lock);
	list_add(&req->list, &port->write_pool);
	port->write_started--;

	switch (req->status) {
	default:
		 
		pr_warn("%s: unexpected %s status %d\n",
			__func__, ep->name, req->status);
		fallthrough;
	case 0:
		 
		gs_start_tx(port);
		break;

	case -ESHUTDOWN:
		 
		pr_vdebug("%s: %s shutdown\n", __func__, ep->name);
		break;
	}

	spin_unlock(&port->port_lock);
}

static void gs_free_requests(struct usb_ep *ep, struct list_head *head,
							 int *allocated)
{
	struct usb_request	*req;

	while (!list_empty(head)) {
		req = list_entry(head->next, struct usb_request, list);
		list_del(&req->list);
		gs_free_req(ep, req);
		if (allocated)
			(*allocated)--;
	}
}

static int gs_alloc_requests(struct usb_ep *ep, struct list_head *head,
		void (*fn)(struct usb_ep *, struct usb_request *),
		int *allocated)
{
	int			i;
	struct usb_request	*req;
	int n = allocated ? QUEUE_SIZE - *allocated : QUEUE_SIZE;

	 
	for (i = 0; i < n; i++) {
		req = gs_alloc_req(ep, ep->maxpacket, GFP_ATOMIC);
		if (!req)
			return list_empty(head) ? -ENOMEM : 0;
		req->complete = fn;
		list_add_tail(&req->list, head);
		if (allocated)
			(*allocated)++;
	}
	return 0;
}

 
static int gs_start_io(struct gs_port *port)
{
	struct list_head	*head = &port->read_pool;
	struct usb_ep		*ep;
	int			status;
	unsigned		started;

	if (!port->port_usb || !port->port.tty)
		return -EIO;

	 
	ep = port->port_usb->out;
	status = gs_alloc_requests(ep, head, gs_read_complete,
		&port->read_allocated);
	if (status)
		return status;

	status = gs_alloc_requests(port->port_usb->in, &port->write_pool,
			gs_write_complete, &port->write_allocated);
	if (status) {
		gs_free_requests(ep, head, &port->read_allocated);
		return status;
	}

	 
	port->n_read = 0;
	started = gs_start_rx(port);

	if (started) {
		gs_start_tx(port);
		 
		tty_wakeup(port->port.tty);
	} else {
		gs_free_requests(ep, head, &port->read_allocated);
		gs_free_requests(port->port_usb->in, &port->write_pool,
			&port->write_allocated);
		status = -EIO;
	}

	return status;
}

 

 

 
static int gs_open(struct tty_struct *tty, struct file *file)
{
	int		port_num = tty->index;
	struct gs_port	*port;
	int		status = 0;

	mutex_lock(&ports[port_num].lock);
	port = ports[port_num].port;
	if (!port) {
		status = -ENODEV;
		goto out;
	}

	spin_lock_irq(&port->port_lock);

	 
	if (!kfifo_initialized(&port->port_write_buf)) {

		spin_unlock_irq(&port->port_lock);

		 

		status = kfifo_alloc(&port->port_write_buf,
				     WRITE_BUF_SIZE, GFP_KERNEL);
		if (status) {
			pr_debug("gs_open: ttyGS%d (%p,%p) no buffer\n",
				 port_num, tty, file);
			goto out;
		}

		spin_lock_irq(&port->port_lock);
	}

	 
	if (port->port.count++)
		goto exit_unlock_port;

	tty->driver_data = port;
	port->port.tty = tty;

	 
	if (port->port_usb) {
		 
		if (!port->suspended) {
			struct gserial	*gser = port->port_usb;

			pr_debug("gs_open: start ttyGS%d\n", port->port_num);
			gs_start_io(port);

			if (gser->connect)
				gser->connect(gser);
		} else {
			pr_debug("delay start of ttyGS%d\n", port->port_num);
			port->start_delayed = true;
		}
	}

	pr_debug("gs_open: ttyGS%d (%p,%p)\n", port->port_num, tty, file);

exit_unlock_port:
	spin_unlock_irq(&port->port_lock);
out:
	mutex_unlock(&ports[port_num].lock);
	return status;
}

static int gs_close_flush_done(struct gs_port *p)
{
	int cond;

	 
	spin_lock_irq(&p->port_lock);
	cond = p->port_usb == NULL || !kfifo_len(&p->port_write_buf) ||
		p->port.count > 1;
	spin_unlock_irq(&p->port_lock);

	return cond;
}

static void gs_close(struct tty_struct *tty, struct file *file)
{
	struct gs_port *port = tty->driver_data;
	struct gserial	*gser;

	spin_lock_irq(&port->port_lock);

	if (port->port.count != 1) {
raced_with_open:
		if (port->port.count == 0)
			WARN_ON(1);
		else
			--port->port.count;
		goto exit;
	}

	pr_debug("gs_close: ttyGS%d (%p,%p) ...\n", port->port_num, tty, file);

	gser = port->port_usb;
	if (gser && !port->suspended && gser->disconnect)
		gser->disconnect(gser);

	 
	if (kfifo_len(&port->port_write_buf) > 0 && gser) {
		spin_unlock_irq(&port->port_lock);
		wait_event_interruptible_timeout(port->drain_wait,
					gs_close_flush_done(port),
					GS_CLOSE_TIMEOUT * HZ);
		spin_lock_irq(&port->port_lock);

		if (port->port.count != 1)
			goto raced_with_open;

		gser = port->port_usb;
	}

	 
	if (gser == NULL)
		kfifo_free(&port->port_write_buf);
	else
		kfifo_reset(&port->port_write_buf);

	port->start_delayed = false;
	port->port.count = 0;
	port->port.tty = NULL;

	pr_debug("gs_close: ttyGS%d (%p,%p) done!\n",
			port->port_num, tty, file);

	wake_up(&port->close_wait);
exit:
	spin_unlock_irq(&port->port_lock);
}

static ssize_t gs_write(struct tty_struct *tty, const u8 *buf, size_t count)
{
	struct gs_port	*port = tty->driver_data;
	unsigned long	flags;

	pr_vdebug("gs_write: ttyGS%d (%p) writing %zu bytes\n",
			port->port_num, tty, count);

	spin_lock_irqsave(&port->port_lock, flags);
	if (count)
		count = kfifo_in(&port->port_write_buf, buf, count);
	 
	if (port->port_usb)
		gs_start_tx(port);
	spin_unlock_irqrestore(&port->port_lock, flags);

	return count;
}

static int gs_put_char(struct tty_struct *tty, u8 ch)
{
	struct gs_port	*port = tty->driver_data;
	unsigned long	flags;
	int		status;

	pr_vdebug("gs_put_char: (%d,%p) char=0x%x, called from %ps\n",
		port->port_num, tty, ch, __builtin_return_address(0));

	spin_lock_irqsave(&port->port_lock, flags);
	status = kfifo_put(&port->port_write_buf, ch);
	spin_unlock_irqrestore(&port->port_lock, flags);

	return status;
}

static void gs_flush_chars(struct tty_struct *tty)
{
	struct gs_port	*port = tty->driver_data;
	unsigned long	flags;

	pr_vdebug("gs_flush_chars: (%d,%p)\n", port->port_num, tty);

	spin_lock_irqsave(&port->port_lock, flags);
	if (port->port_usb)
		gs_start_tx(port);
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static unsigned int gs_write_room(struct tty_struct *tty)
{
	struct gs_port	*port = tty->driver_data;
	unsigned long	flags;
	unsigned int room = 0;

	spin_lock_irqsave(&port->port_lock, flags);
	if (port->port_usb)
		room = kfifo_avail(&port->port_write_buf);
	spin_unlock_irqrestore(&port->port_lock, flags);

	pr_vdebug("gs_write_room: (%d,%p) room=%u\n",
		port->port_num, tty, room);

	return room;
}

static unsigned int gs_chars_in_buffer(struct tty_struct *tty)
{
	struct gs_port	*port = tty->driver_data;
	unsigned long	flags;
	unsigned int	chars;

	spin_lock_irqsave(&port->port_lock, flags);
	chars = kfifo_len(&port->port_write_buf);
	spin_unlock_irqrestore(&port->port_lock, flags);

	pr_vdebug("gs_chars_in_buffer: (%d,%p) chars=%u\n",
		port->port_num, tty, chars);

	return chars;
}

 
static void gs_unthrottle(struct tty_struct *tty)
{
	struct gs_port		*port = tty->driver_data;
	unsigned long		flags;

	spin_lock_irqsave(&port->port_lock, flags);
	if (port->port_usb) {
		 
		pr_vdebug("ttyGS%d: unthrottle\n", port->port_num);
		schedule_delayed_work(&port->push, 0);
	}
	spin_unlock_irqrestore(&port->port_lock, flags);
}

static int gs_break_ctl(struct tty_struct *tty, int duration)
{
	struct gs_port	*port = tty->driver_data;
	int		status = 0;
	struct gserial	*gser;

	pr_vdebug("gs_break_ctl: ttyGS%d, send break (%d) \n",
			port->port_num, duration);

	spin_lock_irq(&port->port_lock);
	gser = port->port_usb;
	if (gser && gser->send_break)
		status = gser->send_break(gser, duration);
	spin_unlock_irq(&port->port_lock);

	return status;
}

static const struct tty_operations gs_tty_ops = {
	.open =			gs_open,
	.close =		gs_close,
	.write =		gs_write,
	.put_char =		gs_put_char,
	.flush_chars =		gs_flush_chars,
	.write_room =		gs_write_room,
	.chars_in_buffer =	gs_chars_in_buffer,
	.unthrottle =		gs_unthrottle,
	.break_ctl =		gs_break_ctl,
};

 

static struct tty_driver *gs_tty_driver;

#ifdef CONFIG_U_SERIAL_CONSOLE

static void gs_console_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct gs_console *cons = req->context;

	switch (req->status) {
	default:
		pr_warn("%s: unexpected %s status %d\n",
			__func__, ep->name, req->status);
		fallthrough;
	case 0:
		 
		spin_lock(&cons->lock);
		req->length = 0;
		schedule_work(&cons->work);
		spin_unlock(&cons->lock);
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		 
		pr_vdebug("%s: %s shutdown\n", __func__, ep->name);
		break;
	}
}

static void __gs_console_push(struct gs_console *cons)
{
	struct usb_request *req = cons->req;
	struct usb_ep *ep;
	size_t size;

	if (!req)
		return;	 

	if (req->length)
		return;	 

	ep = cons->console.data;
	size = kfifo_out(&cons->buf, req->buf, ep->maxpacket);
	if (!size)
		return;

	if (cons->missed && ep->maxpacket >= 64) {
		char buf[64];
		size_t len;

		len = sprintf(buf, "\n[missed %zu bytes]\n", cons->missed);
		kfifo_in(&cons->buf, buf, len);
		cons->missed = 0;
	}

	req->length = size;

	spin_unlock_irq(&cons->lock);
	if (usb_ep_queue(ep, req, GFP_ATOMIC))
		req->length = 0;
	spin_lock_irq(&cons->lock);
}

static void gs_console_work(struct work_struct *work)
{
	struct gs_console *cons = container_of(work, struct gs_console, work);

	spin_lock_irq(&cons->lock);

	__gs_console_push(cons);

	spin_unlock_irq(&cons->lock);
}

static void gs_console_write(struct console *co,
			     const char *buf, unsigned count)
{
	struct gs_console *cons = container_of(co, struct gs_console, console);
	unsigned long flags;
	size_t n;

	spin_lock_irqsave(&cons->lock, flags);

	n = kfifo_in(&cons->buf, buf, count);
	if (n < count)
		cons->missed += count - n;

	if (cons->req && !cons->req->length)
		schedule_work(&cons->work);

	spin_unlock_irqrestore(&cons->lock, flags);
}

static struct tty_driver *gs_console_device(struct console *co, int *index)
{
	*index = co->index;
	return gs_tty_driver;
}

static int gs_console_connect(struct gs_port *port)
{
	struct gs_console *cons = port->console;
	struct usb_request *req;
	struct usb_ep *ep;

	if (!cons)
		return 0;

	ep = port->port_usb->in;
	req = gs_alloc_req(ep, ep->maxpacket, GFP_ATOMIC);
	if (!req)
		return -ENOMEM;
	req->complete = gs_console_complete_out;
	req->context = cons;
	req->length = 0;

	spin_lock(&cons->lock);
	cons->req = req;
	cons->console.data = ep;
	spin_unlock(&cons->lock);

	pr_debug("ttyGS%d: console connected!\n", port->port_num);

	schedule_work(&cons->work);

	return 0;
}

static void gs_console_disconnect(struct gs_port *port)
{
	struct gs_console *cons = port->console;
	struct usb_request *req;
	struct usb_ep *ep;

	if (!cons)
		return;

	spin_lock(&cons->lock);

	req = cons->req;
	ep = cons->console.data;
	cons->req = NULL;

	spin_unlock(&cons->lock);

	if (!req)
		return;

	usb_ep_dequeue(ep, req);
	gs_free_req(ep, req);
}

static int gs_console_init(struct gs_port *port)
{
	struct gs_console *cons;
	int err;

	if (port->console)
		return 0;

	cons = kzalloc(sizeof(*port->console), GFP_KERNEL);
	if (!cons)
		return -ENOMEM;

	strcpy(cons->console.name, "ttyGS");
	cons->console.write = gs_console_write;
	cons->console.device = gs_console_device;
	cons->console.flags = CON_PRINTBUFFER;
	cons->console.index = port->port_num;

	INIT_WORK(&cons->work, gs_console_work);
	spin_lock_init(&cons->lock);

	err = kfifo_alloc(&cons->buf, GS_CONSOLE_BUF_SIZE, GFP_KERNEL);
	if (err) {
		pr_err("ttyGS%d: allocate console buffer failed\n", port->port_num);
		kfree(cons);
		return err;
	}

	port->console = cons;
	register_console(&cons->console);

	spin_lock_irq(&port->port_lock);
	if (port->port_usb)
		gs_console_connect(port);
	spin_unlock_irq(&port->port_lock);

	return 0;
}

static void gs_console_exit(struct gs_port *port)
{
	struct gs_console *cons = port->console;

	if (!cons)
		return;

	unregister_console(&cons->console);

	spin_lock_irq(&port->port_lock);
	if (cons->req)
		gs_console_disconnect(port);
	spin_unlock_irq(&port->port_lock);

	cancel_work_sync(&cons->work);
	kfifo_free(&cons->buf);
	kfree(cons);
	port->console = NULL;
}

ssize_t gserial_set_console(unsigned char port_num, const char *page, size_t count)
{
	struct gs_port *port;
	bool enable;
	int ret;

	ret = kstrtobool(page, &enable);
	if (ret)
		return ret;

	mutex_lock(&ports[port_num].lock);
	port = ports[port_num].port;

	if (WARN_ON(port == NULL)) {
		ret = -ENXIO;
		goto out;
	}

	if (enable)
		ret = gs_console_init(port);
	else
		gs_console_exit(port);
out:
	mutex_unlock(&ports[port_num].lock);

	return ret < 0 ? ret : count;
}
EXPORT_SYMBOL_GPL(gserial_set_console);

ssize_t gserial_get_console(unsigned char port_num, char *page)
{
	struct gs_port *port;
	ssize_t ret;

	mutex_lock(&ports[port_num].lock);
	port = ports[port_num].port;

	if (WARN_ON(port == NULL))
		ret = -ENXIO;
	else
		ret = sprintf(page, "%u\n", !!port->console);

	mutex_unlock(&ports[port_num].lock);

	return ret;
}
EXPORT_SYMBOL_GPL(gserial_get_console);

#else

static int gs_console_connect(struct gs_port *port)
{
	return 0;
}

static void gs_console_disconnect(struct gs_port *port)
{
}

static int gs_console_init(struct gs_port *port)
{
	return -ENOSYS;
}

static void gs_console_exit(struct gs_port *port)
{
}

#endif

static int
gs_port_alloc(unsigned port_num, struct usb_cdc_line_coding *coding)
{
	struct gs_port	*port;
	int		ret = 0;

	mutex_lock(&ports[port_num].lock);
	if (ports[port_num].port) {
		ret = -EBUSY;
		goto out;
	}

	port = kzalloc(sizeof(struct gs_port), GFP_KERNEL);
	if (port == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	tty_port_init(&port->port);
	spin_lock_init(&port->port_lock);
	init_waitqueue_head(&port->drain_wait);
	init_waitqueue_head(&port->close_wait);

	INIT_DELAYED_WORK(&port->push, gs_rx_push);

	INIT_LIST_HEAD(&port->read_pool);
	INIT_LIST_HEAD(&port->read_queue);
	INIT_LIST_HEAD(&port->write_pool);

	port->port_num = port_num;
	port->port_line_coding = *coding;

	ports[port_num].port = port;
out:
	mutex_unlock(&ports[port_num].lock);
	return ret;
}

static int gs_closed(struct gs_port *port)
{
	int cond;

	spin_lock_irq(&port->port_lock);
	cond = port->port.count == 0;
	spin_unlock_irq(&port->port_lock);

	return cond;
}

static void gserial_free_port(struct gs_port *port)
{
	cancel_delayed_work_sync(&port->push);
	 
	wait_event(port->close_wait, gs_closed(port));
	WARN_ON(port->port_usb != NULL);
	tty_port_destroy(&port->port);
	kfree(port);
}

void gserial_free_line(unsigned char port_num)
{
	struct gs_port	*port;

	mutex_lock(&ports[port_num].lock);
	if (!ports[port_num].port) {
		mutex_unlock(&ports[port_num].lock);
		return;
	}
	port = ports[port_num].port;
	gs_console_exit(port);
	ports[port_num].port = NULL;
	mutex_unlock(&ports[port_num].lock);

	gserial_free_port(port);
	tty_unregister_device(gs_tty_driver, port_num);
}
EXPORT_SYMBOL_GPL(gserial_free_line);

int gserial_alloc_line_no_console(unsigned char *line_num)
{
	struct usb_cdc_line_coding	coding;
	struct gs_port			*port;
	struct device			*tty_dev;
	int				ret;
	int				port_num;

	coding.dwDTERate = cpu_to_le32(9600);
	coding.bCharFormat = 8;
	coding.bParityType = USB_CDC_NO_PARITY;
	coding.bDataBits = USB_CDC_1_STOP_BITS;

	for (port_num = 0; port_num < MAX_U_SERIAL_PORTS; port_num++) {
		ret = gs_port_alloc(port_num, &coding);
		if (ret == -EBUSY)
			continue;
		if (ret)
			return ret;
		break;
	}
	if (ret)
		return ret;

	 

	port = ports[port_num].port;
	tty_dev = tty_port_register_device(&port->port,
			gs_tty_driver, port_num, NULL);
	if (IS_ERR(tty_dev)) {
		pr_err("%s: failed to register tty for port %d, err %ld\n",
				__func__, port_num, PTR_ERR(tty_dev));

		ret = PTR_ERR(tty_dev);
		mutex_lock(&ports[port_num].lock);
		ports[port_num].port = NULL;
		mutex_unlock(&ports[port_num].lock);
		gserial_free_port(port);
		goto err;
	}
	*line_num = port_num;
err:
	return ret;
}
EXPORT_SYMBOL_GPL(gserial_alloc_line_no_console);

int gserial_alloc_line(unsigned char *line_num)
{
	int ret = gserial_alloc_line_no_console(line_num);

	if (!ret && !*line_num)
		gs_console_init(ports[*line_num].port);

	return ret;
}
EXPORT_SYMBOL_GPL(gserial_alloc_line);

 
int gserial_connect(struct gserial *gser, u8 port_num)
{
	struct gs_port	*port;
	unsigned long	flags;
	int		status;

	if (port_num >= MAX_U_SERIAL_PORTS)
		return -ENXIO;

	port = ports[port_num].port;
	if (!port) {
		pr_err("serial line %d not allocated.\n", port_num);
		return -EINVAL;
	}
	if (port->port_usb) {
		pr_err("serial line %d is in use.\n", port_num);
		return -EBUSY;
	}

	 
	status = usb_ep_enable(gser->in);
	if (status < 0)
		return status;
	gser->in->driver_data = port;

	status = usb_ep_enable(gser->out);
	if (status < 0)
		goto fail_out;
	gser->out->driver_data = port;

	 
	spin_lock_irqsave(&port->port_lock, flags);
	gser->ioport = port;
	port->port_usb = gser;

	 
	gser->port_line_coding = port->port_line_coding;

	 

	 
	if (port->port.count) {
		pr_debug("gserial_connect: start ttyGS%d\n", port->port_num);
		gs_start_io(port);
		if (gser->connect)
			gser->connect(gser);
	} else {
		if (gser->disconnect)
			gser->disconnect(gser);
	}

	status = gs_console_connect(port);
	spin_unlock_irqrestore(&port->port_lock, flags);

	return status;

fail_out:
	usb_ep_disable(gser->in);
	return status;
}
EXPORT_SYMBOL_GPL(gserial_connect);
 
void gserial_disconnect(struct gserial *gser)
{
	struct gs_port	*port = gser->ioport;
	unsigned long	flags;

	if (!port)
		return;

	spin_lock_irqsave(&serial_port_lock, flags);

	 
	spin_lock(&port->port_lock);

	gs_console_disconnect(port);

	 
	port->port_line_coding = gser->port_line_coding;

	port->port_usb = NULL;
	gser->ioport = NULL;
	if (port->port.count > 0) {
		wake_up_interruptible(&port->drain_wait);
		if (port->port.tty)
			tty_hangup(port->port.tty);
	}
	port->suspended = false;
	spin_unlock(&port->port_lock);
	spin_unlock_irqrestore(&serial_port_lock, flags);

	 
	usb_ep_disable(gser->out);
	usb_ep_disable(gser->in);

	 
	spin_lock_irqsave(&port->port_lock, flags);
	if (port->port.count == 0)
		kfifo_free(&port->port_write_buf);
	gs_free_requests(gser->out, &port->read_pool, NULL);
	gs_free_requests(gser->out, &port->read_queue, NULL);
	gs_free_requests(gser->in, &port->write_pool, NULL);

	port->read_allocated = port->read_started =
		port->write_allocated = port->write_started = 0;

	spin_unlock_irqrestore(&port->port_lock, flags);
}
EXPORT_SYMBOL_GPL(gserial_disconnect);

void gserial_suspend(struct gserial *gser)
{
	struct gs_port	*port;
	unsigned long	flags;

	spin_lock_irqsave(&serial_port_lock, flags);
	port = gser->ioport;

	if (!port) {
		spin_unlock_irqrestore(&serial_port_lock, flags);
		return;
	}

	spin_lock(&port->port_lock);
	spin_unlock(&serial_port_lock);
	port->suspended = true;
	spin_unlock_irqrestore(&port->port_lock, flags);
}
EXPORT_SYMBOL_GPL(gserial_suspend);

void gserial_resume(struct gserial *gser)
{
	struct gs_port *port;
	unsigned long	flags;

	spin_lock_irqsave(&serial_port_lock, flags);
	port = gser->ioport;

	if (!port) {
		spin_unlock_irqrestore(&serial_port_lock, flags);
		return;
	}

	spin_lock(&port->port_lock);
	spin_unlock(&serial_port_lock);
	port->suspended = false;
	if (!port->start_delayed) {
		spin_unlock_irqrestore(&port->port_lock, flags);
		return;
	}

	pr_debug("delayed start ttyGS%d\n", port->port_num);
	gs_start_io(port);
	if (gser->connect)
		gser->connect(gser);
	port->start_delayed = false;
	spin_unlock_irqrestore(&port->port_lock, flags);
}
EXPORT_SYMBOL_GPL(gserial_resume);

static int __init userial_init(void)
{
	struct tty_driver *driver;
	unsigned			i;
	int				status;

	driver = tty_alloc_driver(MAX_U_SERIAL_PORTS, TTY_DRIVER_REAL_RAW |
			TTY_DRIVER_DYNAMIC_DEV);
	if (IS_ERR(driver))
		return PTR_ERR(driver);

	driver->driver_name = "g_serial";
	driver->name = "ttyGS";
	 

	driver->type = TTY_DRIVER_TYPE_SERIAL;
	driver->subtype = SERIAL_TYPE_NORMAL;
	driver->init_termios = tty_std_termios;

	 
	driver->init_termios.c_cflag =
			B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	driver->init_termios.c_ispeed = 9600;
	driver->init_termios.c_ospeed = 9600;

	tty_set_operations(driver, &gs_tty_ops);
	for (i = 0; i < MAX_U_SERIAL_PORTS; i++)
		mutex_init(&ports[i].lock);

	 
	status = tty_register_driver(driver);
	if (status) {
		pr_err("%s: cannot register, err %d\n",
				__func__, status);
		goto fail;
	}

	gs_tty_driver = driver;

	pr_debug("%s: registered %d ttyGS* device%s\n", __func__,
			MAX_U_SERIAL_PORTS,
			(MAX_U_SERIAL_PORTS == 1) ? "" : "s");

	return status;
fail:
	tty_driver_kref_put(driver);
	return status;
}
module_init(userial_init);

static void __exit userial_cleanup(void)
{
	tty_unregister_driver(gs_tty_driver);
	tty_driver_kref_put(gs_tty_driver);
	gs_tty_driver = NULL;
}
module_exit(userial_cleanup);

MODULE_LICENSE("GPL");
