#ifndef	_FREEBSD_ZFS_SYS_ZNODE_IMPL_H
#define	_FREEBSD_ZFS_SYS_ZNODE_IMPL_H
#include <sys/list.h>
#include <sys/dmu.h>
#include <sys/sa.h>
#include <sys/zfs_vfsops.h>
#include <sys/rrwlock.h>
#include <sys/zfs_sa.h>
#include <sys/zfs_stat.h>
#include <sys/zfs_rlock.h>
#include <sys/zfs_acl.h>
#include <sys/zil.h>
#include <sys/zfs_project.h>
#include <vm/vm_object.h>
#include <sys/uio.h>
#ifdef	__cplusplus
extern "C" {
#endif
#define	ZNODE_OS_FIELDS                 \
	struct zfsvfs	*z_zfsvfs;      \
	vnode_t		*z_vnode;       \
	char		*z_cached_symlink;	\
	uint64_t		z_uid;          \
	uint64_t		z_gid;          \
	uint64_t		z_gen;          \
	uint64_t		z_atime[2];     \
	uint64_t		z_links;
#define	ZFS_LINK_MAX	UINT64_MAX
enum zfs_soft_state_type {
	ZSST_ZVOL,
	ZSST_CTLDEV
};
typedef struct zfs_soft_state {
	enum zfs_soft_state_type zss_type;
	void *zss_data;
} zfs_soft_state_t;
#define	ZTOV(ZP)	((ZP)->z_vnode)
#define	ZTOI(ZP)	((ZP)->z_vnode)
#define	VTOZ(VP)	((struct znode *)(VP)->v_data)
#define	VTOZ_SMR(VP)	((znode_t *)vn_load_v_data_smr(VP))
#define	ITOZ(VP)	((struct znode *)(VP)->v_data)
#define	zhold(zp)	vhold(ZTOV((zp)))
#define	zrele(zp)	vrele(ZTOV((zp)))
#define	ZTOZSB(zp) ((zp)->z_zfsvfs)
#define	ITOZSB(vp) (VTOZ(vp)->z_zfsvfs)
#define	ZTOTYPE(zp)	(ZTOV(zp)->v_type)
#define	ZTOGID(zp) ((zp)->z_gid)
#define	ZTOUID(zp) ((zp)->z_uid)
#define	ZTONLNK(zp) ((zp)->z_links)
#define	Z_ISBLK(type) ((type) == VBLK)
#define	Z_ISCHR(type) ((type) == VCHR)
#define	Z_ISLNK(type) ((type) == VLNK)
#define	Z_ISDIR(type) ((type) == VDIR)
#define	zn_has_cached_data(zp, start, end) \
    vn_has_cached_data(ZTOV(zp))
#define	zn_flush_cached_data(zp, sync)	vn_flush_cached_data(ZTOV(zp), sync)
#define	zn_rlimit_fsize(size)		zfs_rlimit_fsize(size)
#define	zn_rlimit_fsize_uio(zp, uio) \
    vn_rlimit_fsize(ZTOV(zp), GET_UIO_STRUCT(uio), zfs_uio_td(uio))
static inline int
zfs_enter(zfsvfs_t *zfsvfs, const char *tag)
{
	ZFS_TEARDOWN_ENTER_READ(zfsvfs, tag);
	if (__predict_false((zfsvfs)->z_unmounted)) {
		ZFS_TEARDOWN_EXIT_READ(zfsvfs, tag);
		return (SET_ERROR(EIO));
	}
	return (0);
}
static inline void
zfs_exit(zfsvfs_t *zfsvfs, const char *tag)
{
	ZFS_TEARDOWN_EXIT_READ(zfsvfs, tag);
}
#define	ZFS_OBJ_HASH(obj_num)	((obj_num) & (ZFS_OBJ_MTX_SZ - 1))
#define	ZFS_OBJ_MUTEX(zfsvfs, obj_num)	\
	(&(zfsvfs)->z_hold_mtx[ZFS_OBJ_HASH(obj_num)])
#define	ZFS_OBJ_HOLD_ENTER(zfsvfs, obj_num) \
	mutex_enter(ZFS_OBJ_MUTEX((zfsvfs), (obj_num)))
#define	ZFS_OBJ_HOLD_TRYENTER(zfsvfs, obj_num) \
	mutex_tryenter(ZFS_OBJ_MUTEX((zfsvfs), (obj_num)))
#define	ZFS_OBJ_HOLD_EXIT(zfsvfs, obj_num) \
	mutex_exit(ZFS_OBJ_MUTEX((zfsvfs), (obj_num)))
#define	ZFS_TIME_ENCODE(tp, stmp)		\
{						\
	(stmp)[0] = (uint64_t)(tp)->tv_sec;	\
	(stmp)[1] = (uint64_t)(tp)->tv_nsec;	\
}
#define	ZFS_TIME_DECODE(tp, stmp)		\
{						\
	(tp)->tv_sec = (time_t)(stmp)[0];		\
	(tp)->tv_nsec = (long)(stmp)[1];		\
}
#define	ZFS_ACCESSTIME_STAMP(zfsvfs, zp) \
	if ((zfsvfs)->z_atime && !((zfsvfs)->z_vfs->vfs_flag & VFS_RDONLY)) \
		zfs_tstamp_update_setup_ext(zp, ACCESSED, NULL, NULL, B_FALSE);
extern void	zfs_tstamp_update_setup_ext(struct znode *,
    uint_t, uint64_t [2], uint64_t [2], boolean_t have_tx);
extern void zfs_znode_free(struct znode *);
extern zil_replay_func_t *const zfs_replay_vector[TX_MAX_TYPE];
extern int zfs_znode_parent_and_name(struct znode *zp, struct znode **dzpp,
    char *buf);
extern int zfs_rlimit_fsize(off_t fsize);
#ifdef	__cplusplus
}
#endif
#endif	 
