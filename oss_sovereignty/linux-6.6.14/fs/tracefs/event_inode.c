
 
#include <linux/fsnotify.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/workqueue.h>
#include <linux/security.h>
#include <linux/tracefs.h>
#include <linux/kref.h>
#include <linux/delay.h>
#include "internal.h"

struct eventfs_inode {
	struct list_head	e_top_files;
};

 
struct eventfs_file {
	const char			*name;
	struct dentry			*d_parent;
	struct dentry			*dentry;
	struct list_head		list;
	struct eventfs_inode		*ei;
	const struct file_operations	*fop;
	const struct inode_operations	*iop;
	 
	union {
		struct llist_node	llist;
		struct rcu_head		rcu;
	};
	void				*data;
	unsigned int			is_freed:1;
	unsigned int			mode:31;
	kuid_t				uid;
	kgid_t				gid;
};

static DEFINE_MUTEX(eventfs_mutex);
DEFINE_STATIC_SRCU(eventfs_srcu);

 
enum {
	EVENTFS_SAVE_MODE	= BIT(16),
	EVENTFS_SAVE_UID	= BIT(17),
	EVENTFS_SAVE_GID	= BIT(18),
};

#define EVENTFS_MODE_MASK	(EVENTFS_SAVE_MODE - 1)

static struct dentry *eventfs_root_lookup(struct inode *dir,
					  struct dentry *dentry,
					  unsigned int flags);
static int dcache_dir_open_wrapper(struct inode *inode, struct file *file);
static int dcache_readdir_wrapper(struct file *file, struct dir_context *ctx);
static int eventfs_release(struct inode *inode, struct file *file);

static void update_attr(struct eventfs_file *ef, struct iattr *iattr)
{
	unsigned int ia_valid = iattr->ia_valid;

	if (ia_valid & ATTR_MODE) {
		ef->mode = (ef->mode & ~EVENTFS_MODE_MASK) |
			(iattr->ia_mode & EVENTFS_MODE_MASK) |
			EVENTFS_SAVE_MODE;
	}
	if (ia_valid & ATTR_UID) {
		ef->mode |= EVENTFS_SAVE_UID;
		ef->uid = iattr->ia_uid;
	}
	if (ia_valid & ATTR_GID) {
		ef->mode |= EVENTFS_SAVE_GID;
		ef->gid = iattr->ia_gid;
	}
}

static int eventfs_set_attr(struct mnt_idmap *idmap, struct dentry *dentry,
			     struct iattr *iattr)
{
	struct eventfs_file *ef;
	int ret;

	mutex_lock(&eventfs_mutex);
	ef = dentry->d_fsdata;
	if (ef && ef->is_freed) {
		 
		mutex_unlock(&eventfs_mutex);
		return -ENODEV;
	}

	ret = simple_setattr(idmap, dentry, iattr);
	if (!ret && ef)
		update_attr(ef, iattr);
	mutex_unlock(&eventfs_mutex);
	return ret;
}

static const struct inode_operations eventfs_root_dir_inode_operations = {
	.lookup		= eventfs_root_lookup,
	.setattr	= eventfs_set_attr,
};

static const struct inode_operations eventfs_file_inode_operations = {
	.setattr	= eventfs_set_attr,
};

static const struct file_operations eventfs_file_operations = {
	.open		= dcache_dir_open_wrapper,
	.read		= generic_read_dir,
	.iterate_shared	= dcache_readdir_wrapper,
	.llseek		= generic_file_llseek,
	.release	= eventfs_release,
};

static void update_inode_attr(struct inode *inode, struct eventfs_file *ef)
{
	inode->i_mode = ef->mode & EVENTFS_MODE_MASK;

	if (ef->mode & EVENTFS_SAVE_UID)
		inode->i_uid = ef->uid;

	if (ef->mode & EVENTFS_SAVE_GID)
		inode->i_gid = ef->gid;
}

 
static struct dentry *create_file(struct eventfs_file *ef,
				  struct dentry *parent, void *data,
				  const struct file_operations *fop)
{
	struct tracefs_inode *ti;
	struct dentry *dentry;
	struct inode *inode;

	if (!(ef->mode & S_IFMT))
		ef->mode |= S_IFREG;

	if (WARN_ON_ONCE(!S_ISREG(ef->mode)))
		return NULL;

	dentry = eventfs_start_creating(ef->name, parent);

	if (IS_ERR(dentry))
		return dentry;

	inode = tracefs_get_inode(dentry->d_sb);
	if (unlikely(!inode))
		return eventfs_failed_creating(dentry);

	 
	update_inode_attr(inode, ef);

	inode->i_op = &eventfs_file_inode_operations;
	inode->i_fop = fop;
	inode->i_private = data;

	ti = get_tracefs(inode);
	ti->flags |= TRACEFS_EVENT_INODE;
	d_instantiate(dentry, inode);
	fsnotify_create(dentry->d_parent->d_inode, dentry);
	return eventfs_end_creating(dentry);
};

 
static struct dentry *create_dir(struct eventfs_file *ef,
				 struct dentry *parent, void *data)
{
	struct tracefs_inode *ti;
	struct dentry *dentry;
	struct inode *inode;

