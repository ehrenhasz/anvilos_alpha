
 

#include <linux/capability.h>
#include <linux/audit.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/lsm_hooks.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/ptrace.h>
#include <linux/xattr.h>
#include <linux/hugetlb.h>
#include <linux/mount.h>
#include <linux/sched.h>
#include <linux/prctl.h>
#include <linux/securebits.h>
#include <linux/user_namespace.h>
#include <linux/binfmts.h>
#include <linux/personality.h>
#include <linux/mnt_idmapping.h>

 
static void warn_setuid_and_fcaps_mixed(const char *fname)
{
	static int warned;
	if (!warned) {
		printk(KERN_INFO "warning: `%s' has both setuid-root and"
			" effective capabilities. Therefore not raising all"
			" capabilities.\n", fname);
		warned = 1;
	}
}

 
int cap_capable(const struct cred *cred, struct user_namespace *targ_ns,
		int cap, unsigned int opts)
{
	struct user_namespace *ns = targ_ns;

	 
	for (;;) {
		 
		if (ns == cred->user_ns)
			return cap_raised(cred->cap_effective, cap) ? 0 : -EPERM;

		 
		if (ns->level <= cred->user_ns->level)
			return -EPERM;

		 
		if ((ns->parent == cred->user_ns) && uid_eq(ns->owner, cred->euid))
			return 0;

		 
		ns = ns->parent;
	}

	 
}

 
int cap_settime(const struct timespec64 *ts, const struct timezone *tz)
{
	if (!capable(CAP_SYS_TIME))
		return -EPERM;
	return 0;
}

 
int cap_ptrace_access_check(struct task_struct *child, unsigned int mode)
{
	int ret = 0;
	const struct cred *cred, *child_cred;
	const kernel_cap_t *caller_caps;

	rcu_read_lock();
	cred = current_cred();
	child_cred = __task_cred(child);
	if (mode & PTRACE_MODE_FSCREDS)
		caller_caps = &cred->cap_effective;
	else
		caller_caps = &cred->cap_permitted;
	if (cred->user_ns == child_cred->user_ns &&
	    cap_issubset(child_cred->cap_permitted, *caller_caps))
		goto out;
	if (ns_capable(child_cred->user_ns, CAP_SYS_PTRACE))
		goto out;
	ret = -EPERM;
out:
	rcu_read_unlock();
	return ret;
}

 
int cap_ptrace_traceme(struct task_struct *parent)
{
	int ret = 0;
	const struct cred *cred, *child_cred;

	rcu_read_lock();
	cred = __task_cred(parent);
	child_cred = current_cred();
	if (cred->user_ns == child_cred->user_ns &&
	    cap_issubset(child_cred->cap_permitted, cred->cap_permitted))
		goto out;
	if (has_ns_capability(parent, child_cred->user_ns, CAP_SYS_PTRACE))
		goto out;
	ret = -EPERM;
out:
	rcu_read_unlock();
	return ret;
}

 
int cap_capget(const struct task_struct *target, kernel_cap_t *effective,
	       kernel_cap_t *inheritable, kernel_cap_t *permitted)
{
	const struct cred *cred;

	 
	rcu_read_lock();
	cred = __task_cred(target);
	*effective   = cred->cap_effective;
	*inheritable = cred->cap_inheritable;
	*permitted   = cred->cap_permitted;
	rcu_read_unlock();
	return 0;
}

 
static inline int cap_inh_is_capped(void)
{
	 
	if (cap_capable(current_cred(), current_cred()->user_ns,
			CAP_SETPCAP, CAP_OPT_NONE) == 0)
		return 0;
	return 1;
}

 
int cap_capset(struct cred *new,
	       const struct cred *old,
	       const kernel_cap_t *effective,
	       const kernel_cap_t *inheritable,
	       const kernel_cap_t *permitted)
{
	if (cap_inh_is_capped() &&
	    !cap_issubset(*inheritable,
			  cap_combine(old->cap_inheritable,
				      old->cap_permitted)))
		 
		return -EPERM;

	if (!cap_issubset(*inheritable,
			  cap_combine(old->cap_inheritable,
				      old->cap_bset)))
		 
		return -EPERM;

	 
	if (!cap_issubset(*permitted, old->cap_permitted))
		return -EPERM;

	 
	if (!cap_issubset(*effective, *permitted))
		return -EPERM;

	new->cap_effective   = *effective;
	new->cap_inheritable = *inheritable;
	new->cap_permitted   = *permitted;

	 
	new->cap_ambient = cap_intersect(new->cap_ambient,
					 cap_intersect(*permitted,
						       *inheritable));
	if (WARN_ON(!cap_ambient_invariant_ok(new)))
		return -EINVAL;
	return 0;
}

 
int cap_inode_need_killpriv(struct dentry *dentry)
{
	struct inode *inode = d_backing_inode(dentry);
	int error;

	error = __vfs_getxattr(dentry, inode, XATTR_NAME_CAPS, NULL, 0);
	return error > 0;
}

 
int cap_inode_killpriv(struct mnt_idmap *idmap, struct dentry *dentry)
{
	int error;

	error = __vfs_removexattr(idmap, dentry, XATTR_NAME_CAPS);
	if (error == -EOPNOTSUPP)
		error = 0;
	return error;
}

