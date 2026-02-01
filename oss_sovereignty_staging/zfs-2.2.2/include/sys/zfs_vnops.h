 
 

#ifndef	_SYS_FS_ZFS_VNOPS_H
#define	_SYS_FS_ZFS_VNOPS_H
#include <sys/zfs_vnops_os.h>

extern int zfs_fsync(znode_t *, int, cred_t *);
extern int zfs_read(znode_t *, zfs_uio_t *, int, cred_t *);
extern int zfs_write(znode_t *, zfs_uio_t *, int, cred_t *);
extern int zfs_holey(znode_t *, ulong_t, loff_t *);
extern int zfs_access(znode_t *, int, int, cred_t *);
extern int zfs_clone_range(znode_t *, uint64_t *, znode_t *, uint64_t *,
    uint64_t *, cred_t *);
extern int zfs_clone_range_replay(znode_t *, uint64_t, uint64_t, uint64_t,
    const blkptr_t *, size_t);

extern int zfs_getsecattr(znode_t *, vsecattr_t *, int, cred_t *);
extern int zfs_setsecattr(znode_t *, vsecattr_t *, int, cred_t *);

extern int mappedread(znode_t *, int, zfs_uio_t *);
extern int mappedread_sf(znode_t *, int, zfs_uio_t *);
extern void update_pages(znode_t *, int64_t, int, objset_t *);

 
extern void zfs_zrele_async(znode_t *zp);

extern zil_get_data_t zfs_get_data;

#endif
