
 

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/mm.h>

#include "sysfs.h"

 
static const struct sysfs_ops *sysfs_file_ops(struct kernfs_node *kn)
{
	struct kobject *kobj = kn->parent->priv;

	if (kn->flags & KERNFS_LOCKDEP)
		lockdep_assert_held(kn);
	return kobj->ktype ? kobj->ktype->sysfs_ops : NULL;
}

 
static int sysfs_kf_seq_show(struct seq_file *sf, void *v)
{
	struct kernfs_open_file *of = sf->private;
	struct kobject *kobj = of->kn->parent->priv;
	const struct sysfs_ops *ops = sysfs_file_ops(of->kn);
	ssize_t count;
	char *buf;

	if (WARN_ON_ONCE(!ops->show))
		return -EINVAL;

	 
	count = seq_get_buf(sf, &buf);
	if (count < PAGE_SIZE) {
		seq_commit(sf, -1);
		return 0;
	}
	memset(buf, 0, PAGE_SIZE);

	count = ops->show(kobj, of->kn->priv, buf);
	if (count < 0)
		return count;

	 
	if (count >= (ssize_t)PAGE_SIZE) {
		printk("fill_read_buffer: %pS returned bad count\n",
				ops->show);
		 
		count = PAGE_SIZE - 1;
	}
	seq_commit(sf, count);
	return 0;
}

static ssize_t sysfs_kf_bin_read(struct kernfs_open_file *of, char *buf,
				 size_t count, loff_t pos)
{
	struct bin_attribute *battr = of->kn->priv;
	struct kobject *kobj = of->kn->parent->priv;
	loff_t size = file_inode(of->file)->i_size;

	if (!count)
		return 0;

	if (size) {
		if (pos >= size)
			return 0;
		if (pos + count > size)
			count = size - pos;
	}

	if (!battr->read)
		return -EIO;

	return battr->read(of->file, kobj, battr, buf, pos, count);
}

 
static ssize_t sysfs_kf_read(struct kernfs_open_file *of, char *buf,
			     size_t count, loff_t pos)
{
	const struct sysfs_ops *ops = sysfs_file_ops(of->kn);
	struct kobject *kobj = of->kn->parent->priv;
	ssize_t len;

	 
	if (WARN_ON_ONCE(buf != of->prealloc_buf))
		return 0;
	len = ops->show(kobj, of->kn->priv, buf);
	if (len < 0)
		return len;
	if (pos) {
		if (len <= pos)
			return 0;
		len -= pos;
		memmove(buf, buf + pos, len);
	}
	return min_t(ssize_t, count, len);
}

 
static ssize_t sysfs_kf_write(struct kernfs_open_file *of, char *buf,
			      size_t count, loff_t pos)
{
	const struct sysfs_ops *ops = sysfs_file_ops(of->kn);
	struct kobject *kobj = of->kn->parent->priv;

	if (!count)
		return 0;

	return ops->store(kobj, of->kn->priv, buf, count);
}

 
static ssize_t sysfs_kf_bin_write(struct kernfs_open_file *of, char *buf,
				  size_t count, loff_t pos)
{
	struct bin_attribute *battr = of->kn->priv;
	struct kobject *kobj = of->kn->parent->priv;
	loff_t size = file_inode(of->file)->i_size;

	if (size) {
		if (size <= pos)
			return -EFBIG;
		count = min_t(ssize_t, count, size - pos);
	}
	if (!count)
		return 0;

	if (!battr->write)
		return -EIO;

	return battr->write(of->file, kobj, battr, buf, pos, count);
}

static int sysfs_kf_bin_mmap(struct kernfs_open_file *of,
			     struct vm_area_struct *vma)
{
	struct bin_attribute *battr = of->kn->priv;
	struct kobject *kobj = of->kn->parent->priv;

	return battr->mmap(of->file, kobj, battr, vma);
}

static int sysfs_kf_bin_open(struct kernfs_open_file *of)
{
	struct bin_attribute *battr = of->kn->priv;

	if (battr->f_mapping)
		of->file->f_mapping = battr->f_mapping();

	return 0;
}

void sysfs_notify(struct kobject *kobj, const char *dir, const char *attr)
{
	struct kernfs_node *kn = kobj->sd, *tmp;

	if (kn && dir)
		kn = kernfs_find_and_get(kn, dir);
	else
		kernfs_get(kn);

	if (kn && attr) {
		tmp = kernfs_find_and_get(kn, attr);
		kernfs_put(kn);
		kn = tmp;
	}

	if (kn) {
		kernfs_notify(kn);
		kernfs_put(kn);
	}
}
EXPORT_SYMBOL_GPL(sysfs_notify);

