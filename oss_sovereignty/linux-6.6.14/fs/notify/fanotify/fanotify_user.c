
#include <linux/fanotify.h>
#include <linux/fcntl.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/fsnotify_backend.h>
#include <linux/init.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/poll.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/sched/signal.h>
#include <linux/memcontrol.h>
#include <linux/statfs.h>
#include <linux/exportfs.h>

#include <asm/ioctls.h>

#include "../../mount.h"
#include "../fdinfo.h"
#include "fanotify.h"

#define FANOTIFY_DEFAULT_MAX_EVENTS	16384
#define FANOTIFY_OLD_DEFAULT_MAX_MARKS	8192
#define FANOTIFY_DEFAULT_MAX_GROUPS	128
#define FANOTIFY_DEFAULT_FEE_POOL_SIZE	32

 
#define FANOTIFY_DEFAULT_MAX_USER_MARKS	\
	(FANOTIFY_OLD_DEFAULT_MAX_MARKS * FANOTIFY_DEFAULT_MAX_GROUPS)

 
#define INODE_MARK_COST	(2 * sizeof(struct inode))

 
static int fanotify_max_queued_events __read_mostly;

#ifdef CONFIG_SYSCTL

#include <linux/sysctl.h>

static long ft_zero = 0;
static long ft_int_max = INT_MAX;

static struct ctl_table fanotify_table[] = {
	{
		.procname	= "max_user_groups",
		.data	= &init_user_ns.ucount_max[UCOUNT_FANOTIFY_GROUPS],
		.maxlen		= sizeof(long),
		.mode		= 0644,
		.proc_handler	= proc_doulongvec_minmax,
		.extra1		= &ft_zero,
		.extra2		= &ft_int_max,
	},
	{
		.procname	= "max_user_marks",
		.data	= &init_user_ns.ucount_max[UCOUNT_FANOTIFY_MARKS],
		.maxlen		= sizeof(long),
		.mode		= 0644,
		.proc_handler	= proc_doulongvec_minmax,
		.extra1		= &ft_zero,
		.extra2		= &ft_int_max,
	},
	{
		.procname	= "max_queued_events",
		.data		= &fanotify_max_queued_events,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO
	},
	{ }
};

static void __init fanotify_sysctls_init(void)
{
	register_sysctl("fs/fanotify", fanotify_table);
}
#else
#define fanotify_sysctls_init() do { } while (0)
#endif  

 
#define	FANOTIFY_INIT_ALL_EVENT_F_BITS				( \
		O_ACCMODE	| O_APPEND	| O_NONBLOCK	| \
		__O_SYNC	| O_DSYNC	| O_CLOEXEC     | \
		O_LARGEFILE	| O_NOATIME	)

extern const struct fsnotify_ops fanotify_fsnotify_ops;

struct kmem_cache *fanotify_mark_cache __read_mostly;
struct kmem_cache *fanotify_fid_event_cachep __read_mostly;
struct kmem_cache *fanotify_path_event_cachep __read_mostly;
struct kmem_cache *fanotify_perm_event_cachep __read_mostly;

#define FANOTIFY_EVENT_ALIGN 4
#define FANOTIFY_FID_INFO_HDR_LEN \
	(sizeof(struct fanotify_event_info_fid) + sizeof(struct file_handle))
#define FANOTIFY_PIDFD_INFO_HDR_LEN \
	sizeof(struct fanotify_event_info_pidfd)
#define FANOTIFY_ERROR_INFO_LEN \
	(sizeof(struct fanotify_event_info_error))

static int fanotify_fid_info_len(int fh_len, int name_len)
{
	int info_len = fh_len;

	if (name_len)
		info_len += name_len + 1;

	return roundup(FANOTIFY_FID_INFO_HDR_LEN + info_len,
		       FANOTIFY_EVENT_ALIGN);
}

 
static int fanotify_dir_name_info_len(struct fanotify_event *event)
{
	struct fanotify_info *info = fanotify_event_info(event);
	int dir_fh_len = fanotify_event_dir_fh_len(event);
	int dir2_fh_len = fanotify_event_dir2_fh_len(event);
	int info_len = 0;

	if (dir_fh_len)
		info_len += fanotify_fid_info_len(dir_fh_len,
						  info->name_len);
	if (dir2_fh_len)
		info_len += fanotify_fid_info_len(dir2_fh_len,
						  info->name2_len);

	return info_len;
}

static size_t fanotify_event_len(unsigned int info_mode,
				 struct fanotify_event *event)
{
	size_t event_len = FAN_EVENT_METADATA_LEN;
	int fh_len;
	int dot_len = 0;

	if (!info_mode)
		return event_len;

	if (fanotify_is_error_event(event->mask))
		event_len += FANOTIFY_ERROR_INFO_LEN;

	if (fanotify_event_has_any_dir_fh(event)) {
		event_len += fanotify_dir_name_info_len(event);
	} else if ((info_mode & FAN_REPORT_NAME) &&
		   (event->mask & FAN_ONDIR)) {
		 
		dot_len = 1;
	}

	if (info_mode & FAN_REPORT_PIDFD)
		event_len += FANOTIFY_PIDFD_INFO_HDR_LEN;

	if (fanotify_event_has_object_fh(event)) {
		fh_len = fanotify_event_object_fh_len(event);
		event_len += fanotify_fid_info_len(fh_len, dot_len);
	}

	return event_len;
}

 
static void fanotify_unhash_event(struct fsnotify_group *group,
				  struct fanotify_event *event)
{
	assert_spin_locked(&group->notification_lock);

	pr_debug("%s: group=%p event=%p bucket=%u\n", __func__,
		 group, event, fanotify_event_hash_bucket(group, event));

	if (WARN_ON_ONCE(hlist_unhashed(&event->merge_list)))
		return;

	hlist_del_init(&event->merge_list);
}

 
static struct fanotify_event *get_one_event(struct fsnotify_group *group,
					    size_t count)
{
	size_t event_size;
	struct fanotify_event *event = NULL;
	struct fsnotify_event *fsn_event;
	unsigned int info_mode = FAN_GROUP_FLAG(group, FANOTIFY_INFO_MODES);

	pr_debug("%s: group=%p count=%zd\n", __func__, group, count);

	spin_lock(&group->notification_lock);
	fsn_event = fsnotify_peek_first_event(group);
	if (!fsn_event)
		goto out;

	event = FANOTIFY_E(fsn_event);
	event_size = fanotify_event_len(info_mode, event);

	if (event_size > count) {
		event = ERR_PTR(-EINVAL);
		goto out;
	}

	 
	fsnotify_remove_first_event(group);
	if (fanotify_is_perm_event(event->mask))
		FANOTIFY_PERM(event)->state = FAN_EVENT_REPORTED;
	if (fanotify_is_hashed_event(event->mask))
		fanotify_unhash_event(group, event);
out:
	spin_unlock(&group->notification_lock);
	return event;
}

