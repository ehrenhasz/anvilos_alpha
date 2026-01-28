#ifndef __SYSFS_INTERNAL_H
#define __SYSFS_INTERNAL_H
#include <linux/sysfs.h>
extern struct kernfs_node *sysfs_root_kn;
extern spinlock_t sysfs_symlink_target_lock;
void sysfs_warn_dup(struct kernfs_node *parent, const char *name);
int sysfs_add_file_mode_ns(struct kernfs_node *parent,
		const struct attribute *attr, umode_t amode, kuid_t uid,
		kgid_t gid, const void *ns);
int sysfs_add_bin_file_mode_ns(struct kernfs_node *parent,
		const struct bin_attribute *battr, umode_t mode,
		kuid_t uid, kgid_t gid, const void *ns);
int sysfs_create_link_sd(struct kernfs_node *kn, struct kobject *target,
			 const char *name);
#endif	 
