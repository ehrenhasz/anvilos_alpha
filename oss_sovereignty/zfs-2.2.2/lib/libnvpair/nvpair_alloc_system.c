 
 



#include <rpc/types.h>
#include <sys/kmem.h>
#include <sys/nvpair.h>

static void *
nv_alloc_sys(nv_alloc_t *nva, size_t size)
{
	return (kmem_alloc(size, (int)(uintptr_t)nva->nva_arg));
}

static void
nv_free_sys(nv_alloc_t *nva, void *buf, size_t size)
{
	(void) nva;
	kmem_free(buf, size);
}

static const nv_alloc_ops_t system_ops = {
	NULL,			 
	NULL,			 
	nv_alloc_sys,		 
	nv_free_sys,		 
	NULL			 
};

static nv_alloc_t nv_alloc_sleep_def = {
	&system_ops,
	(void *)KM_SLEEP
};

static nv_alloc_t nv_alloc_nosleep_def = {
	&system_ops,
	(void *)KM_NOSLEEP
};

nv_alloc_t *const nv_alloc_sleep = &nv_alloc_sleep_def;
nv_alloc_t *const nv_alloc_nosleep = &nv_alloc_nosleep_def;
