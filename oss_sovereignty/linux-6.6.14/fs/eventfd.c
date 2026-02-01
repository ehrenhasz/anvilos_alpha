
 

#include <linux/file.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched/signal.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/anon_inodes.h>
#include <linux/syscalls.h>
#include <linux/export.h>
#include <linux/kref.h>
#include <linux/eventfd.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/idr.h>
#include <linux/uio.h>

static DEFINE_IDA(eventfd_ida);

struct eventfd_ctx {
	struct kref kref;
	wait_queue_head_t wqh;
	 
	__u64 count;
	unsigned int flags;
	int id;
};

__u64 eventfd_signal_mask(struct eventfd_ctx *ctx, __u64 n, __poll_t mask)
{
	unsigned long flags;

	 
	if (WARN_ON_ONCE(current->in_eventfd))
		return 0;

	spin_lock_irqsave(&ctx->wqh.lock, flags);
	current->in_eventfd = 1;
	if (ULLONG_MAX - ctx->count < n)
		n = ULLONG_MAX - ctx->count;
	ctx->count += n;
	if (waitqueue_active(&ctx->wqh))
		wake_up_locked_poll(&ctx->wqh, EPOLLIN | mask);
	current->in_eventfd = 0;
	spin_unlock_irqrestore(&ctx->wqh.lock, flags);

	return n;
}

 
__u64 eventfd_signal(struct eventfd_ctx *ctx, __u64 n)
{
	return eventfd_signal_mask(ctx, n, 0);
}
EXPORT_SYMBOL_GPL(eventfd_signal);

static void eventfd_free_ctx(struct eventfd_ctx *ctx)
{
	if (ctx->id >= 0)
		ida_simple_remove(&eventfd_ida, ctx->id);
	kfree(ctx);
}

static void eventfd_free(struct kref *kref)
{
	struct eventfd_ctx *ctx = container_of(kref, struct eventfd_ctx, kref);

	eventfd_free_ctx(ctx);
}

 
void eventfd_ctx_put(struct eventfd_ctx *ctx)
{
	kref_put(&ctx->kref, eventfd_free);
}
EXPORT_SYMBOL_GPL(eventfd_ctx_put);

static int eventfd_release(struct inode *inode, struct file *file)
{
	struct eventfd_ctx *ctx = file->private_data;

	wake_up_poll(&ctx->wqh, EPOLLHUP);
	eventfd_ctx_put(ctx);
	return 0;
}

static __poll_t eventfd_poll(struct file *file, poll_table *wait)
{
	struct eventfd_ctx *ctx = file->private_data;
	__poll_t events = 0;
	u64 count;

	poll_wait(file, &ctx->wqh, wait);

	 
	count = READ_ONCE(ctx->count);

	if (count > 0)
		events |= EPOLLIN;
	if (count == ULLONG_MAX)
		events |= EPOLLERR;
	if (ULLONG_MAX - 1 > count)
		events |= EPOLLOUT;

	return events;
}

void eventfd_ctx_do_read(struct eventfd_ctx *ctx, __u64 *cnt)
{
	lockdep_assert_held(&ctx->wqh.lock);

	*cnt = ((ctx->flags & EFD_SEMAPHORE) && ctx->count) ? 1 : ctx->count;
	ctx->count -= *cnt;
}
EXPORT_SYMBOL_GPL(eventfd_ctx_do_read);

 
int eventfd_ctx_remove_wait_queue(struct eventfd_ctx *ctx, wait_queue_entry_t *wait,
				  __u64 *cnt)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->wqh.lock, flags);
	eventfd_ctx_do_read(ctx, cnt);
	__remove_wait_queue(&ctx->wqh, wait);
	if (*cnt != 0 && waitqueue_active(&ctx->wqh))
		wake_up_locked_poll(&ctx->wqh, EPOLLOUT);
	spin_unlock_irqrestore(&ctx->wqh.lock, flags);

	return *cnt != 0 ? 0 : -EAGAIN;
}
EXPORT_SYMBOL_GPL(eventfd_ctx_remove_wait_queue);

static ssize_t eventfd_read(struct kiocb *iocb, struct iov_iter *to)
{
	struct file *file = iocb->ki_filp;
	struct eventfd_ctx *ctx = file->private_data;
	__u64 ucnt = 0;

	if (iov_iter_count(to) < sizeof(ucnt))
		return -EINVAL;
	spin_lock_irq(&ctx->wqh.lock);
	if (!ctx->count) {
		if ((file->f_flags & O_NONBLOCK) ||
		    (iocb->ki_flags & IOCB_NOWAIT)) {
			spin_unlock_irq(&ctx->wqh.lock);
			return -EAGAIN;
		}

		if (wait_event_interruptible_locked_irq(ctx->wqh, ctx->count)) {
			spin_unlock_irq(&ctx->wqh.lock);
			return -ERESTARTSYS;
		}
	}
	eventfd_ctx_do_read(ctx, &ucnt);
	current->in_eventfd = 1;
	if (waitqueue_active(&ctx->wqh))
		wake_up_locked_poll(&ctx->wqh, EPOLLOUT);
	current->in_eventfd = 0;
	spin_unlock_irq(&ctx->wqh.lock);
	if (unlikely(copy_to_iter(&ucnt, sizeof(ucnt), to) != sizeof(ucnt)))
		return -EFAULT;

	return sizeof(ucnt);
}

