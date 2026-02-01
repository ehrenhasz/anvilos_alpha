
 

#include <linux/cred.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/magic.h>
#include <linux/anon_inodes.h>
#include <linux/pseudo_fs.h>

#include <linux/uaccess.h>

static struct vfsmount *anon_inode_mnt __read_mostly;
static struct inode *anon_inode_inode;

 
static char *anon_inodefs_dname(struct dentry *dentry, char *buffer, int buflen)
{
	return dynamic_dname(buffer, buflen, "anon_inode:%s",
				dentry->d_name.name);
}

static const struct dentry_operations anon_inodefs_dentry_operations = {
	.d_dname	= anon_inodefs_dname,
};

static int anon_inodefs_init_fs_context(struct fs_context *fc)
{
	struct pseudo_fs_context *ctx = init_pseudo(fc, ANON_INODE_FS_MAGIC);
	if (!ctx)
		return -ENOMEM;
	ctx->dops = &anon_inodefs_dentry_operations;
	return 0;
}

static struct file_system_type anon_inode_fs_type = {
	.name		= "anon_inodefs",
	.init_fs_context = anon_inodefs_init_fs_context,
	.kill_sb	= kill_anon_super,
};

static struct inode *anon_inode_make_secure_inode(
	const char *name,
	const struct inode *context_inode)
{
	struct inode *inode;
	const struct qstr qname = QSTR_INIT(name, strlen(name));
	int error;

	inode = alloc_anon_inode(anon_inode_mnt->mnt_sb);
	if (IS_ERR(inode))
		return inode;
	inode->i_flags &= ~S_PRIVATE;
	error =	security_inode_init_security_anon(inode, &qname, context_inode);
	if (error) {
		iput(inode);
		return ERR_PTR(error);
	}
	return inode;
}

static struct file *__anon_inode_getfile(const char *name,
					 const struct file_operations *fops,
					 void *priv, int flags,
					 const struct inode *context_inode,
					 bool secure)
{
	struct inode *inode;
	struct file *file;

	if (fops->owner && !try_module_get(fops->owner))
		return ERR_PTR(-ENOENT);

	if (secure) {
		inode =	anon_inode_make_secure_inode(name, context_inode);
		if (IS_ERR(inode)) {
			file = ERR_CAST(inode);
			goto err;
		}
	} else {
		inode =	anon_inode_inode;
		if (IS_ERR(inode)) {
			file = ERR_PTR(-ENODEV);
			goto err;
		}
		 
		ihold(inode);
	}

	file = alloc_file_pseudo(inode, anon_inode_mnt, name,
				 flags & (O_ACCMODE | O_NONBLOCK), fops);
	if (IS_ERR(file))
		goto err_iput;

	file->f_mapping = inode->i_mapping;

	file->private_data = priv;

	return file;

err_iput:
	iput(inode);
err:
	module_put(fops->owner);
	return file;
}

 
struct file *anon_inode_getfile(const char *name,
				const struct file_operations *fops,
				void *priv, int flags)
{
	return __anon_inode_getfile(name, fops, priv, flags, NULL, false);
}
EXPORT_SYMBOL_GPL(anon_inode_getfile);

 
struct file *anon_inode_getfile_secure(const char *name,
				       const struct file_operations *fops,
				       void *priv, int flags,
				       const struct inode *context_inode)
{
	return __anon_inode_getfile(name, fops, priv, flags,
				    context_inode, true);
}

static int __anon_inode_getfd(const char *name,
			      const struct file_operations *fops,
			      void *priv, int flags,
			      const struct inode *context_inode,
			      bool secure)
{
	int error, fd;
	struct file *file;

	error = get_unused_fd_flags(flags);
	if (error < 0)
		return error;
	fd = error;

	file = __anon_inode_getfile(name, fops, priv, flags, context_inode,
				    secure);
	if (IS_ERR(file)) {
		error = PTR_ERR(file);
		goto err_put_unused_fd;
	}
	fd_install(fd, file);

	return fd;

err_put_unused_fd:
	put_unused_fd(fd);
	return error;
}

 
int anon_inode_getfd(const char *name, const struct file_operations *fops,
		     void *priv, int flags)
{
	return __anon_inode_getfd(name, fops, priv, flags, NULL, false);
}
EXPORT_SYMBOL_GPL(anon_inode_getfd);

 
int anon_inode_getfd_secure(const char *name, const struct file_operations *fops,
			    void *priv, int flags,
			    const struct inode *context_inode)
{
	return __anon_inode_getfd(name, fops, priv, flags, context_inode, true);
}
EXPORT_SYMBOL_GPL(anon_inode_getfd_secure);

static int __init anon_inode_init(void)
{
	anon_inode_mnt = kern_mount(&anon_inode_fs_type);
	if (IS_ERR(anon_inode_mnt))
		panic("anon_inode_init() kernel mount failed (%ld)\n", PTR_ERR(anon_inode_mnt));

	anon_inode_inode = alloc_anon_inode(anon_inode_mnt->mnt_sb);
	if (IS_ERR(anon_inode_inode))
		panic("anon_inode_init() inode allocation failed (%ld)\n", PTR_ERR(anon_inode_inode));

	return 0;
}

fs_initcall(anon_inode_init);

