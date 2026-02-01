
 

 

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/srcu.h>
#include <linux/ratelimit.h>

#include <linux/atomic.h>

#include <linux/fsnotify_backend.h>
#include "fsnotify.h"

#define FSNOTIFY_REAPER_DELAY	(1)	 

struct srcu_struct fsnotify_mark_srcu;
struct kmem_cache *fsnotify_mark_connector_cachep;

static DEFINE_SPINLOCK(destroy_lock);
static LIST_HEAD(destroy_list);
static struct fsnotify_mark_connector *connector_destroy_list;

static void fsnotify_mark_destroy_workfn(struct work_struct *work);
static DECLARE_DELAYED_WORK(reaper_work, fsnotify_mark_destroy_workfn);

static void fsnotify_connector_destroy_workfn(struct work_struct *work);
static DECLARE_WORK(connector_reaper_work, fsnotify_connector_destroy_workfn);

void fsnotify_get_mark(struct fsnotify_mark *mark)
{
	WARN_ON_ONCE(!refcount_read(&mark->refcnt));
	refcount_inc(&mark->refcnt);
}

static __u32 *fsnotify_conn_mask_p(struct fsnotify_mark_connector *conn)
{
	if (conn->type == FSNOTIFY_OBJ_TYPE_INODE)
		return &fsnotify_conn_inode(conn)->i_fsnotify_mask;
	else if (conn->type == FSNOTIFY_OBJ_TYPE_VFSMOUNT)
		return &fsnotify_conn_mount(conn)->mnt_fsnotify_mask;
	else if (conn->type == FSNOTIFY_OBJ_TYPE_SB)
		return &fsnotify_conn_sb(conn)->s_fsnotify_mask;
	return NULL;
}

__u32 fsnotify_conn_mask(struct fsnotify_mark_connector *conn)
{
	if (WARN_ON(!fsnotify_valid_obj_type(conn->type)))
		return 0;

	return *fsnotify_conn_mask_p(conn);
}

static void fsnotify_get_inode_ref(struct inode *inode)
{
	ihold(inode);
	atomic_long_inc(&inode->i_sb->s_fsnotify_connectors);
}

 
static struct inode *fsnotify_update_iref(struct fsnotify_mark_connector *conn,
					  bool want_iref)
{
	bool has_iref = conn->flags & FSNOTIFY_CONN_FLAG_HAS_IREF;
	struct inode *inode = NULL;

	if (conn->type != FSNOTIFY_OBJ_TYPE_INODE ||
	    want_iref == has_iref)
		return NULL;

	if (want_iref) {
		 
		fsnotify_get_inode_ref(fsnotify_conn_inode(conn));
		conn->flags |= FSNOTIFY_CONN_FLAG_HAS_IREF;
	} else {
		 
		inode = fsnotify_conn_inode(conn);
		conn->flags &= ~FSNOTIFY_CONN_FLAG_HAS_IREF;
	}

	return inode;
}

static void *__fsnotify_recalc_mask(struct fsnotify_mark_connector *conn)
{
	u32 new_mask = 0;
	bool want_iref = false;
	struct fsnotify_mark *mark;

	assert_spin_locked(&conn->lock);
	 
	if (!fsnotify_valid_obj_type(conn->type))
		return NULL;
	hlist_for_each_entry(mark, &conn->list, obj_list) {
		if (!(mark->flags & FSNOTIFY_MARK_FLAG_ATTACHED))
			continue;
		new_mask |= fsnotify_calc_mask(mark);
		if (conn->type == FSNOTIFY_OBJ_TYPE_INODE &&
		    !(mark->flags & FSNOTIFY_MARK_FLAG_NO_IREF))
			want_iref = true;
	}
	*fsnotify_conn_mask_p(conn) = new_mask;

	return fsnotify_update_iref(conn, want_iref);
}

 
void fsnotify_recalc_mask(struct fsnotify_mark_connector *conn)
{
	if (!conn)
		return;

	spin_lock(&conn->lock);
	__fsnotify_recalc_mask(conn);
	spin_unlock(&conn->lock);
	if (conn->type == FSNOTIFY_OBJ_TYPE_INODE)
		__fsnotify_update_child_dentry_flags(
					fsnotify_conn_inode(conn));
}

 
static void fsnotify_connector_destroy_workfn(struct work_struct *work)
{
	struct fsnotify_mark_connector *conn, *free;

	spin_lock(&destroy_lock);
	conn = connector_destroy_list;
	connector_destroy_list = NULL;
	spin_unlock(&destroy_lock);

	synchronize_srcu(&fsnotify_mark_srcu);
	while (conn) {
		free = conn;
		conn = conn->destroy_next;
		kmem_cache_free(fsnotify_mark_connector_cachep, free);
	}
}

