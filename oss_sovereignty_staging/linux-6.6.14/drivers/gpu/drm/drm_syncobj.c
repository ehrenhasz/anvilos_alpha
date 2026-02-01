 

 

#include <linux/anon_inodes.h>
#include <linux/dma-fence-unwrap.h>
#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/sched/signal.h>
#include <linux/sync_file.h>
#include <linux/uaccess.h>

#include <drm/drm.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_gem.h>
#include <drm/drm_print.h>
#include <drm/drm_syncobj.h>
#include <drm/drm_utils.h>

#include "drm_internal.h"

struct syncobj_wait_entry {
	struct list_head node;
	struct task_struct *task;
	struct dma_fence *fence;
	struct dma_fence_cb fence_cb;
	u64    point;
};

static void syncobj_wait_syncobj_func(struct drm_syncobj *syncobj,
				      struct syncobj_wait_entry *wait);

struct syncobj_eventfd_entry {
	struct list_head node;
	struct dma_fence *fence;
	struct dma_fence_cb fence_cb;
	struct drm_syncobj *syncobj;
	struct eventfd_ctx *ev_fd_ctx;
	u64 point;
	u32 flags;
};

static void
syncobj_eventfd_entry_func(struct drm_syncobj *syncobj,
			   struct syncobj_eventfd_entry *entry);

 
struct drm_syncobj *drm_syncobj_find(struct drm_file *file_private,
				     u32 handle)
{
	struct drm_syncobj *syncobj;

	spin_lock(&file_private->syncobj_table_lock);

	 
	syncobj = idr_find(&file_private->syncobj_idr, handle);
	if (syncobj)
		drm_syncobj_get(syncobj);

	spin_unlock(&file_private->syncobj_table_lock);

	return syncobj;
}
EXPORT_SYMBOL(drm_syncobj_find);

static void drm_syncobj_fence_add_wait(struct drm_syncobj *syncobj,
				       struct syncobj_wait_entry *wait)
{
	struct dma_fence *fence;

	if (wait->fence)
		return;

	spin_lock(&syncobj->lock);
	 
	fence = dma_fence_get(rcu_dereference_protected(syncobj->fence, 1));
	if (!fence || dma_fence_chain_find_seqno(&fence, wait->point)) {
		dma_fence_put(fence);
		list_add_tail(&wait->node, &syncobj->cb_list);
	} else if (!fence) {
		wait->fence = dma_fence_get_stub();
	} else {
		wait->fence = fence;
	}
	spin_unlock(&syncobj->lock);
}

static void drm_syncobj_remove_wait(struct drm_syncobj *syncobj,
				    struct syncobj_wait_entry *wait)
{
	if (!wait->node.next)
		return;

	spin_lock(&syncobj->lock);
	list_del_init(&wait->node);
	spin_unlock(&syncobj->lock);
}

static void
syncobj_eventfd_entry_free(struct syncobj_eventfd_entry *entry)
{
	eventfd_ctx_put(entry->ev_fd_ctx);
	dma_fence_put(entry->fence);
	 
	list_del(&entry->node);
	kfree(entry);
}

static void
drm_syncobj_add_eventfd(struct drm_syncobj *syncobj,
			struct syncobj_eventfd_entry *entry)
{
	spin_lock(&syncobj->lock);
	list_add_tail(&entry->node, &syncobj->ev_fd_list);
	syncobj_eventfd_entry_func(syncobj, entry);
	spin_unlock(&syncobj->lock);
}

 
void drm_syncobj_add_point(struct drm_syncobj *syncobj,
			   struct dma_fence_chain *chain,
			   struct dma_fence *fence,
			   uint64_t point)
{
	struct syncobj_wait_entry *wait_cur, *wait_tmp;
	struct syncobj_eventfd_entry *ev_fd_cur, *ev_fd_tmp;
	struct dma_fence *prev;

	dma_fence_get(fence);

	spin_lock(&syncobj->lock);

	prev = drm_syncobj_fence_get(syncobj);
	 
	if (prev && prev->seqno >= point)
		DRM_DEBUG("You are adding an unorder point to timeline!\n");
	dma_fence_chain_init(chain, prev, fence, point);
	rcu_assign_pointer(syncobj->fence, &chain->base);

	list_for_each_entry_safe(wait_cur, wait_tmp, &syncobj->cb_list, node)
		syncobj_wait_syncobj_func(syncobj, wait_cur);
	list_for_each_entry_safe(ev_fd_cur, ev_fd_tmp, &syncobj->ev_fd_list, node)
		syncobj_eventfd_entry_func(syncobj, ev_fd_cur);
	spin_unlock(&syncobj->lock);

	 
	dma_fence_chain_for_each(fence, prev);
	dma_fence_put(prev);
}
EXPORT_SYMBOL(drm_syncobj_add_point);

 
void drm_syncobj_replace_fence(struct drm_syncobj *syncobj,
			       struct dma_fence *fence)
{
	struct dma_fence *old_fence;
	struct syncobj_wait_entry *wait_cur, *wait_tmp;
	struct syncobj_eventfd_entry *ev_fd_cur, *ev_fd_tmp;

