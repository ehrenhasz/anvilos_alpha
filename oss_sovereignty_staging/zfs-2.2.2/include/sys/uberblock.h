 
 
 

#ifndef _SYS_UBERBLOCK_H
#define	_SYS_UBERBLOCK_H

#include <sys/spa.h>
#include <sys/vdev.h>
#include <sys/zio.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct uberblock uberblock_t;

extern int uberblock_verify(uberblock_t *);
extern boolean_t uberblock_update(uberblock_t *ub, vdev_t *rvd, uint64_t txg,
    uint64_t mmp_delay);

#ifdef	__cplusplus
}
#endif

#endif	 