static const struct kernfs_ops sysfs_file_kfops_empty = {
};

static const struct kernfs_ops sysfs_file_kfops_ro = {
	.seq_show	= sysfs_kf_seq_show,
};

static const struct kernfs_ops sysfs_file_kfops_wo = {
	.write		= sysfs_kf_write,
};

static const struct kernfs_ops sysfs_file_kfops_rw = {
	.seq_show	= sysfs_kf_seq_show,
	.write		= sysfs_kf_write,
};

static const struct kernfs_ops sysfs_prealloc_kfops_ro = {
	.read		= sysfs_kf_read,
	.prealloc	= true,
};

static const struct kernfs_ops sysfs_prealloc_kfops_wo = {
	.write		= sysfs_kf_write,
	.prealloc	= true,
};

static const struct kernfs_ops sysfs_prealloc_kfops_rw = {
	.read		= sysfs_kf_read,
	.write		= sysfs_kf_write,
	.prealloc	= true,
};

static const struct kernfs_ops sysfs_bin_kfops_ro = {
	.read		= sysfs_kf_bin_read,
};

static const struct kernfs_ops sysfs_bin_kfops_wo = {
	.write		= sysfs_kf_bin_write,
};

static const struct kernfs_ops sysfs_bin_kfops_rw = {
	.read		= sysfs_kf_bin_read,
	.write		= sysfs_kf_bin_write,
};

static const struct kernfs_ops sysfs_bin_kfops_mmap = {
	.read		= sysfs_kf_bin_read,
	.write		= sysfs_kf_bin_write,
	.mmap		= sysfs_kf_bin_mmap,
	.open		= sysfs_kf_bin_open,
};

int sysfs_add_file_mode_ns(struct kernfs_node *parent,
		const struct attribute *attr, umode_t mode, kuid_t uid,
		kgid_t gid, const void *ns)
{
	struct kobject *kobj = parent->priv;
	const struct sysfs_ops *sysfs_ops = kobj->ktype->sysfs_ops;
	struct lock_class_key *key = NULL;
	const struct kernfs_ops *ops = NULL;
	struct kernfs_node *kn;

	 
	if (WARN(!sysfs_ops, KERN_ERR
			"missing sysfs attribute operations for kobject: %s\n",
			kobject_name(kobj)))
		return -EINVAL;

	if (mode & SYSFS_PREALLOC) {
		if (sysfs_ops->show && sysfs_ops->store)
			ops = &sysfs_prealloc_kfops_rw;
		else if (sysfs_ops->show)
			ops = &sysfs_prealloc_kfops_ro;
		else if (sysfs_ops->store)
			ops = &sysfs_prealloc_kfops_wo;
	} else {
		if (sysfs_ops->show && sysfs_ops->store)
			ops = &sysfs_file_kfops_rw;
		else if (sysfs_ops->show)
			ops = &sysfs_file_kfops_ro;
		else if (sysfs_ops->store)
			ops = &sysfs_file_kfops_wo;
	}

	if (!ops)
		ops = &sysfs_file_kfops_empty;

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	if (!attr->ignore_lockdep)
		key = attr->key ?: (struct lock_class_key *)&attr->skey;
#endif

	kn = __kernfs_create_file(parent, attr->name, mode & 0777, uid, gid,
				  PAGE_SIZE, ops, (void *)attr, ns, key);
	if (IS_ERR(kn)) {
		if (PTR_ERR(kn) == -EEXIST)
			sysfs_warn_dup(parent, attr->name);
		return PTR_ERR(kn);
	}
	return 0;
}

int sysfs_add_bin_file_mode_ns(struct kernfs_node *parent,
		const struct bin_attribute *battr, umode_t mode,
		kuid_t uid, kgid_t gid, const void *ns)
{
	const struct attribute *attr = &battr->attr;
	struct lock_class_key *key = NULL;
	const struct kernfs_ops *ops;
	struct kernfs_node *kn;

	if (battr->mmap)
		ops = &sysfs_bin_kfops_mmap;
	else if (battr->read && battr->write)
		ops = &sysfs_bin_kfops_rw;
	else if (battr->read)
		ops = &sysfs_bin_kfops_ro;
	else if (battr->write)
		ops = &sysfs_bin_kfops_wo;
	else
		ops = &sysfs_file_kfops_empty;

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	if (!attr->ignore_lockdep)
		key = attr->key ?: (struct lock_class_key *)&attr->skey;
#endif

	kn = __kernfs_create_file(parent, attr->name, mode & 0777, uid, gid,
				  battr->size, ops, (void *)attr, ns, key);
	if (IS_ERR(kn)) {
		if (PTR_ERR(kn) == -EEXIST)
			sysfs_warn_dup(parent, attr->name);
		return PTR_ERR(kn);
	}
	return 0;
}

 
int sysfs_create_file_ns(struct kobject *kobj, const struct attribute *attr,
			 const void *ns)
{
	kuid_t uid;
	kgid_t gid;

	if (WARN_ON(!kobj || !kobj->sd || !attr))
		return -EINVAL;

	kobject_get_ownership(kobj, &uid, &gid);
	return sysfs_add_file_mode_ns(kobj->sd, attr, attr->mode, uid, gid, ns);
}
EXPORT_SYMBOL_GPL(sysfs_create_file_ns);

