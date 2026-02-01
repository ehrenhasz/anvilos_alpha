 
 

 

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/acl.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/mntent.h>
#include <sys/mount.h>
#include <sys/cmn_err.h>
#include <sys/zfs_znode.h>
#include <sys/zfs_vnops.h>
#include <sys/zfs_dir.h>
#include <sys/zil.h>
#include <sys/fs/zfs.h>
#include <sys/dmu.h>
#include <sys/dsl_prop.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_deleg.h>
#include <sys/spa.h>
#include <sys/zap.h>
#include <sys/sa.h>
#include <sys/sa_impl.h>
#include <sys/policy.h>
#include <sys/atomic.h>
#include <sys/zfs_ioctl.h>
#include <sys/zfs_ctldir.h>
#include <sys/zfs_fuid.h>
#include <sys/sunddi.h>
#include <sys/dmu_objset.h>
#include <sys/dsl_dir.h>
#include <sys/jail.h>
#include <sys/osd.h>
#include <ufs/ufs/quota.h>
#include <sys/zfs_quota.h>

#include "zfs_comutil.h"

#ifndef	MNTK_VMSETSIZE_BUG
#define	MNTK_VMSETSIZE_BUG	0
#endif
#ifndef	MNTK_NOMSYNC
#define	MNTK_NOMSYNC	8
#endif

struct mtx zfs_debug_mtx;
MTX_SYSINIT(zfs_debug_mtx, &zfs_debug_mtx, "zfs_debug", MTX_DEF);

SYSCTL_NODE(_vfs, OID_AUTO, zfs, CTLFLAG_RW, 0, "ZFS file system");

int zfs_super_owner;
SYSCTL_INT(_vfs_zfs, OID_AUTO, super_owner, CTLFLAG_RW, &zfs_super_owner, 0,
	"File system owners can perform privileged operation on file systems");

int zfs_debug_level;
SYSCTL_INT(_vfs_zfs, OID_AUTO, debug, CTLFLAG_RWTUN, &zfs_debug_level, 0,
	"Debug level");

int zfs_bclone_enabled = 0;
SYSCTL_INT(_vfs_zfs, OID_AUTO, bclone_enabled, CTLFLAG_RWTUN,
	&zfs_bclone_enabled, 0, "Enable block cloning");

struct zfs_jailparam {
	int mount_snapshot;
};

static struct zfs_jailparam zfs_jailparam0 = {
	.mount_snapshot = 0,
};

static int zfs_jailparam_slot;

SYSCTL_JAIL_PARAM_SYS_NODE(zfs, CTLFLAG_RW, "Jail ZFS parameters");
SYSCTL_JAIL_PARAM(_zfs, mount_snapshot, CTLTYPE_INT | CTLFLAG_RW, "I",
	"Allow mounting snapshots in the .zfs directory for unjailed datasets");

SYSCTL_NODE(_vfs_zfs, OID_AUTO, version, CTLFLAG_RD, 0, "ZFS versions");
static int zfs_version_acl = ZFS_ACL_VERSION;
SYSCTL_INT(_vfs_zfs_version, OID_AUTO, acl, CTLFLAG_RD, &zfs_version_acl, 0,
	"ZFS_ACL_VERSION");
static int zfs_version_spa = SPA_VERSION;
SYSCTL_INT(_vfs_zfs_version, OID_AUTO, spa, CTLFLAG_RD, &zfs_version_spa, 0,
	"SPA_VERSION");
static int zfs_version_zpl = ZPL_VERSION;
SYSCTL_INT(_vfs_zfs_version, OID_AUTO, zpl, CTLFLAG_RD, &zfs_version_zpl, 0,
	"ZPL_VERSION");

#if __FreeBSD_version >= 1400018
static int zfs_quotactl(vfs_t *vfsp, int cmds, uid_t id, void *arg,
    bool *mp_busy);
#else
static int zfs_quotactl(vfs_t *vfsp, int cmds, uid_t id, void *arg);
#endif
static int zfs_mount(vfs_t *vfsp);
static int zfs_umount(vfs_t *vfsp, int fflag);
static int zfs_root(vfs_t *vfsp, int flags, vnode_t **vpp);
static int zfs_statfs(vfs_t *vfsp, struct statfs *statp);
static int zfs_vget(vfs_t *vfsp, ino_t ino, int flags, vnode_t **vpp);
static int zfs_sync(vfs_t *vfsp, int waitfor);
#if __FreeBSD_version >= 1300098
static int zfs_checkexp(vfs_t *vfsp, struct sockaddr *nam, uint64_t *extflagsp,
    struct ucred **credanonp, int *numsecflavors, int *secflavors);
#else
static int zfs_checkexp(vfs_t *vfsp, struct sockaddr *nam, int *extflagsp,
    struct ucred **credanonp, int *numsecflavors, int **secflavors);
#endif
static int zfs_fhtovp(vfs_t *vfsp, fid_t *fidp, int flags, vnode_t **vpp);
static void zfs_freevfs(vfs_t *vfsp);

struct vfsops zfs_vfsops = {
	.vfs_mount =		zfs_mount,
	.vfs_unmount =		zfs_umount,
#if __FreeBSD_version >= 1300049
	.vfs_root =		vfs_cache_root,
	.vfs_cachedroot = zfs_root,
#else
	.vfs_root =		zfs_root,
#endif
	.vfs_statfs =		zfs_statfs,
	.vfs_vget =		zfs_vget,
	.vfs_sync =		zfs_sync,
	.vfs_checkexp =		zfs_checkexp,
	.vfs_fhtovp =		zfs_fhtovp,
	.vfs_quotactl =		zfs_quotactl,
};

#ifdef VFCF_CROSS_COPY_FILE_RANGE
VFS_SET(zfs_vfsops, zfs,
    VFCF_DELEGADMIN | VFCF_JAIL | VFCF_CROSS_COPY_FILE_RANGE);
#else
VFS_SET(zfs_vfsops, zfs, VFCF_DELEGADMIN | VFCF_JAIL);
#endif

 
static uint32_t	zfs_active_fs_count = 0;

int
zfs_get_temporary_prop(dsl_dataset_t *ds, zfs_prop_t zfs_prop, uint64_t *val,
    char *setpoint)
{
	int error;
	zfsvfs_t *zfvp;
	vfs_t *vfsp;
	objset_t *os;
	uint64_t tmp = *val;

	error = dmu_objset_from_ds(ds, &os);
	if (error != 0)
		return (error);

	error = getzfsvfs_impl(os, &zfvp);
	if (error != 0)
		return (error);
	if (zfvp == NULL)
		return (ENOENT);
	vfsp = zfvp->z_vfs;
	switch (zfs_prop) {
	case ZFS_PROP_ATIME:
		if (vfs_optionisset(vfsp, MNTOPT_NOATIME, NULL))
			tmp = 0;
		if (vfs_optionisset(vfsp, MNTOPT_ATIME, NULL))
			tmp = 1;
		break;
	case ZFS_PROP_DEVICES:
		if (vfs_optionisset(vfsp, MNTOPT_NODEVICES, NULL))
			tmp = 0;
		if (vfs_optionisset(vfsp, MNTOPT_DEVICES, NULL))
			tmp = 1;
		break;
	case ZFS_PROP_EXEC:
		if (vfs_optionisset(vfsp, MNTOPT_NOEXEC, NULL))
			tmp = 0;
		if (vfs_optionisset(vfsp, MNTOPT_EXEC, NULL))
			tmp = 1;
		break;
	case ZFS_PROP_SETUID:
		if (vfs_optionisset(vfsp, MNTOPT_NOSETUID, NULL))
			tmp = 0;
		if (vfs_optionisset(vfsp, MNTOPT_SETUID, NULL))
			tmp = 1;
		break;
	case ZFS_PROP_READONLY:
		if (vfs_optionisset(vfsp, MNTOPT_RW, NULL))
			tmp = 0;
		if (vfs_optionisset(vfsp, MNTOPT_RO, NULL))
			tmp = 1;
		break;
	case ZFS_PROP_XATTR:
		if (zfvp->z_flags & ZSB_XATTR)
			tmp = zfvp->z_xattr;
		break;
	case ZFS_PROP_NBMAND:
		if (vfs_optionisset(vfsp, MNTOPT_NONBMAND, NULL))
			tmp = 0;
		if (vfs_optionisset(vfsp, MNTOPT_NBMAND, NULL))
			tmp = 1;
		break;
	default:
		vfs_unbusy(vfsp);
		return (ENOENT);
	}

	vfs_unbusy(vfsp);
	if (tmp != *val) {
		if (setpoint)
			(void) strcpy(setpoint, "temporary");
		*val = tmp;
	}
	return (0);
}

