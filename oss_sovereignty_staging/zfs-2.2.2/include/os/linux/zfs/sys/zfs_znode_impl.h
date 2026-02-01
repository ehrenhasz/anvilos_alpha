 
 

#ifndef	_SYS_ZFS_ZNODE_IMPL_H
#define	_SYS_ZFS_ZNODE_IMPL_H

#ifndef _KERNEL
#error "no user serviceable parts within"
#endif

#include <sys/isa_defs.h>
#include <sys/types32.h>
#include <sys/list.h>
#include <sys/dmu.h>
#include <sys/sa.h>
#include <sys/time.h>
#include <sys/zfs_vfsops.h>
#include <sys/rrwlock.h>
#include <sys/zfs_sa.h>
#include <sys/zfs_stat.h>
#include <sys/zfs_rlock.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(HAVE_FILEMAP_RANGE_HAS_PAGE)
#define	ZNODE_OS_FIELDS			\
	inode_timespec_t z_btime;   \
	struct inode	z_inode;
#else
#define	ZNODE_OS_FIELDS			\
	inode_timespec_t z_btime;   \
	struct inode	z_inode;                                     \
	boolean_t	z_is_mapped;     
#endif

 
#define	ZTOI(znode)	(&((znode)->z_inode))
#define	ITOZ(inode)	(container_of((inode), znode_t, z_inode))
#define	ZTOZSB(znode)	((zfsvfs_t *)(ZTOI(znode)->i_sb->s_fs_info))
#define	ITOZSB(inode)	((zfsvfs_t *)((inode)->i_sb->s_fs_info))

#define	ZTOTYPE(zp)	(ZTOI(zp)->i_mode)
#define	ZTOGID(zp) (ZTOI(zp)->i_gid)
#define	ZTOUID(zp) (ZTOI(zp)->i_uid)
#define	ZTONLNK(zp) (ZTOI(zp)->i_nlink)

#define	Z_ISBLK(type) S_ISBLK(type)
#define	Z_ISCHR(type) S_ISCHR(type)
#define	Z_ISLNK(type) S_ISLNK(type)
#define	Z_ISDEV(type)	(S_ISCHR(type) || S_ISBLK(type) || S_ISFIFO(type))
#define	Z_ISDIR(type)	S_ISDIR(type)

#if defined(HAVE_FILEMAP_RANGE_HAS_PAGE)
#define	zn_has_cached_data(zp, start, end) \
	filemap_range_has_page(ZTOI(zp)->i_mapping, start, end)
#else
#define	zn_has_cached_data(zp, start, end) \
	((zp)->z_is_mapped)
#endif

#define	zn_flush_cached_data(zp, sync)	write_inode_now(ZTOI(zp), sync)
#define	zn_rlimit_fsize(size)		(0)
#define	zn_rlimit_fsize_uio(zp, uio)	(0)

 
#define	zhold(zp)	VERIFY3P(igrab(ZTOI((zp))), !=, NULL)
#define	zrele(zp)	iput(ZTOI((zp)))

 
static inline int
zfs_enter(zfsvfs_t *zfsvfs, const char *tag)
{
	ZFS_TEARDOWN_ENTER_READ(zfsvfs, tag);
	if (unlikely(zfsvfs->z_unmounted)) {
		ZFS_TEARDOWN_EXIT_READ(zfsvfs, tag);
		return (SET_ERROR(EIO));
	}
	return (0);
}

 
static inline void
zfs_exit(zfsvfs_t *zfsvfs, const char *tag)
{
	zfs_exit_fs(zfsvfs);
	ZFS_TEARDOWN_EXIT_READ(zfsvfs, tag);
}

static inline int
zpl_enter(zfsvfs_t *zfsvfs, const char *tag)
{
	return (-zfs_enter(zfsvfs, tag));
}

static inline void
zpl_exit(zfsvfs_t *zfsvfs, const char *tag)
{
	ZFS_TEARDOWN_EXIT_READ(zfsvfs, tag);
}

 
#define	zpl_verify_zp(zp)	(-zfs_verify_zp(zp))
#define	zpl_enter_verify_zp(zfsvfs, zp, tag)	\
	(-zfs_enter_verify_zp(zfsvfs, zp, tag))

 
#define	ZFS_OBJ_MTX_SZ		64
#define	ZFS_OBJ_MTX_MAX		(1024 * 1024)
#define	ZFS_OBJ_HASH(zfsvfs, obj)	((obj) & ((zfsvfs->z_hold_size) - 1))

extern unsigned int zfs_object_mutex_size;

 
#define	ZFS_TIME_ENCODE(tp, stmp)		\
do {						\
	(stmp)[0] = (uint64_t)(tp)->tv_sec;	\
	(stmp)[1] = (uint64_t)(tp)->tv_nsec;	\
} while (0)

#if defined(HAVE_INODE_TIMESPEC64_TIMES)
 
#define	ZFS_TIME_DECODE(tp, stmp)		\
do {						\
	(tp)->tv_sec = (time64_t)(stmp)[0];	\
	(tp)->tv_nsec = (long)(stmp)[1];	\
} while (0)
#else
 
#define	ZFS_TIME_DECODE(tp, stmp)		\
do {						\
	(tp)->tv_sec = (time_t)(stmp)[0];	\
	(tp)->tv_nsec = (long)(stmp)[1];	\
} while (0)
#endif  

#define	ZFS_ACCESSTIME_STAMP(zfsvfs, zp)

struct znode;

extern int	zfs_sync(struct super_block *, int, cred_t *);
extern int	zfs_inode_alloc(struct super_block *, struct inode **ip);
extern void	zfs_inode_destroy(struct inode *);
extern void	zfs_mark_inode_dirty(struct inode *);
extern boolean_t zfs_relatime_need_update(const struct inode *);

#if defined(HAVE_UIO_RW)
extern caddr_t zfs_map_page(page_t *, enum seg_rw);
extern void zfs_unmap_page(page_t *, caddr_t);
#endif  

extern zil_replay_func_t *const zfs_replay_vector[TX_MAX_TYPE];

#ifdef	__cplusplus
}
#endif

#endif	 
