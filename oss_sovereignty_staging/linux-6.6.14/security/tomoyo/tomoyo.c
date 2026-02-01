
 

#include <linux/lsm_hooks.h>
#include "common.h"

 
struct tomoyo_domain_info *tomoyo_domain(void)
{
	struct tomoyo_task *s = tomoyo_task(current);

	if (s->old_domain_info && !current->in_execve) {
		atomic_dec(&s->old_domain_info->users);
		s->old_domain_info = NULL;
	}
	return s->domain_info;
}

 
static int tomoyo_cred_prepare(struct cred *new, const struct cred *old,
			       gfp_t gfp)
{
	 
	struct tomoyo_task *s = tomoyo_task(current);

	if (s->old_domain_info && !current->in_execve) {
		atomic_dec(&s->domain_info->users);
		s->domain_info = s->old_domain_info;
		s->old_domain_info = NULL;
	}
	return 0;
}

 
static void tomoyo_bprm_committed_creds(struct linux_binprm *bprm)
{
	 
	struct tomoyo_task *s = tomoyo_task(current);

	atomic_dec(&s->old_domain_info->users);
	s->old_domain_info = NULL;
}

#ifndef CONFIG_SECURITY_TOMOYO_OMIT_USERSPACE_LOADER
 
static int tomoyo_bprm_creds_for_exec(struct linux_binprm *bprm)
{
	 
	if (!tomoyo_policy_loaded)
		tomoyo_load_policy(bprm->filename);
	return 0;
}
#endif

 
static int tomoyo_bprm_check_security(struct linux_binprm *bprm)
{
	struct tomoyo_task *s = tomoyo_task(current);

	 
	if (!s->old_domain_info) {
		const int idx = tomoyo_read_lock();
		const int err = tomoyo_find_next_domain(bprm);

		tomoyo_read_unlock(idx);
		return err;
	}
	 
	return tomoyo_check_open_permission(s->domain_info,
					    &bprm->file->f_path, O_RDONLY);
}

 
static int tomoyo_inode_getattr(const struct path *path)
{
	return tomoyo_path_perm(TOMOYO_TYPE_GETATTR, path, NULL);
}

 
static int tomoyo_path_truncate(const struct path *path)
{
	return tomoyo_path_perm(TOMOYO_TYPE_TRUNCATE, path, NULL);
}

 
static int tomoyo_file_truncate(struct file *file)
{
	return tomoyo_path_truncate(&file->f_path);
}

 
static int tomoyo_path_unlink(const struct path *parent, struct dentry *dentry)
{
	struct path path = { .mnt = parent->mnt, .dentry = dentry };

	return tomoyo_path_perm(TOMOYO_TYPE_UNLINK, &path, NULL);
}

 
static int tomoyo_path_mkdir(const struct path *parent, struct dentry *dentry,
			     umode_t mode)
{
	struct path path = { .mnt = parent->mnt, .dentry = dentry };

	return tomoyo_path_number_perm(TOMOYO_TYPE_MKDIR, &path,
				       mode & S_IALLUGO);
}

 
static int tomoyo_path_rmdir(const struct path *parent, struct dentry *dentry)
{
	struct path path = { .mnt = parent->mnt, .dentry = dentry };

	return tomoyo_path_perm(TOMOYO_TYPE_RMDIR, &path, NULL);
}

 
static int tomoyo_path_symlink(const struct path *parent, struct dentry *dentry,
			       const char *old_name)
{
	struct path path = { .mnt = parent->mnt, .dentry = dentry };

	return tomoyo_path_perm(TOMOYO_TYPE_SYMLINK, &path, old_name);
}

 
static int tomoyo_path_mknod(const struct path *parent, struct dentry *dentry,
			     umode_t mode, unsigned int dev)
{
	struct path path = { .mnt = parent->mnt, .dentry = dentry };
	int type = TOMOYO_TYPE_CREATE;
	const unsigned int perm = mode & S_IALLUGO;

	switch (mode & S_IFMT) {
	case S_IFCHR:
		type = TOMOYO_TYPE_MKCHAR;
		break;
	case S_IFBLK:
		type = TOMOYO_TYPE_MKBLOCK;
		break;
	default:
		goto no_dev;
	}
	return tomoyo_mkdev_perm(type, &path, perm, dev);
 no_dev:
	switch (mode & S_IFMT) {
	case S_IFIFO:
		type = TOMOYO_TYPE_MKFIFO;
		break;
	case S_IFSOCK:
		type = TOMOYO_TYPE_MKSOCK;
		break;
	}
	return tomoyo_path_number_perm(type, &path, perm);
}

 
static int tomoyo_path_link(struct dentry *old_dentry, const struct path *new_dir,
			    struct dentry *new_dentry)
{
	struct path path1 = { .mnt = new_dir->mnt, .dentry = old_dentry };
	struct path path2 = { .mnt = new_dir->mnt, .dentry = new_dentry };

