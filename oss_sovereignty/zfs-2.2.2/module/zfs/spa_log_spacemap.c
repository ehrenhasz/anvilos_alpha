 

 

#include <sys/dmu_objset.h>
#include <sys/metaslab.h>
#include <sys/metaslab_impl.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/spa_log_spacemap.h>
#include <sys/vdev_impl.h>
#include <sys/zap.h>

 

 
static const unsigned long zfs_log_sm_blksz = 1ULL << 17;

 
static uint64_t zfs_unflushed_max_mem_ppm = 1000;

 
static uint64_t zfs_unflushed_max_mem_amt = 1ULL << 30;

 
static uint_t zfs_unflushed_log_block_pct = 400;

 
static uint64_t zfs_unflushed_log_block_min = 1000;

 
static uint64_t zfs_unflushed_log_block_max = (1ULL << 17);

 
static uint64_t zfs_unflushed_log_txg_max = 1000;

 
static uint64_t zfs_max_logsm_summary_length = 10;

 
static uint64_t zfs_min_metaslabs_to_flush = 1;

 
static uint64_t zfs_max_log_walking = 5;

 
int zfs_keep_log_spacemaps_at_export = 0;

static uint64_t
spa_estimate_incoming_log_blocks(spa_t *spa)
{
	ASSERT3U(spa_sync_pass(spa), ==, 1);
	uint64_t steps = 0, sum = 0;
	for (spa_log_sm_t *sls = avl_last(&spa->spa_sm_logs_by_txg);
	    sls != NULL && steps < zfs_max_log_walking;
	    sls = AVL_PREV(&spa->spa_sm_logs_by_txg, sls)) {
		if (sls->sls_txg == spa_syncing_txg(spa)) {
			 
			continue;
		}
		sum += sls->sls_nblocks;
		steps++;
	}
	return ((steps > 0) ? DIV_ROUND_UP(sum, steps) : 0);
}

uint64_t
spa_log_sm_blocklimit(spa_t *spa)
{
	return (spa->spa_unflushed_stats.sus_blocklimit);
}

void
spa_log_sm_set_blocklimit(spa_t *spa)
{
	if (!spa_feature_is_active(spa, SPA_FEATURE_LOG_SPACEMAP)) {
		ASSERT0(spa_log_sm_blocklimit(spa));
		return;
	}

	uint64_t msdcount = 0;
	for (log_summary_entry_t *e = list_head(&spa->spa_log_summary);
	    e; e = list_next(&spa->spa_log_summary, e))
		msdcount += e->lse_msdcount;

	uint64_t limit = msdcount * zfs_unflushed_log_block_pct / 100;
	spa->spa_unflushed_stats.sus_blocklimit = MIN(MAX(limit,
	    zfs_unflushed_log_block_min), zfs_unflushed_log_block_max);
}

uint64_t
spa_log_sm_nblocks(spa_t *spa)
{
	return (spa->spa_unflushed_stats.sus_nblocks);
}

 
static void
spa_log_summary_verify_counts(spa_t *spa)
{
	ASSERT(spa_feature_is_active(spa, SPA_FEATURE_LOG_SPACEMAP));

	if ((zfs_flags & ZFS_DEBUG_LOG_SPACEMAP) == 0)
		return;

	uint64_t ms_in_avl = avl_numnodes(&spa->spa_metaslabs_by_flushed);

	uint64_t ms_in_summary = 0, blk_in_summary = 0;
	for (log_summary_entry_t *e = list_head(&spa->spa_log_summary);
	    e; e = list_next(&spa->spa_log_summary, e)) {
		ms_in_summary += e->lse_mscount;
		blk_in_summary += e->lse_blkcount;
	}

	uint64_t ms_in_logs = 0, blk_in_logs = 0;
	for (spa_log_sm_t *sls = avl_first(&spa->spa_sm_logs_by_txg);
	    sls; sls = AVL_NEXT(&spa->spa_sm_logs_by_txg, sls)) {
		ms_in_logs += sls->sls_mscount;
		blk_in_logs += sls->sls_nblocks;
	}

	VERIFY3U(ms_in_logs, ==, ms_in_summary);
	VERIFY3U(ms_in_logs, ==, ms_in_avl);
	VERIFY3U(blk_in_logs, ==, blk_in_summary);
	VERIFY3U(blk_in_logs, ==, spa_log_sm_nblocks(spa));
}

