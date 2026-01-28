#ifndef	_SYS_FS_ZFS_ZNODE_H
#define	_SYS_FS_ZFS_ZNODE_H
#include <sys/zfs_acl.h>
#include <sys/zil.h>
#include <sys/zfs_project.h>
#ifdef	__cplusplus
extern "C" {
#endif
#define	ZFS_READONLY		0x0000000100000000ull
#define	ZFS_HIDDEN		0x0000000200000000ull
#define	ZFS_SYSTEM		0x0000000400000000ull
#define	ZFS_ARCHIVE		0x0000000800000000ull
#define	ZFS_IMMUTABLE		0x0000001000000000ull
#define	ZFS_NOUNLINK		0x0000002000000000ull
#define	ZFS_APPENDONLY		0x0000004000000000ull
#define	ZFS_NODUMP		0x0000008000000000ull
#define	ZFS_OPAQUE		0x0000010000000000ull
#define	ZFS_AV_QUARANTINED	0x0000020000000000ull
#define	ZFS_AV_MODIFIED		0x0000040000000000ull
#define	ZFS_REPARSE		0x0000080000000000ull
#define	ZFS_OFFLINE		0x0000100000000000ull
#define	ZFS_SPARSE		0x0000200000000000ull
#define	ZFS_PROJINHERIT		0x0000400000000000ull
#define	ZFS_PROJID		0x0000800000000000ull
#define	ZFS_ATTR_SET(zp, attr, value, pflags, tx) \
{ \
	if (value) \
		pflags |= attr; \
	else \
		pflags &= ~attr; \
	VERIFY(0 == sa_update(zp->z_sa_hdl, SA_ZPL_FLAGS(ZTOZSB(zp)), \
	    &pflags, sizeof (pflags), tx)); \
}
#define	ZFS_XATTR		0x1		 
#define	ZFS_INHERIT_ACE		0x2		 
#define	ZFS_ACL_TRIVIAL		0x4		 
#define	ZFS_ACL_OBJ_ACE		0x8		 
#define	ZFS_ACL_PROTECTED	0x10		 
#define	ZFS_ACL_DEFAULTED	0x20		 
#define	ZFS_ACL_AUTO_INHERIT	0x40		 
#define	ZFS_BONUS_SCANSTAMP	0x80		 
#define	ZFS_NO_EXECS_DENIED	0x100		 
#define	SA_ZPL_ATIME(z)		z->z_attr_table[ZPL_ATIME]
#define	SA_ZPL_MTIME(z)		z->z_attr_table[ZPL_MTIME]
#define	SA_ZPL_CTIME(z)		z->z_attr_table[ZPL_CTIME]
#define	SA_ZPL_CRTIME(z)	z->z_attr_table[ZPL_CRTIME]
#define	SA_ZPL_GEN(z)		z->z_attr_table[ZPL_GEN]
#define	SA_ZPL_DACL_ACES(z)	z->z_attr_table[ZPL_DACL_ACES]
#define	SA_ZPL_XATTR(z)		z->z_attr_table[ZPL_XATTR]
#define	SA_ZPL_SYMLINK(z)	z->z_attr_table[ZPL_SYMLINK]
#define	SA_ZPL_RDEV(z)		z->z_attr_table[ZPL_RDEV]
#define	SA_ZPL_SCANSTAMP(z)	z->z_attr_table[ZPL_SCANSTAMP]
#define	SA_ZPL_UID(z)		z->z_attr_table[ZPL_UID]
#define	SA_ZPL_GID(z)		z->z_attr_table[ZPL_GID]
#define	SA_ZPL_PARENT(z)	z->z_attr_table[ZPL_PARENT]
#define	SA_ZPL_LINKS(z)		z->z_attr_table[ZPL_LINKS]
#define	SA_ZPL_MODE(z)		z->z_attr_table[ZPL_MODE]
#define	SA_ZPL_DACL_COUNT(z)	z->z_attr_table[ZPL_DACL_COUNT]
#define	SA_ZPL_FLAGS(z)		z->z_attr_table[ZPL_FLAGS]
#define	SA_ZPL_SIZE(z)		z->z_attr_table[ZPL_SIZE]
#define	SA_ZPL_ZNODE_ACL(z)	z->z_attr_table[ZPL_ZNODE_ACL]
#define	SA_ZPL_DXATTR(z)	z->z_attr_table[ZPL_DXATTR]
#define	SA_ZPL_PAD(z)		z->z_attr_table[ZPL_PAD]
#define	SA_ZPL_PROJID(z)	z->z_attr_table[ZPL_PROJID]
#define	IS_EPHEMERAL(x)		(x > MAXUID)
#define	USE_FUIDS(version, os)	(version >= ZPL_VERSION_FUID && \
    spa_version(dmu_objset_spa(os)) >= SPA_VERSION_FUID)
