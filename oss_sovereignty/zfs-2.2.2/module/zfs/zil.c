 
 

 

#include <sys/zfs_context.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/dmu.h>
#include <sys/zap.h>
#include <sys/arc.h>
#include <sys/stat.h>
#include <sys/zil.h>
#include <sys/zil_impl.h>
#include <sys/dsl_dataset.h>
#include <sys/vdev_impl.h>
#include <sys/dmu_tx.h>
#include <sys/dsl_pool.h>
#include <sys/metaslab.h>
#include <sys/trace_zfs.h>
#include <sys/abd.h>
#include <sys/brt.h>
#include <sys/wmsum.h>

 

 
static uint_t zfs_commit_timeout_pct = 5;

 
static uint64_t zil_min_commit_timeout = 5000;

 
static zil_kstat_values_t zil_stats = {
	{ "zil_commit_count",			KSTAT_DATA_UINT64 },
	{ "zil_commit_writer_count",		KSTAT_DATA_UINT64 },
	{ "zil_itx_count",			KSTAT_DATA_UINT64 },
	{ "zil_itx_indirect_count",		KSTAT_DATA_UINT64 },
	{ "zil_itx_indirect_bytes",		KSTAT_DATA_UINT64 },
	{ "zil_itx_copied_count",		KSTAT_DATA_UINT64 },
	{ "zil_itx_copied_bytes",		KSTAT_DATA_UINT64 },
	{ "zil_itx_needcopy_count",		KSTAT_DATA_UINT64 },
	{ "zil_itx_needcopy_bytes",		KSTAT_DATA_UINT64 },
	{ "zil_itx_metaslab_normal_count",	KSTAT_DATA_UINT64 },
	{ "zil_itx_metaslab_normal_bytes",	KSTAT_DATA_UINT64 },
	{ "zil_itx_metaslab_normal_write",	KSTAT_DATA_UINT64 },
	{ "zil_itx_metaslab_normal_alloc",	KSTAT_DATA_UINT64 },
	{ "zil_itx_metaslab_slog_count",	KSTAT_DATA_UINT64 },
	{ "zil_itx_metaslab_slog_bytes",	KSTAT_DATA_UINT64 },
	{ "zil_itx_metaslab_slog_write",	KSTAT_DATA_UINT64 },
	{ "zil_itx_metaslab_slog_alloc",	KSTAT_DATA_UINT64 },
};

static zil_sums_t zil_sums_global;
static kstat_t *zil_kstats_global;

 
int zil_replay_disable = 0;

 
static int zil_nocacheflush = 0;

 
static uint64_t zil_slog_bulk = 64 * 1024 * 1024;

static kmem_cache_t *zil_lwb_cache;
static kmem_cache_t *zil_zcw_cache;

static void zil_lwb_commit(zilog_t *zilog, lwb_t *lwb, itx_t *itx);
static itx_t *zil_itx_clone(itx_t *oitx);

static int
zil_bp_compare(const void *x1, const void *x2)
{
	const dva_t *dva1 = &((zil_bp_node_t *)x1)->zn_dva;
	const dva_t *dva2 = &((zil_bp_node_t *)x2)->zn_dva;

	int cmp = TREE_CMP(DVA_GET_VDEV(dva1), DVA_GET_VDEV(dva2));
	if (likely(cmp))
		return (cmp);

	return (TREE_CMP(DVA_GET_OFFSET(dva1), DVA_GET_OFFSET(dva2)));
}

static void
zil_bp_tree_init(zilog_t *zilog)
{
	avl_create(&zilog->zl_bp_tree, zil_bp_compare,
	    sizeof (zil_bp_node_t), offsetof(zil_bp_node_t, zn_node));
}

static void
zil_bp_tree_fini(zilog_t *zilog)
{
	avl_tree_t *t = &zilog->zl_bp_tree;
	zil_bp_node_t *zn;
	void *cookie = NULL;

	while ((zn = avl_destroy_nodes(t, &cookie)) != NULL)
		kmem_free(zn, sizeof (zil_bp_node_t));

	avl_destroy(t);
}

int
zil_bp_tree_add(zilog_t *zilog, const blkptr_t *bp)
{
	avl_tree_t *t = &zilog->zl_bp_tree;
	const dva_t *dva;
	zil_bp_node_t *zn;
	avl_index_t where;

	if (BP_IS_EMBEDDED(bp))
		return (0);

	dva = BP_IDENTITY(bp);

	if (avl_find(t, dva, &where) != NULL)
		return (SET_ERROR(EEXIST));

	zn = kmem_alloc(sizeof (zil_bp_node_t), KM_SLEEP);
	zn->zn_dva = *dva;
	avl_insert(t, zn, where);

	return (0);
}

static zil_header_t *
zil_header_in_syncing_context(zilog_t *zilog)
{
	return ((zil_header_t *)zilog->zl_header);
}

static void
zil_init_log_chain(zilog_t *zilog, blkptr_t *bp)
{
	zio_cksum_t *zc = &bp->blk_cksum;

	(void) random_get_pseudo_bytes((void *)&zc->zc_word[ZIL_ZC_GUID_0],
	    sizeof (zc->zc_word[ZIL_ZC_GUID_0]));
	(void) random_get_pseudo_bytes((void *)&zc->zc_word[ZIL_ZC_GUID_1],
	    sizeof (zc->zc_word[ZIL_ZC_GUID_1]));
	zc->zc_word[ZIL_ZC_OBJSET] = dmu_objset_id(zilog->zl_os);
	zc->zc_word[ZIL_ZC_SEQ] = 1ULL;
}

static int
zil_kstats_global_update(kstat_t *ksp, int rw)
{
	zil_kstat_values_t *zs = ksp->ks_data;
	ASSERT3P(&zil_stats, ==, zs);

	if (rw == KSTAT_WRITE) {
		return (SET_ERROR(EACCES));
	}

	zil_kstat_values_update(zs, &zil_sums_global);

	return (0);
}

 
static int
zil_read_log_block(zilog_t *zilog, boolean_t decrypt, const blkptr_t *bp,
    blkptr_t *nbp, char **begin, char **end, arc_buf_t **abuf)
{
	zio_flag_t zio_flags = ZIO_FLAG_CANFAIL;
	arc_flags_t aflags = ARC_FLAG_WAIT;
	zbookmark_phys_t zb;
	int error;

	if (zilog->zl_header->zh_claim_txg == 0)
		zio_flags |= ZIO_FLAG_SPECULATIVE | ZIO_FLAG_SCRUB;

	if (!(zilog->zl_header->zh_flags & ZIL_CLAIM_LR_SEQ_VALID))
		zio_flags |= ZIO_FLAG_SPECULATIVE;

	if (!decrypt)
		zio_flags |= ZIO_FLAG_RAW;

	SET_BOOKMARK(&zb, bp->blk_cksum.zc_word[ZIL_ZC_OBJSET],
	    ZB_ZIL_OBJECT, ZB_ZIL_LEVEL, bp->blk_cksum.zc_word[ZIL_ZC_SEQ]);

	error = arc_read(NULL, zilog->zl_spa, bp, arc_getbuf_func,
	    abuf, ZIO_PRIORITY_SYNC_READ, zio_flags, &aflags, &zb);

	if (error == 0) {
		zio_cksum_t cksum = bp->blk_cksum;

		 
		cksum.zc_word[ZIL_ZC_SEQ]++;

		uint64_t size = BP_GET_LSIZE(bp);
		if (BP_GET_CHECKSUM(bp) == ZIO_CHECKSUM_ZILOG2) {
			zil_chain_t *zilc = (*abuf)->b_data;
			char *lr = (char *)(zilc + 1);

			if (memcmp(&cksum, &zilc->zc_next_blk.blk_cksum,
			    sizeof (cksum)) ||
			    zilc->zc_nused < sizeof (*zilc) ||
			    zilc->zc_nused > size) {
				error = SET_ERROR(ECKSUM);
			} else {
				*begin = lr;
				*end = lr + zilc->zc_nused - sizeof (*zilc);
				*nbp = zilc->zc_next_blk;
			}
		} else {
			char *lr = (*abuf)->b_data;
			zil_chain_t *zilc = (zil_chain_t *)(lr + size) - 1;

			if (memcmp(&cksum, &zilc->zc_next_blk.blk_cksum,
			    sizeof (cksum)) ||
			    (zilc->zc_nused > (size - sizeof (*zilc)))) {
				error = SET_ERROR(ECKSUM);
			} else {
				*begin = lr;
				*end = lr + zilc->zc_nused;
				*nbp = zilc->zc_next_blk;
			}
		}
	}

	return (error);
}

 
static int
zil_read_log_data(zilog_t *zilog, const lr_write_t *lr, void *wbuf)
{
	zio_flag_t zio_flags = ZIO_FLAG_CANFAIL;
	const blkptr_t *bp = &lr->lr_blkptr;
	arc_flags_t aflags = ARC_FLAG_WAIT;
	arc_buf_t *abuf = NULL;
	zbookmark_phys_t zb;
	int error;

	if (BP_IS_HOLE(bp)) {
		if (wbuf != NULL)
			memset(wbuf, 0, MAX(BP_GET_LSIZE(bp), lr->lr_length));
		return (0);
	}

	if (zilog->zl_header->zh_claim_txg == 0)
		zio_flags |= ZIO_FLAG_SPECULATIVE | ZIO_FLAG_SCRUB;

	 
	if (wbuf == NULL)
		zio_flags |= ZIO_FLAG_RAW;

	ASSERT3U(BP_GET_LSIZE(bp), !=, 0);
	SET_BOOKMARK(&zb, dmu_objset_id(zilog->zl_os), lr->lr_foid,
	    ZB_ZIL_LEVEL, lr->lr_offset / BP_GET_LSIZE(bp));

	error = arc_read(NULL, zilog->zl_spa, bp, arc_getbuf_func, &abuf,
	    ZIO_PRIORITY_SYNC_READ, zio_flags, &aflags, &zb);

	if (error == 0) {
		if (wbuf != NULL)
			memcpy(wbuf, abuf->b_data, arc_buf_size(abuf));
		arc_buf_destroy(abuf, &abuf);
	}

	return (error);
}

void
zil_sums_init(zil_sums_t *zs)
{
	wmsum_init(&zs->zil_commit_count, 0);
	wmsum_init(&zs->zil_commit_writer_count, 0);
	wmsum_init(&zs->zil_itx_count, 0);
	wmsum_init(&zs->zil_itx_indirect_count, 0);
	wmsum_init(&zs->zil_itx_indirect_bytes, 0);
	wmsum_init(&zs->zil_itx_copied_count, 0);
	wmsum_init(&zs->zil_itx_copied_bytes, 0);
	wmsum_init(&zs->zil_itx_needcopy_count, 0);
	wmsum_init(&zs->zil_itx_needcopy_bytes, 0);
	wmsum_init(&zs->zil_itx_metaslab_normal_count, 0);
	wmsum_init(&zs->zil_itx_metaslab_normal_bytes, 0);
	wmsum_init(&zs->zil_itx_metaslab_normal_write, 0);
	wmsum_init(&zs->zil_itx_metaslab_normal_alloc, 0);
	wmsum_init(&zs->zil_itx_metaslab_slog_count, 0);
	wmsum_init(&zs->zil_itx_metaslab_slog_bytes, 0);
	wmsum_init(&zs->zil_itx_metaslab_slog_write, 0);
	wmsum_init(&zs->zil_itx_metaslab_slog_alloc, 0);
}

void
zil_sums_fini(zil_sums_t *zs)
{
	wmsum_fini(&zs->zil_commit_count);
	wmsum_fini(&zs->zil_commit_writer_count);
	wmsum_fini(&zs->zil_itx_count);
	wmsum_fini(&zs->zil_itx_indirect_count);
	wmsum_fini(&zs->zil_itx_indirect_bytes);
	wmsum_fini(&zs->zil_itx_copied_count);
	wmsum_fini(&zs->zil_itx_copied_bytes);
	wmsum_fini(&zs->zil_itx_needcopy_count);
	wmsum_fini(&zs->zil_itx_needcopy_bytes);
	wmsum_fini(&zs->zil_itx_metaslab_normal_count);
	wmsum_fini(&zs->zil_itx_metaslab_normal_bytes);
	wmsum_fini(&zs->zil_itx_metaslab_normal_write);
	wmsum_fini(&zs->zil_itx_metaslab_normal_alloc);
	wmsum_fini(&zs->zil_itx_metaslab_slog_count);
	wmsum_fini(&zs->zil_itx_metaslab_slog_bytes);
	wmsum_fini(&zs->zil_itx_metaslab_slog_write);
	wmsum_fini(&zs->zil_itx_metaslab_slog_alloc);
}