static boolean_t
summary_entry_is_full(spa_t *spa, log_summary_entry_t *e, uint64_t txg)
{
	if (e->lse_end == txg)
		return (0);
	if (e->lse_txgcount >= DIV_ROUND_UP(zfs_unflushed_log_txg_max,
	    zfs_max_logsm_summary_length))
		return (1);
	uint64_t blocks_per_row = MAX(1,
	    DIV_ROUND_UP(spa_log_sm_blocklimit(spa),
	    zfs_max_logsm_summary_length));
	return (blocks_per_row <= e->lse_blkcount);
}

 
void
spa_log_summary_decrement_mscount(spa_t *spa, uint64_t txg, boolean_t dirty)
{
	 
	if (!spa_writeable(spa))
		return;

	log_summary_entry_t *target = NULL;
	for (log_summary_entry_t *e = list_head(&spa->spa_log_summary);
	    e != NULL; e = list_next(&spa->spa_log_summary, e)) {
		if (e->lse_start > txg)
			break;
		target = e;
	}

	if (target == NULL || target->lse_mscount == 0) {
		 
		VERIFY3S(spa_load_state(spa), ==, SPA_LOAD_ERROR);
		return;
	}

	target->lse_mscount--;
	if (dirty)
		target->lse_msdcount--;
}

 
void
spa_log_summary_decrement_blkcount(spa_t *spa, uint64_t blocks_gone)
{
	log_summary_entry_t *e = list_head(&spa->spa_log_summary);
	ASSERT3P(e, !=, NULL);
	if (e->lse_txgcount > 0)
		e->lse_txgcount--;
	for (; e != NULL; e = list_head(&spa->spa_log_summary)) {
		if (e->lse_blkcount > blocks_gone) {
			e->lse_blkcount -= blocks_gone;
			blocks_gone = 0;
			break;
		} else if (e->lse_mscount == 0) {
			 
			blocks_gone -= e->lse_blkcount;
			list_remove(&spa->spa_log_summary, e);
			kmem_free(e, sizeof (log_summary_entry_t));
		} else {
			 
			VERIFY3U(blocks_gone, ==, e->lse_blkcount);

			 
			VERIFY3P(e, ==, list_tail(&spa->spa_log_summary));
			ASSERT3P(e, ==, list_head(&spa->spa_log_summary));

			blocks_gone = e->lse_blkcount = 0;
			break;
		}
	}

	 
	ASSERT0(blocks_gone);
}

void
spa_log_sm_decrement_mscount(spa_t *spa, uint64_t txg)
{
	spa_log_sm_t target = { .sls_txg = txg };
	spa_log_sm_t *sls = avl_find(&spa->spa_sm_logs_by_txg,
	    &target, NULL);

	if (sls == NULL) {
		 
		VERIFY3S(spa_load_state(spa), ==, SPA_LOAD_ERROR);
		return;
	}

	ASSERT(sls->sls_mscount > 0);
	sls->sls_mscount--;
}

void
spa_log_sm_increment_current_mscount(spa_t *spa)
{
	spa_log_sm_t *last_sls = avl_last(&spa->spa_sm_logs_by_txg);
	ASSERT3U(last_sls->sls_txg, ==, spa_syncing_txg(spa));
	last_sls->sls_mscount++;
}

static void
summary_add_data(spa_t *spa, uint64_t txg, uint64_t metaslabs_flushed,
    uint64_t metaslabs_dirty, uint64_t nblocks)
{
	log_summary_entry_t *e = list_tail(&spa->spa_log_summary);

	if (e == NULL || summary_entry_is_full(spa, e, txg)) {
		e = kmem_zalloc(sizeof (log_summary_entry_t), KM_SLEEP);
		e->lse_start = e->lse_end = txg;
		e->lse_txgcount = 1;
		list_insert_tail(&spa->spa_log_summary, e);
	}

	ASSERT3U(e->lse_start, <=, txg);
	if (e->lse_end < txg) {
		e->lse_end = txg;
		e->lse_txgcount++;
	}
	e->lse_mscount += metaslabs_flushed;
	e->lse_msdcount += metaslabs_dirty;
	e->lse_blkcount += nblocks;
}

