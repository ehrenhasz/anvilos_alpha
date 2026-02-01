




















 

#ifndef	_SYS_CITYHASH_H
#define	_SYS_CITYHASH_H extern __attribute__((visibility("default")))

#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

_SYS_CITYHASH_H uint64_t cityhash4(uint64_t, uint64_t, uint64_t, uint64_t);

#ifdef	__cplusplus
}
#endif

#endif	 
