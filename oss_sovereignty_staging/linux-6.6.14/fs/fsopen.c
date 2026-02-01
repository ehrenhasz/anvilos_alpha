
 

#include <linux/fs_context.h>
#include <linux/fs_parser.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/security.h>
#include <linux/anon_inodes.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <uapi/linux/mount.h>
#include "internal.h"
#include "mount.h"

 
static ssize_t fscontext_read(struct file *file,
			      char __user *_buf, size_t len, loff_t *pos)
{
	struct fs_context *fc = file->private_data;
	struct fc_log *log = fc->log.log;
	unsigned int logsize = ARRAY_SIZE(log->buffer);
	ssize_t ret;
	char *p;
	bool need_free;
	int index, n;

	ret = mutex_lock_interruptible(&fc->uapi_mutex);
	if (ret < 0)
		return ret;

	if (log->head == log->tail) {
		mutex_unlock(&fc->uapi_mutex);
		return -ENODATA;
	}

	index = log->tail & (logsize - 1);
	p = log->buffer[index];
	need_free = log->need_free & (1 << index);
	log->buffer[index] = NULL;
	log->need_free &= ~(1 << index);
	log->tail++;
	mutex_unlock(&fc->uapi_mutex);

	ret = -EMSGSIZE;
	n = strlen(p);
	if (n > len)
		goto err_free;
	ret = -EFAULT;
	if (copy_to_user(_buf, p, n) != 0)
		goto err_free;
	ret = n;

err_free:
	if (need_free)
		kfree(p);
	return ret;
}

static int fscontext_release(struct inode *inode, struct file *file)
{
	struct fs_context *fc = file->private_data;

	if (fc) {
		file->private_data = NULL;
		put_fs_context(fc);
	}
	return 0;
}

const struct file_operations fscontext_fops = {
	.read		= fscontext_read,
	.release	= fscontext_release,
	.llseek		= no_llseek,
};

 
static int fscontext_create_fd(struct fs_context *fc, unsigned int o_flags)
{
	int fd;

	fd = anon_inode_getfd("[fscontext]", &fscontext_fops, fc,
			      O_RDWR | o_flags);
	if (fd < 0)
		put_fs_context(fc);
	return fd;
}

static int fscontext_alloc_log(struct fs_context *fc)
{
	fc->log.log = kzalloc(sizeof(*fc->log.log), GFP_KERNEL);
	if (!fc->log.log)
		return -ENOMEM;
	refcount_set(&fc->log.log->usage, 1);
	fc->log.log->owner = fc->fs_type->owner;
	return 0;
}

 
SYSCALL_DEFINE2(fsopen, const char __user *, _fs_name, unsigned int, flags)
{
	struct file_system_type *fs_type;
	struct fs_context *fc;
	const char *fs_name;
	int ret;

	if (!may_mount())
		return -EPERM;

	if (flags & ~FSOPEN_CLOEXEC)
		return -EINVAL;

	fs_name = strndup_user(_fs_name, PAGE_SIZE);
	if (IS_ERR(fs_name))
		return PTR_ERR(fs_name);

	fs_type = get_fs_type(fs_name);
	kfree(fs_name);
	if (!fs_type)
		return -ENODEV;

	fc = fs_context_for_mount(fs_type, 0);
	put_filesystem(fs_type);
	if (IS_ERR(fc))
		return PTR_ERR(fc);

	fc->phase = FS_CONTEXT_CREATE_PARAMS;

	ret = fscontext_alloc_log(fc);
	if (ret < 0)
		goto err_fc;

	return fscontext_create_fd(fc, flags & FSOPEN_CLOEXEC ? O_CLOEXEC : 0);

err_fc:
	put_fs_context(fc);
	return ret;
}

 
SYSCALL_DEFINE3(fspick, int, dfd, const char __user *, path, unsigned int, flags)
{
	struct fs_context *fc;
	struct path target;
	unsigned int lookup_flags;
	int ret;

	if (!may_mount())
		return -EPERM;

	if ((flags & ~(FSPICK_CLOEXEC |
		       FSPICK_SYMLINK_NOFOLLOW |
		       FSPICK_NO_AUTOMOUNT |
		       FSPICK_EMPTY_PATH)) != 0)
		return -EINVAL;

	lookup_flags = LOOKUP_FOLLOW | LOOKUP_AUTOMOUNT;
	if (flags & FSPICK_SYMLINK_NOFOLLOW)
		lookup_flags &= ~LOOKUP_FOLLOW;
	if (flags & FSPICK_NO_AUTOMOUNT)
		lookup_flags &= ~LOOKUP_AUTOMOUNT;
	if (flags & FSPICK_EMPTY_PATH)
		lookup_flags |= LOOKUP_EMPTY;
	ret = user_path_at(dfd, path, lookup_flags, &target);
	if (ret < 0)
		goto err;

	ret = -EINVAL;
	if (target.mnt->mnt_root != target.dentry)
		goto err_path;

	fc = fs_context_for_reconfigure(target.dentry, 0, 0);
	if (IS_ERR(fc)) {
		ret = PTR_ERR(fc);
		goto err_path;
	}

	fc->phase = FS_CONTEXT_RECONF_PARAMS;

	ret = fscontext_alloc_log(fc);
	if (ret < 0)
		goto err_fc;

	path_put(&target);
	return fscontext_create_fd(fc, flags & FSPICK_CLOEXEC ? O_CLOEXEC : 0);

err_fc:
	put_fs_context(fc);
err_path:
	path_put(&target);
err:
	return ret;
}

