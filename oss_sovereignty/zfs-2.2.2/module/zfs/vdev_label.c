 

 

 

#include <sys/zfs_context.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/dmu.h>
#include <sys/zap.h>
#include <sys/vdev.h>
#include <sys/vdev_impl.h>
#include <sys/vdev_draid.h>
#include <sys/uberblock_impl.h>
#include <sys/metaslab.h>
#include <sys/metaslab_impl.h>
#include <sys/zio.h>
#include <sys/dsl_scan.h>
#include <sys/abd.h>
#include <sys/fs/zfs.h>
#include <sys/byteorder.h>
#include <sys/zfs_bootenv.h>

 
uint64_t
vdev_label_offset(uint64_t psize, int l, uint64_t offset)
{
	ASSERT(offset < sizeof (vdev_label_t));
	ASSERT(P2PHASE_TYPED(psize, sizeof (vdev_label_t), uint64_t) == 0);

	return (offset + l * sizeof (vdev_label_t) + (l < VDEV_LABELS / 2 ?
	    0 : psize - VDEV_LABELS * sizeof (vdev_label_t)));
}

 
int
vdev_label_number(uint64_t psize, uint64_t offset)
{
	int l;

	if (offset >= psize - VDEV_LABEL_END_SIZE) {
		offset -= psize - VDEV_LABEL_END_SIZE;
		offset += (VDEV_LABELS / 2) * sizeof (vdev_label_t);
	}
	l = offset / sizeof (vdev_label_t);
	return (l < VDEV_LABELS ? l : -1);
}

static void
vdev_label_read(zio_t *zio, vdev_t *vd, int l, abd_t *buf, uint64_t offset,
    uint64_t size, zio_done_func_t *done, void *private, int flags)
{
	ASSERT(
	    spa_config_held(zio->io_spa, SCL_STATE, RW_READER) == SCL_STATE ||
	    spa_config_held(zio->io_spa, SCL_STATE, RW_WRITER) == SCL_STATE);
	ASSERT(flags & ZIO_FLAG_CONFIG_WRITER);

	zio_nowait(zio_read_phys(zio, vd,
	    vdev_label_offset(vd->vdev_psize, l, offset),
	    size, buf, ZIO_CHECKSUM_LABEL, done, private,
	    ZIO_PRIORITY_SYNC_READ, flags, B_TRUE));
}