	if (fence)
		dma_fence_get(fence);

	spin_lock(&syncobj->lock);

	old_fence = rcu_dereference_protected(syncobj->fence,
					      lockdep_is_held(&syncobj->lock));
	rcu_assign_pointer(syncobj->fence, fence);

	if (fence != old_fence) {
		list_for_each_entry_safe(wait_cur, wait_tmp, &syncobj->cb_list, node)
			syncobj_wait_syncobj_func(syncobj, wait_cur);
		list_for_each_entry_safe(ev_fd_cur, ev_fd_tmp, &syncobj->ev_fd_list, node)
			syncobj_eventfd_entry_func(syncobj, ev_fd_cur);
	}

	spin_unlock(&syncobj->lock);

	dma_fence_put(old_fence);
}
EXPORT_SYMBOL(drm_syncobj_replace_fence);

 
static int drm_syncobj_assign_null_handle(struct drm_syncobj *syncobj)
{
	struct dma_fence *fence = dma_fence_allocate_private_stub(ktime_get());

	if (!fence)
		return -ENOMEM;

	drm_syncobj_replace_fence(syncobj, fence);
	dma_fence_put(fence);
	return 0;
}

 
#define DRM_SYNCOBJ_WAIT_FOR_SUBMIT_TIMEOUT 5000000000ULL
 
int drm_syncobj_find_fence(struct drm_file *file_private,
			   u32 handle, u64 point, u64 flags,
			   struct dma_fence **fence)
{
	struct drm_syncobj *syncobj = drm_syncobj_find(file_private, handle);
	struct syncobj_wait_entry wait;
	u64 timeout = nsecs_to_jiffies64(DRM_SYNCOBJ_WAIT_FOR_SUBMIT_TIMEOUT);
	int ret;

	if (!syncobj)
		return -ENOENT;

	 
	if (flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT) {
		might_sleep();
		lockdep_assert_none_held_once();
	}

	*fence = drm_syncobj_fence_get(syncobj);

	if (*fence) {
		ret = dma_fence_chain_find_seqno(fence, point);
		if (!ret) {
			 
			if (!*fence)
				*fence = dma_fence_get_stub();

			goto out;
		}
		dma_fence_put(*fence);
	} else {
		ret = -EINVAL;
	}

	if (!(flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT))
		goto out;

	memset(&wait, 0, sizeof(wait));
	wait.task = current;
	wait.point = point;
	drm_syncobj_fence_add_wait(syncobj, &wait);

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		if (wait.fence) {
			ret = 0;
			break;
		}
                if (timeout == 0) {
                        ret = -ETIME;
                        break;
                }

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

                timeout = schedule_timeout(timeout);
	} while (1);

	__set_current_state(TASK_RUNNING);
	*fence = wait.fence;

	if (wait.node.next)
		drm_syncobj_remove_wait(syncobj, &wait);

out:
	drm_syncobj_put(syncobj);

	return ret;
}
EXPORT_SYMBOL(drm_syncobj_find_fence);

 
void drm_syncobj_free(struct kref *kref)
{
	struct drm_syncobj *syncobj = container_of(kref,
						   struct drm_syncobj,
						   refcount);
	struct syncobj_eventfd_entry *ev_fd_cur, *ev_fd_tmp;

	drm_syncobj_replace_fence(syncobj, NULL);

	list_for_each_entry_safe(ev_fd_cur, ev_fd_tmp, &syncobj->ev_fd_list, node)
		syncobj_eventfd_entry_free(ev_fd_cur);

	kfree(syncobj);
}
EXPORT_SYMBOL(drm_syncobj_free);

 
int drm_syncobj_create(struct drm_syncobj **out_syncobj, uint32_t flags,
		       struct dma_fence *fence)
{
	int ret;
	struct drm_syncobj *syncobj;

	syncobj = kzalloc(sizeof(struct drm_syncobj), GFP_KERNEL);
	if (!syncobj)
		return -ENOMEM;

	kref_init(&syncobj->refcount);
	INIT_LIST_HEAD(&syncobj->cb_list);
	INIT_LIST_HEAD(&syncobj->ev_fd_list);
	spin_lock_init(&syncobj->lock);