int sysfs_create_files(struct kobject *kobj, const struct attribute * const *ptr)
{
	int err = 0;
	int i;

	for (i = 0; ptr[i] && !err; i++)
		err = sysfs_create_file(kobj, ptr[i]);
	if (err)
		while (--i >= 0)
			sysfs_remove_file(kobj, ptr[i]);
	return err;
}
EXPORT_SYMBOL_GPL(sysfs_create_files);

 
int sysfs_add_file_to_group(struct kobject *kobj,
		const struct attribute *attr, const char *group)
{
	struct kernfs_node *parent;
	kuid_t uid;
	kgid_t gid;
	int error;

	if (group) {
		parent = kernfs_find_and_get(kobj->sd, group);
	} else {
		parent = kobj->sd;
		kernfs_get(parent);
	}

	if (!parent)
		return -ENOENT;

	kobject_get_ownership(kobj, &uid, &gid);
	error = sysfs_add_file_mode_ns(parent, attr, attr->mode, uid, gid,
				       NULL);
	kernfs_put(parent);

	return error;
}
EXPORT_SYMBOL_GPL(sysfs_add_file_to_group);

 
int sysfs_chmod_file(struct kobject *kobj, const struct attribute *attr,
		     umode_t mode)
{
	struct kernfs_node *kn;
	struct iattr newattrs;
	int rc;

	kn = kernfs_find_and_get(kobj->sd, attr->name);
	if (!kn)
		return -ENOENT;

	newattrs.ia_mode = (mode & S_IALLUGO) | (kn->mode & ~S_IALLUGO);
	newattrs.ia_valid = ATTR_MODE;

	rc = kernfs_setattr(kn, &newattrs);

	kernfs_put(kn);
	return rc;
}
EXPORT_SYMBOL_GPL(sysfs_chmod_file);

 
struct kernfs_node *sysfs_break_active_protection(struct kobject *kobj,
						  const struct attribute *attr)
{
	struct kernfs_node *kn;

	kobject_get(kobj);
	kn = kernfs_find_and_get(kobj->sd, attr->name);
	if (kn)
		kernfs_break_active_protection(kn);
	return kn;
}
EXPORT_SYMBOL_GPL(sysfs_break_active_protection);

 
void sysfs_unbreak_active_protection(struct kernfs_node *kn)
{
	struct kobject *kobj = kn->parent->priv;

	kernfs_unbreak_active_protection(kn);
	kernfs_put(kn);
	kobject_put(kobj);
}
EXPORT_SYMBOL_GPL(sysfs_unbreak_active_protection);

 
void sysfs_remove_file_ns(struct kobject *kobj, const struct attribute *attr,
			  const void *ns)
{
	struct kernfs_node *parent = kobj->sd;

	kernfs_remove_by_name_ns(parent, attr->name, ns);
}
EXPORT_SYMBOL_GPL(sysfs_remove_file_ns);

 
bool sysfs_remove_file_self(struct kobject *kobj, const struct attribute *attr)
{
	struct kernfs_node *parent = kobj->sd;
	struct kernfs_node *kn;
	bool ret;

	kn = kernfs_find_and_get(parent, attr->name);
	if (WARN_ON_ONCE(!kn))
		return false;

	ret = kernfs_remove_self(kn);

	kernfs_put(kn);
	return ret;
}
EXPORT_SYMBOL_GPL(sysfs_remove_file_self);