static void
spa_log_summary_add_incoming_blocks(spa_t *spa, uint64_t nblocks)
{
	summary_add_data(spa, spa_syncing_txg(spa), 0, 0, nblocks);
}

void
spa_log_summary_add_flushed_metaslab(spa_t *spa, boolean_t dirty)
{
	summary_add_data(spa, spa_syncing_txg(spa), 1, dirty ? 1 : 0, 0);
}

void
spa_log_summary_dirty_flushed_metaslab(spa_t *spa, uint64_t txg)
{
	log_summary_entry_t *target = NULL;
	for (log_summary_entry_t *e = list_head(&spa->spa_log_summary);
	    e != NULL; e = list_next(&spa->spa_log_summary, e)) {
		if (e->lse_start > txg)
			break;
		target = e;
	}
	ASSERT3P(target, !=, NULL);
	ASSERT3U(target->lse_mscount, !=, 0);
	target->lse_msdcount++;
}

 
static uint64_t
spa_estimate_metaslabs_to_flush(spa_t *spa)
{
	ASSERT(spa_feature_is_active(spa, SPA_FEATURE_LOG_SPACEMAP));
	ASSERT3U(spa_sync_pass(spa), ==, 1);
	ASSERT(spa_log_sm_blocklimit(spa) != 0);

	 
	uint64_t incoming = spa_estimate_incoming_log_blocks(spa);

	 
	uint64_t txgs_in_future = 1;

	 
	int64_t available_blocks =
	    spa_log_sm_blocklimit(spa) - spa_log_sm_nblocks(spa) - incoming;

	int64_t available_txgs = zfs_unflushed_log_txg_max;
	for (log_summary_entry_t *e = list_head(&spa->spa_log_summary);
	    e; e = list_next(&spa->spa_log_summary, e))
		available_txgs -= e->lse_txgcount;

	 
	uint64_t total_flushes = 0;

	 
	uint64_t max_flushes_pertxg = zfs_min_metaslabs_to_flush;

	 
	for (log_summary_entry_t *e = list_head(&spa->spa_log_summary);
	    e; e = list_next(&spa->spa_log_summary, e)) {

		 
		if (available_blocks >= 0 && available_txgs >= 0) {
			uint64_t skip_txgs = (incoming == 0) ?
			    available_txgs + 1 : MIN(available_txgs + 1,
			    (available_blocks / incoming) + 1);
			available_blocks -= (skip_txgs * incoming);
			available_txgs -= skip_txgs;
			txgs_in_future += skip_txgs;
			ASSERT3S(available_blocks, >=, -incoming);
			ASSERT3S(available_txgs, >=, -1);
		}

		 
		ASSERT(available_blocks < 0 || available_txgs < 0);
		available_blocks += e->lse_blkcount;
		available_txgs += e->lse_txgcount;
		total_flushes += e->lse_msdcount;

		 
		max_flushes_pertxg = MAX(max_flushes_pertxg,
		    DIV_ROUND_UP(total_flushes, txgs_in_future));
	}
	return (max_flushes_pertxg);
}

uint64_t
spa_log_sm_memused(spa_t *spa)
{
	return (spa->spa_unflushed_stats.sus_memused);
}

static boolean_t
spa_log_exceeds_memlimit(spa_t *spa)
{
	if (spa_log_sm_memused(spa) > zfs_unflushed_max_mem_amt)
		return (B_TRUE);

	uint64_t system_mem_allowed = ((physmem * PAGESIZE) *
	    zfs_unflushed_max_mem_ppm) / 1000000;
	if (spa_log_sm_memused(spa) > system_mem_allowed)
		return (B_TRUE);

	return (B_FALSE);
}

boolean_t
spa_flush_all_logs_requested(spa_t *spa)
{
	return (spa->spa_log_flushall_txg != 0);
}

