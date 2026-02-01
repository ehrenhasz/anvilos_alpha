
 

#include <linux/file.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/list.h>
#include <linux/anon_inodes.h>
#include <linux/signalfd.h>
#include <linux/syscalls.h>
#include <linux/proc_fs.h>
#include <linux/compat.h>

void signalfd_cleanup(struct sighand_struct *sighand)
{
	wake_up_pollfree(&sighand->signalfd_wqh);
}

struct signalfd_ctx {
	sigset_t sigmask;
};

static int signalfd_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static __poll_t signalfd_poll(struct file *file, poll_table *wait)
{
	struct signalfd_ctx *ctx = file->private_data;
	__poll_t events = 0;

	poll_wait(file, &current->sighand->signalfd_wqh, wait);

	spin_lock_irq(&current->sighand->siglock);
	if (next_signal(&current->pending, &ctx->sigmask) ||
	    next_signal(&current->signal->shared_pending,
			&ctx->sigmask))
		events |= EPOLLIN;
	spin_unlock_irq(&current->sighand->siglock);

	return events;
}

 
static int signalfd_copyinfo(struct signalfd_siginfo __user *uinfo,
			     kernel_siginfo_t const *kinfo)
{
	struct signalfd_siginfo new;

	BUILD_BUG_ON(sizeof(struct signalfd_siginfo) != 128);

	 
	memset(&new, 0, sizeof(new));

	 
	new.ssi_signo = kinfo->si_signo;
	new.ssi_errno = kinfo->si_errno;
	new.ssi_code  = kinfo->si_code;
	switch (siginfo_layout(kinfo->si_signo, kinfo->si_code)) {
	case SIL_KILL:
		new.ssi_pid = kinfo->si_pid;
		new.ssi_uid = kinfo->si_uid;
		break;
	case SIL_TIMER:
		new.ssi_tid = kinfo->si_tid;
		new.ssi_overrun = kinfo->si_overrun;
		new.ssi_ptr = (long) kinfo->si_ptr;
		new.ssi_int = kinfo->si_int;
		break;
	case SIL_POLL:
		new.ssi_band = kinfo->si_band;
		new.ssi_fd   = kinfo->si_fd;
		break;
	case SIL_FAULT_BNDERR:
	case SIL_FAULT_PKUERR:
	case SIL_FAULT_PERF_EVENT:
		 
	case SIL_FAULT:
		new.ssi_addr = (long) kinfo->si_addr;
		break;
	case SIL_FAULT_TRAPNO:
		new.ssi_addr = (long) kinfo->si_addr;
		new.ssi_trapno = kinfo->si_trapno;
		break;
	case SIL_FAULT_MCEERR:
		new.ssi_addr = (long) kinfo->si_addr;
		new.ssi_addr_lsb = (short) kinfo->si_addr_lsb;
		break;
	case SIL_CHLD:
		new.ssi_pid    = kinfo->si_pid;
		new.ssi_uid    = kinfo->si_uid;
		new.ssi_status = kinfo->si_status;
		new.ssi_utime  = kinfo->si_utime;
		new.ssi_stime  = kinfo->si_stime;
		break;
	case SIL_RT:
		 
		new.ssi_pid = kinfo->si_pid;
		new.ssi_uid = kinfo->si_uid;
		new.ssi_ptr = (long) kinfo->si_ptr;
		new.ssi_int = kinfo->si_int;
		break;
	case SIL_SYS:
		new.ssi_call_addr = (long) kinfo->si_call_addr;
		new.ssi_syscall   = kinfo->si_syscall;
		new.ssi_arch      = kinfo->si_arch;
		break;
	}

	if (copy_to_user(uinfo, &new, sizeof(struct signalfd_siginfo)))
		return -EFAULT;

	return sizeof(*uinfo);
}

static ssize_t signalfd_dequeue(struct signalfd_ctx *ctx, kernel_siginfo_t *info,
				int nonblock)
{
	enum pid_type type;
	ssize_t ret;
	DECLARE_WAITQUEUE(wait, current);

	spin_lock_irq(&current->sighand->siglock);
	ret = dequeue_signal(current, &ctx->sigmask, info, &type);
	switch (ret) {
	case 0:
		if (!nonblock)
			break;
		ret = -EAGAIN;
		fallthrough;
	default:
		spin_unlock_irq(&current->sighand->siglock);
		return ret;
	}

	add_wait_queue(&current->sighand->signalfd_wqh, &wait);
	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		ret = dequeue_signal(current, &ctx->sigmask, info, &type);
		if (ret != 0)
			break;
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
		spin_unlock_irq(&current->sighand->siglock);
		schedule();
		spin_lock_irq(&current->sighand->siglock);
	}
	spin_unlock_irq(&current->sighand->siglock);

	remove_wait_queue(&current->sighand->signalfd_wqh, &wait);
	__set_current_state(TASK_RUNNING);

	return ret;
}

 
static ssize_t signalfd_read(struct file *file, char __user *buf, size_t count,
			     loff_t *ppos)
{
	struct signalfd_ctx *ctx = file->private_data;
	struct signalfd_siginfo __user *siginfo;
	int nonblock = file->f_flags & O_NONBLOCK;
	ssize_t ret, total = 0;
	kernel_siginfo_t info;

	count /= sizeof(struct signalfd_siginfo);
	if (!count)
		return -EINVAL;

	siginfo = (struct signalfd_siginfo __user *) buf;
	do {
		ret = signalfd_dequeue(ctx, &info, nonblock);
		if (unlikely(ret <= 0))
			break;
		ret = signalfd_copyinfo(siginfo, &info);
		if (ret < 0)
			break;
		siginfo++;
		total += ret;
		nonblock = 1;
	} while (--count);

	return total ? total: ret;
}