	dentry = eventfs_start_creating(ef->name, parent);
	if (IS_ERR(dentry))
		return dentry;

	inode = tracefs_get_inode(dentry->d_sb);
	if (unlikely(!inode))
		return eventfs_failed_creating(dentry);

	update_inode_attr(inode, ef);

	inode->i_op = &eventfs_root_dir_inode_operations;
	inode->i_fop = &eventfs_file_operations;
	inode->i_private = data;

	ti = get_tracefs(inode);
	ti->flags |= TRACEFS_EVENT_INODE;

	inc_nlink(inode);
	d_instantiate(dentry, inode);
	inc_nlink(dentry->d_parent->d_inode);
	fsnotify_mkdir(dentry->d_parent->d_inode, dentry);
	return eventfs_end_creating(dentry);
}

static void free_ef(struct eventfs_file *ef)
{
	kfree(ef->name);
	kfree(ef->ei);
	kfree(ef);
}

 
void eventfs_set_ef_status_free(struct tracefs_inode *ti, struct dentry *dentry)
{
	struct eventfs_inode *ei;
	struct eventfs_file *ef;

	 
	if (unlikely(ti->flags & TRACEFS_EVENT_TOP_INODE)) {
		mutex_lock(&eventfs_mutex);
		ei = ti->private;

		 
		ti->private = NULL;
		mutex_unlock(&eventfs_mutex);

		ef = dentry->d_fsdata;
		if (ef)
			free_ef(ef);
		return;
	}

	mutex_lock(&eventfs_mutex);

	ef = dentry->d_fsdata;
	if (!ef)
		goto out;

	if (ef->is_freed) {
		free_ef(ef);
	} else {
		ef->dentry = NULL;
	}

	dentry->d_fsdata = NULL;
out:
	mutex_unlock(&eventfs_mutex);
}

 
static void eventfs_post_create_dir(struct eventfs_file *ef)
{
	struct eventfs_file *ef_child;
	struct tracefs_inode *ti;

	 
	 
	list_for_each_entry_srcu(ef_child, &ef->ei->e_top_files, list,
				 srcu_read_lock_held(&eventfs_srcu)) {
		ef_child->d_parent = ef->dentry;
	}

	ti = get_tracefs(ef->dentry->d_inode);
	ti->private = ef->ei;
}

 
static struct dentry *
create_dentry(struct eventfs_file *ef, struct dentry *parent, bool lookup)
{
	bool invalidate = false;
	struct dentry *dentry;

	mutex_lock(&eventfs_mutex);
	if (ef->is_freed) {
		mutex_unlock(&eventfs_mutex);
		return NULL;
	}
	if (ef->dentry) {
		dentry = ef->dentry;
		 
		if (!lookup)
			dget(dentry);
		mutex_unlock(&eventfs_mutex);
		return dentry;
	}
	mutex_unlock(&eventfs_mutex);

	if (!lookup)
		inode_lock(parent->d_inode);

	if (ef->ei)
		dentry = create_dir(ef, parent, ef->data);
	else
		dentry = create_file(ef, parent, ef->data, ef->fop);

	if (!lookup)
		inode_unlock(parent->d_inode);

	mutex_lock(&eventfs_mutex);
	if (IS_ERR_OR_NULL(dentry)) {
		 
		dentry = ef->dentry;
		if (dentry && !lookup)
			dget(dentry);
		mutex_unlock(&eventfs_mutex);
		return dentry;
	}

	if (!ef->dentry && !ef->is_freed) {
		ef->dentry = dentry;
		if (ef->ei)
			eventfs_post_create_dir(ef);
		dentry->d_fsdata = ef;
	} else {
		 
		invalidate = true;

		 
		WARN_ON_ONCE(!ef->is_freed);
	}
	mutex_unlock(&eventfs_mutex);
	if (invalidate)
		d_invalidate(dentry);

	if (lookup || invalidate)
		dput(dentry);

	return invalidate ? NULL : dentry;
}