static int create_fd(struct fsnotify_group *group, const struct path *path,
		     struct file **file)
{
	int client_fd;
	struct file *new_file;

	client_fd = get_unused_fd_flags(group->fanotify_data.f_flags);
	if (client_fd < 0)
		return client_fd;

	 
	new_file = dentry_open(path,
			       group->fanotify_data.f_flags | __FMODE_NONOTIFY,
			       current_cred());
	if (IS_ERR(new_file)) {
		 
		put_unused_fd(client_fd);
		client_fd = PTR_ERR(new_file);
	} else {
		*file = new_file;
	}

	return client_fd;
}

static int process_access_response_info(const char __user *info,
					size_t info_len,
				struct fanotify_response_info_audit_rule *friar)
{
	if (info_len != sizeof(*friar))
		return -EINVAL;

	if (copy_from_user(friar, info, sizeof(*friar)))
		return -EFAULT;

	if (friar->hdr.type != FAN_RESPONSE_INFO_AUDIT_RULE)
		return -EINVAL;
	if (friar->hdr.pad != 0)
		return -EINVAL;
	if (friar->hdr.len != sizeof(*friar))
		return -EINVAL;

	return info_len;
}

 
static void finish_permission_event(struct fsnotify_group *group,
				    struct fanotify_perm_event *event, u32 response,
				    struct fanotify_response_info_audit_rule *friar)
				    __releases(&group->notification_lock)
{
	bool destroy = false;

	assert_spin_locked(&group->notification_lock);
	event->response = response & ~FAN_INFO;
	if (response & FAN_INFO)
		memcpy(&event->audit_rule, friar, sizeof(*friar));

	if (event->state == FAN_EVENT_CANCELED)
		destroy = true;
	else
		event->state = FAN_EVENT_ANSWERED;
	spin_unlock(&group->notification_lock);
	if (destroy)
		fsnotify_destroy_event(group, &event->fae.fse);
}

static int process_access_response(struct fsnotify_group *group,
				   struct fanotify_response *response_struct,
				   const char __user *info,
				   size_t info_len)
{
	struct fanotify_perm_event *event;
	int fd = response_struct->fd;
	u32 response = response_struct->response;
	int ret = info_len;
	struct fanotify_response_info_audit_rule friar;

	pr_debug("%s: group=%p fd=%d response=%u buf=%p size=%zu\n", __func__,
		 group, fd, response, info, info_len);
	 
	if (response & ~FANOTIFY_RESPONSE_VALID_MASK)
		return -EINVAL;

	switch (response & FANOTIFY_RESPONSE_ACCESS) {
	case FAN_ALLOW:
	case FAN_DENY:
		break;
	default:
		return -EINVAL;
	}

	if ((response & FAN_AUDIT) && !FAN_GROUP_FLAG(group, FAN_ENABLE_AUDIT))
		return -EINVAL;

	if (response & FAN_INFO) {
		ret = process_access_response_info(info, info_len, &friar);
		if (ret < 0)
			return ret;
		if (fd == FAN_NOFD)
			return ret;
	} else {
		ret = 0;
	}

	if (fd < 0)
		return -EINVAL;

	spin_lock(&group->notification_lock);
	list_for_each_entry(event, &group->fanotify_data.access_list,
			    fae.fse.list) {
		if (event->fd != fd)
			continue;

		list_del_init(&event->fae.fse.list);
		finish_permission_event(group, event, response, &friar);
		wake_up(&group->fanotify_data.access_waitq);
		return ret;
	}
	spin_unlock(&group->notification_lock);

	return -ENOENT;
}

static size_t copy_error_info_to_user(struct fanotify_event *event,
				      char __user *buf, int count)
{
	struct fanotify_event_info_error info = { };
	struct fanotify_error_event *fee = FANOTIFY_EE(event);

	info.hdr.info_type = FAN_EVENT_INFO_TYPE_ERROR;
	info.hdr.len = FANOTIFY_ERROR_INFO_LEN;

	if (WARN_ON(count < info.hdr.len))
		return -EFAULT;

	info.error = fee->error;
	info.error_count = fee->err_count;

	if (copy_to_user(buf, &info, sizeof(info)))
		return -EFAULT;

	return info.hdr.len;
}

static int copy_fid_info_to_user(__kernel_fsid_t *fsid, struct fanotify_fh *fh,
				 int info_type, const char *name,
				 size_t name_len,
				 char __user *buf, size_t count)
{
	struct fanotify_event_info_fid info = { };
	struct file_handle handle = { };
	unsigned char bounce[FANOTIFY_INLINE_FH_LEN], *fh_buf;
	size_t fh_len = fh ? fh->len : 0;
	size_t info_len = fanotify_fid_info_len(fh_len, name_len);
	size_t len = info_len;

	pr_debug("%s: fh_len=%zu name_len=%zu, info_len=%zu, count=%zu\n",
		 __func__, fh_len, name_len, info_len, count);

	if (WARN_ON_ONCE(len < sizeof(info) || len > count))
		return -EFAULT;

	 
	switch (info_type) {
	case FAN_EVENT_INFO_TYPE_FID:
	case FAN_EVENT_INFO_TYPE_DFID:
		if (WARN_ON_ONCE(name_len))
			return -EFAULT;
		break;
	case FAN_EVENT_INFO_TYPE_DFID_NAME:
	case FAN_EVENT_INFO_TYPE_OLD_DFID_NAME:
	case FAN_EVENT_INFO_TYPE_NEW_DFID_NAME:
		if (WARN_ON_ONCE(!name || !name_len))
			return -EFAULT;
		break;
	default:
		return -EFAULT;
	}

	info.hdr.info_type = info_type;
	info.hdr.len = len;
	info.fsid = *fsid;
	if (copy_to_user(buf, &info, sizeof(info)))
		return -EFAULT;

	buf += sizeof(info);
	len -= sizeof(info);
	if (WARN_ON_ONCE(len < sizeof(handle)))
		return -EFAULT;

	handle.handle_type = fh->type;
	handle.handle_bytes = fh_len;

	 
	if (!fh_len)
		handle.handle_type = FILEID_INVALID;

	if (copy_to_user(buf, &handle, sizeof(handle)))
		return -EFAULT;

	buf += sizeof(handle);
	len -= sizeof(handle);
	if (WARN_ON_ONCE(len < fh_len))
		return -EFAULT;

	 
	fh_buf = fanotify_fh_buf(fh);
	if (fh_len <= FANOTIFY_INLINE_FH_LEN) {
		memcpy(bounce, fh_buf, fh_len);
		fh_buf = bounce;
	}
	if (copy_to_user(buf, fh_buf, fh_len))
		return -EFAULT;

	buf += fh_len;
	len -= fh_len;

	if (name_len) {
		 
		name_len++;
		if (WARN_ON_ONCE(len < name_len))
			return -EFAULT;

		if (copy_to_user(buf, name, name_len))
			return -EFAULT;

		buf += name_len;
		len -= name_len;
	}

	 
	WARN_ON_ONCE(len < 0 || len >= FANOTIFY_EVENT_ALIGN);
	if (len > 0 && clear_user(buf, len))
		return -EFAULT;

	return info_len;
}

