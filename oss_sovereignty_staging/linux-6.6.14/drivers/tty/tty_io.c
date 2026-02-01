
 

 

#include <linux/types.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/fcntl.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/devpts_fs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/console.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/kd.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/ppp-ioctl.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/serial.h>
#include <linux/ratelimit.h>
#include <linux/compat.h>
#include <linux/uaccess.h>
#include <linux/termios_internal.h>
#include <linux/fs.h>

#include <linux/kbd_kern.h>
#include <linux/vt_kern.h>
#include <linux/selection.h>

#include <linux/kmod.h>
#include <linux/nsproxy.h>
#include "tty.h"

#undef TTY_DEBUG_HANGUP
#ifdef TTY_DEBUG_HANGUP
# define tty_debug_hangup(tty, f, args...)	tty_debug(tty, f, ##args)
#else
# define tty_debug_hangup(tty, f, args...)	do { } while (0)
#endif

#define TTY_PARANOIA_CHECK 1
#define CHECK_TTY_COUNT 1

struct ktermios tty_std_termios = {	 
	.c_iflag = ICRNL | IXON,
	.c_oflag = OPOST | ONLCR,
	.c_cflag = B38400 | CS8 | CREAD | HUPCL,
	.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK |
		   ECHOCTL | ECHOKE | IEXTEN,
	.c_cc = INIT_C_CC,
	.c_ispeed = 38400,
	.c_ospeed = 38400,
	 
};
EXPORT_SYMBOL(tty_std_termios);

 

LIST_HEAD(tty_drivers);			 

 
DEFINE_MUTEX(tty_mutex);

static ssize_t tty_read(struct kiocb *, struct iov_iter *);
static ssize_t tty_write(struct kiocb *, struct iov_iter *);
static __poll_t tty_poll(struct file *, poll_table *);
static int tty_open(struct inode *, struct file *);
#ifdef CONFIG_COMPAT
static long tty_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg);
#else
#define tty_compat_ioctl NULL
#endif
static int __tty_fasync(int fd, struct file *filp, int on);
static int tty_fasync(int fd, struct file *filp, int on);
static void release_tty(struct tty_struct *tty, int idx);

 
static void free_tty_struct(struct tty_struct *tty)
{
	tty_ldisc_deinit(tty);
	put_device(tty->dev);
	kvfree(tty->write_buf);
	kfree(tty);
}

static inline struct tty_struct *file_tty(struct file *file)
{
	return ((struct tty_file_private *)file->private_data)->tty;
}

int tty_alloc_file(struct file *file)
{
	struct tty_file_private *priv;

	priv = kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	file->private_data = priv;

	return 0;
}

 
void tty_add_file(struct tty_struct *tty, struct file *file)
{
	struct tty_file_private *priv = file->private_data;

	priv->tty = tty;
	priv->file = file;

	spin_lock(&tty->files_lock);
	list_add(&priv->list, &tty->tty_files);
	spin_unlock(&tty->files_lock);
}

 
void tty_free_file(struct file *file)
{
	struct tty_file_private *priv = file->private_data;

	file->private_data = NULL;
	kfree(priv);
}

 
static void tty_del_file(struct file *file)
{
	struct tty_file_private *priv = file->private_data;
	struct tty_struct *tty = priv->tty;

	spin_lock(&tty->files_lock);
	list_del(&priv->list);
	spin_unlock(&tty->files_lock);
	tty_free_file(file);
}

 
const char *tty_name(const struct tty_struct *tty)
{
	if (!tty)  
		return "NULL tty";
	return tty->name;
}
EXPORT_SYMBOL(tty_name);

const char *tty_driver_name(const struct tty_struct *tty)
{
	if (!tty || !tty->driver)
		return "";
	return tty->driver->name;
}

static int tty_paranoia_check(struct tty_struct *tty, struct inode *inode,
			      const char *routine)
{
#ifdef TTY_PARANOIA_CHECK
	if (!tty) {
		pr_warn("(%d:%d): %s: NULL tty\n",
			imajor(inode), iminor(inode), routine);
		return 1;
	}
#endif
	return 0;
}

 
static void check_tty_count(struct tty_struct *tty, const char *routine)
{
#ifdef CHECK_TTY_COUNT
	struct list_head *p;
	int count = 0, kopen_count = 0;

	spin_lock(&tty->files_lock);
	list_for_each(p, &tty->tty_files) {
		count++;
	}
	spin_unlock(&tty->files_lock);
	if (tty->driver->type == TTY_DRIVER_TYPE_PTY &&
	    tty->driver->subtype == PTY_TYPE_SLAVE &&
	    tty->link && tty->link->count)
		count++;
	if (tty_port_kopened(tty->port))
		kopen_count++;
	if (tty->count != (count + kopen_count)) {
		tty_warn(tty, "%s: tty->count(%d) != (#fd's(%d) + #kopen's(%d))\n",
			 routine, tty->count, count, kopen_count);
	}
#endif
}

 
static struct tty_driver *get_tty_driver(dev_t device, int *index)
{
	struct tty_driver *p;

