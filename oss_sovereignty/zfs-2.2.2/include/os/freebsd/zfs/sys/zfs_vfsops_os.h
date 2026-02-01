 
 

#ifndef	_SYS_FS_ZFS_VFSOPS_H
#define	_SYS_FS_ZFS_VFSOPS_H

#if __FreeBSD_version >= 1300125
#define	TEARDOWN_RMS
#endif

#if __FreeBSD_version >= 1300109
#define	TEARDOWN_INACTIVE_RMS
#endif

#include <sys/dataset_kstats.h>
#include <sys/list.h>
#include <sys/vfs.h>
#include <sys/zil.h>
#include <sys/sa.h>
#include <sys/rrwlock.h>
#ifdef TEARDOWN_INACTIVE_RMS
#include <sys/rmlock.h>
#endif
#include <sys/zfs_ioctl.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef TEARDOWN_RMS
typedef struct rmslock zfs_teardown_lock_t;
#else
#define	zfs_teardown_lock_t		rrmlock_t
#endif

#ifdef TEARDOWN_INACTIVE_RMS
typedef struct rmslock zfs_teardown_inactive_lock_t;
#else
#define	zfs_teardown_inactive_lock_t krwlock_t
#endif

typedef struct zfsvfs zfsvfs_t;
struct znode;

struct zfsvfs {
	vfs_t		*z_vfs;		 
	zfsvfs_t	*z_parent;	 
	objset_t	*z_os;		 
	uint64_t	z_flags;	 
	uint64_t	z_root;		 
	uint64_t	z_unlinkedobj;	 
	uint64_t	z_max_blksz;	 
	uint64_t	z_fuid_obj;	 
	uint64_t	z_fuid_size;	 
	avl_tree_t	z_fuid_idx;	 
	avl_tree_t	z_fuid_domain;	 
	krwlock_t	z_fuid_lock;	 
	boolean_t	z_fuid_loaded;	 
	boolean_t	z_fuid_dirty;    
	struct zfs_fuid_info	*z_fuid_replay;  
	zilog_t		*z_log;		 
	uint_t		z_acl_type;	 
	uint_t		z_acl_mode;	 
	uint_t		z_acl_inherit;	 
	zfs_case_t	z_case;		 
	boolean_t	z_utf8;		 
	int		z_norm;		 
	boolean_t	z_atime;	 
	boolean_t	z_unmounted;	 
	zfs_teardown_lock_t z_teardown_lock;
	zfs_teardown_inactive_lock_t z_teardown_inactive_lock;
	list_t		z_all_znodes;	 
	kmutex_t	z_znodes_lock;	 
	struct zfsctl_root	*z_ctldir;	 
	boolean_t	z_show_ctldir;	 
	boolean_t	z_issnap;	 
	boolean_t	z_use_fuids;	 
	boolean_t	z_replay;	 
	boolean_t	z_use_sa;	 
	boolean_t	z_xattr_sa;	 
	boolean_t	z_use_namecache;  
	uint8_t		z_xattr;	 
	uint64_t	z_version;	 
	uint64_t	z_shares_dir;	 
	dataset_kstats_t	z_kstat;	 
	kmutex_t	z_lock;
	uint64_t	z_userquota_obj;
	uint64_t	z_groupquota_obj;
	uint64_t	z_userobjquota_obj;
	uint64_t	z_groupobjquota_obj;
	uint64_t	z_projectquota_obj;
	uint64_t	z_projectobjquota_obj;
	uint64_t	z_replay_eof;	 
	sa_attr_type_t	*z_attr_table;	 
#define	ZFS_OBJ_MTX_SZ	64
	kmutex_t	z_hold_mtx[ZFS_OBJ_MTX_SZ];	 
	struct task	z_unlinked_drain_task;
};

#ifdef TEARDOWN_RMS
#define	ZFS_TEARDOWN_INIT(zfsvfs)		\
	rms_init(&(zfsvfs)->z_teardown_lock, "zfs teardown")

#define	ZFS_TEARDOWN_DESTROY(zfsvfs)		\
	rms_destroy(&(zfsvfs)->z_teardown_lock)