void
vdev_label_write(zio_t *zio, vdev_t *vd, int l, abd_t *buf, uint64_t offset,
    uint64_t size, zio_done_func_t *done, void *private, int flags)
{
	ASSERT(
	    spa_config_held(zio->io_spa, SCL_STATE, RW_READER) == SCL_STATE ||
	    spa_config_held(zio->io_spa, SCL_STATE, RW_WRITER) == SCL_STATE);
	ASSERT(flags & ZIO_FLAG_CONFIG_WRITER);

	zio_nowait(zio_write_phys(zio, vd,
	    vdev_label_offset(vd->vdev_psize, l, offset),
	    size, buf, ZIO_CHECKSUM_LABEL, done, private,
	    ZIO_PRIORITY_SYNC_WRITE, flags, B_TRUE));
}

 
void
vdev_config_generate_stats(vdev_t *vd, nvlist_t *nv)
{
	nvlist_t *nvx;
	vdev_stat_t *vs;
	vdev_stat_ex_t *vsx;

	vs = kmem_alloc(sizeof (*vs), KM_SLEEP);
	vsx = kmem_alloc(sizeof (*vsx), KM_SLEEP);

	vdev_get_stats_ex(vd, vs, vsx);
	fnvlist_add_uint64_array(nv, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t *)vs, sizeof (*vs) / sizeof (uint64_t));

	 
	nvx = fnvlist_alloc();

	 
	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_SYNC_R_ACTIVE_QUEUE,
	    vsx->vsx_active_queue[ZIO_PRIORITY_SYNC_READ]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_SYNC_W_ACTIVE_QUEUE,
	    vsx->vsx_active_queue[ZIO_PRIORITY_SYNC_WRITE]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_ASYNC_R_ACTIVE_QUEUE,
	    vsx->vsx_active_queue[ZIO_PRIORITY_ASYNC_READ]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_ASYNC_W_ACTIVE_QUEUE,
	    vsx->vsx_active_queue[ZIO_PRIORITY_ASYNC_WRITE]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_SCRUB_ACTIVE_QUEUE,
	    vsx->vsx_active_queue[ZIO_PRIORITY_SCRUB]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_TRIM_ACTIVE_QUEUE,
	    vsx->vsx_active_queue[ZIO_PRIORITY_TRIM]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_REBUILD_ACTIVE_QUEUE,
	    vsx->vsx_active_queue[ZIO_PRIORITY_REBUILD]);

	 
	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_SYNC_R_PEND_QUEUE,
	    vsx->vsx_pend_queue[ZIO_PRIORITY_SYNC_READ]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_SYNC_W_PEND_QUEUE,
	    vsx->vsx_pend_queue[ZIO_PRIORITY_SYNC_WRITE]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_ASYNC_R_PEND_QUEUE,
	    vsx->vsx_pend_queue[ZIO_PRIORITY_ASYNC_READ]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_ASYNC_W_PEND_QUEUE,
	    vsx->vsx_pend_queue[ZIO_PRIORITY_ASYNC_WRITE]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_SCRUB_PEND_QUEUE,
	    vsx->vsx_pend_queue[ZIO_PRIORITY_SCRUB]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_TRIM_PEND_QUEUE,
	    vsx->vsx_pend_queue[ZIO_PRIORITY_TRIM]);

	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_REBUILD_PEND_QUEUE,
	    vsx->vsx_pend_queue[ZIO_PRIORITY_REBUILD]);

	 
	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_TOT_R_LAT_HISTO,
	    vsx->vsx_total_histo[ZIO_TYPE_READ],
	    ARRAY_SIZE(vsx->vsx_total_histo[ZIO_TYPE_READ]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_TOT_W_LAT_HISTO,
	    vsx->vsx_total_histo[ZIO_TYPE_WRITE],
	    ARRAY_SIZE(vsx->vsx_total_histo[ZIO_TYPE_WRITE]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_DISK_R_LAT_HISTO,
	    vsx->vsx_disk_histo[ZIO_TYPE_READ],
	    ARRAY_SIZE(vsx->vsx_disk_histo[ZIO_TYPE_READ]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_DISK_W_LAT_HISTO,
	    vsx->vsx_disk_histo[ZIO_TYPE_WRITE],
	    ARRAY_SIZE(vsx->vsx_disk_histo[ZIO_TYPE_WRITE]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_SYNC_R_LAT_HISTO,
	    vsx->vsx_queue_histo[ZIO_PRIORITY_SYNC_READ],
	    ARRAY_SIZE(vsx->vsx_queue_histo[ZIO_PRIORITY_SYNC_READ]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_SYNC_W_LAT_HISTO,
	    vsx->vsx_queue_histo[ZIO_PRIORITY_SYNC_WRITE],
	    ARRAY_SIZE(vsx->vsx_queue_histo[ZIO_PRIORITY_SYNC_WRITE]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_ASYNC_R_LAT_HISTO,
	    vsx->vsx_queue_histo[ZIO_PRIORITY_ASYNC_READ],
	    ARRAY_SIZE(vsx->vsx_queue_histo[ZIO_PRIORITY_ASYNC_READ]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_ASYNC_W_LAT_HISTO,
	    vsx->vsx_queue_histo[ZIO_PRIORITY_ASYNC_WRITE],
	    ARRAY_SIZE(vsx->vsx_queue_histo[ZIO_PRIORITY_ASYNC_WRITE]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_SCRUB_LAT_HISTO,
	    vsx->vsx_queue_histo[ZIO_PRIORITY_SCRUB],
	    ARRAY_SIZE(vsx->vsx_queue_histo[ZIO_PRIORITY_SCRUB]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_TRIM_LAT_HISTO,
	    vsx->vsx_queue_histo[ZIO_PRIORITY_TRIM],
	    ARRAY_SIZE(vsx->vsx_queue_histo[ZIO_PRIORITY_TRIM]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_REBUILD_LAT_HISTO,
	    vsx->vsx_queue_histo[ZIO_PRIORITY_REBUILD],
	    ARRAY_SIZE(vsx->vsx_queue_histo[ZIO_PRIORITY_REBUILD]));

	 
	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_SYNC_IND_R_HISTO,
	    vsx->vsx_ind_histo[ZIO_PRIORITY_SYNC_READ],
	    ARRAY_SIZE(vsx->vsx_ind_histo[ZIO_PRIORITY_SYNC_READ]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_SYNC_IND_W_HISTO,
	    vsx->vsx_ind_histo[ZIO_PRIORITY_SYNC_WRITE],
	    ARRAY_SIZE(vsx->vsx_ind_histo[ZIO_PRIORITY_SYNC_WRITE]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_ASYNC_IND_R_HISTO,
	    vsx->vsx_ind_histo[ZIO_PRIORITY_ASYNC_READ],
	    ARRAY_SIZE(vsx->vsx_ind_histo[ZIO_PRIORITY_ASYNC_READ]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_ASYNC_IND_W_HISTO,
	    vsx->vsx_ind_histo[ZIO_PRIORITY_ASYNC_WRITE],
	    ARRAY_SIZE(vsx->vsx_ind_histo[ZIO_PRIORITY_ASYNC_WRITE]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_IND_SCRUB_HISTO,
	    vsx->vsx_ind_histo[ZIO_PRIORITY_SCRUB],
	    ARRAY_SIZE(vsx->vsx_ind_histo[ZIO_PRIORITY_SCRUB]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_IND_TRIM_HISTO,
	    vsx->vsx_ind_histo[ZIO_PRIORITY_TRIM],
	    ARRAY_SIZE(vsx->vsx_ind_histo[ZIO_PRIORITY_TRIM]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_IND_REBUILD_HISTO,
	    vsx->vsx_ind_histo[ZIO_PRIORITY_REBUILD],
	    ARRAY_SIZE(vsx->vsx_ind_histo[ZIO_PRIORITY_REBUILD]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_SYNC_AGG_R_HISTO,
	    vsx->vsx_agg_histo[ZIO_PRIORITY_SYNC_READ],
	    ARRAY_SIZE(vsx->vsx_agg_histo[ZIO_PRIORITY_SYNC_READ]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_SYNC_AGG_W_HISTO,
	    vsx->vsx_agg_histo[ZIO_PRIORITY_SYNC_WRITE],
	    ARRAY_SIZE(vsx->vsx_agg_histo[ZIO_PRIORITY_SYNC_WRITE]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_ASYNC_AGG_R_HISTO,
	    vsx->vsx_agg_histo[ZIO_PRIORITY_ASYNC_READ],
	    ARRAY_SIZE(vsx->vsx_agg_histo[ZIO_PRIORITY_ASYNC_READ]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_ASYNC_AGG_W_HISTO,
	    vsx->vsx_agg_histo[ZIO_PRIORITY_ASYNC_WRITE],
	    ARRAY_SIZE(vsx->vsx_agg_histo[ZIO_PRIORITY_ASYNC_WRITE]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_AGG_SCRUB_HISTO,
	    vsx->vsx_agg_histo[ZIO_PRIORITY_SCRUB],
	    ARRAY_SIZE(vsx->vsx_agg_histo[ZIO_PRIORITY_SCRUB]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_AGG_TRIM_HISTO,
	    vsx->vsx_agg_histo[ZIO_PRIORITY_TRIM],
	    ARRAY_SIZE(vsx->vsx_agg_histo[ZIO_PRIORITY_TRIM]));

	fnvlist_add_uint64_array(nvx, ZPOOL_CONFIG_VDEV_AGG_REBUILD_HISTO,
	    vsx->vsx_agg_histo[ZIO_PRIORITY_REBUILD],
	    ARRAY_SIZE(vsx->vsx_agg_histo[ZIO_PRIORITY_REBUILD]));

	 
	fnvlist_add_uint64(nvx, ZPOOL_CONFIG_VDEV_SLOW_IOS, vs->vs_slow_ios);

	 
	fnvlist_add_nvlist(nv, ZPOOL_CONFIG_VDEV_STATS_EX, nvx);

	fnvlist_free(nvx);
	kmem_free(vs, sizeof (*vs));
	kmem_free(vsx, sizeof (*vsx));
}

static void
root_vdev_actions_getprogress(vdev_t *vd, nvlist_t *nvl)
{
	spa_t *spa = vd->vdev_spa;

	if (vd != spa->spa_root_vdev)
		return;

	 
	pool_scan_stat_t ps;
	if (spa_scan_get_stats(spa, &ps) == 0) {
		fnvlist_add_uint64_array(nvl,
		    ZPOOL_CONFIG_SCAN_STATS, (uint64_t *)&ps,
		    sizeof (pool_scan_stat_t) / sizeof (uint64_t));
	}

	pool_removal_stat_t prs;
	if (spa_removal_get_stats(spa, &prs) == 0) {
		fnvlist_add_uint64_array(nvl,
		    ZPOOL_CONFIG_REMOVAL_STATS, (uint64_t *)&prs,
		    sizeof (prs) / sizeof (uint64_t));
	}

	pool_checkpoint_stat_t pcs;
	if (spa_checkpoint_get_stats(spa, &pcs) == 0) {
		fnvlist_add_uint64_array(nvl,
		    ZPOOL_CONFIG_CHECKPOINT_STATS, (uint64_t *)&pcs,
		    sizeof (pcs) / sizeof (uint64_t));
	}
}

static void
top_vdev_actions_getprogress(vdev_t *vd, nvlist_t *nvl)
{
	if (vd == vd->vdev_top) {
		vdev_rebuild_stat_t vrs;
		if (vdev_rebuild_get_stats(vd, &vrs) == 0) {
			fnvlist_add_uint64_array(nvl,
			    ZPOOL_CONFIG_REBUILD_STATS, (uint64_t *)&vrs,
			    sizeof (vrs) / sizeof (uint64_t));
		}
	}
}

 
nvlist_t *
vdev_config_generate(spa_t *spa, vdev_t *vd, boolean_t getstats,
    vdev_config_flag_t flags)
{
	nvlist_t *nv = NULL;
	vdev_indirect_config_t *vic = &vd->vdev_indirect_config;

	nv = fnvlist_alloc();

	fnvlist_add_string(nv, ZPOOL_CONFIG_TYPE, vd->vdev_ops->vdev_op_type);
	if (!(flags & (VDEV_CONFIG_SPARE | VDEV_CONFIG_L2CACHE)))
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_ID, vd->vdev_id);
	fnvlist_add_uint64(nv, ZPOOL_CONFIG_GUID, vd->vdev_guid);

	if (vd->vdev_path != NULL)
		fnvlist_add_string(nv, ZPOOL_CONFIG_PATH, vd->vdev_path);

	if (vd->vdev_devid != NULL)
		fnvlist_add_string(nv, ZPOOL_CONFIG_DEVID, vd->vdev_devid);

	if (vd->vdev_physpath != NULL)
		fnvlist_add_string(nv, ZPOOL_CONFIG_PHYS_PATH,
		    vd->vdev_physpath);

	if (vd->vdev_enc_sysfs_path != NULL)
		fnvlist_add_string(nv, ZPOOL_CONFIG_VDEV_ENC_SYSFS_PATH,
		    vd->vdev_enc_sysfs_path);

	if (vd->vdev_fru != NULL)
		fnvlist_add_string(nv, ZPOOL_CONFIG_FRU, vd->vdev_fru);

	if (vd->vdev_ops->vdev_op_config_generate != NULL)
		vd->vdev_ops->vdev_op_config_generate(vd, nv);

	if (vd->vdev_wholedisk != -1ULL) {
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_WHOLE_DISK,
		    vd->vdev_wholedisk);
	}

	if (vd->vdev_not_present && !(flags & VDEV_CONFIG_MISSING))
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_NOT_PRESENT, 1);

	if (vd->vdev_isspare)
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_IS_SPARE, 1);

	if (flags & VDEV_CONFIG_L2CACHE)
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_ASHIFT, vd->vdev_ashift);

	if (!(flags & (VDEV_CONFIG_SPARE | VDEV_CONFIG_L2CACHE)) &&
	    vd == vd->vdev_top) {
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_METASLAB_ARRAY,
		    vd->vdev_ms_array);
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_METASLAB_SHIFT,
		    vd->vdev_ms_shift);
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_ASHIFT, vd->vdev_ashift);
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_ASIZE,
		    vd->vdev_asize);
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_IS_LOG, vd->vdev_islog);
		if (vd->vdev_noalloc) {
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_NONALLOCATING,
			    vd->vdev_noalloc);
		}

		 
		if (vd->vdev_removing && !vd->vdev_islog) {
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_REMOVING,
			    vd->vdev_removing);
		}

		 
		if (getstats && vd->vdev_alloc_bias != VDEV_BIAS_NONE) {
			const char *bias = NULL;

			switch (vd->vdev_alloc_bias) {
			case VDEV_BIAS_LOG:
				bias = VDEV_ALLOC_BIAS_LOG;
				break;
			case VDEV_BIAS_SPECIAL:
				bias = VDEV_ALLOC_BIAS_SPECIAL;
				break;
			case VDEV_BIAS_DEDUP:
				bias = VDEV_ALLOC_BIAS_DEDUP;
				break;
			default:
				ASSERT3U(vd->vdev_alloc_bias, ==,
				    VDEV_BIAS_NONE);
			}
			fnvlist_add_string(nv, ZPOOL_CONFIG_ALLOCATION_BIAS,
			    bias);
		}
	}

	if (vd->vdev_dtl_sm != NULL) {
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_DTL,
		    space_map_object(vd->vdev_dtl_sm));
	}

	if (vic->vic_mapping_object != 0) {
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_INDIRECT_OBJECT,
		    vic->vic_mapping_object);
	}

	if (vic->vic_births_object != 0) {
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_INDIRECT_BIRTHS,
		    vic->vic_births_object);
	}

	if (vic->vic_prev_indirect_vdev != UINT64_MAX) {
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_PREV_INDIRECT_VDEV,
		    vic->vic_prev_indirect_vdev);
	}

	if (vd->vdev_crtxg)
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_CREATE_TXG, vd->vdev_crtxg);

	if (vd->vdev_expansion_time)
		fnvlist_add_uint64(nv, ZPOOL_CONFIG_EXPANSION_TIME,
		    vd->vdev_expansion_time);

	if (flags & VDEV_CONFIG_MOS) {
		if (vd->vdev_leaf_zap != 0) {
			ASSERT(vd->vdev_ops->vdev_op_leaf);
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_VDEV_LEAF_ZAP,
			    vd->vdev_leaf_zap);
		}

		if (vd->vdev_top_zap != 0) {
			ASSERT(vd == vd->vdev_top);
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_VDEV_TOP_ZAP,
			    vd->vdev_top_zap);
		}

		if (vd->vdev_ops == &vdev_root_ops && vd->vdev_root_zap != 0 &&
		    spa_feature_is_active(vd->vdev_spa, SPA_FEATURE_AVZ_V2)) {
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_VDEV_ROOT_ZAP,
			    vd->vdev_root_zap);
		}

		if (vd->vdev_resilver_deferred) {
			ASSERT(vd->vdev_ops->vdev_op_leaf);
			ASSERT(spa->spa_resilver_deferred);
			fnvlist_add_boolean(nv, ZPOOL_CONFIG_RESILVER_DEFER);
		}
	}

	if (getstats) {
		vdev_config_generate_stats(vd, nv);

		root_vdev_actions_getprogress(vd, nv);
		top_vdev_actions_getprogress(vd, nv);

		 
		rw_enter(&vd->vdev_indirect_rwlock, RW_READER);
		if (vd->vdev_indirect_mapping != NULL) {
			ASSERT(vd->vdev_indirect_births != NULL);
			vdev_indirect_mapping_t *vim =
			    vd->vdev_indirect_mapping;
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_INDIRECT_SIZE,
			    vdev_indirect_mapping_size(vim));
		}
		rw_exit(&vd->vdev_indirect_rwlock);
		if (vd->vdev_mg != NULL &&
		    vd->vdev_mg->mg_fragmentation != ZFS_FRAG_INVALID) {
			 
			uint64_t seg_count = 0;
			uint64_t to_alloc = vd->vdev_stat.vs_alloc;

			 
			for (int i = 0; i < RANGE_TREE_HISTOGRAM_SIZE; i++) {
				if (i + 1 < highbit64(vdev_removal_max_span)
				    - 1) {
					to_alloc +=
					    vd->vdev_mg->mg_histogram[i] <<
					    (i + 1);
				} else {
					seg_count +=
					    vd->vdev_mg->mg_histogram[i];
				}
			}

			 
			seg_count += to_alloc / spa_remove_max_segment(spa);

			fnvlist_add_uint64(nv, ZPOOL_CONFIG_INDIRECT_SIZE,
			    seg_count *
			    sizeof (vdev_indirect_mapping_entry_phys_t));
		}
	}

	if (!vd->vdev_ops->vdev_op_leaf) {
		nvlist_t **child;
		uint64_t c;

		ASSERT(!vd->vdev_ishole);

		child = kmem_alloc(vd->vdev_children * sizeof (nvlist_t *),
		    KM_SLEEP);

		for (c = 0; c < vd->vdev_children; c++) {
			child[c] = vdev_config_generate(spa, vd->vdev_child[c],
			    getstats, flags);
		}

		fnvlist_add_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
		    (const nvlist_t * const *)child, vd->vdev_children);

		for (c = 0; c < vd->vdev_children; c++)
			nvlist_free(child[c]);

		kmem_free(child, vd->vdev_children * sizeof (nvlist_t *));

	} else {
		const char *aux = NULL;

		if (vd->vdev_offline && !vd->vdev_tmpoffline)
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_OFFLINE, B_TRUE);
		if (vd->vdev_resilver_txg != 0)
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_RESILVER_TXG,
			    vd->vdev_resilver_txg);
		if (vd->vdev_rebuild_txg != 0)
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_REBUILD_TXG,
			    vd->vdev_rebuild_txg);
		if (vd->vdev_faulted)
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_FAULTED, B_TRUE);
		if (vd->vdev_degraded)
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_DEGRADED, B_TRUE);
		if (vd->vdev_removed)
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_REMOVED, B_TRUE);
		if (vd->vdev_unspare)
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_UNSPARE, B_TRUE);
		if (vd->vdev_ishole)
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_IS_HOLE, B_TRUE);

		 
		switch (vd->vdev_stat.vs_aux) {
		case VDEV_AUX_ERR_EXCEEDED:
			aux = "err_exceeded";
			break;

		case VDEV_AUX_EXTERNAL:
			aux = "external";
			break;
		}

		if (aux != NULL && !vd->vdev_tmpoffline) {
			fnvlist_add_string(nv, ZPOOL_CONFIG_AUX_STATE, aux);
		} else {
			 
			if (nvlist_exists(nv, ZPOOL_CONFIG_AUX_STATE))
				nvlist_remove_all(nv, ZPOOL_CONFIG_AUX_STATE);
		}

		if (vd->vdev_splitting && vd->vdev_orig_guid != 0LL) {
			fnvlist_add_uint64(nv, ZPOOL_CONFIG_ORIG_GUID,
			    vd->vdev_orig_guid);
		}
	}

	return (nv);
}

 
void
vdev_top_config_generate(spa_t *spa, nvlist_t *config)
{
	vdev_t *rvd = spa->spa_root_vdev;
	uint64_t *array;
	uint_t c, idx;

	array = kmem_alloc(rvd->vdev_children * sizeof (uint64_t), KM_SLEEP);

	for (c = 0, idx = 0; c < rvd->vdev_children; c++) {
		vdev_t *tvd = rvd->vdev_child[c];

		if (tvd->vdev_ishole) {
			array[idx++] = c;
		}
	}

	if (idx) {
		VERIFY(nvlist_add_uint64_array(config, ZPOOL_CONFIG_HOLE_ARRAY,
		    array, idx) == 0);
	}

	VERIFY(nvlist_add_uint64(config, ZPOOL_CONFIG_VDEV_CHILDREN,
	    rvd->vdev_children) == 0);

	kmem_free(array, rvd->vdev_children * sizeof (uint64_t));
}

 
nvlist_t *
vdev_label_read_config(vdev_t *vd, uint64_t txg)
{
	spa_t *spa = vd->vdev_spa;
	nvlist_t *config = NULL;
	vdev_phys_t *vp[VDEV_LABELS];
	abd_t *vp_abd[VDEV_LABELS];
	zio_t *zio[VDEV_LABELS];
	uint64_t best_txg = 0;
	uint64_t label_txg = 0;
	int error = 0;
	int flags = ZIO_FLAG_CONFIG_WRITER | ZIO_FLAG_CANFAIL |
	    ZIO_FLAG_SPECULATIVE;

	ASSERT(vd->vdev_validate_thread == curthread ||
	    spa_config_held(spa, SCL_STATE_ALL, RW_WRITER) == SCL_STATE_ALL);

	if (!vdev_readable(vd))
		return (NULL);

	 
	if (vd->vdev_ops == &vdev_draid_spare_ops)
		return (vdev_draid_read_config_spare(vd));

	for (int l = 0; l < VDEV_LABELS; l++) {
		vp_abd[l] = abd_alloc_linear(sizeof (vdev_phys_t), B_TRUE);
		vp[l] = abd_to_buf(vp_abd[l]);
	}

retry:
	for (int l = 0; l < VDEV_LABELS; l++) {
		zio[l] = zio_root(spa, NULL, NULL, flags);

		vdev_label_read(zio[l], vd, l, vp_abd[l],
		    offsetof(vdev_label_t, vl_vdev_phys), sizeof (vdev_phys_t),
		    NULL, NULL, flags);
	}
	for (int l = 0; l < VDEV_LABELS; l++) {
		nvlist_t *label = NULL;

		if (zio_wait(zio[l]) == 0 &&
		    nvlist_unpack(vp[l]->vp_nvlist, sizeof (vp[l]->vp_nvlist),
		    &label, 0) == 0) {
			 
			error = nvlist_lookup_uint64(label,
			    ZPOOL_CONFIG_POOL_TXG, &label_txg);
			if ((error || label_txg == 0) && !config) {
				config = label;
				for (l++; l < VDEV_LABELS; l++)
					zio_wait(zio[l]);
				break;
			} else if (label_txg <= txg && label_txg > best_txg) {
				best_txg = label_txg;
				nvlist_free(config);
				config = fnvlist_dup(label);
			}
		}

		if (label != NULL) {
			nvlist_free(label);
			label = NULL;
		}
	}

	if (config == NULL && !(flags & ZIO_FLAG_TRYHARD)) {
		flags |= ZIO_FLAG_TRYHARD;
		goto retry;
	}

	 
	if (config == NULL && label_txg != 0) {
		vdev_dbgmsg(vd, "label discarded as txg is too large "
		    "(%llu > %llu)", (u_longlong_t)label_txg,
		    (u_longlong_t)txg);
	}

	for (int l = 0; l < VDEV_LABELS; l++) {
		abd_free(vp_abd[l]);
	}

	return (config);
}

 
static boolean_t
vdev_inuse(vdev_t *vd, uint64_t crtxg, vdev_labeltype_t reason,
    uint64_t *spare_guid, uint64_t *l2cache_guid)
{
	spa_t *spa = vd->vdev_spa;
	uint64_t state, pool_guid, device_guid, txg, spare_pool;
	uint64_t vdtxg = 0;
	nvlist_t *label;

	if (spare_guid)
		*spare_guid = 0ULL;
	if (l2cache_guid)
		*l2cache_guid = 0ULL;

	 
	if ((label = vdev_label_read_config(vd, -1ULL)) == NULL)
		return (B_FALSE);

	(void) nvlist_lookup_uint64(label, ZPOOL_CONFIG_CREATE_TXG,
	    &vdtxg);

	if (nvlist_lookup_uint64(label, ZPOOL_CONFIG_POOL_STATE,
	    &state) != 0 ||
	    nvlist_lookup_uint64(label, ZPOOL_CONFIG_GUID,
	    &device_guid) != 0) {
		nvlist_free(label);
		return (B_FALSE);
	}

	if (state != POOL_STATE_SPARE && state != POOL_STATE_L2CACHE &&
	    (nvlist_lookup_uint64(label, ZPOOL_CONFIG_POOL_GUID,
	    &pool_guid) != 0 ||
	    nvlist_lookup_uint64(label, ZPOOL_CONFIG_POOL_TXG,
	    &txg) != 0)) {
		nvlist_free(label);
		return (B_FALSE);
	}

	nvlist_free(label);

	 
	if (state != POOL_STATE_SPARE && state != POOL_STATE_L2CACHE &&
	    !spa_guid_exists(pool_guid, device_guid) &&
	    !spa_spare_exists(device_guid, NULL, NULL) &&
	    !spa_l2cache_exists(device_guid, NULL))
		return (B_FALSE);

	 
	if (state != POOL_STATE_SPARE && state != POOL_STATE_L2CACHE &&
	    txg == 0 && vdtxg == crtxg)
		return (B_TRUE);

	 
	if (spa_spare_exists(device_guid, &spare_pool, NULL) ||
	    spa_has_spare(spa, device_guid)) {
		if (spare_guid)
			*spare_guid = device_guid;

		switch (reason) {
		case VDEV_LABEL_CREATE:
			return (B_TRUE);

		case VDEV_LABEL_REPLACE:
			return (!spa_has_spare(spa, device_guid) ||
			    spare_pool != 0ULL);

		case VDEV_LABEL_SPARE:
			return (spa_has_spare(spa, device_guid));
		default:
			break;
		}
	}

	 
	if (spa_l2cache_exists(device_guid, NULL) ||
	    spa_has_l2cache(spa, device_guid)) {
		if (l2cache_guid)
			*l2cache_guid = device_guid;

		switch (reason) {
		case VDEV_LABEL_CREATE:
			return (B_TRUE);

		case VDEV_LABEL_REPLACE:
			return (!spa_has_l2cache(spa, device_guid));

		case VDEV_LABEL_L2CACHE:
			return (spa_has_l2cache(spa, device_guid));
		default:
			break;
		}
	}

	 
	if (state != POOL_STATE_SPARE && state != POOL_STATE_L2CACHE &&
	    (spa = spa_by_guid(pool_guid, device_guid)) != NULL &&
	    spa_mode(spa) == SPA_MODE_READ)
		state = POOL_STATE_ACTIVE;

	 
	return (state == POOL_STATE_ACTIVE);
}

 
int
vdev_label_init(vdev_t *vd, uint64_t crtxg, vdev_labeltype_t reason)
{
	spa_t *spa = vd->vdev_spa;
	nvlist_t *label;
	vdev_phys_t *vp;
	abd_t *vp_abd;
	abd_t *bootenv;
	uberblock_t *ub;
	abd_t *ub_abd;
	zio_t *zio;
	char *buf;
	size_t buflen;
	int error;
	uint64_t spare_guid = 0, l2cache_guid = 0;
	int flags = ZIO_FLAG_CONFIG_WRITER | ZIO_FLAG_CANFAIL;

	ASSERT(spa_config_held(spa, SCL_ALL, RW_WRITER) == SCL_ALL);

	for (int c = 0; c < vd->vdev_children; c++)
		if ((error = vdev_label_init(vd->vdev_child[c],
		    crtxg, reason)) != 0)
			return (error);

	 
	vd->vdev_crtxg = crtxg;

	if (!vd->vdev_ops->vdev_op_leaf || !spa_writeable(spa))
		return (0);

	 
	if (vdev_is_dead(vd))
		return (SET_ERROR(EIO));

	 
	if (reason != VDEV_LABEL_REMOVE && reason != VDEV_LABEL_SPLIT &&
	    vdev_inuse(vd, crtxg, reason, &spare_guid, &l2cache_guid))
		return (SET_ERROR(EBUSY));

	 
	if (reason != VDEV_LABEL_REMOVE && reason != VDEV_LABEL_L2CACHE &&
	    spare_guid != 0ULL) {
		uint64_t guid_delta = spare_guid - vd->vdev_guid;

		vd->vdev_guid += guid_delta;

		for (vdev_t *pvd = vd; pvd != NULL; pvd = pvd->vdev_parent)
			pvd->vdev_guid_sum += guid_delta;

		 
		if (reason == VDEV_LABEL_SPARE)
			return (0);
		ASSERT(reason == VDEV_LABEL_REPLACE ||
		    reason == VDEV_LABEL_SPLIT);
	}

	if (reason != VDEV_LABEL_REMOVE && reason != VDEV_LABEL_SPARE &&
	    l2cache_guid != 0ULL) {
		uint64_t guid_delta = l2cache_guid - vd->vdev_guid;

		vd->vdev_guid += guid_delta;

		for (vdev_t *pvd = vd; pvd != NULL; pvd = pvd->vdev_parent)
			pvd->vdev_guid_sum += guid_delta;

		 
		if (reason == VDEV_LABEL_L2CACHE)
			return (0);
		ASSERT(reason == VDEV_LABEL_REPLACE);
	}

	 
	vp_abd = abd_alloc_linear(sizeof (vdev_phys_t), B_TRUE);
	abd_zero(vp_abd, sizeof (vdev_phys_t));
	vp = abd_to_buf(vp_abd);

	 
	if (reason == VDEV_LABEL_SPARE ||
	    (reason == VDEV_LABEL_REMOVE && vd->vdev_isspare)) {
		 
		VERIFY(nvlist_alloc(&label, NV_UNIQUE_NAME, KM_SLEEP) == 0);

		VERIFY(nvlist_add_uint64(label, ZPOOL_CONFIG_VERSION,
		    spa_version(spa)) == 0);
		VERIFY(nvlist_add_uint64(label, ZPOOL_CONFIG_POOL_STATE,
		    POOL_STATE_SPARE) == 0);
		VERIFY(nvlist_add_uint64(label, ZPOOL_CONFIG_GUID,
		    vd->vdev_guid) == 0);
	} else if (reason == VDEV_LABEL_L2CACHE ||
	    (reason == VDEV_LABEL_REMOVE && vd->vdev_isl2cache)) {
		 
		VERIFY(nvlist_alloc(&label, NV_UNIQUE_NAME, KM_SLEEP) == 0);

		VERIFY(nvlist_add_uint64(label, ZPOOL_CONFIG_VERSION,
		    spa_version(spa)) == 0);
		VERIFY(nvlist_add_uint64(label, ZPOOL_CONFIG_POOL_STATE,
		    POOL_STATE_L2CACHE) == 0);
		VERIFY(nvlist_add_uint64(label, ZPOOL_CONFIG_GUID,
		    vd->vdev_guid) == 0);

		 
		VERIFY(nvlist_add_uint64(label, ZPOOL_CONFIG_ASHIFT,
		    vd->vdev_ashift) == 0);
	} else {
		uint64_t txg = 0ULL;

		if (reason == VDEV_LABEL_SPLIT)
			txg = spa->spa_uberblock.ub_txg;
		label = spa_config_generate(spa, vd, txg, B_FALSE);

		 
		VERIFY(nvlist_add_uint64(label, ZPOOL_CONFIG_CREATE_TXG,
		    crtxg) == 0);
	}

	buf = vp->vp_nvlist;
	buflen = sizeof (vp->vp_nvlist);

	error = nvlist_pack(label, &buf, &buflen, NV_ENCODE_XDR, KM_SLEEP);
	if (error != 0) {
		nvlist_free(label);
		abd_free(vp_abd);
		 
		return (SET_ERROR(error == EFAULT ? ENAMETOOLONG : EINVAL));
	}

	 
	ub_abd = abd_alloc_linear(VDEV_UBERBLOCK_RING, B_TRUE);
	abd_zero(ub_abd, VDEV_UBERBLOCK_RING);
	abd_copy_from_buf(ub_abd, &spa->spa_uberblock, sizeof (uberblock_t));
	ub = abd_to_buf(ub_abd);
	ub->ub_txg = 0;

	 
	bootenv = abd_alloc_for_io(VDEV_PAD_SIZE, B_TRUE);
	abd_zero(bootenv, VDEV_PAD_SIZE);

	 
retry:
	zio = zio_root(spa, NULL, NULL, flags);

	for (int l = 0; l < VDEV_LABELS; l++) {

		vdev_label_write(zio, vd, l, vp_abd,
		    offsetof(vdev_label_t, vl_vdev_phys),
		    sizeof (vdev_phys_t), NULL, NULL, flags);

		 
		vdev_label_write(zio, vd, l, bootenv,
		    offsetof(vdev_label_t, vl_be),
		    VDEV_PAD_SIZE, NULL, NULL, flags);

		vdev_label_write(zio, vd, l, ub_abd,
		    offsetof(vdev_label_t, vl_uberblock),
		    VDEV_UBERBLOCK_RING, NULL, NULL, flags);
	}

	error = zio_wait(zio);

	if (error != 0 && !(flags & ZIO_FLAG_TRYHARD)) {
		flags |= ZIO_FLAG_TRYHARD;
		goto retry;
	}

	nvlist_free(label);
	abd_free(bootenv);
	abd_free(ub_abd);
	abd_free(vp_abd);

	 
	if (error == 0 && !vd->vdev_isspare &&
	    (reason == VDEV_LABEL_SPARE ||
	    spa_spare_exists(vd->vdev_guid, NULL, NULL)))
		spa_spare_add(vd);

	if (error == 0 && !vd->vdev_isl2cache &&
	    (reason == VDEV_LABEL_L2CACHE ||
	    spa_l2cache_exists(vd->vdev_guid, NULL)))
		spa_l2cache_add(vd);

	return (error);
}

 
static void
vdev_label_read_bootenv_done(zio_t *zio)
{
	zio_t *rio = zio->io_private;
	abd_t **cbp = rio->io_private;

	ASSERT3U(zio->io_size, ==, VDEV_PAD_SIZE);

	if (zio->io_error == 0) {
		mutex_enter(&rio->io_lock);
		if (*cbp == NULL) {
			 
			*cbp = zio->io_abd;
		} else {
			abd_free(zio->io_abd);
		}
		mutex_exit(&rio->io_lock);
	} else {
		abd_free(zio->io_abd);
	}
}

