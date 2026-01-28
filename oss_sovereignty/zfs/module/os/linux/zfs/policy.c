#include <sys/policy.h>
#include <linux/security.h>
#include <linux/vfs_compat.h>
static int
priv_policy_ns(const cred_t *cr, int capability, int err,
    struct user_namespace *ns)
{
	if (cr != CRED() && (cr != kcred))
		return (err);
#if defined(CONFIG_USER_NS)
	if (!(ns ? ns_capable(ns, capability) : capable(capability)))
#else
	if (!capable(capability))
#endif
		return (err);
	return (0);
}
static int
priv_policy(const cred_t *cr, int capability, int err)
{
	return (priv_policy_ns(cr, capability, err, cr->user_ns));
}
static int
priv_policy_user(const cred_t *cr, int capability, int err)
{
#if defined(CONFIG_USER_NS)
	return (priv_policy_ns(cr, capability, err, cr->user_ns));
#else
	return (priv_policy_ns(cr, capability, err, NULL));
#endif
}
int
secpolicy_nfs(const cred_t *cr)
{
	return (priv_policy(cr, CAP_SYS_ADMIN, EPERM));
}
int
secpolicy_sys_config(const cred_t *cr, boolean_t checkonly)
{
	return (priv_policy(cr, CAP_SYS_ADMIN, EPERM));
}
int
secpolicy_vnode_access2(const cred_t *cr, struct inode *ip, uid_t owner,
    mode_t curmode, mode_t wantmode)
{
	return (0);
}
int
secpolicy_vnode_any_access(const cred_t *cr, struct inode *ip, uid_t owner)
{
	if (crgetuid(cr) == owner)
		return (0);
	if (zpl_inode_owner_or_capable(zfs_init_idmap, ip))
		return (0);
#if defined(CONFIG_USER_NS)
	if (!kuid_has_mapping(cr->user_ns, SUID_TO_KUID(owner)))
		return (EPERM);
#endif
	if (priv_policy_user(cr, CAP_DAC_OVERRIDE, EPERM) == 0)
		return (0);
	if (priv_policy_user(cr, CAP_DAC_READ_SEARCH, EPERM) == 0)
		return (0);
	return (EPERM);
}
int
secpolicy_vnode_chown(const cred_t *cr, uid_t owner)
{
	if (crgetuid(cr) == owner)
		return (0);
#if defined(CONFIG_USER_NS)
	if (!kuid_has_mapping(cr->user_ns, SUID_TO_KUID(owner)))
		return (EPERM);
#endif
	return (priv_policy_user(cr, CAP_FOWNER, EPERM));
}
int
secpolicy_vnode_create_gid(const cred_t *cr)
{
	return (priv_policy(cr, CAP_SETGID, EPERM));
}
int
secpolicy_vnode_remove(const cred_t *cr)
{
	return (priv_policy(cr, CAP_FOWNER, EPERM));
}
int
secpolicy_vnode_setdac(const cred_t *cr, uid_t owner)
{
	if (crgetuid(cr) == owner)
		return (0);
#if defined(CONFIG_USER_NS)
	if (!kuid_has_mapping(cr->user_ns, SUID_TO_KUID(owner)))
		return (EPERM);
#endif
	return (priv_policy_user(cr, CAP_FOWNER, EPERM));
}
int
secpolicy_vnode_setid_retain(struct znode *zp __maybe_unused, const cred_t *cr,
    boolean_t issuidroot)
{
	return (priv_policy_user(cr, CAP_FSETID, EPERM));
}
int
secpolicy_vnode_setids_setgids(const cred_t *cr, gid_t gid, zidmap_t *mnt_ns,
    struct user_namespace *fs_ns)
{
	gid = zfs_gid_to_vfsgid(mnt_ns, fs_ns, gid);
#if defined(CONFIG_USER_NS)
	if (!kgid_has_mapping(cr->user_ns, SGID_TO_KGID(gid)))
		return (EPERM);
#endif
	if (crgetgid(cr) != gid && !groupmember(gid, cr))
		return (priv_policy_user(cr, CAP_FSETID, EPERM));
	return (0);
}
int
secpolicy_zinject(const cred_t *cr)
{
	return (priv_policy(cr, CAP_SYS_ADMIN, EACCES));
}
int
secpolicy_zfs(const cred_t *cr)
{
	return (priv_policy(cr, CAP_SYS_ADMIN, EACCES));
}
int
secpolicy_zfs_proc(const cred_t *cr, proc_t *proc)
{
#if defined(HAVE_HAS_CAPABILITY)
	if (!has_capability(proc, CAP_SYS_ADMIN))
		return (EACCES);
	return (0);
#else
	return (EACCES);
#endif
}
void
secpolicy_setid_clear(vattr_t *vap, cred_t *cr)
{
	if ((vap->va_mode & (S_ISUID | S_ISGID)) != 0 &&
	    secpolicy_vnode_setid_retain(NULL, cr,
	    (vap->va_mode & S_ISUID) != 0 &&
	    (vap->va_mask & AT_UID) != 0 && vap->va_uid == 0) != 0) {
		vap->va_mask |= AT_MODE;
		vap->va_mode &= ~(S_ISUID|S_ISGID);
	}
}
static int
secpolicy_vnode_setid_modify(const cred_t *cr, uid_t owner, zidmap_t *mnt_ns,
    struct user_namespace *fs_ns)
{
	owner = zfs_uid_to_vfsuid(mnt_ns, fs_ns, owner);
	if (crgetuid(cr) == owner)
		return (0);
#if defined(CONFIG_USER_NS)
	if (!kuid_has_mapping(cr->user_ns, SUID_TO_KUID(owner)))
		return (EPERM);
#endif
	return (priv_policy_user(cr, CAP_FSETID, EPERM));
}
static int
secpolicy_vnode_stky_modify(const cred_t *cr)
{
	return (0);
}
int
secpolicy_setid_setsticky_clear(struct inode *ip, vattr_t *vap,
    const vattr_t *ovap, cred_t *cr, zidmap_t *mnt_ns,
    struct user_namespace *fs_ns)
{
	int error;
	if ((vap->va_mode & S_ISUID) != 0 &&
	    (error = secpolicy_vnode_setid_modify(cr,
	    ovap->va_uid, mnt_ns, fs_ns)) != 0) {
		return (error);
	}
	if (!S_ISDIR(ip->i_mode) && (vap->va_mode & S_ISVTX) != 0 &&
	    secpolicy_vnode_stky_modify(cr) != 0) {
		vap->va_mode &= ~S_ISVTX;
	}
	if ((vap->va_mode & S_ISGID) != 0 &&
	    secpolicy_vnode_setids_setgids(cr, ovap->va_gid,
	    mnt_ns, fs_ns) != 0) {
		vap->va_mode &= ~S_ISGID;
	}
	return (0);
}
int
secpolicy_xvattr(xvattr_t *xvap, uid_t owner, cred_t *cr, mode_t type)
{
	return (secpolicy_vnode_chown(cr, owner));
}
int
secpolicy_vnode_setattr(cred_t *cr, struct inode *ip, struct vattr *vap,
    const struct vattr *ovap, int flags,
    int unlocked_access(void *, int, cred_t *), void *node)
{
	return (0);
}
int
secpolicy_basic_link(const cred_t *cr)
{
	return (0);
}
