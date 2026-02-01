 

#ifndef _SPL_VMEM_H
#define	_SPL_VMEM_H

#include <sys/kmem.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>

typedef struct vmem { } vmem_t;

 
#define	VMEM_ALLOC	0x01
#define	VMEM_FREE	0x02

#ifndef VMALLOC_TOTAL
#define	VMALLOC_TOTAL	(VMALLOC_END - VMALLOC_START)
#endif

 

#define	vmem_alloc(sz, fl)	spl_vmem_alloc((sz), (fl), __func__, __LINE__)
#define	vmem_zalloc(sz, fl)	spl_vmem_zalloc((sz), (fl), __func__, __LINE__)
#define	vmem_free(ptr, sz)	spl_vmem_free((ptr), (sz))

extern void *spl_vmem_alloc(size_t sz, int fl, const char *func, int line)
    __attribute__((malloc, alloc_size(1)));
extern void *spl_vmem_zalloc(size_t sz, int fl, const char *func, int line)
    __attribute__((malloc, alloc_size(1)));
extern void spl_vmem_free(const void *ptr, size_t sz);

int spl_vmem_init(void);
void spl_vmem_fini(void);

#endif	 