void
zil_kstat_values_update(zil_kstat_values_t *zs, zil_sums_t *zil_sums)
{
	zs->zil_commit_count.value.ui64 =
	    wmsum_value(&zil_sums->zil_commit_count);
	zs->zil_commit_writer_count.value.ui64 =
	    wmsum_value(&zil_sums->zil_commit_writer_count);
	zs->zil_itx_count.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_count);
	zs->zil_itx_indirect_count.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_indirect_count);
	zs->zil_itx_indirect_bytes.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_indirect_bytes);
	zs->zil_itx_copied_count.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_copied_count);
	zs->zil_itx_copied_bytes.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_copied_bytes);
	zs->zil_itx_needcopy_count.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_needcopy_count);
	zs->zil_itx_needcopy_bytes.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_needcopy_bytes);
	zs->zil_itx_metaslab_normal_count.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_metaslab_normal_count);
	zs->zil_itx_metaslab_normal_bytes.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_metaslab_normal_bytes);
	zs->zil_itx_metaslab_normal_write.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_metaslab_normal_write);
	zs->zil_itx_metaslab_normal_alloc.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_metaslab_normal_alloc);
	zs->zil_itx_metaslab_slog_count.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_metaslab_slog_count);
	zs->zil_itx_metaslab_slog_bytes.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_metaslab_slog_bytes);
	zs->zil_itx_metaslab_slog_write.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_metaslab_slog_write);
	zs->zil_itx_metaslab_slog_alloc.value.ui64 =
	    wmsum_value(&zil_sums->zil_itx_metaslab_slog_alloc);
}

 
int
zil_parse(zilog_t *zilog, zil_parse_blk_func_t *parse_blk_func,
    zil_parse_lr_func_t *parse_lr_func, void *arg, uint64_t txg,
    boolean_t decrypt)
{
	const zil_header_t *zh = zilog->zl_header;
	boolean_t claimed = !!zh->zh_claim_txg;
	uint64_t claim_blk_seq = claimed ? zh->zh_claim_blk_seq : UINT64_MAX;
	uint64_t claim_lr_seq = claimed ? zh->zh_claim_lr_seq : UINT64_MAX;
	uint64_t max_blk_seq = 0;
	uint64_t max_lr_seq = 0;
	uint64_t blk_count = 0;
	uint64_t lr_count = 0;
	blkptr_t blk, next_blk = {{{{0}}}};
	int error = 0;

	 
	if (!(zh->zh_flags & ZIL_CLAIM_LR_SEQ_VALID))
		claim_lr_seq = UINT64_MAX;

	 
	zil_bp_tree_init(zilog);

	for (blk = zh->zh_log; !BP_IS_HOLE(&blk); blk = next_blk) {
		uint64_t blk_seq = blk.blk_cksum.zc_word[ZIL_ZC_SEQ];
		int reclen;
		char *lrp, *end;
		arc_buf_t *abuf = NULL;

		if (blk_seq > claim_blk_seq)
			break;

		error = parse_blk_func(zilog, &blk, arg, txg);
		if (error != 0)
			break;
		ASSERT3U(max_blk_seq, <, blk_seq);
		max_blk_seq = blk_seq;
		blk_count++;

		if (max_lr_seq == claim_lr_seq && max_blk_seq == claim_blk_seq)
			break;

		error = zil_read_log_block(zilog, decrypt, &blk, &next_blk,
		    &lrp, &end, &abuf);
		if (error != 0) {
			if (abuf)
				arc_buf_destroy(abuf, &abuf);
			if (claimed) {
				char name[ZFS_MAX_DATASET_NAME_LEN];

				dmu_objset_name(zilog->zl_os, name);

				cmn_err(CE_WARN, "ZFS read log block error %d, "
				    "dataset %s, seq 0x%llx\n", error, name,
				    (u_longlong_t)blk_seq);
			}
			break;
		}

		for (; lrp < end; lrp += reclen) {
			lr_t *lr = (lr_t *)lrp;
			reclen = lr->lrc_reclen;
			ASSERT3U(reclen, >=, sizeof (lr_t));
			if (lr->lrc_seq > claim_lr_seq) {
				arc_buf_destroy(abuf, &abuf);
				goto done;
			}

			error = parse_lr_func(zilog, lr, arg, txg);
			if (error != 0) {
				arc_buf_destroy(abuf, &abuf);
				goto done;
			}
			ASSERT3U(max_lr_seq, <, lr->lrc_seq);
			max_lr_seq = lr->lrc_seq;
			lr_count++;
		}
		arc_buf_destroy(abuf, &abuf);
	}
done:
	zilog->zl_parse_error = error;
	zilog->zl_parse_blk_seq = max_blk_seq;
	zilog->zl_parse_lr_seq = max_lr_seq;
	zilog->zl_parse_blk_count = blk_count;
	zilog->zl_parse_lr_count = lr_count;

	zil_bp_tree_fini(zilog);

	return (error);
}

static int
zil_clear_log_block(zilog_t *zilog, const blkptr_t *bp, void *tx,
    uint64_t first_txg)
{
	(void) tx;
	ASSERT(!BP_IS_HOLE(bp));

	 
	if (bp->blk_birth >= first_txg)
		return (-1);

	if (zil_bp_tree_add(zilog, bp) != 0)
		return (0);

	zio_free(zilog->zl_spa, first_txg, bp);
	return (0);
}

static int
zil_noop_log_record(zilog_t *zilog, const lr_t *lrc, void *tx,
    uint64_t first_txg)
{
	(void) zilog, (void) lrc, (void) tx, (void) first_txg;
	return (0);
}

static int
zil_claim_log_block(zilog_t *zilog, const blkptr_t *bp, void *tx,
    uint64_t first_txg)
{
	 
	if (BP_IS_HOLE(bp) || bp->blk_birth < first_txg ||
	    zil_bp_tree_add(zilog, bp) != 0)
		return (0);

	return (zio_wait(zio_claim(NULL, zilog->zl_spa,
	    tx == NULL ? 0 : first_txg, bp, spa_claim_notify, NULL,
	    ZIO_FLAG_CANFAIL | ZIO_FLAG_SPECULATIVE | ZIO_FLAG_SCRUB)));
}

static int
zil_claim_write(zilog_t *zilog, const lr_t *lrc, void *tx, uint64_t first_txg)
{
	lr_write_t *lr = (lr_write_t *)lrc;
	int error;

	ASSERT(lrc->lrc_txtype == TX_WRITE);

	 
	if (lr->lr_blkptr.blk_birth >= first_txg) {
		error = zil_read_log_data(zilog, lr, NULL);
		if (error != 0)
			return (error);
	}

	return (zil_claim_log_block(zilog, &lr->lr_blkptr, tx, first_txg));
}

static int
zil_claim_clone_range(zilog_t *zilog, const lr_t *lrc, void *tx)
{
	const lr_clone_range_t *lr = (const lr_clone_range_t *)lrc;
	const blkptr_t *bp;
	spa_t *spa;
	uint_t ii;

	ASSERT(lrc->lrc_txtype == TX_CLONE_RANGE);

	if (tx == NULL) {
		return (0);
	}

	 

	spa = zilog->zl_spa;

	for (ii = 0; ii < lr->lr_nbps; ii++) {
		bp = &lr->lr_bps[ii];

		 
		if (!BP_IS_HOLE(bp) && !BP_IS_EMBEDDED(bp)) {
			brt_pending_add(spa, bp, tx);
		}
	}

	return (0);
}

static int
zil_claim_log_record(zilog_t *zilog, const lr_t *lrc, void *tx,
    uint64_t first_txg)
{

	switch (lrc->lrc_txtype) {
	case TX_WRITE:
		return (zil_claim_write(zilog, lrc, tx, first_txg));
	case TX_CLONE_RANGE:
		return (zil_claim_clone_range(zilog, lrc, tx));
	default:
		return (0);
	}
}

static int
zil_free_log_block(zilog_t *zilog, const blkptr_t *bp, void *tx,
    uint64_t claim_txg)
{
	(void) claim_txg;

	zio_free(zilog->zl_spa, dmu_tx_get_txg(tx), bp);

	return (0);
}

static int
zil_free_write(zilog_t *zilog, const lr_t *lrc, void *tx, uint64_t claim_txg)
{
	lr_write_t *lr = (lr_write_t *)lrc;
	blkptr_t *bp = &lr->lr_blkptr;

	ASSERT(lrc->lrc_txtype == TX_WRITE);

	 
	if (bp->blk_birth >= claim_txg && zil_bp_tree_add(zilog, bp) == 0 &&
	    !BP_IS_HOLE(bp)) {
		zio_free(zilog->zl_spa, dmu_tx_get_txg(tx), bp);
	}

	return (0);
}

static int
zil_free_clone_range(zilog_t *zilog, const lr_t *lrc, void *tx)
{
	const lr_clone_range_t *lr = (const lr_clone_range_t *)lrc;
	const blkptr_t *bp;
	spa_t *spa;
	uint_t ii;

	ASSERT(lrc->lrc_txtype == TX_CLONE_RANGE);

	if (tx == NULL) {
		return (0);
	}

	spa = zilog->zl_spa;

	for (ii = 0; ii < lr->lr_nbps; ii++) {
		bp = &lr->lr_bps[ii];

		if (!BP_IS_HOLE(bp)) {
			zio_free(spa, dmu_tx_get_txg(tx), bp);
		}
	}

	return (0);
}

static int
zil_free_log_record(zilog_t *zilog, const lr_t *lrc, void *tx,
    uint64_t claim_txg)
{

	if (claim_txg == 0) {
		return (0);
	}

	switch (lrc->lrc_txtype) {
	case TX_WRITE:
		return (zil_free_write(zilog, lrc, tx, claim_txg));
	case TX_CLONE_RANGE:
		return (zil_free_clone_range(zilog, lrc, tx));
	default:
		return (0);
	}
}

static int
zil_lwb_vdev_compare(const void *x1, const void *x2)
{
	const uint64_t v1 = ((zil_vdev_node_t *)x1)->zv_vdev;
	const uint64_t v2 = ((zil_vdev_node_t *)x2)->zv_vdev;

	return (TREE_CMP(v1, v2));
}

 
static lwb_t *
zil_alloc_lwb(zilog_t *zilog, int sz, blkptr_t *bp, boolean_t slog,
    uint64_t txg, lwb_state_t state)
{
	lwb_t *lwb;

	lwb = kmem_cache_alloc(zil_lwb_cache, KM_SLEEP);
	lwb->lwb_zilog = zilog;
	if (bp) {
		lwb->lwb_blk = *bp;
		lwb->lwb_slim = (BP_GET_CHECKSUM(bp) == ZIO_CHECKSUM_ZILOG2);
		sz = BP_GET_LSIZE(bp);
	} else {
		BP_ZERO(&lwb->lwb_blk);
		lwb->lwb_slim = (spa_version(zilog->zl_spa) >=
		    SPA_VERSION_SLIM_ZIL);
	}
	lwb->lwb_slog = slog;
	lwb->lwb_error = 0;
	if (lwb->lwb_slim) {
		lwb->lwb_nmax = sz;
		lwb->lwb_nused = lwb->lwb_nfilled = sizeof (zil_chain_t);
	} else {
		lwb->lwb_nmax = sz - sizeof (zil_chain_t);
		lwb->lwb_nused = lwb->lwb_nfilled = 0;
	}
	lwb->lwb_sz = sz;
	lwb->lwb_state = state;
	lwb->lwb_buf = zio_buf_alloc(sz);
	lwb->lwb_child_zio = NULL;
	lwb->lwb_write_zio = NULL;
	lwb->lwb_root_zio = NULL;
	lwb->lwb_issued_timestamp = 0;
	lwb->lwb_issued_txg = 0;
	lwb->lwb_alloc_txg = txg;
	lwb->lwb_max_txg = 0;

	mutex_enter(&zilog->zl_lock);
	list_insert_tail(&zilog->zl_lwb_list, lwb);
	if (state != LWB_STATE_NEW)
		zilog->zl_last_lwb_opened = lwb;
	mutex_exit(&zilog->zl_lock);

	return (lwb);
}