static int vfs_cmd_create(struct fs_context *fc, bool exclusive)
{
	struct super_block *sb;
	int ret;

	if (fc->phase != FS_CONTEXT_CREATE_PARAMS)
		return -EBUSY;

	if (!mount_capable(fc))
		return -EPERM;

	 
	if (exclusive && fc->ops == &legacy_fs_context_ops)
		return -EOPNOTSUPP;

	fc->phase = FS_CONTEXT_CREATING;
	fc->exclusive = exclusive;

	ret = vfs_get_tree(fc);
	if (ret) {
		fc->phase = FS_CONTEXT_FAILED;
		return ret;
	}

	sb = fc->root->d_sb;
	ret = security_sb_kern_mount(sb);
	if (unlikely(ret)) {
		fc_drop_locked(fc);
		fc->phase = FS_CONTEXT_FAILED;
		return ret;
	}

	 
	up_write(&sb->s_umount);
	fc->phase = FS_CONTEXT_AWAITING_MOUNT;
	return 0;
}

static int vfs_cmd_reconfigure(struct fs_context *fc)
{
	struct super_block *sb;
	int ret;

	if (fc->phase != FS_CONTEXT_RECONF_PARAMS)
		return -EBUSY;

	fc->phase = FS_CONTEXT_RECONFIGURING;

	sb = fc->root->d_sb;
	if (!ns_capable(sb->s_user_ns, CAP_SYS_ADMIN)) {
		fc->phase = FS_CONTEXT_FAILED;
		return -EPERM;
	}

	down_write(&sb->s_umount);
	ret = reconfigure_super(fc);
	up_write(&sb->s_umount);
	if (ret) {
		fc->phase = FS_CONTEXT_FAILED;
		return ret;
	}

	vfs_clean_context(fc);
	return 0;
}

 
static int vfs_fsconfig_locked(struct fs_context *fc, int cmd,
			       struct fs_parameter *param)
{
	int ret;

