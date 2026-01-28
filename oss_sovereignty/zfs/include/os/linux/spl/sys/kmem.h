#ifndef _SPL_KMEM_H
#define	_SPL_KMEM_H
#include <sys/debug.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
extern int kmem_debugging(void);
__attribute__((format(printf, 1, 0)))
extern char *kmem_vasprintf(const char *fmt, va_list ap);
__attribute__((format(printf, 1, 2)))
extern char *kmem_asprintf(const char *fmt, ...);
extern char *kmem_strdup(const char *str);
extern void kmem_strfree(char *str);
#define	kmem_scnprintf	scnprintf
#define	POINTER_IS_VALID(p)	(!((uintptr_t)(p) & 0x3))
#define	POINTER_INVALIDATE(pp)	(*(pp) = (void *)((uintptr_t)(*(pp)) | 0x1))
#define	KM_SLEEP	0x0000	 
#define	KM_NOSLEEP	0x0001	 
#define	KM_PUSHPAGE	0x0004	 
#define	KM_ZERO		0x1000	 
#define	KM_VMEM		0x2000	 
#define	KM_PUBLIC_MASK	(KM_SLEEP | KM_NOSLEEP | KM_PUSHPAGE)
static int spl_fstrans_check(void);
void *spl_kvmalloc(size_t size, gfp_t flags);
static inline gfp_t
kmem_flags_convert(int flags)
{
	gfp_t lflags = __GFP_NOWARN | __GFP_COMP;
	if (flags & KM_NOSLEEP) {
		lflags |= GFP_ATOMIC | __GFP_NORETRY;
	} else {
		lflags |= GFP_KERNEL;
		if (spl_fstrans_check())
			lflags &= ~(__GFP_IO|__GFP_FS);
	}
	if (flags & KM_PUSHPAGE)
		lflags |= __GFP_HIGH;
	if (flags & KM_ZERO)
		lflags |= __GFP_ZERO;
	return (lflags);
}
typedef struct {
	struct task_struct *fstrans_thread;
	unsigned int saved_flags;
} fstrans_cookie_t;
#ifdef PF_MEMALLOC_NOIO
#define	__SPL_PF_MEMALLOC_NOIO (PF_MEMALLOC_NOIO)
#else
#define	__SPL_PF_MEMALLOC_NOIO (0)
#endif
#ifdef PF_FSTRANS
#define	__SPL_PF_FSTRANS (PF_FSTRANS)
#else
#define	__SPL_PF_FSTRANS (0)
#endif
#define	SPL_FSTRANS (__SPL_PF_FSTRANS|__SPL_PF_MEMALLOC_NOIO)
static inline fstrans_cookie_t
spl_fstrans_mark(void)
{
	fstrans_cookie_t cookie;
	BUILD_BUG_ON(SPL_FSTRANS == 0);
	cookie.fstrans_thread = current;
	cookie.saved_flags = current->flags & SPL_FSTRANS;
	current->flags |= SPL_FSTRANS;
	return (cookie);
}
static inline void
spl_fstrans_unmark(fstrans_cookie_t cookie)
{
	ASSERT3P(cookie.fstrans_thread, ==, current);
	ASSERT((current->flags & SPL_FSTRANS) == SPL_FSTRANS);
	current->flags &= ~SPL_FSTRANS;
	current->flags |= cookie.saved_flags;
}
static inline int
spl_fstrans_check(void)
{
	return (current->flags & SPL_FSTRANS);
}
static inline int
__spl_pf_fstrans_check(void)
{
	return (current->flags & __SPL_PF_FSTRANS);
}
#ifndef __GFP_RETRY_MAYFAIL
#define	__GFP_RETRY_MAYFAIL	__GFP_REPEAT
#endif
#ifndef __GFP_RECLAIM
#define	__GFP_RECLAIM		__GFP_WAIT
#endif
#ifdef HAVE_ATOMIC64_T
#define	kmem_alloc_used_add(size)	atomic64_add(size, &kmem_alloc_used)
#define	kmem_alloc_used_sub(size)	atomic64_sub(size, &kmem_alloc_used)
#define	kmem_alloc_used_read()		atomic64_read(&kmem_alloc_used)
#define	kmem_alloc_used_set(size)	atomic64_set(&kmem_alloc_used, size)
extern atomic64_t kmem_alloc_used;
extern unsigned long long kmem_alloc_max;
#else   
#define	kmem_alloc_used_add(size)	atomic_add(size, &kmem_alloc_used)
#define	kmem_alloc_used_sub(size)	atomic_sub(size, &kmem_alloc_used)
#define	kmem_alloc_used_read()		atomic_read(&kmem_alloc_used)
#define	kmem_alloc_used_set(size)	atomic_set(&kmem_alloc_used, size)
extern atomic_t kmem_alloc_used;
extern unsigned long long kmem_alloc_max;
#endif  
extern unsigned int spl_kmem_alloc_warn;
extern unsigned int spl_kmem_alloc_max;
#define	kmem_alloc(sz, fl)	spl_kmem_alloc((sz), (fl), __func__, __LINE__)
#define	kmem_zalloc(sz, fl)	spl_kmem_zalloc((sz), (fl), __func__, __LINE__)
#define	kmem_free(ptr, sz)	spl_kmem_free((ptr), (sz))
#define	kmem_cache_reap_active	spl_kmem_cache_reap_active
__attribute__((malloc, alloc_size(1)))
extern void *spl_kmem_alloc(size_t sz, int fl, const char *func, int line);
__attribute__((malloc, alloc_size(1)))
extern void *spl_kmem_zalloc(size_t sz, int fl, const char *func, int line);
extern void spl_kmem_free(const void *ptr, size_t sz);
#ifdef HAVE_VMALLOC_PAGE_KERNEL
#define	spl_vmalloc(size, flags)	__vmalloc(size, flags, PAGE_KERNEL)
#else
#define	spl_vmalloc(size, flags)	__vmalloc(size, flags)
#endif
extern void *spl_kmem_alloc_impl(size_t size, int flags, int node);
extern void *spl_kmem_alloc_debug(size_t size, int flags, int node);
extern void *spl_kmem_alloc_track(size_t size, int flags,
    const char *func, int line, int node);
extern void spl_kmem_free_impl(const void *buf, size_t size);
extern void spl_kmem_free_debug(const void *buf, size_t size);
extern void spl_kmem_free_track(const void *buf, size_t size);
extern int spl_kmem_init(void);
extern void spl_kmem_fini(void);
extern int spl_kmem_cache_reap_active(void);
#endif	 