static void
zil_free_lwb(zilog_t *zilog, lwb_t *lwb)
{
	ASSERT(MUTEX_HELD(&zilog->zl_lock));
	ASSERT(lwb->lwb_state == LWB_STATE_NEW ||
	    lwb->lwb_state == LWB_STATE_FLUSH_DONE);
	ASSERT3P(lwb->lwb_child_zio, ==, NULL);
	ASSERT3P(lwb->lwb_write_zio, ==, NULL);
	ASSERT3P(lwb->lwb_root_zio, ==, NULL);
	ASSERT3U(lwb->lwb_alloc_txg, <=, spa_syncing_txg(zilog->zl_spa));
	ASSERT3U(lwb->lwb_max_txg, <=, spa_syncing_txg(zilog->zl_spa));
	VERIFY(list_is_empty(&lwb->lwb_itxs));
	VERIFY(list_is_empty(&lwb->lwb_waiters));
	ASSERT(avl_is_empty(&lwb->lwb_vdev_tree));
	ASSERT(!MUTEX_HELD(&lwb->lwb_vdev_lock));

	 
	if (zilog->zl_last_lwb_opened == lwb)
		zilog->zl_last_lwb_opened = NULL;

	kmem_cache_free(zil_lwb_cache, lwb);
}

 
static void
zilog_dirty(zilog_t *zilog, uint64_t txg)
{
	dsl_pool_t *dp = zilog->zl_dmu_pool;
	dsl_dataset_t *ds = dmu_objset_ds(zilog->zl_os);

	ASSERT(spa_writeable(zilog->zl_spa));

	if (ds->ds_is_snapshot)
		panic("dirtying snapshot!");

	if (txg_list_add(&dp->dp_dirty_zilogs, zilog, txg)) {
		 
		dmu_buf_add_ref(ds->ds_dbuf, zilog);

		zilog->zl_dirty_max_txg = MAX(txg, zilog->zl_dirty_max_txg);
	}
}

 
static boolean_t __maybe_unused
zilog_is_dirty_in_txg(zilog_t *zilog, uint64_t txg)
{
	dsl_pool_t *dp = zilog->zl_dmu_pool;

	if (txg_list_member(&dp->dp_dirty_zilogs, zilog, txg & TXG_MASK))
		return (B_TRUE);
	return (B_FALSE);
}

 
static boolean_t
zilog_is_dirty(zilog_t *zilog)
{
	dsl_pool_t *dp = zilog->zl_dmu_pool;

	for (int t = 0; t < TXG_SIZE; t++) {
		if (txg_list_member(&dp->dp_dirty_zilogs, zilog, t))
			return (B_TRUE);
	}
	return (B_FALSE);
}

 
static void
zil_commit_activate_saxattr_feature(zilog_t *zilog)
{
	dsl_dataset_t *ds = dmu_objset_ds(zilog->zl_os);
	uint64_t txg = 0;
	dmu_tx_t *tx = NULL;

	if (spa_feature_is_enabled(zilog->zl_spa, SPA_FEATURE_ZILSAXATTR) &&
	    dmu_objset_type(zilog->zl_os) != DMU_OST_ZVOL &&
	    !dsl_dataset_feature_is_active(ds, SPA_FEATURE_ZILSAXATTR)) {
		tx = dmu_tx_create(zilog->zl_os);
		VERIFY0(dmu_tx_assign(tx, TXG_WAIT));
		dsl_dataset_dirty(ds, tx);
		txg = dmu_tx_get_txg(tx);

		mutex_enter(&ds->ds_lock);
		ds->ds_feature_activation[SPA_FEATURE_ZILSAXATTR] =
		    (void *)B_TRUE;
		mutex_exit(&ds->ds_lock);
		dmu_tx_commit(tx);
		txg_wait_synced(zilog->zl_dmu_pool, txg);
	}
}

 
static lwb_t *
zil_create(zilog_t *zilog)
{
	const zil_header_t *zh = zilog->zl_header;
	lwb_t *lwb = NULL;
	uint64_t txg = 0;
	dmu_tx_t *tx = NULL;
	blkptr_t blk;
	int error = 0;
	boolean_t slog = FALSE;
	dsl_dataset_t *ds = dmu_objset_ds(zilog->zl_os);


	 
	txg_wait_synced(zilog->zl_dmu_pool, zilog->zl_destroy_txg);

	ASSERT(zh->zh_claim_txg == 0);
	ASSERT(zh->zh_replay_seq == 0);

	blk = zh->zh_log;

	 
	if (BP_IS_HOLE(&blk) || BP_SHOULD_BYTESWAP(&blk)) {
		tx = dmu_tx_create(zilog->zl_os);
		VERIFY0(dmu_tx_assign(tx, TXG_WAIT));
		dsl_dataset_dirty(dmu_objset_ds(zilog->zl_os), tx);
		txg = dmu_tx_get_txg(tx);

		if (!BP_IS_HOLE(&blk)) {
			zio_free(zilog->zl_spa, txg, &blk);
			BP_ZERO(&blk);
		}

		error = zio_alloc_zil(zilog->zl_spa, zilog->zl_os, txg, &blk,
		    ZIL_MIN_BLKSZ, &slog);
		if (error == 0)
			zil_init_log_chain(zilog, &blk);
	}

	 
	if (error == 0)
		lwb = zil_alloc_lwb(zilog, 0, &blk, slog, txg, LWB_STATE_NEW);

	 
	if (tx != NULL) {
		 
		if (spa_feature_is_enabled(zilog->zl_spa,
		    SPA_FEATURE_ZILSAXATTR) && dmu_objset_type(zilog->zl_os) !=
		    DMU_OST_ZVOL) {
			mutex_enter(&ds->ds_lock);
			ds->ds_feature_activation[SPA_FEATURE_ZILSAXATTR] =
			    (void *)B_TRUE;
			mutex_exit(&ds->ds_lock);
		}

		dmu_tx_commit(tx);
		txg_wait_synced(zilog->zl_dmu_pool, txg);
	} else {
		 
		zil_commit_activate_saxattr_feature(zilog);
	}
	IMPLY(spa_feature_is_enabled(zilog->zl_spa, SPA_FEATURE_ZILSAXATTR) &&
	    dmu_objset_type(zilog->zl_os) != DMU_OST_ZVOL,
	    dsl_dataset_feature_is_active(ds, SPA_FEATURE_ZILSAXATTR));

	ASSERT(error != 0 || memcmp(&blk, &zh->zh_log, sizeof (blk)) == 0);
	IMPLY(error == 0, lwb != NULL);

	return (lwb);
}

 
boolean_t
zil_destroy(zilog_t *zilog, boolean_t keep_first)
{
	const zil_header_t *zh = zilog->zl_header;
	lwb_t *lwb;
	dmu_tx_t *tx;
	uint64_t txg;

	 
	txg_wait_synced(zilog->zl_dmu_pool, zilog->zl_destroy_txg);

	zilog->zl_old_header = *zh;		 

	if (BP_IS_HOLE(&zh->zh_log))
		return (B_FALSE);

	tx = dmu_tx_create(zilog->zl_os);
	VERIFY0(dmu_tx_assign(tx, TXG_WAIT));
	dsl_dataset_dirty(dmu_objset_ds(zilog->zl_os), tx);
	txg = dmu_tx_get_txg(tx);

	mutex_enter(&zilog->zl_lock);

	ASSERT3U(zilog->zl_destroy_txg, <, txg);
	zilog->zl_destroy_txg = txg;
	zilog->zl_keep_first = keep_first;

	if (!list_is_empty(&zilog->zl_lwb_list)) {
		ASSERT(zh->zh_claim_txg == 0);
		VERIFY(!keep_first);
		while ((lwb = list_remove_head(&zilog->zl_lwb_list)) != NULL) {
			if (lwb->lwb_buf != NULL)
				zio_buf_free(lwb->lwb_buf, lwb->lwb_sz);
			if (!BP_IS_HOLE(&lwb->lwb_blk))
				zio_free(zilog->zl_spa, txg, &lwb->lwb_blk);
			zil_free_lwb(zilog, lwb);
		}
	} else if (!keep_first) {
		zil_destroy_sync(zilog, tx);
	}
	mutex_exit(&zilog->zl_lock);

	dmu_tx_commit(tx);

	return (B_TRUE);
}

void
zil_destroy_sync(zilog_t *zilog, dmu_tx_t *tx)
{
	ASSERT(list_is_empty(&zilog->zl_lwb_list));
	(void) zil_parse(zilog, zil_free_log_block,
	    zil_free_log_record, tx, zilog->zl_header->zh_claim_txg, B_FALSE);
}

int
zil_claim(dsl_pool_t *dp, dsl_dataset_t *ds, void *txarg)
{
	dmu_tx_t *tx = txarg;
	zilog_t *zilog;
	uint64_t first_txg;
	zil_header_t *zh;
	objset_t *os;
	int error;

	error = dmu_objset_own_obj(dp, ds->ds_object,
	    DMU_OST_ANY, B_FALSE, B_FALSE, FTAG, &os);
	if (error != 0) {
		 
		if (error != EBUSY) {
			cmn_err(CE_WARN, "can't open objset for %llu, error %u",
			    (unsigned long long)ds->ds_object, error);
		}

		return (0);
	}

	zilog = dmu_objset_zil(os);
	zh = zil_header_in_syncing_context(zilog);
	ASSERT3U(tx->tx_txg, ==, spa_first_txg(zilog->zl_spa));
	first_txg = spa_min_claim_txg(zilog->zl_spa);

	 
	if (spa_get_log_state(zilog->zl_spa) == SPA_LOG_CLEAR ||
	    (zilog->zl_spa->spa_uberblock.ub_checkpoint_txg != 0 &&
	    zh->zh_claim_txg == 0)) {
		if (!BP_IS_HOLE(&zh->zh_log)) {
			(void) zil_parse(zilog, zil_clear_log_block,
			    zil_noop_log_record, tx, first_txg, B_FALSE);
		}
		BP_ZERO(&zh->zh_log);
		if (os->os_encrypted)
			os->os_next_write_raw[tx->tx_txg & TXG_MASK] = B_TRUE;
		dsl_dataset_dirty(dmu_objset_ds(os), tx);
		dmu_objset_disown(os, B_FALSE, FTAG);
		return (0);
	}

	 
	ASSERT3U(first_txg, ==, spa_first_txg(zilog->zl_spa));

	 
	ASSERT3U(zh->zh_claim_txg, <=, first_txg);
	if (zh->zh_claim_txg == 0 && !BP_IS_HOLE(&zh->zh_log)) {
		(void) zil_parse(zilog, zil_claim_log_block,
		    zil_claim_log_record, tx, first_txg, B_FALSE);
		zh->zh_claim_txg = first_txg;
		zh->zh_claim_blk_seq = zilog->zl_parse_blk_seq;
		zh->zh_claim_lr_seq = zilog->zl_parse_lr_seq;
		if (zilog->zl_parse_lr_count || zilog->zl_parse_blk_count > 1)
			zh->zh_flags |= ZIL_REPLAY_NEEDED;
		zh->zh_flags |= ZIL_CLAIM_LR_SEQ_VALID;
		if (os->os_encrypted)
			os->os_next_write_raw[tx->tx_txg & TXG_MASK] = B_TRUE;
		dsl_dataset_dirty(dmu_objset_ds(os), tx);
	}

	ASSERT3U(first_txg, ==, (spa_last_synced_txg(zilog->zl_spa) + 1));
	dmu_objset_disown(os, B_FALSE, FTAG);
	return (0);
}

 
int
zil_check_log_chain(dsl_pool_t *dp, dsl_dataset_t *ds, void *tx)
{
	(void) dp;
	zilog_t *zilog;
	objset_t *os;
	blkptr_t *bp;
	int error;

	ASSERT(tx == NULL);

	error = dmu_objset_from_ds(ds, &os);
	if (error != 0) {
		cmn_err(CE_WARN, "can't open objset %llu, error %d",
		    (unsigned long long)ds->ds_object, error);
		return (0);
	}

	zilog = dmu_objset_zil(os);
	bp = (blkptr_t *)&zilog->zl_header->zh_log;

	if (!BP_IS_HOLE(bp)) {
		vdev_t *vd;
		boolean_t valid = B_TRUE;

		 
		spa_config_enter(os->os_spa, SCL_STATE, FTAG, RW_READER);
		vd = vdev_lookup_top(os->os_spa, DVA_GET_VDEV(&bp->blk_dva[0]));
		if (vd->vdev_islog && vdev_is_dead(vd))
			valid = vdev_log_state_valid(vd);
		spa_config_exit(os->os_spa, SCL_STATE, FTAG);

		if (!valid)
			return (0);

		 
		zil_header_t *zh = zil_header_in_syncing_context(zilog);
		if (zilog->zl_spa->spa_uberblock.ub_checkpoint_txg != 0 &&
		    zh->zh_claim_txg == 0)
			return (0);
	}

	 
	error = zil_parse(zilog, zil_claim_log_block, zil_claim_log_record, tx,
	    zilog->zl_header->zh_claim_txg ? -1ULL :
	    spa_min_claim_txg(os->os_spa), B_FALSE);

	return ((error == ECKSUM || error == ENOENT) ? 0 : error);
}

 
static void
zil_commit_waiter_skip(zil_commit_waiter_t *zcw)
{
	mutex_enter(&zcw->zcw_lock);
	ASSERT3B(zcw->zcw_done, ==, B_FALSE);
	zcw->zcw_done = B_TRUE;
	cv_broadcast(&zcw->zcw_cv);
	mutex_exit(&zcw->zcw_lock);
}

 
static void
zil_commit_waiter_link_lwb(zil_commit_waiter_t *zcw, lwb_t *lwb)
{
	 
	ASSERT(MUTEX_HELD(&lwb->lwb_zilog->zl_issuer_lock));
	IMPLY(lwb->lwb_state != LWB_STATE_OPENED,
	    MUTEX_HELD(&lwb->lwb_zilog->zl_lock));
	ASSERT3S(lwb->lwb_state, !=, LWB_STATE_NEW);
	ASSERT3S(lwb->lwb_state, !=, LWB_STATE_FLUSH_DONE);

	ASSERT(!list_link_active(&zcw->zcw_node));
	list_insert_tail(&lwb->lwb_waiters, zcw);
	ASSERT3P(zcw->zcw_lwb, ==, NULL);
	zcw->zcw_lwb = lwb;
}

 
static void
zil_commit_waiter_link_nolwb(zil_commit_waiter_t *zcw, list_t *nolwb)
{
	ASSERT(!list_link_active(&zcw->zcw_node));
	list_insert_tail(nolwb, zcw);
	ASSERT3P(zcw->zcw_lwb, ==, NULL);
}

void
zil_lwb_add_block(lwb_t *lwb, const blkptr_t *bp)
{
	avl_tree_t *t = &lwb->lwb_vdev_tree;
	avl_index_t where;
	zil_vdev_node_t *zv, zvsearch;
	int ndvas = BP_GET_NDVAS(bp);
	int i;

	ASSERT3S(lwb->lwb_state, !=, LWB_STATE_WRITE_DONE);
	ASSERT3S(lwb->lwb_state, !=, LWB_STATE_FLUSH_DONE);

	if (zil_nocacheflush)
		return;

	mutex_enter(&lwb->lwb_vdev_lock);
	for (i = 0; i < ndvas; i++) {
		zvsearch.zv_vdev = DVA_GET_VDEV(&bp->blk_dva[i]);
		if (avl_find(t, &zvsearch, &where) == NULL) {
			zv = kmem_alloc(sizeof (*zv), KM_SLEEP);
			zv->zv_vdev = zvsearch.zv_vdev;
			avl_insert(t, zv, where);
		}
	}
	mutex_exit(&lwb->lwb_vdev_lock);
}

static void
zil_lwb_flush_defer(lwb_t *lwb, lwb_t *nlwb)
{
	avl_tree_t *src = &lwb->lwb_vdev_tree;
	avl_tree_t *dst = &nlwb->lwb_vdev_tree;
	void *cookie = NULL;
	zil_vdev_node_t *zv;

	ASSERT3S(lwb->lwb_state, ==, LWB_STATE_WRITE_DONE);
	ASSERT3S(nlwb->lwb_state, !=, LWB_STATE_WRITE_DONE);
	ASSERT3S(nlwb->lwb_state, !=, LWB_STATE_FLUSH_DONE);

	 
	mutex_enter(&nlwb->lwb_vdev_lock);
	 
	while ((zv = avl_destroy_nodes(src, &cookie)) != NULL) {
		avl_index_t where;

		if (avl_find(dst, zv, &where) == NULL) {
			avl_insert(dst, zv, where);
		} else {
			kmem_free(zv, sizeof (*zv));
		}
	}
	mutex_exit(&nlwb->lwb_vdev_lock);
}