static bool rootid_owns_currentns(vfsuid_t rootvfsuid)
{
	struct user_namespace *ns;
	kuid_t kroot;

	if (!vfsuid_valid(rootvfsuid))
		return false;

	kroot = vfsuid_into_kuid(rootvfsuid);
	for (ns = current_user_ns();; ns = ns->parent) {
		if (from_kuid(ns, kroot) == 0)
			return true;
		if (ns == &init_user_ns)
			break;
	}

	return false;
}

static __u32 sansflags(__u32 m)
{
	return m & ~VFS_CAP_FLAGS_EFFECTIVE;
}

static bool is_v2header(int size, const struct vfs_cap_data *cap)
{
	if (size != XATTR_CAPS_SZ_2)
		return false;
	return sansflags(le32_to_cpu(cap->magic_etc)) == VFS_CAP_REVISION_2;
}

static bool is_v3header(int size, const struct vfs_cap_data *cap)
{
	if (size != XATTR_CAPS_SZ_3)
		return false;
	return sansflags(le32_to_cpu(cap->magic_etc)) == VFS_CAP_REVISION_3;
}

 
int cap_inode_getsecurity(struct mnt_idmap *idmap,
			  struct inode *inode, const char *name, void **buffer,
			  bool alloc)
{
	int size;
	kuid_t kroot;
	vfsuid_t vfsroot;
	u32 nsmagic, magic;
	uid_t root, mappedroot;
	char *tmpbuf = NULL;
	struct vfs_cap_data *cap;
	struct vfs_ns_cap_data *nscap = NULL;
	struct dentry *dentry;
	struct user_namespace *fs_ns;

	if (strcmp(name, "capability") != 0)
		return -EOPNOTSUPP;

	dentry = d_find_any_alias(inode);
	if (!dentry)
		return -EINVAL;
	size = vfs_getxattr_alloc(idmap, dentry, XATTR_NAME_CAPS, &tmpbuf,
				  sizeof(struct vfs_ns_cap_data), GFP_NOFS);
	dput(dentry);
	 
	if (size < 0 || !tmpbuf)
		goto out_free;

	fs_ns = inode->i_sb->s_user_ns;
	cap = (struct vfs_cap_data *) tmpbuf;
	if (is_v2header(size, cap)) {
		root = 0;
	} else if (is_v3header(size, cap)) {
		nscap = (struct vfs_ns_cap_data *) tmpbuf;
		root = le32_to_cpu(nscap->rootid);
	} else {
		size = -EINVAL;
		goto out_free;
	}

	kroot = make_kuid(fs_ns, root);

	 
	vfsroot = make_vfsuid(idmap, fs_ns, kroot);

	 
	mappedroot = from_kuid(current_user_ns(), vfsuid_into_kuid(vfsroot));
	if (mappedroot != (uid_t)-1 && mappedroot != (uid_t)0) {
		size = sizeof(struct vfs_ns_cap_data);
		if (alloc) {
			if (!nscap) {
				 
				nscap = kzalloc(size, GFP_ATOMIC);
				if (!nscap) {
					size = -ENOMEM;
					goto out_free;
				}
				nsmagic = VFS_CAP_REVISION_3;
				magic = le32_to_cpu(cap->magic_etc);
				if (magic & VFS_CAP_FLAGS_EFFECTIVE)
					nsmagic |= VFS_CAP_FLAGS_EFFECTIVE;
				memcpy(&nscap->data, &cap->data, sizeof(__le32) * 2 * VFS_CAP_U32);
				nscap->magic_etc = cpu_to_le32(nsmagic);
			} else {
				 
				tmpbuf = NULL;
			}
			nscap->rootid = cpu_to_le32(mappedroot);
			*buffer = nscap;
		}
		goto out_free;
	}

	if (!rootid_owns_currentns(vfsroot)) {
		size = -EOVERFLOW;
		goto out_free;
	}

	 
	size = sizeof(struct vfs_cap_data);
	if (alloc) {
		if (nscap) {
			 
			cap = kzalloc(size, GFP_ATOMIC);
			if (!cap) {
				size = -ENOMEM;
				goto out_free;
			}
			magic = VFS_CAP_REVISION_2;
			nsmagic = le32_to_cpu(nscap->magic_etc);
			if (nsmagic & VFS_CAP_FLAGS_EFFECTIVE)
				magic |= VFS_CAP_FLAGS_EFFECTIVE;
			memcpy(&cap->data, &nscap->data, sizeof(__le32) * 2 * VFS_CAP_U32);
			cap->magic_etc = cpu_to_le32(magic);
		} else {
			 
			tmpbuf = NULL;
		}
		*buffer = cap;
	}
out_free:
	kfree(tmpbuf);
	return size;
}

 
static vfsuid_t rootid_from_xattr(const void *value, size_t size,
				  struct user_namespace *task_ns)
{
	const struct vfs_ns_cap_data *nscap = value;
	uid_t rootid = 0;