	if (flags & DRM_SYNCOBJ_CREATE_SIGNALED) {
		ret = drm_syncobj_assign_null_handle(syncobj);
		if (ret < 0) {
			drm_syncobj_put(syncobj);
			return ret;
		}
	}

	if (fence)
		drm_syncobj_replace_fence(syncobj, fence);

	*out_syncobj = syncobj;
	return 0;
}
EXPORT_SYMBOL(drm_syncobj_create);

 
int drm_syncobj_get_handle(struct drm_file *file_private,
			   struct drm_syncobj *syncobj, u32 *handle)
{
	int ret;

	 
	drm_syncobj_get(syncobj);

	idr_preload(GFP_KERNEL);
	spin_lock(&file_private->syncobj_table_lock);
	ret = idr_alloc(&file_private->syncobj_idr, syncobj, 1, 0, GFP_NOWAIT);
	spin_unlock(&file_private->syncobj_table_lock);

	idr_preload_end();

	if (ret < 0) {
		drm_syncobj_put(syncobj);
		return ret;
	}

	*handle = ret;
	return 0;
}
EXPORT_SYMBOL(drm_syncobj_get_handle);

static int drm_syncobj_create_as_handle(struct drm_file *file_private,
					u32 *handle, uint32_t flags)
{
	int ret;
	struct drm_syncobj *syncobj;

	ret = drm_syncobj_create(&syncobj, flags, NULL);
	if (ret)
		return ret;

	ret = drm_syncobj_get_handle(file_private, syncobj, handle);
	drm_syncobj_put(syncobj);
	return ret;
}

static int drm_syncobj_destroy(struct drm_file *file_private,
			       u32 handle)
{
	struct drm_syncobj *syncobj;

	spin_lock(&file_private->syncobj_table_lock);
	syncobj = idr_remove(&file_private->syncobj_idr, handle);
	spin_unlock(&file_private->syncobj_table_lock);

	if (!syncobj)
		return -EINVAL;

	drm_syncobj_put(syncobj);
	return 0;
}

static int drm_syncobj_file_release(struct inode *inode, struct file *file)
{
	struct drm_syncobj *syncobj = file->private_data;

	drm_syncobj_put(syncobj);
	return 0;
}

static const struct file_operations drm_syncobj_file_fops = {
	.release = drm_syncobj_file_release,
};

 
int drm_syncobj_get_fd(struct drm_syncobj *syncobj, int *p_fd)
{
	struct file *file;
	int fd;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0)
		return fd;

	file = anon_inode_getfile("syncobj_file",
				  &drm_syncobj_file_fops,
				  syncobj, 0);
	if (IS_ERR(file)) {
		put_unused_fd(fd);
		return PTR_ERR(file);
	}

	drm_syncobj_get(syncobj);
	fd_install(fd, file);

	*p_fd = fd;
	return 0;
}
EXPORT_SYMBOL(drm_syncobj_get_fd);

static int drm_syncobj_handle_to_fd(struct drm_file *file_private,
				    u32 handle, int *p_fd)
{
	struct drm_syncobj *syncobj = drm_syncobj_find(file_private, handle);
	int ret;

	if (!syncobj)
		return -EINVAL;

	ret = drm_syncobj_get_fd(syncobj, p_fd);
	drm_syncobj_put(syncobj);
	return ret;
}

static int drm_syncobj_fd_to_handle(struct drm_file *file_private,
				    int fd, u32 *handle)
{
	struct drm_syncobj *syncobj;
	struct fd f = fdget(fd);
	int ret;

	if (!f.file)
		return -EINVAL;

	if (f.file->f_op != &drm_syncobj_file_fops) {
		fdput(f);
		return -EINVAL;
	}

	 
	syncobj = f.file->private_data;
	drm_syncobj_get(syncobj);

	idr_preload(GFP_KERNEL);
	spin_lock(&file_private->syncobj_table_lock);
	ret = idr_alloc(&file_private->syncobj_idr, syncobj, 1, 0, GFP_NOWAIT);
	spin_unlock(&file_private->syncobj_table_lock);
	idr_preload_end();

	if (ret > 0) {
		*handle = ret;
		ret = 0;
	} else
		drm_syncobj_put(syncobj);

	fdput(f);
	return ret;
}

static int drm_syncobj_import_sync_file_fence(struct drm_file *file_private,
					      int fd, int handle)
{
	struct dma_fence *fence = sync_file_get_fence(fd);
	struct drm_syncobj *syncobj;

	if (!fence)
		return -EINVAL;

	syncobj = drm_syncobj_find(file_private, handle);
	if (!syncobj) {
		dma_fence_put(fence);
		return -ENOENT;
	}

	drm_syncobj_replace_fence(syncobj, fence);
	dma_fence_put(fence);
	drm_syncobj_put(syncobj);
	return 0;
}

