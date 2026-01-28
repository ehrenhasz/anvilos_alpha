#ifndef	_ZFS_CHKSUM_H
#define	_ZFS_CHKSUM_H
#ifdef  _KERNEL
#include <sys/types.h>
#else
#include <stdint.h>
#include <stdlib.h>
#endif
#ifdef	__cplusplus
extern "C" {
#endif
void chksum_init(void);
void chksum_fini(void);
#ifdef	__cplusplus
}
#endif
#endif	 