static int
zfs_getquota(zfsvfs_t *zfsvfs, uid_t id, int isgroup, struct dqblk64 *dqp)
{
	int error = 0;
	char buf[32];
	uint64_t usedobj, quotaobj;
	uint64_t quota, used = 0;
	timespec_t now;

	usedobj = isgroup ? DMU_GROUPUSED_OBJECT : DMU_USERUSED_OBJECT;
	quotaobj = isgroup ? zfsvfs->z_groupquota_obj : zfsvfs->z_userquota_obj;

	if (quotaobj == 0 || zfsvfs->z_replay) {
		error = ENOENT;
		goto done;
	}
	(void) sprintf(buf, "%llx", (longlong_t)id);
	if ((error = zap_lookup(zfsvfs->z_os, quotaobj,
	    buf, sizeof (quota), 1, &quota)) != 0) {
		dprintf("%s(%d): quotaobj lookup failed\n",
		    __FUNCTION__, __LINE__);
		goto done;
	}
	 
	dqp->dqb_bsoftlimit = dqp->dqb_bhardlimit = btodb(quota);
	error = zap_lookup(zfsvfs->z_os, usedobj, buf, sizeof (used), 1, &used);
	if (error && error != ENOENT) {
		dprintf("%s(%d):  usedobj failed; %d\n",
		    __FUNCTION__, __LINE__, error);
		goto done;
	}
	dqp->dqb_curblocks = btodb(used);
	dqp->dqb_ihardlimit = dqp->dqb_isoftlimit = 0;
	vfs_timestamp(&now);
	 
	dqp->dqb_btime = dqp->dqb_itime = now.tv_sec;
done:
	return (error);
}

static int
#if __FreeBSD_version >= 1400018
zfs_quotactl(vfs_t *vfsp, int cmds, uid_t id, void *arg, bool *mp_busy)
#else
zfs_quotactl(vfs_t *vfsp, int cmds, uid_t id, void *arg)
#endif
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	struct thread *td;
	int cmd, type, error = 0;
	int bitsize;
	zfs_userquota_prop_t quota_type;
	struct dqblk64 dqblk = { 0 };

	td = curthread;
	cmd = cmds >> SUBCMDSHIFT;
	type = cmds & SUBCMDMASK;

	if ((error = zfs_enter(zfsvfs, FTAG)) != 0)
		return (error);
	if (id == -1) {
		switch (type) {
		case USRQUOTA:
			id = td->td_ucred->cr_ruid;
			break;
		case GRPQUOTA:
			id = td->td_ucred->cr_rgid;
			break;
		default:
			error = EINVAL;
#if __FreeBSD_version < 1400018
			if (cmd == Q_QUOTAON || cmd == Q_QUOTAOFF)
				vfs_unbusy(vfsp);
#endif
			goto done;
		}
	}
	 
	switch (cmd) {
	case Q_SETQUOTA:
	case Q_SETQUOTA32:
		if (type == USRQUOTA)
			quota_type = ZFS_PROP_USERQUOTA;
		else if (type == GRPQUOTA)
			quota_type = ZFS_PROP_GROUPQUOTA;
		else
			error = EINVAL;
		break;
	case Q_GETQUOTA:
	case Q_GETQUOTA32:
		if (type == USRQUOTA)
			quota_type = ZFS_PROP_USERUSED;
		else if (type == GRPQUOTA)
			quota_type = ZFS_PROP_GROUPUSED;
		else
			error = EINVAL;
		break;
	}

	 
	if ((uint32_t)type >= MAXQUOTAS) {
		error = EINVAL;
		goto done;
	}

	switch (cmd) {
	case Q_GETQUOTASIZE:
		bitsize = 64;
		error = copyout(&bitsize, arg, sizeof (int));
		break;
	case Q_QUOTAON:
		
		error = 0;
#if __FreeBSD_version < 1400018
		vfs_unbusy(vfsp);
#endif
		break;
	case Q_QUOTAOFF:
		error = ENOTSUP;
#if __FreeBSD_version < 1400018
		vfs_unbusy(vfsp);
#endif
		break;
	case Q_SETQUOTA:
		error = copyin(arg, &dqblk, sizeof (dqblk));
		if (error == 0)
			error = zfs_set_userquota(zfsvfs, quota_type,
			    "", id, dbtob(dqblk.dqb_bhardlimit));
		break;
	case Q_GETQUOTA:
		error = zfs_getquota(zfsvfs, id, type == GRPQUOTA, &dqblk);
		if (error == 0)
			error = copyout(&dqblk, arg, sizeof (dqblk));
		break;
	default:
		error = EINVAL;
		break;
	}
done:
	zfs_exit(zfsvfs, FTAG);
	return (error);
}


boolean_t
zfs_is_readonly(zfsvfs_t *zfsvfs)
{
	return (!!(zfsvfs->z_vfs->vfs_flag & VFS_RDONLY));
}

static int
zfs_sync(vfs_t *vfsp, int waitfor)
{

	 
	if (panicstr)
		return (0);

	 
	if (waitfor == MNT_LAZY)
		return (0);

	if (vfsp != NULL) {
		 
		zfsvfs_t *zfsvfs = vfsp->vfs_data;
		dsl_pool_t *dp;
		int error;

		if ((error = zfs_enter(zfsvfs, FTAG)) != 0)
			return (error);
		dp = dmu_objset_pool(zfsvfs->z_os);

		 
		if (rebooting && spa_suspended(dp->dp_spa)) {
			zfs_exit(zfsvfs, FTAG);
			return (0);
		}

		if (zfsvfs->z_log != NULL)
			zil_commit(zfsvfs->z_log, 0);

		zfs_exit(zfsvfs, FTAG);
	} else {
		 
		spa_sync_allpools();
	}

	return (0);
}

static void
atime_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval == TRUE) {
		zfsvfs->z_atime = TRUE;
		zfsvfs->z_vfs->vfs_flag &= ~MNT_NOATIME;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NOATIME);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_ATIME, NULL, 0);
	} else {
		zfsvfs->z_atime = FALSE;
		zfsvfs->z_vfs->vfs_flag |= MNT_NOATIME;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_ATIME);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NOATIME, NULL, 0);
	}
}

static void
xattr_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval == ZFS_XATTR_OFF) {
		zfsvfs->z_flags &= ~ZSB_XATTR;
	} else {
		zfsvfs->z_flags |= ZSB_XATTR;

		if (newval == ZFS_XATTR_SA)
			zfsvfs->z_xattr_sa = B_TRUE;
		else
			zfsvfs->z_xattr_sa = B_FALSE;
	}
}

static void
blksz_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;
	ASSERT3U(newval, <=, spa_maxblocksize(dmu_objset_spa(zfsvfs->z_os)));
	ASSERT3U(newval, >=, SPA_MINBLOCKSIZE);
	ASSERT(ISP2(newval));

	zfsvfs->z_max_blksz = newval;
	zfsvfs->z_vfs->mnt_stat.f_iosize = newval;
}

static void
readonly_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval) {
		 
		zfsvfs->z_vfs->vfs_flag |= VFS_RDONLY;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_RW);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_RO, NULL, 0);
	} else {
		 
		zfsvfs->z_vfs->vfs_flag &= ~VFS_RDONLY;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_RO);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_RW, NULL, 0);
	}
}

static void
setuid_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval == FALSE) {
		zfsvfs->z_vfs->vfs_flag |= VFS_NOSETUID;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_SETUID);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NOSETUID, NULL, 0);
	} else {
		zfsvfs->z_vfs->vfs_flag &= ~VFS_NOSETUID;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NOSETUID);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_SETUID, NULL, 0);
	}
}

static void
exec_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	if (newval == FALSE) {
		zfsvfs->z_vfs->vfs_flag |= VFS_NOEXEC;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_EXEC);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NOEXEC, NULL, 0);
	} else {
		zfsvfs->z_vfs->vfs_flag &= ~VFS_NOEXEC;
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NOEXEC);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_EXEC, NULL, 0);
	}
}

 
static void
nbmand_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;
	if (newval == FALSE) {
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NBMAND);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NONBMAND, NULL, 0);
	} else {
		vfs_clearmntopt(zfsvfs->z_vfs, MNTOPT_NONBMAND);
		vfs_setmntopt(zfsvfs->z_vfs, MNTOPT_NBMAND, NULL, 0);
	}
}

static void
snapdir_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	zfsvfs->z_show_ctldir = newval;
}

static void
acl_mode_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	zfsvfs->z_acl_mode = newval;
}

static void
acl_inherit_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	zfsvfs->z_acl_inherit = newval;
}

static void
acl_type_changed_cb(void *arg, uint64_t newval)
{
	zfsvfs_t *zfsvfs = arg;

	zfsvfs->z_acl_type = newval;
}

