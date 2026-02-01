
 

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/namei.h>
#include <linux/slab.h>

#include <linux/configfs.h>
#include "configfs_internal.h"

 
DEFINE_MUTEX(configfs_symlink_mutex);

static int item_depth(struct config_item * item)
{
	struct config_item * p = item;
	int depth = 0;
	do { depth++; } while ((p = p->ci_parent) && !configfs_is_root(p));
	return depth;
}

static int item_path_length(struct config_item * item)
{
	struct config_item * p = item;
	int length = 1;
	do {
		length += strlen(config_item_name(p)) + 1;
		p = p->ci_parent;
	} while (p && !configfs_is_root(p));
	return length;
}

static void fill_item_path(struct config_item * item, char * buffer, int length)
{
	struct config_item * p;

	--length;
	for (p = item; p && !configfs_is_root(p); p = p->ci_parent) {
		int cur = strlen(config_item_name(p));

		 
		length -= cur;
		memcpy(buffer + length, config_item_name(p), cur);
		*(buffer + --length) = '/';
	}
}

static int configfs_get_target_path(struct config_item *item,
		struct config_item *target, char *path)
{
	int depth, size;
	char *s;

	depth = item_depth(item);
	size = item_path_length(target) + depth * 3 - 1;
	if (size > PATH_MAX)
		return -ENAMETOOLONG;

	pr_debug("%s: depth = %d, size = %d\n", __func__, depth, size);

	for (s = path; depth--; s += 3)
		strcpy(s,"../");

	fill_item_path(target, path, size);
	pr_debug("%s: path = '%s'\n", __func__, path);
	return 0;
}

static int create_link(struct config_item *parent_item,
		       struct config_item *item,
		       struct dentry *dentry)
{
	struct configfs_dirent *target_sd = item->ci_dentry->d_fsdata;
	char *body;
	int ret;

	if (!configfs_dirent_is_ready(target_sd))
		return -ENOENT;

	body = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!body)
		return -ENOMEM;

	configfs_get(target_sd);
	spin_lock(&configfs_dirent_lock);
	if (target_sd->s_type & CONFIGFS_USET_DROPPING) {
		spin_unlock(&configfs_dirent_lock);
		configfs_put(target_sd);
		kfree(body);
		return -ENOENT;
	}
	target_sd->s_links++;
	spin_unlock(&configfs_dirent_lock);
	ret = configfs_get_target_path(parent_item, item, body);
	if (!ret)
		ret = configfs_create_link(target_sd, parent_item->ci_dentry,
					   dentry, body);
	if (ret) {
		spin_lock(&configfs_dirent_lock);
		target_sd->s_links--;
		spin_unlock(&configfs_dirent_lock);
		configfs_put(target_sd);
		kfree(body);
	}
	return ret;
}


static int get_target(const char *symname, struct path *path,
		      struct config_item **target, struct super_block *sb)
{
	int ret;

	ret = kern_path(symname, LOOKUP_FOLLOW|LOOKUP_DIRECTORY, path);
	if (!ret) {
		if (path->dentry->d_sb == sb) {
			*target = configfs_get_config_item(path->dentry);
			if (!*target) {
				ret = -ENOENT;
				path_put(path);
			}
		} else {
			ret = -EPERM;
			path_put(path);
		}
	}

	return ret;
}


int configfs_symlink(struct mnt_idmap *idmap, struct inode *dir,
		     struct dentry *dentry, const char *symname)
{
	int ret;
	struct path path;
	struct configfs_dirent *sd;
	struct config_item *parent_item;
	struct config_item *target_item = NULL;
	const struct config_item_type *type;

	sd = dentry->d_parent->d_fsdata;
	 
	if (!configfs_dirent_is_ready(sd))
		return -ENOENT;

	parent_item = configfs_get_config_item(dentry->d_parent);
	type = parent_item->ci_type;

	ret = -EPERM;
	if (!type || !type->ct_item_ops ||
	    !type->ct_item_ops->allow_link)
		goto out_put;

	 
	inode_unlock(dir);
	ret = get_target(symname, &path, &target_item, dentry->d_sb);
	inode_lock(dir);
	if (ret)
		goto out_put;

	if (dentry->d_inode || d_unhashed(dentry))
		ret = -EEXIST;
	else
		ret = inode_permission(&nop_mnt_idmap, dir,
				       MAY_WRITE | MAY_EXEC);
	if (!ret)
		ret = type->ct_item_ops->allow_link(parent_item, target_item);
	if (!ret) {
		mutex_lock(&configfs_symlink_mutex);
		ret = create_link(parent_item, target_item, dentry);
		mutex_unlock(&configfs_symlink_mutex);
		if (ret && type->ct_item_ops->drop_link)
			type->ct_item_ops->drop_link(parent_item,
						     target_item);
	}

	config_item_put(target_item);
	path_put(&path);

out_put:
	config_item_put(parent_item);
	return ret;
}

int configfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct configfs_dirent *sd = dentry->d_fsdata, *target_sd;
	struct config_item *parent_item;
	const struct config_item_type *type;
	int ret;

	ret = -EPERM;   
	if (!(sd->s_type & CONFIGFS_ITEM_LINK))
		goto out;

	target_sd = sd->s_element;

	parent_item = configfs_get_config_item(dentry->d_parent);
	type = parent_item->ci_type;

	spin_lock(&configfs_dirent_lock);
	list_del_init(&sd->s_sibling);
	spin_unlock(&configfs_dirent_lock);
	configfs_drop_dentry(sd, dentry->d_parent);
	dput(dentry);
	configfs_put(sd);

	 
	if (type && type->ct_item_ops &&
	    type->ct_item_ops->drop_link)
		type->ct_item_ops->drop_link(parent_item,
					       target_sd->s_element);

	spin_lock(&configfs_dirent_lock);
	target_sd->s_links--;
	spin_unlock(&configfs_dirent_lock);
	configfs_put(target_sd);

	config_item_put(parent_item);

	ret = 0;

out:
	return ret;
}

const struct inode_operations configfs_symlink_inode_operations = {
	.get_link = simple_get_link,
	.setattr = configfs_setattr,
};

