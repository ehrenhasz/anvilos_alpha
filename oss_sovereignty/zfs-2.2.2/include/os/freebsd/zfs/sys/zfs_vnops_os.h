 

#ifndef	_SYS_FS_ZFS_VNOPS_OS_H
#define	_SYS_FS_ZFS_VNOPS_OS_H

int dmu_write_pages(objset_t *os, uint64_t object, uint64_t offset,
    uint64_t size, struct vm_page **ppa, dmu_tx_t *tx);
int dmu_read_pages(objset_t *os, uint64_t object, vm_page_t *ma, int count,
    int *rbehind, int *rahead, int last_size);
extern int zfs_remove(znode_t *dzp, const char *name, cred_t *cr, int flags);
extern int zfs_mkdir(znode_t *dzp, const char *dirname, vattr_t *vap,
    znode_t **zpp, cred_t *cr, int flags, vsecattr_t *vsecp, zidmap_t *mnt_ns);
extern int zfs_rmdir(znode_t *dzp, const char *name, znode_t *cwd,
    cred_t *cr, int flags);
extern int zfs_setattr(znode_t *zp, vattr_t *vap, int flag, cred_t *cr,
    zidmap_t *mnt_ns);
extern int zfs_rename(znode_t *sdzp, const char *snm, znode_t *tdzp,
    const char *tnm, cred_t *cr, int flags, uint64_t rflags, vattr_t *wo_vap,
    zidmap_t *mnt_ns);
extern int zfs_symlink(znode_t *dzp, const char *name, vattr_t *vap,
    const char *link, znode_t **zpp, cred_t *cr, int flags, zidmap_t *mnt_ns);
extern int zfs_link(znode_t *tdzp, znode_t *sp,
    const char *name, cred_t *cr, int flags);
extern int zfs_space(znode_t *zp, int cmd, struct flock *bfp, int flag,
    offset_t offset, cred_t *cr);
extern int zfs_create(znode_t *dzp, const char *name, vattr_t *vap, int excl,
    int mode, znode_t **zpp, cred_t *cr, int flag, vsecattr_t *vsecp,
    zidmap_t *mnt_ns);
extern int zfs_setsecattr(znode_t *zp, vsecattr_t *vsecp, int flag,
    cred_t *cr);
extern int zfs_write_simple(znode_t *zp, const void *data, size_t len,
    loff_t pos, size_t *resid);

#endif