#define	USE_SA(version, os) (version >= ZPL_VERSION_SA && \
    spa_version(dmu_objset_spa(os)) >= SPA_VERSION_SA)
#define	MASTER_NODE_OBJ	1
#define	ZFS_FSID		"FSID"
#define	ZFS_UNLINKED_SET	"DELETE_QUEUE"
#define	ZFS_ROOT_OBJ		"ROOT"
#define	ZPL_VERSION_STR		"VERSION"
#define	ZFS_FUID_TABLES		"FUID"
#define	ZFS_SHARES_DIR		"SHARES"
#define	ZFS_SA_ATTRS		"SA_ATTRS"
#ifndef IFTODT
#define	IFTODT(mode) (((mode) & S_IFMT) >> 12)
#endif
#define	ZFS_DIRENT_TYPE(de) BF64_GET(de, 60, 4)
#define	ZFS_DIRENT_OBJ(de) BF64_GET(de, 0, 48)
extern int zfs_obj_to_path(objset_t *osp, uint64_t obj, char *buf, int len);
extern int zfs_get_zplprop(objset_t *os, zfs_prop_t prop, uint64_t *value);
#ifdef _KERNEL
#include <sys/zfs_znode_impl.h>
typedef struct zfs_dirlock {
	char		*dl_name;	 
	uint32_t	dl_sharecnt;	 
	uint8_t		dl_namelock;	 
	uint16_t	dl_namesize;	 
	kcondvar_t	dl_cv;		 
	struct znode	*dl_dzp;	 
	struct zfs_dirlock *dl_next;	 
} zfs_dirlock_t;
typedef struct znode {
	uint64_t	z_id;		 
	kmutex_t	z_lock;		 
	krwlock_t	z_parent_lock;	 
	krwlock_t	z_name_lock;	 
	zfs_dirlock_t	*z_dirlocks;	 
	zfs_rangelock_t	z_rangelock;	 
	boolean_t	z_unlinked;	 
	boolean_t	z_atime_dirty;	 
	boolean_t	z_zn_prefetch;	 
	boolean_t	z_is_sa;	 
	boolean_t	z_is_ctldir;	 
	boolean_t	z_suspended;	 
	uint_t		z_blksz;	 
	uint_t		z_seq;		 
	uint64_t	z_mapcnt;	 
	uint64_t	z_dnodesize;	 
	uint64_t	z_size;		 
	uint64_t	z_pflags;	 
	uint32_t	z_sync_cnt;	 
	uint32_t	z_sync_writes_cnt;  
	uint32_t	z_async_writes_cnt;  
	mode_t		z_mode;		 
	kmutex_t	z_acl_lock;	 
	zfs_acl_t	*z_acl_cached;	 
	krwlock_t	z_xattr_lock;	 
	nvlist_t	*z_xattr_cached;  
	uint64_t	z_xattr_parent;	 
	uint64_t	z_projid;	 
	list_node_t	z_link_node;	 
	sa_handle_t	*z_sa_hdl;	 
	ZNODE_OS_FIELDS;
} znode_t;
static inline int
zfs_verify_zp(znode_t *zp)
{
	if (unlikely(zp->z_sa_hdl == NULL))
		return (SET_ERROR(EIO));
	return (0);
}
static inline int
zfs_enter_verify_zp(zfsvfs_t *zfsvfs, znode_t *zp, const char *tag)
{
	int error;
	if ((error = zfs_enter(zfsvfs, tag)) != 0)
		return (error);
	if ((error = zfs_verify_zp(zp)) != 0) {
		zfs_exit(zfsvfs, tag);
		return (error);
	}
	return (0);
}
typedef struct znode_hold {
	uint64_t	zh_obj;		 
	avl_node_t	zh_node;	 
	kmutex_t	zh_lock;	 
	int		zh_refcount;	 
} znode_hold_t;
static inline uint64_t
zfs_inherit_projid(znode_t *dzp)
{
	return ((dzp->z_pflags & ZFS_PROJINHERIT) ? dzp->z_projid :
	    ZFS_DEFAULT_PROJID);
}
#define	ACCESSED		(ATTR_ATIME)
#define	STATE_CHANGED		(ATTR_CTIME)
#define	CONTENT_MODIFIED	(ATTR_MTIME | ATTR_CTIME)
extern int	zfs_init_fs(zfsvfs_t *, znode_t **);
extern void	zfs_set_dataprop(objset_t *);
extern void	zfs_create_fs(objset_t *os, cred_t *cr, nvlist_t *,
    dmu_tx_t *tx);