void
spa_flush_metaslabs(spa_t *spa, dmu_tx_t *tx)
{
	uint64_t txg = dmu_tx_get_txg(tx);

	if (spa_sync_pass(spa) != 1)
		return;

	if (!spa_feature_is_active(spa, SPA_FEATURE_LOG_SPACEMAP))
		return;

	 
	if (avl_numnodes(&spa->spa_metaslabs_by_flushed) == 0)
		return;

	 
	if (spa->spa_uberblock.ub_rootbp.blk_birth < txg &&
	    !dmu_objset_is_dirty(spa_meta_objset(spa), txg) &&
	    !spa_flush_all_logs_requested(spa))
		return;

	 
	spa_generate_syncing_log_sm(spa, tx);

	 
	uint64_t want_to_flush;
	if (spa_flush_all_logs_requested(spa)) {
		ASSERT3S(spa_state(spa), ==, POOL_STATE_EXPORTED);
		want_to_flush = UINT64_MAX;
	} else {
		want_to_flush = spa_estimate_metaslabs_to_flush(spa);
	}

	 
	uint64_t visited = 0;

	 
	metaslab_t *next = NULL;
	for (metaslab_t *curr = avl_first(&spa->spa_metaslabs_by_flushed);
	    curr != NULL; curr = next) {
		next = AVL_NEXT(&spa->spa_metaslabs_by_flushed, curr);

		 
		if (metaslab_unflushed_txg(curr) == txg)
			break;

		 
		if (want_to_flush == 0 && !spa_log_exceeds_memlimit(spa))
			break;

		if (metaslab_unflushed_dirty(curr)) {
			mutex_enter(&curr->ms_sync_lock);
			mutex_enter(&curr->ms_lock);
			metaslab_flush(curr, tx);
			mutex_exit(&curr->ms_lock);
			mutex_exit(&curr->ms_sync_lock);
			if (want_to_flush > 0)
				want_to_flush--;
		} else
			metaslab_unflushed_bump(curr, tx, B_FALSE);

		visited++;
	}
	ASSERT3U(avl_numnodes(&spa->spa_metaslabs_by_flushed), >=, visited);

	spa_log_sm_set_blocklimit(spa);
}

 
void
spa_sync_close_syncing_log_sm(spa_t *spa)
{
	if (spa_syncing_log_sm(spa) == NULL)
		return;
	ASSERT(spa_feature_is_active(spa, SPA_FEATURE_LOG_SPACEMAP));

	spa_log_sm_t *sls = avl_last(&spa->spa_sm_logs_by_txg);
	ASSERT3U(sls->sls_txg, ==, spa_syncing_txg(spa));

	sls->sls_nblocks = space_map_nblocks(spa_syncing_log_sm(spa));
	spa->spa_unflushed_stats.sus_nblocks += sls->sls_nblocks;

	 
	ASSERT(sls->sls_nblocks != 0);

	spa_log_summary_add_incoming_blocks(spa, sls->sls_nblocks);
	spa_log_summary_verify_counts(spa);

	space_map_close(spa->spa_syncing_log_sm);
	spa->spa_syncing_log_sm = NULL;

	 
	if (spa_flush_all_logs_requested(spa)) {
		ASSERT3S(spa_state(spa), ==, POOL_STATE_EXPORTED);
		spa->spa_log_flushall_txg = 0;
	}
}

void
spa_cleanup_old_sm_logs(spa_t *spa, dmu_tx_t *tx)
{
	objset_t *mos = spa_meta_objset(spa);

	uint64_t spacemap_zap;
	int error = zap_lookup(mos, DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_LOG_SPACEMAP_ZAP, sizeof (spacemap_zap), 1, &spacemap_zap);
	if (error == ENOENT) {
		ASSERT(avl_is_empty(&spa->spa_sm_logs_by_txg));
		return;
	}
	VERIFY0(error);

	metaslab_t *oldest = avl_first(&spa->spa_metaslabs_by_flushed);
	uint64_t oldest_flushed_txg = metaslab_unflushed_txg(oldest);

	 
	for (spa_log_sm_t *sls = avl_first(&spa->spa_sm_logs_by_txg);
	    sls && sls->sls_txg < oldest_flushed_txg;
	    sls = avl_first(&spa->spa_sm_logs_by_txg)) {
		ASSERT0(sls->sls_mscount);
		avl_remove(&spa->spa_sm_logs_by_txg, sls);
		space_map_free_obj(mos, sls->sls_sm_obj, tx);
		VERIFY0(zap_remove_int(mos, spacemap_zap, sls->sls_txg, tx));
		spa_log_summary_decrement_blkcount(spa, sls->sls_nblocks);
		spa->spa_unflushed_stats.sus_nblocks -= sls->sls_nblocks;
		kmem_free(sls, sizeof (spa_log_sm_t));
	}
}

