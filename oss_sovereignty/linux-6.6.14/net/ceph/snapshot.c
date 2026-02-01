
 

#include <linux/types.h>
#include <linux/export.h>
#include <linux/ceph/libceph.h>

 

 
struct ceph_snap_context *ceph_create_snap_context(u32 snap_count,
						gfp_t gfp_flags)
{
	struct ceph_snap_context *snapc;
	size_t size;

	size = sizeof (struct ceph_snap_context);
	size += snap_count * sizeof (snapc->snaps[0]);
	snapc = kzalloc(size, gfp_flags);
	if (!snapc)
		return NULL;

	refcount_set(&snapc->nref, 1);
	snapc->num_snaps = snap_count;

	return snapc;
}
EXPORT_SYMBOL(ceph_create_snap_context);

struct ceph_snap_context *ceph_get_snap_context(struct ceph_snap_context *sc)
{
	if (sc)
		refcount_inc(&sc->nref);
	return sc;
}
EXPORT_SYMBOL(ceph_get_snap_context);

void ceph_put_snap_context(struct ceph_snap_context *sc)
{
	if (!sc)
		return;
	if (refcount_dec_and_test(&sc->nref)) {
		 
		kfree(sc);
	}
}
EXPORT_SYMBOL(ceph_put_snap_context);
