 
 

#ifndef _SYS_KMEM_H
#define	_SYS_KMEM_H

#include <stdlib.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	KM_SLEEP	0x00000000	 
#define	KM_NOSLEEP	0x00000001	 

#define	kmem_alloc(size, flags)		((void) sizeof (flags), malloc(size))
#define	kmem_free(ptr, size)		((void) sizeof (size), free(ptr))

#ifdef	__cplusplus
}
#endif

#endif	 