static spa_log_sm_t *
spa_log_sm_alloc(uint64_t sm_obj, uint64_t txg)
{
	spa_log_sm_t *sls = kmem_zalloc(sizeof (*sls), KM_SLEEP);
	sls->sls_sm_obj = sm_obj;
	sls->sls_txg = txg;
	return (sls);
}

void
spa_generate_syncing_log_sm(spa_t *spa, dmu_tx_t *tx)
{
	uint64_t txg = dmu_tx_get_txg(tx);
	objset_t *mos = spa_meta_objset(spa);

	if (spa_syncing_log_sm(spa) != NULL)
		return;

	if (!spa_feature_is_enabled(spa, SPA_FEATURE_LOG_SPACEMAP))
		return;

	uint64_t spacemap_zap;
	int error = zap_lookup(mos, DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_LOG_SPACEMAP_ZAP, sizeof (spacemap_zap), 1, &spacemap_zap);
	if (error == ENOENT) {
		ASSERT(avl_is_empty(&spa->spa_sm_logs_by_txg));

		error = 0;
		spacemap_zap = zap_create(mos,
		    DMU_OTN_ZAP_METADATA, DMU_OT_NONE, 0, tx);
		VERIFY0(zap_add(mos, DMU_POOL_DIRECTORY_OBJECT,
		    DMU_POOL_LOG_SPACEMAP_ZAP, sizeof (spacemap_zap), 1,
		    &spacemap_zap, tx));
		spa_feature_incr(spa, SPA_FEATURE_LOG_SPACEMAP, tx);
	}
	VERIFY0(error);

	uint64_t sm_obj;
	ASSERT3U(zap_lookup_int_key(mos, spacemap_zap, txg, &sm_obj),
	    ==, ENOENT);
	sm_obj = space_map_alloc(mos, zfs_log_sm_blksz, tx);
	VERIFY0(zap_add_int_key(mos, spacemap_zap, txg, sm_obj, tx));
	avl_add(&spa->spa_sm_logs_by_txg, spa_log_sm_alloc(sm_obj, txg));

	 
	VERIFY0(space_map_open(&spa->spa_syncing_log_sm, mos, sm_obj,
	    0, UINT64_MAX, SPA_MINBLOCKSHIFT));

	spa_log_sm_set_blocklimit(spa);
}

 
static int
spa_ld_log_sm_metadata(spa_t *spa)
{
	int error;
	uint64_t spacemap_zap;

	ASSERT(avl_is_empty(&spa->spa_sm_logs_by_txg));

	error = zap_lookup(spa_meta_objset(spa), DMU_POOL_DIRECTORY_OBJECT,
	    DMU_POOL_LOG_SPACEMAP_ZAP, sizeof (spacemap_zap), 1, &spacemap_zap);
	if (error == ENOENT) {
		 
		return (0);
	} else if (error != 0) {
		spa_load_failed(spa, "spa_ld_log_sm_metadata(): failed at "
		    "zap_lookup(DMU_POOL_DIRECTORY_OBJECT) [error %d]",
		    error);
		return (error);
	}

	zap_cursor_t zc;
	zap_attribute_t za;
	for (zap_cursor_init(&zc, spa_meta_objset(spa), spacemap_zap);
	    (error = zap_cursor_retrieve(&zc, &za)) == 0;
	    zap_cursor_advance(&zc)) {
		uint64_t log_txg = zfs_strtonum(za.za_name, NULL);
		spa_log_sm_t *sls =
		    spa_log_sm_alloc(za.za_first_integer, log_txg);
		avl_add(&spa->spa_sm_logs_by_txg, sls);
	}
	zap_cursor_fini(&zc);
	if (error != ENOENT) {
		spa_load_failed(spa, "spa_ld_log_sm_metadata(): failed at "
		    "zap_cursor_retrieve(spacemap_zap) [error %d]",
		    error);
		return (error);
	}

	for (metaslab_t *m = avl_first(&spa->spa_metaslabs_by_flushed);
	    m; m = AVL_NEXT(&spa->spa_metaslabs_by_flushed, m)) {
		spa_log_sm_t target = { .sls_txg = metaslab_unflushed_txg(m) };
		spa_log_sm_t *sls = avl_find(&spa->spa_sm_logs_by_txg,
		    &target, NULL);

		 
		ASSERT(sls != NULL);
		if (sls == NULL) {
			spa_load_failed(spa, "spa_ld_log_sm_metadata(): bug "
			    "encountered: could not find log spacemap for "
			    "TXG %llu [error %d]",
			    (u_longlong_t)metaslab_unflushed_txg(m), ENOENT);
			return (ENOENT);
		}
		sls->sls_mscount++;
	}

	return (0);
}