static int copy_pidfd_info_to_user(int pidfd,
				   char __user *buf,
				   size_t count)
{
	struct fanotify_event_info_pidfd info = { };
	size_t info_len = FANOTIFY_PIDFD_INFO_HDR_LEN;

	if (WARN_ON_ONCE(info_len > count))
		return -EFAULT;

	info.hdr.info_type = FAN_EVENT_INFO_TYPE_PIDFD;
	info.hdr.len = info_len;
	info.pidfd = pidfd;

	if (copy_to_user(buf, &info, info_len))
		return -EFAULT;

	return info_len;
}

static int copy_info_records_to_user(struct fanotify_event *event,
				     struct fanotify_info *info,
				     unsigned int info_mode, int pidfd,
				     char __user *buf, size_t count)
{
	int ret, total_bytes = 0, info_type = 0;
	unsigned int fid_mode = info_mode & FANOTIFY_FID_BITS;
	unsigned int pidfd_mode = info_mode & FAN_REPORT_PIDFD;

	 
	if (fanotify_event_has_dir_fh(event)) {
		info_type = info->name_len ? FAN_EVENT_INFO_TYPE_DFID_NAME :
					     FAN_EVENT_INFO_TYPE_DFID;

		 
		if (event->mask & FAN_RENAME)
			info_type = FAN_EVENT_INFO_TYPE_OLD_DFID_NAME;

		ret = copy_fid_info_to_user(fanotify_event_fsid(event),
					    fanotify_info_dir_fh(info),
					    info_type,
					    fanotify_info_name(info),
					    info->name_len, buf, count);
		if (ret < 0)
			return ret;

		buf += ret;
		count -= ret;
		total_bytes += ret;
	}

	 
	if (fanotify_event_has_dir2_fh(event)) {
		info_type = FAN_EVENT_INFO_TYPE_NEW_DFID_NAME;
		ret = copy_fid_info_to_user(fanotify_event_fsid(event),
					    fanotify_info_dir2_fh(info),
					    info_type,
					    fanotify_info_name2(info),
					    info->name2_len, buf, count);
		if (ret < 0)
			return ret;

		buf += ret;
		count -= ret;
		total_bytes += ret;
	}

	if (fanotify_event_has_object_fh(event)) {
		const char *dot = NULL;
		int dot_len = 0;

		if (fid_mode == FAN_REPORT_FID || info_type) {
			 
			info_type = FAN_EVENT_INFO_TYPE_FID;
		} else if ((fid_mode & FAN_REPORT_NAME) &&
			   (event->mask & FAN_ONDIR)) {
			 
			info_type = FAN_EVENT_INFO_TYPE_DFID_NAME;
			dot = ".";
			dot_len = 1;
		} else if ((event->mask & ALL_FSNOTIFY_DIRENT_EVENTS) ||
			   (event->mask & FAN_ONDIR)) {
			 
			info_type = FAN_EVENT_INFO_TYPE_DFID;
		} else {
			 
			info_type = FAN_EVENT_INFO_TYPE_FID;
		}

		ret = copy_fid_info_to_user(fanotify_event_fsid(event),
					    fanotify_event_object_fh(event),
					    info_type, dot, dot_len,
					    buf, count);
		if (ret < 0)
			return ret;

		buf += ret;
		count -= ret;
		total_bytes += ret;
	}

	if (pidfd_mode) {
		ret = copy_pidfd_info_to_user(pidfd, buf, count);
		if (ret < 0)
			return ret;

		buf += ret;
		count -= ret;
		total_bytes += ret;
	}

	if (fanotify_is_error_event(event->mask)) {
		ret = copy_error_info_to_user(event, buf, count);
		if (ret < 0)
			return ret;
		buf += ret;
		count -= ret;
		total_bytes += ret;
	}

	return total_bytes;
}

static ssize_t copy_event_to_user(struct fsnotify_group *group,
				  struct fanotify_event *event,
				  char __user *buf, size_t count)
{
	struct fanotify_event_metadata metadata;
	const struct path *path = fanotify_event_path(event);
	struct fanotify_info *info = fanotify_event_info(event);
	unsigned int info_mode = FAN_GROUP_FLAG(group, FANOTIFY_INFO_MODES);
	unsigned int pidfd_mode = info_mode & FAN_REPORT_PIDFD;
	struct file *f = NULL, *pidfd_file = NULL;
	int ret, pidfd = FAN_NOPIDFD, fd = FAN_NOFD;

	pr_debug("%s: group=%p event=%p\n", __func__, group, event);

	metadata.event_len = fanotify_event_len(info_mode, event);
	metadata.metadata_len = FAN_EVENT_METADATA_LEN;
	metadata.vers = FANOTIFY_METADATA_VERSION;
	metadata.reserved = 0;
	metadata.mask = event->mask & FANOTIFY_OUTGOING_EVENTS;
	metadata.pid = pid_vnr(event->pid);
	 
	if (FAN_GROUP_FLAG(group, FANOTIFY_UNPRIV) &&
	    task_tgid(current) != event->pid)
		metadata.pid = 0;

	 
	if (!FAN_GROUP_FLAG(group, FANOTIFY_UNPRIV) &&
	    path && path->mnt && path->dentry) {
		fd = create_fd(group, path, &f);
		if (fd < 0)
			return fd;
	}
	metadata.fd = fd;

	if (pidfd_mode) {
		 
		WARN_ON_ONCE(FAN_GROUP_FLAG(group, FAN_REPORT_TID));

		 
		if (metadata.pid == 0 ||
		    !pid_has_task(event->pid, PIDTYPE_TGID)) {
			pidfd = FAN_NOPIDFD;
		} else {
			pidfd = pidfd_prepare(event->pid, 0, &pidfd_file);
			if (pidfd < 0)
				pidfd = FAN_EPIDFD;
		}
	}

	ret = -EFAULT;
	 
	if (WARN_ON_ONCE(metadata.event_len > count))
		goto out_close_fd;

	if (copy_to_user(buf, &metadata, FAN_EVENT_METADATA_LEN))
		goto out_close_fd;

	buf += FAN_EVENT_METADATA_LEN;
	count -= FAN_EVENT_METADATA_LEN;

	if (fanotify_is_perm_event(event->mask))
		FANOTIFY_PERM(event)->fd = fd;

	if (info_mode) {
		ret = copy_info_records_to_user(event, info, info_mode, pidfd,
						buf, count);
		if (ret < 0)
			goto out_close_fd;
	}

	if (f)
		fd_install(fd, f);

	if (pidfd_file)
		fd_install(pidfd, pidfd_file);

	return metadata.event_len;

out_close_fd:
	if (fd != FAN_NOFD) {
		put_unused_fd(fd);
		fput(f);
	}

	if (pidfd >= 0) {
		put_unused_fd(pidfd);
		fput(pidfd_file);
	}

	return ret;
}

 
static __poll_t fanotify_poll(struct file *file, poll_table *wait)
{
	struct fsnotify_group *group = file->private_data;
	__poll_t ret = 0;

	poll_wait(file, &group->notification_waitq, wait);
	spin_lock(&group->notification_lock);
	if (!fsnotify_notify_queue_is_empty(group))
		ret = EPOLLIN | EPOLLRDNORM;
	spin_unlock(&group->notification_lock);

	return ret;
}

