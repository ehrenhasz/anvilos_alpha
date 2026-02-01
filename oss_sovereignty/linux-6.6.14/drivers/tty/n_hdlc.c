
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>

#include <linux/poll.h>
#include <linux/in.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/errno.h>
#include <linux/string.h>	 
#include <linux/signal.h>	 
#include <linux/if.h>
#include <linux/bitops.h>

#include <linux/uaccess.h>
#include "tty.h"

 
#define MAX_HDLC_FRAME_SIZE 65535
#define DEFAULT_RX_BUF_COUNT 10
#define MAX_RX_BUF_COUNT 60
#define DEFAULT_TX_BUF_COUNT 3

struct n_hdlc_buf {
	struct list_head  list_item;
	int		  count;
	char		  buf[];
};

struct n_hdlc_buf_list {
	struct list_head  list;
	int		  count;
	spinlock_t	  spinlock;
};

 
struct n_hdlc {
	bool			tbusy;
	bool			woke_up;
	struct n_hdlc_buf_list	tx_buf_list;
	struct n_hdlc_buf_list	rx_buf_list;
	struct n_hdlc_buf_list	tx_free_buf_list;
	struct n_hdlc_buf_list	rx_free_buf_list;
	struct work_struct	write_work;
	struct tty_struct	*tty_for_write_work;
};

 
static void n_hdlc_buf_return(struct n_hdlc_buf_list *buf_list,
						struct n_hdlc_buf *buf);
static void n_hdlc_buf_put(struct n_hdlc_buf_list *list,
			   struct n_hdlc_buf *buf);
static struct n_hdlc_buf *n_hdlc_buf_get(struct n_hdlc_buf_list *list);

 

static struct n_hdlc *n_hdlc_alloc(void);
static void n_hdlc_tty_write_work(struct work_struct *work);

 
static int maxframe = 4096;

static void flush_rx_queue(struct tty_struct *tty)
{
	struct n_hdlc *n_hdlc = tty->disc_data;
	struct n_hdlc_buf *buf;

	while ((buf = n_hdlc_buf_get(&n_hdlc->rx_buf_list)))
		n_hdlc_buf_put(&n_hdlc->rx_free_buf_list, buf);
}

static void flush_tx_queue(struct tty_struct *tty)
{
	struct n_hdlc *n_hdlc = tty->disc_data;
	struct n_hdlc_buf *buf;

	while ((buf = n_hdlc_buf_get(&n_hdlc->tx_buf_list)))
		n_hdlc_buf_put(&n_hdlc->tx_free_buf_list, buf);
}