	if (size == XATTR_CAPS_SZ_3)
		rootid = le32_to_cpu(nscap->rootid);

	return VFSUIDT_INIT(make_kuid(task_ns, rootid));
}

static bool validheader(size_t size, const struct vfs_cap_data *cap)
{
	return is_v2header(size, cap) || is_v3header(size, cap);
}

 
int cap_convert_nscap(struct mnt_idmap *idmap, struct dentry *dentry,
		      const void **ivalue, size_t size)
{
	struct vfs_ns_cap_data *nscap;
	uid_t nsrootid;
	const struct vfs_cap_data *cap = *ivalue;
	__u32 magic, nsmagic;
	struct inode *inode = d_backing_inode(dentry);
	struct user_namespace *task_ns = current_user_ns(),
		*fs_ns = inode->i_sb->s_user_ns;
	kuid_t rootid;
	vfsuid_t vfsrootid;
	size_t newsize;

	if (!*ivalue)
		return -EINVAL;
	if (!validheader(size, cap))
		return -EINVAL;
	if (!capable_wrt_inode_uidgid(idmap, inode, CAP_SETFCAP))
		return -EPERM;
	if (size == XATTR_CAPS_SZ_2 && (idmap == &nop_mnt_idmap))
		if (ns_capable(inode->i_sb->s_user_ns, CAP_SETFCAP))
			 
			return size;

	vfsrootid = rootid_from_xattr(*ivalue, size, task_ns);
	if (!vfsuid_valid(vfsrootid))
		return -EINVAL;

	rootid = from_vfsuid(idmap, fs_ns, vfsrootid);
	if (!uid_valid(rootid))
		return -EINVAL;

	nsrootid = from_kuid(fs_ns, rootid);
	if (nsrootid == -1)
		return -EINVAL;

	newsize = sizeof(struct vfs_ns_cap_data);
	nscap = kmalloc(newsize, GFP_ATOMIC);
	if (!nscap)
		return -ENOMEM;
	nscap->rootid = cpu_to_le32(nsrootid);
	nsmagic = VFS_CAP_REVISION_3;
	magic = le32_to_cpu(cap->magic_etc);
	if (magic & VFS_CAP_FLAGS_EFFECTIVE)
		nsmagic |= VFS_CAP_FLAGS_EFFECTIVE;
	nscap->magic_etc = cpu_to_le32(nsmagic);
	memcpy(&nscap->data, &cap->data, sizeof(__le32) * 2 * VFS_CAP_U32);

	*ivalue = nscap;
	return newsize;
}

 
static inline int bprm_caps_from_vfs_caps(struct cpu_vfs_cap_data *caps,
					  struct linux_binprm *bprm,
					  bool *effective,
					  bool *has_fcap)
{
	struct cred *new = bprm->cred;
	int ret = 0;

	if (caps->magic_etc & VFS_CAP_FLAGS_EFFECTIVE)
		*effective = true;

	if (caps->magic_etc & VFS_CAP_REVISION_MASK)
		*has_fcap = true;

	 
	new->cap_permitted.val =
		(new->cap_bset.val & caps->permitted.val) |
		(new->cap_inheritable.val & caps->inheritable.val);

	if (caps->permitted.val & ~new->cap_permitted.val)
		 
		ret = -EPERM;

	 
	return *effective ? ret : 0;
}

 
int get_vfs_caps_from_disk(struct mnt_idmap *idmap,
			   const struct dentry *dentry,
			   struct cpu_vfs_cap_data *cpu_caps)
{
	struct inode *inode = d_backing_inode(dentry);
	__u32 magic_etc;
	int size;
	struct vfs_ns_cap_data data, *nscaps = &data;
	struct vfs_cap_data *caps = (struct vfs_cap_data *) &data;
	kuid_t rootkuid;
	vfsuid_t rootvfsuid;
	struct user_namespace *fs_ns;