static ssize_t fanotify_read(struct file *file, char __user *buf,
			     size_t count, loff_t *pos)
{
	struct fsnotify_group *group;
	struct fanotify_event *event;
	char __user *start;
	int ret;
	DEFINE_WAIT_FUNC(wait, woken_wake_function);

	start = buf;
	group = file->private_data;

	pr_debug("%s: group=%p\n", __func__, group);

	add_wait_queue(&group->notification_waitq, &wait);
	while (1) {
		 
		cond_resched();
		event = get_one_event(group, count);
		if (IS_ERR(event)) {
			ret = PTR_ERR(event);
			break;
		}

		if (!event) {
			ret = -EAGAIN;
			if (file->f_flags & O_NONBLOCK)
				break;

			ret = -ERESTARTSYS;
			if (signal_pending(current))
				break;

			if (start != buf)
				break;

			wait_woken(&wait, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
			continue;
		}

		ret = copy_event_to_user(group, event, buf, count);
		if (unlikely(ret == -EOPENSTALE)) {
			 
			ret = 0;
		}

		 
		if (!fanotify_is_perm_event(event->mask)) {
			fsnotify_destroy_event(group, &event->fse);
		} else {
			if (ret <= 0) {
				spin_lock(&group->notification_lock);
				finish_permission_event(group,
					FANOTIFY_PERM(event), FAN_DENY, NULL);
				wake_up(&group->fanotify_data.access_waitq);
			} else {
				spin_lock(&group->notification_lock);
				list_add_tail(&event->fse.list,
					&group->fanotify_data.access_list);
				spin_unlock(&group->notification_lock);
			}
		}
		if (ret < 0)
			break;
		buf += ret;
		count -= ret;
	}
	remove_wait_queue(&group->notification_waitq, &wait);

	if (start != buf && ret != -EFAULT)
		ret = buf - start;
	return ret;
}

static ssize_t fanotify_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	struct fanotify_response response;
	struct fsnotify_group *group;
	int ret;
	const char __user *info_buf = buf + sizeof(struct fanotify_response);
	size_t info_len;

	if (!IS_ENABLED(CONFIG_FANOTIFY_ACCESS_PERMISSIONS))
		return -EINVAL;

	group = file->private_data;

	pr_debug("%s: group=%p count=%zu\n", __func__, group, count);

	if (count < sizeof(response))
		return -EINVAL;

	if (copy_from_user(&response, buf, sizeof(response)))
		return -EFAULT;

	info_len = count - sizeof(response);

	ret = process_access_response(group, &response, info_buf, info_len);
	if (ret < 0)
		count = ret;
	else
		count = sizeof(response) + ret;

	return count;
}

static int fanotify_release(struct inode *ignored, struct file *file)
{
	struct fsnotify_group *group = file->private_data;
	struct fsnotify_event *fsn_event;

	 
	fsnotify_group_stop_queueing(group);

	 
	spin_lock(&group->notification_lock);
	while (!list_empty(&group->fanotify_data.access_list)) {
		struct fanotify_perm_event *event;

		event = list_first_entry(&group->fanotify_data.access_list,
				struct fanotify_perm_event, fae.fse.list);
		list_del_init(&event->fae.fse.list);
		finish_permission_event(group, event, FAN_ALLOW, NULL);
		spin_lock(&group->notification_lock);
	}

	 
	while ((fsn_event = fsnotify_remove_first_event(group))) {
		struct fanotify_event *event = FANOTIFY_E(fsn_event);

		if (!(event->mask & FANOTIFY_PERM_EVENTS)) {
			spin_unlock(&group->notification_lock);
			fsnotify_destroy_event(group, fsn_event);
		} else {
			finish_permission_event(group, FANOTIFY_PERM(event),
						FAN_ALLOW, NULL);
		}
		spin_lock(&group->notification_lock);
	}
	spin_unlock(&group->notification_lock);

	 
	wake_up(&group->fanotify_data.access_waitq);

	 
	fsnotify_destroy_group(group);

	return 0;
}

static long fanotify_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fsnotify_group *group;
	struct fsnotify_event *fsn_event;
	void __user *p;
	int ret = -ENOTTY;
	size_t send_len = 0;

	group = file->private_data;

	p = (void __user *) arg;

	switch (cmd) {
	case FIONREAD:
		spin_lock(&group->notification_lock);
		list_for_each_entry(fsn_event, &group->notification_list, list)
			send_len += FAN_EVENT_METADATA_LEN;
		spin_unlock(&group->notification_lock);
		ret = put_user(send_len, (int __user *) p);
		break;
	}

	return ret;
}

static const struct file_operations fanotify_fops = {
	.show_fdinfo	= fanotify_show_fdinfo,
	.poll		= fanotify_poll,
	.read		= fanotify_read,
	.write		= fanotify_write,
	.fasync		= NULL,
	.release	= fanotify_release,
	.unlocked_ioctl	= fanotify_ioctl,
	.compat_ioctl	= compat_ptr_ioctl,
	.llseek		= noop_llseek,
};

static int fanotify_find_path(int dfd, const char __user *filename,
			      struct path *path, unsigned int flags, __u64 mask,
			      unsigned int obj_type)
{
	int ret;

	pr_debug("%s: dfd=%d filename=%p flags=%x\n", __func__,
		 dfd, filename, flags);

	if (filename == NULL) {
		struct fd f = fdget(dfd);

		ret = -EBADF;
		if (!f.file)
			goto out;

		ret = -ENOTDIR;
		if ((flags & FAN_MARK_ONLYDIR) &&
		    !(S_ISDIR(file_inode(f.file)->i_mode))) {
			fdput(f);
			goto out;
		}

		*path = f.file->f_path;
		path_get(path);
		fdput(f);
	} else {
		unsigned int lookup_flags = 0;

		if (!(flags & FAN_MARK_DONT_FOLLOW))
			lookup_flags |= LOOKUP_FOLLOW;
		if (flags & FAN_MARK_ONLYDIR)
			lookup_flags |= LOOKUP_DIRECTORY;

		ret = user_path_at(dfd, filename, lookup_flags, path);
		if (ret)
			goto out;
	}

	 
	ret = path_permission(path, MAY_READ);
	if (ret) {
		path_put(path);
		goto out;
	}

	ret = security_path_notify(path, mask, obj_type);
	if (ret)
		path_put(path);

out:
	return ret;
}