	list_for_each_entry(p, &tty_drivers, tty_drivers) {
		dev_t base = MKDEV(p->major, p->minor_start);

		if (device < base || device >= base + p->num)
			continue;
		*index = device - base;
		return tty_driver_kref_get(p);
	}
	return NULL;
}

 
int tty_dev_name_to_number(const char *name, dev_t *number)
{
	struct tty_driver *p;
	int ret;
	int index, prefix_length = 0;
	const char *str;

	for (str = name; *str && !isdigit(*str); str++)
		;

	if (!*str)
		return -EINVAL;

	ret = kstrtoint(str, 10, &index);
	if (ret)
		return ret;

	prefix_length = str - name;
	mutex_lock(&tty_mutex);

	list_for_each_entry(p, &tty_drivers, tty_drivers)
		if (prefix_length == strlen(p->name) && strncmp(name,
					p->name, prefix_length) == 0) {
			if (index < p->num) {
				*number = MKDEV(p->major, p->minor_start + index);
				goto out;
			}
		}

	 
	ret = -ENODEV;
out:
	mutex_unlock(&tty_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(tty_dev_name_to_number);

#ifdef CONFIG_CONSOLE_POLL

 
struct tty_driver *tty_find_polling_driver(char *name, int *line)
{
	struct tty_driver *p, *res = NULL;
	int tty_line = 0;
	int len;
	char *str, *stp;

	for (str = name; *str; str++)
		if ((*str >= '0' && *str <= '9') || *str == ',')
			break;
	if (!*str)
		return NULL;

	len = str - name;
	tty_line = simple_strtoul(str, &str, 10);

	mutex_lock(&tty_mutex);
	 
	list_for_each_entry(p, &tty_drivers, tty_drivers) {
		if (!len || strncmp(name, p->name, len) != 0)
			continue;
		stp = str;
		if (*stp == ',')
			stp++;
		if (*stp == '\0')
			stp = NULL;

		if (tty_line >= 0 && tty_line < p->num && p->ops &&
		    p->ops->poll_init && !p->ops->poll_init(p, tty_line, stp)) {
			res = tty_driver_kref_get(p);
			*line = tty_line;
			break;
		}
	}
	mutex_unlock(&tty_mutex);

	return res;
}
EXPORT_SYMBOL_GPL(tty_find_polling_driver);
#endif

static ssize_t hung_up_tty_read(struct kiocb *iocb, struct iov_iter *to)
{
	return 0;
}

static ssize_t hung_up_tty_write(struct kiocb *iocb, struct iov_iter *from)
{
	return -EIO;
}

 
static __poll_t hung_up_tty_poll(struct file *filp, poll_table *wait)
{
	return EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDNORM | EPOLLWRNORM;
}

static long hung_up_tty_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	return cmd == TIOCSPGRP ? -ENOTTY : -EIO;
}

static long hung_up_tty_compat_ioctl(struct file *file,
				     unsigned int cmd, unsigned long arg)
{
	return cmd == TIOCSPGRP ? -ENOTTY : -EIO;
}

static int hung_up_tty_fasync(int fd, struct file *file, int on)
{
	return -ENOTTY;
}

static void tty_show_fdinfo(struct seq_file *m, struct file *file)
{
	struct tty_struct *tty = file_tty(file);

	if (tty && tty->ops && tty->ops->show_fdinfo)
		tty->ops->show_fdinfo(tty, m);
}

static const struct file_operations tty_fops = {
	.llseek		= no_llseek,
	.read_iter	= tty_read,
	.write_iter	= tty_write,
	.splice_read	= copy_splice_read,
	.splice_write	= iter_file_splice_write,
	.poll		= tty_poll,
	.unlocked_ioctl	= tty_ioctl,
	.compat_ioctl	= tty_compat_ioctl,
	.open		= tty_open,
	.release	= tty_release,
	.fasync		= tty_fasync,
	.show_fdinfo	= tty_show_fdinfo,
};

static const struct file_operations console_fops = {
	.llseek		= no_llseek,
	.read_iter	= tty_read,
	.write_iter	= redirected_tty_write,
	.splice_read	= copy_splice_read,
	.splice_write	= iter_file_splice_write,
	.poll		= tty_poll,
	.unlocked_ioctl	= tty_ioctl,
	.compat_ioctl	= tty_compat_ioctl,
	.open		= tty_open,
	.release	= tty_release,
	.fasync		= tty_fasync,
};

static const struct file_operations hung_up_tty_fops = {
	.llseek		= no_llseek,
	.read_iter	= hung_up_tty_read,
	.write_iter	= hung_up_tty_write,
	.poll		= hung_up_tty_poll,
	.unlocked_ioctl	= hung_up_tty_ioctl,
	.compat_ioctl	= hung_up_tty_compat_ioctl,
	.release	= tty_release,
	.fasync		= hung_up_tty_fasync,
};

static DEFINE_SPINLOCK(redirect_lock);
static struct file *redirect;

 
void tty_wakeup(struct tty_struct *tty)
{
	struct tty_ldisc *ld;

	if (test_bit(TTY_DO_WRITE_WAKEUP, &tty->flags)) {
		ld = tty_ldisc_ref(tty);
		if (ld) {
			if (ld->ops->write_wakeup)
				ld->ops->write_wakeup(tty);
			tty_ldisc_deref(ld);
		}
	}
	wake_up_interruptible_poll(&tty->write_wait, EPOLLOUT);
}
EXPORT_SYMBOL_GPL(tty_wakeup);

 
static struct file *tty_release_redirect(struct tty_struct *tty)
{
	struct file *f = NULL;

	spin_lock(&redirect_lock);
	if (redirect && file_tty(redirect) == tty) {
		f = redirect;
		redirect = NULL;
	}
	spin_unlock(&redirect_lock);

	return f;
}

 
static void __tty_hangup(struct tty_struct *tty, int exit_session)
{
	struct file *cons_filp = NULL;
	struct file *filp, *f;
	struct tty_file_private *priv;
	int    closecount = 0, n;
	int refs;

	if (!tty)
		return;

	f = tty_release_redirect(tty);

	tty_lock(tty);

	if (test_bit(TTY_HUPPED, &tty->flags)) {
		tty_unlock(tty);
		return;
	}

	 
	set_bit(TTY_HUPPING, &tty->flags);

	 
	check_tty_count(tty, "tty_hangup");

	spin_lock(&tty->files_lock);
	 
	list_for_each_entry(priv, &tty->tty_files, list) {
		filp = priv->file;
		if (filp->f_op->write_iter == redirected_tty_write)
			cons_filp = filp;
		if (filp->f_op->write_iter != tty_write)
			continue;
		closecount++;
		__tty_fasync(-1, filp, 0);	 
		filp->f_op = &hung_up_tty_fops;
	}
	spin_unlock(&tty->files_lock);

	refs = tty_signal_session_leader(tty, exit_session);
	 
	while (refs--)
		tty_kref_put(tty);

	tty_ldisc_hangup(tty, cons_filp != NULL);

	spin_lock_irq(&tty->ctrl.lock);
	clear_bit(TTY_THROTTLED, &tty->flags);
	clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
	put_pid(tty->ctrl.session);
	put_pid(tty->ctrl.pgrp);
	tty->ctrl.session = NULL;
	tty->ctrl.pgrp = NULL;
	tty->ctrl.pktstatus = 0;
	spin_unlock_irq(&tty->ctrl.lock);

	 
	if (cons_filp) {
		if (tty->ops->close)
			for (n = 0; n < closecount; n++)
				tty->ops->close(tty, cons_filp);
	} else if (tty->ops->hangup)
		tty->ops->hangup(tty);
	 
	set_bit(TTY_HUPPED, &tty->flags);
	clear_bit(TTY_HUPPING, &tty->flags);
	tty_unlock(tty);

	if (f)
		fput(f);
}

static void do_tty_hangup(struct work_struct *work)
{
	struct tty_struct *tty =
		container_of(work, struct tty_struct, hangup_work);

	__tty_hangup(tty, 0);
}

 
void tty_hangup(struct tty_struct *tty)
{
	tty_debug_hangup(tty, "hangup\n");
	schedule_work(&tty->hangup_work);
}
EXPORT_SYMBOL(tty_hangup);

 
void tty_vhangup(struct tty_struct *tty)
{
	tty_debug_hangup(tty, "vhangup\n");
	__tty_hangup(tty, 0);
}
EXPORT_SYMBOL(tty_vhangup);


 
void tty_vhangup_self(void)
{
	struct tty_struct *tty;

	tty = get_current_tty();
	if (tty) {
		tty_vhangup(tty);
		tty_kref_put(tty);
	}
}

 
void tty_vhangup_session(struct tty_struct *tty)
{
	tty_debug_hangup(tty, "session hangup\n");
	__tty_hangup(tty, 1);
}

 
int tty_hung_up_p(struct file *filp)
{
	return (filp && filp->f_op == &hung_up_tty_fops);
}
EXPORT_SYMBOL(tty_hung_up_p);

void __stop_tty(struct tty_struct *tty)
{
	if (tty->flow.stopped)
		return;
	tty->flow.stopped = true;
	if (tty->ops->stop)
		tty->ops->stop(tty);
}

 
void stop_tty(struct tty_struct *tty)
{
	unsigned long flags;

	spin_lock_irqsave(&tty->flow.lock, flags);
	__stop_tty(tty);
	spin_unlock_irqrestore(&tty->flow.lock, flags);
}
EXPORT_SYMBOL(stop_tty);

void __start_tty(struct tty_struct *tty)
{
	if (!tty->flow.stopped || tty->flow.tco_stopped)
		return;
	tty->flow.stopped = false;
	if (tty->ops->start)
		tty->ops->start(tty);
	tty_wakeup(tty);
}

 
void start_tty(struct tty_struct *tty)
{
	unsigned long flags;

	spin_lock_irqsave(&tty->flow.lock, flags);
	__start_tty(tty);
	spin_unlock_irqrestore(&tty->flow.lock, flags);
}
EXPORT_SYMBOL(start_tty);

static void tty_update_time(struct tty_struct *tty, bool mtime)
{
	time64_t sec = ktime_get_real_seconds();
	struct tty_file_private *priv;

	spin_lock(&tty->files_lock);
	list_for_each_entry(priv, &tty->tty_files, list) {
		struct inode *inode = file_inode(priv->file);
		struct timespec64 *time = mtime ? &inode->i_mtime : &inode->i_atime;

		 
		if ((sec ^ time->tv_sec) & ~7)
			time->tv_sec = sec;
	}
	spin_unlock(&tty->files_lock);
}

 
static ssize_t iterate_tty_read(struct tty_ldisc *ld, struct tty_struct *tty,
				struct file *file, struct iov_iter *to)
{
	void *cookie = NULL;
	unsigned long offset = 0;
	char kernel_buf[64];
	ssize_t retval = 0;
	size_t copied, count = iov_iter_count(to);

	do {
		ssize_t size = min(count, sizeof(kernel_buf));

		size = ld->ops->read(tty, file, kernel_buf, size, &cookie, offset);
		if (!size)
			break;

		if (size < 0) {
			 
			if (retval)
				break;
			retval = size;

			 
			if (retval == -EOVERFLOW)
				offset = 0;
			break;
		}

		copied = copy_to_iter(kernel_buf, size, to);
		offset += copied;
		count -= copied;

		 
		if (unlikely(copied != size)) {
			count = 0;
			retval = -EFAULT;
		}
	} while (cookie);

	 
	memzero_explicit(kernel_buf, sizeof(kernel_buf));
	return offset ? offset : retval;
}


 
static ssize_t tty_read(struct kiocb *iocb, struct iov_iter *to)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file_inode(file);
	struct tty_struct *tty = file_tty(file);
	struct tty_ldisc *ld;
	ssize_t ret;

	if (tty_paranoia_check(tty, inode, "tty_read"))
		return -EIO;
	if (!tty || tty_io_error(tty))
		return -EIO;

	 
	ld = tty_ldisc_ref_wait(tty);
	if (!ld)
		return hung_up_tty_read(iocb, to);
	ret = -EIO;
	if (ld->ops->read)
		ret = iterate_tty_read(ld, tty, file, to);
	tty_ldisc_deref(ld);

	if (ret > 0)
		tty_update_time(tty, false);

	return ret;
}

void tty_write_unlock(struct tty_struct *tty)
{
	mutex_unlock(&tty->atomic_write_lock);
	wake_up_interruptible_poll(&tty->write_wait, EPOLLOUT);
}

int tty_write_lock(struct tty_struct *tty, bool ndelay)
{
	if (!mutex_trylock(&tty->atomic_write_lock)) {
		if (ndelay)
			return -EAGAIN;
		if (mutex_lock_interruptible(&tty->atomic_write_lock))
			return -ERESTARTSYS;
	}
	return 0;
}

 
static ssize_t iterate_tty_write(struct tty_ldisc *ld, struct tty_struct *tty,
				 struct file *file, struct iov_iter *from)
{
	size_t chunk, count = iov_iter_count(from);
	ssize_t ret, written = 0;

