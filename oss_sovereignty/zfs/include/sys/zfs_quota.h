#ifndef _SYS_ZFS_QUOTA_H
#define	_SYS_ZFS_QUOTA_H
#include <sys/dmu.h>
#include <sys/fs/zfs.h>
struct zfsvfs;
struct zfs_file_info_t;
extern int zpl_get_file_info(dmu_object_type_t,
    const void *, struct zfs_file_info *);
extern int zfs_userspace_one(struct zfsvfs *, zfs_userquota_prop_t,
    const char *, uint64_t, uint64_t *);
extern int zfs_userspace_many(struct zfsvfs *, zfs_userquota_prop_t,
    uint64_t *, void *, uint64_t *);
extern int zfs_set_userquota(struct zfsvfs *, zfs_userquota_prop_t,
    const char *, uint64_t, uint64_t);
extern boolean_t zfs_id_overobjquota(struct zfsvfs *, uint64_t, uint64_t);
extern boolean_t zfs_id_overblockquota(struct zfsvfs *, uint64_t, uint64_t);
extern boolean_t zfs_id_overquota(struct zfsvfs *, uint64_t, uint64_t);
#endif