static __u32 fanotify_mark_remove_from_mask(struct fsnotify_mark *fsn_mark,
					    __u32 mask, unsigned int flags,
					    __u32 umask, int *destroy)
{
	__u32 oldmask, newmask;

	 
	mask &= ~umask;
	spin_lock(&fsn_mark->lock);
	oldmask = fsnotify_calc_mask(fsn_mark);
	if (!(flags & FANOTIFY_MARK_IGNORE_BITS)) {
		fsn_mark->mask &= ~mask;
	} else {
		fsn_mark->ignore_mask &= ~mask;
	}
	newmask = fsnotify_calc_mask(fsn_mark);
	 
	*destroy = !((fsn_mark->mask | fsn_mark->ignore_mask) & ~umask);
	spin_unlock(&fsn_mark->lock);

	return oldmask & ~newmask;
}

static int fanotify_remove_mark(struct fsnotify_group *group,
				fsnotify_connp_t *connp, __u32 mask,
				unsigned int flags, __u32 umask)
{
	struct fsnotify_mark *fsn_mark = NULL;
	__u32 removed;
	int destroy_mark;

	fsnotify_group_lock(group);
	fsn_mark = fsnotify_find_mark(connp, group);
	if (!fsn_mark) {
		fsnotify_group_unlock(group);
		return -ENOENT;
	}

	removed = fanotify_mark_remove_from_mask(fsn_mark, mask, flags,
						 umask, &destroy_mark);
	if (removed & fsnotify_conn_mask(fsn_mark->connector))
		fsnotify_recalc_mask(fsn_mark->connector);
	if (destroy_mark)
		fsnotify_detach_mark(fsn_mark);
	fsnotify_group_unlock(group);
	if (destroy_mark)
		fsnotify_free_mark(fsn_mark);

	 
	fsnotify_put_mark(fsn_mark);
	return 0;
}

static int fanotify_remove_vfsmount_mark(struct fsnotify_group *group,
					 struct vfsmount *mnt, __u32 mask,
					 unsigned int flags, __u32 umask)
{
	return fanotify_remove_mark(group, &real_mount(mnt)->mnt_fsnotify_marks,
				    mask, flags, umask);
}

static int fanotify_remove_sb_mark(struct fsnotify_group *group,
				   struct super_block *sb, __u32 mask,
				   unsigned int flags, __u32 umask)
{
	return fanotify_remove_mark(group, &sb->s_fsnotify_marks, mask,
				    flags, umask);
}

static int fanotify_remove_inode_mark(struct fsnotify_group *group,
				      struct inode *inode, __u32 mask,
				      unsigned int flags, __u32 umask)
{
	return fanotify_remove_mark(group, &inode->i_fsnotify_marks, mask,
				    flags, umask);
}

static bool fanotify_mark_update_flags(struct fsnotify_mark *fsn_mark,
				       unsigned int fan_flags)
{
	bool want_iref = !(fan_flags & FAN_MARK_EVICTABLE);
	unsigned int ignore = fan_flags & FANOTIFY_MARK_IGNORE_BITS;
	bool recalc = false;

	 
	if (ignore == FAN_MARK_IGNORE)
		fsn_mark->flags |= FSNOTIFY_MARK_FLAG_HAS_IGNORE_FLAGS;

	 
	if (ignore && (fan_flags & FAN_MARK_IGNORED_SURV_MODIFY) &&
	    !(fsn_mark->flags & FSNOTIFY_MARK_FLAG_IGNORED_SURV_MODIFY)) {
		fsn_mark->flags |= FSNOTIFY_MARK_FLAG_IGNORED_SURV_MODIFY;
		if (!(fsn_mark->mask & FS_MODIFY))
			recalc = true;
	}

	if (fsn_mark->connector->type != FSNOTIFY_OBJ_TYPE_INODE ||
	    want_iref == !(fsn_mark->flags & FSNOTIFY_MARK_FLAG_NO_IREF))
		return recalc;

	 
	WARN_ON_ONCE(!want_iref);
	fsn_mark->flags &= ~FSNOTIFY_MARK_FLAG_NO_IREF;

	return true;
}

static bool fanotify_mark_add_to_mask(struct fsnotify_mark *fsn_mark,
				      __u32 mask, unsigned int fan_flags)
{
	bool recalc;

	spin_lock(&fsn_mark->lock);
	if (!(fan_flags & FANOTIFY_MARK_IGNORE_BITS))
		fsn_mark->mask |= mask;
	else
		fsn_mark->ignore_mask |= mask;

	recalc = fsnotify_calc_mask(fsn_mark) &
		~fsnotify_conn_mask(fsn_mark->connector);

	recalc |= fanotify_mark_update_flags(fsn_mark, fan_flags);
	spin_unlock(&fsn_mark->lock);

	return recalc;
}

static struct fsnotify_mark *fanotify_add_new_mark(struct fsnotify_group *group,
						   fsnotify_connp_t *connp,
						   unsigned int obj_type,
						   unsigned int fan_flags,
						   __kernel_fsid_t *fsid)
{
	struct ucounts *ucounts = group->fanotify_data.ucounts;
	struct fsnotify_mark *mark;
	int ret;

	 
	if (!FAN_GROUP_FLAG(group, FAN_UNLIMITED_MARKS) &&
	    !inc_ucount(ucounts->ns, ucounts->uid, UCOUNT_FANOTIFY_MARKS))
		return ERR_PTR(-ENOSPC);

	mark = kmem_cache_alloc(fanotify_mark_cache, GFP_KERNEL);
	if (!mark) {
		ret = -ENOMEM;
		goto out_dec_ucounts;
	}

	fsnotify_init_mark(mark, group);
	if (fan_flags & FAN_MARK_EVICTABLE)
		mark->flags |= FSNOTIFY_MARK_FLAG_NO_IREF;

	ret = fsnotify_add_mark_locked(mark, connp, obj_type, 0, fsid);
	if (ret) {
		fsnotify_put_mark(mark);
		goto out_dec_ucounts;
	}

	return mark;

out_dec_ucounts:
	if (!FAN_GROUP_FLAG(group, FAN_UNLIMITED_MARKS))
		dec_ucount(ucounts, UCOUNT_FANOTIFY_MARKS);
	return ERR_PTR(ret);
}

static int fanotify_group_init_error_pool(struct fsnotify_group *group)
{
	if (mempool_initialized(&group->fanotify_data.error_events_pool))
		return 0;

	return mempool_init_kmalloc_pool(&group->fanotify_data.error_events_pool,
					 FANOTIFY_DEFAULT_FEE_POOL_SIZE,
					 sizeof(struct fanotify_error_event));
}