	ret = tty_write_lock(tty, file->f_flags & O_NDELAY);
	if (ret < 0)
		return ret;

	 
	chunk = 2048;
	if (test_bit(TTY_NO_WRITE_SPLIT, &tty->flags))
		chunk = 65536;
	if (count < chunk)
		chunk = count;

	 
	if (tty->write_cnt < chunk) {
		unsigned char *buf_chunk;

		if (chunk < 1024)
			chunk = 1024;

		buf_chunk = kvmalloc(chunk, GFP_KERNEL | __GFP_RETRY_MAYFAIL);
		if (!buf_chunk) {
			ret = -ENOMEM;
			goto out;
		}
		kvfree(tty->write_buf);
		tty->write_cnt = chunk;
		tty->write_buf = buf_chunk;
	}

	 
	for (;;) {
		size_t size = min(chunk, count);

		ret = -EFAULT;
		if (copy_from_iter(tty->write_buf, size, from) != size)
			break;

		ret = ld->ops->write(tty, file, tty->write_buf, size);
		if (ret <= 0)
			break;

		written += ret;
		if (ret > size)
			break;

		 
		if (ret != size)
			iov_iter_revert(from, size-ret);

		count -= ret;
		if (!count)
			break;
		ret = -ERESTARTSYS;
		if (signal_pending(current))
			break;
		cond_resched();
	}
	if (written) {
		tty_update_time(tty, true);
		ret = written;
	}
out:
	tty_write_unlock(tty);
	return ret;
}

 
void tty_write_message(struct tty_struct *tty, char *msg)
{
	if (tty) {
		mutex_lock(&tty->atomic_write_lock);
		tty_lock(tty);
		if (tty->ops->write && tty->count > 0)
			tty->ops->write(tty, msg, strlen(msg));
		tty_unlock(tty);
		tty_write_unlock(tty);
	}
}

static ssize_t file_tty_write(struct file *file, struct kiocb *iocb, struct iov_iter *from)
{
	struct tty_struct *tty = file_tty(file);
	struct tty_ldisc *ld;
	ssize_t ret;

	if (tty_paranoia_check(tty, file_inode(file), "tty_write"))
		return -EIO;
	if (!tty || !tty->ops->write ||	tty_io_error(tty))
		return -EIO;
	 
	if (tty->ops->write_room == NULL)
		tty_err(tty, "missing write_room method\n");
	ld = tty_ldisc_ref_wait(tty);
	if (!ld)
		return hung_up_tty_write(iocb, from);
	if (!ld->ops->write)
		ret = -EIO;
	else
		ret = iterate_tty_write(ld, tty, file, from);
	tty_ldisc_deref(ld);
	return ret;
}

 
static ssize_t tty_write(struct kiocb *iocb, struct iov_iter *from)
{
	return file_tty_write(iocb->ki_filp, iocb, from);
}

ssize_t redirected_tty_write(struct kiocb *iocb, struct iov_iter *iter)
{
	struct file *p = NULL;

	spin_lock(&redirect_lock);
	if (redirect)
		p = get_file(redirect);
	spin_unlock(&redirect_lock);

	 
	if (p) {
		ssize_t res;

		res = file_tty_write(p, iocb, iter);
		fput(p);
		return res;
	}
	return tty_write(iocb, iter);
}

 
int tty_send_xchar(struct tty_struct *tty, char ch)
{
	bool was_stopped = tty->flow.stopped;

	if (tty->ops->send_xchar) {
		down_read(&tty->termios_rwsem);
		tty->ops->send_xchar(tty, ch);
		up_read(&tty->termios_rwsem);
		return 0;
	}

	if (tty_write_lock(tty, false) < 0)
		return -ERESTARTSYS;

	down_read(&tty->termios_rwsem);
	if (was_stopped)
		start_tty(tty);
	tty->ops->write(tty, &ch, 1);
	if (was_stopped)
		stop_tty(tty);
	up_read(&tty->termios_rwsem);
	tty_write_unlock(tty);
	return 0;
}

 
static void pty_line_name(struct tty_driver *driver, int index, char *p)
{
	static const char ptychar[] = "pqrstuvwxyzabcde";
	int i = index + driver->name_base;
	 
	sprintf(p, "%s%c%x",
		driver->subtype == PTY_TYPE_SLAVE ? "tty" : driver->name,
		ptychar[i >> 4 & 0xf], i & 0xf);
}

 
static ssize_t tty_line_name(struct tty_driver *driver, int index, char *p)
{
	if (driver->flags & TTY_DRIVER_UNNUMBERED_NODE)
		return sprintf(p, "%s", driver->name);
	else
		return sprintf(p, "%s%d", driver->name,
			       index + driver->name_base);
}

 
static struct tty_struct *tty_driver_lookup_tty(struct tty_driver *driver,
		struct file *file, int idx)
{
	struct tty_struct *tty;

	if (driver->ops->lookup) {
		if (!file)
			tty = ERR_PTR(-EIO);
		else
			tty = driver->ops->lookup(driver, file, idx);
	} else {
		if (idx >= driver->num)
			return ERR_PTR(-EINVAL);
		tty = driver->ttys[idx];
	}
	if (!IS_ERR(tty))
		tty_kref_get(tty);
	return tty;
}

 
void tty_init_termios(struct tty_struct *tty)
{
	struct ktermios *tp;
	int idx = tty->index;

	if (tty->driver->flags & TTY_DRIVER_RESET_TERMIOS)
		tty->termios = tty->driver->init_termios;
	else {
		 
		tp = tty->driver->termios[idx];
		if (tp != NULL) {
			tty->termios = *tp;
			tty->termios.c_line  = tty->driver->init_termios.c_line;
		} else
			tty->termios = tty->driver->init_termios;
	}
	 
	tty->termios.c_ispeed = tty_termios_input_baud_rate(&tty->termios);
	tty->termios.c_ospeed = tty_termios_baud_rate(&tty->termios);
}
EXPORT_SYMBOL_GPL(tty_init_termios);

 
int tty_standard_install(struct tty_driver *driver, struct tty_struct *tty)
{
	tty_init_termios(tty);
	tty_driver_kref_get(driver);
	tty->count++;
	driver->ttys[tty->index] = tty;
	return 0;
}
EXPORT_SYMBOL_GPL(tty_standard_install);

 
static int tty_driver_install_tty(struct tty_driver *driver,
						struct tty_struct *tty)
{
	return driver->ops->install ? driver->ops->install(driver, tty) :
		tty_standard_install(driver, tty);
}

 
static void tty_driver_remove_tty(struct tty_driver *driver, struct tty_struct *tty)
{
	if (driver->ops->remove)
		driver->ops->remove(driver, tty);
	else
		driver->ttys[tty->index] = NULL;
}

 
static int tty_reopen(struct tty_struct *tty)
{
	struct tty_driver *driver = tty->driver;
	struct tty_ldisc *ld;
	int retval = 0;

	if (driver->type == TTY_DRIVER_TYPE_PTY &&
	    driver->subtype == PTY_TYPE_MASTER)
		return -EIO;

	if (!tty->count)
		return -EAGAIN;

	if (test_bit(TTY_EXCLUSIVE, &tty->flags) && !capable(CAP_SYS_ADMIN))
		return -EBUSY;

	ld = tty_ldisc_ref_wait(tty);
	if (ld) {
		tty_ldisc_deref(ld);
	} else {
		retval = tty_ldisc_lock(tty, 5 * HZ);
		if (retval)
			return retval;

		if (!tty->ldisc)
			retval = tty_ldisc_reinit(tty, tty->termios.c_line);
		tty_ldisc_unlock(tty);
	}

	if (retval == 0)
		tty->count++;

	return retval;
}

 
struct tty_struct *tty_init_dev(struct tty_driver *driver, int idx)
{
	struct tty_struct *tty;
	int retval;

	 

	if (!try_module_get(driver->owner))
		return ERR_PTR(-ENODEV);

	tty = alloc_tty_struct(driver, idx);
	if (!tty) {
		retval = -ENOMEM;
		goto err_module_put;
	}

	tty_lock(tty);
	retval = tty_driver_install_tty(driver, tty);
	if (retval < 0)
		goto err_free_tty;

	if (!tty->port)
		tty->port = driver->ports[idx];

	if (WARN_RATELIMIT(!tty->port,
			"%s: %s driver does not set tty->port. This would crash the kernel. Fix the driver!\n",
			__func__, tty->driver->name)) {
		retval = -EINVAL;
		goto err_release_lock;
	}

	retval = tty_ldisc_lock(tty, 5 * HZ);
	if (retval)
		goto err_release_lock;
	tty->port->itty = tty;

	 
	retval = tty_ldisc_setup(tty, tty->link);
	if (retval)
		goto err_release_tty;
	tty_ldisc_unlock(tty);
	 
	return tty;

err_free_tty:
	tty_unlock(tty);
	free_tty_struct(tty);
err_module_put:
	module_put(driver->owner);
	return ERR_PTR(retval);

	 
err_release_tty:
	tty_ldisc_unlock(tty);
	tty_info_ratelimited(tty, "ldisc open failed (%d), clearing slot %d\n",
			     retval, idx);
err_release_lock:
	tty_unlock(tty);
	release_tty(tty, idx);
	return ERR_PTR(retval);
}

 
void tty_save_termios(struct tty_struct *tty)
{
	struct ktermios *tp;
	int idx = tty->index;

	 
	if (tty->driver->flags & TTY_DRIVER_RESET_TERMIOS)
		return;

	 
	tp = tty->driver->termios[idx];
	if (tp == NULL) {
		tp = kmalloc(sizeof(*tp), GFP_KERNEL);
		if (tp == NULL)
			return;
		tty->driver->termios[idx] = tp;
	}
	*tp = tty->termios;
}
EXPORT_SYMBOL_GPL(tty_save_termios);

 
static void tty_flush_works(struct tty_struct *tty)
{
	flush_work(&tty->SAK_work);
	flush_work(&tty->hangup_work);
	if (tty->link) {
		flush_work(&tty->link->SAK_work);
		flush_work(&tty->link->hangup_work);
	}
}

 
static void release_one_tty(struct work_struct *work)
{
	struct tty_struct *tty =
		container_of(work, struct tty_struct, hangup_work);
	struct tty_driver *driver = tty->driver;
	struct module *owner = driver->owner;

	if (tty->ops->cleanup)
		tty->ops->cleanup(tty);

	tty_driver_kref_put(driver);
	module_put(owner);

	spin_lock(&tty->files_lock);
	list_del_init(&tty->tty_files);
	spin_unlock(&tty->files_lock);

	put_pid(tty->ctrl.pgrp);
	put_pid(tty->ctrl.session);
	free_tty_struct(tty);
}