	memset(cpu_caps, 0, sizeof(struct cpu_vfs_cap_data));

	if (!inode)
		return -ENODATA;

	fs_ns = inode->i_sb->s_user_ns;
	size = __vfs_getxattr((struct dentry *)dentry, inode,
			      XATTR_NAME_CAPS, &data, XATTR_CAPS_SZ);
	if (size == -ENODATA || size == -EOPNOTSUPP)
		 
		return -ENODATA;

	if (size < 0)
		return size;

	if (size < sizeof(magic_etc))
		return -EINVAL;

	cpu_caps->magic_etc = magic_etc = le32_to_cpu(caps->magic_etc);

	rootkuid = make_kuid(fs_ns, 0);
	switch (magic_etc & VFS_CAP_REVISION_MASK) {
	case VFS_CAP_REVISION_1:
		if (size != XATTR_CAPS_SZ_1)
			return -EINVAL;
		break;
	case VFS_CAP_REVISION_2:
		if (size != XATTR_CAPS_SZ_2)
			return -EINVAL;
		break;
	case VFS_CAP_REVISION_3:
		if (size != XATTR_CAPS_SZ_3)
			return -EINVAL;
		rootkuid = make_kuid(fs_ns, le32_to_cpu(nscaps->rootid));
		break;

	default:
		return -EINVAL;
	}

	rootvfsuid = make_vfsuid(idmap, fs_ns, rootkuid);
	if (!vfsuid_valid(rootvfsuid))
		return -ENODATA;

	 
	if (!rootid_owns_currentns(rootvfsuid))
		return -ENODATA;

	cpu_caps->permitted.val = le32_to_cpu(caps->data[0].permitted);
	cpu_caps->inheritable.val = le32_to_cpu(caps->data[0].inheritable);

	 
	if ((magic_etc & VFS_CAP_REVISION_MASK) != VFS_CAP_REVISION_1) {
		cpu_caps->permitted.val += (u64)le32_to_cpu(caps->data[1].permitted) << 32;
		cpu_caps->inheritable.val += (u64)le32_to_cpu(caps->data[1].inheritable) << 32;
	}

	cpu_caps->permitted.val &= CAP_VALID_MASK;
	cpu_caps->inheritable.val &= CAP_VALID_MASK;

	cpu_caps->rootid = vfsuid_into_kuid(rootvfsuid);

