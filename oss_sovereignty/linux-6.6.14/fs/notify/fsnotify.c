
 

#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/srcu.h>

#include <linux/fsnotify_backend.h>
#include "fsnotify.h"

 
void __fsnotify_inode_delete(struct inode *inode)
{
	fsnotify_clear_marks_by_inode(inode);
}
EXPORT_SYMBOL_GPL(__fsnotify_inode_delete);

void __fsnotify_vfsmount_delete(struct vfsmount *mnt)
{
	fsnotify_clear_marks_by_mount(mnt);
}

 
static void fsnotify_unmount_inodes(struct super_block *sb)
{
	struct inode *inode, *iput_inode = NULL;

	spin_lock(&sb->s_inode_list_lock);
	list_for_each_entry(inode, &sb->s_inodes, i_sb_list) {
		 
		spin_lock(&inode->i_lock);
		if (inode->i_state & (I_FREEING|I_WILL_FREE|I_NEW)) {
			spin_unlock(&inode->i_lock);
			continue;
		}

		 
		if (!atomic_read(&inode->i_count)) {
			spin_unlock(&inode->i_lock);
			continue;
		}

		__iget(inode);
		spin_unlock(&inode->i_lock);
		spin_unlock(&sb->s_inode_list_lock);

		iput(iput_inode);

		 
		fsnotify_inode(inode, FS_UNMOUNT);

		fsnotify_inode_delete(inode);

		iput_inode = inode;

		cond_resched();
		spin_lock(&sb->s_inode_list_lock);
	}
	spin_unlock(&sb->s_inode_list_lock);

	iput(iput_inode);
}

void fsnotify_sb_delete(struct super_block *sb)
{
	fsnotify_unmount_inodes(sb);
	fsnotify_clear_marks_by_sb(sb);
	 
	wait_var_event(&sb->s_fsnotify_connectors,
		       !atomic_long_read(&sb->s_fsnotify_connectors));
}

 
void __fsnotify_update_child_dentry_flags(struct inode *inode)
{
	struct dentry *alias;
	int watched;

	if (!S_ISDIR(inode->i_mode))
		return;

	 
	watched = fsnotify_inode_watches_children(inode);

	spin_lock(&inode->i_lock);
	 
	hlist_for_each_entry(alias, &inode->i_dentry, d_u.d_alias) {
		struct dentry *child;

		 
		spin_lock(&alias->d_lock);
		list_for_each_entry(child, &alias->d_subdirs, d_child) {
			if (!child->d_inode)
				continue;

			spin_lock_nested(&child->d_lock, DENTRY_D_LOCK_NESTED);
			if (watched)
				child->d_flags |= DCACHE_FSNOTIFY_PARENT_WATCHED;
			else
				child->d_flags &= ~DCACHE_FSNOTIFY_PARENT_WATCHED;
			spin_unlock(&child->d_lock);
		}
		spin_unlock(&alias->d_lock);
	}
	spin_unlock(&inode->i_lock);
}

 
static bool fsnotify_event_needs_parent(struct inode *inode, struct mount *mnt,
					__u32 mask)
{
	__u32 marks_mask = 0;

	 
	if (mask & FS_ISDIR)
		return false;

	 
	BUILD_BUG_ON(FS_EVENTS_POSS_ON_CHILD & ~FS_EVENTS_POSS_TO_PARENT);

	 
	marks_mask |= fsnotify_parent_needed_mask(inode->i_fsnotify_mask);
	marks_mask |= fsnotify_parent_needed_mask(inode->i_sb->s_fsnotify_mask);
	if (mnt)
		marks_mask |= fsnotify_parent_needed_mask(mnt->mnt_fsnotify_mask);

	 
	return mask & marks_mask;
}

 
int __fsnotify_parent(struct dentry *dentry, __u32 mask, const void *data,
		      int data_type)
{
	const struct path *path = fsnotify_data_path(data, data_type);
	struct mount *mnt = path ? real_mount(path->mnt) : NULL;
	struct inode *inode = d_inode(dentry);
	struct dentry *parent;
	bool parent_watched = dentry->d_flags & DCACHE_FSNOTIFY_PARENT_WATCHED;
	bool parent_needed, parent_interested;
	__u32 p_mask;
	struct inode *p_inode = NULL;
	struct name_snapshot name;
	struct qstr *file_name = NULL;
	int ret = 0;

	 
	if (!inode->i_fsnotify_marks && !inode->i_sb->s_fsnotify_marks &&
	    (!mnt || !mnt->mnt_fsnotify_marks) && !parent_watched)
		return 0;

	parent = NULL;
	parent_needed = fsnotify_event_needs_parent(inode, mnt, mask);
	if (!parent_watched && !parent_needed)
		goto notify;

	 
	parent = dget_parent(dentry);
	p_inode = parent->d_inode;
	p_mask = fsnotify_inode_watches_children(p_inode);
	if (unlikely(parent_watched && !p_mask))
		__fsnotify_update_child_dentry_flags(p_inode);

	 
	parent_interested = mask & p_mask & ALL_FSNOTIFY_EVENTS;
	if (parent_needed || parent_interested) {
		 
		WARN_ON_ONCE(inode != fsnotify_data_inode(data, data_type));

		 
		take_dentry_name_snapshot(&name, dentry);
		file_name = &name.name;
		if (parent_interested)
			mask |= FS_EVENT_ON_CHILD;
	}

notify:
	ret = fsnotify(mask, data, data_type, p_inode, file_name, inode, 0);

	if (file_name)
		release_dentry_name_snapshot(&name);
	dput(parent);

	return ret;
}
EXPORT_SYMBOL_GPL(__fsnotify_parent);