static void queue_release_one_tty(struct kref *kref)
{
	struct tty_struct *tty = container_of(kref, struct tty_struct, kref);

	 
	INIT_WORK(&tty->hangup_work, release_one_tty);
	schedule_work(&tty->hangup_work);
}

 
void tty_kref_put(struct tty_struct *tty)
{
	if (tty)
		kref_put(&tty->kref, queue_release_one_tty);
}
EXPORT_SYMBOL(tty_kref_put);

 
static void release_tty(struct tty_struct *tty, int idx)
{
	 
	WARN_ON(tty->index != idx);
	WARN_ON(!mutex_is_locked(&tty_mutex));
	if (tty->ops->shutdown)
		tty->ops->shutdown(tty);
	tty_save_termios(tty);
	tty_driver_remove_tty(tty->driver, tty);
	if (tty->port)
		tty->port->itty = NULL;
	if (tty->link)
		tty->link->port->itty = NULL;
	if (tty->port)
		tty_buffer_cancel_work(tty->port);
	if (tty->link)
		tty_buffer_cancel_work(tty->link->port);

	tty_kref_put(tty->link);
	tty_kref_put(tty);
}

 
static int tty_release_checks(struct tty_struct *tty, int idx)
{
#ifdef TTY_PARANOIA_CHECK
	if (idx < 0 || idx >= tty->driver->num) {
		tty_debug(tty, "bad idx %d\n", idx);
		return -1;
	}

	 
	if (tty->driver->flags & TTY_DRIVER_DEVPTS_MEM)
		return 0;

	if (tty != tty->driver->ttys[idx]) {
		tty_debug(tty, "bad driver table[%d] = %p\n",
			  idx, tty->driver->ttys[idx]);
		return -1;
	}
	if (tty->driver->other) {
		struct tty_struct *o_tty = tty->link;

		if (o_tty != tty->driver->other->ttys[idx]) {
			tty_debug(tty, "bad other table[%d] = %p\n",
				  idx, tty->driver->other->ttys[idx]);
			return -1;
		}
		if (o_tty->link != tty) {
			tty_debug(tty, "bad link = %p\n", o_tty->link);
			return -1;
		}
	}
#endif
	return 0;
}

 
void tty_kclose(struct tty_struct *tty)
{
	 
	tty_ldisc_release(tty);

	 
	tty_flush_works(tty);

	tty_debug_hangup(tty, "freeing structure\n");
	 
	mutex_lock(&tty_mutex);
	tty_port_set_kopened(tty->port, 0);
	release_tty(tty, tty->index);
	mutex_unlock(&tty_mutex);
}
EXPORT_SYMBOL_GPL(tty_kclose);

 
void tty_release_struct(struct tty_struct *tty, int idx)
{
	 
	tty_ldisc_release(tty);

	 
	tty_flush_works(tty);

	tty_debug_hangup(tty, "freeing structure\n");
	 
	mutex_lock(&tty_mutex);
	release_tty(tty, idx);
	mutex_unlock(&tty_mutex);
}
EXPORT_SYMBOL_GPL(tty_release_struct);

 
int tty_release(struct inode *inode, struct file *filp)
{
	struct tty_struct *tty = file_tty(filp);
	struct tty_struct *o_tty = NULL;
	int	do_sleep, final;
	int	idx;
	long	timeout = 0;
	int	once = 1;

	if (tty_paranoia_check(tty, inode, __func__))
		return 0;

	tty_lock(tty);
	check_tty_count(tty, __func__);

	__tty_fasync(-1, filp, 0);

	idx = tty->index;
	if (tty->driver->type == TTY_DRIVER_TYPE_PTY &&
	    tty->driver->subtype == PTY_TYPE_MASTER)
		o_tty = tty->link;

	if (tty_release_checks(tty, idx)) {
		tty_unlock(tty);
		return 0;
	}

	tty_debug_hangup(tty, "releasing (count=%d)\n", tty->count);

	if (tty->ops->close)
		tty->ops->close(tty, filp);

	 
	tty_lock_slave(o_tty);

	 
	while (1) {
		do_sleep = 0;

		if (tty->count <= 1) {
			if (waitqueue_active(&tty->read_wait)) {
				wake_up_poll(&tty->read_wait, EPOLLIN);
				do_sleep++;
			}
			if (waitqueue_active(&tty->write_wait)) {
				wake_up_poll(&tty->write_wait, EPOLLOUT);
				do_sleep++;
			}
		}
		if (o_tty && o_tty->count <= 1) {
			if (waitqueue_active(&o_tty->read_wait)) {
				wake_up_poll(&o_tty->read_wait, EPOLLIN);
				do_sleep++;
			}
			if (waitqueue_active(&o_tty->write_wait)) {
				wake_up_poll(&o_tty->write_wait, EPOLLOUT);
				do_sleep++;
			}
		}
		if (!do_sleep)
			break;

		if (once) {
			once = 0;
			tty_warn(tty, "read/write wait queue active!\n");
		}
		schedule_timeout_killable(timeout);
		if (timeout < 120 * HZ)
			timeout = 2 * timeout + 1;
		else
			timeout = MAX_SCHEDULE_TIMEOUT;
	}

	if (o_tty) {
		if (--o_tty->count < 0) {
			tty_warn(tty, "bad slave count (%d)\n", o_tty->count);
			o_tty->count = 0;
		}
	}
	if (--tty->count < 0) {
		tty_warn(tty, "bad tty->count (%d)\n", tty->count);
		tty->count = 0;
	}

	 
	tty_del_file(filp);

	 
	if (!tty->count) {
		read_lock(&tasklist_lock);
		session_clear_tty(tty->ctrl.session);
		if (o_tty)
			session_clear_tty(o_tty->ctrl.session);
		read_unlock(&tasklist_lock);
	}

	 
	final = !tty->count && !(o_tty && o_tty->count);

	tty_unlock_slave(o_tty);
	tty_unlock(tty);

	 

	if (!final)
		return 0;

	tty_debug_hangup(tty, "final close\n");

	tty_release_struct(tty, idx);
	return 0;
}

 
static struct tty_struct *tty_open_current_tty(dev_t device, struct file *filp)
{
	struct tty_struct *tty;
	int retval;

	if (device != MKDEV(TTYAUX_MAJOR, 0))
		return NULL;

	tty = get_current_tty();
	if (!tty)
		return ERR_PTR(-ENXIO);

	filp->f_flags |= O_NONBLOCK;  
	 
	tty_lock(tty);
	tty_kref_put(tty);	 

	retval = tty_reopen(tty);
	if (retval < 0) {
		tty_unlock(tty);
		tty = ERR_PTR(retval);
	}
	return tty;
}

 
static struct tty_driver *tty_lookup_driver(dev_t device, struct file *filp,
		int *index)
{
	struct tty_driver *driver = NULL;

	switch (device) {
#ifdef CONFIG_VT
	case MKDEV(TTY_MAJOR, 0): {
		extern struct tty_driver *console_driver;

		driver = tty_driver_kref_get(console_driver);
		*index = fg_console;
		break;
	}
#endif
	case MKDEV(TTYAUX_MAJOR, 1): {
		struct tty_driver *console_driver = console_device(index);

		if (console_driver) {
			driver = tty_driver_kref_get(console_driver);
			if (driver && filp) {
				 
				filp->f_flags |= O_NONBLOCK;
				break;
			}
		}
		if (driver)
			tty_driver_kref_put(driver);
		return ERR_PTR(-ENODEV);
	}
	default:
		driver = get_tty_driver(device, index);
		if (!driver)
			return ERR_PTR(-ENODEV);
		break;
	}
	return driver;
}

static struct tty_struct *tty_kopen(dev_t device, int shared)
{
	struct tty_struct *tty;
	struct tty_driver *driver;
	int index = -1;

	mutex_lock(&tty_mutex);
	driver = tty_lookup_driver(device, NULL, &index);
	if (IS_ERR(driver)) {
		mutex_unlock(&tty_mutex);
		return ERR_CAST(driver);
	}

	 
	tty = tty_driver_lookup_tty(driver, NULL, index);
	if (IS_ERR(tty) || shared)
		goto out;

	if (tty) {
		 
		tty_kref_put(tty);
		tty = ERR_PTR(-EBUSY);
	} else {  
		tty = tty_init_dev(driver, index);
		if (IS_ERR(tty))
			goto out;
		tty_port_set_kopened(tty->port, 1);
	}
out:
	mutex_unlock(&tty_mutex);
	tty_driver_kref_put(driver);
	return tty;
}

 
struct tty_struct *tty_kopen_exclusive(dev_t device)
{
	return tty_kopen(device, 0);
}
EXPORT_SYMBOL_GPL(tty_kopen_exclusive);

 
struct tty_struct *tty_kopen_shared(dev_t device)
{
	return tty_kopen(device, 1);
}
EXPORT_SYMBOL_GPL(tty_kopen_shared);

 
static struct tty_struct *tty_open_by_driver(dev_t device,
					     struct file *filp)
{
	struct tty_struct *tty;
	struct tty_driver *driver = NULL;
	int index = -1;
	int retval;

