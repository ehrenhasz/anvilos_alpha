
 

#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/err.h>
#include <linux/fs.h>
#include "sysfs.h"


static void remove_files(struct kernfs_node *parent,
			 const struct attribute_group *grp)
{
	struct attribute *const *attr;
	struct bin_attribute *const *bin_attr;

	if (grp->attrs)
		for (attr = grp->attrs; *attr; attr++)
			kernfs_remove_by_name(parent, (*attr)->name);
	if (grp->bin_attrs)
		for (bin_attr = grp->bin_attrs; *bin_attr; bin_attr++)
			kernfs_remove_by_name(parent, (*bin_attr)->attr.name);
}

static int create_files(struct kernfs_node *parent, struct kobject *kobj,
			kuid_t uid, kgid_t gid,
			const struct attribute_group *grp, int update)
{
	struct attribute *const *attr;
	struct bin_attribute *const *bin_attr;
	int error = 0, i;

	if (grp->attrs) {
		for (i = 0, attr = grp->attrs; *attr && !error; i++, attr++) {
			umode_t mode = (*attr)->mode;

			 
			if (update)
				kernfs_remove_by_name(parent, (*attr)->name);
			if (grp->is_visible) {
				mode = grp->is_visible(kobj, *attr, i);
				if (!mode)
					continue;
			}

			WARN(mode & ~(SYSFS_PREALLOC | 0664),
			     "Attribute %s: Invalid permissions 0%o\n",
			     (*attr)->name, mode);

			mode &= SYSFS_PREALLOC | 0664;
			error = sysfs_add_file_mode_ns(parent, *attr, mode, uid,
						       gid, NULL);
			if (unlikely(error))
				break;
		}
		if (error) {
			remove_files(parent, grp);
			goto exit;
		}
	}

	if (grp->bin_attrs) {
		for (i = 0, bin_attr = grp->bin_attrs; *bin_attr; i++, bin_attr++) {
			umode_t mode = (*bin_attr)->attr.mode;

			if (update)
				kernfs_remove_by_name(parent,
						(*bin_attr)->attr.name);
			if (grp->is_bin_visible) {
				mode = grp->is_bin_visible(kobj, *bin_attr, i);
				if (!mode)
					continue;
			}

			WARN(mode & ~(SYSFS_PREALLOC | 0664),
			     "Attribute %s: Invalid permissions 0%o\n",
			     (*bin_attr)->attr.name, mode);

			mode &= SYSFS_PREALLOC | 0664;
			error = sysfs_add_bin_file_mode_ns(parent, *bin_attr,
							   mode, uid, gid,
							   NULL);
			if (error)
				break;
		}
		if (error)
			remove_files(parent, grp);
	}
exit:
	return error;
}


static int internal_create_group(struct kobject *kobj, int update,
				 const struct attribute_group *grp)
{
	struct kernfs_node *kn;
	kuid_t uid;
	kgid_t gid;
	int error;

	if (WARN_ON(!kobj || (!update && !kobj->sd)))
		return -EINVAL;

	 
	if (unlikely(update && !kobj->sd))
		return -EINVAL;

	if (!grp->attrs && !grp->bin_attrs) {
		pr_debug("sysfs: (bin_)attrs not set by subsystem for group: %s/%s, skipping\n",
			 kobj->name, grp->name ?: "");
		return 0;
	}

	kobject_get_ownership(kobj, &uid, &gid);
	if (grp->name) {
		if (update) {
			kn = kernfs_find_and_get(kobj->sd, grp->name);
			if (!kn) {
				pr_warn("Can't update unknown attr grp name: %s/%s\n",
					kobj->name, grp->name);
				return -EINVAL;
			}
		} else {
			kn = kernfs_create_dir_ns(kobj->sd, grp->name,
						  S_IRWXU | S_IRUGO | S_IXUGO,
						  uid, gid, kobj, NULL);
			if (IS_ERR(kn)) {
				if (PTR_ERR(kn) == -EEXIST)
					sysfs_warn_dup(kobj->sd, grp->name);
				return PTR_ERR(kn);
			}
		}
	} else {
		kn = kobj->sd;
	}

	kernfs_get(kn);
	error = create_files(kn, kobj, uid, gid, grp, update);
	if (error) {
		if (grp->name)
			kernfs_remove(kn);
	}
	kernfs_put(kn);

	if (grp->name && update)
		kernfs_put(kn);

	return error;
}

 
int sysfs_create_group(struct kobject *kobj,
		       const struct attribute_group *grp)
{
	return internal_create_group(kobj, 0, grp);
}
EXPORT_SYMBOL_GPL(sysfs_create_group);