static bool match_event_file(struct eventfs_file *ef, const char *name)
{
	bool ret;

	mutex_lock(&eventfs_mutex);
	ret = !ef->is_freed && strcmp(ef->name, name) == 0;
	mutex_unlock(&eventfs_mutex);

	return ret;
}

 
static struct dentry *eventfs_root_lookup(struct inode *dir,
					  struct dentry *dentry,
					  unsigned int flags)
{
	struct tracefs_inode *ti;
	struct eventfs_inode *ei;
	struct eventfs_file *ef;
	struct dentry *ret = NULL;
	int idx;

	ti = get_tracefs(dir);
	if (!(ti->flags & TRACEFS_EVENT_INODE))
		return NULL;

	ei = ti->private;
	idx = srcu_read_lock(&eventfs_srcu);
	list_for_each_entry_srcu(ef, &ei->e_top_files, list,
				 srcu_read_lock_held(&eventfs_srcu)) {
		if (!match_event_file(ef, dentry->d_name.name))
			continue;
		ret = simple_lookup(dir, dentry, flags);
		create_dentry(ef, ef->d_parent, true);
		break;
	}
	srcu_read_unlock(&eventfs_srcu, idx);
	return ret;
}

struct dentry_list {
	void			*cursor;
	struct dentry		**dentries;
};

 
static int eventfs_release(struct inode *inode, struct file *file)
{
	struct tracefs_inode *ti;
	struct dentry_list *dlist = file->private_data;
	void *cursor;
	int i;

	ti = get_tracefs(inode);
	if (!(ti->flags & TRACEFS_EVENT_INODE))
		return -EINVAL;

	if (WARN_ON_ONCE(!dlist))
		return -EINVAL;

	for (i = 0; dlist->dentries && dlist->dentries[i]; i++) {
		dput(dlist->dentries[i]);
	}

	cursor = dlist->cursor;
	kfree(dlist->dentries);
	kfree(dlist);
	file->private_data = cursor;
	return dcache_dir_close(inode, file);
}

 
static int dcache_dir_open_wrapper(struct inode *inode, struct file *file)
{
	struct tracefs_inode *ti;
	struct eventfs_inode *ei;
	struct eventfs_file *ef;
	struct dentry_list *dlist;
	struct dentry **dentries = NULL;
	struct dentry *dentry = file_dentry(file);
	struct dentry *d;
	struct inode *f_inode = file_inode(file);
	int cnt = 0;
	int idx;
	int ret;

	ti = get_tracefs(f_inode);
	if (!(ti->flags & TRACEFS_EVENT_INODE))
		return -EINVAL;

	if (WARN_ON_ONCE(file->private_data))
		return -EINVAL;

	dlist = kmalloc(sizeof(*dlist), GFP_KERNEL);
	if (!dlist)
		return -ENOMEM;

	ei = ti->private;
	idx = srcu_read_lock(&eventfs_srcu);
	list_for_each_entry_srcu(ef, &ei->e_top_files, list,
				 srcu_read_lock_held(&eventfs_srcu)) {
		d = create_dentry(ef, dentry, false);
		if (d) {
			struct dentry **tmp;


			tmp = krealloc(dentries, sizeof(d) * (cnt + 2), GFP_KERNEL);
			if (!tmp)
				break;
			tmp[cnt] = d;
			tmp[cnt + 1] = NULL;
			cnt++;
			dentries = tmp;
		}
	}
	srcu_read_unlock(&eventfs_srcu, idx);
	ret = dcache_dir_open(inode, file);

	 
	dlist->cursor = file->private_data;
	dlist->dentries = dentries;
	file->private_data = dlist;
	return ret;
}

 
static int dcache_readdir_wrapper(struct file *file, struct dir_context *ctx)
{
	struct dentry_list *dlist = file->private_data;
	int ret;

	file->private_data = dlist->cursor;
	ret = dcache_readdir(file, ctx);
	dlist->cursor = file->private_data;
	file->private_data = dlist;
	return ret;
}

 
static struct eventfs_file *eventfs_prepare_ef(const char *name, umode_t mode,
					const struct file_operations *fop,
					const struct inode_operations *iop,
					void *data)
{
	struct eventfs_file *ef;

	ef = kzalloc(sizeof(*ef), GFP_KERNEL);
	if (!ef)
		return ERR_PTR(-ENOMEM);

	ef->name = kstrdup(name, GFP_KERNEL);
	if (!ef->name) {
		kfree(ef);
		return ERR_PTR(-ENOMEM);
	}

	if (S_ISDIR(mode)) {
		ef->ei = kzalloc(sizeof(*ef->ei), GFP_KERNEL);
		if (!ef->ei) {
			kfree(ef->name);
			kfree(ef);
			return ERR_PTR(-ENOMEM);
		}
		INIT_LIST_HEAD(&ef->ei->e_top_files);
		ef->mode = S_IFDIR | S_IRWXU | S_IRUGO | S_IXUGO;
	} else {
		ef->ei = NULL;
		ef->mode = mode;
	}

	ef->iop = iop;
	ef->fop = fop;
	ef->data = data;
	return ef;
}

 
struct dentry *eventfs_create_events_dir(const char *name,
					 struct dentry *parent)
{
	struct dentry *dentry = tracefs_start_creating(name, parent);
	struct eventfs_inode *ei;
	struct tracefs_inode *ti;
	struct inode *inode;