static void
vdev_label_read_bootenv_impl(zio_t *zio, vdev_t *vd, int flags)
{
	for (int c = 0; c < vd->vdev_children; c++)
		vdev_label_read_bootenv_impl(zio, vd->vdev_child[c], flags);

	 
	if (vd->vdev_ops->vdev_op_leaf && vdev_readable(vd)) {
		for (int l = 0; l < VDEV_LABELS; l++) {
			vdev_label_read(zio, vd, l,
			    abd_alloc_linear(VDEV_PAD_SIZE, B_FALSE),
			    offsetof(vdev_label_t, vl_be), VDEV_PAD_SIZE,
			    vdev_label_read_bootenv_done, zio, flags);
		}
	}
}

int
vdev_label_read_bootenv(vdev_t *rvd, nvlist_t *bootenv)
{
	nvlist_t *config;
	spa_t *spa = rvd->vdev_spa;
	abd_t *abd = NULL;
	int flags = ZIO_FLAG_CONFIG_WRITER | ZIO_FLAG_CANFAIL |
	    ZIO_FLAG_SPECULATIVE | ZIO_FLAG_TRYHARD;

	ASSERT(bootenv);
	ASSERT(spa_config_held(spa, SCL_ALL, RW_WRITER) == SCL_ALL);

	zio_t *zio = zio_root(spa, NULL, &abd, flags);
	vdev_label_read_bootenv_impl(zio, rvd, flags);
	int err = zio_wait(zio);

	if (abd != NULL) {
		char *buf;
		vdev_boot_envblock_t *vbe = abd_to_buf(abd);

		vbe->vbe_version = ntohll(vbe->vbe_version);
		switch (vbe->vbe_version) {
		case VB_RAW:
			 
			fnvlist_add_uint64(bootenv, BOOTENV_VERSION, VB_RAW);
			vbe->vbe_bootenv[sizeof (vbe->vbe_bootenv) - 1] = '\0';
			fnvlist_add_string(bootenv, GRUB_ENVMAP,
			    vbe->vbe_bootenv);
			break;

		case VB_NVLIST:
			err = nvlist_unpack(vbe->vbe_bootenv,
			    sizeof (vbe->vbe_bootenv), &config, 0);
			if (err == 0) {
				fnvlist_merge(bootenv, config);
				nvlist_free(config);
				break;
			}
			zfs_fallthrough;
		default:
			 
			buf = abd_to_buf(abd);
			if (*buf == '\0') {
				fnvlist_add_uint64(bootenv, BOOTENV_VERSION,
				    VB_NVLIST);
				break;
			}
			fnvlist_add_string(bootenv, FREEBSD_BOOTONCE, buf);
		}

		 
		abd_free(abd);
		 
		return (0);
	}
	return (err);
}