void
zil_lwb_add_txg(lwb_t *lwb, uint64_t txg)
{
	lwb->lwb_max_txg = MAX(lwb->lwb_max_txg, txg);
}

 
static void
zil_lwb_flush_vdevs_done(zio_t *zio)
{
	lwb_t *lwb = zio->io_private;
	zilog_t *zilog = lwb->lwb_zilog;
	zil_commit_waiter_t *zcw;
	itx_t *itx;

	spa_config_exit(zilog->zl_spa, SCL_STATE, lwb);

	hrtime_t t = gethrtime() - lwb->lwb_issued_timestamp;

	mutex_enter(&zilog->zl_lock);

	zilog->zl_last_lwb_latency = (zilog->zl_last_lwb_latency * 7 + t) / 8;

	lwb->lwb_root_zio = NULL;

	ASSERT3S(lwb->lwb_state, ==, LWB_STATE_WRITE_DONE);
	lwb->lwb_state = LWB_STATE_FLUSH_DONE;

	if (zilog->zl_last_lwb_opened == lwb) {
		 
		zilog->zl_commit_lr_seq = zilog->zl_lr_seq;
	}

	while ((itx = list_remove_head(&lwb->lwb_itxs)) != NULL)
		zil_itx_destroy(itx);

	while ((zcw = list_remove_head(&lwb->lwb_waiters)) != NULL) {
		mutex_enter(&zcw->zcw_lock);

		ASSERT3P(zcw->zcw_lwb, ==, lwb);
		zcw->zcw_lwb = NULL;
		 

		zcw->zcw_zio_error = zio->io_error;

		ASSERT3B(zcw->zcw_done, ==, B_FALSE);
		zcw->zcw_done = B_TRUE;
		cv_broadcast(&zcw->zcw_cv);

		mutex_exit(&zcw->zcw_lock);
	}

	uint64_t txg = lwb->lwb_issued_txg;

	 
	mutex_exit(&zilog->zl_lock);

	mutex_enter(&zilog->zl_lwb_io_lock);
	ASSERT3U(zilog->zl_lwb_inflight[txg & TXG_MASK], >, 0);
	zilog->zl_lwb_inflight[txg & TXG_MASK]--;
	if (zilog->zl_lwb_inflight[txg & TXG_MASK] == 0)
		cv_broadcast(&zilog->zl_lwb_io_cv);
	mutex_exit(&zilog->zl_lwb_io_lock);
}

 
static void
zil_lwb_flush_wait_all(zilog_t *zilog, uint64_t txg)
{
	ASSERT3U(txg, ==, spa_syncing_txg(zilog->zl_spa));

	mutex_enter(&zilog->zl_lwb_io_lock);
	while (zilog->zl_lwb_inflight[txg & TXG_MASK] > 0)
		cv_wait(&zilog->zl_lwb_io_cv, &zilog->zl_lwb_io_lock);
	mutex_exit(&zilog->zl_lwb_io_lock);

#ifdef ZFS_DEBUG
	mutex_enter(&zilog->zl_lock);
	mutex_enter(&zilog->zl_lwb_io_lock);
	lwb_t *lwb = list_head(&zilog->zl_lwb_list);
	while (lwb != NULL) {
		if (lwb->lwb_issued_txg <= txg) {
			ASSERT(lwb->lwb_state != LWB_STATE_ISSUED);
			ASSERT(lwb->lwb_state != LWB_STATE_WRITE_DONE);
			IMPLY(lwb->lwb_issued_txg > 0,
			    lwb->lwb_state == LWB_STATE_FLUSH_DONE);
		}
		IMPLY(lwb->lwb_state == LWB_STATE_WRITE_DONE ||
		    lwb->lwb_state == LWB_STATE_FLUSH_DONE,
		    lwb->lwb_buf == NULL);
		lwb = list_next(&zilog->zl_lwb_list, lwb);
	}
	mutex_exit(&zilog->zl_lwb_io_lock);
	mutex_exit(&zilog->zl_lock);
#endif
}

 
static void
zil_lwb_write_done(zio_t *zio)
{
	lwb_t *lwb = zio->io_private;
	spa_t *spa = zio->io_spa;
	zilog_t *zilog = lwb->lwb_zilog;
	avl_tree_t *t = &lwb->lwb_vdev_tree;
	void *cookie = NULL;
	zil_vdev_node_t *zv;
	lwb_t *nlwb;

	ASSERT3S(spa_config_held(spa, SCL_STATE, RW_READER), !=, 0);

	abd_free(zio->io_abd);
	zio_buf_free(lwb->lwb_buf, lwb->lwb_sz);
	lwb->lwb_buf = NULL;

	mutex_enter(&zilog->zl_lock);
	ASSERT3S(lwb->lwb_state, ==, LWB_STATE_ISSUED);
	lwb->lwb_state = LWB_STATE_WRITE_DONE;
	lwb->lwb_child_zio = NULL;
	lwb->lwb_write_zio = NULL;

	 
	nlwb = list_next(&zilog->zl_lwb_list, lwb);
	if (nlwb && nlwb->lwb_state != LWB_STATE_ISSUED)
		nlwb = NULL;
	mutex_exit(&zilog->zl_lock);

	if (avl_numnodes(t) == 0)
		return;

	 
	if (zio->io_error != 0) {
		while ((zv = avl_destroy_nodes(t, &cookie)) != NULL)
			kmem_free(zv, sizeof (*zv));
		return;
	}

	 
	if (list_is_empty(&lwb->lwb_waiters) && nlwb != NULL) {
		zil_lwb_flush_defer(lwb, nlwb);
		ASSERT(avl_is_empty(&lwb->lwb_vdev_tree));
		return;
	}

	while ((zv = avl_destroy_nodes(t, &cookie)) != NULL) {
		vdev_t *vd = vdev_lookup_top(spa, zv->zv_vdev);
		if (vd != NULL && !vd->vdev_nowritecache) {
			 
			zio_flush(lwb->lwb_root_zio, vd);
		}
		kmem_free(zv, sizeof (*zv));
	}
}

 
static void
zil_lwb_set_zio_dependency(zilog_t *zilog, lwb_t *lwb)
{
	ASSERT(MUTEX_HELD(&zilog->zl_lock));

	lwb_t *prev_lwb = list_prev(&zilog->zl_lwb_list, lwb);
	if (prev_lwb == NULL ||
	    prev_lwb->lwb_state == LWB_STATE_FLUSH_DONE)
		return;

	 
	if (prev_lwb->lwb_state == LWB_STATE_ISSUED) {
		ASSERT3P(prev_lwb->lwb_write_zio, !=, NULL);
		zio_add_child(lwb->lwb_write_zio, prev_lwb->lwb_write_zio);
	} else {
		ASSERT3S(prev_lwb->lwb_state, ==, LWB_STATE_WRITE_DONE);
	}

	ASSERT3P(prev_lwb->lwb_root_zio, !=, NULL);
	zio_add_child(lwb->lwb_root_zio, prev_lwb->lwb_root_zio);
}


 
static void
zil_lwb_write_open(zilog_t *zilog, lwb_t *lwb)
{
	ASSERT(MUTEX_HELD(&zilog->zl_issuer_lock));

	if (lwb->lwb_state != LWB_STATE_NEW) {
		ASSERT3S(lwb->lwb_state, ==, LWB_STATE_OPENED);
		return;
	}

	mutex_enter(&zilog->zl_lock);
	lwb->lwb_state = LWB_STATE_OPENED;
	zilog->zl_last_lwb_opened = lwb;
	mutex_exit(&zilog->zl_lock);
}

 
static const struct {
	uint64_t	limit;
	uint64_t	blksz;
} zil_block_buckets[] = {
	{ 4096,		4096 },			 
	{ 8192 + 4096,	8192 + 4096 },		 
	{ 32768 + 4096,	32768 + 4096 },		 
	{ 65536 + 4096,	65536 + 4096 },		 
	{ 131072,	131072 },		 
	{ 131072 +4096,	65536 + 4096 },		 
	{ UINT64_MAX,	SPA_OLD_MAXBLOCKSIZE},	 
};

 
static uint_t zil_maxblocksize = SPA_OLD_MAXBLOCKSIZE;

 
static lwb_t *
zil_lwb_write_close(zilog_t *zilog, lwb_t *lwb, lwb_state_t state)
{
	int i;

	ASSERT(MUTEX_HELD(&zilog->zl_issuer_lock));
	ASSERT3S(lwb->lwb_state, ==, LWB_STATE_OPENED);
	lwb->lwb_state = LWB_STATE_CLOSED;

	 
	if (lwb->lwb_error != 0)
		return (NULL);

	 
	uint64_t zil_blksz = zilog->zl_cur_used + sizeof (zil_chain_t);
	for (i = 0; zil_blksz > zil_block_buckets[i].limit; i++)
		continue;
	zil_blksz = MIN(zil_block_buckets[i].blksz, zilog->zl_max_block_size);
	zilog->zl_prev_blks[zilog->zl_prev_rotor] = zil_blksz;
	for (i = 0; i < ZIL_PREV_BLKS; i++)
		zil_blksz = MAX(zil_blksz, zilog->zl_prev_blks[i]);
	DTRACE_PROBE3(zil__block__size, zilog_t *, zilog,
	    uint64_t, zil_blksz,
	    uint64_t, zilog->zl_prev_blks[zilog->zl_prev_rotor]);
	zilog->zl_prev_rotor = (zilog->zl_prev_rotor + 1) & (ZIL_PREV_BLKS - 1);

	return (zil_alloc_lwb(zilog, zil_blksz, NULL, 0, 0, state));
}

 
static void
zil_lwb_write_issue(zilog_t *zilog, lwb_t *lwb)
{
	spa_t *spa = zilog->zl_spa;
	zil_chain_t *zilc;
	boolean_t slog;
	zbookmark_phys_t zb;
	zio_priority_t prio;
	int error;

	ASSERT3S(lwb->lwb_state, ==, LWB_STATE_CLOSED);

	 
	for (itx_t *itx = list_head(&lwb->lwb_itxs); itx;
	    itx = list_next(&lwb->lwb_itxs, itx))
		zil_lwb_commit(zilog, lwb, itx);
	lwb->lwb_nused = lwb->lwb_nfilled;

	lwb->lwb_root_zio = zio_root(spa, zil_lwb_flush_vdevs_done, lwb,
	    ZIO_FLAG_CANFAIL);

	 
	mutex_enter(&zilog->zl_lock);
	lwb->lwb_state = LWB_STATE_READY;
	if (BP_IS_HOLE(&lwb->lwb_blk) && lwb->lwb_error == 0) {
		mutex_exit(&zilog->zl_lock);
		return;
	}
	mutex_exit(&zilog->zl_lock);

next_lwb:
	if (lwb->lwb_slim)
		zilc = (zil_chain_t *)lwb->lwb_buf;
	else
		zilc = (zil_chain_t *)(lwb->lwb_buf + lwb->lwb_nmax);
	int wsz = lwb->lwb_sz;
	if (lwb->lwb_error == 0) {
		abd_t *lwb_abd = abd_get_from_buf(lwb->lwb_buf, lwb->lwb_sz);
		if (!lwb->lwb_slog || zilog->zl_cur_used <= zil_slog_bulk)
			prio = ZIO_PRIORITY_SYNC_WRITE;
		else
			prio = ZIO_PRIORITY_ASYNC_WRITE;
		SET_BOOKMARK(&zb, lwb->lwb_blk.blk_cksum.zc_word[ZIL_ZC_OBJSET],
		    ZB_ZIL_OBJECT, ZB_ZIL_LEVEL,
		    lwb->lwb_blk.blk_cksum.zc_word[ZIL_ZC_SEQ]);
		lwb->lwb_write_zio = zio_rewrite(lwb->lwb_root_zio, spa, 0,
		    &lwb->lwb_blk, lwb_abd, lwb->lwb_sz, zil_lwb_write_done,
		    lwb, prio, ZIO_FLAG_CANFAIL, &zb);
		zil_lwb_add_block(lwb, &lwb->lwb_blk);

		if (lwb->lwb_slim) {
			 
			wsz = P2ROUNDUP_TYPED(lwb->lwb_nused, ZIL_MIN_BLKSZ,
			    int);
			ASSERT3S(wsz, <=, lwb->lwb_sz);
			zio_shrink(lwb->lwb_write_zio, wsz);
			wsz = lwb->lwb_write_zio->io_size;
		}
		memset(lwb->lwb_buf + lwb->lwb_nused, 0, wsz - lwb->lwb_nused);
		zilc->zc_pad = 0;
		zilc->zc_nused = lwb->lwb_nused;
		zilc->zc_eck.zec_cksum = lwb->lwb_blk.blk_cksum;
	} else {
		 
		lwb->lwb_write_zio = zio_null(lwb->lwb_root_zio, spa, NULL,
		    zil_lwb_write_done, lwb, ZIO_FLAG_CANFAIL);
		lwb->lwb_write_zio->io_error = lwb->lwb_error;
	}
	if (lwb->lwb_child_zio)
		zio_add_child(lwb->lwb_write_zio, lwb->lwb_child_zio);

	 
	dmu_tx_t *tx = dmu_tx_create(zilog->zl_os);
	VERIFY0(dmu_tx_assign(tx, TXG_WAIT | TXG_NOTHROTTLE));
	dsl_dataset_dirty(dmu_objset_ds(zilog->zl_os), tx);
	uint64_t txg = dmu_tx_get_txg(tx);

	 
	lwb_t *nlwb = list_next(&zilog->zl_lwb_list, lwb);
	blkptr_t *bp = &zilc->zc_next_blk;
	BP_ZERO(bp);
	error = lwb->lwb_error;
	if (error == 0) {
		error = zio_alloc_zil(spa, zilog->zl_os, txg, bp, nlwb->lwb_sz,
		    &slog);
	}
	if (error == 0) {
		ASSERT3U(bp->blk_birth, ==, txg);
		BP_SET_CHECKSUM(bp, nlwb->lwb_slim ? ZIO_CHECKSUM_ZILOG2 :
		    ZIO_CHECKSUM_ZILOG);
		bp->blk_cksum = lwb->lwb_blk.blk_cksum;
		bp->blk_cksum.zc_word[ZIL_ZC_SEQ]++;
	}

	 
	mutex_enter(&zilog->zl_lwb_io_lock);
	lwb->lwb_issued_txg = txg;
	zilog->zl_lwb_inflight[txg & TXG_MASK]++;
	zilog->zl_lwb_max_issued_txg = MAX(txg, zilog->zl_lwb_max_issued_txg);
	mutex_exit(&zilog->zl_lwb_io_lock);
	dmu_tx_commit(tx);

	spa_config_enter(spa, SCL_STATE, lwb, RW_READER);

	 
	mutex_enter(&zilog->zl_lock);
	zil_lwb_set_zio_dependency(zilog, lwb);
	lwb->lwb_state = LWB_STATE_ISSUED;

	if (nlwb) {
		nlwb->lwb_blk = *bp;
		nlwb->lwb_error = error;
		nlwb->lwb_slog = slog;
		nlwb->lwb_alloc_txg = txg;
		if (nlwb->lwb_state != LWB_STATE_READY)
			nlwb = NULL;
	}
	mutex_exit(&zilog->zl_lock);

	if (lwb->lwb_slog) {
		ZIL_STAT_BUMP(zilog, zil_itx_metaslab_slog_count);
		ZIL_STAT_INCR(zilog, zil_itx_metaslab_slog_bytes,
		    lwb->lwb_nused);
		ZIL_STAT_INCR(zilog, zil_itx_metaslab_slog_write,
		    wsz);
		ZIL_STAT_INCR(zilog, zil_itx_metaslab_slog_alloc,
		    BP_GET_LSIZE(&lwb->lwb_blk));
	} else {
		ZIL_STAT_BUMP(zilog, zil_itx_metaslab_normal_count);
		ZIL_STAT_INCR(zilog, zil_itx_metaslab_normal_bytes,
		    lwb->lwb_nused);
		ZIL_STAT_INCR(zilog, zil_itx_metaslab_normal_write,
		    wsz);
		ZIL_STAT_INCR(zilog, zil_itx_metaslab_normal_alloc,
		    BP_GET_LSIZE(&lwb->lwb_blk));
	}
	lwb->lwb_issued_timestamp = gethrtime();
	if (lwb->lwb_child_zio)
		zio_nowait(lwb->lwb_child_zio);
	zio_nowait(lwb->lwb_write_zio);
	zio_nowait(lwb->lwb_root_zio);

	 
	lwb = nlwb;
	if (lwb)
		goto next_lwb;
}

 
uint64_t
zil_max_log_data(zilog_t *zilog, size_t hdrsize)
{
	return (zilog->zl_max_block_size - sizeof (zil_chain_t) - hdrsize);
}

 
static inline uint64_t
zil_max_waste_space(zilog_t *zilog)
{
	return (zil_max_log_data(zilog, sizeof (lr_write_t)) / 16);
}

 
static uint_t zil_maxcopied = 7680;