static int drm_syncobj_export_sync_file(struct drm_file *file_private,
					int handle, int *p_fd)
{
	int ret;
	struct dma_fence *fence;
	struct sync_file *sync_file;
	int fd = get_unused_fd_flags(O_CLOEXEC);

	if (fd < 0)
		return fd;

	ret = drm_syncobj_find_fence(file_private, handle, 0, 0, &fence);
	if (ret)
		goto err_put_fd;

	sync_file = sync_file_create(fence);

	dma_fence_put(fence);

	if (!sync_file) {
		ret = -EINVAL;
		goto err_put_fd;
	}

	fd_install(fd, sync_file->file);

	*p_fd = fd;
	return 0;
err_put_fd:
	put_unused_fd(fd);
	return ret;
}
 
void
drm_syncobj_open(struct drm_file *file_private)
{
	idr_init_base(&file_private->syncobj_idr, 1);
	spin_lock_init(&file_private->syncobj_table_lock);
}

static int
drm_syncobj_release_handle(int id, void *ptr, void *data)
{
	struct drm_syncobj *syncobj = ptr;

	drm_syncobj_put(syncobj);
	return 0;
}

 
void
drm_syncobj_release(struct drm_file *file_private)
{
	idr_for_each(&file_private->syncobj_idr,
		     &drm_syncobj_release_handle, file_private);
	idr_destroy(&file_private->syncobj_idr);
}

int
drm_syncobj_create_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_private)
{
	struct drm_syncobj_create *args = data;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		return -EOPNOTSUPP;

	 
	if (args->flags & ~DRM_SYNCOBJ_CREATE_SIGNALED)
		return -EINVAL;

	return drm_syncobj_create_as_handle(file_private,
					    &args->handle, args->flags);
}

int
drm_syncobj_destroy_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_private)
{
	struct drm_syncobj_destroy *args = data;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		return -EOPNOTSUPP;

	 
	if (args->pad)
		return -EINVAL;
	return drm_syncobj_destroy(file_private, args->handle);
}

int
drm_syncobj_handle_to_fd_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_private)
{
	struct drm_syncobj_handle *args = data;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		return -EOPNOTSUPP;

	if (args->pad)
		return -EINVAL;

	if (args->flags != 0 &&
	    args->flags != DRM_SYNCOBJ_HANDLE_TO_FD_FLAGS_EXPORT_SYNC_FILE)
		return -EINVAL;

	if (args->flags & DRM_SYNCOBJ_HANDLE_TO_FD_FLAGS_EXPORT_SYNC_FILE)
		return drm_syncobj_export_sync_file(file_private, args->handle,
						    &args->fd);

	return drm_syncobj_handle_to_fd(file_private, args->handle,
					&args->fd);
}

int
drm_syncobj_fd_to_handle_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_private)
{
	struct drm_syncobj_handle *args = data;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		return -EOPNOTSUPP;

	if (args->pad)
		return -EINVAL;

	if (args->flags != 0 &&
	    args->flags != DRM_SYNCOBJ_FD_TO_HANDLE_FLAGS_IMPORT_SYNC_FILE)
		return -EINVAL;

	if (args->flags & DRM_SYNCOBJ_FD_TO_HANDLE_FLAGS_IMPORT_SYNC_FILE)
		return drm_syncobj_import_sync_file_fence(file_private,
							  args->fd,
							  args->handle);

	return drm_syncobj_fd_to_handle(file_private, args->fd,
					&args->handle);
}

static int drm_syncobj_transfer_to_timeline(struct drm_file *file_private,
					    struct drm_syncobj_transfer *args)
{
	struct drm_syncobj *timeline_syncobj = NULL;
	struct dma_fence *fence, *tmp;
	struct dma_fence_chain *chain;
	int ret;

	timeline_syncobj = drm_syncobj_find(file_private, args->dst_handle);
	if (!timeline_syncobj) {
		return -ENOENT;
	}
	ret = drm_syncobj_find_fence(file_private, args->src_handle,
				     args->src_point, args->flags,
				     &tmp);
	if (ret)
		goto err_put_timeline;

	fence = dma_fence_unwrap_merge(tmp);
	dma_fence_put(tmp);
	if (!fence) {
		ret = -ENOMEM;
		goto err_put_timeline;
	}

	chain = dma_fence_chain_alloc();
	if (!chain) {
		ret = -ENOMEM;
		goto err_free_fence;
	}

	drm_syncobj_add_point(timeline_syncobj, chain, fence, args->dst_point);
err_free_fence:
	dma_fence_put(fence);
err_put_timeline:
	drm_syncobj_put(timeline_syncobj);

	return ret;
}