static void fsnotify_put_inode_ref(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;

	iput(inode);
	if (atomic_long_dec_and_test(&sb->s_fsnotify_connectors))
		wake_up_var(&sb->s_fsnotify_connectors);
}

static void fsnotify_get_sb_connectors(struct fsnotify_mark_connector *conn)
{
	struct super_block *sb = fsnotify_connector_sb(conn);

	if (sb)
		atomic_long_inc(&sb->s_fsnotify_connectors);
}

static void fsnotify_put_sb_connectors(struct fsnotify_mark_connector *conn)
{
	struct super_block *sb = fsnotify_connector_sb(conn);

	if (sb && atomic_long_dec_and_test(&sb->s_fsnotify_connectors))
		wake_up_var(&sb->s_fsnotify_connectors);
}

static void *fsnotify_detach_connector_from_object(
					struct fsnotify_mark_connector *conn,
					unsigned int *type)
{
	struct inode *inode = NULL;

	*type = conn->type;
	if (conn->type == FSNOTIFY_OBJ_TYPE_DETACHED)
		return NULL;

	if (conn->type == FSNOTIFY_OBJ_TYPE_INODE) {
		inode = fsnotify_conn_inode(conn);
		inode->i_fsnotify_mask = 0;

		 
		if (!(conn->flags & FSNOTIFY_CONN_FLAG_HAS_IREF))
			inode = NULL;
	} else if (conn->type == FSNOTIFY_OBJ_TYPE_VFSMOUNT) {
		fsnotify_conn_mount(conn)->mnt_fsnotify_mask = 0;
	} else if (conn->type == FSNOTIFY_OBJ_TYPE_SB) {
		fsnotify_conn_sb(conn)->s_fsnotify_mask = 0;
	}

	fsnotify_put_sb_connectors(conn);
	rcu_assign_pointer(*(conn->obj), NULL);
	conn->obj = NULL;
	conn->type = FSNOTIFY_OBJ_TYPE_DETACHED;

	return inode;
}

static void fsnotify_final_mark_destroy(struct fsnotify_mark *mark)
{
	struct fsnotify_group *group = mark->group;

	if (WARN_ON_ONCE(!group))
		return;
	group->ops->free_mark(mark);
	fsnotify_put_group(group);
}

 
static void fsnotify_drop_object(unsigned int type, void *objp)
{
	if (!objp)
		return;
	 
	if (WARN_ON_ONCE(type != FSNOTIFY_OBJ_TYPE_INODE))
		return;
	fsnotify_put_inode_ref(objp);
}