extern void	zfs_tstamp_update_setup(znode_t *, uint_t, uint64_t [2],
    uint64_t [2]);
extern void	zfs_grow_blocksize(znode_t *, uint64_t, dmu_tx_t *);
extern int	zfs_freesp(znode_t *, uint64_t, uint64_t, int, boolean_t);
extern void	zfs_znode_init(void);
extern void	zfs_znode_fini(void);
extern int	zfs_znode_hold_compare(const void *, const void *);
extern znode_hold_t *zfs_znode_hold_enter(zfsvfs_t *, uint64_t);
extern void	zfs_znode_hold_exit(zfsvfs_t *, znode_hold_t *);
extern int	zfs_zget(zfsvfs_t *, uint64_t, znode_t **);
extern int	zfs_rezget(znode_t *);
extern void	zfs_zinactive(znode_t *);
extern void	zfs_znode_delete(znode_t *, dmu_tx_t *);
extern void	zfs_remove_op_tables(void);
extern int	zfs_create_op_tables(void);
extern dev_t	zfs_cmpldev(uint64_t);
extern int	zfs_get_stats(objset_t *os, nvlist_t *nv);
extern boolean_t zfs_get_vfs_flag_unmounted(objset_t *os);
extern void	zfs_znode_dmu_fini(znode_t *);
extern void zfs_log_create(zilog_t *zilog, dmu_tx_t *tx, uint64_t txtype,
    znode_t *dzp, znode_t *zp, const char *name, vsecattr_t *,
    zfs_fuid_info_t *, vattr_t *vap);
extern int zfs_log_create_txtype(zil_create_t, vsecattr_t *vsecp,
    vattr_t *vap);
extern void zfs_log_remove(zilog_t *zilog, dmu_tx_t *tx, uint64_t txtype,
    znode_t *dzp, const char *name, uint64_t foid, boolean_t unlinked);
#define	ZFS_NO_OBJECT	0	 
extern void zfs_log_link(zilog_t *zilog, dmu_tx_t *tx, uint64_t txtype,
    znode_t *dzp, znode_t *zp, const char *name);
extern void zfs_log_symlink(zilog_t *zilog, dmu_tx_t *tx, uint64_t txtype,
    znode_t *dzp, znode_t *zp, const char *name, const char *link);
extern void zfs_log_rename(zilog_t *zilog, dmu_tx_t *tx, uint64_t txtype,
    znode_t *sdzp, const char *sname, znode_t *tdzp, const char *dname,
    znode_t *szp);
extern void zfs_log_rename_exchange(zilog_t *zilog, dmu_tx_t *tx,
    uint64_t txtype, znode_t *sdzp, const char *sname, znode_t *tdzp,
    const char *dname, znode_t *szp);
extern void zfs_log_rename_whiteout(zilog_t *zilog, dmu_tx_t *tx,
    uint64_t txtype, znode_t *sdzp, const char *sname, znode_t *tdzp,
    const char *dname, znode_t *szp, znode_t *wzp);
extern void zfs_log_write(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *zp, offset_t off, ssize_t len, int ioflag,
    zil_callback_t callback, void *callback_data);
extern void zfs_log_truncate(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *zp, uint64_t off, uint64_t len);
extern void zfs_log_setattr(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *zp, vattr_t *vap, uint_t mask_applied, zfs_fuid_info_t *fuidp);
extern void zfs_log_acl(zilog_t *zilog, dmu_tx_t *tx, znode_t *zp,
    vsecattr_t *vsecp, zfs_fuid_info_t *fuidp);
extern void zfs_log_clone_range(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *zp, uint64_t offset, uint64_t length, uint64_t blksz,
    const blkptr_t *bps, size_t nbps);
extern void zfs_xvattr_set(znode_t *zp, xvattr_t *xvap, dmu_tx_t *tx);
extern void zfs_upgrade(zfsvfs_t *zfsvfs, dmu_tx_t *tx);
extern void zfs_log_setsaxattr(zilog_t *zilog, dmu_tx_t *tx, int txtype,
    znode_t *zp, const char *name, const void *value, size_t size);
extern void zfs_znode_update_vfs(struct znode *);
#endif
#ifdef	__cplusplus
}
#endif
#endif	 