	mutex_lock(&tty_mutex);
	driver = tty_lookup_driver(device, filp, &index);
	if (IS_ERR(driver)) {
		mutex_unlock(&tty_mutex);
		return ERR_CAST(driver);
	}

	 
	tty = tty_driver_lookup_tty(driver, filp, index);
	if (IS_ERR(tty)) {
		mutex_unlock(&tty_mutex);
		goto out;
	}

	if (tty) {
		if (tty_port_kopened(tty->port)) {
			tty_kref_put(tty);
			mutex_unlock(&tty_mutex);
			tty = ERR_PTR(-EBUSY);
			goto out;
		}
		mutex_unlock(&tty_mutex);
		retval = tty_lock_interruptible(tty);
		tty_kref_put(tty);   
		if (retval) {
			if (retval == -EINTR)
				retval = -ERESTARTSYS;
			tty = ERR_PTR(retval);
			goto out;
		}
		retval = tty_reopen(tty);
		if (retval < 0) {
			tty_unlock(tty);
			tty = ERR_PTR(retval);
		}
	} else {  
		tty = tty_init_dev(driver, index);
		mutex_unlock(&tty_mutex);
	}
out:
	tty_driver_kref_put(driver);
	return tty;
}

 
static int tty_open(struct inode *inode, struct file *filp)
{
	struct tty_struct *tty;
	int noctty, retval;
	dev_t device = inode->i_rdev;
	unsigned saved_flags = filp->f_flags;

	nonseekable_open(inode, filp);

retry_open:
	retval = tty_alloc_file(filp);
	if (retval)
		return -ENOMEM;

	tty = tty_open_current_tty(device, filp);
	if (!tty)
		tty = tty_open_by_driver(device, filp);

	if (IS_ERR(tty)) {
		tty_free_file(filp);
		retval = PTR_ERR(tty);
		if (retval != -EAGAIN || signal_pending(current))
			return retval;
		schedule();
		goto retry_open;
	}

	tty_add_file(tty, filp);

	check_tty_count(tty, __func__);
	tty_debug_hangup(tty, "opening (count=%d)\n", tty->count);

	if (tty->ops->open)
		retval = tty->ops->open(tty, filp);
	else
		retval = -ENODEV;
	filp->f_flags = saved_flags;

	if (retval) {
		tty_debug_hangup(tty, "open error %d, releasing\n", retval);

		tty_unlock(tty);  
		tty_release(inode, filp);
		if (retval != -ERESTARTSYS)
			return retval;

		if (signal_pending(current))
			return retval;

		schedule();
		 
		if (tty_hung_up_p(filp))
			filp->f_op = &tty_fops;
		goto retry_open;
	}
	clear_bit(TTY_HUPPED, &tty->flags);

	noctty = (filp->f_flags & O_NOCTTY) ||
		 (IS_ENABLED(CONFIG_VT) && device == MKDEV(TTY_MAJOR, 0)) ||
		 device == MKDEV(TTYAUX_MAJOR, 1) ||
		 (tty->driver->type == TTY_DRIVER_TYPE_PTY &&
		  tty->driver->subtype == PTY_TYPE_MASTER);
	if (!noctty)
		tty_open_proc_set_tty(filp, tty);
	tty_unlock(tty);
	return 0;
}


 
static __poll_t tty_poll(struct file *filp, poll_table *wait)
{
	struct tty_struct *tty = file_tty(filp);
	struct tty_ldisc *ld;
	__poll_t ret = 0;

	if (tty_paranoia_check(tty, file_inode(filp), "tty_poll"))
		return 0;

	ld = tty_ldisc_ref_wait(tty);
	if (!ld)
		return hung_up_tty_poll(filp, wait);
	if (ld->ops->poll)
		ret = ld->ops->poll(tty, filp, wait);
	tty_ldisc_deref(ld);
	return ret;
}

static int __tty_fasync(int fd, struct file *filp, int on)
{
	struct tty_struct *tty = file_tty(filp);
	unsigned long flags;
	int retval = 0;

	if (tty_paranoia_check(tty, file_inode(filp), "tty_fasync"))
		goto out;

	retval = fasync_helper(fd, filp, on, &tty->fasync);
	if (retval <= 0)
		goto out;

	if (on) {
		enum pid_type type;
		struct pid *pid;

		spin_lock_irqsave(&tty->ctrl.lock, flags);
		if (tty->ctrl.pgrp) {
			pid = tty->ctrl.pgrp;
			type = PIDTYPE_PGID;
		} else {
			pid = task_pid(current);
			type = PIDTYPE_TGID;
		}
		get_pid(pid);
		spin_unlock_irqrestore(&tty->ctrl.lock, flags);
		__f_setown(filp, pid, type, 0);
		put_pid(pid);
		retval = 0;
	}
out:
	return retval;
}

static int tty_fasync(int fd, struct file *filp, int on)
{
	struct tty_struct *tty = file_tty(filp);
	int retval = -ENOTTY;

	tty_lock(tty);
	if (!tty_hung_up_p(filp))
		retval = __tty_fasync(fd, filp, on);
	tty_unlock(tty);

	return retval;
}

static bool tty_legacy_tiocsti __read_mostly = IS_ENABLED(CONFIG_LEGACY_TIOCSTI);
 