void fsnotify_put_mark(struct fsnotify_mark *mark)
{
	struct fsnotify_mark_connector *conn = READ_ONCE(mark->connector);
	void *objp = NULL;
	unsigned int type = FSNOTIFY_OBJ_TYPE_DETACHED;
	bool free_conn = false;

	 
	if (!conn) {
		if (refcount_dec_and_test(&mark->refcnt))
			fsnotify_final_mark_destroy(mark);
		return;
	}

	 
	if (!refcount_dec_and_lock(&mark->refcnt, &conn->lock))
		return;

	hlist_del_init_rcu(&mark->obj_list);
	if (hlist_empty(&conn->list)) {
		objp = fsnotify_detach_connector_from_object(conn, &type);
		free_conn = true;
	} else {
		objp = __fsnotify_recalc_mask(conn);
		type = conn->type;
	}
	WRITE_ONCE(mark->connector, NULL);
	spin_unlock(&conn->lock);

	fsnotify_drop_object(type, objp);

	if (free_conn) {
		spin_lock(&destroy_lock);
		conn->destroy_next = connector_destroy_list;
		connector_destroy_list = conn;
		spin_unlock(&destroy_lock);
		queue_work(system_unbound_wq, &connector_reaper_work);
	}
	 
	spin_lock(&destroy_lock);
	list_add(&mark->g_list, &destroy_list);
	spin_unlock(&destroy_lock);
	queue_delayed_work(system_unbound_wq, &reaper_work,
			   FSNOTIFY_REAPER_DELAY);
}
EXPORT_SYMBOL_GPL(fsnotify_put_mark);

 
static bool fsnotify_get_mark_safe(struct fsnotify_mark *mark)
{
	if (!mark)
		return true;

	if (refcount_inc_not_zero(&mark->refcnt)) {
		spin_lock(&mark->lock);
		if (mark->flags & FSNOTIFY_MARK_FLAG_ATTACHED) {
			 
			atomic_inc(&mark->group->user_waits);
			spin_unlock(&mark->lock);
			return true;
		}
		spin_unlock(&mark->lock);
		fsnotify_put_mark(mark);
	}
	return false;
}

 
static void fsnotify_put_mark_wake(struct fsnotify_mark *mark)
{
	if (mark) {
		struct fsnotify_group *group = mark->group;

		fsnotify_put_mark(mark);
		 
		if (atomic_dec_and_test(&group->user_waits) && group->shutdown)
			wake_up(&group->notification_waitq);
	}
}

bool fsnotify_prepare_user_wait(struct fsnotify_iter_info *iter_info)
	__releases(&fsnotify_mark_srcu)
{
	int type;

	fsnotify_foreach_iter_type(type) {
		 
		if (!fsnotify_get_mark_safe(iter_info->marks[type])) {
			__release(&fsnotify_mark_srcu);
			goto fail;
		}
	}

	 
	srcu_read_unlock(&fsnotify_mark_srcu, iter_info->srcu_idx);

	return true;

fail:
	for (type--; type >= 0; type--)
		fsnotify_put_mark_wake(iter_info->marks[type]);
	return false;
}

void fsnotify_finish_user_wait(struct fsnotify_iter_info *iter_info)
	__acquires(&fsnotify_mark_srcu)
{
	int type;

	iter_info->srcu_idx = srcu_read_lock(&fsnotify_mark_srcu);
	fsnotify_foreach_iter_type(type)
		fsnotify_put_mark_wake(iter_info->marks[type]);
}

 
void fsnotify_detach_mark(struct fsnotify_mark *mark)
{
	fsnotify_group_assert_locked(mark->group);
	WARN_ON_ONCE(!srcu_read_lock_held(&fsnotify_mark_srcu) &&
		     refcount_read(&mark->refcnt) < 1 +
			!!(mark->flags & FSNOTIFY_MARK_FLAG_ATTACHED));

	spin_lock(&mark->lock);
	 
	if (!(mark->flags & FSNOTIFY_MARK_FLAG_ATTACHED)) {
		spin_unlock(&mark->lock);
		return;
	}
	mark->flags &= ~FSNOTIFY_MARK_FLAG_ATTACHED;
	list_del_init(&mark->g_list);
	spin_unlock(&mark->lock);

	 
	fsnotify_put_mark(mark);
}

 
void fsnotify_free_mark(struct fsnotify_mark *mark)
{
	struct fsnotify_group *group = mark->group;

	spin_lock(&mark->lock);
	 
	if (!(mark->flags & FSNOTIFY_MARK_FLAG_ALIVE)) {
		spin_unlock(&mark->lock);
		return;
	}
	mark->flags &= ~FSNOTIFY_MARK_FLAG_ALIVE;
	spin_unlock(&mark->lock);

	 
	if (group->ops->freeing_mark)
		group->ops->freeing_mark(mark, group);
}