	if (security_locked_down(LOCKDOWN_TRACEFS))
		return NULL;

	if (IS_ERR(dentry))
		return dentry;

	ei = kzalloc(sizeof(*ei), GFP_KERNEL);
	if (!ei)
		return ERR_PTR(-ENOMEM);
	inode = tracefs_get_inode(dentry->d_sb);
	if (unlikely(!inode)) {
		kfree(ei);
		tracefs_failed_creating(dentry);
		return ERR_PTR(-ENOMEM);
	}

	INIT_LIST_HEAD(&ei->e_top_files);

	ti = get_tracefs(inode);
	ti->flags |= TRACEFS_EVENT_INODE | TRACEFS_EVENT_TOP_INODE;
	ti->private = ei;

	inode->i_mode = S_IFDIR | S_IRWXU | S_IRUGO | S_IXUGO;
	inode->i_op = &eventfs_root_dir_inode_operations;
	inode->i_fop = &eventfs_file_operations;

	 
	inc_nlink(inode);
	d_instantiate(dentry, inode);
	inc_nlink(dentry->d_parent->d_inode);
	fsnotify_mkdir(dentry->d_parent->d_inode, dentry);
	return tracefs_end_creating(dentry);
}

 
struct eventfs_file *eventfs_add_subsystem_dir(const char *name,
					       struct dentry *parent)
{
	struct tracefs_inode *ti_parent;
	struct eventfs_inode *ei_parent;
	struct eventfs_file *ef;

	if (security_locked_down(LOCKDOWN_TRACEFS))
		return NULL;

	if (!parent)
		return ERR_PTR(-EINVAL);

	ti_parent = get_tracefs(parent->d_inode);
	ei_parent = ti_parent->private;

	ef = eventfs_prepare_ef(name, S_IFDIR, NULL, NULL, NULL);
	if (IS_ERR(ef))
		return ef;

	mutex_lock(&eventfs_mutex);
	list_add_tail(&ef->list, &ei_parent->e_top_files);
	ef->d_parent = parent;
	mutex_unlock(&eventfs_mutex);
	return ef;
}

 
struct eventfs_file *eventfs_add_dir(const char *name,
				     struct eventfs_file *ef_parent)
{
	struct eventfs_file *ef;

	if (security_locked_down(LOCKDOWN_TRACEFS))
		return NULL;

	if (!ef_parent)
		return ERR_PTR(-EINVAL);

	ef = eventfs_prepare_ef(name, S_IFDIR, NULL, NULL, NULL);
	if (IS_ERR(ef))
		return ef;