int
vdev_label_write_bootenv(vdev_t *vd, nvlist_t *env)
{
	zio_t *zio;
	spa_t *spa = vd->vdev_spa;
	vdev_boot_envblock_t *bootenv;
	int flags = ZIO_FLAG_CONFIG_WRITER | ZIO_FLAG_CANFAIL;
	int error;
	size_t nvsize;
	char *nvbuf;
	const char *tmp;

	error = nvlist_size(env, &nvsize, NV_ENCODE_XDR);
	if (error != 0)
		return (SET_ERROR(error));

	if (nvsize >= sizeof (bootenv->vbe_bootenv)) {
		return (SET_ERROR(E2BIG));
	}

	ASSERT(spa_config_held(spa, SCL_ALL, RW_WRITER) == SCL_ALL);

	error = ENXIO;
	for (int c = 0; c < vd->vdev_children; c++) {
		int child_err;

		child_err = vdev_label_write_bootenv(vd->vdev_child[c], env);
		 
		if (child_err == 0)
			error = child_err;
	}

	if (!vd->vdev_ops->vdev_op_leaf || vdev_is_dead(vd) ||
	    !vdev_writeable(vd)) {
		return (error);
	}
	ASSERT3U(sizeof (*bootenv), ==, VDEV_PAD_SIZE);
	abd_t *abd = abd_alloc_for_io(VDEV_PAD_SIZE, B_TRUE);
	abd_zero(abd, VDEV_PAD_SIZE);

	bootenv = abd_borrow_buf_copy(abd, VDEV_PAD_SIZE);
	nvbuf = bootenv->vbe_bootenv;
	nvsize = sizeof (bootenv->vbe_bootenv);

	bootenv->vbe_version = fnvlist_lookup_uint64(env, BOOTENV_VERSION);
	switch (bootenv->vbe_version) {
	case VB_RAW:
		if (nvlist_lookup_string(env, GRUB_ENVMAP, &tmp) == 0) {
			(void) strlcpy(bootenv->vbe_bootenv, tmp, nvsize);
		}
		error = 0;
		break;

	case VB_NVLIST:
		error = nvlist_pack(env, &nvbuf, &nvsize, NV_ENCODE_XDR,
		    KM_SLEEP);
		break;

	default:
		error = EINVAL;
		break;
	}

	if (error == 0) {
		bootenv->vbe_version = htonll(bootenv->vbe_version);
		abd_return_buf_copy(abd, bootenv, VDEV_PAD_SIZE);
	} else {
		abd_free(abd);
		return (SET_ERROR(error));
	}

retry:
	zio = zio_root(spa, NULL, NULL, flags);
	for (int l = 0; l < VDEV_LABELS; l++) {
		vdev_label_write(zio, vd, l, abd,
		    offsetof(vdev_label_t, vl_be),
		    VDEV_PAD_SIZE, NULL, NULL, flags);
	}

	error = zio_wait(zio);
	if (error != 0 && !(flags & ZIO_FLAG_TRYHARD)) {
		flags |= ZIO_FLAG_TRYHARD;
		goto retry;
	}

	abd_free(abd);
	return (error);
}

 

 
static int
vdev_uberblock_compare(const uberblock_t *ub1, const uberblock_t *ub2)
{
	int cmp = TREE_CMP(ub1->ub_txg, ub2->ub_txg);

	if (likely(cmp))
		return (cmp);

	cmp = TREE_CMP(ub1->ub_timestamp, ub2->ub_timestamp);
	if (likely(cmp))
		return (cmp);

	 
	unsigned int seq1 = 0;
	unsigned int seq2 = 0;

	if (MMP_VALID(ub1) && MMP_SEQ_VALID(ub1))
		seq1 = MMP_SEQ(ub1);

	if (MMP_VALID(ub2) && MMP_SEQ_VALID(ub2))
		seq2 = MMP_SEQ(ub2);

	return (TREE_CMP(seq1, seq2));
}

