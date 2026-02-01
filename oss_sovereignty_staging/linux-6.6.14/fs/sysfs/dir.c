
 

#define pr_fmt(fmt)	"sysfs: " fmt

#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include "sysfs.h"

DEFINE_SPINLOCK(sysfs_symlink_target_lock);

void sysfs_warn_dup(struct kernfs_node *parent, const char *name)
{
	char *buf;

	buf = kzalloc(PATH_MAX, GFP_KERNEL);
	if (buf)
		kernfs_path(parent, buf, PATH_MAX);

	pr_warn("cannot create duplicate filename '%s/%s'\n", buf, name);
	dump_stack();

	kfree(buf);
}

 
int sysfs_create_dir_ns(struct kobject *kobj, const void *ns)
{
	struct kernfs_node *parent, *kn;
	kuid_t uid;
	kgid_t gid;

	if (WARN_ON(!kobj))
		return -EINVAL;

	if (kobj->parent)
		parent = kobj->parent->sd;
	else
		parent = sysfs_root_kn;

	if (!parent)
		return -ENOENT;

	kobject_get_ownership(kobj, &uid, &gid);

	kn = kernfs_create_dir_ns(parent, kobject_name(kobj), 0755, uid, gid,
				  kobj, ns);
	if (IS_ERR(kn)) {
		if (PTR_ERR(kn) == -EEXIST)
			sysfs_warn_dup(parent, kobject_name(kobj));
		return PTR_ERR(kn);
	}

	kobj->sd = kn;
	return 0;
}

 
void sysfs_remove_dir(struct kobject *kobj)
{
	struct kernfs_node *kn = kobj->sd;

	 
	spin_lock(&sysfs_symlink_target_lock);
	kobj->sd = NULL;
	spin_unlock(&sysfs_symlink_target_lock);

	if (kn) {
		WARN_ON_ONCE(kernfs_type(kn) != KERNFS_DIR);
		kernfs_remove(kn);
	}
}

int sysfs_rename_dir_ns(struct kobject *kobj, const char *new_name,
			const void *new_ns)
{
	struct kernfs_node *parent;
	int ret;

	parent = kernfs_get_parent(kobj->sd);
	ret = kernfs_rename_ns(kobj->sd, parent, new_name, new_ns);
	kernfs_put(parent);
	return ret;
}

int sysfs_move_dir_ns(struct kobject *kobj, struct kobject *new_parent_kobj,
		      const void *new_ns)
{
	struct kernfs_node *kn = kobj->sd;
	struct kernfs_node *new_parent;

	new_parent = new_parent_kobj && new_parent_kobj->sd ?
		new_parent_kobj->sd : sysfs_root_kn;

	return kernfs_rename_ns(kn, new_parent, kn->name, new_ns);
}

 
int sysfs_create_mount_point(struct kobject *parent_kobj, const char *name)
{
	struct kernfs_node *kn, *parent = parent_kobj->sd;

	kn = kernfs_create_empty_dir(parent, name);
	if (IS_ERR(kn)) {
		if (PTR_ERR(kn) == -EEXIST)
			sysfs_warn_dup(parent, name);
		return PTR_ERR(kn);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sysfs_create_mount_point);

 
void sysfs_remove_mount_point(struct kobject *parent_kobj, const char *name)
{
	struct kernfs_node *parent = parent_kobj->sd;

	kernfs_remove_by_name_ns(parent, name, NULL);
}
EXPORT_SYMBOL_GPL(sysfs_remove_mount_point);