static int
zfs_register_callbacks(vfs_t *vfsp)
{
	struct dsl_dataset *ds = NULL;
	objset_t *os = NULL;
	zfsvfs_t *zfsvfs = NULL;
	uint64_t nbmand;
	boolean_t readonly = B_FALSE;
	boolean_t do_readonly = B_FALSE;
	boolean_t setuid = B_FALSE;
	boolean_t do_setuid = B_FALSE;
	boolean_t exec = B_FALSE;
	boolean_t do_exec = B_FALSE;
	boolean_t xattr = B_FALSE;
	boolean_t atime = B_FALSE;
	boolean_t do_atime = B_FALSE;
	boolean_t do_xattr = B_FALSE;
	int error = 0;

	ASSERT3P(vfsp, !=, NULL);
	zfsvfs = vfsp->vfs_data;
	ASSERT3P(zfsvfs, !=, NULL);
	os = zfsvfs->z_os;

	 
	if (dmu_objset_is_snapshot(os))
		return (EOPNOTSUPP);

	 
	if (vfs_optionisset(vfsp, MNTOPT_RO, NULL) ||
	    !spa_writeable(dmu_objset_spa(os))) {
		readonly = B_TRUE;
		do_readonly = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_RW, NULL)) {
		readonly = B_FALSE;
		do_readonly = B_TRUE;
	}
	if (vfs_optionisset(vfsp, MNTOPT_NOSETUID, NULL)) {
		setuid = B_FALSE;
		do_setuid = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_SETUID, NULL)) {
		setuid = B_TRUE;
		do_setuid = B_TRUE;
	}
	if (vfs_optionisset(vfsp, MNTOPT_NOEXEC, NULL)) {
		exec = B_FALSE;
		do_exec = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_EXEC, NULL)) {
		exec = B_TRUE;
		do_exec = B_TRUE;
	}
	if (vfs_optionisset(vfsp, MNTOPT_NOXATTR, NULL)) {
		zfsvfs->z_xattr = xattr = ZFS_XATTR_OFF;
		do_xattr = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_XATTR, NULL)) {
		zfsvfs->z_xattr = xattr = ZFS_XATTR_DIR;
		do_xattr = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_DIRXATTR, NULL)) {
		zfsvfs->z_xattr = xattr = ZFS_XATTR_DIR;
		do_xattr = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_SAXATTR, NULL)) {
		zfsvfs->z_xattr = xattr = ZFS_XATTR_SA;
		do_xattr = B_TRUE;
	}
	if (vfs_optionisset(vfsp, MNTOPT_NOATIME, NULL)) {
		atime = B_FALSE;
		do_atime = B_TRUE;
	} else if (vfs_optionisset(vfsp, MNTOPT_ATIME, NULL)) {
		atime = B_TRUE;
		do_atime = B_TRUE;
	}

	 
	ds = dmu_objset_ds(os);
	dsl_pool_config_enter(dmu_objset_pool(os), FTAG);

	 
	if (vfs_optionisset(vfsp, MNTOPT_NONBMAND, NULL)) {
		nbmand = B_FALSE;
	} else if (vfs_optionisset(vfsp, MNTOPT_NBMAND, NULL)) {
		nbmand = B_TRUE;
	} else if ((error = dsl_prop_get_int_ds(ds, "nbmand", &nbmand)) != 0) {
		dsl_pool_config_exit(dmu_objset_pool(os), FTAG);
		return (error);
	}

	 
	error = dsl_prop_register(ds,
	    zfs_prop_to_name(ZFS_PROP_ATIME), atime_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    zfs_prop_to_name(ZFS_PROP_XATTR), xattr_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    zfs_prop_to_name(ZFS_PROP_RECORDSIZE), blksz_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    zfs_prop_to_name(ZFS_PROP_READONLY), readonly_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    zfs_prop_to_name(ZFS_PROP_SETUID), setuid_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    zfs_prop_to_name(ZFS_PROP_EXEC), exec_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    zfs_prop_to_name(ZFS_PROP_SNAPDIR), snapdir_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    zfs_prop_to_name(ZFS_PROP_ACLTYPE), acl_type_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    zfs_prop_to_name(ZFS_PROP_ACLMODE), acl_mode_changed_cb, zfsvfs);
	error = error ? error : dsl_prop_register(ds,
	    zfs_prop_to_name(ZFS_PROP_ACLINHERIT), acl_inherit_changed_cb,
	    zfsvfs);
	dsl_pool_config_exit(dmu_objset_pool(os), FTAG);
	if (error)
		goto unregister;

	 
	if (do_readonly)
		readonly_changed_cb(zfsvfs, readonly);
	if (do_setuid)
		setuid_changed_cb(zfsvfs, setuid);
	if (do_exec)
		exec_changed_cb(zfsvfs, exec);
	if (do_xattr)
		xattr_changed_cb(zfsvfs, xattr);
	if (do_atime)
		atime_changed_cb(zfsvfs, atime);

	nbmand_changed_cb(zfsvfs, nbmand);

	return (0);

unregister:
	dsl_prop_unregister_all(ds, zfsvfs);
	return (error);
}

 
static int
zfsvfs_init(zfsvfs_t *zfsvfs, objset_t *os)
{
	int error;
	uint64_t val;

	zfsvfs->z_max_blksz = SPA_OLD_MAXBLOCKSIZE;
	zfsvfs->z_show_ctldir = ZFS_SNAPDIR_VISIBLE;
	zfsvfs->z_os = os;

	error = zfs_get_zplprop(os, ZFS_PROP_VERSION, &zfsvfs->z_version);
	if (error != 0)
		return (error);
	if (zfsvfs->z_version >
	    zfs_zpl_version_map(spa_version(dmu_objset_spa(os)))) {
		(void) printf("Can't mount a version %lld file system "
		    "on a version %lld pool\n. Pool must be upgraded to mount "
		    "this file system.", (u_longlong_t)zfsvfs->z_version,
		    (u_longlong_t)spa_version(dmu_objset_spa(os)));
		return (SET_ERROR(ENOTSUP));
	}
	error = zfs_get_zplprop(os, ZFS_PROP_NORMALIZE, &val);
	if (error != 0)
		return (error);
	zfsvfs->z_norm = (int)val;

	error = zfs_get_zplprop(os, ZFS_PROP_UTF8ONLY, &val);
	if (error != 0)
		return (error);
	zfsvfs->z_utf8 = (val != 0);

	error = zfs_get_zplprop(os, ZFS_PROP_CASE, &val);
	if (error != 0)
		return (error);
	zfsvfs->z_case = (uint_t)val;

	error = zfs_get_zplprop(os, ZFS_PROP_ACLTYPE, &val);
	if (error != 0)
		return (error);
	zfsvfs->z_acl_type = (uint_t)val;

	 
	if (zfsvfs->z_case == ZFS_CASE_INSENSITIVE ||
	    zfsvfs->z_case == ZFS_CASE_MIXED)
		zfsvfs->z_norm |= U8_TEXTPREP_TOUPPER;

	zfsvfs->z_use_fuids = USE_FUIDS(zfsvfs->z_version, zfsvfs->z_os);
	zfsvfs->z_use_sa = USE_SA(zfsvfs->z_version, zfsvfs->z_os);

	uint64_t sa_obj = 0;
	if (zfsvfs->z_use_sa) {
		 
		error = zap_lookup(os, MASTER_NODE_OBJ, ZFS_SA_ATTRS, 8, 1,
		    &sa_obj);
		if (error != 0)
			return (error);

		error = zfs_get_zplprop(os, ZFS_PROP_XATTR, &val);
		if (error == 0 && val == ZFS_XATTR_SA)
			zfsvfs->z_xattr_sa = B_TRUE;
	}

	error = sa_setup(os, sa_obj, zfs_attr_table, ZPL_END,
	    &zfsvfs->z_attr_table);
	if (error != 0)
		return (error);

	if (zfsvfs->z_version >= ZPL_VERSION_SA)
		sa_register_update_callback(os, zfs_sa_upgrade);

	error = zap_lookup(os, MASTER_NODE_OBJ, ZFS_ROOT_OBJ, 8, 1,
	    &zfsvfs->z_root);
	if (error != 0)
		return (error);
	ASSERT3U(zfsvfs->z_root, !=, 0);

	error = zap_lookup(os, MASTER_NODE_OBJ, ZFS_UNLINKED_SET, 8, 1,
	    &zfsvfs->z_unlinkedobj);
	if (error != 0)
		return (error);

	error = zap_lookup(os, MASTER_NODE_OBJ,
	    zfs_userquota_prop_prefixes[ZFS_PROP_USERQUOTA],
	    8, 1, &zfsvfs->z_userquota_obj);
	if (error == ENOENT)
		zfsvfs->z_userquota_obj = 0;
	else if (error != 0)
		return (error);

	error = zap_lookup(os, MASTER_NODE_OBJ,
	    zfs_userquota_prop_prefixes[ZFS_PROP_GROUPQUOTA],
	    8, 1, &zfsvfs->z_groupquota_obj);
	if (error == ENOENT)
		zfsvfs->z_groupquota_obj = 0;
	else if (error != 0)
		return (error);

	error = zap_lookup(os, MASTER_NODE_OBJ,
	    zfs_userquota_prop_prefixes[ZFS_PROP_PROJECTQUOTA],
	    8, 1, &zfsvfs->z_projectquota_obj);
	if (error == ENOENT)
		zfsvfs->z_projectquota_obj = 0;
	else if (error != 0)
		return (error);

	error = zap_lookup(os, MASTER_NODE_OBJ,
	    zfs_userquota_prop_prefixes[ZFS_PROP_USEROBJQUOTA],
	    8, 1, &zfsvfs->z_userobjquota_obj);
	if (error == ENOENT)
		zfsvfs->z_userobjquota_obj = 0;
	else if (error != 0)
		return (error);

	error = zap_lookup(os, MASTER_NODE_OBJ,
	    zfs_userquota_prop_prefixes[ZFS_PROP_GROUPOBJQUOTA],
	    8, 1, &zfsvfs->z_groupobjquota_obj);
	if (error == ENOENT)
		zfsvfs->z_groupobjquota_obj = 0;
	else if (error != 0)
		return (error);

	error = zap_lookup(os, MASTER_NODE_OBJ,
	    zfs_userquota_prop_prefixes[ZFS_PROP_PROJECTOBJQUOTA],
	    8, 1, &zfsvfs->z_projectobjquota_obj);
	if (error == ENOENT)
		zfsvfs->z_projectobjquota_obj = 0;
	else if (error != 0)
		return (error);

	error = zap_lookup(os, MASTER_NODE_OBJ, ZFS_FUID_TABLES, 8, 1,
	    &zfsvfs->z_fuid_obj);
	if (error == ENOENT)
		zfsvfs->z_fuid_obj = 0;
	else if (error != 0)
		return (error);

	error = zap_lookup(os, MASTER_NODE_OBJ, ZFS_SHARES_DIR, 8, 1,
	    &zfsvfs->z_shares_dir);
	if (error == ENOENT)
		zfsvfs->z_shares_dir = 0;
	else if (error != 0)
		return (error);

	 
	zfsvfs->z_use_namecache = !zfsvfs->z_norm ||
	    ((zfsvfs->z_case == ZFS_CASE_MIXED) &&
	    !(zfsvfs->z_norm & ~U8_TEXTPREP_TOUPPER));

	return (0);
}