struct ubl_cbdata {
	uberblock_t	*ubl_ubbest;	 
	vdev_t		*ubl_vd;	 
};

static void
vdev_uberblock_load_done(zio_t *zio)
{
	vdev_t *vd = zio->io_vd;
	spa_t *spa = zio->io_spa;
	zio_t *rio = zio->io_private;
	uberblock_t *ub = abd_to_buf(zio->io_abd);
	struct ubl_cbdata *cbp = rio->io_private;

	ASSERT3U(zio->io_size, ==, VDEV_UBERBLOCK_SIZE(vd));

	if (zio->io_error == 0 && uberblock_verify(ub) == 0) {
		mutex_enter(&rio->io_lock);
		if (ub->ub_txg <= spa->spa_load_max_txg &&
		    vdev_uberblock_compare(ub, cbp->ubl_ubbest) > 0) {
			 
			*cbp->ubl_ubbest = *ub;
			cbp->ubl_vd = vd;
		}
		mutex_exit(&rio->io_lock);
	}

	abd_free(zio->io_abd);
}

static void
vdev_uberblock_load_impl(zio_t *zio, vdev_t *vd, int flags,
    struct ubl_cbdata *cbp)
{
	for (int c = 0; c < vd->vdev_children; c++)
		vdev_uberblock_load_impl(zio, vd->vdev_child[c], flags, cbp);