typedef struct spa_ld_log_sm_arg {
	spa_t *slls_spa;
	uint64_t slls_txg;
} spa_ld_log_sm_arg_t;

static int
spa_ld_log_sm_cb(space_map_entry_t *sme, void *arg)
{
	uint64_t offset = sme->sme_offset;
	uint64_t size = sme->sme_run;
	uint32_t vdev_id = sme->sme_vdev;

	spa_ld_log_sm_arg_t *slls = arg;
	spa_t *spa = slls->slls_spa;

	vdev_t *vd = vdev_lookup_top(spa, vdev_id);

	 
	if (!vdev_is_concrete(vd))
		return (0);

	metaslab_t *ms = vd->vdev_ms[offset >> vd->vdev_ms_shift];
	ASSERT(!ms->ms_loaded);

	 
	if (slls->slls_txg < metaslab_unflushed_txg(ms))
		return (0);

	switch (sme->sme_type) {
	case SM_ALLOC:
		range_tree_remove_xor_add_segment(offset, offset + size,
		    ms->ms_unflushed_frees, ms->ms_unflushed_allocs);
		break;
	case SM_FREE:
		range_tree_remove_xor_add_segment(offset, offset + size,
		    ms->ms_unflushed_allocs, ms->ms_unflushed_frees);
		break;
	default:
		panic("invalid maptype_t");
		break;
	}
	if (!metaslab_unflushed_dirty(ms)) {
		metaslab_set_unflushed_dirty(ms, B_TRUE);
		spa_log_summary_dirty_flushed_metaslab(spa,
		    metaslab_unflushed_txg(ms));
	}
	return (0);
}