taskq_t *zfsvfs_taskq;

static void
zfsvfs_task_unlinked_drain(void *context, int pending __unused)
{

	zfs_unlinked_drain((zfsvfs_t *)context);
}

int
zfsvfs_create(const char *osname, boolean_t readonly, zfsvfs_t **zfvp)
{
	objset_t *os;
	zfsvfs_t *zfsvfs;
	int error;
	boolean_t ro = (readonly || (strchr(osname, '@') != NULL));

	 
	if (strlen(osname) >= MNAMELEN)
		return (SET_ERROR(ENAMETOOLONG));

	zfsvfs = kmem_zalloc(sizeof (zfsvfs_t), KM_SLEEP);

	error = dmu_objset_own(osname, DMU_OST_ZFS, ro, B_TRUE, zfsvfs,
	    &os);
	if (error != 0) {
		kmem_free(zfsvfs, sizeof (zfsvfs_t));
		return (error);
	}

	error = zfsvfs_create_impl(zfvp, zfsvfs, os);

	return (error);
}


int
zfsvfs_create_impl(zfsvfs_t **zfvp, zfsvfs_t *zfsvfs, objset_t *os)
{
	int error;

	zfsvfs->z_vfs = NULL;
	zfsvfs->z_parent = zfsvfs;

	mutex_init(&zfsvfs->z_znodes_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&zfsvfs->z_lock, NULL, MUTEX_DEFAULT, NULL);
	list_create(&zfsvfs->z_all_znodes, sizeof (znode_t),
	    offsetof(znode_t, z_link_node));
	TASK_INIT(&zfsvfs->z_unlinked_drain_task, 0,
	    zfsvfs_task_unlinked_drain, zfsvfs);
	ZFS_TEARDOWN_INIT(zfsvfs);
	ZFS_TEARDOWN_INACTIVE_INIT(zfsvfs);
	rw_init(&zfsvfs->z_fuid_lock, NULL, RW_DEFAULT, NULL);
	for (int i = 0; i != ZFS_OBJ_MTX_SZ; i++)
		mutex_init(&zfsvfs->z_hold_mtx[i], NULL, MUTEX_DEFAULT, NULL);

	error = zfsvfs_init(zfsvfs, os);
	if (error != 0) {
		dmu_objset_disown(os, B_TRUE, zfsvfs);
		*zfvp = NULL;
		kmem_free(zfsvfs, sizeof (zfsvfs_t));
		return (error);
	}

	*zfvp = zfsvfs;
	return (0);
}

static int
zfsvfs_setup(zfsvfs_t *zfsvfs, boolean_t mounting)
{
	int error;

	 
	if (!(zfsvfs->z_vfs->vfs_flag & VFS_RDONLY) &&
	    dmu_objset_incompatible_encryption_version(zfsvfs->z_os))
		return (SET_ERROR(EROFS));

	error = zfs_register_callbacks(zfsvfs->z_vfs);
	if (error)
		return (error);

	 
	if (mounting) {
		boolean_t readonly;

		ASSERT3P(zfsvfs->z_kstat.dk_kstats, ==, NULL);
		error = dataset_kstats_create(&zfsvfs->z_kstat, zfsvfs->z_os);
		if (error)
			return (error);
		zfsvfs->z_log = zil_open(zfsvfs->z_os, zfs_get_data,
		    &zfsvfs->z_kstat.dk_zil_sums);

		 
		readonly = zfsvfs->z_vfs->vfs_flag & VFS_RDONLY;
		if (readonly != 0) {
			zfsvfs->z_vfs->vfs_flag &= ~VFS_RDONLY;
		} else {
			dsl_dir_t *dd;
			zap_stats_t zs;

			if (zap_get_stats(zfsvfs->z_os, zfsvfs->z_unlinkedobj,
			    &zs) == 0) {
				dataset_kstats_update_nunlinks_kstat(
				    &zfsvfs->z_kstat, zs.zs_num_entries);
				dprintf_ds(zfsvfs->z_os->os_dsl_dataset,
				    "num_entries in unlinked set: %llu",
				    (u_longlong_t)zs.zs_num_entries);
			}

			zfs_unlinked_drain(zfsvfs);
			dd = zfsvfs->z_os->os_dsl_dataset->ds_dir;
			dd->dd_activity_cancelled = B_FALSE;
		}

		 
		if (spa_writeable(dmu_objset_spa(zfsvfs->z_os))) {
			if (zil_replay_disable) {
				zil_destroy(zfsvfs->z_log, B_FALSE);
			} else {
				boolean_t use_nc = zfsvfs->z_use_namecache;
				zfsvfs->z_use_namecache = B_FALSE;
				zfsvfs->z_replay = B_TRUE;
				zil_replay(zfsvfs->z_os, zfsvfs,
				    zfs_replay_vector);
				zfsvfs->z_replay = B_FALSE;
				zfsvfs->z_use_namecache = use_nc;
			}
		}

		 
		if (readonly != 0)
			zfsvfs->z_vfs->vfs_flag |= VFS_RDONLY;
	} else {
		ASSERT3P(zfsvfs->z_kstat.dk_kstats, !=, NULL);
		zfsvfs->z_log = zil_open(zfsvfs->z_os, zfs_get_data,
		    &zfsvfs->z_kstat.dk_zil_sums);
	}

	 
	mutex_enter(&zfsvfs->z_os->os_user_ptr_lock);
	dmu_objset_set_user(zfsvfs->z_os, zfsvfs);
	mutex_exit(&zfsvfs->z_os->os_user_ptr_lock);

	return (0);
}

void
zfsvfs_free(zfsvfs_t *zfsvfs)
{
	int i;

	zfs_fuid_destroy(zfsvfs);

	mutex_destroy(&zfsvfs->z_znodes_lock);
	mutex_destroy(&zfsvfs->z_lock);
	list_destroy(&zfsvfs->z_all_znodes);
	ZFS_TEARDOWN_DESTROY(zfsvfs);
	ZFS_TEARDOWN_INACTIVE_DESTROY(zfsvfs);
	rw_destroy(&zfsvfs->z_fuid_lock);
	for (i = 0; i != ZFS_OBJ_MTX_SZ; i++)
		mutex_destroy(&zfsvfs->z_hold_mtx[i]);
	dataset_kstats_destroy(&zfsvfs->z_kstat);
	kmem_free(zfsvfs, sizeof (zfsvfs_t));
}

static void
zfs_set_fuid_feature(zfsvfs_t *zfsvfs)
{
	zfsvfs->z_use_fuids = USE_FUIDS(zfsvfs->z_version, zfsvfs->z_os);
	zfsvfs->z_use_sa = USE_SA(zfsvfs->z_version, zfsvfs->z_os);
}

static int
zfs_domount(vfs_t *vfsp, char *osname)
{
	uint64_t recordsize, fsid_guid;
	int error = 0;
	zfsvfs_t *zfsvfs;

	ASSERT3P(vfsp, !=, NULL);
	ASSERT3P(osname, !=, NULL);

	error = zfsvfs_create(osname, vfsp->mnt_flag & MNT_RDONLY, &zfsvfs);
	if (error)
		return (error);
	zfsvfs->z_vfs = vfsp;

	if ((error = dsl_prop_get_integer(osname,
	    "recordsize", &recordsize, NULL)))
		goto out;
	zfsvfs->z_vfs->vfs_bsize = SPA_MINBLOCKSIZE;
	zfsvfs->z_vfs->mnt_stat.f_iosize = recordsize;

	vfsp->vfs_data = zfsvfs;
	vfsp->mnt_flag |= MNT_LOCAL;
	vfsp->mnt_kern_flag |= MNTK_LOOKUP_SHARED;
	vfsp->mnt_kern_flag |= MNTK_SHARED_WRITES;
	vfsp->mnt_kern_flag |= MNTK_EXTENDED_SHARED;
	 
	vfsp->mnt_kern_flag |= MNTK_NO_IOPF;	 
	vfsp->mnt_kern_flag |= MNTK_NOMSYNC;
	vfsp->mnt_kern_flag |= MNTK_VMSETSIZE_BUG;

#if defined(_KERNEL) && !defined(KMEM_DEBUG)
	vfsp->mnt_kern_flag |= MNTK_FPLOOKUP;
#endif
	 
	fsid_guid = dmu_objset_fsid_guid(zfsvfs->z_os);
	ASSERT3U((fsid_guid & ~((1ULL << 56) - 1)), ==, 0);
	vfsp->vfs_fsid.val[0] = fsid_guid;
	vfsp->vfs_fsid.val[1] = ((fsid_guid >> 32) << 8) |
	    (vfsp->mnt_vfc->vfc_typenum & 0xFF);

	 
	zfs_set_fuid_feature(zfsvfs);

	if (dmu_objset_is_snapshot(zfsvfs->z_os)) {
		uint64_t pval;

		atime_changed_cb(zfsvfs, B_FALSE);
		readonly_changed_cb(zfsvfs, B_TRUE);
		if ((error = dsl_prop_get_integer(osname,
		    "xattr", &pval, NULL)))
			goto out;
		xattr_changed_cb(zfsvfs, pval);
		if ((error = dsl_prop_get_integer(osname,
		    "acltype", &pval, NULL)))
			goto out;
		acl_type_changed_cb(zfsvfs, pval);
		zfsvfs->z_issnap = B_TRUE;
		zfsvfs->z_os->os_sync = ZFS_SYNC_DISABLED;

		mutex_enter(&zfsvfs->z_os->os_user_ptr_lock);
		dmu_objset_set_user(zfsvfs->z_os, zfsvfs);
		mutex_exit(&zfsvfs->z_os->os_user_ptr_lock);
	} else {
		if ((error = zfsvfs_setup(zfsvfs, B_TRUE)))
			goto out;
	}

	vfs_mountedfrom(vfsp, osname);

	if (!zfsvfs->z_issnap)
		zfsctl_create(zfsvfs);
out:
	if (error) {
		dmu_objset_disown(zfsvfs->z_os, B_TRUE, zfsvfs);
		zfsvfs_free(zfsvfs);
	} else {
		atomic_inc_32(&zfs_active_fs_count);
	}

	return (error);
}