static void n_hdlc_free_buf_list(struct n_hdlc_buf_list *list)
{
	struct n_hdlc_buf *buf;

	do {
		buf = n_hdlc_buf_get(list);
		kfree(buf);
	} while (buf);
}

 
static void n_hdlc_tty_close(struct tty_struct *tty)
{
	struct n_hdlc *n_hdlc = tty->disc_data;

#if defined(TTY_NO_WRITE_SPLIT)
	clear_bit(TTY_NO_WRITE_SPLIT, &tty->flags);
#endif
	tty->disc_data = NULL;

	 
	wake_up_interruptible(&tty->read_wait);
	wake_up_interruptible(&tty->write_wait);

	cancel_work_sync(&n_hdlc->write_work);

	n_hdlc_free_buf_list(&n_hdlc->rx_free_buf_list);
	n_hdlc_free_buf_list(&n_hdlc->tx_free_buf_list);
	n_hdlc_free_buf_list(&n_hdlc->rx_buf_list);
	n_hdlc_free_buf_list(&n_hdlc->tx_buf_list);
	kfree(n_hdlc);
}	 

 
static int n_hdlc_tty_open(struct tty_struct *tty)
{
	struct n_hdlc *n_hdlc = tty->disc_data;

	pr_debug("%s() called (device=%s)\n", __func__, tty->name);

	 
	if (n_hdlc) {
		pr_err("%s: tty already associated!\n", __func__);
		return -EEXIST;
	}

	n_hdlc = n_hdlc_alloc();
	if (!n_hdlc) {
		pr_err("%s: n_hdlc_alloc failed\n", __func__);
		return -ENFILE;
	}

	INIT_WORK(&n_hdlc->write_work, n_hdlc_tty_write_work);
	n_hdlc->tty_for_write_work = tty;
	tty->disc_data = n_hdlc;
	tty->receive_room = 65536;

	 
	set_bit(TTY_NO_WRITE_SPLIT, &tty->flags);

	 
	tty_driver_flush_buffer(tty);

	return 0;

}	 

 
static void n_hdlc_send_frames(struct n_hdlc *n_hdlc, struct tty_struct *tty)
{
	register int actual;
	unsigned long flags;
	struct n_hdlc_buf *tbuf;

check_again:

	spin_lock_irqsave(&n_hdlc->tx_buf_list.spinlock, flags);
	if (n_hdlc->tbusy) {
		n_hdlc->woke_up = true;
		spin_unlock_irqrestore(&n_hdlc->tx_buf_list.spinlock, flags);
		return;
	}
	n_hdlc->tbusy = true;
	n_hdlc->woke_up = false;
	spin_unlock_irqrestore(&n_hdlc->tx_buf_list.spinlock, flags);

	tbuf = n_hdlc_buf_get(&n_hdlc->tx_buf_list);
	while (tbuf) {
		pr_debug("sending frame %p, count=%d\n", tbuf, tbuf->count);

		 
		set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
		actual = tty->ops->write(tty, tbuf->buf, tbuf->count);

		 
		if (actual == -ERESTARTSYS) {
			n_hdlc_buf_return(&n_hdlc->tx_buf_list, tbuf);
			break;
		}
		 
		 
		if (actual < 0)
			actual = tbuf->count;

		if (actual == tbuf->count) {
			pr_debug("frame %p completed\n", tbuf);

			 
			n_hdlc_buf_put(&n_hdlc->tx_free_buf_list, tbuf);

			 
			wake_up_interruptible(&tty->write_wait);

			 
			tbuf = n_hdlc_buf_get(&n_hdlc->tx_buf_list);
		} else {
			pr_debug("frame %p pending\n", tbuf);

			 
			n_hdlc_buf_return(&n_hdlc->tx_buf_list, tbuf);
			break;
		}
	}

	if (!tbuf)
		clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);

	 
	spin_lock_irqsave(&n_hdlc->tx_buf_list.spinlock, flags);
	n_hdlc->tbusy = false;
	spin_unlock_irqrestore(&n_hdlc->tx_buf_list.spinlock, flags);

	if (n_hdlc->woke_up)
		goto check_again;
}	 

 
static void n_hdlc_tty_write_work(struct work_struct *work)
{
	struct n_hdlc *n_hdlc = container_of(work, struct n_hdlc, write_work);
	struct tty_struct *tty = n_hdlc->tty_for_write_work;

	n_hdlc_send_frames(n_hdlc, tty);
}	 

 
static void n_hdlc_tty_wakeup(struct tty_struct *tty)
{
	struct n_hdlc *n_hdlc = tty->disc_data;

	schedule_work(&n_hdlc->write_work);
}	 

 
static void n_hdlc_tty_receive(struct tty_struct *tty, const u8 *data,
			       const u8 *flags, size_t count)
{
	register struct n_hdlc *n_hdlc = tty->disc_data;
	register struct n_hdlc_buf *buf;

	pr_debug("%s() called count=%zu\n", __func__, count);

	if (count > maxframe) {
		pr_debug("rx count>maxframesize, data discarded\n");
		return;
	}

	 
	buf = n_hdlc_buf_get(&n_hdlc->rx_free_buf_list);
	if (!buf) {
		 
		if (n_hdlc->rx_buf_list.count < MAX_RX_BUF_COUNT)
			buf = kmalloc(struct_size(buf, buf, maxframe),
				      GFP_ATOMIC);
	}

	if (!buf) {
		pr_debug("no more rx buffers, data discarded\n");
		return;
	}

	 
	memcpy(buf->buf, data, count);
	buf->count = count;

	 
	n_hdlc_buf_put(&n_hdlc->rx_buf_list, buf);

	 
	wake_up_interruptible(&tty->read_wait);
	if (tty->fasync != NULL)
		kill_fasync(&tty->fasync, SIGIO, POLL_IN);

}	 

 
static ssize_t n_hdlc_tty_read(struct tty_struct *tty, struct file *file,
			       u8 *kbuf, size_t nr, void **cookie,
			       unsigned long offset)
{
	struct n_hdlc *n_hdlc = tty->disc_data;
	int ret = 0;
	struct n_hdlc_buf *rbuf;
	DECLARE_WAITQUEUE(wait, current);

	 
	rbuf = *cookie;
	if (rbuf)
		goto have_rbuf;

	add_wait_queue(&tty->read_wait, &wait);

	for (;;) {
		if (test_bit(TTY_OTHER_CLOSED, &tty->flags)) {
			ret = -EIO;
			break;
		}
		if (tty_hung_up_p(file))
			break;

		set_current_state(TASK_INTERRUPTIBLE);

		rbuf = n_hdlc_buf_get(&n_hdlc->rx_buf_list);
		if (rbuf)
			break;

		 
		if (tty_io_nonblock(tty, file)) {
			ret = -EAGAIN;
			break;
		}

		schedule();

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}
	}

	remove_wait_queue(&tty->read_wait, &wait);
	__set_current_state(TASK_RUNNING);

	if (!rbuf)
		return ret;
	*cookie = rbuf;

