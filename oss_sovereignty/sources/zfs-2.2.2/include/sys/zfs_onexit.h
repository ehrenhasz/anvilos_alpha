



#ifndef	_SYS_ZFS_ONEXIT_H
#define	_SYS_ZFS_ONEXIT_H

#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL

typedef struct zfs_onexit {
	kmutex_t	zo_lock;
	list_t		zo_actions;
} zfs_onexit_t;

typedef struct zfs_onexit_action_node {
	list_node_t	za_link;
	void		(*za_func)(void *);
	void		*za_data;
} zfs_onexit_action_node_t;

extern void zfs_onexit_init(zfs_onexit_t **zo);
extern void zfs_onexit_destroy(zfs_onexit_t *zo);

#endif

extern zfs_file_t *zfs_onexit_fd_hold(int fd, minor_t *minorp);
extern void zfs_onexit_fd_rele(zfs_file_t *);
extern int zfs_onexit_add_cb(minor_t minor, void (*func)(void *), void *data,
    uintptr_t *action_handle);

#ifdef	__cplusplus
}
#endif

#endif	