static int internal_create_groups(struct kobject *kobj, int update,
				  const struct attribute_group **groups)
{
	int error = 0;
	int i;

	if (!groups)
		return 0;

	for (i = 0; groups[i]; i++) {
		error = internal_create_group(kobj, update, groups[i]);
		if (error) {
			while (--i >= 0)
				sysfs_remove_group(kobj, groups[i]);
			break;
		}
	}
	return error;
}

 
int sysfs_create_groups(struct kobject *kobj,
			const struct attribute_group **groups)
{
	return internal_create_groups(kobj, 0, groups);
}
EXPORT_SYMBOL_GPL(sysfs_create_groups);

 
int sysfs_update_groups(struct kobject *kobj,
			const struct attribute_group **groups)
{
	return internal_create_groups(kobj, 1, groups);
}
EXPORT_SYMBOL_GPL(sysfs_update_groups);

 
int sysfs_update_group(struct kobject *kobj,
		       const struct attribute_group *grp)
{
	return internal_create_group(kobj, 1, grp);
}
EXPORT_SYMBOL_GPL(sysfs_update_group);

 
void sysfs_remove_group(struct kobject *kobj,
			const struct attribute_group *grp)
{
	struct kernfs_node *parent = kobj->sd;
	struct kernfs_node *kn;

	if (grp->name) {
		kn = kernfs_find_and_get(parent, grp->name);
		if (!kn) {
			WARN(!kn, KERN_WARNING
			     "sysfs group '%s' not found for kobject '%s'\n",
			     grp->name, kobject_name(kobj));
			return;
		}
	} else {
		kn = parent;
		kernfs_get(kn);
	}

	remove_files(kn, grp);
	if (grp->name)
		kernfs_remove(kn);

	kernfs_put(kn);
}
EXPORT_SYMBOL_GPL(sysfs_remove_group);

 
void sysfs_remove_groups(struct kobject *kobj,
			 const struct attribute_group **groups)
{
	int i;

	if (!groups)
		return;
	for (i = 0; groups[i]; i++)
		sysfs_remove_group(kobj, groups[i]);
}
EXPORT_SYMBOL_GPL(sysfs_remove_groups);

 
int sysfs_merge_group(struct kobject *kobj,
		       const struct attribute_group *grp)
{
	struct kernfs_node *parent;
	kuid_t uid;
	kgid_t gid;
	int error = 0;
	struct attribute *const *attr;
	int i;

	parent = kernfs_find_and_get(kobj->sd, grp->name);
	if (!parent)
		return -ENOENT;

	kobject_get_ownership(kobj, &uid, &gid);

	for ((i = 0, attr = grp->attrs); *attr && !error; (++i, ++attr))
		error = sysfs_add_file_mode_ns(parent, *attr, (*attr)->mode,
					       uid, gid, NULL);
	if (error) {
		while (--i >= 0)
			kernfs_remove_by_name(parent, (*--attr)->name);
	}
	kernfs_put(parent);

	return error;
}
EXPORT_SYMBOL_GPL(sysfs_merge_group);

 
void sysfs_unmerge_group(struct kobject *kobj,
		       const struct attribute_group *grp)
{
	struct kernfs_node *parent;
	struct attribute *const *attr;

	parent = kernfs_find_and_get(kobj->sd, grp->name);
	if (parent) {
		for (attr = grp->attrs; *attr; ++attr)
			kernfs_remove_by_name(parent, (*attr)->name);
		kernfs_put(parent);
	}
}
EXPORT_SYMBOL_GPL(sysfs_unmerge_group);

 
int sysfs_add_link_to_group(struct kobject *kobj, const char *group_name,
			    struct kobject *target, const char *link_name)
{
	struct kernfs_node *parent;
	int error = 0;

	parent = kernfs_find_and_get(kobj->sd, group_name);
	if (!parent)
		return -ENOENT;

	error = sysfs_create_link_sd(parent, target, link_name);
	kernfs_put(parent);