have_rbuf:
	 
	if (offset >= rbuf->count)
		goto done_with_rbuf;

	 
	ret = -EOVERFLOW;
	if (!nr)
		goto done_with_rbuf;

	 
	ret = rbuf->count - offset;
	if (ret > nr)
		ret = nr;
	memcpy(kbuf, rbuf->buf+offset, ret);
	offset += ret;

	 
	if (offset < rbuf->count)
		return ret;

done_with_rbuf:
	*cookie = NULL;

	if (n_hdlc->rx_free_buf_list.count > DEFAULT_RX_BUF_COUNT)
		kfree(rbuf);
	else
		n_hdlc_buf_put(&n_hdlc->rx_free_buf_list, rbuf);

	return ret;

}	 

 
static ssize_t n_hdlc_tty_write(struct tty_struct *tty, struct file *file,
				const u8 *data, size_t count)
{
	struct n_hdlc *n_hdlc = tty->disc_data;
	int error = 0;
	DECLARE_WAITQUEUE(wait, current);
	struct n_hdlc_buf *tbuf;

	pr_debug("%s() called count=%zd\n", __func__, count);

	 
	if (count > maxframe) {
		pr_debug("%s: truncating user packet from %zu to %d\n",
				__func__, count, maxframe);
		count = maxframe;
	}

	add_wait_queue(&tty->write_wait, &wait);

	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);

		tbuf = n_hdlc_buf_get(&n_hdlc->tx_free_buf_list);
		if (tbuf)
			break;

		if (tty_io_nonblock(tty, file)) {
			error = -EAGAIN;
			break;
		}
		schedule();

		if (signal_pending(current)) {
			error = -EINTR;
			break;
		}
	}

	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&tty->write_wait, &wait);

	if (!error) {
		 
		memcpy(tbuf->buf, data, count);

		 
		tbuf->count = error = count;
		n_hdlc_buf_put(&n_hdlc->tx_buf_list, tbuf);
		n_hdlc_send_frames(n_hdlc, tty);
	}

	return error;

}	 

 
static int n_hdlc_tty_ioctl(struct tty_struct *tty, unsigned int cmd,
			    unsigned long arg)
{
	struct n_hdlc *n_hdlc = tty->disc_data;
	int error = 0;
	int count;
	unsigned long flags;
	struct n_hdlc_buf *buf = NULL;

	pr_debug("%s() called %d\n", __func__, cmd);

	switch (cmd) {
	case FIONREAD:
		 
		 
		spin_lock_irqsave(&n_hdlc->rx_buf_list.spinlock, flags);
		buf = list_first_entry_or_null(&n_hdlc->rx_buf_list.list,
						struct n_hdlc_buf, list_item);
		if (buf)
			count = buf->count;
		else
			count = 0;
		spin_unlock_irqrestore(&n_hdlc->rx_buf_list.spinlock, flags);
		error = put_user(count, (int __user *)arg);
		break;

	case TIOCOUTQ:
		 
		count = tty_chars_in_buffer(tty);
		 
		spin_lock_irqsave(&n_hdlc->tx_buf_list.spinlock, flags);
		buf = list_first_entry_or_null(&n_hdlc->tx_buf_list.list,
						struct n_hdlc_buf, list_item);
		if (buf)
			count += buf->count;
		spin_unlock_irqrestore(&n_hdlc->tx_buf_list.spinlock, flags);
		error = put_user(count, (int __user *)arg);
		break;

	case TCFLSH:
		switch (arg) {
		case TCIOFLUSH:
		case TCOFLUSH:
			flush_tx_queue(tty);
		}
		fallthrough;	 

	default:
		error = n_tty_ioctl_helper(tty, cmd, arg);
		break;
	}
	return error;

}	 

 
static __poll_t n_hdlc_tty_poll(struct tty_struct *tty, struct file *filp,
				    poll_table *wait)
{
	struct n_hdlc *n_hdlc = tty->disc_data;
	__poll_t mask = 0;

	 
	poll_wait(filp, &tty->read_wait, wait);
	poll_wait(filp, &tty->write_wait, wait);

	 
	if (!list_empty(&n_hdlc->rx_buf_list.list))
		mask |= EPOLLIN | EPOLLRDNORM;	 
	if (test_bit(TTY_OTHER_CLOSED, &tty->flags))
		mask |= EPOLLHUP;
	if (tty_hung_up_p(filp))
		mask |= EPOLLHUP;
	if (!tty_is_writelocked(tty) &&
			!list_empty(&n_hdlc->tx_free_buf_list.list))
		mask |= EPOLLOUT | EPOLLWRNORM;	 

	return mask;
}	 