void fsnotify_destroy_mark(struct fsnotify_mark *mark,
			   struct fsnotify_group *group)
{
	fsnotify_group_lock(group);
	fsnotify_detach_mark(mark);
	fsnotify_group_unlock(group);
	fsnotify_free_mark(mark);
}
EXPORT_SYMBOL_GPL(fsnotify_destroy_mark);

 
int fsnotify_compare_groups(struct fsnotify_group *a, struct fsnotify_group *b)
{
	if (a == b)
		return 0;
	if (!a)
		return 1;
	if (!b)
		return -1;
	if (a->priority < b->priority)
		return 1;
	if (a->priority > b->priority)
		return -1;
	if (a < b)
		return 1;
	return -1;
}

static int fsnotify_attach_connector_to_object(fsnotify_connp_t *connp,
					       unsigned int obj_type,
					       __kernel_fsid_t *fsid)
{
	struct fsnotify_mark_connector *conn;

	conn = kmem_cache_alloc(fsnotify_mark_connector_cachep, GFP_KERNEL);
	if (!conn)
		return -ENOMEM;
	spin_lock_init(&conn->lock);
	INIT_HLIST_HEAD(&conn->list);
	conn->flags = 0;
	conn->type = obj_type;
	conn->obj = connp;
	 
	if (fsid) {
		conn->fsid = *fsid;
		conn->flags = FSNOTIFY_CONN_FLAG_HAS_FSID;
	} else {
		conn->fsid.val[0] = conn->fsid.val[1] = 0;
		conn->flags = 0;
	}
	fsnotify_get_sb_connectors(conn);

	 
	if (cmpxchg(connp, NULL, conn)) {
		 
		fsnotify_put_sb_connectors(conn);
		kmem_cache_free(fsnotify_mark_connector_cachep, conn);
	}

	return 0;
}

 
static struct fsnotify_mark_connector *fsnotify_grab_connector(
						fsnotify_connp_t *connp)
{
	struct fsnotify_mark_connector *conn;
	int idx;

	idx = srcu_read_lock(&fsnotify_mark_srcu);
	conn = srcu_dereference(*connp, &fsnotify_mark_srcu);
	if (!conn)
		goto out;
	spin_lock(&conn->lock);
	if (conn->type == FSNOTIFY_OBJ_TYPE_DETACHED) {
		spin_unlock(&conn->lock);
		srcu_read_unlock(&fsnotify_mark_srcu, idx);
		return NULL;
	}
out:
	srcu_read_unlock(&fsnotify_mark_srcu, idx);
	return conn;
}

 
static int fsnotify_add_mark_list(struct fsnotify_mark *mark,
				  fsnotify_connp_t *connp,
				  unsigned int obj_type,
				  int add_flags, __kernel_fsid_t *fsid)
{
	struct fsnotify_mark *lmark, *last = NULL;
	struct fsnotify_mark_connector *conn;
	int cmp;
	int err = 0;

	if (WARN_ON(!fsnotify_valid_obj_type(obj_type)))
		return -EINVAL;

	 
	if (fsid && WARN_ON_ONCE(!fsid->val[0] && !fsid->val[1]))
		return -ENODEV;

restart:
	spin_lock(&mark->lock);
	conn = fsnotify_grab_connector(connp);
	if (!conn) {
		spin_unlock(&mark->lock);
		err = fsnotify_attach_connector_to_object(connp, obj_type,
							  fsid);
		if (err)
			return err;
		goto restart;
	} else if (fsid && !(conn->flags & FSNOTIFY_CONN_FLAG_HAS_FSID)) {
		conn->fsid = *fsid;
		 
		smp_wmb();
		conn->flags |= FSNOTIFY_CONN_FLAG_HAS_FSID;
	} else if (fsid && (conn->flags & FSNOTIFY_CONN_FLAG_HAS_FSID) &&
		   (fsid->val[0] != conn->fsid.val[0] ||
		    fsid->val[1] != conn->fsid.val[1])) {
		 
		pr_warn_ratelimited("%s: fsid mismatch on object of type %u: "
				    "%x.%x != %x.%x\n", __func__, conn->type,
				    fsid->val[0], fsid->val[1],
				    conn->fsid.val[0], conn->fsid.val[1]);
		err = -EXDEV;
		goto out_err;
	}

	 
	if (hlist_empty(&conn->list)) {
		hlist_add_head_rcu(&mark->obj_list, &conn->list);
		goto added;
	}

	 
	hlist_for_each_entry(lmark, &conn->list, obj_list) {
		last = lmark;

		if ((lmark->group == mark->group) &&
		    (lmark->flags & FSNOTIFY_MARK_FLAG_ATTACHED) &&
		    !(mark->group->flags & FSNOTIFY_GROUP_DUPS)) {
			err = -EEXIST;
			goto out_err;
		}

		cmp = fsnotify_compare_groups(lmark->group, mark->group);
		if (cmp >= 0) {
			hlist_add_before_rcu(&mark->obj_list, &lmark->obj_list);
			goto added;
		}
	}

	BUG_ON(last == NULL);
	 
	hlist_add_behind_rcu(&mark->obj_list, &last->obj_list);
added:
	 
	WRITE_ONCE(mark->connector, conn);
out_err:
	spin_unlock(&conn->lock);
	spin_unlock(&mark->lock);
	return err;
}

 
int fsnotify_add_mark_locked(struct fsnotify_mark *mark,
			     fsnotify_connp_t *connp, unsigned int obj_type,
			     int add_flags, __kernel_fsid_t *fsid)
{
	struct fsnotify_group *group = mark->group;
	int ret = 0;

	fsnotify_group_assert_locked(group);

	 
	spin_lock(&mark->lock);
	mark->flags |= FSNOTIFY_MARK_FLAG_ALIVE | FSNOTIFY_MARK_FLAG_ATTACHED;

	list_add(&mark->g_list, &group->marks_list);
	fsnotify_get_mark(mark);  
	spin_unlock(&mark->lock);

	ret = fsnotify_add_mark_list(mark, connp, obj_type, add_flags, fsid);
	if (ret)
		goto err;

	fsnotify_recalc_mask(mark->connector);

	return ret;
err:
	spin_lock(&mark->lock);
	mark->flags &= ~(FSNOTIFY_MARK_FLAG_ALIVE |
			 FSNOTIFY_MARK_FLAG_ATTACHED);
	list_del_init(&mark->g_list);
	spin_unlock(&mark->lock);

	fsnotify_put_mark(mark);
	return ret;
}