	mutex_lock(&eventfs_mutex);
	list_add_tail(&ef->list, &ef_parent->ei->e_top_files);
	ef->d_parent = ef_parent->dentry;
	mutex_unlock(&eventfs_mutex);
	return ef;
}

 
int eventfs_add_events_file(const char *name, umode_t mode,
			 struct dentry *parent, void *data,
			 const struct file_operations *fop)
{
	struct tracefs_inode *ti;
	struct eventfs_inode *ei;
	struct eventfs_file *ef;

	if (security_locked_down(LOCKDOWN_TRACEFS))
		return -ENODEV;

	if (!parent)
		return -EINVAL;

	if (!(mode & S_IFMT))
		mode |= S_IFREG;

	if (!parent->d_inode)
		return -EINVAL;

	ti = get_tracefs(parent->d_inode);
	if (!(ti->flags & TRACEFS_EVENT_INODE))
		return -EINVAL;

	ei = ti->private;
	ef = eventfs_prepare_ef(name, mode, fop, NULL, data);

	if (IS_ERR(ef))
		return -ENOMEM;

	mutex_lock(&eventfs_mutex);
	list_add_tail(&ef->list, &ei->e_top_files);
	ef->d_parent = parent;
	mutex_unlock(&eventfs_mutex);
	return 0;
}

 
int eventfs_add_file(const char *name, umode_t mode,
		     struct eventfs_file *ef_parent,
		     void *data,
		     const struct file_operations *fop)
{
	struct eventfs_file *ef;

	if (security_locked_down(LOCKDOWN_TRACEFS))
		return -ENODEV;

	if (!ef_parent)
		return -EINVAL;

	if (!(mode & S_IFMT))
		mode |= S_IFREG;

	ef = eventfs_prepare_ef(name, mode, fop, NULL, data);
	if (IS_ERR(ef))
		return -ENOMEM;

	mutex_lock(&eventfs_mutex);
	list_add_tail(&ef->list, &ef_parent->ei->e_top_files);
	ef->d_parent = ef_parent->dentry;
	mutex_unlock(&eventfs_mutex);
	return 0;
}

static LLIST_HEAD(free_list);

static void eventfs_workfn(struct work_struct *work)
{
        struct eventfs_file *ef, *tmp;
        struct llist_node *llnode;

	llnode = llist_del_all(&free_list);
        llist_for_each_entry_safe(ef, tmp, llnode, llist) {
		 
		if (!WARN_ON_ONCE(!ef->dentry))
			dput(ef->dentry);
        }
}

static DECLARE_WORK(eventfs_work, eventfs_workfn);

static void free_rcu_ef(struct rcu_head *head)
{
	struct eventfs_file *ef = container_of(head, struct eventfs_file, rcu);

	if (ef->dentry) {
		 
		if (llist_add(&ef->llist, &free_list))
			queue_work(system_unbound_wq, &eventfs_work);
		return;
	}

	free_ef(ef);
}

static void unhook_dentry(struct dentry *dentry)
{
	if (!dentry)
		return;
	 
	dget(dentry);

	 
	dget(dentry);
}

 
static void eventfs_remove_rec(struct eventfs_file *ef, int level)
{
	struct eventfs_file *ef_child;

	if (!ef)
		return;
	 
	if (WARN_ON_ONCE(level > 3))
		return;

	if (ef->ei) {
		 
		list_for_each_entry_srcu(ef_child, &ef->ei->e_top_files, list,
					 lockdep_is_held(&eventfs_mutex)) {
			eventfs_remove_rec(ef_child, level + 1);
		}
	}

	ef->is_freed = 1;

	unhook_dentry(ef->dentry);

	list_del_rcu(&ef->list);
	call_srcu(&eventfs_srcu, &ef->rcu, free_rcu_ef);
}

 
void eventfs_remove(struct eventfs_file *ef)
{
	struct dentry *dentry;

	if (!ef)
		return;

	mutex_lock(&eventfs_mutex);
	dentry = ef->dentry;
	eventfs_remove_rec(ef, 0);
	mutex_unlock(&eventfs_mutex);

	 
	if (dentry)
		simple_recursive_removal(dentry, NULL);
}

 
void eventfs_remove_events_dir(struct dentry *dentry)
{
	struct eventfs_file *ef_child;
	struct eventfs_inode *ei;
	struct tracefs_inode *ti;

	if (!dentry || !dentry->d_inode)
		return;

	ti = get_tracefs(dentry->d_inode);
	if (!ti || !(ti->flags & TRACEFS_EVENT_INODE))
		return;

	mutex_lock(&eventfs_mutex);
	ei = ti->private;
	list_for_each_entry_srcu(ef_child, &ei->e_top_files, list,
				 lockdep_is_held(&eventfs_mutex)) {
		eventfs_remove_rec(ef_child, 0);
	}
	mutex_unlock(&eventfs_mutex);
}