#define	ZFS_TEARDOWN_ENTER_READ(zfsvfs, tag)	\
	rms_rlock(&(zfsvfs)->z_teardown_lock);

#define	ZFS_TEARDOWN_EXIT_READ(zfsvfs, tag)	\
	rms_runlock(&(zfsvfs)->z_teardown_lock)

#define	ZFS_TEARDOWN_ENTER_WRITE(zfsvfs, tag)	\
	rms_wlock(&(zfsvfs)->z_teardown_lock)

#define	ZFS_TEARDOWN_EXIT_WRITE(zfsvfs)		\
	rms_wunlock(&(zfsvfs)->z_teardown_lock)

#define	ZFS_TEARDOWN_EXIT(zfsvfs, tag)		\
	rms_unlock(&(zfsvfs)->z_teardown_lock)

#define	ZFS_TEARDOWN_READ_HELD(zfsvfs)		\
	rms_rowned(&(zfsvfs)->z_teardown_lock)

#define	ZFS_TEARDOWN_WRITE_HELD(zfsvfs)		\
	rms_wowned(&(zfsvfs)->z_teardown_lock)

#define	ZFS_TEARDOWN_HELD(zfsvfs)		\
	rms_owned_any(&(zfsvfs)->z_teardown_lock)
#else
#define	ZFS_TEARDOWN_INIT(zfsvfs)		\
	rrm_init(&(zfsvfs)->z_teardown_lock, B_FALSE)

#define	ZFS_TEARDOWN_DESTROY(zfsvfs)		\
	rrm_destroy(&(zfsvfs)->z_teardown_lock)

#define	ZFS_TEARDOWN_ENTER_READ(zfsvfs, tag)	\
	rrm_enter_read(&(zfsvfs)->z_teardown_lock, tag);

#define	ZFS_TEARDOWN_EXIT_READ(zfsvfs, tag)	\
	rrm_exit(&(zfsvfs)->z_teardown_lock, tag)

#define	ZFS_TEARDOWN_ENTER_WRITE(zfsvfs, tag)	\
	rrm_enter(&(zfsvfs)->z_teardown_lock, RW_WRITER, tag)

#define	ZFS_TEARDOWN_EXIT_WRITE(zfsvfs)		\
	rrm_exit(&(zfsvfs)->z_teardown_lock, tag)

#define	ZFS_TEARDOWN_EXIT(zfsvfs, tag)		\
	rrm_exit(&(zfsvfs)->z_teardown_lock, tag)

#define	ZFS_TEARDOWN_READ_HELD(zfsvfs)		\
	RRM_READ_HELD(&(zfsvfs)->z_teardown_lock)

#define	ZFS_TEARDOWN_WRITE_HELD(zfsvfs)		\
	RRM_WRITE_HELD(&(zfsvfs)->z_teardown_lock)

#define	ZFS_TEARDOWN_HELD(zfsvfs)		\
	RRM_LOCK_HELD(&(zfsvfs)->z_teardown_lock)
#endif

#ifdef TEARDOWN_INACTIVE_RMS
#define	ZFS_TEARDOWN_INACTIVE_INIT(zfsvfs)		\
	rms_init(&(zfsvfs)->z_teardown_inactive_lock, "zfs teardown inactive")

#define	ZFS_TEARDOWN_INACTIVE_DESTROY(zfsvfs)		\
	rms_destroy(&(zfsvfs)->z_teardown_inactive_lock)

#define	ZFS_TEARDOWN_INACTIVE_TRY_ENTER_READ(zfsvfs)	\
	rms_try_rlock(&(zfsvfs)->z_teardown_inactive_lock)

#define	ZFS_TEARDOWN_INACTIVE_ENTER_READ(zfsvfs)	\
	rms_rlock(&(zfsvfs)->z_teardown_inactive_lock)

#define	ZFS_TEARDOWN_INACTIVE_EXIT_READ(zfsvfs)		\
	rms_runlock(&(zfsvfs)->z_teardown_inactive_lock)

