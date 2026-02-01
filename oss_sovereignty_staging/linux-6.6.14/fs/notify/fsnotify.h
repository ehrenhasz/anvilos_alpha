 
#ifndef __FS_NOTIFY_FSNOTIFY_H_
#define __FS_NOTIFY_FSNOTIFY_H_

#include <linux/list.h>
#include <linux/fsnotify.h>
#include <linux/srcu.h>
#include <linux/types.h>

#include "../mount.h"

static inline struct inode *fsnotify_conn_inode(
				struct fsnotify_mark_connector *conn)
{
	return container_of(conn->obj, struct inode, i_fsnotify_marks);
}

static inline struct mount *fsnotify_conn_mount(
				struct fsnotify_mark_connector *conn)
{
	return container_of(conn->obj, struct mount, mnt_fsnotify_marks);
}

static inline struct super_block *fsnotify_conn_sb(
				struct fsnotify_mark_connector *conn)
{
	return container_of(conn->obj, struct super_block, s_fsnotify_marks);
}

static inline struct super_block *fsnotify_connector_sb(
				struct fsnotify_mark_connector *conn)
{
	switch (conn->type) {
	case FSNOTIFY_OBJ_TYPE_INODE:
		return fsnotify_conn_inode(conn)->i_sb;
	case FSNOTIFY_OBJ_TYPE_VFSMOUNT:
		return fsnotify_conn_mount(conn)->mnt.mnt_sb;
	case FSNOTIFY_OBJ_TYPE_SB:
		return fsnotify_conn_sb(conn);
	default:
		return NULL;
	}
}

 
extern void fsnotify_flush_notify(struct fsnotify_group *group);

 
extern struct srcu_struct fsnotify_mark_srcu;

 
extern int fsnotify_compare_groups(struct fsnotify_group *a,
				   struct fsnotify_group *b);

 
extern void fsnotify_destroy_marks(fsnotify_connp_t *connp);
 
static inline void fsnotify_clear_marks_by_inode(struct inode *inode)
{
	fsnotify_destroy_marks(&inode->i_fsnotify_marks);
}
 
static inline void fsnotify_clear_marks_by_mount(struct vfsmount *mnt)
{
	fsnotify_destroy_marks(&real_mount(mnt)->mnt_fsnotify_marks);
}
 
static inline void fsnotify_clear_marks_by_sb(struct super_block *sb)
{
	fsnotify_destroy_marks(&sb->s_fsnotify_marks);
}

 
extern void __fsnotify_update_child_dentry_flags(struct inode *inode);

extern struct kmem_cache *fsnotify_mark_connector_cachep;

#endif	 