uint64_t
zil_max_copied_data(zilog_t *zilog)
{
	uint64_t max_data = zil_max_log_data(zilog, sizeof (lr_write_t));
	return (MIN(max_data, zil_maxcopied));
}

 
static lwb_t *
zil_lwb_assign(zilog_t *zilog, lwb_t *lwb, itx_t *itx, list_t *ilwbs)
{
	itx_t *citx;
	lr_t *lr, *clr;
	lr_write_t *lrw;
	uint64_t dlen, dnow, lwb_sp, reclen, max_log_data;

	ASSERT(MUTEX_HELD(&zilog->zl_issuer_lock));
	ASSERT3P(lwb, !=, NULL);
	ASSERT3P(lwb->lwb_buf, !=, NULL);

	zil_lwb_write_open(zilog, lwb);

	lr = &itx->itx_lr;
	lrw = (lr_write_t *)lr;

	 
	if (lr->lrc_txtype == TX_COMMIT) {
		zil_commit_waiter_link_lwb(itx->itx_private, lwb);
		list_insert_tail(&lwb->lwb_itxs, itx);
		return (lwb);
	}

	if (lr->lrc_txtype == TX_WRITE && itx->itx_wr_state == WR_NEED_COPY) {
		dlen = P2ROUNDUP_TYPED(
		    lrw->lr_length, sizeof (uint64_t), uint64_t);
	} else {
		dlen = 0;
	}
	reclen = lr->lrc_reclen;
	zilog->zl_cur_used += (reclen + dlen);

cont:
	 
	lwb_sp = lwb->lwb_nmax - lwb->lwb_nused;
	max_log_data = zil_max_log_data(zilog, sizeof (lr_write_t));
	if (reclen > lwb_sp || (reclen + dlen > lwb_sp &&
	    lwb_sp < zil_max_waste_space(zilog) &&
	    (dlen % max_log_data == 0 ||
	    lwb_sp < reclen + dlen % max_log_data))) {
		list_insert_tail(ilwbs, lwb);
		lwb = zil_lwb_write_close(zilog, lwb, LWB_STATE_OPENED);
		if (lwb == NULL)
			return (NULL);
		lwb_sp = lwb->lwb_nmax - lwb->lwb_nused;

		 
		ASSERT3U(reclen + MIN(dlen, sizeof (uint64_t)), <=, lwb_sp);
	}

	dnow = MIN(dlen, lwb_sp - reclen);
	if (dlen > dnow) {
		ASSERT3U(lr->lrc_txtype, ==, TX_WRITE);
		ASSERT3U(itx->itx_wr_state, ==, WR_NEED_COPY);
		citx = zil_itx_clone(itx);
		clr = &citx->itx_lr;
		lr_write_t *clrw = (lr_write_t *)clr;
		clrw->lr_length = dnow;
		lrw->lr_offset += dnow;
		lrw->lr_length -= dnow;
	} else {
		citx = itx;
		clr = lr;
	}

	 
	clr->lrc_seq = ++zilog->zl_lr_seq;

	lwb->lwb_nused += reclen + dnow;
	ASSERT3U(lwb->lwb_nused, <=, lwb->lwb_nmax);
	ASSERT0(P2PHASE(lwb->lwb_nused, sizeof (uint64_t)));

	zil_lwb_add_txg(lwb, lr->lrc_txg);
	list_insert_tail(&lwb->lwb_itxs, citx);

	dlen -= dnow;
	if (dlen > 0) {
		zilog->zl_cur_used += reclen;
		goto cont;
	}

	if (lr->lrc_txtype == TX_WRITE &&
	    lr->lrc_txg > spa_freeze_txg(zilog->zl_spa))
		txg_wait_synced(zilog->zl_dmu_pool, lr->lrc_txg);

	return (lwb);
}

 
static void
zil_lwb_commit(zilog_t *zilog, lwb_t *lwb, itx_t *itx)
{
	lr_t *lr, *lrb;
	lr_write_t *lrw, *lrwb;
	char *lr_buf;
	uint64_t dlen, reclen;

	lr = &itx->itx_lr;
	lrw = (lr_write_t *)lr;

	if (lr->lrc_txtype == TX_COMMIT)
		return;

	if (lr->lrc_txtype == TX_WRITE && itx->itx_wr_state == WR_NEED_COPY) {
		dlen = P2ROUNDUP_TYPED(
		    lrw->lr_length, sizeof (uint64_t), uint64_t);
	} else {
		dlen = 0;
	}
	reclen = lr->lrc_reclen;
	ASSERT3U(reclen + dlen, <=, lwb->lwb_nused - lwb->lwb_nfilled);

	lr_buf = lwb->lwb_buf + lwb->lwb_nfilled;
	memcpy(lr_buf, lr, reclen);
	lrb = (lr_t *)lr_buf;		 
	lrwb = (lr_write_t *)lrb;	 

	ZIL_STAT_BUMP(zilog, zil_itx_count);

	 
	if (lr->lrc_txtype == TX_WRITE) {
		if (itx->itx_wr_state == WR_COPIED) {
			ZIL_STAT_BUMP(zilog, zil_itx_copied_count);
			ZIL_STAT_INCR(zilog, zil_itx_copied_bytes,
			    lrw->lr_length);
		} else {
			char *dbuf;
			int error;

			if (itx->itx_wr_state == WR_NEED_COPY) {
				dbuf = lr_buf + reclen;
				lrb->lrc_reclen += dlen;
				ZIL_STAT_BUMP(zilog, zil_itx_needcopy_count);
				ZIL_STAT_INCR(zilog, zil_itx_needcopy_bytes,
				    dlen);
			} else {
				ASSERT3S(itx->itx_wr_state, ==, WR_INDIRECT);
				dbuf = NULL;
				ZIL_STAT_BUMP(zilog, zil_itx_indirect_count);
				ZIL_STAT_INCR(zilog, zil_itx_indirect_bytes,
				    lrw->lr_length);
				if (lwb->lwb_child_zio == NULL) {
					lwb->lwb_child_zio = zio_root(
					    zilog->zl_spa, NULL, NULL,
					    ZIO_FLAG_CANFAIL);
				}
			}

			 
			error = zilog->zl_get_data(itx->itx_private,
			    itx->itx_gen, lrwb, dbuf, lwb,
			    lwb->lwb_child_zio);
			if (dbuf != NULL && error == 0) {
				 
				memset((char *)dbuf + lrwb->lr_length, 0,
				    dlen - lrwb->lr_length);
			}

			 
			switch (error) {
			case 0:
				break;
			default:
				cmn_err(CE_WARN, "zil_lwb_commit() received "
				    "unexpected error %d from ->zl_get_data()"
				    ". Falling back to txg_wait_synced().",
				    error);
				zfs_fallthrough;
			case EIO:
				txg_wait_synced(zilog->zl_dmu_pool,
				    lr->lrc_txg);
				zfs_fallthrough;
			case ENOENT:
				zfs_fallthrough;
			case EEXIST:
				zfs_fallthrough;
			case EALREADY:
				return;
			}
		}
	}

	lwb->lwb_nfilled += reclen + dlen;
	ASSERT3S(lwb->lwb_nfilled, <=, lwb->lwb_nused);
	ASSERT0(P2PHASE(lwb->lwb_nfilled, sizeof (uint64_t)));
}

itx_t *
zil_itx_create(uint64_t txtype, size_t olrsize)
{
	size_t itxsize, lrsize;
	itx_t *itx;

	lrsize = P2ROUNDUP_TYPED(olrsize, sizeof (uint64_t), size_t);
	itxsize = offsetof(itx_t, itx_lr) + lrsize;

	itx = zio_data_buf_alloc(itxsize);
	itx->itx_lr.lrc_txtype = txtype;
	itx->itx_lr.lrc_reclen = lrsize;
	itx->itx_lr.lrc_seq = 0;	 
	memset((char *)&itx->itx_lr + olrsize, 0, lrsize - olrsize);
	itx->itx_sync = B_TRUE;		 
	itx->itx_callback = NULL;
	itx->itx_callback_data = NULL;
	itx->itx_size = itxsize;

	return (itx);
}

static itx_t *
zil_itx_clone(itx_t *oitx)
{
	itx_t *itx = zio_data_buf_alloc(oitx->itx_size);
	memcpy(itx, oitx, oitx->itx_size);
	itx->itx_callback = NULL;
	itx->itx_callback_data = NULL;
	return (itx);
}

void
zil_itx_destroy(itx_t *itx)
{
	IMPLY(itx->itx_lr.lrc_txtype == TX_COMMIT, itx->itx_callback == NULL);
	IMPLY(itx->itx_callback != NULL, itx->itx_lr.lrc_txtype != TX_COMMIT);

	if (itx->itx_callback != NULL)
		itx->itx_callback(itx->itx_callback_data);

	zio_data_buf_free(itx, itx->itx_size);
}

 
static void
zil_itxg_clean(void *arg)
{
	itx_t *itx;
	list_t *list;
	avl_tree_t *t;
	void *cookie;
	itxs_t *itxs = arg;
	itx_async_node_t *ian;

	list = &itxs->i_sync_list;
	while ((itx = list_remove_head(list)) != NULL) {
		 
		if (itx->itx_lr.lrc_txtype == TX_COMMIT)
			zil_commit_waiter_skip(itx->itx_private);

		zil_itx_destroy(itx);
	}

	cookie = NULL;
	t = &itxs->i_async_tree;
	while ((ian = avl_destroy_nodes(t, &cookie)) != NULL) {
		list = &ian->ia_list;
		while ((itx = list_remove_head(list)) != NULL) {
			 
			ASSERT3U(itx->itx_lr.lrc_txtype, !=, TX_COMMIT);
			zil_itx_destroy(itx);
		}
		list_destroy(list);
		kmem_free(ian, sizeof (itx_async_node_t));
	}
	avl_destroy(t);

	kmem_free(itxs, sizeof (itxs_t));
}

static int
zil_aitx_compare(const void *x1, const void *x2)
{
	const uint64_t o1 = ((itx_async_node_t *)x1)->ia_foid;
	const uint64_t o2 = ((itx_async_node_t *)x2)->ia_foid;

	return (TREE_CMP(o1, o2));
}

 
void
zil_remove_async(zilog_t *zilog, uint64_t oid)
{
	uint64_t otxg, txg;
	itx_async_node_t *ian;
	avl_tree_t *t;
	avl_index_t where;
	list_t clean_list;
	itx_t *itx;

	ASSERT(oid != 0);
	list_create(&clean_list, sizeof (itx_t), offsetof(itx_t, itx_node));

	if (spa_freeze_txg(zilog->zl_spa) != UINT64_MAX)  
		otxg = ZILTEST_TXG;
	else
		otxg = spa_last_synced_txg(zilog->zl_spa) + 1;

	for (txg = otxg; txg < (otxg + TXG_CONCURRENT_STATES); txg++) {
		itxg_t *itxg = &zilog->zl_itxg[txg & TXG_MASK];

		mutex_enter(&itxg->itxg_lock);
		if (itxg->itxg_txg != txg) {
			mutex_exit(&itxg->itxg_lock);
			continue;
		}

		 
		t = &itxg->itxg_itxs->i_async_tree;
		ian = avl_find(t, &oid, &where);
		if (ian != NULL)
			list_move_tail(&clean_list, &ian->ia_list);
		mutex_exit(&itxg->itxg_lock);
	}
	while ((itx = list_remove_head(&clean_list)) != NULL) {
		 
		ASSERT3U(itx->itx_lr.lrc_txtype, !=, TX_COMMIT);
		zil_itx_destroy(itx);
	}
	list_destroy(&clean_list);
}