	return tomoyo_path2_perm(TOMOYO_TYPE_LINK, &path1, &path2);
}

 
static int tomoyo_path_rename(const struct path *old_parent,
			      struct dentry *old_dentry,
			      const struct path *new_parent,
			      struct dentry *new_dentry,
			      const unsigned int flags)
{
	struct path path1 = { .mnt = old_parent->mnt, .dentry = old_dentry };
	struct path path2 = { .mnt = new_parent->mnt, .dentry = new_dentry };

	if (flags & RENAME_EXCHANGE) {
		const int err = tomoyo_path2_perm(TOMOYO_TYPE_RENAME, &path2,
				&path1);

		if (err)
			return err;
	}
	return tomoyo_path2_perm(TOMOYO_TYPE_RENAME, &path1, &path2);
}

 
static int tomoyo_file_fcntl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	if (!(cmd == F_SETFL && ((arg ^ file->f_flags) & O_APPEND)))
		return 0;
	return tomoyo_check_open_permission(tomoyo_domain(), &file->f_path,
					    O_WRONLY | (arg & O_APPEND));
}

 
static int tomoyo_file_open(struct file *f)
{
	 
	if (current->in_execve)
		return 0;
	return tomoyo_check_open_permission(tomoyo_domain(), &f->f_path,
					    f->f_flags);
}

 
static int tomoyo_file_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	return tomoyo_path_number_perm(TOMOYO_TYPE_IOCTL, &file->f_path, cmd);
}

 
static int tomoyo_path_chmod(const struct path *path, umode_t mode)
{
	return tomoyo_path_number_perm(TOMOYO_TYPE_CHMOD, path,
				       mode & S_IALLUGO);
}

 
static int tomoyo_path_chown(const struct path *path, kuid_t uid, kgid_t gid)
{
	int error = 0;

	if (uid_valid(uid))
		error = tomoyo_path_number_perm(TOMOYO_TYPE_CHOWN, path,
						from_kuid(&init_user_ns, uid));
	if (!error && gid_valid(gid))
		error = tomoyo_path_number_perm(TOMOYO_TYPE_CHGRP, path,
						from_kgid(&init_user_ns, gid));
	return error;
}

 
static int tomoyo_path_chroot(const struct path *path)
{
	return tomoyo_path_perm(TOMOYO_TYPE_CHROOT, path, NULL);
}

 
static int tomoyo_sb_mount(const char *dev_name, const struct path *path,
			   const char *type, unsigned long flags, void *data)
{
	return tomoyo_mount_permission(dev_name, path, type, flags, data);
}

 
static int tomoyo_sb_umount(struct vfsmount *mnt, int flags)
{
	struct path path = { .mnt = mnt, .dentry = mnt->mnt_root };

	return tomoyo_path_perm(TOMOYO_TYPE_UMOUNT, &path, NULL);
}

 
static int tomoyo_sb_pivotroot(const struct path *old_path, const struct path *new_path)
{
	return tomoyo_path2_perm(TOMOYO_TYPE_PIVOT_ROOT, new_path, old_path);
}

 
static int tomoyo_socket_listen(struct socket *sock, int backlog)
{
	return tomoyo_socket_listen_permission(sock);
}

 
static int tomoyo_socket_connect(struct socket *sock, struct sockaddr *addr,
				 int addr_len)
{
	return tomoyo_socket_connect_permission(sock, addr, addr_len);
}

 
static int tomoyo_socket_bind(struct socket *sock, struct sockaddr *addr,
			      int addr_len)
{
	return tomoyo_socket_bind_permission(sock, addr, addr_len);
}

 
static int tomoyo_socket_sendmsg(struct socket *sock, struct msghdr *msg,
				 int size)
{
	return tomoyo_socket_sendmsg_permission(sock, msg, size);
}