#define	ZFS_TEARDOWN_INACTIVE_ENTER_WRITE(zfsvfs)	\
	rms_wlock(&(zfsvfs)->z_teardown_inactive_lock)

#define	ZFS_TEARDOWN_INACTIVE_EXIT_WRITE(zfsvfs)	\
	rms_wunlock(&(zfsvfs)->z_teardown_inactive_lock)

#define	ZFS_TEARDOWN_INACTIVE_WRITE_HELD(zfsvfs)	\
	rms_wowned(&(zfsvfs)->z_teardown_inactive_lock)
#else
#define	ZFS_TEARDOWN_INACTIVE_INIT(zfsvfs)		\
	rw_init(&(zfsvfs)->z_teardown_inactive_lock, NULL, RW_DEFAULT, NULL)

#define	ZFS_TEARDOWN_INACTIVE_DESTROY(zfsvfs)		\
	rw_destroy(&(zfsvfs)->z_teardown_inactive_lock)

#define	ZFS_TEARDOWN_INACTIVE_TRY_ENTER_READ(zfsvfs)	\
	rw_tryenter(&(zfsvfs)->z_teardown_inactive_lock, RW_READER)

#define	ZFS_TEARDOWN_INACTIVE_ENTER_READ(zfsvfs)	\
	rw_enter(&(zfsvfs)->z_teardown_inactive_lock, RW_READER)

#define	ZFS_TEARDOWN_INACTIVE_EXIT_READ(zfsvfs)		\
	rw_exit(&(zfsvfs)->z_teardown_inactive_lock)

#define	ZFS_TEARDOWN_INACTIVE_ENTER_WRITE(zfsvfs)	\
	rw_enter(&(zfsvfs)->z_teardown_inactive_lock, RW_WRITER)

#define	ZFS_TEARDOWN_INACTIVE_EXIT_WRITE(zfsvfs)	\
	rw_exit(&(zfsvfs)->z_teardown_inactive_lock)

#define	ZFS_TEARDOWN_INACTIVE_WRITE_HELD(zfsvfs)	\
	RW_WRITE_HELD(&(zfsvfs)->z_teardown_inactive_lock)
#endif

#define	ZSB_XATTR	0x0001		 
 
typedef struct zfid_short {
	uint16_t	zf_len;
	uint8_t		zf_object[6];		 
	uint8_t		zf_gen[4];		 
} zfid_short_t;

 
typedef struct zfid_long {
	zfid_short_t	z_fid;
	uint8_t		zf_setid[6];		 
	uint8_t		zf_setgen[2];		 
} zfid_long_t;

#define	SHORT_FID_LEN	(sizeof (zfid_short_t) - sizeof (uint16_t))
#define	LONG_FID_LEN	(sizeof (zfid_long_t) - sizeof (uint16_t))

extern uint_t zfs_fsyncer_key;
extern int zfs_super_owner;
extern int zfs_bclone_enabled;

extern void zfs_init(void);
extern void zfs_fini(void);

extern int zfs_suspend_fs(zfsvfs_t *zfsvfs);
extern int zfs_resume_fs(zfsvfs_t *zfsvfs, struct dsl_dataset *ds);
extern int zfs_end_fs(zfsvfs_t *zfsvfs, struct dsl_dataset *ds);
extern int zfs_set_version(zfsvfs_t *zfsvfs, uint64_t newvers);
extern int zfsvfs_create(const char *name, boolean_t readonly, zfsvfs_t **zfvp);
extern int zfsvfs_create_impl(zfsvfs_t **zfvp, zfsvfs_t *zfsvfs, objset_t *os);
extern void zfsvfs_free(zfsvfs_t *zfsvfs);
extern int zfs_check_global_label(const char *dsname, const char *hexsl);
extern boolean_t zfs_is_readonly(zfsvfs_t *zfsvfs);
extern int zfs_get_temporary_prop(struct dsl_dataset *ds, zfs_prop_t zfs_prop,
    uint64_t *val, char *setpoint);
extern int zfs_busy(void);

#ifdef	__cplusplus
}
#endif

#endif	 