static int
spa_ld_log_sm_data(spa_t *spa)
{
	spa_log_sm_t *sls, *psls;
	int error = 0;

	 
	if (!spa_writeable(spa))
		return (0);

	ASSERT0(spa->spa_unflushed_stats.sus_nblocks);
	ASSERT0(spa->spa_unflushed_stats.sus_memused);

	hrtime_t read_logs_starttime = gethrtime();

	 
	for (sls = avl_first(&spa->spa_sm_logs_by_txg); sls;
	    sls = AVL_NEXT(&spa->spa_sm_logs_by_txg, sls)) {
		dmu_prefetch(spa_meta_objset(spa), sls->sls_sm_obj,
		    0, 0, 0, ZIO_PRIORITY_SYNC_READ);
	}

	uint_t pn = 0;
	uint64_t ps = 0;
	psls = sls = avl_first(&spa->spa_sm_logs_by_txg);
	while (sls != NULL) {
		 
		if (psls != NULL && pn < 16 &&
		    (pn < 2 || ps < 2 * dmu_prefetch_max)) {
			error = space_map_open(&psls->sls_sm,
			    spa_meta_objset(spa), psls->sls_sm_obj, 0,
			    UINT64_MAX, SPA_MINBLOCKSHIFT);
			if (error != 0) {
				spa_load_failed(spa, "spa_ld_log_sm_data(): "
				    "failed at space_map_open(obj=%llu) "
				    "[error %d]",
				    (u_longlong_t)sls->sls_sm_obj, error);
				goto out;
			}
			dmu_prefetch(spa_meta_objset(spa), psls->sls_sm_obj,
			    0, 0, space_map_length(psls->sls_sm),
			    ZIO_PRIORITY_ASYNC_READ);
			pn++;
			ps += space_map_length(psls->sls_sm);
			psls = AVL_NEXT(&spa->spa_sm_logs_by_txg, psls);
			continue;
		}

		 
		kpreempt(KPREEMPT_SYNC);
		ASSERT0(sls->sls_nblocks);
		sls->sls_nblocks = space_map_nblocks(sls->sls_sm);
		spa->spa_unflushed_stats.sus_nblocks += sls->sls_nblocks;
		summary_add_data(spa, sls->sls_txg,
		    sls->sls_mscount, 0, sls->sls_nblocks);

		struct spa_ld_log_sm_arg vla = {
			.slls_spa = spa,
			.slls_txg = sls->sls_txg
		};
		error = space_map_iterate(sls->sls_sm,
		    space_map_length(sls->sls_sm), spa_ld_log_sm_cb, &vla);
		if (error != 0) {
			spa_load_failed(spa, "spa_ld_log_sm_data(): failed "
			    "at space_map_iterate(obj=%llu) [error %d]",
			    (u_longlong_t)sls->sls_sm_obj, error);
			goto out;
		}

		pn--;
		ps -= space_map_length(sls->sls_sm);
		space_map_close(sls->sls_sm);
		sls->sls_sm = NULL;
		sls = AVL_NEXT(&spa->spa_sm_logs_by_txg, sls);

		 
		spa_log_sm_set_blocklimit(spa);
	}

	hrtime_t read_logs_endtime = gethrtime();
	spa_load_note(spa,
	    "read %llu log space maps (%llu total blocks - blksz = %llu bytes) "
	    "in %lld ms", (u_longlong_t)avl_numnodes(&spa->spa_sm_logs_by_txg),
	    (u_longlong_t)spa_log_sm_nblocks(spa),
	    (u_longlong_t)zfs_log_sm_blksz,
	    (longlong_t)((read_logs_endtime - read_logs_starttime) / 1000000));

out:
	if (error != 0) {
		for (spa_log_sm_t *sls = avl_first(&spa->spa_sm_logs_by_txg);
		    sls; sls = AVL_NEXT(&spa->spa_sm_logs_by_txg, sls)) {
			if (sls->sls_sm) {
				space_map_close(sls->sls_sm);
				sls->sls_sm = NULL;
			}
		}
	} else {
		ASSERT0(pn);
		ASSERT0(ps);
	}
	 
	for (metaslab_t *m = avl_first(&spa->spa_metaslabs_by_flushed);
	    m != NULL; m = AVL_NEXT(&spa->spa_metaslabs_by_flushed, m)) {
		mutex_enter(&m->ms_lock);
		m->ms_allocated_space = space_map_allocated(m->ms_sm) +
		    range_tree_space(m->ms_unflushed_allocs) -
		    range_tree_space(m->ms_unflushed_frees);

		vdev_t *vd = m->ms_group->mg_vd;
		metaslab_space_update(vd, m->ms_group->mg_class,
		    range_tree_space(m->ms_unflushed_allocs), 0, 0);
		metaslab_space_update(vd, m->ms_group->mg_class,
		    -range_tree_space(m->ms_unflushed_frees), 0, 0);

		ASSERT0(m->ms_weight & METASLAB_ACTIVE_MASK);
		metaslab_recalculate_weight_and_sort(m);

		spa->spa_unflushed_stats.sus_memused +=
		    metaslab_unflushed_changes_memused(m);

		if (metaslab_debug_load && m->ms_sm != NULL) {
			VERIFY0(metaslab_load(m));
			metaslab_set_selected_txg(m, 0);
		}
		mutex_exit(&m->ms_lock);
	}

	return (error);
}

