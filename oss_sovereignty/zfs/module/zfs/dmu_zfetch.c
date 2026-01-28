#include <sys/zfs_context.h>
#include <sys/arc_impl.h>
#include <sys/dnode.h>
#include <sys/dmu_objset.h>
#include <sys/dmu_zfetch.h>
#include <sys/dmu.h>
#include <sys/dbuf.h>
#include <sys/kstat.h>
#include <sys/wmsum.h>
static int zfs_prefetch_disable = B_FALSE;
static unsigned int	zfetch_max_streams = 8;
static unsigned int	zfetch_min_sec_reap = 1;
static unsigned int	zfetch_max_sec_reap = 2;
#ifdef _ILP32
static unsigned int	zfetch_min_distance = 2 * 1024 * 1024;
unsigned int	zfetch_max_distance = 8 * 1024 * 1024;
#else
static unsigned int	zfetch_min_distance = 4 * 1024 * 1024;
unsigned int	zfetch_max_distance = 64 * 1024 * 1024;
#endif
unsigned int	zfetch_max_idistance = 64 * 1024 * 1024;
typedef struct zfetch_stats {
	kstat_named_t zfetchstat_hits;
	kstat_named_t zfetchstat_misses;
	kstat_named_t zfetchstat_max_streams;
	kstat_named_t zfetchstat_io_issued;
	kstat_named_t zfetchstat_io_active;
} zfetch_stats_t;
static zfetch_stats_t zfetch_stats = {
	{ "hits",			KSTAT_DATA_UINT64 },
	{ "misses",			KSTAT_DATA_UINT64 },
	{ "max_streams",		KSTAT_DATA_UINT64 },
	{ "io_issued",			KSTAT_DATA_UINT64 },
	{ "io_active",			KSTAT_DATA_UINT64 },
};
struct {
	wmsum_t zfetchstat_hits;
	wmsum_t zfetchstat_misses;
	wmsum_t zfetchstat_max_streams;
	wmsum_t zfetchstat_io_issued;
	aggsum_t zfetchstat_io_active;
} zfetch_sums;
#define	ZFETCHSTAT_BUMP(stat)					\
	wmsum_add(&zfetch_sums.stat, 1)
#define	ZFETCHSTAT_ADD(stat, val)				\
	wmsum_add(&zfetch_sums.stat, val)
