#include <sys/types.h>
#include <sys/pathname.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
void
pn_alloc(struct pathname *pnp)
{
	pn_alloc_sz(pnp, MAXPATHLEN);
}
void
pn_alloc_sz(struct pathname *pnp, size_t sz)
{
	pnp->pn_buf = kmem_alloc(sz, KM_SLEEP);
	pnp->pn_bufsize = sz;
}
void
pn_free(struct pathname *pnp)
{
	kmem_free(pnp->pn_buf, pnp->pn_bufsize);
	pnp->pn_buf = NULL;
	pnp->pn_bufsize = 0;
}