static void n_hdlc_alloc_buf(struct n_hdlc_buf_list *list, unsigned int count,
		const char *name)
{
	struct n_hdlc_buf *buf;
	unsigned int i;

	for (i = 0; i < count; i++) {
		buf = kmalloc(struct_size(buf, buf, maxframe), GFP_KERNEL);
		if (!buf) {
			pr_debug("%s(), kmalloc() failed for %s buffer %u\n",
					__func__, name, i);
			return;
		}
		n_hdlc_buf_put(list, buf);
	}
}

 
static struct n_hdlc *n_hdlc_alloc(void)
{
	struct n_hdlc *n_hdlc = kzalloc(sizeof(*n_hdlc), GFP_KERNEL);

	if (!n_hdlc)
		return NULL;

	spin_lock_init(&n_hdlc->rx_free_buf_list.spinlock);
	spin_lock_init(&n_hdlc->tx_free_buf_list.spinlock);
	spin_lock_init(&n_hdlc->rx_buf_list.spinlock);
	spin_lock_init(&n_hdlc->tx_buf_list.spinlock);

	INIT_LIST_HEAD(&n_hdlc->rx_free_buf_list.list);
	INIT_LIST_HEAD(&n_hdlc->tx_free_buf_list.list);
	INIT_LIST_HEAD(&n_hdlc->rx_buf_list.list);
	INIT_LIST_HEAD(&n_hdlc->tx_buf_list.list);

	n_hdlc_alloc_buf(&n_hdlc->rx_free_buf_list, DEFAULT_RX_BUF_COUNT, "rx");
	n_hdlc_alloc_buf(&n_hdlc->tx_free_buf_list, DEFAULT_TX_BUF_COUNT, "tx");

	return n_hdlc;

}	 

 
static void n_hdlc_buf_return(struct n_hdlc_buf_list *buf_list,
						struct n_hdlc_buf *buf)
{
	unsigned long flags;

	spin_lock_irqsave(&buf_list->spinlock, flags);

	list_add(&buf->list_item, &buf_list->list);
	buf_list->count++;

	spin_unlock_irqrestore(&buf_list->spinlock, flags);
}

 
static void n_hdlc_buf_put(struct n_hdlc_buf_list *buf_list,
			   struct n_hdlc_buf *buf)
{
	unsigned long flags;

	spin_lock_irqsave(&buf_list->spinlock, flags);

	list_add_tail(&buf->list_item, &buf_list->list);
	buf_list->count++;

	spin_unlock_irqrestore(&buf_list->spinlock, flags);
}	 

 
static struct n_hdlc_buf *n_hdlc_buf_get(struct n_hdlc_buf_list *buf_list)
{
	unsigned long flags;
	struct n_hdlc_buf *buf;

	spin_lock_irqsave(&buf_list->spinlock, flags);

	buf = list_first_entry_or_null(&buf_list->list,
						struct n_hdlc_buf, list_item);
	if (buf) {
		list_del(&buf->list_item);
		buf_list->count--;
	}

	spin_unlock_irqrestore(&buf_list->spinlock, flags);
	return buf;
}	 

static struct tty_ldisc_ops n_hdlc_ldisc = {
	.owner		= THIS_MODULE,
	.num		= N_HDLC,
	.name		= "hdlc",
	.open		= n_hdlc_tty_open,
	.close		= n_hdlc_tty_close,
	.read		= n_hdlc_tty_read,
	.write		= n_hdlc_tty_write,
	.ioctl		= n_hdlc_tty_ioctl,
	.poll		= n_hdlc_tty_poll,
	.receive_buf	= n_hdlc_tty_receive,
	.write_wakeup	= n_hdlc_tty_wakeup,
	.flush_buffer   = flush_rx_queue,
};

static int __init n_hdlc_init(void)
{
	int status;

	 
	maxframe = clamp(maxframe, 4096, MAX_HDLC_FRAME_SIZE);

	status = tty_register_ldisc(&n_hdlc_ldisc);
	if (!status)
		pr_info("N_HDLC line discipline registered with maxframe=%d\n",
				maxframe);
	else
		pr_err("N_HDLC: error registering line discipline: %d\n",
				status);

	return status;

}	 

static void __exit n_hdlc_exit(void)
{
	tty_unregister_ldisc(&n_hdlc_ldisc);
}

module_init(n_hdlc_init);
module_exit(n_hdlc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Paul Fulghum paulkf@microgate.com");
module_param(maxframe, int, 0);
MODULE_ALIAS_LDISC(N_HDLC);