void sysfs_remove_files(struct kobject *kobj, const struct attribute * const *ptr)
{
	int i;

	for (i = 0; ptr[i]; i++)
		sysfs_remove_file(kobj, ptr[i]);
}
EXPORT_SYMBOL_GPL(sysfs_remove_files);

 
void sysfs_remove_file_from_group(struct kobject *kobj,
		const struct attribute *attr, const char *group)
{
	struct kernfs_node *parent;

	if (group) {
		parent = kernfs_find_and_get(kobj->sd, group);
	} else {
		parent = kobj->sd;
		kernfs_get(parent);
	}

	if (parent) {
		kernfs_remove_by_name(parent, attr->name);
		kernfs_put(parent);
	}
}
EXPORT_SYMBOL_GPL(sysfs_remove_file_from_group);

 
int sysfs_create_bin_file(struct kobject *kobj,
			  const struct bin_attribute *attr)
{
	kuid_t uid;
	kgid_t gid;

	if (WARN_ON(!kobj || !kobj->sd || !attr))
		return -EINVAL;

	kobject_get_ownership(kobj, &uid, &gid);
	return sysfs_add_bin_file_mode_ns(kobj->sd, attr, attr->attr.mode, uid,
					   gid, NULL);
}
EXPORT_SYMBOL_GPL(sysfs_create_bin_file);

 
void sysfs_remove_bin_file(struct kobject *kobj,
			   const struct bin_attribute *attr)
{
	kernfs_remove_by_name(kobj->sd, attr->attr.name);
}
EXPORT_SYMBOL_GPL(sysfs_remove_bin_file);

static int internal_change_owner(struct kernfs_node *kn, kuid_t kuid,
				 kgid_t kgid)
{
	struct iattr newattrs = {
		.ia_valid = ATTR_UID | ATTR_GID,
		.ia_uid = kuid,
		.ia_gid = kgid,
	};
	return kernfs_setattr(kn, &newattrs);
}

 
int sysfs_link_change_owner(struct kobject *kobj, struct kobject *targ,
			    const char *name, kuid_t kuid, kgid_t kgid)
{
	struct kernfs_node *kn = NULL;
	int error;

	if (!name || !kobj->state_in_sysfs || !targ->state_in_sysfs)
		return -EINVAL;

	error = -ENOENT;
	kn = kernfs_find_and_get_ns(kobj->sd, name, targ->sd->ns);
	if (!kn)
		goto out;

	error = -EINVAL;
	if (kernfs_type(kn) != KERNFS_LINK)
		goto out;
	if (kn->symlink.target_kn->priv != targ)
		goto out;

	error = internal_change_owner(kn, kuid, kgid);

out:
	kernfs_put(kn);
	return error;
}

 
int sysfs_file_change_owner(struct kobject *kobj, const char *name, kuid_t kuid,
			    kgid_t kgid)
{
	struct kernfs_node *kn;
	int error;

	if (!name)
		return -EINVAL;

	if (!kobj->state_in_sysfs)
		return -EINVAL;

	kn = kernfs_find_and_get(kobj->sd, name);
	if (!kn)
		return -ENOENT;

	error = internal_change_owner(kn, kuid, kgid);

	kernfs_put(kn);

	return error;
}
EXPORT_SYMBOL_GPL(sysfs_file_change_owner);

 
int sysfs_change_owner(struct kobject *kobj, kuid_t kuid, kgid_t kgid)
{
	int error;
	const struct kobj_type *ktype;

	if (!kobj->state_in_sysfs)
		return -EINVAL;

	 
	error = internal_change_owner(kobj->sd, kuid, kgid);
	if (error)
		return error;

	ktype = get_ktype(kobj);
	if (ktype) {
		 
		error = sysfs_groups_change_owner(kobj, ktype->default_groups,
						  kuid, kgid);
		if (error)
			return error;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sysfs_change_owner);

 
int sysfs_emit(char *buf, const char *fmt, ...)
{
	va_list args;
	int len;

	if (WARN(!buf || offset_in_page(buf),
		 "invalid sysfs_emit: buf:%p\n", buf))
		return 0;

	va_start(args, fmt);
	len = vscnprintf(buf, PAGE_SIZE, fmt, args);
	va_end(args);

	return len;
}
EXPORT_SYMBOL_GPL(sysfs_emit);

 
int sysfs_emit_at(char *buf, int at, const char *fmt, ...)
{
	va_list args;
	int len;

	if (WARN(!buf || offset_in_page(buf) || at < 0 || at >= PAGE_SIZE,
		 "invalid sysfs_emit_at: buf:%p at:%d\n", buf, at))
		return 0;

	va_start(args, fmt);
	len = vscnprintf(buf + at, PAGE_SIZE - at, fmt, args);
	va_end(args);

	return len;
}
EXPORT_SYMBOL_GPL(sysfs_emit_at);