	ret = finish_clean_context(fc);
	if (ret)
		return ret;
	switch (cmd) {
	case FSCONFIG_CMD_CREATE:
		return vfs_cmd_create(fc, false);
	case FSCONFIG_CMD_CREATE_EXCL:
		return vfs_cmd_create(fc, true);
	case FSCONFIG_CMD_RECONFIGURE:
		return vfs_cmd_reconfigure(fc);
	default:
		if (fc->phase != FS_CONTEXT_CREATE_PARAMS &&
		    fc->phase != FS_CONTEXT_RECONF_PARAMS)
			return -EBUSY;

		return vfs_parse_fs_param(fc, param);
	}
}

 
SYSCALL_DEFINE5(fsconfig,
		int, fd,
		unsigned int, cmd,
		const char __user *, _key,
		const void __user *, _value,
		int, aux)
{
	struct fs_context *fc;
	struct fd f;
	int ret;
	int lookup_flags = 0;

	struct fs_parameter param = {
		.type	= fs_value_is_undefined,
	};

	if (fd < 0)
		return -EINVAL;

	switch (cmd) {
	case FSCONFIG_SET_FLAG:
		if (!_key || _value || aux)
			return -EINVAL;
		break;
	case FSCONFIG_SET_STRING:
		if (!_key || !_value || aux)
			return -EINVAL;
		break;
	case FSCONFIG_SET_BINARY:
		if (!_key || !_value || aux <= 0 || aux > 1024 * 1024)
			return -EINVAL;
		break;
	case FSCONFIG_SET_PATH:
	case FSCONFIG_SET_PATH_EMPTY:
		if (!_key || !_value || (aux != AT_FDCWD && aux < 0))
			return -EINVAL;
		break;
	case FSCONFIG_SET_FD:
		if (!_key || _value || aux < 0)
			return -EINVAL;
		break;
	case FSCONFIG_CMD_CREATE:
	case FSCONFIG_CMD_CREATE_EXCL:
	case FSCONFIG_CMD_RECONFIGURE:
		if (_key || _value || aux)
			return -EINVAL;
		break;
	default:
		return -EOPNOTSUPP;
	}

	f = fdget(fd);
	if (!f.file)
		return -EBADF;
	ret = -EINVAL;
	if (f.file->f_op != &fscontext_fops)
		goto out_f;

	fc = f.file->private_data;
	if (fc->ops == &legacy_fs_context_ops) {
		switch (cmd) {
		case FSCONFIG_SET_BINARY:
		case FSCONFIG_SET_PATH:
		case FSCONFIG_SET_PATH_EMPTY:
		case FSCONFIG_SET_FD:
			ret = -EOPNOTSUPP;
			goto out_f;
		}
	}

	if (_key) {
		param.key = strndup_user(_key, 256);
		if (IS_ERR(param.key)) {
			ret = PTR_ERR(param.key);
			goto out_f;
		}
	}

	switch (cmd) {
	case FSCONFIG_SET_FLAG:
		param.type = fs_value_is_flag;
		break;
	case FSCONFIG_SET_STRING:
		param.type = fs_value_is_string;
		param.string = strndup_user(_value, 256);
		if (IS_ERR(param.string)) {
			ret = PTR_ERR(param.string);
			goto out_key;
		}
		param.size = strlen(param.string);
		break;
	case FSCONFIG_SET_BINARY:
		param.type = fs_value_is_blob;
		param.size = aux;
		param.blob = memdup_user_nul(_value, aux);
		if (IS_ERR(param.blob)) {
			ret = PTR_ERR(param.blob);
			goto out_key;
		}
		break;
	case FSCONFIG_SET_PATH_EMPTY:
		lookup_flags = LOOKUP_EMPTY;
		fallthrough;
	case FSCONFIG_SET_PATH:
		param.type = fs_value_is_filename;
		param.name = getname_flags(_value, lookup_flags, NULL);
		if (IS_ERR(param.name)) {
			ret = PTR_ERR(param.name);
			goto out_key;
		}
		param.dirfd = aux;
		param.size = strlen(param.name->name);
		break;
	case FSCONFIG_SET_FD:
		param.type = fs_value_is_file;
		ret = -EBADF;
		param.file = fget(aux);
		if (!param.file)
			goto out_key;
		break;
	default:
		break;
	}

	ret = mutex_lock_interruptible(&fc->uapi_mutex);
	if (ret == 0) {
		ret = vfs_fsconfig_locked(fc, cmd, &param);
		mutex_unlock(&fc->uapi_mutex);
	}

	 
	switch (cmd) {
	case FSCONFIG_SET_STRING:
	case FSCONFIG_SET_BINARY:
		kfree(param.string);
		break;
	case FSCONFIG_SET_PATH:
	case FSCONFIG_SET_PATH_EMPTY:
		if (param.name)
			putname(param.name);
		break;
	case FSCONFIG_SET_FD:
		if (param.file)
			fput(param.file);
		break;
	default:
		break;
	}
out_key:
	kfree(param.key);
out_f:
	fdput(f);
	return ret;
}