	return 0;
}

 
static int get_file_caps(struct linux_binprm *bprm, struct file *file,
			 bool *effective, bool *has_fcap)
{
	int rc = 0;
	struct cpu_vfs_cap_data vcaps;

	cap_clear(bprm->cred->cap_permitted);

	if (!file_caps_enabled)
		return 0;

	if (!mnt_may_suid(file->f_path.mnt))
		return 0;

	 
	if (!current_in_userns(file->f_path.mnt->mnt_sb->s_user_ns))
		return 0;

	rc = get_vfs_caps_from_disk(file_mnt_idmap(file),
				    file->f_path.dentry, &vcaps);
	if (rc < 0) {
		if (rc == -EINVAL)
			printk(KERN_NOTICE "Invalid argument reading file caps for %s\n",
					bprm->filename);
		else if (rc == -ENODATA)
			rc = 0;
		goto out;
	}

	rc = bprm_caps_from_vfs_caps(&vcaps, bprm, effective, has_fcap);

out:
	if (rc)
		cap_clear(bprm->cred->cap_permitted);

	return rc;
}

static inline bool root_privileged(void) { return !issecure(SECURE_NOROOT); }

static inline bool __is_real(kuid_t uid, struct cred *cred)
{ return uid_eq(cred->uid, uid); }

static inline bool __is_eff(kuid_t uid, struct cred *cred)
{ return uid_eq(cred->euid, uid); }

static inline bool __is_suid(kuid_t uid, struct cred *cred)
{ return !__is_real(uid, cred) && __is_eff(uid, cred); }

 
static void handle_privileged_root(struct linux_binprm *bprm, bool has_fcap,
				   bool *effective, kuid_t root_uid)
{
	const struct cred *old = current_cred();
	struct cred *new = bprm->cred;

	if (!root_privileged())
		return;
	 
	if (has_fcap && __is_suid(root_uid, new)) {
		warn_setuid_and_fcaps_mixed(bprm->filename);
		return;
	}
	 
	if (__is_eff(root_uid, new) || __is_real(root_uid, new)) {
		 
		new->cap_permitted = cap_combine(old->cap_bset,
						 old->cap_inheritable);
	}
	 
	if (__is_eff(root_uid, new))
		*effective = true;
}