static void
zfs_unregister_callbacks(zfsvfs_t *zfsvfs)
{
	objset_t *os = zfsvfs->z_os;

	if (!dmu_objset_is_snapshot(os))
		dsl_prop_unregister_all(dmu_objset_ds(os), zfsvfs);
}

static int
getpoolname(const char *osname, char *poolname)
{
	char *p;

	p = strchr(osname, '/');
	if (p == NULL) {
		if (strlen(osname) >= MAXNAMELEN)
			return (ENAMETOOLONG);
		(void) strcpy(poolname, osname);
	} else {
		if (p - osname >= MAXNAMELEN)
			return (ENAMETOOLONG);
		(void) strlcpy(poolname, osname, p - osname + 1);
	}
	return (0);
}

static void
fetch_osname_options(char *name, bool *checkpointrewind)
{

	if (name[0] == '!') {
		*checkpointrewind = true;
		memmove(name, name + 1, strlen(name));
	} else {
		*checkpointrewind = false;
	}
}

static int
zfs_mount(vfs_t *vfsp)
{
	kthread_t	*td = curthread;
	vnode_t		*mvp = vfsp->mnt_vnodecovered;
	cred_t		*cr = td->td_ucred;
	char		*osname;
	int		error = 0;
	int		canwrite;
	bool		checkpointrewind, isctlsnap = false;

	if (vfs_getopt(vfsp->mnt_optnew, "from", (void **)&osname, NULL))
		return (SET_ERROR(EINVAL));

	 
	if (zfs_super_owner &&
	    dsl_deleg_access(osname, ZFS_DELEG_PERM_MOUNT, cr) != ECANCELED) {
		secpolicy_fs_mount_clearopts(cr, vfsp);
	}

	fetch_osname_options(osname, &checkpointrewind);
	isctlsnap = (mvp != NULL && zfsctl_is_node(mvp) &&
	    strchr(osname, '@') != NULL);

	 
	error = secpolicy_fs_mount(cr, mvp, vfsp);
	if (error && isctlsnap) {
		secpolicy_fs_mount_clearopts(cr, vfsp);
	} else if (error) {
		if (dsl_deleg_access(osname, ZFS_DELEG_PERM_MOUNT, cr) != 0)
			goto out;

		if (!(vfsp->vfs_flag & MS_REMOUNT)) {
			vattr_t		vattr;

			 

			vattr.va_mask = AT_UID;

			vn_lock(mvp, LK_SHARED | LK_RETRY);
			if (VOP_GETATTR(mvp, &vattr, cr)) {
				VOP_UNLOCK1(mvp);
				goto out;
			}

			if (secpolicy_vnode_owner(mvp, cr, vattr.va_uid) != 0 &&
			    VOP_ACCESS(mvp, VWRITE, cr, td) != 0) {
				VOP_UNLOCK1(mvp);
				goto out;
			}
			VOP_UNLOCK1(mvp);
		}

		secpolicy_fs_mount_clearopts(cr, vfsp);
	}

	 
	if (!INGLOBALZONE(curproc) &&
	    (!zone_dataset_visible(osname, &canwrite) || !canwrite)) {
		boolean_t mount_snapshot = B_FALSE;

		 
		if (isctlsnap) {
			struct prison *pr;
			struct zfs_jailparam *zjp;

			pr = curthread->td_ucred->cr_prison;
			mtx_lock(&pr->pr_mtx);
			zjp = osd_jail_get(pr, zfs_jailparam_slot);
			mtx_unlock(&pr->pr_mtx);
			if (zjp && zjp->mount_snapshot)
				mount_snapshot = B_TRUE;
		}
		if (!mount_snapshot) {
			error = SET_ERROR(EPERM);
			goto out;
		}
	}

	vfsp->vfs_flag |= MNT_NFS4ACLS;

	 
	if (vfsp->vfs_flag & MS_REMOUNT) {
		zfsvfs_t *zfsvfs = vfsp->vfs_data;

		 
		ZFS_TEARDOWN_ENTER_WRITE(zfsvfs, FTAG);
		zfs_unregister_callbacks(zfsvfs);
		error = zfs_register_callbacks(vfsp);
		ZFS_TEARDOWN_EXIT(zfsvfs, FTAG);
		goto out;
	}

	 
	if ((vfsp->vfs_flag & MNT_ROOTFS) != 0 &&
	    (vfsp->vfs_flag & MNT_UPDATE) == 0) {
		char pname[MAXNAMELEN];

		error = getpoolname(osname, pname);
		if (error == 0)
			error = spa_import_rootpool(pname, checkpointrewind);
		if (error)
			goto out;
	}
	DROP_GIANT();
	error = zfs_domount(vfsp, osname);
	PICKUP_GIANT();

out:
	return (error);
}

static int
zfs_statfs(vfs_t *vfsp, struct statfs *statp)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	uint64_t refdbytes, availbytes, usedobjs, availobjs;
	int error;

	statp->f_version = STATFS_VERSION;

	if ((error = zfs_enter(zfsvfs, FTAG)) != 0)
		return (error);

	dmu_objset_space(zfsvfs->z_os,
	    &refdbytes, &availbytes, &usedobjs, &availobjs);

	 
	statp->f_bsize = SPA_MINBLOCKSIZE;
	statp->f_iosize = zfsvfs->z_vfs->mnt_stat.f_iosize;

	 

	statp->f_blocks = (refdbytes + availbytes) >> SPA_MINBLOCKSHIFT;
	statp->f_bfree = availbytes / statp->f_bsize;
	statp->f_bavail = statp->f_bfree;  

	 
	statp->f_ffree = MIN(availobjs, statp->f_bfree);
	statp->f_files = statp->f_ffree + usedobjs;

	 
	strlcpy(statp->f_fstypename, "zfs",
	    sizeof (statp->f_fstypename));

	strlcpy(statp->f_mntfromname, vfsp->mnt_stat.f_mntfromname,
	    sizeof (statp->f_mntfromname));
	strlcpy(statp->f_mntonname, vfsp->mnt_stat.f_mntonname,
	    sizeof (statp->f_mntonname));

	statp->f_namemax = MAXNAMELEN - 1;

	zfs_exit(zfsvfs, FTAG);
	return (0);
}

