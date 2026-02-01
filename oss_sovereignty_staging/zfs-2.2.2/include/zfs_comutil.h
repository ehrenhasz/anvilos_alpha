 
 

#ifndef	_ZFS_COMUTIL_H
#define	_ZFS_COMUTIL_H extern __attribute__((visibility("default")))

#include <sys/fs/zfs.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

_ZFS_COMUTIL_H boolean_t zfs_allocatable_devs(nvlist_t *);
_ZFS_COMUTIL_H boolean_t zfs_special_devs(nvlist_t *, const char *);
_ZFS_COMUTIL_H void zpool_get_load_policy(nvlist_t *, zpool_load_policy_t *);

_ZFS_COMUTIL_H int zfs_zpl_version_map(int spa_version);
_ZFS_COMUTIL_H int zfs_spa_version_map(int zpl_version);

_ZFS_COMUTIL_H boolean_t zfs_dataset_name_hidden(const char *);

#define	ZFS_NUM_LEGACY_HISTORY_EVENTS 41
_ZFS_COMUTIL_H const char *const
    zfs_history_event_names[ZFS_NUM_LEGACY_HISTORY_EVENTS];

#ifdef	__cplusplus
}
#endif

#endif	 
