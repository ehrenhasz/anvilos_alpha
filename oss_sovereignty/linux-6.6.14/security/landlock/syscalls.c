
 

#include <asm/current.h>
#include <linux/anon_inodes.h>
#include <linux/build_bug.h>
#include <linux/capability.h>
#include <linux/compiler_types.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/limits.h>
#include <linux/mount.h>
#include <linux/path.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/stddef.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <uapi/linux/landlock.h>

#include "cred.h"
#include "fs.h"
#include "limits.h"
#include "ruleset.h"
#include "setup.h"

 
static __always_inline int
copy_min_struct_from_user(void *const dst, const size_t ksize,
			  const size_t ksize_min, const void __user *const src,
			  const size_t usize)
{
	 
	BUILD_BUG_ON(!dst);
	if (!src)
		return -EFAULT;

	 
	BUILD_BUG_ON(ksize <= 0);
	BUILD_BUG_ON(ksize < ksize_min);
	if (usize < ksize_min)
		return -EINVAL;
	if (usize > PAGE_SIZE)
		return -E2BIG;

	 
	return copy_struct_from_user(dst, ksize, src, usize);
}

 
static void build_check_abi(void)
{
	struct landlock_ruleset_attr ruleset_attr;
	struct landlock_path_beneath_attr path_beneath_attr;
	size_t ruleset_size, path_beneath_size;

	 
	ruleset_size = sizeof(ruleset_attr.handled_access_fs);
	BUILD_BUG_ON(sizeof(ruleset_attr) != ruleset_size);
	BUILD_BUG_ON(sizeof(ruleset_attr) != 8);

	path_beneath_size = sizeof(path_beneath_attr.allowed_access);
	path_beneath_size += sizeof(path_beneath_attr.parent_fd);
	BUILD_BUG_ON(sizeof(path_beneath_attr) != path_beneath_size);
	BUILD_BUG_ON(sizeof(path_beneath_attr) != 12);
}

 

static int fop_ruleset_release(struct inode *const inode,
			       struct file *const filp)
{
	struct landlock_ruleset *ruleset = filp->private_data;

	landlock_put_ruleset(ruleset);
	return 0;
}

static ssize_t fop_dummy_read(struct file *const filp, char __user *const buf,
			      const size_t size, loff_t *const ppos)
{
	 
	return -EINVAL;
}

static ssize_t fop_dummy_write(struct file *const filp,
			       const char __user *const buf, const size_t size,
			       loff_t *const ppos)
{
	 
	return -EINVAL;
}

 
static const struct file_operations ruleset_fops = {
	.release = fop_ruleset_release,
	.read = fop_dummy_read,
	.write = fop_dummy_write,
};