#ifdef CONFIG_PROC_FS
static void signalfd_show_fdinfo(struct seq_file *m, struct file *f)
{
	struct signalfd_ctx *ctx = f->private_data;
	sigset_t sigmask;

	sigmask = ctx->sigmask;
	signotset(&sigmask);
	render_sigset_t(m, "sigmask:\t", &sigmask);
}
#endif

static const struct file_operations signalfd_fops = {
#ifdef CONFIG_PROC_FS
	.show_fdinfo	= signalfd_show_fdinfo,
#endif
	.release	= signalfd_release,
	.poll		= signalfd_poll,
	.read		= signalfd_read,
	.llseek		= noop_llseek,
};

static int do_signalfd4(int ufd, sigset_t *mask, int flags)
{
	struct signalfd_ctx *ctx;

	 
	BUILD_BUG_ON(SFD_CLOEXEC != O_CLOEXEC);
	BUILD_BUG_ON(SFD_NONBLOCK != O_NONBLOCK);

	if (flags & ~(SFD_CLOEXEC | SFD_NONBLOCK))
		return -EINVAL;

	sigdelsetmask(mask, sigmask(SIGKILL) | sigmask(SIGSTOP));
	signotset(mask);

	if (ufd == -1) {
		ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
		if (!ctx)
			return -ENOMEM;

		ctx->sigmask = *mask;

		 
		ufd = anon_inode_getfd("[signalfd]", &signalfd_fops, ctx,
				       O_RDWR | (flags & (O_CLOEXEC | O_NONBLOCK)));
		if (ufd < 0)
			kfree(ctx);
	} else {
		struct fd f = fdget(ufd);
		if (!f.file)
			return -EBADF;
		ctx = f.file->private_data;
		if (f.file->f_op != &signalfd_fops) {
			fdput(f);
			return -EINVAL;
		}
		spin_lock_irq(&current->sighand->siglock);
		ctx->sigmask = *mask;
		spin_unlock_irq(&current->sighand->siglock);

		wake_up(&current->sighand->signalfd_wqh);
		fdput(f);
	}

	return ufd;
}

SYSCALL_DEFINE4(signalfd4, int, ufd, sigset_t __user *, user_mask,
		size_t, sizemask, int, flags)
{
	sigset_t mask;

	if (sizemask != sizeof(sigset_t))
		return -EINVAL;
	if (copy_from_user(&mask, user_mask, sizeof(mask)))
		return -EFAULT;
	return do_signalfd4(ufd, &mask, flags);
}

SYSCALL_DEFINE3(signalfd, int, ufd, sigset_t __user *, user_mask,
		size_t, sizemask)
{
	sigset_t mask;

	if (sizemask != sizeof(sigset_t))
		return -EINVAL;
	if (copy_from_user(&mask, user_mask, sizeof(mask)))
		return -EFAULT;
	return do_signalfd4(ufd, &mask, 0);
}

#ifdef CONFIG_COMPAT
static long do_compat_signalfd4(int ufd,
			const compat_sigset_t __user *user_mask,
			compat_size_t sigsetsize, int flags)
{
	sigset_t mask;

	if (sigsetsize != sizeof(compat_sigset_t))
		return -EINVAL;
	if (get_compat_sigset(&mask, user_mask))
		return -EFAULT;
	return do_signalfd4(ufd, &mask, flags);
}

COMPAT_SYSCALL_DEFINE4(signalfd4, int, ufd,
		     const compat_sigset_t __user *, user_mask,
		     compat_size_t, sigsetsize,
		     int, flags)
{
	return do_compat_signalfd4(ufd, user_mask, sigsetsize, flags);
}

COMPAT_SYSCALL_DEFINE3(signalfd, int, ufd,
		     const compat_sigset_t __user *, user_mask,
		     compat_size_t, sigsetsize)
{
	return do_compat_signalfd4(ufd, user_mask, sigsetsize, 0);
}
#endif
