#ifndef	_SYS_FS_ZFS_DIR_H
#define	_SYS_FS_ZFS_DIR_H
#include <sys/pathname.h>
#include <sys/dmu.h>
#include <sys/zfs_znode.h>
#ifdef	__cplusplus
extern "C" {
#endif
#define	ZNEW		0x0001		 
#define	ZEXISTS		0x0002		 
#define	ZSHARED		0x0004		 
#define	ZXATTR		0x0008		 
#define	ZRENAMING	0x0010		 
#define	ZCILOOK		0x0020		 
#define	ZCIEXACT	0x0040		 
#define	ZHAVELOCK	0x0080		 
#define	IS_ROOT_NODE	0x01		 
#define	IS_XATTR	0x02		 
extern int zfs_dirent_lookup(znode_t *, const char *, znode_t **, int);
extern int zfs_link_create(znode_t *, const char *, znode_t *, dmu_tx_t *, int);
extern int zfs_link_destroy(znode_t *, const char *, znode_t *, dmu_tx_t *, int,
    boolean_t *);
extern int zfs_dirlook(znode_t *, const char *name, znode_t **);
extern void zfs_mknode(znode_t *, vattr_t *, dmu_tx_t *, cred_t *,
    uint_t, znode_t **, zfs_acl_ids_t *);
extern void zfs_rmnode(znode_t *);
extern boolean_t zfs_dirempty(znode_t *);
extern void zfs_unlinked_add(znode_t *, dmu_tx_t *);
extern void zfs_unlinked_drain(zfsvfs_t *zfsvfs);
extern int zfs_sticky_remove_access(znode_t *, znode_t *, cred_t *cr);
extern int zfs_get_xattrdir(znode_t *, znode_t **, cred_t *, int);
extern int zfs_make_xattrdir(znode_t *, vattr_t *, znode_t **, cred_t *);
#ifdef	__cplusplus
}
#endif
#endif	 