static int
spa_ld_unflushed_txgs(vdev_t *vd)
{
	spa_t *spa = vd->vdev_spa;
	objset_t *mos = spa_meta_objset(spa);

	if (vd->vdev_top_zap == 0)
		return (0);

	uint64_t object = 0;
	int error = zap_lookup(mos, vd->vdev_top_zap,
	    VDEV_TOP_ZAP_MS_UNFLUSHED_PHYS_TXGS,
	    sizeof (uint64_t), 1, &object);
	if (error == ENOENT)
		return (0);
	else if (error != 0) {
		spa_load_failed(spa, "spa_ld_unflushed_txgs(): failed at "
		    "zap_lookup(vdev_top_zap=%llu) [error %d]",
		    (u_longlong_t)vd->vdev_top_zap, error);
		return (error);
	}

	for (uint64_t m = 0; m < vd->vdev_ms_count; m++) {
		metaslab_t *ms = vd->vdev_ms[m];
		ASSERT(ms != NULL);

		metaslab_unflushed_phys_t entry;
		uint64_t entry_size = sizeof (entry);
		uint64_t entry_offset = ms->ms_id * entry_size;

		error = dmu_read(mos, object,
		    entry_offset, entry_size, &entry, 0);
		if (error != 0) {
			spa_load_failed(spa, "spa_ld_unflushed_txgs(): "
			    "failed at dmu_read(obj=%llu) [error %d]",
			    (u_longlong_t)object, error);
			return (error);
		}

		ms->ms_unflushed_txg = entry.msp_unflushed_txg;
		ms->ms_unflushed_dirty = B_FALSE;
		ASSERT(range_tree_is_empty(ms->ms_unflushed_allocs));
		ASSERT(range_tree_is_empty(ms->ms_unflushed_frees));
		if (ms->ms_unflushed_txg != 0) {
			mutex_enter(&spa->spa_flushed_ms_lock);
			avl_add(&spa->spa_metaslabs_by_flushed, ms);
			mutex_exit(&spa->spa_flushed_ms_lock);
		}
	}
	return (0);
}

 
int
spa_ld_log_spacemaps(spa_t *spa)
{
	int error;

	spa_log_sm_set_blocklimit(spa);

	for (uint64_t c = 0; c < spa->spa_root_vdev->vdev_children; c++) {
		vdev_t *vd = spa->spa_root_vdev->vdev_child[c];
		error = spa_ld_unflushed_txgs(vd);
		if (error != 0)
			return (error);
	}

	error = spa_ld_log_sm_metadata(spa);
	if (error != 0)
		return (error);

	 
	spa_config_enter(spa, SCL_CONFIG, FTAG, RW_READER);
	error = spa_ld_log_sm_data(spa);
	spa_config_exit(spa, SCL_CONFIG, FTAG);

	return (error);
}

 
ZFS_MODULE_PARAM(zfs, zfs_, unflushed_max_mem_amt, U64, ZMOD_RW,
	"Specific hard-limit in memory that ZFS allows to be used for "
	"unflushed changes");

ZFS_MODULE_PARAM(zfs, zfs_, unflushed_max_mem_ppm, U64, ZMOD_RW,
	"Percentage of the overall system memory that ZFS allows to be "
	"used for unflushed changes (value is calculated over 1000000 for "
	"finer granularity)");

ZFS_MODULE_PARAM(zfs, zfs_, unflushed_log_block_max, U64, ZMOD_RW,
	"Hard limit (upper-bound) in the size of the space map log "
	"in terms of blocks.");

ZFS_MODULE_PARAM(zfs, zfs_, unflushed_log_block_min, U64, ZMOD_RW,
	"Lower-bound limit for the maximum amount of blocks allowed in "
	"log spacemap (see zfs_unflushed_log_block_max)");

ZFS_MODULE_PARAM(zfs, zfs_, unflushed_log_txg_max, U64, ZMOD_RW,
    "Hard limit (upper-bound) in the size of the space map log "
    "in terms of dirty TXGs.");

ZFS_MODULE_PARAM(zfs, zfs_, unflushed_log_block_pct, UINT, ZMOD_RW,
	"Tunable used to determine the number of blocks that can be used for "
	"the spacemap log, expressed as a percentage of the total number of "
	"metaslabs in the pool (e.g. 400 means the number of log blocks is "
	"capped at 4 times the number of metaslabs)");

ZFS_MODULE_PARAM(zfs, zfs_, max_log_walking, U64, ZMOD_RW,
	"The number of past TXGs that the flushing algorithm of the log "
	"spacemap feature uses to estimate incoming log blocks");

ZFS_MODULE_PARAM(zfs, zfs_, keep_log_spacemaps_at_export, INT, ZMOD_RW,
	"Prevent the log spacemaps from being flushed and destroyed "
	"during pool export/destroy");
 

ZFS_MODULE_PARAM(zfs, zfs_, max_logsm_summary_length, U64, ZMOD_RW,
	"Maximum number of rows allowed in the summary of the spacemap log");

ZFS_MODULE_PARAM(zfs, zfs_, min_metaslabs_to_flush, U64, ZMOD_RW,
	"Minimum number of metaslabs to flush per dirty TXG");