static int fsnotify_handle_inode_event(struct fsnotify_group *group,
				       struct fsnotify_mark *inode_mark,
				       u32 mask, const void *data, int data_type,
				       struct inode *dir, const struct qstr *name,
				       u32 cookie)
{
	const struct path *path = fsnotify_data_path(data, data_type);
	struct inode *inode = fsnotify_data_inode(data, data_type);
	const struct fsnotify_ops *ops = group->ops;

	if (WARN_ON_ONCE(!ops->handle_inode_event))
		return 0;

	if (WARN_ON_ONCE(!inode && !dir))
		return 0;

	if ((inode_mark->flags & FSNOTIFY_MARK_FLAG_EXCL_UNLINK) &&
	    path && d_unlinked(path->dentry))
		return 0;

	 
	if (!(mask & inode_mark->mask & ALL_FSNOTIFY_EVENTS))
		return 0;

	return ops->handle_inode_event(inode_mark, mask, inode, dir, name, cookie);
}

static int fsnotify_handle_event(struct fsnotify_group *group, __u32 mask,
				 const void *data, int data_type,
				 struct inode *dir, const struct qstr *name,
				 u32 cookie, struct fsnotify_iter_info *iter_info)
{
	struct fsnotify_mark *inode_mark = fsnotify_iter_inode_mark(iter_info);
	struct fsnotify_mark *parent_mark = fsnotify_iter_parent_mark(iter_info);
	int ret;

	if (WARN_ON_ONCE(fsnotify_iter_sb_mark(iter_info)) ||
	    WARN_ON_ONCE(fsnotify_iter_vfsmount_mark(iter_info)))
		return 0;

	 
	if (mask & FS_RENAME) {
		struct dentry *moved = fsnotify_data_dentry(data, data_type);

		if (dir != moved->d_parent->d_inode)
			return 0;
	}

	if (parent_mark) {
		ret = fsnotify_handle_inode_event(group, parent_mark, mask,
						  data, data_type, dir, name, 0);
		if (ret)
			return ret;
	}

	if (!inode_mark)
		return 0;

	if (mask & FS_EVENT_ON_CHILD) {
		 
		mask &= ~FS_EVENT_ON_CHILD;
		dir = NULL;
		name = NULL;
	}

	return fsnotify_handle_inode_event(group, inode_mark, mask, data, data_type,
					   dir, name, cookie);
}