static int tiocsti(struct tty_struct *tty, char __user *p)
{
	char ch, mbz = 0;
	struct tty_ldisc *ld;

	if (!tty_legacy_tiocsti && !capable(CAP_SYS_ADMIN))
		return -EIO;

	if ((current->signal->tty != tty) && !capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (get_user(ch, p))
		return -EFAULT;
	tty_audit_tiocsti(tty, ch);
	ld = tty_ldisc_ref_wait(tty);
	if (!ld)
		return -EIO;
	tty_buffer_lock_exclusive(tty->port);
	if (ld->ops->receive_buf)
		ld->ops->receive_buf(tty, &ch, &mbz, 1);
	tty_buffer_unlock_exclusive(tty->port);
	tty_ldisc_deref(ld);
	return 0;
}

 
static int tiocgwinsz(struct tty_struct *tty, struct winsize __user *arg)
{
	int err;

	mutex_lock(&tty->winsize_mutex);
	err = copy_to_user(arg, &tty->winsize, sizeof(*arg));
	mutex_unlock(&tty->winsize_mutex);

	return err ? -EFAULT : 0;
}

 
int tty_do_resize(struct tty_struct *tty, struct winsize *ws)
{
	struct pid *pgrp;

	 
	mutex_lock(&tty->winsize_mutex);
	if (!memcmp(ws, &tty->winsize, sizeof(*ws)))
		goto done;

	 
	pgrp = tty_get_pgrp(tty);
	if (pgrp)
		kill_pgrp(pgrp, SIGWINCH, 1);
	put_pid(pgrp);

	tty->winsize = *ws;
done:
	mutex_unlock(&tty->winsize_mutex);
	return 0;
}
EXPORT_SYMBOL(tty_do_resize);

 
static int tiocswinsz(struct tty_struct *tty, struct winsize __user *arg)
{
	struct winsize tmp_ws;

	if (copy_from_user(&tmp_ws, arg, sizeof(*arg)))
		return -EFAULT;

	if (tty->ops->resize)
		return tty->ops->resize(tty, &tmp_ws);
	else
		return tty_do_resize(tty, &tmp_ws);
}

 
static int tioccons(struct file *file)
{
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (file->f_op->write_iter == redirected_tty_write) {
		struct file *f;

		spin_lock(&redirect_lock);
		f = redirect;
		redirect = NULL;
		spin_unlock(&redirect_lock);
		if (f)
			fput(f);
		return 0;
	}
	if (file->f_op->write_iter != tty_write)
		return -ENOTTY;
	if (!(file->f_mode & FMODE_WRITE))
		return -EBADF;
	if (!(file->f_mode & FMODE_CAN_WRITE))
		return -EINVAL;
	spin_lock(&redirect_lock);
	if (redirect) {
		spin_unlock(&redirect_lock);
		return -EBUSY;
	}
	redirect = get_file(file);
	spin_unlock(&redirect_lock);
	return 0;
}

 
static int tiocsetd(struct tty_struct *tty, int __user *p)
{
	int disc;
	int ret;

	if (get_user(disc, p))
		return -EFAULT;

	ret = tty_set_ldisc(tty, disc);

	return ret;
}

 
static int tiocgetd(struct tty_struct *tty, int __user *p)
{
	struct tty_ldisc *ld;
	int ret;

	ld = tty_ldisc_ref_wait(tty);
	if (!ld)
		return -EIO;
	ret = put_user(ld->ops->num, p);
	tty_ldisc_deref(ld);
	return ret;
}

 
static int send_break(struct tty_struct *tty, unsigned int duration)
{
	int retval;

	if (tty->ops->break_ctl == NULL)
		return 0;

	if (tty->driver->flags & TTY_DRIVER_HARDWARE_BREAK)
		return tty->ops->break_ctl(tty, duration);

	 
	if (tty_write_lock(tty, false) < 0)
		return -EINTR;

	retval = tty->ops->break_ctl(tty, -1);
	if (!retval) {
		msleep_interruptible(duration);
		retval = tty->ops->break_ctl(tty, 0);
	} else if (retval == -EOPNOTSUPP) {
		 
		retval = 0;
	}
	tty_write_unlock(tty);

	if (signal_pending(current))
		retval = -EINTR;

	return retval;
}

 
static int tty_tiocmget(struct tty_struct *tty, int __user *p)
{
	int retval = -ENOTTY;

	if (tty->ops->tiocmget) {
		retval = tty->ops->tiocmget(tty);

		if (retval >= 0)
			retval = put_user(retval, p);
	}
	return retval;
}

 
static int tty_tiocmset(struct tty_struct *tty, unsigned int cmd,
	     unsigned __user *p)
{
	int retval;
	unsigned int set, clear, val;

	if (tty->ops->tiocmset == NULL)
		return -ENOTTY;

	retval = get_user(val, p);
	if (retval)
		return retval;
	set = clear = 0;
	switch (cmd) {
	case TIOCMBIS:
		set = val;
		break;
	case TIOCMBIC:
		clear = val;
		break;
	case TIOCMSET:
		set = val;
		clear = ~val;
		break;
	}
	set &= TIOCM_DTR|TIOCM_RTS|TIOCM_OUT1|TIOCM_OUT2|TIOCM_LOOP;
	clear &= TIOCM_DTR|TIOCM_RTS|TIOCM_OUT1|TIOCM_OUT2|TIOCM_LOOP;
	return tty->ops->tiocmset(tty, set, clear);
}

 
int tty_get_icount(struct tty_struct *tty,
		   struct serial_icounter_struct *icount)
{
	memset(icount, 0, sizeof(*icount));

	if (tty->ops->get_icount)
		return tty->ops->get_icount(tty, icount);
	else
		return -ENOTTY;
}
EXPORT_SYMBOL_GPL(tty_get_icount);

static int tty_tiocgicount(struct tty_struct *tty, void __user *arg)
{
	struct serial_icounter_struct icount;
	int retval;

	retval = tty_get_icount(tty, &icount);
	if (retval != 0)
		return retval;

	if (copy_to_user(arg, &icount, sizeof(icount)))
		return -EFAULT;
	return 0;
}

static int tty_set_serial(struct tty_struct *tty, struct serial_struct *ss)
{
	char comm[TASK_COMM_LEN];
	int flags;

	flags = ss->flags & ASYNC_DEPRECATED;

	if (flags)
		pr_warn_ratelimited("%s: '%s' is using deprecated serial flags (with no effect): %.8x\n",
				__func__, get_task_comm(comm, current), flags);

	if (!tty->ops->set_serial)
		return -ENOTTY;

	return tty->ops->set_serial(tty, ss);
}

static int tty_tiocsserial(struct tty_struct *tty, struct serial_struct __user *ss)
{
	struct serial_struct v;

	if (copy_from_user(&v, ss, sizeof(*ss)))
		return -EFAULT;

	return tty_set_serial(tty, &v);
}

static int tty_tiocgserial(struct tty_struct *tty, struct serial_struct __user *ss)
{
	struct serial_struct v;
	int err;

	memset(&v, 0, sizeof(v));
	if (!tty->ops->get_serial)
		return -ENOTTY;
	err = tty->ops->get_serial(tty, &v);
	if (!err && copy_to_user(ss, &v, sizeof(v)))
		err = -EFAULT;
	return err;
}

 
static struct tty_struct *tty_pair_get_tty(struct tty_struct *tty)
{
	if (tty->driver->type == TTY_DRIVER_TYPE_PTY &&
	    tty->driver->subtype == PTY_TYPE_MASTER)
		tty = tty->link;
	return tty;
}

 
long tty_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tty_struct *tty = file_tty(file);
	struct tty_struct *real_tty;
	void __user *p = (void __user *)arg;
	int retval;
	struct tty_ldisc *ld;

	if (tty_paranoia_check(tty, file_inode(file), "tty_ioctl"))
		return -EINVAL;

	real_tty = tty_pair_get_tty(tty);

	 
	switch (cmd) {
	case TIOCSETD:
	case TIOCSBRK:
	case TIOCCBRK:
	case TCSBRK:
	case TCSBRKP:
		retval = tty_check_change(tty);
		if (retval)
			return retval;
		if (cmd != TIOCCBRK) {
			tty_wait_until_sent(tty, 0);
			if (signal_pending(current))
				return -EINTR;
		}
		break;
	}

	 
	switch (cmd) {
	case TIOCSTI:
		return tiocsti(tty, p);
	case TIOCGWINSZ:
		return tiocgwinsz(real_tty, p);
	case TIOCSWINSZ:
		return tiocswinsz(real_tty, p);
	case TIOCCONS:
		return real_tty != tty ? -EINVAL : tioccons(file);
	case TIOCEXCL:
		set_bit(TTY_EXCLUSIVE, &tty->flags);
		return 0;
	case TIOCNXCL:
		clear_bit(TTY_EXCLUSIVE, &tty->flags);
		return 0;
	case TIOCGEXCL:
	{
		int excl = test_bit(TTY_EXCLUSIVE, &tty->flags);

		return put_user(excl, (int __user *)p);
	}
	case TIOCGETD:
		return tiocgetd(tty, p);
	case TIOCSETD:
		return tiocsetd(tty, p);
	case TIOCVHANGUP:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		tty_vhangup(tty);
		return 0;
	case TIOCGDEV:
	{
		unsigned int ret = new_encode_dev(tty_devnum(real_tty));

		return put_user(ret, (unsigned int __user *)p);
	}
	 
	case TIOCSBRK:	 
		if (tty->ops->break_ctl)
			return tty->ops->break_ctl(tty, -1);
		return 0;
	case TIOCCBRK:	 
		if (tty->ops->break_ctl)
			return tty->ops->break_ctl(tty, 0);
		return 0;
	case TCSBRK:    
		 
		if (!arg)
			return send_break(tty, 250);
		return 0;
	case TCSBRKP:	 
		return send_break(tty, arg ? arg*100 : 250);

	case TIOCMGET:
		return tty_tiocmget(tty, p);
	case TIOCMSET:
	case TIOCMBIC:
	case TIOCMBIS:
		return tty_tiocmset(tty, cmd, p);
	case TIOCGICOUNT:
		return tty_tiocgicount(tty, p);
	case TCFLSH:
		switch (arg) {
		case TCIFLUSH:
		case TCIOFLUSH:
		 
			tty_buffer_flush(tty, NULL);
			break;
		}
		break;
	case TIOCSSERIAL:
		return tty_tiocsserial(tty, p);
	case TIOCGSERIAL:
		return tty_tiocgserial(tty, p);
	case TIOCGPTPEER:
		 
		return ptm_open_peer(file, tty, (int)arg);
	default:
		retval = tty_jobctrl_ioctl(tty, real_tty, file, cmd, arg);
		if (retval != -ENOIOCTLCMD)
			return retval;
	}
	if (tty->ops->ioctl) {
		retval = tty->ops->ioctl(tty, cmd, arg);
		if (retval != -ENOIOCTLCMD)
			return retval;
	}
	ld = tty_ldisc_ref_wait(tty);
	if (!ld)
		return hung_up_tty_ioctl(file, cmd, arg);
	retval = -EINVAL;
	if (ld->ops->ioctl) {
		retval = ld->ops->ioctl(tty, cmd, arg);
		if (retval == -ENOIOCTLCMD)
			retval = -ENOTTY;
	}
	tty_ldisc_deref(ld);
	return retval;
}

#ifdef CONFIG_COMPAT

struct serial_struct32 {
	compat_int_t    type;
	compat_int_t    line;
	compat_uint_t   port;
	compat_int_t    irq;
	compat_int_t    flags;
	compat_int_t    xmit_fifo_size;
	compat_int_t    custom_divisor;
	compat_int_t    baud_base;
	unsigned short  close_delay;
	char    io_type;
	char    reserved_char;
	compat_int_t    hub6;
	unsigned short  closing_wait;  
	unsigned short  closing_wait2;  
	compat_uint_t   iomem_base;
	unsigned short  iomem_reg_shift;
	unsigned int    port_high;
	 
	compat_int_t    reserved;
};

static int compat_tty_tiocsserial(struct tty_struct *tty,
		struct serial_struct32 __user *ss)
{
	struct serial_struct32 v32;
	struct serial_struct v;

	if (copy_from_user(&v32, ss, sizeof(*ss)))
		return -EFAULT;

	memcpy(&v, &v32, offsetof(struct serial_struct32, iomem_base));
	v.iomem_base = compat_ptr(v32.iomem_base);
	v.iomem_reg_shift = v32.iomem_reg_shift;
	v.port_high = v32.port_high;
	v.iomap_base = 0;

	return tty_set_serial(tty, &v);
}

static int compat_tty_tiocgserial(struct tty_struct *tty,
			struct serial_struct32 __user *ss)
{
	struct serial_struct32 v32;
	struct serial_struct v;
	int err;

	memset(&v, 0, sizeof(v));
	memset(&v32, 0, sizeof(v32));

