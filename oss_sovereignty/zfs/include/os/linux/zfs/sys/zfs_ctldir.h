#ifndef	_ZFS_CTLDIR_H
#define	_ZFS_CTLDIR_H
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/zfs_vfsops.h>
#include <sys/zfs_znode.h>
#define	ZFS_CTLDIR_NAME		".zfs"
#define	ZFS_SNAPDIR_NAME	"snapshot"
#define	ZFS_SHAREDIR_NAME	"shares"
#define	zfs_has_ctldir(zdp)	\
	((zdp)->z_id == ZTOZSB(zdp)->z_root && \
	(ZTOZSB(zdp)->z_ctldir != NULL))
#define	zfs_show_ctldir(zdp)	\
	(zfs_has_ctldir(zdp) && \
	(ZTOZSB(zdp)->z_show_ctldir))
extern int zfs_expire_snapshot;
extern int zfsctl_create(zfsvfs_t *);
extern void zfsctl_destroy(zfsvfs_t *);
extern struct inode *zfsctl_root(znode_t *);
extern void zfsctl_init(void);
extern void zfsctl_fini(void);
extern boolean_t zfsctl_is_node(struct inode *ip);
extern boolean_t zfsctl_is_snapdir(struct inode *ip);
extern int zfsctl_fid(struct inode *ip, fid_t *fidp);
extern int zfsctl_root_lookup(struct inode *dip, const char *name,
    struct inode **ipp, int flags, cred_t *cr, int *direntflags,
    pathname_t *realpnp);
extern int zfsctl_snapdir_lookup(struct inode *dip, const char *name,
    struct inode **ipp, int flags, cred_t *cr, int *direntflags,
    pathname_t *realpnp);
extern int zfsctl_snapdir_rename(struct inode *sdip, const char *sname,
    struct inode *tdip, const char *tname, cred_t *cr, int flags);
extern int zfsctl_snapdir_remove(struct inode *dip, const char *name,
    cred_t *cr, int flags);
extern int zfsctl_snapdir_mkdir(struct inode *dip, const char *dirname,
    vattr_t *vap, struct inode **ipp, cred_t *cr, int flags);
extern int zfsctl_snapshot_mount(struct path *path, int flags);
extern int zfsctl_snapshot_unmount(const char *snapname, int flags);
extern int zfsctl_snapshot_unmount_delay(spa_t *spa, uint64_t objsetid,
    int delay);
extern int zfsctl_snapdir_vget(struct super_block *sb, uint64_t objsetid,
    int gen, struct inode **ipp);
extern int zfsctl_shares_lookup(struct inode *dip, char *name,
    struct inode **ipp, int flags, cred_t *cr, int *direntflags,
    pathname_t *realpnp);
#define	ZFSCTL_INO_ROOT		0x0000FFFFFFFFFFFFULL
#define	ZFSCTL_INO_SHARES	0x0000FFFFFFFFFFFEULL
#define	ZFSCTL_INO_SNAPDIR	0x0000FFFFFFFFFFFDULL
#define	ZFSCTL_INO_SNAPDIRS	0x0000FFFFFFFFFFFCULL
#define	ZFSCTL_EXPIRE_SNAPSHOT	300
#endif	 