static ssize_t eventfd_write(struct file *file, const char __user *buf, size_t count,
			     loff_t *ppos)
{
	struct eventfd_ctx *ctx = file->private_data;
	ssize_t res;
	__u64 ucnt;

	if (count < sizeof(ucnt))
		return -EINVAL;
	if (copy_from_user(&ucnt, buf, sizeof(ucnt)))
		return -EFAULT;
	if (ucnt == ULLONG_MAX)
		return -EINVAL;
	spin_lock_irq(&ctx->wqh.lock);
	res = -EAGAIN;
	if (ULLONG_MAX - ctx->count > ucnt)
		res = sizeof(ucnt);
	else if (!(file->f_flags & O_NONBLOCK)) {
		res = wait_event_interruptible_locked_irq(ctx->wqh,
				ULLONG_MAX - ctx->count > ucnt);
		if (!res)
			res = sizeof(ucnt);
	}
	if (likely(res > 0)) {
		ctx->count += ucnt;
		current->in_eventfd = 1;
		if (waitqueue_active(&ctx->wqh))
			wake_up_locked_poll(&ctx->wqh, EPOLLIN);
		current->in_eventfd = 0;
	}
	spin_unlock_irq(&ctx->wqh.lock);

	return res;
}

#ifdef CONFIG_PROC_FS
static void eventfd_show_fdinfo(struct seq_file *m, struct file *f)
{
	struct eventfd_ctx *ctx = f->private_data;

	spin_lock_irq(&ctx->wqh.lock);
	seq_printf(m, "eventfd-count: %16llx\n",
		   (unsigned long long)ctx->count);
	spin_unlock_irq(&ctx->wqh.lock);
	seq_printf(m, "eventfd-id: %d\n", ctx->id);
	seq_printf(m, "eventfd-semaphore: %d\n",
		   !!(ctx->flags & EFD_SEMAPHORE));
}
#endif

static const struct file_operations eventfd_fops = {
#ifdef CONFIG_PROC_FS
	.show_fdinfo	= eventfd_show_fdinfo,
#endif
	.release	= eventfd_release,
	.poll		= eventfd_poll,
	.read_iter	= eventfd_read,
	.write		= eventfd_write,
	.llseek		= noop_llseek,
};

 
struct file *eventfd_fget(int fd)
{
	struct file *file;

	file = fget(fd);
	if (!file)
		return ERR_PTR(-EBADF);
	if (file->f_op != &eventfd_fops) {
		fput(file);
		return ERR_PTR(-EINVAL);
	}

	return file;
}
EXPORT_SYMBOL_GPL(eventfd_fget);

 
struct eventfd_ctx *eventfd_ctx_fdget(int fd)
{
	struct eventfd_ctx *ctx;
	struct fd f = fdget(fd);
	if (!f.file)
		return ERR_PTR(-EBADF);
	ctx = eventfd_ctx_fileget(f.file);
	fdput(f);
	return ctx;
}
EXPORT_SYMBOL_GPL(eventfd_ctx_fdget);

 
struct eventfd_ctx *eventfd_ctx_fileget(struct file *file)
{
	struct eventfd_ctx *ctx;

	if (file->f_op != &eventfd_fops)
		return ERR_PTR(-EINVAL);

	ctx = file->private_data;
	kref_get(&ctx->kref);
	return ctx;
}
EXPORT_SYMBOL_GPL(eventfd_ctx_fileget);

static int do_eventfd(unsigned int count, int flags)
{
	struct eventfd_ctx *ctx;
	struct file *file;
	int fd;

	 
	BUILD_BUG_ON(EFD_CLOEXEC != O_CLOEXEC);
	BUILD_BUG_ON(EFD_NONBLOCK != O_NONBLOCK);

	if (flags & ~EFD_FLAGS_SET)
		return -EINVAL;

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	kref_init(&ctx->kref);
	init_waitqueue_head(&ctx->wqh);
	ctx->count = count;
	ctx->flags = flags;
	ctx->id = ida_simple_get(&eventfd_ida, 0, 0, GFP_KERNEL);

	flags &= EFD_SHARED_FCNTL_FLAGS;
	flags |= O_RDWR;
	fd = get_unused_fd_flags(flags);
	if (fd < 0)
		goto err;

	file = anon_inode_getfile("[eventfd]", &eventfd_fops, ctx, flags);
	if (IS_ERR(file)) {
		put_unused_fd(fd);
		fd = PTR_ERR(file);
		goto err;
	}

	file->f_mode |= FMODE_NOWAIT;
	fd_install(fd, file);
	return fd;
err:
	eventfd_free_ctx(ctx);
	return fd;
}

SYSCALL_DEFINE2(eventfd2, unsigned int, count, int, flags)
{
	return do_eventfd(count, flags);
}

SYSCALL_DEFINE1(eventfd, unsigned int, count)
{
	return do_eventfd(count, 0);
}