int fsnotify_add_mark(struct fsnotify_mark *mark, fsnotify_connp_t *connp,
		      unsigned int obj_type, int add_flags,
		      __kernel_fsid_t *fsid)
{
	int ret;
	struct fsnotify_group *group = mark->group;

	fsnotify_group_lock(group);
	ret = fsnotify_add_mark_locked(mark, connp, obj_type, add_flags, fsid);
	fsnotify_group_unlock(group);
	return ret;
}
EXPORT_SYMBOL_GPL(fsnotify_add_mark);

 
struct fsnotify_mark *fsnotify_find_mark(fsnotify_connp_t *connp,
					 struct fsnotify_group *group)
{
	struct fsnotify_mark_connector *conn;
	struct fsnotify_mark *mark;

	conn = fsnotify_grab_connector(connp);
	if (!conn)
		return NULL;

	hlist_for_each_entry(mark, &conn->list, obj_list) {
		if (mark->group == group &&
		    (mark->flags & FSNOTIFY_MARK_FLAG_ATTACHED)) {
			fsnotify_get_mark(mark);
			spin_unlock(&conn->lock);
			return mark;
		}
	}
	spin_unlock(&conn->lock);
	return NULL;
}
EXPORT_SYMBOL_GPL(fsnotify_find_mark);

 
void fsnotify_clear_marks_by_group(struct fsnotify_group *group,
				   unsigned int obj_type)
{
	struct fsnotify_mark *lmark, *mark;
	LIST_HEAD(to_free);
	struct list_head *head = &to_free;

	 
	if (obj_type == FSNOTIFY_OBJ_TYPE_ANY) {
		head = &group->marks_list;
		goto clear;
	}
	 
	fsnotify_group_lock(group);
	list_for_each_entry_safe(mark, lmark, &group->marks_list, g_list) {
		if (mark->connector->type == obj_type)
			list_move(&mark->g_list, &to_free);
	}
	fsnotify_group_unlock(group);

clear:
	while (1) {
		fsnotify_group_lock(group);
		if (list_empty(head)) {
			fsnotify_group_unlock(group);
			break;
		}
		mark = list_first_entry(head, struct fsnotify_mark, g_list);
		fsnotify_get_mark(mark);
		fsnotify_detach_mark(mark);
		fsnotify_group_unlock(group);
		fsnotify_free_mark(mark);
		fsnotify_put_mark(mark);
	}
}

 
void fsnotify_destroy_marks(fsnotify_connp_t *connp)
{
	struct fsnotify_mark_connector *conn;
	struct fsnotify_mark *mark, *old_mark = NULL;
	void *objp;
	unsigned int type;

	conn = fsnotify_grab_connector(connp);
	if (!conn)
		return;
	 
	hlist_for_each_entry(mark, &conn->list, obj_list) {
		fsnotify_get_mark(mark);
		spin_unlock(&conn->lock);
		if (old_mark)
			fsnotify_put_mark(old_mark);
		old_mark = mark;
		fsnotify_destroy_mark(mark, mark->group);
		spin_lock(&conn->lock);
	}
	 
	objp = fsnotify_detach_connector_from_object(conn, &type);
	spin_unlock(&conn->lock);
	if (old_mark)
		fsnotify_put_mark(old_mark);
	fsnotify_drop_object(type, objp);
}

 
void fsnotify_init_mark(struct fsnotify_mark *mark,
			struct fsnotify_group *group)
{
	memset(mark, 0, sizeof(*mark));
	spin_lock_init(&mark->lock);
	refcount_set(&mark->refcnt, 1);
	fsnotify_get_group(group);
	mark->group = group;
	WRITE_ONCE(mark->connector, NULL);
}
EXPORT_SYMBOL_GPL(fsnotify_init_mark);

 
static void fsnotify_mark_destroy_workfn(struct work_struct *work)
{
	struct fsnotify_mark *mark, *next;
	struct list_head private_destroy_list;

	spin_lock(&destroy_lock);
	 
	list_replace_init(&destroy_list, &private_destroy_list);
	spin_unlock(&destroy_lock);

	synchronize_srcu(&fsnotify_mark_srcu);

	list_for_each_entry_safe(mark, next, &private_destroy_list, g_list) {
		list_del_init(&mark->g_list);
		fsnotify_final_mark_destroy(mark);
	}
}

 
void fsnotify_wait_marks_destroyed(void)
{
	flush_delayed_work(&reaper_work);
}
EXPORT_SYMBOL_GPL(fsnotify_wait_marks_destroyed);