static int fanotify_may_update_existing_mark(struct fsnotify_mark *fsn_mark,
					      unsigned int fan_flags)
{
	 
	if (fan_flags & FAN_MARK_EVICTABLE &&
	    !(fsn_mark->flags & FSNOTIFY_MARK_FLAG_NO_IREF))
		return -EEXIST;

	 
	if (fan_flags & FAN_MARK_IGNORED_MASK &&
	    fsn_mark->flags & FSNOTIFY_MARK_FLAG_HAS_IGNORE_FLAGS)
		return -EEXIST;

	 
	if (fan_flags & FAN_MARK_IGNORE &&
	    !(fan_flags & FAN_MARK_IGNORED_SURV_MODIFY) &&
	    fsn_mark->flags & FSNOTIFY_MARK_FLAG_IGNORED_SURV_MODIFY)
		return -EEXIST;

	return 0;
}

static int fanotify_add_mark(struct fsnotify_group *group,
			     fsnotify_connp_t *connp, unsigned int obj_type,
			     __u32 mask, unsigned int fan_flags,
			     __kernel_fsid_t *fsid)
{
	struct fsnotify_mark *fsn_mark;
	bool recalc;
	int ret = 0;

	fsnotify_group_lock(group);
	fsn_mark = fsnotify_find_mark(connp, group);
	if (!fsn_mark) {
		fsn_mark = fanotify_add_new_mark(group, connp, obj_type,
						 fan_flags, fsid);
		if (IS_ERR(fsn_mark)) {
			fsnotify_group_unlock(group);
			return PTR_ERR(fsn_mark);
		}
	}

	 
	ret = fanotify_may_update_existing_mark(fsn_mark, fan_flags);
	if (ret)
		goto out;

	 
	if (!(fan_flags & FANOTIFY_MARK_IGNORE_BITS) &&
	    (mask & FAN_FS_ERROR)) {
		ret = fanotify_group_init_error_pool(group);
		if (ret)
			goto out;
	}

	recalc = fanotify_mark_add_to_mask(fsn_mark, mask, fan_flags);
	if (recalc)
		fsnotify_recalc_mask(fsn_mark->connector);

out:
	fsnotify_group_unlock(group);

	fsnotify_put_mark(fsn_mark);
	return ret;
}

static int fanotify_add_vfsmount_mark(struct fsnotify_group *group,
				      struct vfsmount *mnt, __u32 mask,
				      unsigned int flags, __kernel_fsid_t *fsid)
{
	return fanotify_add_mark(group, &real_mount(mnt)->mnt_fsnotify_marks,
				 FSNOTIFY_OBJ_TYPE_VFSMOUNT, mask, flags, fsid);
}

static int fanotify_add_sb_mark(struct fsnotify_group *group,
				struct super_block *sb, __u32 mask,
				unsigned int flags, __kernel_fsid_t *fsid)
{
	return fanotify_add_mark(group, &sb->s_fsnotify_marks,
				 FSNOTIFY_OBJ_TYPE_SB, mask, flags, fsid);
}

static int fanotify_add_inode_mark(struct fsnotify_group *group,
				   struct inode *inode, __u32 mask,
				   unsigned int flags, __kernel_fsid_t *fsid)
{
	pr_debug("%s: group=%p inode=%p\n", __func__, group, inode);

	 
	if ((flags & FANOTIFY_MARK_IGNORE_BITS) &&
	    !(flags & FAN_MARK_IGNORED_SURV_MODIFY) &&
	    inode_is_open_for_write(inode))
		return 0;

	return fanotify_add_mark(group, &inode->i_fsnotify_marks,
				 FSNOTIFY_OBJ_TYPE_INODE, mask, flags, fsid);
}

static struct fsnotify_event *fanotify_alloc_overflow_event(void)
{
	struct fanotify_event *oevent;

	oevent = kmalloc(sizeof(*oevent), GFP_KERNEL_ACCOUNT);
	if (!oevent)
		return NULL;

	fanotify_init_event(oevent, 0, FS_Q_OVERFLOW);
	oevent->type = FANOTIFY_EVENT_TYPE_OVERFLOW;

	return &oevent->fse;
}

static struct hlist_head *fanotify_alloc_merge_hash(void)
{
	struct hlist_head *hash;

	hash = kmalloc(sizeof(struct hlist_head) << FANOTIFY_HTABLE_BITS,
		       GFP_KERNEL_ACCOUNT);
	if (!hash)
		return NULL;

	__hash_init(hash, FANOTIFY_HTABLE_SIZE);

	return hash;
}

 
SYSCALL_DEFINE2(fanotify_init, unsigned int, flags, unsigned int, event_f_flags)
{
	struct fsnotify_group *group;
	int f_flags, fd;
	unsigned int fid_mode = flags & FANOTIFY_FID_BITS;
	unsigned int class = flags & FANOTIFY_CLASS_BITS;
	unsigned int internal_flags = 0;

	pr_debug("%s: flags=%x event_f_flags=%x\n",
		 __func__, flags, event_f_flags);

	if (!capable(CAP_SYS_ADMIN)) {
		 
		if ((flags & FANOTIFY_ADMIN_INIT_FLAGS) || !fid_mode)
			return -EPERM;

		 
		internal_flags |= FANOTIFY_UNPRIV;
	}

#ifdef CONFIG_AUDITSYSCALL
	if (flags & ~(FANOTIFY_INIT_FLAGS | FAN_ENABLE_AUDIT))
#else
	if (flags & ~FANOTIFY_INIT_FLAGS)
#endif
		return -EINVAL;

	 
	if ((flags & FAN_REPORT_PIDFD) && (flags & FAN_REPORT_TID))
		return -EINVAL;

	if (event_f_flags & ~FANOTIFY_INIT_ALL_EVENT_F_BITS)
		return -EINVAL;

	switch (event_f_flags & O_ACCMODE) {
	case O_RDONLY:
	case O_RDWR:
	case O_WRONLY:
		break;
	default:
		return -EINVAL;
	}

	if (fid_mode && class != FAN_CLASS_NOTIF)
		return -EINVAL;

	 
	if ((fid_mode & FAN_REPORT_NAME) && !(fid_mode & FAN_REPORT_DIR_FID))
		return -EINVAL;

	 
	if ((fid_mode & FAN_REPORT_TARGET_FID) &&
	    (!(fid_mode & FAN_REPORT_NAME) || !(fid_mode & FAN_REPORT_FID)))
		return -EINVAL;

	f_flags = O_RDWR | __FMODE_NONOTIFY;
	if (flags & FAN_CLOEXEC)
		f_flags |= O_CLOEXEC;
	if (flags & FAN_NONBLOCK)
		f_flags |= O_NONBLOCK;

	 
	group = fsnotify_alloc_group(&fanotify_fsnotify_ops,
				     FSNOTIFY_GROUP_USER | FSNOTIFY_GROUP_NOFS);
	if (IS_ERR(group)) {
		return PTR_ERR(group);
	}

	 
	group->fanotify_data.ucounts = inc_ucount(current_user_ns(),
						  current_euid(),
						  UCOUNT_FANOTIFY_GROUPS);
	if (!group->fanotify_data.ucounts) {
		fd = -EMFILE;
		goto out_destroy_group;
	}

	group->fanotify_data.flags = flags | internal_flags;
	group->memcg = get_mem_cgroup_from_mm(current->mm);

	group->fanotify_data.merge_hash = fanotify_alloc_merge_hash();
	if (!group->fanotify_data.merge_hash) {
		fd = -ENOMEM;
		goto out_destroy_group;
	}

	group->overflow_event = fanotify_alloc_overflow_event();
	if (unlikely(!group->overflow_event)) {
		fd = -ENOMEM;
		goto out_destroy_group;
	}

	if (force_o_largefile())
		event_f_flags |= O_LARGEFILE;
	group->fanotify_data.f_flags = event_f_flags;
	init_waitqueue_head(&group->fanotify_data.access_waitq);
	INIT_LIST_HEAD(&group->fanotify_data.access_list);
	switch (class) {
	case FAN_CLASS_NOTIF:
		group->priority = FS_PRIO_0;
		break;
	case FAN_CLASS_CONTENT:
		group->priority = FS_PRIO_1;
		break;
	case FAN_CLASS_PRE_CONTENT:
		group->priority = FS_PRIO_2;
		break;
	default:
		fd = -EINVAL;
		goto out_destroy_group;
	}

	if (flags & FAN_UNLIMITED_QUEUE) {
		fd = -EPERM;
		if (!capable(CAP_SYS_ADMIN))
			goto out_destroy_group;
		group->max_events = UINT_MAX;
	} else {
		group->max_events = fanotify_max_queued_events;
	}

	if (flags & FAN_UNLIMITED_MARKS) {
		fd = -EPERM;
		if (!capable(CAP_SYS_ADMIN))
			goto out_destroy_group;
	}

	if (flags & FAN_ENABLE_AUDIT) {
		fd = -EPERM;
		if (!capable(CAP_AUDIT_WRITE))
			goto out_destroy_group;
	}

	fd = anon_inode_getfd("[fanotify]", &fanotify_fops, group, f_flags);
	if (fd < 0)
		goto out_destroy_group;

	return fd;

out_destroy_group:
	fsnotify_destroy_group(group);
	return fd;
}

