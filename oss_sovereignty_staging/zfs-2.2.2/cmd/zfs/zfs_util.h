 
 

#ifndef	_ZFS_UTIL_H
#define	_ZFS_UTIL_H

#include <libzfs.h>

#ifdef	__cplusplus
extern "C" {
#endif

void *safe_malloc(size_t size);
void nomem(void);
extern libzfs_handle_t *g_zfs;

#ifdef	__cplusplus
}
#endif

#endif	 