static int
drm_syncobj_transfer_to_binary(struct drm_file *file_private,
			       struct drm_syncobj_transfer *args)
{
	struct drm_syncobj *binary_syncobj = NULL;
	struct dma_fence *fence;
	int ret;

	binary_syncobj = drm_syncobj_find(file_private, args->dst_handle);
	if (!binary_syncobj)
		return -ENOENT;
	ret = drm_syncobj_find_fence(file_private, args->src_handle,
				     args->src_point, args->flags, &fence);
	if (ret)
		goto err;
	drm_syncobj_replace_fence(binary_syncobj, fence);
	dma_fence_put(fence);
err:
	drm_syncobj_put(binary_syncobj);

	return ret;
}
int
drm_syncobj_transfer_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_private)
{
	struct drm_syncobj_transfer *args = data;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ_TIMELINE))
		return -EOPNOTSUPP;

	if (args->pad)
		return -EINVAL;

	if (args->dst_point)
		ret = drm_syncobj_transfer_to_timeline(file_private, args);
	else
		ret = drm_syncobj_transfer_to_binary(file_private, args);

	return ret;
}

static void syncobj_wait_fence_func(struct dma_fence *fence,
				    struct dma_fence_cb *cb)
{
	struct syncobj_wait_entry *wait =
		container_of(cb, struct syncobj_wait_entry, fence_cb);

	wake_up_process(wait->task);
}

static void syncobj_wait_syncobj_func(struct drm_syncobj *syncobj,
				      struct syncobj_wait_entry *wait)
{
	struct dma_fence *fence;

	 
	fence = rcu_dereference_protected(syncobj->fence,
					  lockdep_is_held(&syncobj->lock));
	dma_fence_get(fence);
	if (!fence || dma_fence_chain_find_seqno(&fence, wait->point)) {
		dma_fence_put(fence);
		return;
	} else if (!fence) {
		wait->fence = dma_fence_get_stub();
	} else {
		wait->fence = fence;
	}

	wake_up_process(wait->task);
	list_del_init(&wait->node);
}

static signed long drm_syncobj_array_wait_timeout(struct drm_syncobj **syncobjs,
						  void __user *user_points,
						  uint32_t count,
						  uint32_t flags,
						  signed long timeout,
						  uint32_t *idx)
{
	struct syncobj_wait_entry *entries;
	struct dma_fence *fence;
	uint64_t *points;
	uint32_t signaled_count, i;

	if (flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT)
		lockdep_assert_none_held_once();

	points = kmalloc_array(count, sizeof(*points), GFP_KERNEL);
	if (points == NULL)
		return -ENOMEM;

	if (!user_points) {
		memset(points, 0, count * sizeof(uint64_t));

	} else if (copy_from_user(points, user_points,
				  sizeof(uint64_t) * count)) {
		timeout = -EFAULT;
		goto err_free_points;
	}

	entries = kcalloc(count, sizeof(*entries), GFP_KERNEL);
	if (!entries) {
		timeout = -ENOMEM;
		goto err_free_points;
	}
	 
	signaled_count = 0;
	for (i = 0; i < count; ++i) {
		struct dma_fence *fence;

		entries[i].task = current;
		entries[i].point = points[i];
		fence = drm_syncobj_fence_get(syncobjs[i]);
		if (!fence || dma_fence_chain_find_seqno(&fence, points[i])) {
			dma_fence_put(fence);
			if (flags & (DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT |
				     DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE)) {
				continue;
			} else {
				timeout = -EINVAL;
				goto cleanup_entries;
			}
		}

		if (fence)
			entries[i].fence = fence;
		else
			entries[i].fence = dma_fence_get_stub();

		if ((flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE) ||
		    dma_fence_is_signaled(entries[i].fence)) {
			if (signaled_count == 0 && idx)
				*idx = i;
			signaled_count++;
		}
	}

	if (signaled_count == count ||
	    (signaled_count > 0 &&
	     !(flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL)))
		goto cleanup_entries;

	 

	if (flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT) {
		for (i = 0; i < count; ++i)
			drm_syncobj_fence_add_wait(syncobjs[i], &entries[i]);
	}

	do {
		set_current_state(TASK_INTERRUPTIBLE);

		signaled_count = 0;
		for (i = 0; i < count; ++i) {
			fence = entries[i].fence;
			if (!fence)
				continue;

			if ((flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE) ||
			    dma_fence_is_signaled(fence) ||
			    (!entries[i].fence_cb.func &&
			     dma_fence_add_callback(fence,
						    &entries[i].fence_cb,
						    syncobj_wait_fence_func))) {
				 
				if (flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL) {
					signaled_count++;
				} else {
					if (idx)
						*idx = i;
					goto done_waiting;
				}
			}
		}

		if (signaled_count == count)
			goto done_waiting;

		if (timeout == 0) {
			timeout = -ETIME;
			goto done_waiting;
		}

		if (signal_pending(current)) {
			timeout = -ERESTARTSYS;
			goto done_waiting;
		}

		timeout = schedule_timeout(timeout);
	} while (1);

done_waiting:
	__set_current_state(TASK_RUNNING);

cleanup_entries:
	for (i = 0; i < count; ++i) {
		drm_syncobj_remove_wait(syncobjs[i], &entries[i]);
		if (entries[i].fence_cb.func)
			dma_fence_remove_callback(entries[i].fence,
						  &entries[i].fence_cb);
		dma_fence_put(entries[i].fence);
	}
	kfree(entries);

err_free_points:
	kfree(points);

	return timeout;
}

 
signed long drm_timeout_abs_to_jiffies(int64_t timeout_nsec)
{
	ktime_t abs_timeout, now;
	u64 timeout_ns, timeout_jiffies64;

	 
	if (timeout_nsec == 0)
		return 0;

	abs_timeout = ns_to_ktime(timeout_nsec);
	now = ktime_get();

	if (!ktime_after(abs_timeout, now))
		return 0;

	timeout_ns = ktime_to_ns(ktime_sub(abs_timeout, now));

	timeout_jiffies64 = nsecs_to_jiffies64(timeout_ns);
	 
	if (timeout_jiffies64 >= MAX_SCHEDULE_TIMEOUT - 1)
		return MAX_SCHEDULE_TIMEOUT - 1;

	return timeout_jiffies64 + 1;
}
EXPORT_SYMBOL(drm_timeout_abs_to_jiffies);

