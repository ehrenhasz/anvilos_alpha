

#ifndef _OPENSOLARIS_SYS_KMEM_H_
#define	_OPENSOLARIS_SYS_KMEM_H_

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/vmem.h>
#include <sys/counter.h>

#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

MALLOC_DECLARE(M_SOLARIS);

#define	POINTER_IS_VALID(p)	(!((uintptr_t)(p) & 0x3))
#define	POINTER_INVALIDATE(pp)	(*(pp) = (void *)((uintptr_t)(*(pp)) | 0x1))

#define	KM_SLEEP		M_WAITOK
#define	KM_PUSHPAGE		M_WAITOK
#define	KM_NOSLEEP		M_NOWAIT
#define	KM_NORMALPRI		0
#define	KMC_NODEBUG		UMA_ZONE_NODUMP

typedef struct vmem vmem_t;

extern char	*kmem_asprintf(const char *, ...)
    __attribute__((format(printf, 1, 2)));
extern char *kmem_vasprintf(const char *fmt, va_list ap)
    __attribute__((format(printf, 1, 0)));

extern int kmem_scnprintf(char *restrict str, size_t size,
    const char *restrict fmt, ...);

typedef struct kmem_cache {
	char		kc_name[32];
#if !defined(KMEM_DEBUG)
	uma_zone_t	kc_zone;
#else
	size_t		kc_size;
#endif
	int		(*kc_constructor)(void *, void *, int);
	void		(*kc_destructor)(void *, void *);
	void		*kc_private;
} kmem_cache_t;

extern uint64_t spl_kmem_cache_inuse(kmem_cache_t *cache);
extern uint64_t spl_kmem_cache_entry_size(kmem_cache_t *cache);

__attribute__((malloc, alloc_size(1)))
void *zfs_kmem_alloc(size_t size, int kmflags);
void zfs_kmem_free(void *buf, size_t size);
uint64_t kmem_size(void);
kmem_cache_t *kmem_cache_create(const char *name, size_t bufsize, size_t align,
    int (*constructor)(void *, void *, int), void (*destructor)(void *, void *),
    void (*reclaim)(void *) __unused, void *private, vmem_t *vmp, int cflags);
void kmem_cache_destroy(kmem_cache_t *cache);
__attribute__((malloc))
void *kmem_cache_alloc(kmem_cache_t *cache, int flags);
void kmem_cache_free(kmem_cache_t *cache, void *buf);
boolean_t kmem_cache_reap_active(void);
void kmem_cache_reap_soon(kmem_cache_t *);
void kmem_reap(void);
int kmem_debugging(void);
void *calloc(size_t n, size_t s);


#define	kmem_cache_reap_now kmem_cache_reap_soon
#define	freemem				vm_free_count()
#define	minfree				vm_cnt.v_free_min
#define	kmem_alloc(size, kmflags)	zfs_kmem_alloc((size), (kmflags))
#define	kmem_zalloc(size, kmflags)				\
	zfs_kmem_alloc((size), (kmflags) | M_ZERO)
#define	kmem_free(buf, size)		zfs_kmem_free((buf), (size))

#endif	

#ifdef _STANDALONE

typedef int kmem_cache_t;
#endif 

#endif	
