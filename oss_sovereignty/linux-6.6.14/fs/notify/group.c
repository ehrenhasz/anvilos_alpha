
 

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/srcu.h>
#include <linux/rculist.h>
#include <linux/wait.h>
#include <linux/memcontrol.h>

#include <linux/fsnotify_backend.h>
#include "fsnotify.h"

#include <linux/atomic.h>

 
static void fsnotify_final_destroy_group(struct fsnotify_group *group)
{
	if (group->ops->free_group_priv)
		group->ops->free_group_priv(group);

	mem_cgroup_put(group->memcg);
	mutex_destroy(&group->mark_mutex);

	kfree(group);
}

 
void fsnotify_group_stop_queueing(struct fsnotify_group *group)
{
	spin_lock(&group->notification_lock);
	group->shutdown = true;
	spin_unlock(&group->notification_lock);
}

 
void fsnotify_destroy_group(struct fsnotify_group *group)
{
	 
	fsnotify_group_stop_queueing(group);

	 
	fsnotify_clear_marks_by_group(group, FSNOTIFY_OBJ_TYPE_ANY);

	 
	wait_event(group->notification_waitq, !atomic_read(&group->user_waits));

	 
	fsnotify_wait_marks_destroyed();

	 
	fsnotify_flush_notify(group);

	 
	if (group->overflow_event)
		group->ops->free_event(group, group->overflow_event);

	fsnotify_put_group(group);
}

 
void fsnotify_get_group(struct fsnotify_group *group)
{
	refcount_inc(&group->refcnt);
}

 
void fsnotify_put_group(struct fsnotify_group *group)
{
	if (refcount_dec_and_test(&group->refcnt))
		fsnotify_final_destroy_group(group);
}
EXPORT_SYMBOL_GPL(fsnotify_put_group);

static struct fsnotify_group *__fsnotify_alloc_group(
				const struct fsnotify_ops *ops,
				int flags, gfp_t gfp)
{
	static struct lock_class_key nofs_marks_lock;
	struct fsnotify_group *group;

	group = kzalloc(sizeof(struct fsnotify_group), gfp);
	if (!group)
		return ERR_PTR(-ENOMEM);

	 
	refcount_set(&group->refcnt, 1);
	atomic_set(&group->user_waits, 0);

	spin_lock_init(&group->notification_lock);
	INIT_LIST_HEAD(&group->notification_list);
	init_waitqueue_head(&group->notification_waitq);
	group->max_events = UINT_MAX;

	mutex_init(&group->mark_mutex);
	INIT_LIST_HEAD(&group->marks_list);

	group->ops = ops;
	group->flags = flags;
	 
	if (flags & FSNOTIFY_GROUP_NOFS)
		lockdep_set_class(&group->mark_mutex, &nofs_marks_lock);

	return group;
}

 
struct fsnotify_group *fsnotify_alloc_group(const struct fsnotify_ops *ops,
					    int flags)
{
	gfp_t gfp = (flags & FSNOTIFY_GROUP_USER) ? GFP_KERNEL_ACCOUNT :
						    GFP_KERNEL;

	return __fsnotify_alloc_group(ops, flags, gfp);
}
EXPORT_SYMBOL_GPL(fsnotify_alloc_group);

int fsnotify_fasync(int fd, struct file *file, int on)
{
	struct fsnotify_group *group = file->private_data;

	return fasync_helper(fd, file, on, &group->fsn_fa) >= 0 ? 0 : -EIO;
}