	return error;
}
EXPORT_SYMBOL_GPL(sysfs_add_link_to_group);

 
void sysfs_remove_link_from_group(struct kobject *kobj, const char *group_name,
				  const char *link_name)
{
	struct kernfs_node *parent;

	parent = kernfs_find_and_get(kobj->sd, group_name);
	if (parent) {
		kernfs_remove_by_name(parent, link_name);
		kernfs_put(parent);
	}
}
EXPORT_SYMBOL_GPL(sysfs_remove_link_from_group);

 
int compat_only_sysfs_link_entry_to_kobj(struct kobject *kobj,
					 struct kobject *target_kobj,
					 const char *target_name,
					 const char *symlink_name)
{
	struct kernfs_node *target;
	struct kernfs_node *entry;
	struct kernfs_node *link;

	 
	spin_lock(&sysfs_symlink_target_lock);
	target = target_kobj->sd;
	if (target)
		kernfs_get(target);
	spin_unlock(&sysfs_symlink_target_lock);
	if (!target)
		return -ENOENT;

	entry = kernfs_find_and_get(target, target_name);
	if (!entry) {
		kernfs_put(target);
		return -ENOENT;
	}

	if (!symlink_name)
		symlink_name = target_name;

	link = kernfs_create_link(kobj->sd, symlink_name, entry);
	if (PTR_ERR(link) == -EEXIST)
		sysfs_warn_dup(kobj->sd, symlink_name);

	kernfs_put(entry);
	kernfs_put(target);
	return PTR_ERR_OR_ZERO(link);
}
EXPORT_SYMBOL_GPL(compat_only_sysfs_link_entry_to_kobj);

static int sysfs_group_attrs_change_owner(struct kernfs_node *grp_kn,
					  const struct attribute_group *grp,
					  struct iattr *newattrs)
{
	struct kernfs_node *kn;
	int error;

	if (grp->attrs) {
		struct attribute *const *attr;

		for (attr = grp->attrs; *attr; attr++) {
			kn = kernfs_find_and_get(grp_kn, (*attr)->name);
			if (!kn)
				return -ENOENT;

			error = kernfs_setattr(kn, newattrs);
			kernfs_put(kn);
			if (error)
				return error;
		}
	}

	if (grp->bin_attrs) {
		struct bin_attribute *const *bin_attr;

		for (bin_attr = grp->bin_attrs; *bin_attr; bin_attr++) {
			kn = kernfs_find_and_get(grp_kn, (*bin_attr)->attr.name);
			if (!kn)
				return -ENOENT;

			error = kernfs_setattr(kn, newattrs);
			kernfs_put(kn);
			if (error)
				return error;
		}
	}

	return 0;
}

 
int sysfs_group_change_owner(struct kobject *kobj,
			     const struct attribute_group *grp, kuid_t kuid,
			     kgid_t kgid)
{
	struct kernfs_node *grp_kn;
	int error;
	struct iattr newattrs = {
		.ia_valid = ATTR_UID | ATTR_GID,
		.ia_uid = kuid,
		.ia_gid = kgid,
	};

	if (!kobj->state_in_sysfs)
		return -EINVAL;

	if (grp->name) {
		grp_kn = kernfs_find_and_get(kobj->sd, grp->name);
	} else {
		kernfs_get(kobj->sd);
		grp_kn = kobj->sd;
	}
	if (!grp_kn)
		return -ENOENT;

	error = kernfs_setattr(grp_kn, &newattrs);
	if (!error)
		error = sysfs_group_attrs_change_owner(grp_kn, grp, &newattrs);

	kernfs_put(grp_kn);

	return error;
}
EXPORT_SYMBOL_GPL(sysfs_group_change_owner);

 
int sysfs_groups_change_owner(struct kobject *kobj,
			      const struct attribute_group **groups,
			      kuid_t kuid, kgid_t kgid)
{
	int error = 0, i;

	if (!kobj->state_in_sysfs)
		return -EINVAL;

	if (!groups)
		return 0;

	for (i = 0; groups[i]; i++) {
		error = sysfs_group_change_owner(kobj, groups[i], kuid, kgid);
		if (error)
			break;
	}

	return error;
}
EXPORT_SYMBOL_GPL(sysfs_groups_change_owner);