void
zil_itx_assign(zilog_t *zilog, itx_t *itx, dmu_tx_t *tx)
{
	uint64_t txg;
	itxg_t *itxg;
	itxs_t *itxs, *clean = NULL;

	 
	if ((itx->itx_lr.lrc_txtype & ~TX_CI) == TX_RENAME)
		zil_async_to_sync(zilog, itx->itx_oid);

	if (spa_freeze_txg(zilog->zl_spa) != UINT64_MAX)
		txg = ZILTEST_TXG;
	else
		txg = dmu_tx_get_txg(tx);

	itxg = &zilog->zl_itxg[txg & TXG_MASK];
	mutex_enter(&itxg->itxg_lock);
	itxs = itxg->itxg_itxs;
	if (itxg->itxg_txg != txg) {
		if (itxs != NULL) {
			 
			zfs_dbgmsg("zil_itx_assign: missed itx cleanup for "
			    "txg %llu", (u_longlong_t)itxg->itxg_txg);
			clean = itxg->itxg_itxs;
		}
		itxg->itxg_txg = txg;
		itxs = itxg->itxg_itxs = kmem_zalloc(sizeof (itxs_t),
		    KM_SLEEP);

		list_create(&itxs->i_sync_list, sizeof (itx_t),
		    offsetof(itx_t, itx_node));
		avl_create(&itxs->i_async_tree, zil_aitx_compare,
		    sizeof (itx_async_node_t),
		    offsetof(itx_async_node_t, ia_node));
	}
	if (itx->itx_sync) {
		list_insert_tail(&itxs->i_sync_list, itx);
	} else {
		avl_tree_t *t = &itxs->i_async_tree;
		uint64_t foid =
		    LR_FOID_GET_OBJ(((lr_ooo_t *)&itx->itx_lr)->lr_foid);
		itx_async_node_t *ian;
		avl_index_t where;

		ian = avl_find(t, &foid, &where);
		if (ian == NULL) {
			ian = kmem_alloc(sizeof (itx_async_node_t),
			    KM_SLEEP);
			list_create(&ian->ia_list, sizeof (itx_t),
			    offsetof(itx_t, itx_node));
			ian->ia_foid = foid;
			avl_insert(t, ian, where);
		}
		list_insert_tail(&ian->ia_list, itx);
	}

	itx->itx_lr.lrc_txg = dmu_tx_get_txg(tx);

	 
	zilog_dirty(zilog, dmu_tx_get_txg(tx));
	mutex_exit(&itxg->itxg_lock);

	 
	if (clean != NULL)
		zil_itxg_clean(clean);
}

 
void
zil_clean(zilog_t *zilog, uint64_t synced_txg)
{
	itxg_t *itxg = &zilog->zl_itxg[synced_txg & TXG_MASK];
	itxs_t *clean_me;

	ASSERT3U(synced_txg, <, ZILTEST_TXG);

	mutex_enter(&itxg->itxg_lock);
	if (itxg->itxg_itxs == NULL || itxg->itxg_txg == ZILTEST_TXG) {
		mutex_exit(&itxg->itxg_lock);
		return;
	}
	ASSERT3U(itxg->itxg_txg, <=, synced_txg);
	ASSERT3U(itxg->itxg_txg, !=, 0);
	clean_me = itxg->itxg_itxs;
	itxg->itxg_itxs = NULL;
	itxg->itxg_txg = 0;
	mutex_exit(&itxg->itxg_lock);
	 
	ASSERT3P(zilog->zl_dmu_pool, !=, NULL);
	ASSERT3P(zilog->zl_dmu_pool->dp_zil_clean_taskq, !=, NULL);
	taskqid_t id = taskq_dispatch(zilog->zl_dmu_pool->dp_zil_clean_taskq,
	    zil_itxg_clean, clean_me, TQ_NOSLEEP);
	if (id == TASKQID_INVALID)
		zil_itxg_clean(clean_me);
}

 
static uint64_t
zil_get_commit_list(zilog_t *zilog)
{
	uint64_t otxg, txg, wtxg = 0;
	list_t *commit_list = &zilog->zl_itx_commit_list;

	ASSERT(MUTEX_HELD(&zilog->zl_issuer_lock));

	if (spa_freeze_txg(zilog->zl_spa) != UINT64_MAX)  
		otxg = ZILTEST_TXG;
	else
		otxg = spa_last_synced_txg(zilog->zl_spa) + 1;

	 
	for (txg = otxg; txg < (otxg + TXG_CONCURRENT_STATES); txg++) {
		itxg_t *itxg = &zilog->zl_itxg[txg & TXG_MASK];

		mutex_enter(&itxg->itxg_lock);
		if (itxg->itxg_txg != txg) {
			mutex_exit(&itxg->itxg_lock);
			continue;
		}

		 
		ASSERT(zilog_is_dirty_in_txg(zilog, txg) ||
		    spa_freeze_txg(zilog->zl_spa) != UINT64_MAX);
		list_t *sync_list = &itxg->itxg_itxs->i_sync_list;
		if (unlikely(zilog->zl_suspend > 0)) {
			 
			if (!list_is_empty(sync_list))
				wtxg = MAX(wtxg, txg);
		} else {
			list_move_tail(commit_list, sync_list);
		}

		mutex_exit(&itxg->itxg_lock);
	}
	return (wtxg);
}

 
void
zil_async_to_sync(zilog_t *zilog, uint64_t foid)
{
	uint64_t otxg, txg;
	itx_async_node_t *ian;
	avl_tree_t *t;
	avl_index_t where;

	if (spa_freeze_txg(zilog->zl_spa) != UINT64_MAX)  
		otxg = ZILTEST_TXG;
	else
		otxg = spa_last_synced_txg(zilog->zl_spa) + 1;

	 
	for (txg = otxg; txg < (otxg + TXG_CONCURRENT_STATES); txg++) {
		itxg_t *itxg = &zilog->zl_itxg[txg & TXG_MASK];

		mutex_enter(&itxg->itxg_lock);
		if (itxg->itxg_txg != txg) {
			mutex_exit(&itxg->itxg_lock);
			continue;
		}

		 
		t = &itxg->itxg_itxs->i_async_tree;
		if (foid != 0) {
			ian = avl_find(t, &foid, &where);
			if (ian != NULL) {
				list_move_tail(&itxg->itxg_itxs->i_sync_list,
				    &ian->ia_list);
			}
		} else {
			void *cookie = NULL;

			while ((ian = avl_destroy_nodes(t, &cookie)) != NULL) {
				list_move_tail(&itxg->itxg_itxs->i_sync_list,
				    &ian->ia_list);
				list_destroy(&ian->ia_list);
				kmem_free(ian, sizeof (itx_async_node_t));
			}
		}
		mutex_exit(&itxg->itxg_lock);
	}
}

 
static void
zil_prune_commit_list(zilog_t *zilog)
{
	itx_t *itx;

	ASSERT(MUTEX_HELD(&zilog->zl_issuer_lock));

	while ((itx = list_head(&zilog->zl_itx_commit_list)) != NULL) {
		lr_t *lrc = &itx->itx_lr;
		if (lrc->lrc_txtype != TX_COMMIT)
			break;

		mutex_enter(&zilog->zl_lock);

		lwb_t *last_lwb = zilog->zl_last_lwb_opened;
		if (last_lwb == NULL ||
		    last_lwb->lwb_state == LWB_STATE_FLUSH_DONE) {
			 
			zil_commit_waiter_skip(itx->itx_private);
		} else {
			zil_commit_waiter_link_lwb(itx->itx_private, last_lwb);
		}

		mutex_exit(&zilog->zl_lock);

		list_remove(&zilog->zl_itx_commit_list, itx);
		zil_itx_destroy(itx);
	}

	IMPLY(itx != NULL, itx->itx_lr.lrc_txtype != TX_COMMIT);
}

static void
zil_commit_writer_stall(zilog_t *zilog)
{
	 
	ASSERT(MUTEX_HELD(&zilog->zl_issuer_lock));
	txg_wait_synced(zilog->zl_dmu_pool, 0);
	ASSERT(list_is_empty(&zilog->zl_lwb_list));
}

 
static void
zil_process_commit_list(zilog_t *zilog, zil_commit_waiter_t *zcw, list_t *ilwbs)
{
	spa_t *spa = zilog->zl_spa;
	list_t nolwb_itxs;
	list_t nolwb_waiters;
	lwb_t *lwb, *plwb;
	itx_t *itx;
	boolean_t first = B_TRUE;

	ASSERT(MUTEX_HELD(&zilog->zl_issuer_lock));

	 
	if (list_is_empty(&zilog->zl_itx_commit_list))
		return;

	list_create(&nolwb_itxs, sizeof (itx_t), offsetof(itx_t, itx_node));
	list_create(&nolwb_waiters, sizeof (zil_commit_waiter_t),
	    offsetof(zil_commit_waiter_t, zcw_node));

	lwb = list_tail(&zilog->zl_lwb_list);
	if (lwb == NULL) {
		lwb = zil_create(zilog);
	} else {
		 
		zil_commit_activate_saxattr_feature(zilog);
		ASSERT(lwb->lwb_state == LWB_STATE_NEW ||
		    lwb->lwb_state == LWB_STATE_OPENED);
		first = (lwb->lwb_state == LWB_STATE_NEW) &&
		    ((plwb = list_prev(&zilog->zl_lwb_list, lwb)) == NULL ||
		    plwb->lwb_state == LWB_STATE_FLUSH_DONE);
	}

	while ((itx = list_remove_head(&zilog->zl_itx_commit_list)) != NULL) {
		lr_t *lrc = &itx->itx_lr;
		uint64_t txg = lrc->lrc_txg;

		ASSERT3U(txg, !=, 0);

		if (lrc->lrc_txtype == TX_COMMIT) {
			DTRACE_PROBE2(zil__process__commit__itx,
			    zilog_t *, zilog, itx_t *, itx);
		} else {
			DTRACE_PROBE2(zil__process__normal__itx,
			    zilog_t *, zilog, itx_t *, itx);
		}

		boolean_t synced = txg <= spa_last_synced_txg(spa);
		boolean_t frozen = txg > spa_freeze_txg(spa);

		 
		if (frozen || !synced || lrc->lrc_txtype == TX_COMMIT) {
			if (lwb != NULL) {
				lwb = zil_lwb_assign(zilog, lwb, itx, ilwbs);
				if (lwb == NULL) {
					list_insert_tail(&nolwb_itxs, itx);
				} else if ((zcw->zcw_lwb != NULL &&
				    zcw->zcw_lwb != lwb) || zcw->zcw_done) {
					 
					first = B_FALSE;
					break;
				}
			} else {
				if (lrc->lrc_txtype == TX_COMMIT) {
					zil_commit_waiter_link_nolwb(
					    itx->itx_private, &nolwb_waiters);
				}
				list_insert_tail(&nolwb_itxs, itx);
			}
		} else {
			ASSERT3S(lrc->lrc_txtype, !=, TX_COMMIT);
			zil_itx_destroy(itx);
		}
	}

	if (lwb == NULL) {
		 
		while ((lwb = list_remove_head(ilwbs)) != NULL)
			zil_lwb_write_issue(zilog, lwb);
		zil_commit_writer_stall(zilog);

		 
		zil_commit_waiter_t *zcw;
		while ((zcw = list_remove_head(&nolwb_waiters)) != NULL)
			zil_commit_waiter_skip(zcw);

		 
		while ((itx = list_remove_head(&nolwb_itxs)) != NULL)
			zil_itx_destroy(itx);
	} else {
		ASSERT(list_is_empty(&nolwb_waiters));
		ASSERT3P(lwb, !=, NULL);
		ASSERT(lwb->lwb_state == LWB_STATE_NEW ||
		    lwb->lwb_state == LWB_STATE_OPENED);

		 
		if (lwb->lwb_state == LWB_STATE_OPENED && first) {
			hrtime_t sleep = zilog->zl_last_lwb_latency *
			    zfs_commit_timeout_pct / 100;
			if (sleep < zil_min_commit_timeout ||
			    lwb->lwb_nmax - lwb->lwb_nused <
			    lwb->lwb_nmax / 8) {
				list_insert_tail(ilwbs, lwb);
				lwb = zil_lwb_write_close(zilog, lwb,
				    LWB_STATE_NEW);
				zilog->zl_cur_used = 0;
				if (lwb == NULL) {
					while ((lwb = list_remove_head(ilwbs))
					    != NULL)
						zil_lwb_write_issue(zilog, lwb);
					zil_commit_writer_stall(zilog);
				}
			}
		}
	}
}

 
static uint64_t
zil_commit_writer(zilog_t *zilog, zil_commit_waiter_t *zcw)
{
	list_t ilwbs;
	lwb_t *lwb;
	uint64_t wtxg = 0;

	ASSERT(!MUTEX_HELD(&zilog->zl_lock));
	ASSERT(spa_writeable(zilog->zl_spa));

	list_create(&ilwbs, sizeof (lwb_t), offsetof(lwb_t, lwb_issue_node));
	mutex_enter(&zilog->zl_issuer_lock);

	if (zcw->zcw_lwb != NULL || zcw->zcw_done) {
		 
		goto out;
	}

	ZIL_STAT_BUMP(zilog, zil_commit_writer_count);

	wtxg = zil_get_commit_list(zilog);
	zil_prune_commit_list(zilog);
	zil_process_commit_list(zilog, zcw, &ilwbs);

out:
	mutex_exit(&zilog->zl_issuer_lock);
	while ((lwb = list_remove_head(&ilwbs)) != NULL)
		zil_lwb_write_issue(zilog, lwb);
	list_destroy(&ilwbs);
	return (wtxg);
}