static int drm_syncobj_array_wait(struct drm_device *dev,
				  struct drm_file *file_private,
				  struct drm_syncobj_wait *wait,
				  struct drm_syncobj_timeline_wait *timeline_wait,
				  struct drm_syncobj **syncobjs, bool timeline)
{
	signed long timeout = 0;
	uint32_t first = ~0;

	if (!timeline) {
		timeout = drm_timeout_abs_to_jiffies(wait->timeout_nsec);
		timeout = drm_syncobj_array_wait_timeout(syncobjs,
							 NULL,
							 wait->count_handles,
							 wait->flags,
							 timeout, &first);
		if (timeout < 0)
			return timeout;
		wait->first_signaled = first;
	} else {
		timeout = drm_timeout_abs_to_jiffies(timeline_wait->timeout_nsec);
		timeout = drm_syncobj_array_wait_timeout(syncobjs,
							 u64_to_user_ptr(timeline_wait->points),
							 timeline_wait->count_handles,
							 timeline_wait->flags,
							 timeout, &first);
		if (timeout < 0)
			return timeout;
		timeline_wait->first_signaled = first;
	}
	return 0;
}

static int drm_syncobj_array_find(struct drm_file *file_private,
				  void __user *user_handles,
				  uint32_t count_handles,
				  struct drm_syncobj ***syncobjs_out)
{
	uint32_t i, *handles;
	struct drm_syncobj **syncobjs;
	int ret;

	handles = kmalloc_array(count_handles, sizeof(*handles), GFP_KERNEL);
	if (handles == NULL)
		return -ENOMEM;

	if (copy_from_user(handles, user_handles,
			   sizeof(uint32_t) * count_handles)) {
		ret = -EFAULT;
		goto err_free_handles;
	}

	syncobjs = kmalloc_array(count_handles, sizeof(*syncobjs), GFP_KERNEL);
	if (syncobjs == NULL) {
		ret = -ENOMEM;
		goto err_free_handles;
	}

	for (i = 0; i < count_handles; i++) {
		syncobjs[i] = drm_syncobj_find(file_private, handles[i]);
		if (!syncobjs[i]) {
			ret = -ENOENT;
			goto err_put_syncobjs;
		}
	}

	kfree(handles);
	*syncobjs_out = syncobjs;
	return 0;

err_put_syncobjs:
	while (i-- > 0)
		drm_syncobj_put(syncobjs[i]);
	kfree(syncobjs);
err_free_handles:
	kfree(handles);

	return ret;
}

static void drm_syncobj_array_free(struct drm_syncobj **syncobjs,
				   uint32_t count)
{
	uint32_t i;

	for (i = 0; i < count; i++)
		drm_syncobj_put(syncobjs[i]);
	kfree(syncobjs);
}