static int fanotify_test_fsid(struct dentry *dentry, __kernel_fsid_t *fsid)
{
	__kernel_fsid_t root_fsid;
	int err;

	 
	err = vfs_get_fsid(dentry, fsid);
	if (err)
		return err;

	if (!fsid->val[0] && !fsid->val[1])
		return -ENODEV;

	 
	err = vfs_get_fsid(dentry->d_sb->s_root, &root_fsid);
	if (err)
		return err;

	if (root_fsid.val[0] != fsid->val[0] ||
	    root_fsid.val[1] != fsid->val[1])
		return -EXDEV;

	return 0;
}

 
static int fanotify_test_fid(struct dentry *dentry, unsigned int flags)
{
	unsigned int mark_type = flags & FANOTIFY_MARK_TYPE_BITS;
	const struct export_operations *nop = dentry->d_sb->s_export_op;

	 
	if (!nop)
		return -EOPNOTSUPP;

	 
	if (mark_type != FAN_MARK_INODE && !nop->fh_to_dentry)
		return -EOPNOTSUPP;

	return 0;
}

static int fanotify_events_supported(struct fsnotify_group *group,
				     const struct path *path, __u64 mask,
				     unsigned int flags)
{
	unsigned int mark_type = flags & FANOTIFY_MARK_TYPE_BITS;
	 
	bool strict_dir_events = FAN_GROUP_FLAG(group, FAN_REPORT_TARGET_FID) ||
				 (mask & FAN_RENAME) ||
				 (flags & FAN_MARK_IGNORE);

	 
	if (mask & FANOTIFY_PERM_EVENTS &&
	    path->mnt->mnt_sb->s_type->fs_flags & FS_DISALLOW_NOTIFY_PERM)
		return -EINVAL;

	 
	if (mark_type != FAN_MARK_INODE &&
	    path->mnt->mnt_sb->s_flags & SB_NOUSER)
		return -EINVAL;

	 
	if (strict_dir_events && mark_type == FAN_MARK_INODE &&
	    !d_is_dir(path->dentry) && (mask & FANOTIFY_DIRONLY_EVENT_BITS))
		return -ENOTDIR;

	return 0;
}