#define LANDLOCK_ABI_VERSION 3

 
SYSCALL_DEFINE3(landlock_create_ruleset,
		const struct landlock_ruleset_attr __user *const, attr,
		const size_t, size, const __u32, flags)
{
	struct landlock_ruleset_attr ruleset_attr;
	struct landlock_ruleset *ruleset;
	int err, ruleset_fd;

	 
	build_check_abi();

	if (!landlock_initialized)
		return -EOPNOTSUPP;

	if (flags) {
		if ((flags == LANDLOCK_CREATE_RULESET_VERSION) && !attr &&
		    !size)
			return LANDLOCK_ABI_VERSION;
		return -EINVAL;
	}

	 
	err = copy_min_struct_from_user(&ruleset_attr, sizeof(ruleset_attr),
					offsetofend(typeof(ruleset_attr),
						    handled_access_fs),
					attr, size);
	if (err)
		return err;

	 
	if ((ruleset_attr.handled_access_fs | LANDLOCK_MASK_ACCESS_FS) !=
	    LANDLOCK_MASK_ACCESS_FS)
		return -EINVAL;

	 
	ruleset = landlock_create_ruleset(ruleset_attr.handled_access_fs);
	if (IS_ERR(ruleset))
		return PTR_ERR(ruleset);

	 
	ruleset_fd = anon_inode_getfd("[landlock-ruleset]", &ruleset_fops,
				      ruleset, O_RDWR | O_CLOEXEC);
	if (ruleset_fd < 0)
		landlock_put_ruleset(ruleset);
	return ruleset_fd;
}

 
static struct landlock_ruleset *get_ruleset_from_fd(const int fd,
						    const fmode_t mode)
{
	struct fd ruleset_f;
	struct landlock_ruleset *ruleset;

	ruleset_f = fdget(fd);
	if (!ruleset_f.file)
		return ERR_PTR(-EBADF);

	 
	if (ruleset_f.file->f_op != &ruleset_fops) {
		ruleset = ERR_PTR(-EBADFD);
		goto out_fdput;
	}
	if (!(ruleset_f.file->f_mode & mode)) {
		ruleset = ERR_PTR(-EPERM);
		goto out_fdput;
	}
	ruleset = ruleset_f.file->private_data;
	if (WARN_ON_ONCE(ruleset->num_layers != 1)) {
		ruleset = ERR_PTR(-EINVAL);
		goto out_fdput;
	}
	landlock_get_ruleset(ruleset);

out_fdput:
	fdput(ruleset_f);
	return ruleset;
}

 

 
static int get_path_from_fd(const s32 fd, struct path *const path)
{
	struct fd f;
	int err = 0;

	BUILD_BUG_ON(!__same_type(
		fd, ((struct landlock_path_beneath_attr *)NULL)->parent_fd));

	 
	f = fdget_raw(fd);
	if (!f.file)
		return -EBADF;
	 
	if ((f.file->f_op == &ruleset_fops) ||
	    (f.file->f_path.mnt->mnt_flags & MNT_INTERNAL) ||
	    (f.file->f_path.dentry->d_sb->s_flags & SB_NOUSER) ||
	    d_is_negative(f.file->f_path.dentry) ||
	    IS_PRIVATE(d_backing_inode(f.file->f_path.dentry))) {
		err = -EBADFD;
		goto out_fdput;
	}
	*path = f.file->f_path;
	path_get(path);

out_fdput:
	fdput(f);
	return err;
}

 
SYSCALL_DEFINE4(landlock_add_rule, const int, ruleset_fd,
		const enum landlock_rule_type, rule_type,
		const void __user *const, rule_attr, const __u32, flags)
{
	struct landlock_path_beneath_attr path_beneath_attr;
	struct path path;
	struct landlock_ruleset *ruleset;
	int res, err;

	if (!landlock_initialized)
		return -EOPNOTSUPP;

	 
	if (flags)
		return -EINVAL;

	 
	ruleset = get_ruleset_from_fd(ruleset_fd, FMODE_CAN_WRITE);
	if (IS_ERR(ruleset))
		return PTR_ERR(ruleset);

	if (rule_type != LANDLOCK_RULE_PATH_BENEATH) {
		err = -EINVAL;
		goto out_put_ruleset;
	}

	 
	res = copy_from_user(&path_beneath_attr, rule_attr,
			     sizeof(path_beneath_attr));
	if (res) {
		err = -EFAULT;
		goto out_put_ruleset;
	}

	 
	if (!path_beneath_attr.allowed_access) {
		err = -ENOMSG;
		goto out_put_ruleset;
	}
	 
	if ((path_beneath_attr.allowed_access | ruleset->fs_access_masks[0]) !=
	    ruleset->fs_access_masks[0]) {
		err = -EINVAL;
		goto out_put_ruleset;
	}

	 
	err = get_path_from_fd(path_beneath_attr.parent_fd, &path);
	if (err)
		goto out_put_ruleset;

	 
	err = landlock_append_fs_rule(ruleset, &path,
				      path_beneath_attr.allowed_access);
	path_put(&path);

out_put_ruleset:
	landlock_put_ruleset(ruleset);
	return err;
}

 

 
SYSCALL_DEFINE2(landlock_restrict_self, const int, ruleset_fd, const __u32,
		flags)
{
	struct landlock_ruleset *new_dom, *ruleset;
	struct cred *new_cred;
	struct landlock_cred_security *new_llcred;
	int err;

	if (!landlock_initialized)
		return -EOPNOTSUPP;

	 
	if (!task_no_new_privs(current) &&
	    !ns_capable_noaudit(current_user_ns(), CAP_SYS_ADMIN))
		return -EPERM;

	 
	if (flags)
		return -EINVAL;

	 
	ruleset = get_ruleset_from_fd(ruleset_fd, FMODE_CAN_READ);
	if (IS_ERR(ruleset))
		return PTR_ERR(ruleset);

	 
	new_cred = prepare_creds();
	if (!new_cred) {
		err = -ENOMEM;
		goto out_put_ruleset;
	}
	new_llcred = landlock_cred(new_cred);

	 
	new_dom = landlock_merge_ruleset(new_llcred->domain, ruleset);
	if (IS_ERR(new_dom)) {
		err = PTR_ERR(new_dom);
		goto out_put_creds;
	}

	 
	landlock_put_ruleset(new_llcred->domain);
	new_llcred->domain = new_dom;

	landlock_put_ruleset(ruleset);
	return commit_creds(new_cred);

out_put_creds:
	abort_creds(new_cred);

out_put_ruleset:
	landlock_put_ruleset(ruleset);
	return err;
}