struct lsm_blob_sizes tomoyo_blob_sizes __ro_after_init = {
	.lbs_task = sizeof(struct tomoyo_task),
};

 
static int tomoyo_task_alloc(struct task_struct *task,
			     unsigned long clone_flags)
{
	struct tomoyo_task *old = tomoyo_task(current);
	struct tomoyo_task *new = tomoyo_task(task);

	new->domain_info = old->domain_info;
	atomic_inc(&new->domain_info->users);
	new->old_domain_info = NULL;
	return 0;
}

 
static void tomoyo_task_free(struct task_struct *task)
{
	struct tomoyo_task *s = tomoyo_task(task);

	if (s->domain_info) {
		atomic_dec(&s->domain_info->users);
		s->domain_info = NULL;
	}
	if (s->old_domain_info) {
		atomic_dec(&s->old_domain_info->users);
		s->old_domain_info = NULL;
	}
}

 
static struct security_hook_list tomoyo_hooks[] __ro_after_init = {
	LSM_HOOK_INIT(cred_prepare, tomoyo_cred_prepare),
	LSM_HOOK_INIT(bprm_committed_creds, tomoyo_bprm_committed_creds),
	LSM_HOOK_INIT(task_alloc, tomoyo_task_alloc),
	LSM_HOOK_INIT(task_free, tomoyo_task_free),
#ifndef CONFIG_SECURITY_TOMOYO_OMIT_USERSPACE_LOADER
	LSM_HOOK_INIT(bprm_creds_for_exec, tomoyo_bprm_creds_for_exec),
#endif
	LSM_HOOK_INIT(bprm_check_security, tomoyo_bprm_check_security),
	LSM_HOOK_INIT(file_fcntl, tomoyo_file_fcntl),
	LSM_HOOK_INIT(file_open, tomoyo_file_open),
	LSM_HOOK_INIT(file_truncate, tomoyo_file_truncate),
	LSM_HOOK_INIT(path_truncate, tomoyo_path_truncate),
	LSM_HOOK_INIT(path_unlink, tomoyo_path_unlink),
	LSM_HOOK_INIT(path_mkdir, tomoyo_path_mkdir),
	LSM_HOOK_INIT(path_rmdir, tomoyo_path_rmdir),
	LSM_HOOK_INIT(path_symlink, tomoyo_path_symlink),
	LSM_HOOK_INIT(path_mknod, tomoyo_path_mknod),
	LSM_HOOK_INIT(path_link, tomoyo_path_link),
	LSM_HOOK_INIT(path_rename, tomoyo_path_rename),
	LSM_HOOK_INIT(inode_getattr, tomoyo_inode_getattr),
	LSM_HOOK_INIT(file_ioctl, tomoyo_file_ioctl),
	LSM_HOOK_INIT(path_chmod, tomoyo_path_chmod),
	LSM_HOOK_INIT(path_chown, tomoyo_path_chown),
	LSM_HOOK_INIT(path_chroot, tomoyo_path_chroot),
	LSM_HOOK_INIT(sb_mount, tomoyo_sb_mount),
	LSM_HOOK_INIT(sb_umount, tomoyo_sb_umount),
	LSM_HOOK_INIT(sb_pivotroot, tomoyo_sb_pivotroot),
	LSM_HOOK_INIT(socket_bind, tomoyo_socket_bind),
	LSM_HOOK_INIT(socket_connect, tomoyo_socket_connect),
	LSM_HOOK_INIT(socket_listen, tomoyo_socket_listen),
	LSM_HOOK_INIT(socket_sendmsg, tomoyo_socket_sendmsg),
};

 
DEFINE_SRCU(tomoyo_ss);

int tomoyo_enabled __ro_after_init = 1;

 
static int __init tomoyo_init(void)
{
	struct tomoyo_task *s = tomoyo_task(current);

	 
	security_add_hooks(tomoyo_hooks, ARRAY_SIZE(tomoyo_hooks), "tomoyo");
	pr_info("TOMOYO Linux initialized\n");
	s->domain_info = &tomoyo_kernel_domain;
	atomic_inc(&tomoyo_kernel_domain.users);
	s->old_domain_info = NULL;
	tomoyo_mm_init();

	return 0;
}

DEFINE_LSM(tomoyo) = {
	.name = "tomoyo",
	.enabled = &tomoyo_enabled,
	.flags = LSM_FLAG_LEGACY_MAJOR,
	.blobs = &tomoyo_blob_sizes,
	.init = tomoyo_init,
};