int
drm_syncobj_wait_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_private)
{
	struct drm_syncobj_wait *args = data;
	struct drm_syncobj **syncobjs;
	int ret = 0;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		return -EOPNOTSUPP;

	if (args->flags & ~(DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL |
			    DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT))
		return -EINVAL;

	if (args->count_handles == 0)
		return -EINVAL;

	ret = drm_syncobj_array_find(file_private,
				     u64_to_user_ptr(args->handles),
				     args->count_handles,
				     &syncobjs);
	if (ret < 0)
		return ret;

	ret = drm_syncobj_array_wait(dev, file_private,
				     args, NULL, syncobjs, false);

	drm_syncobj_array_free(syncobjs, args->count_handles);

	return ret;
}

int
drm_syncobj_timeline_wait_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_private)
{
	struct drm_syncobj_timeline_wait *args = data;
	struct drm_syncobj **syncobjs;
	int ret = 0;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ_TIMELINE))
		return -EOPNOTSUPP;

	if (args->flags & ~(DRM_SYNCOBJ_WAIT_FLAGS_WAIT_ALL |
			    DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT |
			    DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE))
		return -EINVAL;

	if (args->count_handles == 0)
		return -EINVAL;

	ret = drm_syncobj_array_find(file_private,
				     u64_to_user_ptr(args->handles),
				     args->count_handles,
				     &syncobjs);
	if (ret < 0)
		return ret;

	ret = drm_syncobj_array_wait(dev, file_private,
				     NULL, args, syncobjs, true);

	drm_syncobj_array_free(syncobjs, args->count_handles);

	return ret;
}

static void syncobj_eventfd_entry_fence_func(struct dma_fence *fence,
					     struct dma_fence_cb *cb)
{
	struct syncobj_eventfd_entry *entry =
		container_of(cb, struct syncobj_eventfd_entry, fence_cb);

	eventfd_signal(entry->ev_fd_ctx, 1);
	syncobj_eventfd_entry_free(entry);
}

static void
syncobj_eventfd_entry_func(struct drm_syncobj *syncobj,
			   struct syncobj_eventfd_entry *entry)
{
	int ret;
	struct dma_fence *fence;

	 
	fence = dma_fence_get(rcu_dereference_protected(syncobj->fence, 1));
	ret = dma_fence_chain_find_seqno(&fence, entry->point);
	if (ret != 0 || !fence) {
		dma_fence_put(fence);
		return;
	}

	list_del_init(&entry->node);
	entry->fence = fence;

	if (entry->flags & DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE) {
		eventfd_signal(entry->ev_fd_ctx, 1);
		syncobj_eventfd_entry_free(entry);
	} else {
		ret = dma_fence_add_callback(fence, &entry->fence_cb,
					     syncobj_eventfd_entry_fence_func);
		if (ret == -ENOENT) {
			eventfd_signal(entry->ev_fd_ctx, 1);
			syncobj_eventfd_entry_free(entry);
		}
	}
}

int
drm_syncobj_eventfd_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_private)
{
	struct drm_syncobj_eventfd *args = data;
	struct drm_syncobj *syncobj;
	struct eventfd_ctx *ev_fd_ctx;
	struct syncobj_eventfd_entry *entry;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ_TIMELINE))
		return -EOPNOTSUPP;

	if (args->flags & ~DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE)
		return -EINVAL;

	if (args->pad)
		return -EINVAL;

	syncobj = drm_syncobj_find(file_private, args->handle);
	if (!syncobj)
		return -ENOENT;

	ev_fd_ctx = eventfd_ctx_fdget(args->fd);
	if (IS_ERR(ev_fd_ctx))
		return PTR_ERR(ev_fd_ctx);

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		eventfd_ctx_put(ev_fd_ctx);
		return -ENOMEM;
	}
	entry->syncobj = syncobj;
	entry->ev_fd_ctx = ev_fd_ctx;
	entry->point = args->point;
	entry->flags = args->flags;

	drm_syncobj_add_eventfd(syncobj, entry);
	drm_syncobj_put(syncobj);

	return 0;
}

int
drm_syncobj_reset_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file_private)
{
	struct drm_syncobj_array *args = data;
	struct drm_syncobj **syncobjs;
	uint32_t i;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		return -EOPNOTSUPP;

	if (args->pad != 0)
		return -EINVAL;

	if (args->count_handles == 0)
		return -EINVAL;

	ret = drm_syncobj_array_find(file_private,
				     u64_to_user_ptr(args->handles),
				     args->count_handles,
				     &syncobjs);
	if (ret < 0)
		return ret;

	for (i = 0; i < args->count_handles; i++)
		drm_syncobj_replace_fence(syncobjs[i], NULL);

	drm_syncobj_array_free(syncobjs, args->count_handles);

	return 0;
}