	if (!tty->ops->get_serial)
		return -ENOTTY;
	err = tty->ops->get_serial(tty, &v);
	if (!err) {
		memcpy(&v32, &v, offsetof(struct serial_struct32, iomem_base));
		v32.iomem_base = (unsigned long)v.iomem_base >> 32 ?
			0xfffffff : ptr_to_compat(v.iomem_base);
		v32.iomem_reg_shift = v.iomem_reg_shift;
		v32.port_high = v.port_high;
		if (copy_to_user(ss, &v32, sizeof(v32)))
			err = -EFAULT;
	}
	return err;
}
static long tty_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct tty_struct *tty = file_tty(file);
	struct tty_ldisc *ld;
	int retval = -ENOIOCTLCMD;

	switch (cmd) {
	case TIOCOUTQ:
	case TIOCSTI:
	case TIOCGWINSZ:
	case TIOCSWINSZ:
	case TIOCGEXCL:
	case TIOCGETD:
	case TIOCSETD:
	case TIOCGDEV:
	case TIOCMGET:
	case TIOCMSET:
	case TIOCMBIC:
	case TIOCMBIS:
	case TIOCGICOUNT:
	case TIOCGPGRP:
	case TIOCSPGRP:
	case TIOCGSID:
	case TIOCSERGETLSR:
	case TIOCGRS485:
	case TIOCSRS485:
#ifdef TIOCGETP
	case TIOCGETP:
	case TIOCSETP:
	case TIOCSETN:
#endif
#ifdef TIOCGETC
	case TIOCGETC:
	case TIOCSETC:
#endif
#ifdef TIOCGLTC
	case TIOCGLTC:
	case TIOCSLTC:
#endif
	case TCSETSF:
	case TCSETSW:
	case TCSETS:
	case TCGETS:
#ifdef TCGETS2
	case TCGETS2:
	case TCSETSF2:
	case TCSETSW2:
	case TCSETS2:
#endif
	case TCGETA:
	case TCSETAF:
	case TCSETAW:
	case TCSETA:
	case TIOCGLCKTRMIOS:
	case TIOCSLCKTRMIOS:
#ifdef TCGETX
	case TCGETX:
	case TCSETX:
	case TCSETXW:
	case TCSETXF:
#endif
	case TIOCGSOFTCAR:
	case TIOCSSOFTCAR:

	case PPPIOCGCHAN:
	case PPPIOCGUNIT:
		return tty_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
	case TIOCCONS:
	case TIOCEXCL:
	case TIOCNXCL:
	case TIOCVHANGUP:
	case TIOCSBRK:
	case TIOCCBRK:
	case TCSBRK:
	case TCSBRKP:
	case TCFLSH:
	case TIOCGPTPEER:
	case TIOCNOTTY:
	case TIOCSCTTY:
	case TCXONC:
	case TIOCMIWAIT:
	case TIOCSERCONFIG:
		return tty_ioctl(file, cmd, arg);
	}

	if (tty_paranoia_check(tty, file_inode(file), "tty_ioctl"))
		return -EINVAL;

	switch (cmd) {
	case TIOCSSERIAL:
		return compat_tty_tiocsserial(tty, compat_ptr(arg));
	case TIOCGSERIAL:
		return compat_tty_tiocgserial(tty, compat_ptr(arg));
	}
	if (tty->ops->compat_ioctl) {
		retval = tty->ops->compat_ioctl(tty, cmd, arg);
		if (retval != -ENOIOCTLCMD)
			return retval;
	}

	ld = tty_ldisc_ref_wait(tty);
	if (!ld)
		return hung_up_tty_compat_ioctl(file, cmd, arg);
	if (ld->ops->compat_ioctl)
		retval = ld->ops->compat_ioctl(tty, cmd, arg);
	if (retval == -ENOIOCTLCMD && ld->ops->ioctl)
		retval = ld->ops->ioctl(tty, (unsigned long)compat_ptr(cmd),
				arg);
	tty_ldisc_deref(ld);

	return retval;
}
#endif

static int this_tty(const void *t, struct file *file, unsigned fd)
{
	if (likely(file->f_op->read_iter != tty_read))
		return 0;
	return file_tty(file) != t ? 0 : fd + 1;
}

 
void __do_SAK(struct tty_struct *tty)
{
	struct task_struct *g, *p;
	struct pid *session;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&tty->ctrl.lock, flags);
	session = get_pid(tty->ctrl.session);
	spin_unlock_irqrestore(&tty->ctrl.lock, flags);

	tty_ldisc_flush(tty);

	tty_driver_flush_buffer(tty);

	read_lock(&tasklist_lock);
	 
	do_each_pid_task(session, PIDTYPE_SID, p) {
		tty_notice(tty, "SAK: killed process %d (%s): by session\n",
			   task_pid_nr(p), p->comm);
		group_send_sig_info(SIGKILL, SEND_SIG_PRIV, p, PIDTYPE_SID);
	} while_each_pid_task(session, PIDTYPE_SID, p);

	 
	for_each_process_thread(g, p) {
		if (p->signal->tty == tty) {
			tty_notice(tty, "SAK: killed process %d (%s): by controlling tty\n",
				   task_pid_nr(p), p->comm);
			group_send_sig_info(SIGKILL, SEND_SIG_PRIV, p,
					PIDTYPE_SID);
			continue;
		}
		task_lock(p);
		i = iterate_fd(p->files, 0, this_tty, tty);
		if (i != 0) {
			tty_notice(tty, "SAK: killed process %d (%s): by fd#%d\n",
				   task_pid_nr(p), p->comm, i - 1);
			group_send_sig_info(SIGKILL, SEND_SIG_PRIV, p,
					PIDTYPE_SID);
		}
		task_unlock(p);
	}
	read_unlock(&tasklist_lock);
	put_pid(session);
}

static void do_SAK_work(struct work_struct *work)
{
	struct tty_struct *tty =
		container_of(work, struct tty_struct, SAK_work);
	__do_SAK(tty);
}

 
void do_SAK(struct tty_struct *tty)
{
	if (!tty)
		return;
	schedule_work(&tty->SAK_work);
}
EXPORT_SYMBOL(do_SAK);

 
static struct device *tty_get_device(struct tty_struct *tty)
{
	dev_t devt = tty_devnum(tty);

	return class_find_device_by_devt(&tty_class, devt);
}


 
struct tty_struct *alloc_tty_struct(struct tty_driver *driver, int idx)
{
	struct tty_struct *tty;

	tty = kzalloc(sizeof(*tty), GFP_KERNEL_ACCOUNT);
	if (!tty)
		return NULL;

	kref_init(&tty->kref);
	if (tty_ldisc_init(tty)) {
		kfree(tty);
		return NULL;
	}
	tty->ctrl.session = NULL;
	tty->ctrl.pgrp = NULL;
	mutex_init(&tty->legacy_mutex);
	mutex_init(&tty->throttle_mutex);
	init_rwsem(&tty->termios_rwsem);
	mutex_init(&tty->winsize_mutex);
	init_ldsem(&tty->ldisc_sem);
	init_waitqueue_head(&tty->write_wait);
	init_waitqueue_head(&tty->read_wait);
	INIT_WORK(&tty->hangup_work, do_tty_hangup);
	mutex_init(&tty->atomic_write_lock);
	spin_lock_init(&tty->ctrl.lock);
	spin_lock_init(&tty->flow.lock);
	spin_lock_init(&tty->files_lock);
	INIT_LIST_HEAD(&tty->tty_files);
	INIT_WORK(&tty->SAK_work, do_SAK_work);

	tty->driver = driver;
	tty->ops = driver->ops;
	tty->index = idx;
	tty_line_name(driver, idx, tty->name);
	tty->dev = tty_get_device(tty);

	return tty;
}

 
int tty_put_char(struct tty_struct *tty, unsigned char ch)
{
	if (tty->ops->put_char)
		return tty->ops->put_char(tty, ch);
	return tty->ops->write(tty, &ch, 1);
}
EXPORT_SYMBOL_GPL(tty_put_char);

static int tty_cdev_add(struct tty_driver *driver, dev_t dev,
		unsigned int index, unsigned int count)
{
	int err;

	 
	driver->cdevs[index] = cdev_alloc();
	if (!driver->cdevs[index])
		return -ENOMEM;
	driver->cdevs[index]->ops = &tty_fops;
	driver->cdevs[index]->owner = driver->owner;
	err = cdev_add(driver->cdevs[index], dev, count);
	if (err)
		kobject_put(&driver->cdevs[index]->kobj);
	return err;
}

 
struct device *tty_register_device(struct tty_driver *driver, unsigned index,
				   struct device *device)
{
	return tty_register_device_attr(driver, index, device, NULL, NULL);
}
EXPORT_SYMBOL(tty_register_device);