static int send_to_group(__u32 mask, const void *data, int data_type,
			 struct inode *dir, const struct qstr *file_name,
			 u32 cookie, struct fsnotify_iter_info *iter_info)
{
	struct fsnotify_group *group = NULL;
	__u32 test_mask = (mask & ALL_FSNOTIFY_EVENTS);
	__u32 marks_mask = 0;
	__u32 marks_ignore_mask = 0;
	bool is_dir = mask & FS_ISDIR;
	struct fsnotify_mark *mark;
	int type;

	if (!iter_info->report_mask)
		return 0;

	 
	if (mask & FS_MODIFY) {
		fsnotify_foreach_iter_mark_type(iter_info, mark, type) {
			if (!(mark->flags &
			      FSNOTIFY_MARK_FLAG_IGNORED_SURV_MODIFY))
				mark->ignore_mask = 0;
		}
	}

	 
	fsnotify_foreach_iter_mark_type(iter_info, mark, type) {
		group = mark->group;
		marks_mask |= mark->mask;
		marks_ignore_mask |=
			fsnotify_effective_ignore_mask(mark, is_dir, type);
	}

	pr_debug("%s: group=%p mask=%x marks_mask=%x marks_ignore_mask=%x data=%p data_type=%d dir=%p cookie=%d\n",
		 __func__, group, mask, marks_mask, marks_ignore_mask,
		 data, data_type, dir, cookie);

	if (!(test_mask & marks_mask & ~marks_ignore_mask))
		return 0;

	if (group->ops->handle_event) {
		return group->ops->handle_event(group, mask, data, data_type, dir,
						file_name, cookie, iter_info);
	}

	return fsnotify_handle_event(group, mask, data, data_type, dir,
				     file_name, cookie, iter_info);
}

static struct fsnotify_mark *fsnotify_first_mark(struct fsnotify_mark_connector **connp)
{
	struct fsnotify_mark_connector *conn;
	struct hlist_node *node = NULL;

	conn = srcu_dereference(*connp, &fsnotify_mark_srcu);
	if (conn)
		node = srcu_dereference(conn->list.first, &fsnotify_mark_srcu);

	return hlist_entry_safe(node, struct fsnotify_mark, obj_list);
}

static struct fsnotify_mark *fsnotify_next_mark(struct fsnotify_mark *mark)
{
	struct hlist_node *node = NULL;

	if (mark)
		node = srcu_dereference(mark->obj_list.next,
					&fsnotify_mark_srcu);

	return hlist_entry_safe(node, struct fsnotify_mark, obj_list);
}

 
static bool fsnotify_iter_select_report_types(
		struct fsnotify_iter_info *iter_info)
{
	struct fsnotify_group *max_prio_group = NULL;
	struct fsnotify_mark *mark;
	int type;

	 
	fsnotify_foreach_iter_type(type) {
		mark = iter_info->marks[type];
		if (mark &&
		    fsnotify_compare_groups(max_prio_group, mark->group) > 0)
			max_prio_group = mark->group;
	}

	if (!max_prio_group)
		return false;

	 
	iter_info->current_group = max_prio_group;
	iter_info->report_mask = 0;
	fsnotify_foreach_iter_type(type) {
		mark = iter_info->marks[type];
		if (mark && mark->group == iter_info->current_group) {
			 
			if (type == FSNOTIFY_ITER_TYPE_PARENT &&
			    !(mark->mask & FS_EVENT_ON_CHILD) &&
			    !(fsnotify_ignore_mask(mark) & FS_EVENT_ON_CHILD))
				continue;

			fsnotify_iter_set_report_type(iter_info, type);
		}
	}