	if (vd->vdev_ops->vdev_op_leaf && vdev_readable(vd) &&
	    vd->vdev_ops != &vdev_draid_spare_ops) {
		for (int l = 0; l < VDEV_LABELS; l++) {
			for (int n = 0; n < VDEV_UBERBLOCK_COUNT(vd); n++) {
				vdev_label_read(zio, vd, l,
				    abd_alloc_linear(VDEV_UBERBLOCK_SIZE(vd),
				    B_TRUE), VDEV_UBERBLOCK_OFFSET(vd, n),
				    VDEV_UBERBLOCK_SIZE(vd),
				    vdev_uberblock_load_done, zio, flags);
			}
		}
	}
}

 
void
vdev_uberblock_load(vdev_t *rvd, uberblock_t *ub, nvlist_t **config)
{
	zio_t *zio;
	spa_t *spa = rvd->vdev_spa;
	struct ubl_cbdata cb;
	int flags = ZIO_FLAG_CONFIG_WRITER | ZIO_FLAG_CANFAIL |
	    ZIO_FLAG_SPECULATIVE | ZIO_FLAG_TRYHARD;

	ASSERT(ub);
	ASSERT(config);

	memset(ub, 0, sizeof (uberblock_t));
	*config = NULL;

	cb.ubl_ubbest = ub;
	cb.ubl_vd = NULL;

	spa_config_enter(spa, SCL_ALL, FTAG, RW_WRITER);
	zio = zio_root(spa, NULL, &cb, flags);
	vdev_uberblock_load_impl(zio, rvd, flags, &cb);
	(void) zio_wait(zio);

	 
	if (cb.ubl_vd != NULL) {
		vdev_dbgmsg(cb.ubl_vd, "best uberblock found for spa %s. "
		    "txg %llu", spa->spa_name, (u_longlong_t)ub->ub_txg);

		*config = vdev_label_read_config(cb.ubl_vd, ub->ub_txg);
		if (*config == NULL && spa->spa_extreme_rewind) {
			vdev_dbgmsg(cb.ubl_vd, "failed to read label config. "
			    "Trying again without txg restrictions.");
			*config = vdev_label_read_config(cb.ubl_vd, UINT64_MAX);
		}
		if (*config == NULL) {
			vdev_dbgmsg(cb.ubl_vd, "failed to read label config");
		}
	}
	spa_config_exit(spa, SCL_ALL, FTAG);
}

 