static int
zfs_root(vfs_t *vfsp, int flags, vnode_t **vpp)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	znode_t *rootzp;
	int error;

	if ((error = zfs_enter(zfsvfs, FTAG)) != 0)
		return (error);

	error = zfs_zget(zfsvfs, zfsvfs->z_root, &rootzp);
	if (error == 0)
		*vpp = ZTOV(rootzp);

	zfs_exit(zfsvfs, FTAG);

	if (error == 0) {
		error = vn_lock(*vpp, flags);
		if (error != 0) {
			VN_RELE(*vpp);
			*vpp = NULL;
		}
	}
	return (error);
}

 
static int
zfsvfs_teardown(zfsvfs_t *zfsvfs, boolean_t unmounting)
{
	znode_t	*zp;
	dsl_dir_t *dd;

	 
	if (zfsvfs->z_os) {
		 
		int round = 0;
		while (!list_is_empty(&zfsvfs->z_all_znodes)) {
			taskq_wait_outstanding(dsl_pool_zrele_taskq(
			    dmu_objset_pool(zfsvfs->z_os)), 0);
			if (++round > 1 && !unmounting)
				break;
		}
	}
	ZFS_TEARDOWN_ENTER_WRITE(zfsvfs, FTAG);

	if (!unmounting) {
		 
#ifdef FREEBSD_NAMECACHE
#if __FreeBSD_version >= 1300117
		cache_purgevfs(zfsvfs->z_parent->z_vfs);
#else
		cache_purgevfs(zfsvfs->z_parent->z_vfs, true);
#endif
#endif
	}

	 
	if (zfsvfs->z_log) {
		zil_close(zfsvfs->z_log);
		zfsvfs->z_log = NULL;
	}

	ZFS_TEARDOWN_INACTIVE_ENTER_WRITE(zfsvfs);

	 
	if (!unmounting && (zfsvfs->z_unmounted || zfsvfs->z_os == NULL)) {
		ZFS_TEARDOWN_INACTIVE_EXIT_WRITE(zfsvfs);
		ZFS_TEARDOWN_EXIT(zfsvfs, FTAG);
		return (SET_ERROR(EIO));
	}

	 
	mutex_enter(&zfsvfs->z_znodes_lock);
	for (zp = list_head(&zfsvfs->z_all_znodes); zp != NULL;
	    zp = list_next(&zfsvfs->z_all_znodes, zp)) {
		if (zp->z_sa_hdl != NULL) {
			zfs_znode_dmu_fini(zp);
		}
	}
	mutex_exit(&zfsvfs->z_znodes_lock);

	 
	if (unmounting) {
		zfsvfs->z_unmounted = B_TRUE;
		ZFS_TEARDOWN_INACTIVE_EXIT_WRITE(zfsvfs);
		ZFS_TEARDOWN_EXIT(zfsvfs, FTAG);
	}

	 
	if (zfsvfs->z_os == NULL)
		return (0);

	 
	zfs_unregister_callbacks(zfsvfs);

	 
	if (!zfs_is_readonly(zfsvfs))
		txg_wait_synced(dmu_objset_pool(zfsvfs->z_os), 0);
	dmu_objset_evict_dbufs(zfsvfs->z_os);
	dd = zfsvfs->z_os->os_dsl_dataset->ds_dir;
	dsl_dir_cancel_waiters(dd);

	return (0);
}

static int
zfs_umount(vfs_t *vfsp, int fflag)
{
	kthread_t *td = curthread;
	zfsvfs_t *zfsvfs = vfsp->vfs_data;
	objset_t *os;
	cred_t *cr = td->td_ucred;
	int ret;

	ret = secpolicy_fs_unmount(cr, vfsp);
	if (ret) {
		if (dsl_deleg_access((char *)vfsp->vfs_resource,
		    ZFS_DELEG_PERM_MOUNT, cr))
			return (ret);
	}

	 
	if (zfsvfs->z_ctldir != NULL) {
		if ((ret = zfsctl_umount_snapshots(vfsp, fflag, cr)) != 0)
			return (ret);
	}

	if (fflag & MS_FORCE) {
		 
		ZFS_TEARDOWN_ENTER_WRITE(zfsvfs, FTAG);
		zfsvfs->z_unmounted = B_TRUE;
		ZFS_TEARDOWN_EXIT(zfsvfs, FTAG);
	}

	 
	ret = vflush(vfsp, 0, (fflag & MS_FORCE) ? FORCECLOSE : 0, td);
	if (ret != 0)
		return (ret);
	while (taskqueue_cancel(zfsvfs_taskq->tq_queue,
	    &zfsvfs->z_unlinked_drain_task, NULL) != 0)
		taskqueue_drain(zfsvfs_taskq->tq_queue,
		    &zfsvfs->z_unlinked_drain_task);

	VERIFY0(zfsvfs_teardown(zfsvfs, B_TRUE));
	os = zfsvfs->z_os;

	 
	if (os != NULL) {
		 
		mutex_enter(&os->os_user_ptr_lock);
		dmu_objset_set_user(os, NULL);
		mutex_exit(&os->os_user_ptr_lock);

		 
		dmu_objset_disown(os, B_TRUE, zfsvfs);
	}

	 
	if (zfsvfs->z_ctldir != NULL)
		zfsctl_destroy(zfsvfs);
	zfs_freevfs(vfsp);

	return (0);
}

static int
zfs_vget(vfs_t *vfsp, ino_t ino, int flags, vnode_t **vpp)
{
	zfsvfs_t	*zfsvfs = vfsp->vfs_data;
	znode_t		*zp;
	int 		err;

	 
	if (ino == ZFSCTL_INO_ROOT || ino == ZFSCTL_INO_SNAPDIR ||
	    (zfsvfs->z_shares_dir != 0 && ino == zfsvfs->z_shares_dir))
		return (EOPNOTSUPP);

	if ((err = zfs_enter(zfsvfs, FTAG)) != 0)
		return (err);
	err = zfs_zget(zfsvfs, ino, &zp);
	if (err == 0 && zp->z_unlinked) {
		vrele(ZTOV(zp));
		err = EINVAL;
	}
	if (err == 0)
		*vpp = ZTOV(zp);
	zfs_exit(zfsvfs, FTAG);
	if (err == 0) {
		err = vn_lock(*vpp, flags);
		if (err != 0)
			vrele(*vpp);
	}
	if (err != 0)
		*vpp = NULL;
	return (err);
}

static int
#if __FreeBSD_version >= 1300098
zfs_checkexp(vfs_t *vfsp, struct sockaddr *nam, uint64_t *extflagsp,
    struct ucred **credanonp, int *numsecflavors, int *secflavors)
#else
zfs_checkexp(vfs_t *vfsp, struct sockaddr *nam, int *extflagsp,
    struct ucred **credanonp, int *numsecflavors, int **secflavors)
#endif
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;

	 
	return (vfs_stdcheckexp(zfsvfs->z_parent->z_vfs, nam, extflagsp,
	    credanonp, numsecflavors, secflavors));
}

_Static_assert(sizeof (struct fid) >= SHORT_FID_LEN,
	"struct fid bigger than SHORT_FID_LEN");
_Static_assert(sizeof (struct fid) >= LONG_FID_LEN,
	"struct fid bigger than LONG_FID_LEN");

static int
zfs_fhtovp(vfs_t *vfsp, fid_t *fidp, int flags, vnode_t **vpp)
{
	struct componentname cn;
	zfsvfs_t	*zfsvfs = vfsp->vfs_data;
	znode_t		*zp;
	vnode_t		*dvp;
	uint64_t	object = 0;
	uint64_t	fid_gen = 0;
	uint64_t	setgen = 0;
	uint64_t	gen_mask;
	uint64_t	zp_gen;
	int 		i, err;

	*vpp = NULL;

	if ((err = zfs_enter(zfsvfs, FTAG)) != 0)
		return (err);

	 
	if (zfsvfs->z_parent == zfsvfs && fidp->fid_len == LONG_FID_LEN) {
		zfid_long_t	*zlfid = (zfid_long_t *)fidp;
		uint64_t	objsetid = 0;

		for (i = 0; i < sizeof (zlfid->zf_setid); i++)
			objsetid |= ((uint64_t)zlfid->zf_setid[i]) << (8 * i);

		for (i = 0; i < sizeof (zlfid->zf_setgen); i++)
			setgen |= ((uint64_t)zlfid->zf_setgen[i]) << (8 * i);

		zfs_exit(zfsvfs, FTAG);

		err = zfsctl_lookup_objset(vfsp, objsetid, &zfsvfs);
		if (err)
			return (SET_ERROR(EINVAL));
		if ((err = zfs_enter(zfsvfs, FTAG)) != 0)
			return (err);
	}

	if (fidp->fid_len == SHORT_FID_LEN || fidp->fid_len == LONG_FID_LEN) {
		zfid_short_t	*zfid = (zfid_short_t *)fidp;

		for (i = 0; i < sizeof (zfid->zf_object); i++)
			object |= ((uint64_t)zfid->zf_object[i]) << (8 * i);

		for (i = 0; i < sizeof (zfid->zf_gen); i++)
			fid_gen |= ((uint64_t)zfid->zf_gen[i]) << (8 * i);
	} else {
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}

	if (fidp->fid_len == LONG_FID_LEN && setgen != 0) {
		zfs_exit(zfsvfs, FTAG);
		dprintf("snapdir fid: fid_gen (%llu) and setgen (%llu)\n",
		    (u_longlong_t)fid_gen, (u_longlong_t)setgen);
		return (SET_ERROR(EINVAL));
	}

	 
	if ((fid_gen == 0 &&
	    (object == ZFSCTL_INO_ROOT || object == ZFSCTL_INO_SNAPDIR)) ||
	    (zfsvfs->z_shares_dir != 0 && object == zfsvfs->z_shares_dir)) {
		zfs_exit(zfsvfs, FTAG);
		VERIFY0(zfsctl_root(zfsvfs, LK_SHARED, &dvp));
		if (object == ZFSCTL_INO_SNAPDIR) {
			cn.cn_nameptr = "snapshot";
			cn.cn_namelen = strlen(cn.cn_nameptr);
			cn.cn_nameiop = LOOKUP;
			cn.cn_flags = ISLASTCN | LOCKLEAF;
			cn.cn_lkflags = flags;
			VERIFY0(VOP_LOOKUP(dvp, vpp, &cn));
			vput(dvp);
		} else if (object == zfsvfs->z_shares_dir) {
			 
			cn.cn_nameptr = "shares";
			cn.cn_namelen = strlen(cn.cn_nameptr);
			cn.cn_nameiop = LOOKUP;
			cn.cn_flags = ISLASTCN;
			cn.cn_lkflags = flags;
			VERIFY0(VOP_LOOKUP(dvp, vpp, &cn));
			vput(dvp);
		} else {
			*vpp = dvp;
		}
		return (err);
	}

	gen_mask = -1ULL >> (64 - 8 * i);

	dprintf("getting %llu [%llu mask %llx]\n", (u_longlong_t)object,
	    (u_longlong_t)fid_gen,
	    (u_longlong_t)gen_mask);
	if ((err = zfs_zget(zfsvfs, object, &zp))) {
		zfs_exit(zfsvfs, FTAG);
		return (err);
	}
	(void) sa_lookup(zp->z_sa_hdl, SA_ZPL_GEN(zfsvfs), &zp_gen,
	    sizeof (uint64_t));
	zp_gen = zp_gen & gen_mask;
	if (zp_gen == 0)
		zp_gen = 1;
	if (zp->z_unlinked || zp_gen != fid_gen) {
		dprintf("znode gen (%llu) != fid gen (%llu)\n",
		    (u_longlong_t)zp_gen, (u_longlong_t)fid_gen);
		vrele(ZTOV(zp));
		zfs_exit(zfsvfs, FTAG);
		return (SET_ERROR(EINVAL));
	}

	*vpp = ZTOV(zp);
	zfs_exit(zfsvfs, FTAG);
	err = vn_lock(*vpp, flags);
	if (err == 0)
		vnode_create_vobject(*vpp, zp->z_size, curthread);
	else
		*vpp = NULL;
	return (err);
}

 
int
zfs_suspend_fs(zfsvfs_t *zfsvfs)
{
	int error;

	if ((error = zfsvfs_teardown(zfsvfs, B_FALSE)) != 0)
		return (error);

	return (0);
}

 
int
zfs_resume_fs(zfsvfs_t *zfsvfs, dsl_dataset_t *ds)
{
	int err;
	znode_t *zp;

	ASSERT(ZFS_TEARDOWN_WRITE_HELD(zfsvfs));
	ASSERT(ZFS_TEARDOWN_INACTIVE_WRITE_HELD(zfsvfs));

	 
	objset_t *os;
	VERIFY3P(ds->ds_owner, ==, zfsvfs);
	VERIFY(dsl_dataset_long_held(ds));
	dsl_pool_t *dp = spa_get_dsl(dsl_dataset_get_spa(ds));
	dsl_pool_config_enter(dp, FTAG);
	VERIFY0(dmu_objset_from_ds(ds, &os));
	dsl_pool_config_exit(dp, FTAG);

	err = zfsvfs_init(zfsvfs, os);
	if (err != 0)
		goto bail;

	ds->ds_dir->dd_activity_cancelled = B_FALSE;
	VERIFY0(zfsvfs_setup(zfsvfs, B_FALSE));

	zfs_set_fuid_feature(zfsvfs);

	 
	mutex_enter(&zfsvfs->z_znodes_lock);
	for (zp = list_head(&zfsvfs->z_all_znodes); zp;
	    zp = list_next(&zfsvfs->z_all_znodes, zp)) {
		(void) zfs_rezget(zp);
	}
	mutex_exit(&zfsvfs->z_znodes_lock);

bail:
	 
	ZFS_TEARDOWN_INACTIVE_EXIT_WRITE(zfsvfs);
	ZFS_TEARDOWN_EXIT(zfsvfs, FTAG);

	if (err) {
		 
		if (vn_vfswlock(zfsvfs->z_vfs->vfs_vnodecovered) == 0) {
			vfs_ref(zfsvfs->z_vfs);
			(void) dounmount(zfsvfs->z_vfs, MS_FORCE, curthread);
		}
	}
	return (err);
}

