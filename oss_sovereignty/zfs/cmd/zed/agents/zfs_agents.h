#ifndef	ZFS_AGENTS_H
#define	ZFS_AGENTS_H
#include <libzfs.h>
#include <libnvpair.h>
#ifdef	__cplusplus
extern "C" {
#endif
extern void zfs_agent_init(libzfs_handle_t *);
extern void zfs_agent_fini(void);
extern void zfs_agent_post_event(const char *, const char *, nvlist_t *);
extern int zfs_slm_init(void);
extern void zfs_slm_fini(void);
extern void zfs_slm_event(const char *, const char *, nvlist_t *);
#ifdef	__cplusplus
}
#endif
#endif	 