int
drm_syncobj_signal_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_private)
{
	struct drm_syncobj_array *args = data;
	struct drm_syncobj **syncobjs;
	uint32_t i;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ))
		return -EOPNOTSUPP;

	if (args->pad != 0)
		return -EINVAL;

	if (args->count_handles == 0)
		return -EINVAL;

	ret = drm_syncobj_array_find(file_private,
				     u64_to_user_ptr(args->handles),
				     args->count_handles,
				     &syncobjs);
	if (ret < 0)
		return ret;

	for (i = 0; i < args->count_handles; i++) {
		ret = drm_syncobj_assign_null_handle(syncobjs[i]);
		if (ret < 0)
			break;
	}

	drm_syncobj_array_free(syncobjs, args->count_handles);

	return ret;
}

int
drm_syncobj_timeline_signal_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_private)
{
	struct drm_syncobj_timeline_array *args = data;
	struct drm_syncobj **syncobjs;
	struct dma_fence_chain **chains;
	uint64_t *points;
	uint32_t i, j;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ_TIMELINE))
		return -EOPNOTSUPP;

	if (args->flags != 0)
		return -EINVAL;

	if (args->count_handles == 0)
		return -EINVAL;

	ret = drm_syncobj_array_find(file_private,
				     u64_to_user_ptr(args->handles),
				     args->count_handles,
				     &syncobjs);
	if (ret < 0)
		return ret;

	points = kmalloc_array(args->count_handles, sizeof(*points),
			       GFP_KERNEL);
	if (!points) {
		ret = -ENOMEM;
		goto out;
	}
	if (!u64_to_user_ptr(args->points)) {
		memset(points, 0, args->count_handles * sizeof(uint64_t));
	} else if (copy_from_user(points, u64_to_user_ptr(args->points),
				  sizeof(uint64_t) * args->count_handles)) {
		ret = -EFAULT;
		goto err_points;
	}

	chains = kmalloc_array(args->count_handles, sizeof(void *), GFP_KERNEL);
	if (!chains) {
		ret = -ENOMEM;
		goto err_points;
	}
	for (i = 0; i < args->count_handles; i++) {
		chains[i] = dma_fence_chain_alloc();
		if (!chains[i]) {
			for (j = 0; j < i; j++)
				dma_fence_chain_free(chains[j]);
			ret = -ENOMEM;
			goto err_chains;
		}
	}

	for (i = 0; i < args->count_handles; i++) {
		struct dma_fence *fence = dma_fence_get_stub();

		drm_syncobj_add_point(syncobjs[i], chains[i],
				      fence, points[i]);
		dma_fence_put(fence);
	}
err_chains:
	kfree(chains);
err_points:
	kfree(points);
out:
	drm_syncobj_array_free(syncobjs, args->count_handles);

	return ret;
}

int drm_syncobj_query_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_private)
{
	struct drm_syncobj_timeline_array *args = data;
	struct drm_syncobj **syncobjs;
	uint64_t __user *points = u64_to_user_ptr(args->points);
	uint32_t i;
	int ret;

	if (!drm_core_check_feature(dev, DRIVER_SYNCOBJ_TIMELINE))
		return -EOPNOTSUPP;

	if (args->flags & ~DRM_SYNCOBJ_QUERY_FLAGS_LAST_SUBMITTED)
		return -EINVAL;

	if (args->count_handles == 0)
		return -EINVAL;

	ret = drm_syncobj_array_find(file_private,
				     u64_to_user_ptr(args->handles),
				     args->count_handles,
				     &syncobjs);
	if (ret < 0)
		return ret;

	for (i = 0; i < args->count_handles; i++) {
		struct dma_fence_chain *chain;
		struct dma_fence *fence;
		uint64_t point;

		fence = drm_syncobj_fence_get(syncobjs[i]);
		chain = to_dma_fence_chain(fence);
		if (chain) {
			struct dma_fence *iter, *last_signaled =
				dma_fence_get(fence);

			if (args->flags &
			    DRM_SYNCOBJ_QUERY_FLAGS_LAST_SUBMITTED) {
				point = fence->seqno;
			} else {
				dma_fence_chain_for_each(iter, fence) {
					if (iter->context != fence->context) {
						dma_fence_put(iter);
						 
						break;
					}
					dma_fence_put(last_signaled);
					last_signaled = dma_fence_get(iter);
				}
				point = dma_fence_is_signaled(last_signaled) ?
					last_signaled->seqno :
					to_dma_fence_chain(last_signaled)->prev_seqno;
			}
			dma_fence_put(last_signaled);
		} else {
			point = 0;
		}
		dma_fence_put(fence);
		ret = copy_to_user(&points[i], &point, sizeof(uint64_t));
		ret = ret ? -EFAULT : 0;
		if (ret)
			break;
	}
	drm_syncobj_array_free(syncobjs, args->count_handles);

	return ret;
}