static void tty_device_create_release(struct device *dev)
{
	dev_dbg(dev, "releasing...\n");
	kfree(dev);
}

 
struct device *tty_register_device_attr(struct tty_driver *driver,
				   unsigned index, struct device *device,
				   void *drvdata,
				   const struct attribute_group **attr_grp)
{
	char name[64];
	dev_t devt = MKDEV(driver->major, driver->minor_start) + index;
	struct ktermios *tp;
	struct device *dev;
	int retval;

	if (index >= driver->num) {
		pr_err("%s: Attempt to register invalid tty line number (%d)\n",
		       driver->name, index);
		return ERR_PTR(-EINVAL);
	}

	if (driver->type == TTY_DRIVER_TYPE_PTY)
		pty_line_name(driver, index, name);
	else
		tty_line_name(driver, index, name);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	dev->devt = devt;
	dev->class = &tty_class;
	dev->parent = device;
	dev->release = tty_device_create_release;
	dev_set_name(dev, "%s", name);
	dev->groups = attr_grp;
	dev_set_drvdata(dev, drvdata);

	dev_set_uevent_suppress(dev, 1);

	retval = device_register(dev);
	if (retval)
		goto err_put;

	if (!(driver->flags & TTY_DRIVER_DYNAMIC_ALLOC)) {
		 
		tp = driver->termios[index];
		if (tp) {
			driver->termios[index] = NULL;
			kfree(tp);
		}

		retval = tty_cdev_add(driver, devt, index, 1);
		if (retval)
			goto err_del;
	}

	dev_set_uevent_suppress(dev, 0);
	kobject_uevent(&dev->kobj, KOBJ_ADD);

	return dev;

err_del:
	device_del(dev);
err_put:
	put_device(dev);

	return ERR_PTR(retval);
}
EXPORT_SYMBOL_GPL(tty_register_device_attr);

 
void tty_unregister_device(struct tty_driver *driver, unsigned index)
{
	device_destroy(&tty_class, MKDEV(driver->major, driver->minor_start) + index);
	if (!(driver->flags & TTY_DRIVER_DYNAMIC_ALLOC)) {
		cdev_del(driver->cdevs[index]);
		driver->cdevs[index] = NULL;
	}
}
EXPORT_SYMBOL(tty_unregister_device);

 
struct tty_driver *__tty_alloc_driver(unsigned int lines, struct module *owner,
		unsigned long flags)
{
	struct tty_driver *driver;
	unsigned int cdevs = 1;
	int err;

	if (!lines || (flags & TTY_DRIVER_UNNUMBERED_NODE && lines > 1))
		return ERR_PTR(-EINVAL);

	driver = kzalloc(sizeof(*driver), GFP_KERNEL);
	if (!driver)
		return ERR_PTR(-ENOMEM);

	kref_init(&driver->kref);
	driver->num = lines;
	driver->owner = owner;
	driver->flags = flags;

	if (!(flags & TTY_DRIVER_DEVPTS_MEM)) {
		driver->ttys = kcalloc(lines, sizeof(*driver->ttys),
				GFP_KERNEL);
		driver->termios = kcalloc(lines, sizeof(*driver->termios),
				GFP_KERNEL);
		if (!driver->ttys || !driver->termios) {
			err = -ENOMEM;
			goto err_free_all;
		}
	}

	if (!(flags & TTY_DRIVER_DYNAMIC_ALLOC)) {
		driver->ports = kcalloc(lines, sizeof(*driver->ports),
				GFP_KERNEL);
		if (!driver->ports) {
			err = -ENOMEM;
			goto err_free_all;
		}
		cdevs = lines;
	}

	driver->cdevs = kcalloc(cdevs, sizeof(*driver->cdevs), GFP_KERNEL);
	if (!driver->cdevs) {
		err = -ENOMEM;
		goto err_free_all;
	}

	return driver;
err_free_all:
	kfree(driver->ports);
	kfree(driver->ttys);
	kfree(driver->termios);
	kfree(driver->cdevs);
	kfree(driver);
	return ERR_PTR(err);
}
EXPORT_SYMBOL(__tty_alloc_driver);

static void destruct_tty_driver(struct kref *kref)
{
	struct tty_driver *driver = container_of(kref, struct tty_driver, kref);
	int i;
	struct ktermios *tp;

	if (driver->flags & TTY_DRIVER_INSTALLED) {
		for (i = 0; i < driver->num; i++) {
			tp = driver->termios[i];
			if (tp) {
				driver->termios[i] = NULL;
				kfree(tp);
			}
			if (!(driver->flags & TTY_DRIVER_DYNAMIC_DEV))
				tty_unregister_device(driver, i);
		}
		proc_tty_unregister_driver(driver);
		if (driver->flags & TTY_DRIVER_DYNAMIC_ALLOC)
			cdev_del(driver->cdevs[0]);
	}
	kfree(driver->cdevs);
	kfree(driver->ports);
	kfree(driver->termios);
	kfree(driver->ttys);
	kfree(driver);
}

 
void tty_driver_kref_put(struct tty_driver *driver)
{
	kref_put(&driver->kref, destruct_tty_driver);
}
EXPORT_SYMBOL(tty_driver_kref_put);

 
int tty_register_driver(struct tty_driver *driver)
{
	int error;
	int i;
	dev_t dev;
	struct device *d;

	if (!driver->major) {
		error = alloc_chrdev_region(&dev, driver->minor_start,
						driver->num, driver->name);
		if (!error) {
			driver->major = MAJOR(dev);
			driver->minor_start = MINOR(dev);
		}
	} else {
		dev = MKDEV(driver->major, driver->minor_start);
		error = register_chrdev_region(dev, driver->num, driver->name);
	}
	if (error < 0)
		goto err;

	if (driver->flags & TTY_DRIVER_DYNAMIC_ALLOC) {
		error = tty_cdev_add(driver, dev, 0, driver->num);
		if (error)
			goto err_unreg_char;
	}

	mutex_lock(&tty_mutex);
	list_add(&driver->tty_drivers, &tty_drivers);
	mutex_unlock(&tty_mutex);

	if (!(driver->flags & TTY_DRIVER_DYNAMIC_DEV)) {
		for (i = 0; i < driver->num; i++) {
			d = tty_register_device(driver, i, NULL);
			if (IS_ERR(d)) {
				error = PTR_ERR(d);
				goto err_unreg_devs;
			}
		}
	}
	proc_tty_register_driver(driver);
	driver->flags |= TTY_DRIVER_INSTALLED;
	return 0;

err_unreg_devs:
	for (i--; i >= 0; i--)
		tty_unregister_device(driver, i);

	mutex_lock(&tty_mutex);
	list_del(&driver->tty_drivers);
	mutex_unlock(&tty_mutex);

err_unreg_char:
	unregister_chrdev_region(dev, driver->num);
err:
	return error;
}
EXPORT_SYMBOL(tty_register_driver);

 
void tty_unregister_driver(struct tty_driver *driver)
{
	unregister_chrdev_region(MKDEV(driver->major, driver->minor_start),
				driver->num);
	mutex_lock(&tty_mutex);
	list_del(&driver->tty_drivers);
	mutex_unlock(&tty_mutex);
}
EXPORT_SYMBOL(tty_unregister_driver);

dev_t tty_devnum(struct tty_struct *tty)
{
	return MKDEV(tty->driver->major, tty->driver->minor_start) + tty->index;
}
EXPORT_SYMBOL(tty_devnum);

void tty_default_fops(struct file_operations *fops)
{
	*fops = tty_fops;
}

static char *tty_devnode(const struct device *dev, umode_t *mode)
{
	if (!mode)
		return NULL;
	if (dev->devt == MKDEV(TTYAUX_MAJOR, 0) ||
	    dev->devt == MKDEV(TTYAUX_MAJOR, 2))
		*mode = 0666;
	return NULL;
}

const struct class tty_class = {
	.name		= "tty",
	.devnode	= tty_devnode,
};

static int __init tty_class_init(void)
{
	return class_register(&tty_class);
}

postcore_initcall(tty_class_init);

 
static struct cdev tty_cdev, console_cdev;

static ssize_t show_cons_active(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct console *cs[16];
	int i = 0;
	struct console *c;
	ssize_t count = 0;

	 
	console_list_lock();

	for_each_console(c) {
		if (!c->device)
			continue;
		if (!c->write)
			continue;
		if ((c->flags & CON_ENABLED) == 0)
			continue;
		cs[i++] = c;
		if (i >= ARRAY_SIZE(cs))
			break;
	}

	 
	console_lock();
	while (i--) {
		int index = cs[i]->index;
		struct tty_driver *drv = cs[i]->device(cs[i], &index);

		 
		if (drv && (cs[i]->index > 0 || drv->major != TTY_MAJOR))
			count += tty_line_name(drv, index, buf + count);
		else
			count += sprintf(buf + count, "%s%d",
					 cs[i]->name, cs[i]->index);

		count += sprintf(buf + count, "%c", i ? ' ':'\n');
	}
	console_unlock();

	console_list_unlock();

	return count;
}
static DEVICE_ATTR(active, S_IRUGO, show_cons_active, NULL);

static struct attribute *cons_dev_attrs[] = {
	&dev_attr_active.attr,
	NULL
};

ATTRIBUTE_GROUPS(cons_dev);

static struct device *consdev;

void console_sysfs_notify(void)
{
	if (consdev)
		sysfs_notify(&consdev->kobj, NULL, "active");
}

static struct ctl_table tty_table[] = {
	{
		.procname	= "legacy_tiocsti",
		.data		= &tty_legacy_tiocsti,
		.maxlen		= sizeof(tty_legacy_tiocsti),
		.mode		= 0644,
		.proc_handler	= proc_dobool,
	},
	{
		.procname	= "ldisc_autoload",
		.data		= &tty_ldisc_autoload,
		.maxlen		= sizeof(tty_ldisc_autoload),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{ }
};

 
int __init tty_init(void)
{
	register_sysctl_init("dev/tty", tty_table);
	cdev_init(&tty_cdev, &tty_fops);
	if (cdev_add(&tty_cdev, MKDEV(TTYAUX_MAJOR, 0), 1) ||
	    register_chrdev_region(MKDEV(TTYAUX_MAJOR, 0), 1, "/dev/tty") < 0)
		panic("Couldn't register /dev/tty driver\n");
	device_create(&tty_class, NULL, MKDEV(TTYAUX_MAJOR, 0), NULL, "tty");

	cdev_init(&console_cdev, &console_fops);
	if (cdev_add(&console_cdev, MKDEV(TTYAUX_MAJOR, 1), 1) ||
	    register_chrdev_region(MKDEV(TTYAUX_MAJOR, 1), 1, "/dev/console") < 0)
		panic("Couldn't register /dev/console driver\n");
	consdev = device_create_with_groups(&tty_class, NULL,
					    MKDEV(TTYAUX_MAJOR, 1), NULL,
					    cons_dev_groups, "console");
	if (IS_ERR(consdev))
		consdev = NULL;

#ifdef CONFIG_VT
	vty_init(&console_fops);
#endif
	return 0;
}