static void
zil_commit_waiter_timeout(zilog_t *zilog, zil_commit_waiter_t *zcw)
{
	ASSERT(!MUTEX_HELD(&zilog->zl_issuer_lock));
	ASSERT(MUTEX_HELD(&zcw->zcw_lock));
	ASSERT3B(zcw->zcw_done, ==, B_FALSE);

	lwb_t *lwb = zcw->zcw_lwb;
	ASSERT3P(lwb, !=, NULL);
	ASSERT3S(lwb->lwb_state, !=, LWB_STATE_NEW);

	 
	if (lwb->lwb_state != LWB_STATE_OPENED)
		return;

	 
	mutex_exit(&zcw->zcw_lock);
	mutex_enter(&zilog->zl_issuer_lock);
	mutex_enter(&zcw->zcw_lock);

	 
	if (zcw->zcw_done) {
		mutex_exit(&zilog->zl_issuer_lock);
		return;
	}

	ASSERT3P(lwb, ==, zcw->zcw_lwb);

	 
	if (lwb->lwb_state != LWB_STATE_OPENED) {
		mutex_exit(&zilog->zl_issuer_lock);
		return;
	}

	 
	mutex_exit(&zcw->zcw_lock);

	 
	lwb_t *nlwb = zil_lwb_write_close(zilog, lwb, LWB_STATE_NEW);

	ASSERT3S(lwb->lwb_state, ==, LWB_STATE_CLOSED);

	 
	zilog->zl_cur_used = 0;

	if (nlwb == NULL) {
		 
		zil_lwb_write_issue(zilog, lwb);
		zil_commit_writer_stall(zilog);
		mutex_exit(&zilog->zl_issuer_lock);
	} else {
		mutex_exit(&zilog->zl_issuer_lock);
		zil_lwb_write_issue(zilog, lwb);
	}
	mutex_enter(&zcw->zcw_lock);
}

 
static void
zil_commit_waiter(zilog_t *zilog, zil_commit_waiter_t *zcw)
{
	ASSERT(!MUTEX_HELD(&zilog->zl_lock));
	ASSERT(!MUTEX_HELD(&zilog->zl_issuer_lock));
	ASSERT(spa_writeable(zilog->zl_spa));

	mutex_enter(&zcw->zcw_lock);

	 
	int pct = MAX(zfs_commit_timeout_pct, 1);
	hrtime_t sleep = (zilog->zl_last_lwb_latency * pct) / 100;
	hrtime_t wakeup = gethrtime() + sleep;
	boolean_t timedout = B_FALSE;

	while (!zcw->zcw_done) {
		ASSERT(MUTEX_HELD(&zcw->zcw_lock));

		lwb_t *lwb = zcw->zcw_lwb;

		 
		IMPLY(lwb != NULL, lwb->lwb_state != LWB_STATE_NEW);

		if (lwb != NULL && lwb->lwb_state == LWB_STATE_OPENED) {
			ASSERT3B(timedout, ==, B_FALSE);

			 
			int rc = cv_timedwait_hires(&zcw->zcw_cv,
			    &zcw->zcw_lock, wakeup, USEC2NSEC(1),
			    CALLOUT_FLAG_ABSOLUTE);

			if (rc != -1 || zcw->zcw_done)
				continue;

			timedout = B_TRUE;
			zil_commit_waiter_timeout(zilog, zcw);

			if (!zcw->zcw_done) {
				 
				ASSERT3P(lwb, ==, zcw->zcw_lwb);
				ASSERT3S(lwb->lwb_state, !=, LWB_STATE_OPENED);
			}
		} else {
			 

			IMPLY(lwb != NULL,
			    lwb->lwb_state == LWB_STATE_CLOSED ||
			    lwb->lwb_state == LWB_STATE_READY ||
			    lwb->lwb_state == LWB_STATE_ISSUED ||
			    lwb->lwb_state == LWB_STATE_WRITE_DONE ||
			    lwb->lwb_state == LWB_STATE_FLUSH_DONE);
			cv_wait(&zcw->zcw_cv, &zcw->zcw_lock);
		}
	}

	mutex_exit(&zcw->zcw_lock);
}

static zil_commit_waiter_t *
zil_alloc_commit_waiter(void)
{
	zil_commit_waiter_t *zcw = kmem_cache_alloc(zil_zcw_cache, KM_SLEEP);

	cv_init(&zcw->zcw_cv, NULL, CV_DEFAULT, NULL);
	mutex_init(&zcw->zcw_lock, NULL, MUTEX_DEFAULT, NULL);
	list_link_init(&zcw->zcw_node);
	zcw->zcw_lwb = NULL;
	zcw->zcw_done = B_FALSE;
	zcw->zcw_zio_error = 0;

	return (zcw);
}

static void
zil_free_commit_waiter(zil_commit_waiter_t *zcw)
{
	ASSERT(!list_link_active(&zcw->zcw_node));
	ASSERT3P(zcw->zcw_lwb, ==, NULL);
	ASSERT3B(zcw->zcw_done, ==, B_TRUE);
	mutex_destroy(&zcw->zcw_lock);
	cv_destroy(&zcw->zcw_cv);
	kmem_cache_free(zil_zcw_cache, zcw);
}

 
static void
zil_commit_itx_assign(zilog_t *zilog, zil_commit_waiter_t *zcw)
{
	dmu_tx_t *tx = dmu_tx_create(zilog->zl_os);

	 
	VERIFY0(dmu_tx_assign(tx, TXG_WAIT | TXG_NOTHROTTLE));

	itx_t *itx = zil_itx_create(TX_COMMIT, sizeof (lr_t));
	itx->itx_sync = B_TRUE;
	itx->itx_private = zcw;

	zil_itx_assign(zilog, itx, tx);

	dmu_tx_commit(tx);
}

 
void
zil_commit(zilog_t *zilog, uint64_t foid)
{
	 
	ASSERT3B(dmu_objset_is_snapshot(zilog->zl_os), ==, B_FALSE);

	if (zilog->zl_sync == ZFS_SYNC_DISABLED)
		return;

	if (!spa_writeable(zilog->zl_spa)) {
		 
		ASSERT(list_is_empty(&zilog->zl_lwb_list));
		ASSERT3P(zilog->zl_last_lwb_opened, ==, NULL);
		for (int i = 0; i < TXG_SIZE; i++)
			ASSERT3P(zilog->zl_itxg[i].itxg_itxs, ==, NULL);
		return;
	}

	 
	if (zilog->zl_suspend > 0) {
		txg_wait_synced(zilog->zl_dmu_pool, 0);
		return;
	}

	zil_commit_impl(zilog, foid);
}

void
zil_commit_impl(zilog_t *zilog, uint64_t foid)
{
	ZIL_STAT_BUMP(zilog, zil_commit_count);

	 
	zil_async_to_sync(zilog, foid);

	 
	zil_commit_waiter_t *zcw = zil_alloc_commit_waiter();
	zil_commit_itx_assign(zilog, zcw);

	uint64_t wtxg = zil_commit_writer(zilog, zcw);
	zil_commit_waiter(zilog, zcw);

	if (zcw->zcw_zio_error != 0) {
		 
		DTRACE_PROBE2(zil__commit__io__error,
		    zilog_t *, zilog, zil_commit_waiter_t *, zcw);
		txg_wait_synced(zilog->zl_dmu_pool, 0);
	} else if (wtxg != 0) {
		txg_wait_synced(zilog->zl_dmu_pool, wtxg);
	}

	zil_free_commit_waiter(zcw);
}

 
void
zil_sync(zilog_t *zilog, dmu_tx_t *tx)
{
	zil_header_t *zh = zil_header_in_syncing_context(zilog);
	uint64_t txg = dmu_tx_get_txg(tx);
	spa_t *spa = zilog->zl_spa;
	uint64_t *replayed_seq = &zilog->zl_replayed_seq[txg & TXG_MASK];
	lwb_t *lwb;

	 
	if (spa_sync_pass(spa) != 1)
		return;

	zil_lwb_flush_wait_all(zilog, txg);

	mutex_enter(&zilog->zl_lock);

	ASSERT(zilog->zl_stop_sync == 0);

	if (*replayed_seq != 0) {
		ASSERT(zh->zh_replay_seq < *replayed_seq);
		zh->zh_replay_seq = *replayed_seq;
		*replayed_seq = 0;
	}

	if (zilog->zl_destroy_txg == txg) {
		blkptr_t blk = zh->zh_log;
		dsl_dataset_t *ds = dmu_objset_ds(zilog->zl_os);

		ASSERT(list_is_empty(&zilog->zl_lwb_list));

		memset(zh, 0, sizeof (zil_header_t));
		memset(zilog->zl_replayed_seq, 0,
		    sizeof (zilog->zl_replayed_seq));

		if (zilog->zl_keep_first) {
			 
			zil_init_log_chain(zilog, &blk);
			zh->zh_log = blk;
		} else {
			 
			if (dsl_dataset_feature_is_active(ds,
			    SPA_FEATURE_ZILSAXATTR))
				dsl_dataset_deactivate_feature(ds,
				    SPA_FEATURE_ZILSAXATTR, tx);
		}
	}

	while ((lwb = list_head(&zilog->zl_lwb_list)) != NULL) {
		zh->zh_log = lwb->lwb_blk;
		if (lwb->lwb_state != LWB_STATE_FLUSH_DONE ||
		    lwb->lwb_alloc_txg > txg || lwb->lwb_max_txg > txg)
			break;
		list_remove(&zilog->zl_lwb_list, lwb);
		if (!BP_IS_HOLE(&lwb->lwb_blk))
			zio_free(spa, txg, &lwb->lwb_blk);
		zil_free_lwb(zilog, lwb);

		 
		if (list_is_empty(&zilog->zl_lwb_list))
			BP_ZERO(&zh->zh_log);
	}

	mutex_exit(&zilog->zl_lock);
}

static int
zil_lwb_cons(void *vbuf, void *unused, int kmflag)
{
	(void) unused, (void) kmflag;
	lwb_t *lwb = vbuf;
	list_create(&lwb->lwb_itxs, sizeof (itx_t), offsetof(itx_t, itx_node));
	list_create(&lwb->lwb_waiters, sizeof (zil_commit_waiter_t),
	    offsetof(zil_commit_waiter_t, zcw_node));
	avl_create(&lwb->lwb_vdev_tree, zil_lwb_vdev_compare,
	    sizeof (zil_vdev_node_t), offsetof(zil_vdev_node_t, zv_node));
	mutex_init(&lwb->lwb_vdev_lock, NULL, MUTEX_DEFAULT, NULL);
	return (0);
}

static void
zil_lwb_dest(void *vbuf, void *unused)
{
	(void) unused;
	lwb_t *lwb = vbuf;
	mutex_destroy(&lwb->lwb_vdev_lock);
	avl_destroy(&lwb->lwb_vdev_tree);
	list_destroy(&lwb->lwb_waiters);
	list_destroy(&lwb->lwb_itxs);
}

void
zil_init(void)
{
	zil_lwb_cache = kmem_cache_create("zil_lwb_cache",
	    sizeof (lwb_t), 0, zil_lwb_cons, zil_lwb_dest, NULL, NULL, NULL, 0);

	zil_zcw_cache = kmem_cache_create("zil_zcw_cache",
	    sizeof (zil_commit_waiter_t), 0, NULL, NULL, NULL, NULL, NULL, 0);

	zil_sums_init(&zil_sums_global);
	zil_kstats_global = kstat_create("zfs", 0, "zil", "misc",
	    KSTAT_TYPE_NAMED, sizeof (zil_stats) / sizeof (kstat_named_t),
	    KSTAT_FLAG_VIRTUAL);

	if (zil_kstats_global != NULL) {
		zil_kstats_global->ks_data = &zil_stats;
		zil_kstats_global->ks_update = zil_kstats_global_update;
		zil_kstats_global->ks_private = NULL;
		kstat_install(zil_kstats_global);
	}
}

void
zil_fini(void)
{
	kmem_cache_destroy(zil_zcw_cache);
	kmem_cache_destroy(zil_lwb_cache);

	if (zil_kstats_global != NULL) {
		kstat_delete(zil_kstats_global);
		zil_kstats_global = NULL;
	}

	zil_sums_fini(&zil_sums_global);
}

void
zil_set_sync(zilog_t *zilog, uint64_t sync)
{
	zilog->zl_sync = sync;
}

void
zil_set_logbias(zilog_t *zilog, uint64_t logbias)
{
	zilog->zl_logbias = logbias;
}