static void
vdev_copy_uberblocks(vdev_t *vd)
{
	abd_t *ub_abd;
	zio_t *write_zio;
	int locks = (SCL_L2ARC | SCL_ZIO);
	int flags = ZIO_FLAG_CONFIG_WRITER | ZIO_FLAG_CANFAIL |
	    ZIO_FLAG_SPECULATIVE;

	ASSERT(spa_config_held(vd->vdev_spa, SCL_STATE, RW_READER) ==
	    SCL_STATE);
	ASSERT(vd->vdev_ops->vdev_op_leaf);

	 
	if (vd->vdev_ops == &vdev_draid_spare_ops)
		return;

	spa_config_enter(vd->vdev_spa, locks, FTAG, RW_READER);

	ub_abd = abd_alloc_linear(VDEV_UBERBLOCK_SIZE(vd), B_TRUE);

	write_zio = zio_root(vd->vdev_spa, NULL, NULL, flags);
	for (int n = 0; n < VDEV_UBERBLOCK_COUNT(vd); n++) {
		const int src_label = 0;
		zio_t *zio;

		zio = zio_root(vd->vdev_spa, NULL, NULL, flags);
		vdev_label_read(zio, vd, src_label, ub_abd,
		    VDEV_UBERBLOCK_OFFSET(vd, n), VDEV_UBERBLOCK_SIZE(vd),
		    NULL, NULL, flags);

		if (zio_wait(zio) || uberblock_verify(abd_to_buf(ub_abd)))
			abd_zero(ub_abd, VDEV_UBERBLOCK_SIZE(vd));

		for (int l = 2; l < VDEV_LABELS; l++)
			vdev_label_write(write_zio, vd, l, ub_abd,
			    VDEV_UBERBLOCK_OFFSET(vd, n),
			    VDEV_UBERBLOCK_SIZE(vd), NULL, NULL,
			    flags | ZIO_FLAG_DONT_PROPAGATE);
	}
	(void) zio_wait(write_zio);

	spa_config_exit(vd->vdev_spa, locks, FTAG);

	abd_free(ub_abd);
}

 
static void
vdev_uberblock_sync_done(zio_t *zio)
{
	uint64_t *good_writes = zio->io_private;

	if (zio->io_error == 0 && zio->io_vd->vdev_top->vdev_ms_array != 0)
		atomic_inc_64(good_writes);
}

 
static void
vdev_uberblock_sync(zio_t *zio, uint64_t *good_writes,
    uberblock_t *ub, vdev_t *vd, int flags)
{
	for (uint64_t c = 0; c < vd->vdev_children; c++) {
		vdev_uberblock_sync(zio, good_writes,
		    ub, vd->vdev_child[c], flags);
	}

	if (!vd->vdev_ops->vdev_op_leaf)
		return;

	if (!vdev_writeable(vd))
		return;

	 
	if (vd->vdev_ops == &vdev_draid_spare_ops)
		return;

	 
	if (vd->vdev_state == VDEV_STATE_HEALTHY &&
	    vd->vdev_copy_uberblocks == B_TRUE) {
		vdev_copy_uberblocks(vd);
		vd->vdev_copy_uberblocks = B_FALSE;
	}

	int m = spa_multihost(vd->vdev_spa) ? MMP_BLOCKS_PER_LABEL : 0;
	int n = ub->ub_txg % (VDEV_UBERBLOCK_COUNT(vd) - m);

	 
	abd_t *ub_abd = abd_alloc_for_io(VDEV_UBERBLOCK_SIZE(vd), B_TRUE);
	abd_zero(ub_abd, VDEV_UBERBLOCK_SIZE(vd));
	abd_copy_from_buf(ub_abd, ub, sizeof (uberblock_t));

	for (int l = 0; l < VDEV_LABELS; l++)
		vdev_label_write(zio, vd, l, ub_abd,
		    VDEV_UBERBLOCK_OFFSET(vd, n), VDEV_UBERBLOCK_SIZE(vd),
		    vdev_uberblock_sync_done, good_writes,
		    flags | ZIO_FLAG_DONT_PROPAGATE);

	abd_free(ub_abd);
}

 
static int
vdev_uberblock_sync_list(vdev_t **svd, int svdcount, uberblock_t *ub, int flags)
{
	spa_t *spa = svd[0]->vdev_spa;
	zio_t *zio;
	uint64_t good_writes = 0;

	zio = zio_root(spa, NULL, NULL, flags);

	for (int v = 0; v < svdcount; v++)
		vdev_uberblock_sync(zio, &good_writes, ub, svd[v], flags);

	(void) zio_wait(zio);

	 
	zio = zio_root(spa, NULL, NULL, flags);

	for (int v = 0; v < svdcount; v++) {
		if (vdev_writeable(svd[v])) {
			zio_flush(zio, svd[v]);
		}
	}

	(void) zio_wait(zio);

	return (good_writes >= 1 ? 0 : EIO);
}

 
static void
vdev_label_sync_done(zio_t *zio)
{
	uint64_t *good_writes = zio->io_private;

	if (zio->io_error == 0)
		atomic_inc_64(good_writes);
}

 
static void
vdev_label_sync_top_done(zio_t *zio)
{
	uint64_t *good_writes = zio->io_private;

	if (*good_writes == 0)
		zio->io_error = SET_ERROR(EIO);

	kmem_free(good_writes, sizeof (uint64_t));
}

 
static void
vdev_label_sync_ignore_done(zio_t *zio)
{
	kmem_free(zio->io_private, sizeof (uint64_t));
}

 
static void
vdev_label_sync(zio_t *zio, uint64_t *good_writes,
    vdev_t *vd, int l, uint64_t txg, int flags)
{
	nvlist_t *label;
	vdev_phys_t *vp;
	abd_t *vp_abd;
	char *buf;
	size_t buflen;

	for (int c = 0; c < vd->vdev_children; c++) {
		vdev_label_sync(zio, good_writes,
		    vd->vdev_child[c], l, txg, flags);
	}

	if (!vd->vdev_ops->vdev_op_leaf)
		return;

	if (!vdev_writeable(vd))
		return;

	 
	if (vd->vdev_ops == &vdev_draid_spare_ops)
		return;

	 
	label = spa_config_generate(vd->vdev_spa, vd, txg, B_FALSE);

	vp_abd = abd_alloc_linear(sizeof (vdev_phys_t), B_TRUE);
	abd_zero(vp_abd, sizeof (vdev_phys_t));
	vp = abd_to_buf(vp_abd);

	buf = vp->vp_nvlist;
	buflen = sizeof (vp->vp_nvlist);

	if (!nvlist_pack(label, &buf, &buflen, NV_ENCODE_XDR, KM_SLEEP)) {
		for (; l < VDEV_LABELS; l += 2) {
			vdev_label_write(zio, vd, l, vp_abd,
			    offsetof(vdev_label_t, vl_vdev_phys),
			    sizeof (vdev_phys_t),
			    vdev_label_sync_done, good_writes,
			    flags | ZIO_FLAG_DONT_PROPAGATE);
		}
	}

	abd_free(vp_abd);
	nvlist_free(label);
}

