
 

 
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/namei.h>
#include <linux/security.h>
#include <linux/lsm_hooks.h>
#include <linux/magic.h>

static struct vfsmount *mount;
static int mount_count;

static void securityfs_free_inode(struct inode *inode)
{
	if (S_ISLNK(inode->i_mode))
		kfree(inode->i_link);
	free_inode_nonrcu(inode);
}

static const struct super_operations securityfs_super_operations = {
	.statfs		= simple_statfs,
	.free_inode	= securityfs_free_inode,
};

static int securityfs_fill_super(struct super_block *sb, struct fs_context *fc)
{
	static const struct tree_descr files[] = {{""}};
	int error;

	error = simple_fill_super(sb, SECURITYFS_MAGIC, files);
	if (error)
		return error;

	sb->s_op = &securityfs_super_operations;

	return 0;
}

static int securityfs_get_tree(struct fs_context *fc)
{
	return get_tree_single(fc, securityfs_fill_super);
}

static const struct fs_context_operations securityfs_context_ops = {
	.get_tree	= securityfs_get_tree,
};

static int securityfs_init_fs_context(struct fs_context *fc)
{
	fc->ops = &securityfs_context_ops;
	return 0;
}

static struct file_system_type fs_type = {
	.owner =	THIS_MODULE,
	.name =		"securityfs",
	.init_fs_context = securityfs_init_fs_context,
	.kill_sb =	kill_litter_super,
};

 
static struct dentry *securityfs_create_dentry(const char *name, umode_t mode,
					struct dentry *parent, void *data,
					const struct file_operations *fops,
					const struct inode_operations *iops)
{
	struct dentry *dentry;
	struct inode *dir, *inode;
	int error;

	if (!(mode & S_IFMT))
		mode = (mode & S_IALLUGO) | S_IFREG;

	pr_debug("securityfs: creating file '%s'\n",name);

	error = simple_pin_fs(&fs_type, &mount, &mount_count);
	if (error)
		return ERR_PTR(error);

	if (!parent)
		parent = mount->mnt_root;

	dir = d_inode(parent);

	inode_lock(dir);
	dentry = lookup_one_len(name, parent, strlen(name));
	if (IS_ERR(dentry))
		goto out;

	if (d_really_is_positive(dentry)) {
		error = -EEXIST;
		goto out1;
	}

	inode = new_inode(dir->i_sb);
	if (!inode) {
		error = -ENOMEM;
		goto out1;
	}

	inode->i_ino = get_next_ino();
	inode->i_mode = mode;
	inode->i_atime = inode->i_mtime = inode_set_ctime_current(inode);
	inode->i_private = data;
	if (S_ISDIR(mode)) {
		inode->i_op = &simple_dir_inode_operations;
		inode->i_fop = &simple_dir_operations;
		inc_nlink(inode);
		inc_nlink(dir);
	} else if (S_ISLNK(mode)) {
		inode->i_op = iops ? iops : &simple_symlink_inode_operations;
		inode->i_link = data;
	} else {
		inode->i_fop = fops;
	}
	d_instantiate(dentry, inode);
	dget(dentry);
	inode_unlock(dir);
	return dentry;

out1:
	dput(dentry);
	dentry = ERR_PTR(error);
out:
	inode_unlock(dir);
	simple_release_fs(&mount, &mount_count);
	return dentry;
}

 
struct dentry *securityfs_create_file(const char *name, umode_t mode,
				      struct dentry *parent, void *data,
				      const struct file_operations *fops)
{
	return securityfs_create_dentry(name, mode, parent, data, fops, NULL);
}
EXPORT_SYMBOL_GPL(securityfs_create_file);

 
struct dentry *securityfs_create_dir(const char *name, struct dentry *parent)
{
	return securityfs_create_file(name, S_IFDIR | 0755, parent, NULL, NULL);
}
EXPORT_SYMBOL_GPL(securityfs_create_dir);

 
struct dentry *securityfs_create_symlink(const char *name,
					 struct dentry *parent,
					 const char *target,
					 const struct inode_operations *iops)
{
	struct dentry *dent;
	char *link = NULL;

	if (target) {
		link = kstrdup(target, GFP_KERNEL);
		if (!link)
			return ERR_PTR(-ENOMEM);
	}
	dent = securityfs_create_dentry(name, S_IFLNK | 0444, parent,
					link, NULL, iops);
	if (IS_ERR(dent))
		kfree(link);

	return dent;
}
EXPORT_SYMBOL_GPL(securityfs_create_symlink);

 
void securityfs_remove(struct dentry *dentry)
{
	struct inode *dir;

	if (!dentry || IS_ERR(dentry))
		return;

	dir = d_inode(dentry->d_parent);
	inode_lock(dir);
	if (simple_positive(dentry)) {
		if (d_is_dir(dentry))
			simple_rmdir(dir, dentry);
		else
			simple_unlink(dir, dentry);
		dput(dentry);
	}
	inode_unlock(dir);
	simple_release_fs(&mount, &mount_count);
}
EXPORT_SYMBOL_GPL(securityfs_remove);

#ifdef CONFIG_SECURITY
static struct dentry *lsm_dentry;
static ssize_t lsm_read(struct file *filp, char __user *buf, size_t count,
			loff_t *ppos)
{
	return simple_read_from_buffer(buf, count, ppos, lsm_names,
		strlen(lsm_names));
}

static const struct file_operations lsm_ops = {
	.read = lsm_read,
	.llseek = generic_file_llseek,
};
#endif

static int __init securityfs_init(void)
{
	int retval;

	retval = sysfs_create_mount_point(kernel_kobj, "security");
	if (retval)
		return retval;

	retval = register_filesystem(&fs_type);
	if (retval) {
		sysfs_remove_mount_point(kernel_kobj, "security");
		return retval;
	}
#ifdef CONFIG_SECURITY
	lsm_dentry = securityfs_create_file("lsm", 0444, NULL, NULL,
						&lsm_ops);
#endif
	return 0;
}
core_initcall(securityfs_init);