static void
zfs_freevfs(vfs_t *vfsp)
{
	zfsvfs_t *zfsvfs = vfsp->vfs_data;

	zfsvfs_free(zfsvfs);

	atomic_dec_32(&zfs_active_fs_count);
}

#ifdef __i386__
static int desiredvnodes_backup;
#include <sys/vmmeter.h>


#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#endif

static void
zfs_vnodes_adjust(void)
{
#ifdef __i386__
	int newdesiredvnodes;

	desiredvnodes_backup = desiredvnodes;

	 
	newdesiredvnodes = min(maxproc + vm_cnt.v_page_count / 4, 2 *
	    vm_kmem_size / (5 * (sizeof (struct vm_object) +
	    sizeof (struct vnode))));
	if (newdesiredvnodes == desiredvnodes)
		desiredvnodes = (3 * newdesiredvnodes) / 4;
#endif
}

static void
zfs_vnodes_adjust_back(void)
{

#ifdef __i386__
	desiredvnodes = desiredvnodes_backup;
#endif
}

#if __FreeBSD_version >= 1300139
static struct sx zfs_vnlru_lock;
static struct vnode *zfs_vnlru_marker;
#endif
static arc_prune_t *zfs_prune;

static void
zfs_prune_task(uint64_t nr_to_scan, void *arg __unused)
{
	if (nr_to_scan > INT_MAX)
		nr_to_scan = INT_MAX;
#if __FreeBSD_version >= 1300139
	sx_xlock(&zfs_vnlru_lock);
	vnlru_free_vfsops(nr_to_scan, &zfs_vfsops, zfs_vnlru_marker);
	sx_xunlock(&zfs_vnlru_lock);
#else
	vnlru_free(nr_to_scan, &zfs_vfsops);
#endif
}

void
zfs_init(void)
{

	printf("ZFS filesystem version: " ZPL_VERSION_STRING "\n");

	 
	zfsctl_init();

	 
	zfs_znode_init();

	 
	zfs_vnodes_adjust();

	dmu_objset_register_type(DMU_OST_ZFS, zpl_get_file_info);

	zfsvfs_taskq = taskq_create("zfsvfs", 1, minclsyspri, 0, 0, 0);

#if __FreeBSD_version >= 1300139
	zfs_vnlru_marker = vnlru_alloc_marker();
	sx_init(&zfs_vnlru_lock, "zfs vnlru lock");
#endif
	zfs_prune = arc_add_prune_callback(zfs_prune_task, NULL);
}

void
zfs_fini(void)
{
	arc_remove_prune_callback(zfs_prune);
#if __FreeBSD_version >= 1300139
	vnlru_free_marker(zfs_vnlru_marker);
	sx_destroy(&zfs_vnlru_lock);
#endif

	taskq_destroy(zfsvfs_taskq);
	zfsctl_fini();
	zfs_znode_fini();
	zfs_vnodes_adjust_back();
}

int
zfs_busy(void)
{
	return (zfs_active_fs_count != 0);
}

 
int
zfs_end_fs(zfsvfs_t *zfsvfs, dsl_dataset_t *ds)
{
	ASSERT(ZFS_TEARDOWN_WRITE_HELD(zfsvfs));
	ASSERT(ZFS_TEARDOWN_INACTIVE_WRITE_HELD(zfsvfs));

	 
	objset_t *os;
	VERIFY3P(ds->ds_owner, ==, zfsvfs);
	VERIFY(dsl_dataset_long_held(ds));
	dsl_pool_t *dp = spa_get_dsl(dsl_dataset_get_spa(ds));
	dsl_pool_config_enter(dp, FTAG);
	VERIFY0(dmu_objset_from_ds(ds, &os));
	dsl_pool_config_exit(dp, FTAG);
	zfsvfs->z_os = os;

	 
	ZFS_TEARDOWN_INACTIVE_EXIT_WRITE(zfsvfs);
	ZFS_TEARDOWN_EXIT(zfsvfs, FTAG);

	 
	(void) zfs_umount(zfsvfs->z_vfs, 0);
	zfsvfs->z_unmounted = B_TRUE;
	return (0);
}

int
zfs_set_version(zfsvfs_t *zfsvfs, uint64_t newvers)
{
	int error;
	objset_t *os = zfsvfs->z_os;
	dmu_tx_t *tx;

	if (newvers < ZPL_VERSION_INITIAL || newvers > ZPL_VERSION)
		return (SET_ERROR(EINVAL));

	if (newvers < zfsvfs->z_version)
		return (SET_ERROR(EINVAL));

	if (zfs_spa_version_map(newvers) >
	    spa_version(dmu_objset_spa(zfsvfs->z_os)))
		return (SET_ERROR(ENOTSUP));

	tx = dmu_tx_create(os);
	dmu_tx_hold_zap(tx, MASTER_NODE_OBJ, B_FALSE, ZPL_VERSION_STR);
	if (newvers >= ZPL_VERSION_SA && !zfsvfs->z_use_sa) {
		dmu_tx_hold_zap(tx, MASTER_NODE_OBJ, B_TRUE,
		    ZFS_SA_ATTRS);
		dmu_tx_hold_zap(tx, DMU_NEW_OBJECT, FALSE, NULL);
	}
	error = dmu_tx_assign(tx, TXG_WAIT);
	if (error) {
		dmu_tx_abort(tx);
		return (error);
	}

	error = zap_update(os, MASTER_NODE_OBJ, ZPL_VERSION_STR,
	    8, 1, &newvers, tx);

	if (error) {
		dmu_tx_commit(tx);
		return (error);
	}

	if (newvers >= ZPL_VERSION_SA && !zfsvfs->z_use_sa) {
		uint64_t sa_obj;

		ASSERT3U(spa_version(dmu_objset_spa(zfsvfs->z_os)), >=,
		    SPA_VERSION_SA);
		sa_obj = zap_create(os, DMU_OT_SA_MASTER_NODE,
		    DMU_OT_NONE, 0, tx);

		error = zap_add(os, MASTER_NODE_OBJ,
		    ZFS_SA_ATTRS, 8, 1, &sa_obj, tx);
		ASSERT0(error);

		VERIFY0(sa_set_sa_object(os, sa_obj));
		sa_register_update_callback(os, zfs_sa_upgrade);
	}

	spa_history_log_internal_ds(dmu_objset_ds(os), "upgrade", tx,
	    "from %ju to %ju", (uintmax_t)zfsvfs->z_version,
	    (uintmax_t)newvers);
	dmu_tx_commit(tx);

	zfsvfs->z_version = newvers;
	os->os_version = newvers;

	zfs_set_fuid_feature(zfsvfs);

	return (0);
}

 
boolean_t
zfs_get_vfs_flag_unmounted(objset_t *os)
{
	zfsvfs_t *zfvp;
	boolean_t unmounted = B_FALSE;

	ASSERT3U(dmu_objset_type(os), ==, DMU_OST_ZFS);

	mutex_enter(&os->os_user_ptr_lock);
	zfvp = dmu_objset_get_user(os);
	if (zfvp != NULL && zfvp->z_vfs != NULL &&
	    (zfvp->z_vfs->mnt_kern_flag & MNTK_UNMOUNT))
		unmounted = B_TRUE;
	mutex_exit(&os->os_user_ptr_lock);

	return (unmounted);
}