zilog_t *
zil_alloc(objset_t *os, zil_header_t *zh_phys)
{
	zilog_t *zilog;

	zilog = kmem_zalloc(sizeof (zilog_t), KM_SLEEP);

	zilog->zl_header = zh_phys;
	zilog->zl_os = os;
	zilog->zl_spa = dmu_objset_spa(os);
	zilog->zl_dmu_pool = dmu_objset_pool(os);
	zilog->zl_destroy_txg = TXG_INITIAL - 1;
	zilog->zl_logbias = dmu_objset_logbias(os);
	zilog->zl_sync = dmu_objset_syncprop(os);
	zilog->zl_dirty_max_txg = 0;
	zilog->zl_last_lwb_opened = NULL;
	zilog->zl_last_lwb_latency = 0;
	zilog->zl_max_block_size = zil_maxblocksize;

	mutex_init(&zilog->zl_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&zilog->zl_issuer_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&zilog->zl_lwb_io_lock, NULL, MUTEX_DEFAULT, NULL);

	for (int i = 0; i < TXG_SIZE; i++) {
		mutex_init(&zilog->zl_itxg[i].itxg_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}

	list_create(&zilog->zl_lwb_list, sizeof (lwb_t),
	    offsetof(lwb_t, lwb_node));

	list_create(&zilog->zl_itx_commit_list, sizeof (itx_t),
	    offsetof(itx_t, itx_node));

	cv_init(&zilog->zl_cv_suspend, NULL, CV_DEFAULT, NULL);
	cv_init(&zilog->zl_lwb_io_cv, NULL, CV_DEFAULT, NULL);

	return (zilog);
}

void
zil_free(zilog_t *zilog)
{
	int i;

	zilog->zl_stop_sync = 1;

	ASSERT0(zilog->zl_suspend);
	ASSERT0(zilog->zl_suspending);

	ASSERT(list_is_empty(&zilog->zl_lwb_list));
	list_destroy(&zilog->zl_lwb_list);

	ASSERT(list_is_empty(&zilog->zl_itx_commit_list));
	list_destroy(&zilog->zl_itx_commit_list);

	for (i = 0; i < TXG_SIZE; i++) {
		 
		if (zilog->zl_itxg[i].itxg_itxs)
			zil_itxg_clean(zilog->zl_itxg[i].itxg_itxs);
		mutex_destroy(&zilog->zl_itxg[i].itxg_lock);
	}

	mutex_destroy(&zilog->zl_issuer_lock);
	mutex_destroy(&zilog->zl_lock);
	mutex_destroy(&zilog->zl_lwb_io_lock);

	cv_destroy(&zilog->zl_cv_suspend);
	cv_destroy(&zilog->zl_lwb_io_cv);

	kmem_free(zilog, sizeof (zilog_t));
}

 
zilog_t *
zil_open(objset_t *os, zil_get_data_t *get_data, zil_sums_t *zil_sums)
{
	zilog_t *zilog = dmu_objset_zil(os);

	ASSERT3P(zilog->zl_get_data, ==, NULL);
	ASSERT3P(zilog->zl_last_lwb_opened, ==, NULL);
	ASSERT(list_is_empty(&zilog->zl_lwb_list));

	zilog->zl_get_data = get_data;
	zilog->zl_sums = zil_sums;

	return (zilog);
}

 
void
zil_close(zilog_t *zilog)
{
	lwb_t *lwb;
	uint64_t txg;

	if (!dmu_objset_is_snapshot(zilog->zl_os)) {
		zil_commit(zilog, 0);
	} else {
		ASSERT(list_is_empty(&zilog->zl_lwb_list));
		ASSERT0(zilog->zl_dirty_max_txg);
		ASSERT3B(zilog_is_dirty(zilog), ==, B_FALSE);
	}

	mutex_enter(&zilog->zl_lock);
	txg = zilog->zl_dirty_max_txg;
	lwb = list_tail(&zilog->zl_lwb_list);
	if (lwb != NULL) {
		txg = MAX(txg, lwb->lwb_alloc_txg);
		txg = MAX(txg, lwb->lwb_max_txg);
	}
	mutex_exit(&zilog->zl_lock);

	 
	mutex_enter(&zilog->zl_lwb_io_lock);
	txg = MAX(zilog->zl_lwb_max_issued_txg, txg);
	mutex_exit(&zilog->zl_lwb_io_lock);

	 
	if (txg != 0)
		txg_wait_synced(zilog->zl_dmu_pool, txg);

	if (zilog_is_dirty(zilog))
		zfs_dbgmsg("zil (%px) is dirty, txg %llu", zilog,
		    (u_longlong_t)txg);
	if (txg < spa_freeze_txg(zilog->zl_spa))
		VERIFY(!zilog_is_dirty(zilog));

	zilog->zl_get_data = NULL;

	 
	mutex_enter(&zilog->zl_lock);
	lwb = list_remove_head(&zilog->zl_lwb_list);
	if (lwb != NULL) {
		ASSERT(list_is_empty(&zilog->zl_lwb_list));
		ASSERT3S(lwb->lwb_state, ==, LWB_STATE_NEW);
		zio_buf_free(lwb->lwb_buf, lwb->lwb_sz);
		zil_free_lwb(zilog, lwb);
	}
	mutex_exit(&zilog->zl_lock);
}

static const char *suspend_tag = "zil suspending";

 
int
zil_suspend(const char *osname, void **cookiep)
{
	objset_t *os;
	zilog_t *zilog;
	const zil_header_t *zh;
	int error;

	error = dmu_objset_hold(osname, suspend_tag, &os);
	if (error != 0)
		return (error);
	zilog = dmu_objset_zil(os);

	mutex_enter(&zilog->zl_lock);
	zh = zilog->zl_header;

	if (zh->zh_flags & ZIL_REPLAY_NEEDED) {		 
		mutex_exit(&zilog->zl_lock);
		dmu_objset_rele(os, suspend_tag);
		return (SET_ERROR(EBUSY));
	}

	 
	if (cookiep == NULL && !zilog->zl_suspending &&
	    (zilog->zl_suspend > 0 || BP_IS_HOLE(&zh->zh_log))) {
		mutex_exit(&zilog->zl_lock);
		dmu_objset_rele(os, suspend_tag);
		return (0);
	}

	dsl_dataset_long_hold(dmu_objset_ds(os), suspend_tag);
	dsl_pool_rele(dmu_objset_pool(os), suspend_tag);

	zilog->zl_suspend++;

	if (zilog->zl_suspend > 1) {
		 

		while (zilog->zl_suspending)
			cv_wait(&zilog->zl_cv_suspend, &zilog->zl_lock);
		mutex_exit(&zilog->zl_lock);

		if (cookiep == NULL)
			zil_resume(os);
		else
			*cookiep = os;
		return (0);
	}

	 
	if (BP_IS_HOLE(&zh->zh_log)) {
		ASSERT(cookiep != NULL);  

		*cookiep = os;
		mutex_exit(&zilog->zl_lock);
		return (0);
	}

	 
	if (os->os_encrypted &&
	    dsl_dataset_create_key_mapping(dmu_objset_ds(os)) != 0) {
		zilog->zl_suspend--;
		mutex_exit(&zilog->zl_lock);
		dsl_dataset_long_rele(dmu_objset_ds(os), suspend_tag);
		dsl_dataset_rele(dmu_objset_ds(os), suspend_tag);
		return (SET_ERROR(EACCES));
	}

	zilog->zl_suspending = B_TRUE;
	mutex_exit(&zilog->zl_lock);

	 
	zil_commit_impl(zilog, 0);

	 
	txg_wait_synced(zilog->zl_dmu_pool, 0);

	zil_destroy(zilog, B_FALSE);

	mutex_enter(&zilog->zl_lock);
	zilog->zl_suspending = B_FALSE;
	cv_broadcast(&zilog->zl_cv_suspend);
	mutex_exit(&zilog->zl_lock);

	if (os->os_encrypted)
		dsl_dataset_remove_key_mapping(dmu_objset_ds(os));

	if (cookiep == NULL)
		zil_resume(os);
	else
		*cookiep = os;
	return (0);
}

void
zil_resume(void *cookie)
{
	objset_t *os = cookie;
	zilog_t *zilog = dmu_objset_zil(os);

	mutex_enter(&zilog->zl_lock);
	ASSERT(zilog->zl_suspend != 0);
	zilog->zl_suspend--;
	mutex_exit(&zilog->zl_lock);
	dsl_dataset_long_rele(dmu_objset_ds(os), suspend_tag);
	dsl_dataset_rele(dmu_objset_ds(os), suspend_tag);
}

typedef struct zil_replay_arg {
	zil_replay_func_t *const *zr_replay;
	void		*zr_arg;
	boolean_t	zr_byteswap;
	char		*zr_lr;
} zil_replay_arg_t;

static int
zil_replay_error(zilog_t *zilog, const lr_t *lr, int error)
{
	char name[ZFS_MAX_DATASET_NAME_LEN];

	zilog->zl_replaying_seq--;	 

	dmu_objset_name(zilog->zl_os, name);

	cmn_err(CE_WARN, "ZFS replay transaction error %d, "
	    "dataset %s, seq 0x%llx, txtype %llu %s\n", error, name,
	    (u_longlong_t)lr->lrc_seq,
	    (u_longlong_t)(lr->lrc_txtype & ~TX_CI),
	    (lr->lrc_txtype & TX_CI) ? "CI" : "");

	return (error);
}

static int
zil_replay_log_record(zilog_t *zilog, const lr_t *lr, void *zra,
    uint64_t claim_txg)
{
	zil_replay_arg_t *zr = zra;
	const zil_header_t *zh = zilog->zl_header;
	uint64_t reclen = lr->lrc_reclen;
	uint64_t txtype = lr->lrc_txtype;
	int error = 0;

	zilog->zl_replaying_seq = lr->lrc_seq;

	if (lr->lrc_seq <= zh->zh_replay_seq)	 
		return (0);

	if (lr->lrc_txg < claim_txg)		 
		return (0);

	 
	txtype &= ~TX_CI;

	if (txtype == 0 || txtype >= TX_MAX_TYPE)
		return (zil_replay_error(zilog, lr, EINVAL));

	 
	if (TX_OOO(txtype)) {
		error = dmu_object_info(zilog->zl_os,
		    LR_FOID_GET_OBJ(((lr_ooo_t *)lr)->lr_foid), NULL);
		if (error == ENOENT || error == EEXIST)
			return (0);
	}

	 
	memcpy(zr->zr_lr, lr, reclen);

	 
	if (txtype == TX_WRITE && reclen == sizeof (lr_write_t)) {
		error = zil_read_log_data(zilog, (lr_write_t *)lr,
		    zr->zr_lr + reclen);
		if (error != 0)
			return (zil_replay_error(zilog, lr, error));
	}

	 
	if (zr->zr_byteswap)
		byteswap_uint64_array(zr->zr_lr, reclen);

	 
	error = zr->zr_replay[txtype](zr->zr_arg, zr->zr_lr, zr->zr_byteswap);
	if (error != 0) {
		 
		txg_wait_synced(spa_get_dsl(zilog->zl_spa), 0);
		error = zr->zr_replay[txtype](zr->zr_arg, zr->zr_lr, B_FALSE);
		if (error != 0)
			return (zil_replay_error(zilog, lr, error));
	}
	return (0);
}

static int
zil_incr_blks(zilog_t *zilog, const blkptr_t *bp, void *arg, uint64_t claim_txg)
{
	(void) bp, (void) arg, (void) claim_txg;

	zilog->zl_replay_blks++;

	return (0);
}

 
boolean_t
zil_replay(objset_t *os, void *arg,
    zil_replay_func_t *const replay_func[TX_MAX_TYPE])
{
	zilog_t *zilog = dmu_objset_zil(os);
	const zil_header_t *zh = zilog->zl_header;
	zil_replay_arg_t zr;

	if ((zh->zh_flags & ZIL_REPLAY_NEEDED) == 0) {
		return (zil_destroy(zilog, B_TRUE));
	}

	zr.zr_replay = replay_func;
	zr.zr_arg = arg;
	zr.zr_byteswap = BP_SHOULD_BYTESWAP(&zh->zh_log);
	zr.zr_lr = vmem_alloc(2 * SPA_MAXBLOCKSIZE, KM_SLEEP);

	 
	txg_wait_synced(zilog->zl_dmu_pool, 0);

	zilog->zl_replay = B_TRUE;
	zilog->zl_replay_time = ddi_get_lbolt();
	ASSERT(zilog->zl_replay_blks == 0);
	(void) zil_parse(zilog, zil_incr_blks, zil_replay_log_record, &zr,
	    zh->zh_claim_txg, B_TRUE);
	vmem_free(zr.zr_lr, 2 * SPA_MAXBLOCKSIZE);

	zil_destroy(zilog, B_FALSE);
	txg_wait_synced(zilog->zl_dmu_pool, zilog->zl_destroy_txg);
	zilog->zl_replay = B_FALSE;

	return (B_TRUE);
}

boolean_t
zil_replaying(zilog_t *zilog, dmu_tx_t *tx)
{
	if (zilog->zl_sync == ZFS_SYNC_DISABLED)
		return (B_TRUE);

	if (zilog->zl_replay) {
		dsl_dataset_dirty(dmu_objset_ds(zilog->zl_os), tx);
		zilog->zl_replayed_seq[dmu_tx_get_txg(tx) & TXG_MASK] =
		    zilog->zl_replaying_seq;
		return (B_TRUE);
	}

	return (B_FALSE);
}

int
zil_reset(const char *osname, void *arg)
{
	(void) arg;

	int error = zil_suspend(osname, NULL);
	 
	if ((error == EACCES) || (error == EBUSY))
		return (SET_ERROR(error));
	if (error != 0)
		return (SET_ERROR(EEXIST));
	return (0);
}

EXPORT_SYMBOL(zil_alloc);
EXPORT_SYMBOL(zil_free);
EXPORT_SYMBOL(zil_open);
EXPORT_SYMBOL(zil_close);
EXPORT_SYMBOL(zil_replay);
EXPORT_SYMBOL(zil_replaying);
EXPORT_SYMBOL(zil_destroy);
EXPORT_SYMBOL(zil_destroy_sync);
EXPORT_SYMBOL(zil_itx_create);
EXPORT_SYMBOL(zil_itx_destroy);
EXPORT_SYMBOL(zil_itx_assign);
EXPORT_SYMBOL(zil_commit);
EXPORT_SYMBOL(zil_claim);
EXPORT_SYMBOL(zil_check_log_chain);
EXPORT_SYMBOL(zil_sync);
EXPORT_SYMBOL(zil_clean);
EXPORT_SYMBOL(zil_suspend);
EXPORT_SYMBOL(zil_resume);
EXPORT_SYMBOL(zil_lwb_add_block);
EXPORT_SYMBOL(zil_bp_tree_add);
EXPORT_SYMBOL(zil_set_sync);
EXPORT_SYMBOL(zil_set_logbias);
EXPORT_SYMBOL(zil_sums_init);
EXPORT_SYMBOL(zil_sums_fini);
EXPORT_SYMBOL(zil_kstat_values_update);

ZFS_MODULE_PARAM(zfs, zfs_, commit_timeout_pct, UINT, ZMOD_RW,
	"ZIL block open timeout percentage");

ZFS_MODULE_PARAM(zfs_zil, zil_, min_commit_timeout, U64, ZMOD_RW,
	"Minimum delay we care for ZIL block commit");

ZFS_MODULE_PARAM(zfs_zil, zil_, replay_disable, INT, ZMOD_RW,
	"Disable intent logging replay");

ZFS_MODULE_PARAM(zfs_zil, zil_, nocacheflush, INT, ZMOD_RW,
	"Disable ZIL cache flushes");

ZFS_MODULE_PARAM(zfs_zil, zil_, slog_bulk, U64, ZMOD_RW,
	"Limit in bytes slog sync writes per commit");

ZFS_MODULE_PARAM(zfs_zil, zil_, maxblocksize, UINT, ZMOD_RW,
	"Limit in bytes of ZIL log block size");

ZFS_MODULE_PARAM(zfs_zil, zil_, maxcopied, UINT, ZMOD_RW,
	"Limit in bytes WR_COPIED size");