static int
vdev_label_sync_list(spa_t *spa, int l, uint64_t txg, int flags)
{
	list_t *dl = &spa->spa_config_dirty_list;
	vdev_t *vd;
	zio_t *zio;
	int error;

	 
	zio = zio_root(spa, NULL, NULL, flags);

	for (vd = list_head(dl); vd != NULL; vd = list_next(dl, vd)) {
		uint64_t *good_writes;

		ASSERT(!vd->vdev_ishole);

		good_writes = kmem_zalloc(sizeof (uint64_t), KM_SLEEP);
		zio_t *vio = zio_null(zio, spa, NULL,
		    (vd->vdev_islog || vd->vdev_aux != NULL) ?
		    vdev_label_sync_ignore_done : vdev_label_sync_top_done,
		    good_writes, flags);
		vdev_label_sync(vio, good_writes, vd, l, txg, flags);
		zio_nowait(vio);
	}

	error = zio_wait(zio);

	 
	zio = zio_root(spa, NULL, NULL, flags);

	for (vd = list_head(dl); vd != NULL; vd = list_next(dl, vd))
		zio_flush(zio, vd);

	(void) zio_wait(zio);

	return (error);
}

 
int
vdev_config_sync(vdev_t **svd, int svdcount, uint64_t txg)
{
	spa_t *spa = svd[0]->vdev_spa;
	uberblock_t *ub = &spa->spa_uberblock;
	int error = 0;
	int flags = ZIO_FLAG_CONFIG_WRITER | ZIO_FLAG_CANFAIL;

	ASSERT(svdcount != 0);
retry:
	 
	if (error != 0) {
		if ((flags & ZIO_FLAG_TRYHARD) != 0)
			return (error);
		flags |= ZIO_FLAG_TRYHARD;
	}

	ASSERT(ub->ub_txg <= txg);

	 
	if (ub->ub_txg < txg) {
		boolean_t changed = uberblock_update(ub, spa->spa_root_vdev,
		    txg, spa->spa_mmp.mmp_delay);

		if (!changed && list_is_empty(&spa->spa_config_dirty_list))
			return (0);
	}

	if (txg > spa_freeze_txg(spa))
		return (0);

	ASSERT(txg <= spa->spa_final_txg);

	 
	zio_t *zio = zio_root(spa, NULL, NULL, flags);

	for (vdev_t *vd =
	    txg_list_head(&spa->spa_vdev_txg_list, TXG_CLEAN(txg)); vd != NULL;
	    vd = txg_list_next(&spa->spa_vdev_txg_list, vd, TXG_CLEAN(txg)))
		zio_flush(zio, vd);

	(void) zio_wait(zio);

	 
	if ((error = vdev_label_sync_list(spa, 0, txg, flags)) != 0) {
		if ((flags & ZIO_FLAG_TRYHARD) != 0) {
			zfs_dbgmsg("vdev_label_sync_list() returned error %d "
			    "for pool '%s' when syncing out the even labels "
			    "of dirty vdevs", error, spa_name(spa));
		}
		goto retry;
	}

	 
	if ((error = vdev_uberblock_sync_list(svd, svdcount, ub, flags)) != 0) {
		if ((flags & ZIO_FLAG_TRYHARD) != 0) {
			zfs_dbgmsg("vdev_uberblock_sync_list() returned error "
			    "%d for pool '%s'", error, spa_name(spa));
		}
		goto retry;
	}

	if (spa_multihost(spa))
		mmp_update_uberblock(spa, ub);

	 
	if ((error = vdev_label_sync_list(spa, 1, txg, flags)) != 0) {
		if ((flags & ZIO_FLAG_TRYHARD) != 0) {
			zfs_dbgmsg("vdev_label_sync_list() returned error %d "
			    "for pool '%s' when syncing out the odd labels of "
			    "dirty vdevs", error, spa_name(spa));
		}
		goto retry;
	}

	return (0);
}