static kstat_t		*zfetch_ksp;
static int
zfetch_kstats_update(kstat_t *ksp, int rw)
{
	zfetch_stats_t *zs = ksp->ks_data;
	if (rw == KSTAT_WRITE)
		return (EACCES);
	zs->zfetchstat_hits.value.ui64 =
	    wmsum_value(&zfetch_sums.zfetchstat_hits);
	zs->zfetchstat_misses.value.ui64 =
	    wmsum_value(&zfetch_sums.zfetchstat_misses);
	zs->zfetchstat_max_streams.value.ui64 =
	    wmsum_value(&zfetch_sums.zfetchstat_max_streams);
	zs->zfetchstat_io_issued.value.ui64 =
	    wmsum_value(&zfetch_sums.zfetchstat_io_issued);
	zs->zfetchstat_io_active.value.ui64 =
	    aggsum_value(&zfetch_sums.zfetchstat_io_active);
	return (0);
}
void
zfetch_init(void)
{
	wmsum_init(&zfetch_sums.zfetchstat_hits, 0);
	wmsum_init(&zfetch_sums.zfetchstat_misses, 0);
	wmsum_init(&zfetch_sums.zfetchstat_max_streams, 0);
	wmsum_init(&zfetch_sums.zfetchstat_io_issued, 0);
	aggsum_init(&zfetch_sums.zfetchstat_io_active, 0);
	zfetch_ksp = kstat_create("zfs", 0, "zfetchstats", "misc",
	    KSTAT_TYPE_NAMED, sizeof (zfetch_stats) / sizeof (kstat_named_t),
	    KSTAT_FLAG_VIRTUAL);
	if (zfetch_ksp != NULL) {
		zfetch_ksp->ks_data = &zfetch_stats;
		zfetch_ksp->ks_update = zfetch_kstats_update;
		kstat_install(zfetch_ksp);
	}
}
void
zfetch_fini(void)
{
	if (zfetch_ksp != NULL) {
		kstat_delete(zfetch_ksp);
		zfetch_ksp = NULL;
	}
	wmsum_fini(&zfetch_sums.zfetchstat_hits);
	wmsum_fini(&zfetch_sums.zfetchstat_misses);
	wmsum_fini(&zfetch_sums.zfetchstat_max_streams);
	wmsum_fini(&zfetch_sums.zfetchstat_io_issued);
	ASSERT0(aggsum_value(&zfetch_sums.zfetchstat_io_active));
	aggsum_fini(&zfetch_sums.zfetchstat_io_active);
}
void
dmu_zfetch_init(zfetch_t *zf, dnode_t *dno)
{
	if (zf == NULL)
		return;
	zf->zf_dnode = dno;
	zf->zf_numstreams = 0;
	list_create(&zf->zf_stream, sizeof (zstream_t),
	    offsetof(zstream_t, zs_node));
	mutex_init(&zf->zf_lock, NULL, MUTEX_DEFAULT, NULL);
}
static void
dmu_zfetch_stream_fini(zstream_t *zs)
{
	ASSERT(!list_link_active(&zs->zs_node));
	zfs_refcount_destroy(&zs->zs_callers);
	zfs_refcount_destroy(&zs->zs_refs);
	kmem_free(zs, sizeof (*zs));
}
static void
dmu_zfetch_stream_remove(zfetch_t *zf, zstream_t *zs)
{
	ASSERT(MUTEX_HELD(&zf->zf_lock));
	list_remove(&zf->zf_stream, zs);
	zf->zf_numstreams--;
	membar_producer();
	if (zfs_refcount_remove(&zs->zs_refs, NULL) == 0)
		dmu_zfetch_stream_fini(zs);
}
void
dmu_zfetch_fini(zfetch_t *zf)
{
	zstream_t *zs;
	mutex_enter(&zf->zf_lock);
	while ((zs = list_head(&zf->zf_stream)) != NULL)
		dmu_zfetch_stream_remove(zf, zs);
	mutex_exit(&zf->zf_lock);
	list_destroy(&zf->zf_stream);
	mutex_destroy(&zf->zf_lock);
	zf->zf_dnode = NULL;
}
static void
dmu_zfetch_stream_create(zfetch_t *zf, uint64_t blkid)
{
	zstream_t *zs, *zs_next, *zs_old = NULL;
	hrtime_t now = gethrtime(), t;
	ASSERT(MUTEX_HELD(&zf->zf_lock));
	t = now - SEC2NSEC(zfetch_max_sec_reap);
	for (zs = list_head(&zf->zf_stream); zs != NULL; zs = zs_next) {
		zs_next = list_next(&zf->zf_stream, zs);
		if (zfs_refcount_count(&zs->zs_refs) != 1)
			continue;
		if (zs->zs_atime > t)
			continue;
		if (zs_old)
			dmu_zfetch_stream_remove(zf, zs);
		else
			zs_old = zs;
	}
	if (zs_old) {
		zs = zs_old;
		goto reuse;
	}
	uint32_t max_streams = MAX(1, MIN(zfetch_max_streams,
	    zf->zf_dnode->dn_maxblkid * zf->zf_dnode->dn_datablksz /
	    zfetch_max_distance));
	if (zf->zf_numstreams >= max_streams) {
		t = now - SEC2NSEC(zfetch_min_sec_reap);
		for (zs = list_head(&zf->zf_stream); zs != NULL;
		    zs = list_next(&zf->zf_stream, zs)) {
			if (zfs_refcount_count(&zs->zs_refs) != 1)
				continue;
			if (zs->zs_atime > t)
				continue;
			if (zs_old == NULL || zs->zs_atime < zs_old->zs_atime)
				zs_old = zs;
		}
		if (zs_old) {
			zs = zs_old;
			goto reuse;
		}
		ZFETCHSTAT_BUMP(zfetchstat_max_streams);
		return;
	}
	zs = kmem_zalloc(sizeof (*zs), KM_SLEEP);
	zs->zs_fetch = zf;
	zfs_refcount_create(&zs->zs_callers);
	zfs_refcount_create(&zs->zs_refs);
	zfs_refcount_add(&zs->zs_refs, NULL);
	zf->zf_numstreams++;
	list_insert_head(&zf->zf_stream, zs);
reuse:
	zs->zs_blkid = blkid;
	zs->zs_pf_dist = 0;
	zs->zs_pf_start = blkid;
	zs->zs_pf_end = blkid;
	zs->zs_ipf_dist = 0;
	zs->zs_ipf_start = blkid;
	zs->zs_ipf_end = blkid;
	zs->zs_atime = now - SEC2NSEC(zfetch_min_sec_reap);
	zs->zs_missed = B_FALSE;
	zs->zs_more = B_FALSE;
}
static void
dmu_zfetch_done(void *arg, uint64_t level, uint64_t blkid, boolean_t io_issued)
{
	zstream_t *zs = arg;
	if (io_issued && level == 0 && blkid < zs->zs_blkid)
		zs->zs_more = B_TRUE;
	if (zfs_refcount_remove(&zs->zs_refs, NULL) == 0)
		dmu_zfetch_stream_fini(zs);
	aggsum_add(&zfetch_sums.zfetchstat_io_active, -1);
}
zstream_t *
dmu_zfetch_prepare(zfetch_t *zf, uint64_t blkid, uint64_t nblks,
    boolean_t fetch_data, boolean_t have_lock)
{
	zstream_t *zs;
	spa_t *spa = zf->zf_dnode->dn_objset->os_spa;
	if (zfs_prefetch_disable)
		return (NULL);
	if (!spa_indirect_vdevs_loaded(spa))
		return (NULL);
	if (!have_lock && blkid == 0)
		return (NULL);
	if (!have_lock)
		rw_enter(&zf->zf_dnode->dn_struct_rwlock, RW_READER);
	uint64_t maxblkid = zf->zf_dnode->dn_maxblkid;
	if (maxblkid < 2) {
		if (!have_lock)
			rw_exit(&zf->zf_dnode->dn_struct_rwlock);
		return (NULL);
	}
	mutex_enter(&zf->zf_lock);
	for (zs = list_head(&zf->zf_stream); zs != NULL;
	    zs = list_next(&zf->zf_stream, zs)) {
		if (blkid == zs->zs_blkid) {
			break;
		} else if (blkid + 1 == zs->zs_blkid) {
			blkid++;
			nblks--;
			break;
		}
	}
	uint64_t end_of_access_blkid = blkid + nblks;
	if (end_of_access_blkid >= maxblkid) {
		if (zs != NULL)
			dmu_zfetch_stream_remove(zf, zs);
		mutex_exit(&zf->zf_lock);
		if (!have_lock)
			rw_exit(&zf->zf_dnode->dn_struct_rwlock);
		return (NULL);
	}
	if (nblks == 0) {
		mutex_exit(&zf->zf_lock);
		if (!have_lock)
			rw_exit(&zf->zf_dnode->dn_struct_rwlock);
		return (NULL);
	}
	if (zs == NULL) {
		dmu_zfetch_stream_create(zf, end_of_access_blkid);
		mutex_exit(&zf->zf_lock);
		if (!have_lock)
			rw_exit(&zf->zf_dnode->dn_struct_rwlock);
		ZFETCHSTAT_BUMP(zfetchstat_misses);
		return (NULL);
	}
	unsigned int dbs = zf->zf_dnode->dn_datablkshift;
	unsigned int nbytes = nblks << dbs;
	unsigned int pf_nblks;
	if (fetch_data) {
		if (unlikely(zs->zs_pf_dist < nbytes))
			zs->zs_pf_dist = nbytes;
		else if (zs->zs_pf_dist < zfetch_min_distance &&
		    (zs->zs_pf_dist < (1 << dbs) ||
		    aggsum_compare(&zfetch_sums.zfetchstat_io_active,
		    arc_c_max >> (4 + dbs)) < 0))
			zs->zs_pf_dist *= 2;
		else if (zs->zs_more)
			zs->zs_pf_dist += zs->zs_pf_dist / 8;
		zs->zs_more = B_FALSE;
		if (zs->zs_pf_dist > zfetch_max_distance)
			zs->zs_pf_dist = zfetch_max_distance;
		pf_nblks = zs->zs_pf_dist >> dbs;
	} else {
		pf_nblks = 0;
	}
	if (zs->zs_pf_start < end_of_access_blkid)
		zs->zs_pf_start = end_of_access_blkid;
	if (zs->zs_pf_end < end_of_access_blkid + pf_nblks)
		zs->zs_pf_end = end_of_access_blkid + pf_nblks;
	if (unlikely(zs->zs_ipf_dist < nbytes))
		zs->zs_ipf_dist = nbytes;
	else
		zs->zs_ipf_dist *= 2;
	if (zs->zs_ipf_dist > zfetch_max_idistance)
		zs->zs_ipf_dist = zfetch_max_idistance;
	pf_nblks = zs->zs_ipf_dist >> dbs;
	if (zs->zs_ipf_start < zs->zs_pf_end)
		zs->zs_ipf_start = zs->zs_pf_end;
	if (zs->zs_ipf_end < zs->zs_pf_end + pf_nblks)
		zs->zs_ipf_end = zs->zs_pf_end + pf_nblks;
	zs->zs_blkid = end_of_access_blkid;
	zs->zs_atime = gethrtime();
	zfs_refcount_add(&zs->zs_refs, NULL);
	zfs_refcount_add(&zs->zs_callers, NULL);
	mutex_exit(&zf->zf_lock);
	if (!have_lock)
		rw_exit(&zf->zf_dnode->dn_struct_rwlock);
	ZFETCHSTAT_BUMP(zfetchstat_hits);
	return (zs);
}
void
dmu_zfetch_run(zstream_t *zs, boolean_t missed, boolean_t have_lock)
{
	zfetch_t *zf = zs->zs_fetch;
	int64_t pf_start, pf_end, ipf_start, ipf_end;
	int epbs, issued;
	if (missed)
		zs->zs_missed = missed;
	if (zfs_refcount_remove(&zs->zs_callers, NULL) != 0) {
		if (zfs_refcount_remove(&zs->zs_refs, NULL) == 0)
			dmu_zfetch_stream_fini(zs);
		return;
	}
	mutex_enter(&zf->zf_lock);
	if (zs->zs_missed) {
		pf_start = zs->zs_pf_start;
		pf_end = zs->zs_pf_start = zs->zs_pf_end;
	} else {
		pf_start = pf_end = 0;
	}
	ipf_start = zs->zs_ipf_start;
	ipf_end = zs->zs_ipf_start = zs->zs_ipf_end;
	mutex_exit(&zf->zf_lock);
	ASSERT3S(pf_start, <=, pf_end);
	ASSERT3S(ipf_start, <=, ipf_end);
	epbs = zf->zf_dnode->dn_indblkshift - SPA_BLKPTRSHIFT;
	ipf_start = P2ROUNDUP(ipf_start, 1 << epbs) >> epbs;
	ipf_end = P2ROUNDUP(ipf_end, 1 << epbs) >> epbs;
	ASSERT3S(ipf_start, <=, ipf_end);
	issued = pf_end - pf_start + ipf_end - ipf_start;
	if (issued > 1) {
		zfs_refcount_add_few(&zs->zs_refs, issued - 1, NULL);
	} else if (issued == 0) {
		if (zfs_refcount_remove(&zs->zs_refs, NULL) == 0)
			dmu_zfetch_stream_fini(zs);
		return;
	}
	aggsum_add(&zfetch_sums.zfetchstat_io_active, issued);
	if (!have_lock)
		rw_enter(&zf->zf_dnode->dn_struct_rwlock, RW_READER);
	issued = 0;
	for (int64_t blk = pf_start; blk < pf_end; blk++) {
		issued += dbuf_prefetch_impl(zf->zf_dnode, 0, blk,
		    ZIO_PRIORITY_ASYNC_READ, 0, dmu_zfetch_done, zs);
	}
	for (int64_t iblk = ipf_start; iblk < ipf_end; iblk++) {
		issued += dbuf_prefetch_impl(zf->zf_dnode, 1, iblk,
		    ZIO_PRIORITY_ASYNC_READ, 0, dmu_zfetch_done, zs);
	}
	if (!have_lock)
		rw_exit(&zf->zf_dnode->dn_struct_rwlock);
	if (issued)
		ZFETCHSTAT_ADD(zfetchstat_io_issued, issued);
}
void
dmu_zfetch(zfetch_t *zf, uint64_t blkid, uint64_t nblks, boolean_t fetch_data,
    boolean_t missed, boolean_t have_lock)
{
	zstream_t *zs;
	zs = dmu_zfetch_prepare(zf, blkid, nblks, fetch_data, have_lock);
	if (zs)
		dmu_zfetch_run(zs, missed, have_lock);
}
ZFS_MODULE_PARAM(zfs_prefetch, zfs_prefetch_, disable, INT, ZMOD_RW,
	"Disable all ZFS prefetching");
ZFS_MODULE_PARAM(zfs_prefetch, zfetch_, max_streams, UINT, ZMOD_RW,
	"Max number of streams per zfetch");
ZFS_MODULE_PARAM(zfs_prefetch, zfetch_, min_sec_reap, UINT, ZMOD_RW,
	"Min time before stream reclaim");
ZFS_MODULE_PARAM(zfs_prefetch, zfetch_, max_sec_reap, UINT, ZMOD_RW,
	"Max time before stream delete");
ZFS_MODULE_PARAM(zfs_prefetch, zfetch_, min_distance, UINT, ZMOD_RW,
	"Min bytes to prefetch per stream");
ZFS_MODULE_PARAM(zfs_prefetch, zfetch_, max_distance, UINT, ZMOD_RW,
	"Max bytes to prefetch per stream");
ZFS_MODULE_PARAM(zfs_prefetch, zfetch_, max_idistance, UINT, ZMOD_RW,
	"Max bytes to prefetch indirects for per stream");