static int do_fanotify_mark(int fanotify_fd, unsigned int flags, __u64 mask,
			    int dfd, const char  __user *pathname)
{
	struct inode *inode = NULL;
	struct vfsmount *mnt = NULL;
	struct fsnotify_group *group;
	struct fd f;
	struct path path;
	__kernel_fsid_t __fsid, *fsid = NULL;
	u32 valid_mask = FANOTIFY_EVENTS | FANOTIFY_EVENT_FLAGS;
	unsigned int mark_type = flags & FANOTIFY_MARK_TYPE_BITS;
	unsigned int mark_cmd = flags & FANOTIFY_MARK_CMD_BITS;
	unsigned int ignore = flags & FANOTIFY_MARK_IGNORE_BITS;
	unsigned int obj_type, fid_mode;
	u32 umask = 0;
	int ret;

	pr_debug("%s: fanotify_fd=%d flags=%x dfd=%d pathname=%p mask=%llx\n",
		 __func__, fanotify_fd, flags, dfd, pathname, mask);

	 
	if (upper_32_bits(mask))
		return -EINVAL;

	if (flags & ~FANOTIFY_MARK_FLAGS)
		return -EINVAL;

	switch (mark_type) {
	case FAN_MARK_INODE:
		obj_type = FSNOTIFY_OBJ_TYPE_INODE;
		break;
	case FAN_MARK_MOUNT:
		obj_type = FSNOTIFY_OBJ_TYPE_VFSMOUNT;
		break;
	case FAN_MARK_FILESYSTEM:
		obj_type = FSNOTIFY_OBJ_TYPE_SB;
		break;
	default:
		return -EINVAL;
	}

	switch (mark_cmd) {
	case FAN_MARK_ADD:
	case FAN_MARK_REMOVE:
		if (!mask)
			return -EINVAL;
		break;
	case FAN_MARK_FLUSH:
		if (flags & ~(FANOTIFY_MARK_TYPE_BITS | FAN_MARK_FLUSH))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_FANOTIFY_ACCESS_PERMISSIONS))
		valid_mask |= FANOTIFY_PERM_EVENTS;

	if (mask & ~valid_mask)
		return -EINVAL;


	 
	if (ignore == (FAN_MARK_IGNORE | FAN_MARK_IGNORED_MASK))
		return -EINVAL;

	 
	if (ignore == FAN_MARK_IGNORED_MASK) {
		mask &= ~FANOTIFY_EVENT_FLAGS;
		umask = FANOTIFY_EVENT_FLAGS;
	}

	f = fdget(fanotify_fd);
	if (unlikely(!f.file))
		return -EBADF;

	 
	ret = -EINVAL;
	if (unlikely(f.file->f_op != &fanotify_fops))
		goto fput_and_out;
	group = f.file->private_data;

	 
	ret = -EPERM;
	if ((!capable(CAP_SYS_ADMIN) ||
	     FAN_GROUP_FLAG(group, FANOTIFY_UNPRIV)) &&
	    mark_type != FAN_MARK_INODE)
		goto fput_and_out;

	 
	ret = -EINVAL;
	if (mask & FANOTIFY_PERM_EVENTS &&
	    group->priority == FS_PRIO_0)
		goto fput_and_out;

	if (mask & FAN_FS_ERROR &&
	    mark_type != FAN_MARK_FILESYSTEM)
		goto fput_and_out;

	 
	if (flags & FAN_MARK_EVICTABLE &&
	     mark_type != FAN_MARK_INODE)
		goto fput_and_out;

	 
	fid_mode = FAN_GROUP_FLAG(group, FANOTIFY_FID_BITS);
	if (mask & ~(FANOTIFY_FD_EVENTS|FANOTIFY_EVENT_FLAGS) &&
	    (!fid_mode || mark_type == FAN_MARK_MOUNT))
		goto fput_and_out;

	 
	if (mask & FAN_RENAME && !(fid_mode & FAN_REPORT_NAME))
		goto fput_and_out;

	if (mark_cmd == FAN_MARK_FLUSH) {
		ret = 0;
		if (mark_type == FAN_MARK_MOUNT)
			fsnotify_clear_vfsmount_marks_by_group(group);
		else if (mark_type == FAN_MARK_FILESYSTEM)
			fsnotify_clear_sb_marks_by_group(group);
		else
			fsnotify_clear_inode_marks_by_group(group);
		goto fput_and_out;
	}

	ret = fanotify_find_path(dfd, pathname, &path, flags,
			(mask & ALL_FSNOTIFY_EVENTS), obj_type);
	if (ret)
		goto fput_and_out;

	if (mark_cmd == FAN_MARK_ADD) {
		ret = fanotify_events_supported(group, &path, mask, flags);
		if (ret)
			goto path_put_and_out;
	}

	if (fid_mode) {
		ret = fanotify_test_fsid(path.dentry, &__fsid);
		if (ret)
			goto path_put_and_out;

		ret = fanotify_test_fid(path.dentry, flags);
		if (ret)
			goto path_put_and_out;

		fsid = &__fsid;
	}

	 
	if (mark_type == FAN_MARK_INODE)
		inode = path.dentry->d_inode;
	else
		mnt = path.mnt;

	ret = mnt ? -EINVAL : -EISDIR;
	 
	if (mark_cmd == FAN_MARK_ADD && ignore == FAN_MARK_IGNORE &&
	    (mnt || S_ISDIR(inode->i_mode)) &&
	    !(flags & FAN_MARK_IGNORED_SURV_MODIFY))
		goto path_put_and_out;

	 
	if (mnt || !S_ISDIR(inode->i_mode)) {
		mask &= ~FAN_EVENT_ON_CHILD;
		umask = FAN_EVENT_ON_CHILD;
		 
		if ((fid_mode & FAN_REPORT_DIR_FID) &&
		    (flags & FAN_MARK_ADD) && !ignore)
			mask |= FAN_EVENT_ON_CHILD;
	}

	 
	switch (mark_cmd) {
	case FAN_MARK_ADD:
		if (mark_type == FAN_MARK_MOUNT)
			ret = fanotify_add_vfsmount_mark(group, mnt, mask,
							 flags, fsid);
		else if (mark_type == FAN_MARK_FILESYSTEM)
			ret = fanotify_add_sb_mark(group, mnt->mnt_sb, mask,
						   flags, fsid);
		else
			ret = fanotify_add_inode_mark(group, inode, mask,
						      flags, fsid);
		break;
	case FAN_MARK_REMOVE:
		if (mark_type == FAN_MARK_MOUNT)
			ret = fanotify_remove_vfsmount_mark(group, mnt, mask,
							    flags, umask);
		else if (mark_type == FAN_MARK_FILESYSTEM)
			ret = fanotify_remove_sb_mark(group, mnt->mnt_sb, mask,
						      flags, umask);
		else
			ret = fanotify_remove_inode_mark(group, inode, mask,
							 flags, umask);
		break;
	default:
		ret = -EINVAL;
	}

path_put_and_out:
	path_put(&path);
fput_and_out:
	fdput(f);
	return ret;
}

#ifndef CONFIG_ARCH_SPLIT_ARG64
SYSCALL_DEFINE5(fanotify_mark, int, fanotify_fd, unsigned int, flags,
			      __u64, mask, int, dfd,
			      const char  __user *, pathname)
{
	return do_fanotify_mark(fanotify_fd, flags, mask, dfd, pathname);
}
#endif

#if defined(CONFIG_ARCH_SPLIT_ARG64) || defined(CONFIG_COMPAT)
SYSCALL32_DEFINE6(fanotify_mark,
				int, fanotify_fd, unsigned int, flags,
				SC_ARG64(mask), int, dfd,
				const char  __user *, pathname)
{
	return do_fanotify_mark(fanotify_fd, flags, SC_VAL64(__u64, mask),
				dfd, pathname);
}
#endif

 
static int __init fanotify_user_setup(void)
{
	struct sysinfo si;
	int max_marks;

	si_meminfo(&si);
	 
	max_marks = (((si.totalram - si.totalhigh) / 100) << PAGE_SHIFT) /
		    INODE_MARK_COST;
	max_marks = clamp(max_marks, FANOTIFY_OLD_DEFAULT_MAX_MARKS,
				     FANOTIFY_DEFAULT_MAX_USER_MARKS);

	BUILD_BUG_ON(FANOTIFY_INIT_FLAGS & FANOTIFY_INTERNAL_GROUP_FLAGS);
	BUILD_BUG_ON(HWEIGHT32(FANOTIFY_INIT_FLAGS) != 12);
	BUILD_BUG_ON(HWEIGHT32(FANOTIFY_MARK_FLAGS) != 11);

	fanotify_mark_cache = KMEM_CACHE(fsnotify_mark,
					 SLAB_PANIC|SLAB_ACCOUNT);
	fanotify_fid_event_cachep = KMEM_CACHE(fanotify_fid_event,
					       SLAB_PANIC);
	fanotify_path_event_cachep = KMEM_CACHE(fanotify_path_event,
						SLAB_PANIC);
	if (IS_ENABLED(CONFIG_FANOTIFY_ACCESS_PERMISSIONS)) {
		fanotify_perm_event_cachep =
			KMEM_CACHE(fanotify_perm_event, SLAB_PANIC);
	}

	fanotify_max_queued_events = FANOTIFY_DEFAULT_MAX_EVENTS;
	init_user_ns.ucount_max[UCOUNT_FANOTIFY_GROUPS] =
					FANOTIFY_DEFAULT_MAX_GROUPS;
	init_user_ns.ucount_max[UCOUNT_FANOTIFY_MARKS] = max_marks;
	fanotify_sysctls_init();

	return 0;
}
device_initcall(fanotify_user_setup);
