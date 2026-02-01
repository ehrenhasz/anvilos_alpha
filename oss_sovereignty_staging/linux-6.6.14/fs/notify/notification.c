
 

 

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/atomic.h>

#include <linux/fsnotify_backend.h>
#include "fsnotify.h"

static atomic_t fsnotify_sync_cookie = ATOMIC_INIT(0);

 
u32 fsnotify_get_cookie(void)
{
	return atomic_inc_return(&fsnotify_sync_cookie);
}
EXPORT_SYMBOL_GPL(fsnotify_get_cookie);

void fsnotify_destroy_event(struct fsnotify_group *group,
			    struct fsnotify_event *event)
{
	 
	if (!event || event == group->overflow_event)
		return;
	 
	if (!list_empty(&event->list)) {
		spin_lock(&group->notification_lock);
		WARN_ON(!list_empty(&event->list));
		spin_unlock(&group->notification_lock);
	}
	group->ops->free_event(group, event);
}

 
int fsnotify_insert_event(struct fsnotify_group *group,
			  struct fsnotify_event *event,
			  int (*merge)(struct fsnotify_group *,
				       struct fsnotify_event *),
			  void (*insert)(struct fsnotify_group *,
					 struct fsnotify_event *))
{
	int ret = 0;
	struct list_head *list = &group->notification_list;

	pr_debug("%s: group=%p event=%p\n", __func__, group, event);

	spin_lock(&group->notification_lock);

	if (group->shutdown) {
		spin_unlock(&group->notification_lock);
		return 2;
	}

	if (event == group->overflow_event ||
	    group->q_len >= group->max_events) {
		ret = 2;
		 
		if (!list_empty(&group->overflow_event->list)) {
			spin_unlock(&group->notification_lock);
			return ret;
		}
		event = group->overflow_event;
		goto queue;
	}

	if (!list_empty(list) && merge) {
		ret = merge(group, event);
		if (ret) {
			spin_unlock(&group->notification_lock);
			return ret;
		}
	}

queue:
	group->q_len++;
	list_add_tail(&event->list, list);
	if (insert)
		insert(group, event);
	spin_unlock(&group->notification_lock);

	wake_up(&group->notification_waitq);
	kill_fasync(&group->fsn_fa, SIGIO, POLL_IN);
	return ret;
}

void fsnotify_remove_queued_event(struct fsnotify_group *group,
				  struct fsnotify_event *event)
{
	assert_spin_locked(&group->notification_lock);
	 
	list_del_init(&event->list);
	group->q_len--;
}

 
struct fsnotify_event *fsnotify_peek_first_event(struct fsnotify_group *group)
{
	assert_spin_locked(&group->notification_lock);

	if (fsnotify_notify_queue_is_empty(group))
		return NULL;

	return list_first_entry(&group->notification_list,
				struct fsnotify_event, list);
}

 
struct fsnotify_event *fsnotify_remove_first_event(struct fsnotify_group *group)
{
	struct fsnotify_event *event = fsnotify_peek_first_event(group);

	if (!event)
		return NULL;

	pr_debug("%s: group=%p event=%p\n", __func__, group, event);

	fsnotify_remove_queued_event(group, event);

	return event;
}

 
void fsnotify_flush_notify(struct fsnotify_group *group)
{
	struct fsnotify_event *event;

	spin_lock(&group->notification_lock);
	while (!fsnotify_notify_queue_is_empty(group)) {
		event = fsnotify_remove_first_event(group);
		spin_unlock(&group->notification_lock);
		fsnotify_destroy_event(group, event);
		spin_lock(&group->notification_lock);
	}
	spin_unlock(&group->notification_lock);
}