#ifdef _KERNEL
void
zfsvfs_update_fromname(const char *oldname, const char *newname)
{
	char tmpbuf[MAXPATHLEN];
	struct mount *mp;
	char *fromname;
	size_t oldlen;

	oldlen = strlen(oldname);

	mtx_lock(&mountlist_mtx);
	TAILQ_FOREACH(mp, &mountlist, mnt_list) {
		fromname = mp->mnt_stat.f_mntfromname;
		if (strcmp(fromname, oldname) == 0) {
			(void) strlcpy(fromname, newname,
			    sizeof (mp->mnt_stat.f_mntfromname));
			continue;
		}
		if (strncmp(fromname, oldname, oldlen) == 0 &&
		    (fromname[oldlen] == '/' || fromname[oldlen] == '@')) {
			(void) snprintf(tmpbuf, sizeof (tmpbuf), "%s%s",
			    newname, fromname + oldlen);
			(void) strlcpy(fromname, tmpbuf,
			    sizeof (mp->mnt_stat.f_mntfromname));
			continue;
		}
	}
	mtx_unlock(&mountlist_mtx);
}
#endif

 
static struct zfs_jailparam *
zfs_jailparam_find(struct prison *spr, struct prison **prp)
{
	struct prison *pr;
	struct zfs_jailparam *zjp;

	for (pr = spr; ; pr = pr->pr_parent) {
		mtx_lock(&pr->pr_mtx);
		if (pr == &prison0) {
			zjp = &zfs_jailparam0;
			break;
		}
		zjp = osd_jail_get(pr, zfs_jailparam_slot);
		if (zjp != NULL)
			break;
		mtx_unlock(&pr->pr_mtx);
	}
	*prp = pr;

	return (zjp);
}

 
static void
zfs_jailparam_alloc(struct prison *pr, struct zfs_jailparam **zjpp)
{
	struct prison *ppr;
	struct zfs_jailparam *zjp, *nzjp;
	void **rsv;

	 
	zjp = zfs_jailparam_find(pr, &ppr);
	if (ppr == pr)
		goto done;

	 
	mtx_unlock(&ppr->pr_mtx);
	nzjp = malloc(sizeof (struct zfs_jailparam), M_PRISON, M_WAITOK);
	rsv = osd_reserve(zfs_jailparam_slot);
	zjp = zfs_jailparam_find(pr, &ppr);
	if (ppr == pr) {
		free(nzjp, M_PRISON);
		osd_free_reserved(rsv);
		goto done;
	}
	 
	mtx_lock(&pr->pr_mtx);
	(void) osd_jail_set_reserved(pr, zfs_jailparam_slot, rsv, nzjp);
	(void) memcpy(nzjp, zjp, sizeof (*zjp));
	zjp = nzjp;
	mtx_unlock(&ppr->pr_mtx);
done:
	if (zjpp != NULL)
		*zjpp = zjp;
	else
		mtx_unlock(&pr->pr_mtx);
}

 
static int
zfs_jailparam_create(void *obj, void *data)
{
	struct prison *pr = obj;
	struct vfsoptlist *opts = data;
	int jsys;

	if (vfs_copyopt(opts, "zfs", &jsys, sizeof (jsys)) == 0 &&
	    jsys == JAIL_SYS_INHERIT)
		return (0);
	 
	zfs_jailparam_alloc(pr, NULL);
	return (0);
}

static int
zfs_jailparam_get(void *obj, void *data)
{
	struct prison *ppr, *pr = obj;
	struct vfsoptlist *opts = data;
	struct zfs_jailparam *zjp;
	int jsys, error;

	zjp = zfs_jailparam_find(pr, &ppr);
	jsys = (ppr == pr) ? JAIL_SYS_NEW : JAIL_SYS_INHERIT;
	error = vfs_setopt(opts, "zfs", &jsys, sizeof (jsys));
	if (error != 0 && error != ENOENT)
		goto done;
	if (jsys == JAIL_SYS_NEW) {
		error = vfs_setopt(opts, "zfs.mount_snapshot",
		    &zjp->mount_snapshot, sizeof (zjp->mount_snapshot));
		if (error != 0 && error != ENOENT)
			goto done;
	} else {
		 
		static int mount_snapshot = 0;

		error = vfs_setopt(opts, "zfs.mount_snapshot",
		    &mount_snapshot, sizeof (mount_snapshot));
		if (error != 0 && error != ENOENT)
			goto done;
	}
	error = 0;
done:
	mtx_unlock(&ppr->pr_mtx);
	return (error);
}

static int
zfs_jailparam_set(void *obj, void *data)
{
	struct prison *pr = obj;
	struct prison *ppr;
	struct vfsoptlist *opts = data;
	int error, jsys, mount_snapshot;

	 
	error = vfs_copyopt(opts, "zfs", &jsys, sizeof (jsys));
	if (error == ENOENT)
		jsys = -1;
	error = vfs_copyopt(opts, "zfs.mount_snapshot", &mount_snapshot,
	    sizeof (mount_snapshot));
	if (error == ENOENT)
		mount_snapshot = -1;
	else
		jsys = JAIL_SYS_NEW;
	switch (jsys) {
	case JAIL_SYS_NEW:
	{
		 
		struct zfs_jailparam *zjp;

		 
		if (pr->pr_parent != &prison0) {
			zjp = zfs_jailparam_find(pr->pr_parent, &ppr);
			mtx_unlock(&ppr->pr_mtx);
			if (zjp->mount_snapshot < mount_snapshot) {
				return (EPERM);
			}
		}
		zfs_jailparam_alloc(pr, &zjp);
		if (mount_snapshot != -1)
			zjp->mount_snapshot = mount_snapshot;
		mtx_unlock(&pr->pr_mtx);
		break;
	}
	case JAIL_SYS_INHERIT:
		 
		mtx_lock(&pr->pr_mtx);
		osd_jail_del(pr, zfs_jailparam_slot);
		mtx_unlock(&pr->pr_mtx);
		break;
	case -1:
		 
		break;
	}

	return (0);
}

static int
zfs_jailparam_check(void *obj __unused, void *data)
{
	struct vfsoptlist *opts = data;
	int error, jsys, mount_snapshot;

	 
	error = vfs_copyopt(opts, "zfs", &jsys, sizeof (jsys));
	if (error != ENOENT) {
		if (error != 0)
			return (error);
		if (jsys != JAIL_SYS_NEW && jsys != JAIL_SYS_INHERIT)
			return (EINVAL);
	}
	error = vfs_copyopt(opts, "zfs.mount_snapshot", &mount_snapshot,
	    sizeof (mount_snapshot));
	if (error != ENOENT) {
		if (error != 0)
			return (error);
		if (mount_snapshot != 0 && mount_snapshot != 1)
			return (EINVAL);
	}
	return (0);
}

static void
zfs_jailparam_destroy(void *data)
{

	free(data, M_PRISON);
}

static void
zfs_jailparam_sysinit(void *arg __unused)
{
	struct prison *pr;
	osd_method_t  methods[PR_MAXMETHOD] = {
		[PR_METHOD_CREATE] = zfs_jailparam_create,
		[PR_METHOD_GET] = zfs_jailparam_get,
		[PR_METHOD_SET] = zfs_jailparam_set,
		[PR_METHOD_CHECK] = zfs_jailparam_check,
	};

	zfs_jailparam_slot = osd_jail_register(zfs_jailparam_destroy, methods);
	 
	sx_slock(&allprison_lock);
	TAILQ_FOREACH(pr, &allprison, pr_list)
		zfs_jailparam_alloc(pr, NULL);
	sx_sunlock(&allprison_lock);
}

static void
zfs_jailparam_sysuninit(void *arg __unused)
{

	osd_jail_deregister(zfs_jailparam_slot);
}

SYSINIT(zfs_jailparam_sysinit, SI_SUB_DRIVERS, SI_ORDER_ANY,
	zfs_jailparam_sysinit, NULL);
SYSUNINIT(zfs_jailparam_sysuninit, SI_SUB_DRIVERS, SI_ORDER_ANY,
	zfs_jailparam_sysuninit, NULL);
