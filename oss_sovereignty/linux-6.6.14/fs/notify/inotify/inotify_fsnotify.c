
 

#include <linux/dcache.h>  
#include <linux/fs.h>  
#include <linux/fsnotify_backend.h>
#include <linux/inotify.h>
#include <linux/path.h>  
#include <linux/slab.h>  
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/sched/user.h>
#include <linux/sched/mm.h>

#include "inotify.h"

 
static bool event_compare(struct fsnotify_event *old_fsn,
			  struct fsnotify_event *new_fsn)
{
	struct inotify_event_info *old, *new;

	old = INOTIFY_E(old_fsn);
	new = INOTIFY_E(new_fsn);
	if (old->mask & FS_IN_IGNORED)
		return false;
	if ((old->mask == new->mask) &&
	    (old->wd == new->wd) &&
	    (old->name_len == new->name_len) &&
	    (!old->name_len || !strcmp(old->name, new->name)))
		return true;
	return false;
}

static int inotify_merge(struct fsnotify_group *group,
			 struct fsnotify_event *event)
{
	struct list_head *list = &group->notification_list;
	struct fsnotify_event *last_event;

	last_event = list_entry(list->prev, struct fsnotify_event, list);
	return event_compare(last_event, event);
}

int inotify_handle_inode_event(struct fsnotify_mark *inode_mark, u32 mask,
			       struct inode *inode, struct inode *dir,
			       const struct qstr *name, u32 cookie)
{
	struct inotify_inode_mark *i_mark;
	struct inotify_event_info *event;
	struct fsnotify_event *fsn_event;
	struct fsnotify_group *group = inode_mark->group;
	int ret;
	int len = 0, wd;
	int alloc_len = sizeof(struct inotify_event_info);
	struct mem_cgroup *old_memcg;

	if (name) {
		len = name->len;
		alloc_len += len + 1;
	}

	pr_debug("%s: group=%p mark=%p mask=%x\n", __func__, group, inode_mark,
		 mask);

	i_mark = container_of(inode_mark, struct inotify_inode_mark,
			      fsn_mark);

	 
	wd = READ_ONCE(i_mark->wd);
	if (wd == -1)
		return 0;
	 
	old_memcg = set_active_memcg(group->memcg);
	event = kmalloc(alloc_len, GFP_KERNEL_ACCOUNT | __GFP_RETRY_MAYFAIL);
	set_active_memcg(old_memcg);

	if (unlikely(!event)) {
		 
		fsnotify_queue_overflow(group);
		return -ENOMEM;
	}

	 
	if (mask & (IN_MOVE_SELF | IN_DELETE_SELF))
		mask &= ~IN_ISDIR;

	fsn_event = &event->fse;
	fsnotify_init_event(fsn_event);
	event->mask = mask;
	event->wd = wd;
	event->sync_cookie = cookie;
	event->name_len = len;
	if (len)
		strcpy(event->name, name->name);

	ret = fsnotify_add_event(group, fsn_event, inotify_merge);
	if (ret) {
		 
		fsnotify_destroy_event(group, fsn_event);
	}

	if (inode_mark->flags & FSNOTIFY_MARK_FLAG_IN_ONESHOT)
		fsnotify_destroy_mark(inode_mark, group);

	return 0;
}

static void inotify_freeing_mark(struct fsnotify_mark *fsn_mark, struct fsnotify_group *group)
{
	inotify_ignored_and_remove_idr(fsn_mark, group);
}

 
static int idr_callback(int id, void *p, void *data)
{
	struct fsnotify_mark *fsn_mark;
	struct inotify_inode_mark *i_mark;
	static bool warned = false;

	if (warned)
		return 0;

	warned = true;
	fsn_mark = p;
	i_mark = container_of(fsn_mark, struct inotify_inode_mark, fsn_mark);

	WARN(1, "inotify closing but id=%d for fsn_mark=%p in group=%p still in "
		"idr.  Probably leaking memory\n", id, p, data);

	 
	if (fsn_mark)
		printk(KERN_WARNING "fsn_mark->group=%p wd=%d\n",
			fsn_mark->group, i_mark->wd);
	return 0;
}

static void inotify_free_group_priv(struct fsnotify_group *group)
{
	 
	idr_for_each(&group->inotify_data.idr, idr_callback, group);
	idr_destroy(&group->inotify_data.idr);
	if (group->inotify_data.ucounts)
		dec_inotify_instances(group->inotify_data.ucounts);
}

static void inotify_free_event(struct fsnotify_group *group,
			       struct fsnotify_event *fsn_event)
{
	kfree(INOTIFY_E(fsn_event));
}

 
static void inotify_free_mark(struct fsnotify_mark *fsn_mark)
{
	struct inotify_inode_mark *i_mark;

	i_mark = container_of(fsn_mark, struct inotify_inode_mark, fsn_mark);

	kmem_cache_free(inotify_inode_mark_cachep, i_mark);
}

const struct fsnotify_ops inotify_fsnotify_ops = {
	.handle_inode_event = inotify_handle_inode_event,
	.free_group_priv = inotify_free_group_priv,
	.free_event = inotify_free_event,
	.freeing_mark = inotify_freeing_mark,
	.free_mark = inotify_free_mark,
};