#define __cap_gained(field, target, source) \
	!cap_issubset(target->cap_##field, source->cap_##field)
#define __cap_grew(target, source, cred) \
	!cap_issubset(cred->cap_##target, cred->cap_##source)
#define __cap_full(field, cred) \
	cap_issubset(CAP_FULL_SET, cred->cap_##field)

static inline bool __is_setuid(struct cred *new, const struct cred *old)
{ return !uid_eq(new->euid, old->uid); }

static inline bool __is_setgid(struct cred *new, const struct cred *old)
{ return !gid_eq(new->egid, old->gid); }

 
static inline bool nonroot_raised_pE(struct cred *new, const struct cred *old,
				     kuid_t root, bool has_fcap)
{
	bool ret = false;

	if ((__cap_grew(effective, ambient, new) &&
	     !(__cap_full(effective, new) &&
	       (__is_eff(root, new) || __is_real(root, new)) &&
	       root_privileged())) ||
	    (root_privileged() &&
	     __is_suid(root, new) &&
	     !__cap_full(effective, new)) ||
	    (!__is_setuid(new, old) &&
	     ((has_fcap &&
	       __cap_gained(permitted, new, old)) ||
	      __cap_gained(ambient, new, old))))

		ret = true;

	return ret;
}

 
int cap_bprm_creds_from_file(struct linux_binprm *bprm, struct file *file)
{
	 
	const struct cred *old = current_cred();
	struct cred *new = bprm->cred;
	bool effective = false, has_fcap = false, is_setid;
	int ret;
	kuid_t root_uid;

	if (WARN_ON(!cap_ambient_invariant_ok(old)))
		return -EPERM;

	ret = get_file_caps(bprm, file, &effective, &has_fcap);
	if (ret < 0)
		return ret;

	root_uid = make_kuid(new->user_ns, 0);

	handle_privileged_root(bprm, has_fcap, &effective, root_uid);

	 
	if (__cap_gained(permitted, new, old))
		bprm->per_clear |= PER_CLEAR_ON_SETID;

	 
	is_setid = __is_setuid(new, old) || __is_setgid(new, old);

	if ((is_setid || __cap_gained(permitted, new, old)) &&
	    ((bprm->unsafe & ~LSM_UNSAFE_PTRACE) ||
	     !ptracer_capable(current, new->user_ns))) {
		 
		if (!ns_capable(new->user_ns, CAP_SETUID) ||
		    (bprm->unsafe & LSM_UNSAFE_NO_NEW_PRIVS)) {
			new->euid = new->uid;
			new->egid = new->gid;
		}
		new->cap_permitted = cap_intersect(new->cap_permitted,
						   old->cap_permitted);
	}

	new->suid = new->fsuid = new->euid;
	new->sgid = new->fsgid = new->egid;

	 
	if (has_fcap || is_setid)
		cap_clear(new->cap_ambient);

	 
	new->cap_permitted = cap_combine(new->cap_permitted, new->cap_ambient);

	 
	if (effective)
		new->cap_effective = new->cap_permitted;
	else
		new->cap_effective = new->cap_ambient;

	if (WARN_ON(!cap_ambient_invariant_ok(new)))
		return -EPERM;

	if (nonroot_raised_pE(new, old, root_uid, has_fcap)) {
		ret = audit_log_bprm_fcaps(bprm, new, old);
		if (ret < 0)
			return ret;
	}

	new->securebits &= ~issecure_mask(SECURE_KEEP_CAPS);

	if (WARN_ON(!cap_ambient_invariant_ok(new)))
		return -EPERM;

	 
	if (is_setid ||
	    (!__is_real(root_uid, new) &&
	     (effective ||
	      __cap_grew(permitted, ambient, new))))
		bprm->secureexec = 1;

	return 0;
}

 
int cap_inode_setxattr(struct dentry *dentry, const char *name,
		       const void *value, size_t size, int flags)
{
	struct user_namespace *user_ns = dentry->d_sb->s_user_ns;

	 
	if (strncmp(name, XATTR_SECURITY_PREFIX,
			XATTR_SECURITY_PREFIX_LEN) != 0)
		return 0;

	 
	if (strcmp(name, XATTR_NAME_CAPS) == 0)
		return 0;

	if (!ns_capable(user_ns, CAP_SYS_ADMIN))
		return -EPERM;
	return 0;
}

 
int cap_inode_removexattr(struct mnt_idmap *idmap,
			  struct dentry *dentry, const char *name)
{
	struct user_namespace *user_ns = dentry->d_sb->s_user_ns;

	 
	if (strncmp(name, XATTR_SECURITY_PREFIX,
			XATTR_SECURITY_PREFIX_LEN) != 0)
		return 0;

	if (strcmp(name, XATTR_NAME_CAPS) == 0) {
		 
		struct inode *inode = d_backing_inode(dentry);
		if (!inode)
			return -EINVAL;
		if (!capable_wrt_inode_uidgid(idmap, inode, CAP_SETFCAP))
			return -EPERM;
		return 0;
	}

	if (!ns_capable(user_ns, CAP_SYS_ADMIN))
		return -EPERM;
	return 0;
}

 
static inline void cap_emulate_setxuid(struct cred *new, const struct cred *old)
{
	kuid_t root_uid = make_kuid(old->user_ns, 0);

	if ((uid_eq(old->uid, root_uid) ||
	     uid_eq(old->euid, root_uid) ||
	     uid_eq(old->suid, root_uid)) &&
	    (!uid_eq(new->uid, root_uid) &&
	     !uid_eq(new->euid, root_uid) &&
	     !uid_eq(new->suid, root_uid))) {
		if (!issecure(SECURE_KEEP_CAPS)) {
			cap_clear(new->cap_permitted);
			cap_clear(new->cap_effective);
		}

		 
		cap_clear(new->cap_ambient);
	}
	if (uid_eq(old->euid, root_uid) && !uid_eq(new->euid, root_uid))
		cap_clear(new->cap_effective);
	if (!uid_eq(old->euid, root_uid) && uid_eq(new->euid, root_uid))
		new->cap_effective = new->cap_permitted;
}

 
int cap_task_fix_setuid(struct cred *new, const struct cred *old, int flags)
{
	switch (flags) {
	case LSM_SETID_RE:
	case LSM_SETID_ID:
	case LSM_SETID_RES:
		 
		if (!issecure(SECURE_NO_SETUID_FIXUP))
			cap_emulate_setxuid(new, old);
		break;

	case LSM_SETID_FS:
		 
		if (!issecure(SECURE_NO_SETUID_FIXUP)) {
			kuid_t root_uid = make_kuid(old->user_ns, 0);
			if (uid_eq(old->fsuid, root_uid) && !uid_eq(new->fsuid, root_uid))
				new->cap_effective =
					cap_drop_fs_set(new->cap_effective);

			if (!uid_eq(old->fsuid, root_uid) && uid_eq(new->fsuid, root_uid))
				new->cap_effective =
					cap_raise_fs_set(new->cap_effective,
							 new->cap_permitted);
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

 
static int cap_safe_nice(struct task_struct *p)
{
	int is_subset, ret = 0;

	rcu_read_lock();
	is_subset = cap_issubset(__task_cred(p)->cap_permitted,
				 current_cred()->cap_permitted);
	if (!is_subset && !ns_capable(__task_cred(p)->user_ns, CAP_SYS_NICE))
		ret = -EPERM;
	rcu_read_unlock();

	return ret;
}

 
int cap_task_setscheduler(struct task_struct *p)
{
	return cap_safe_nice(p);
}

 
int cap_task_setioprio(struct task_struct *p, int ioprio)
{
	return cap_safe_nice(p);
}

 
int cap_task_setnice(struct task_struct *p, int nice)
{
	return cap_safe_nice(p);
}

 
static int cap_prctl_drop(unsigned long cap)
{
	struct cred *new;

	if (!ns_capable(current_user_ns(), CAP_SETPCAP))
		return -EPERM;
	if (!cap_valid(cap))
		return -EINVAL;

	new = prepare_creds();
	if (!new)
		return -ENOMEM;
	cap_lower(new->cap_bset, cap);
	return commit_creds(new);
}

 
int cap_task_prctl(int option, unsigned long arg2, unsigned long arg3,
		   unsigned long arg4, unsigned long arg5)
{
	const struct cred *old = current_cred();
	struct cred *new;

	switch (option) {
	case PR_CAPBSET_READ:
		if (!cap_valid(arg2))
			return -EINVAL;
		return !!cap_raised(old->cap_bset, arg2);

	case PR_CAPBSET_DROP:
		return cap_prctl_drop(arg2);

	 
	case PR_SET_SECUREBITS:
		if ((((old->securebits & SECURE_ALL_LOCKS) >> 1)
		     & (old->securebits ^ arg2))			 
		    || ((old->securebits & SECURE_ALL_LOCKS & ~arg2))	 
		    || (arg2 & ~(SECURE_ALL_LOCKS | SECURE_ALL_BITS))	 
		    || (cap_capable(current_cred(),
				    current_cred()->user_ns,
				    CAP_SETPCAP,
				    CAP_OPT_NONE) != 0)			 
			 
		    )
			 
			return -EPERM;

		new = prepare_creds();
		if (!new)
			return -ENOMEM;
		new->securebits = arg2;
		return commit_creds(new);

	case PR_GET_SECUREBITS:
		return old->securebits;

	case PR_GET_KEEPCAPS:
		return !!issecure(SECURE_KEEP_CAPS);

	case PR_SET_KEEPCAPS:
		if (arg2 > 1)  
			return -EINVAL;
		if (issecure(SECURE_KEEP_CAPS_LOCKED))
			return -EPERM;

		new = prepare_creds();
		if (!new)
			return -ENOMEM;
		if (arg2)
			new->securebits |= issecure_mask(SECURE_KEEP_CAPS);
		else
			new->securebits &= ~issecure_mask(SECURE_KEEP_CAPS);
		return commit_creds(new);

	case PR_CAP_AMBIENT:
		if (arg2 == PR_CAP_AMBIENT_CLEAR_ALL) {
			if (arg3 | arg4 | arg5)
				return -EINVAL;

			new = prepare_creds();
			if (!new)
				return -ENOMEM;
			cap_clear(new->cap_ambient);
			return commit_creds(new);
		}

		if (((!cap_valid(arg3)) | arg4 | arg5))
			return -EINVAL;

		if (arg2 == PR_CAP_AMBIENT_IS_SET) {
			return !!cap_raised(current_cred()->cap_ambient, arg3);
		} else if (arg2 != PR_CAP_AMBIENT_RAISE &&
			   arg2 != PR_CAP_AMBIENT_LOWER) {
			return -EINVAL;
		} else {
			if (arg2 == PR_CAP_AMBIENT_RAISE &&
			    (!cap_raised(current_cred()->cap_permitted, arg3) ||
			     !cap_raised(current_cred()->cap_inheritable,
					 arg3) ||
			     issecure(SECURE_NO_CAP_AMBIENT_RAISE)))
				return -EPERM;

			new = prepare_creds();
			if (!new)
				return -ENOMEM;
			if (arg2 == PR_CAP_AMBIENT_RAISE)
				cap_raise(new->cap_ambient, arg3);
			else
				cap_lower(new->cap_ambient, arg3);
			return commit_creds(new);
		}

	default:
		 
		return -ENOSYS;
	}
}

 
int cap_vm_enough_memory(struct mm_struct *mm, long pages)
{
	int cap_sys_admin = 0;

	if (cap_capable(current_cred(), &init_user_ns,
				CAP_SYS_ADMIN, CAP_OPT_NOAUDIT) == 0)
		cap_sys_admin = 1;

	return cap_sys_admin;
}

 
int cap_mmap_addr(unsigned long addr)
{
	int ret = 0;

	if (addr < dac_mmap_min_addr) {
		ret = cap_capable(current_cred(), &init_user_ns, CAP_SYS_RAWIO,
				  CAP_OPT_NONE);
		 
		if (ret == 0)
			current->flags |= PF_SUPERPRIV;
	}
	return ret;
}

int cap_mmap_file(struct file *file, unsigned long reqprot,
		  unsigned long prot, unsigned long flags)
{
	return 0;
}

#ifdef CONFIG_SECURITY

static struct security_hook_list capability_hooks[] __ro_after_init = {
	LSM_HOOK_INIT(capable, cap_capable),
	LSM_HOOK_INIT(settime, cap_settime),
	LSM_HOOK_INIT(ptrace_access_check, cap_ptrace_access_check),
	LSM_HOOK_INIT(ptrace_traceme, cap_ptrace_traceme),
	LSM_HOOK_INIT(capget, cap_capget),
	LSM_HOOK_INIT(capset, cap_capset),
	LSM_HOOK_INIT(bprm_creds_from_file, cap_bprm_creds_from_file),
	LSM_HOOK_INIT(inode_need_killpriv, cap_inode_need_killpriv),
	LSM_HOOK_INIT(inode_killpriv, cap_inode_killpriv),
	LSM_HOOK_INIT(inode_getsecurity, cap_inode_getsecurity),
	LSM_HOOK_INIT(mmap_addr, cap_mmap_addr),
	LSM_HOOK_INIT(mmap_file, cap_mmap_file),
	LSM_HOOK_INIT(task_fix_setuid, cap_task_fix_setuid),
	LSM_HOOK_INIT(task_prctl, cap_task_prctl),
	LSM_HOOK_INIT(task_setscheduler, cap_task_setscheduler),
	LSM_HOOK_INIT(task_setioprio, cap_task_setioprio),
	LSM_HOOK_INIT(task_setnice, cap_task_setnice),
	LSM_HOOK_INIT(vm_enough_memory, cap_vm_enough_memory),
};

static int __init capability_init(void)
{
	security_add_hooks(capability_hooks, ARRAY_SIZE(capability_hooks),
				"capability");
	return 0;
}

DEFINE_LSM(capability) = {
	.name = "capability",
	.order = LSM_ORDER_FIRST,
	.init = capability_init,
};

#endif  
