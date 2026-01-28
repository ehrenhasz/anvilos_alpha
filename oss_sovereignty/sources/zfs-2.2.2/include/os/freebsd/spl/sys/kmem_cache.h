


#ifndef _SPL_KMEM_CACHE_H
#define	_SPL_KMEM_CACHE_H

#ifdef _KERNEL
#include <sys/taskq.h>


typedef enum kmem_cbrc {
	KMEM_CBRC_YES		= 0,	
	KMEM_CBRC_NO		= 1,	
	KMEM_CBRC_LATER		= 2,	
	KMEM_CBRC_DONT_NEED	= 3,	
	KMEM_CBRC_DONT_KNOW	= 4,	
} kmem_cbrc_t;

extern void spl_kmem_cache_set_move(kmem_cache_t *,
    kmem_cbrc_t (*)(void *, void *, size_t, void *));

#define	kmem_cache_set_move(skc, move)	spl_kmem_cache_set_move(skc, move)

#endif 

#endif