	return true;
}

 
static void fsnotify_iter_next(struct fsnotify_iter_info *iter_info)
{
	struct fsnotify_mark *mark;
	int type;

	 
	fsnotify_foreach_iter_type(type) {
		mark = iter_info->marks[type];
		if (mark && mark->group == iter_info->current_group)
			iter_info->marks[type] =
				fsnotify_next_mark(iter_info->marks[type]);
	}
}

 
int fsnotify(__u32 mask, const void *data, int data_type, struct inode *dir,
	     const struct qstr *file_name, struct inode *inode, u32 cookie)
{
	const struct path *path = fsnotify_data_path(data, data_type);
	struct super_block *sb = fsnotify_data_sb(data, data_type);
	struct fsnotify_iter_info iter_info = {};
	struct mount *mnt = NULL;
	struct inode *inode2 = NULL;
	struct dentry *moved;
	int inode2_type;
	int ret = 0;
	__u32 test_mask, marks_mask;

	if (path)
		mnt = real_mount(path->mnt);

	if (!inode) {
		 
		inode = dir;
		 
		if (mask & FS_RENAME) {
			moved = fsnotify_data_dentry(data, data_type);
			inode2 = moved->d_parent->d_inode;
			inode2_type = FSNOTIFY_ITER_TYPE_INODE2;
		}
	} else if (mask & FS_EVENT_ON_CHILD) {
		 
		inode2 = dir;
		inode2_type = FSNOTIFY_ITER_TYPE_PARENT;
	}

	 
	if (!sb->s_fsnotify_marks &&
	    (!mnt || !mnt->mnt_fsnotify_marks) &&
	    (!inode || !inode->i_fsnotify_marks) &&
	    (!inode2 || !inode2->i_fsnotify_marks))
		return 0;

	marks_mask = sb->s_fsnotify_mask;
	if (mnt)
		marks_mask |= mnt->mnt_fsnotify_mask;
	if (inode)
		marks_mask |= inode->i_fsnotify_mask;
	if (inode2)
		marks_mask |= inode2->i_fsnotify_mask;


	 
	test_mask = (mask & ALL_FSNOTIFY_EVENTS);
	if (!(test_mask & marks_mask))
		return 0;

	iter_info.srcu_idx = srcu_read_lock(&fsnotify_mark_srcu);

	iter_info.marks[FSNOTIFY_ITER_TYPE_SB] =
		fsnotify_first_mark(&sb->s_fsnotify_marks);
	if (mnt) {
		iter_info.marks[FSNOTIFY_ITER_TYPE_VFSMOUNT] =
			fsnotify_first_mark(&mnt->mnt_fsnotify_marks);
	}
	if (inode) {
		iter_info.marks[FSNOTIFY_ITER_TYPE_INODE] =
			fsnotify_first_mark(&inode->i_fsnotify_marks);
	}
	if (inode2) {
		iter_info.marks[inode2_type] =
			fsnotify_first_mark(&inode2->i_fsnotify_marks);
	}

	 
	while (fsnotify_iter_select_report_types(&iter_info)) {
		ret = send_to_group(mask, data, data_type, dir, file_name,
				    cookie, &iter_info);

		if (ret && (mask & ALL_FSNOTIFY_PERM_EVENTS))
			goto out;

		fsnotify_iter_next(&iter_info);
	}
	ret = 0;
out:
	srcu_read_unlock(&fsnotify_mark_srcu, iter_info.srcu_idx);

	return ret;
}
EXPORT_SYMBOL_GPL(fsnotify);

static __init int fsnotify_init(void)
{
	int ret;

	BUILD_BUG_ON(HWEIGHT32(ALL_FSNOTIFY_BITS) != 23);

	ret = init_srcu_struct(&fsnotify_mark_srcu);
	if (ret)
		panic("initializing fsnotify_mark_srcu");

	fsnotify_mark_connector_cachep = KMEM_CACHE(fsnotify_mark_connector,
						    SLAB_PANIC);

	return 0;
}
core_initcall(fsnotify_init);
