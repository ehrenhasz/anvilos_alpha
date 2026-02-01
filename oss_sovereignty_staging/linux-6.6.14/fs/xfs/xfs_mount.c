
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_sb.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_dir2.h"
#include "xfs_ialloc.h"
#include "xfs_alloc.h"
#include "xfs_rtalloc.h"
#include "xfs_bmap.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_log.h"
#include "xfs_log_priv.h"
#include "xfs_error.h"
#include "xfs_quota.h"
#include "xfs_fsops.h"
#include "xfs_icache.h"
#include "xfs_sysfs.h"
#include "xfs_rmap_btree.h"
#include "xfs_refcount_btree.h"
#include "xfs_reflink.h"
#include "xfs_extent_busy.h"
#include "xfs_health.h"
#include "xfs_trace.h"
#include "xfs_ag.h"
#include "scrub/stats.h"

static DEFINE_MUTEX(xfs_uuid_table_mutex);
static int xfs_uuid_table_size;
static uuid_t *xfs_uuid_table;

void
xfs_uuid_table_free(void)
{
	if (xfs_uuid_table_size == 0)
		return;
	kmem_free(xfs_uuid_table);
	xfs_uuid_table = NULL;
	xfs_uuid_table_size = 0;
}

 
STATIC int
xfs_uuid_mount(
	struct xfs_mount	*mp)
{
	uuid_t			*uuid = &mp->m_sb.sb_uuid;
	int			hole, i;

	 
	uuid_copy(&mp->m_super->s_uuid, uuid);

	if (xfs_has_nouuid(mp))
		return 0;

	if (uuid_is_null(uuid)) {
		xfs_warn(mp, "Filesystem has null UUID - can't mount");
		return -EINVAL;
	}

	mutex_lock(&xfs_uuid_table_mutex);
	for (i = 0, hole = -1; i < xfs_uuid_table_size; i++) {
		if (uuid_is_null(&xfs_uuid_table[i])) {
			hole = i;
			continue;
		}
		if (uuid_equal(uuid, &xfs_uuid_table[i]))
			goto out_duplicate;
	}

	if (hole < 0) {
		xfs_uuid_table = krealloc(xfs_uuid_table,
			(xfs_uuid_table_size + 1) * sizeof(*xfs_uuid_table),
			GFP_KERNEL | __GFP_NOFAIL);
		hole = xfs_uuid_table_size++;
	}
	xfs_uuid_table[hole] = *uuid;
	mutex_unlock(&xfs_uuid_table_mutex);

	return 0;

 out_duplicate:
	mutex_unlock(&xfs_uuid_table_mutex);
	xfs_warn(mp, "Filesystem has duplicate UUID %pU - can't mount", uuid);
	return -EINVAL;
}

STATIC void
xfs_uuid_unmount(
	struct xfs_mount	*mp)
{
	uuid_t			*uuid = &mp->m_sb.sb_uuid;
	int			i;

	if (xfs_has_nouuid(mp))
		return;

	mutex_lock(&xfs_uuid_table_mutex);
	for (i = 0; i < xfs_uuid_table_size; i++) {
		if (uuid_is_null(&xfs_uuid_table[i]))
			continue;
		if (!uuid_equal(uuid, &xfs_uuid_table[i]))
			continue;
		memset(&xfs_uuid_table[i], 0, sizeof(uuid_t));
		break;
	}
	ASSERT(i < xfs_uuid_table_size);
	mutex_unlock(&xfs_uuid_table_mutex);
}

 
int
xfs_sb_validate_fsb_count(
	xfs_sb_t	*sbp,
	uint64_t	nblocks)
{
	ASSERT(PAGE_SHIFT >= sbp->sb_blocklog);
	ASSERT(sbp->sb_blocklog >= BBSHIFT);

	 
	if (nblocks >> (PAGE_SHIFT - sbp->sb_blocklog) > ULONG_MAX)
		return -EFBIG;
	return 0;
}

 
int
xfs_readsb(
	struct xfs_mount *mp,
	int		flags)
{
	unsigned int	sector_size;
	struct xfs_buf	*bp;
	struct xfs_sb	*sbp = &mp->m_sb;
	int		error;
	int		loud = !(flags & XFS_MFSI_QUIET);
	const struct xfs_buf_ops *buf_ops;

	ASSERT(mp->m_sb_bp == NULL);
	ASSERT(mp->m_ddev_targp != NULL);

	 
	sector_size = xfs_getsize_buftarg(mp->m_ddev_targp);
	buf_ops = NULL;

	 
reread:
	error = xfs_buf_read_uncached(mp->m_ddev_targp, XFS_SB_DADDR,
				      BTOBB(sector_size), XBF_NO_IOACCT, &bp,
				      buf_ops);
	if (error) {
		if (loud)
			xfs_warn(mp, "SB validate failed with error %d.", error);
		 
		if (error == -EFSBADCRC)
			error = -EFSCORRUPTED;
		return error;
	}

	 
	xfs_sb_from_disk(sbp, bp->b_addr);

	 
	if (sbp->sb_magicnum != XFS_SB_MAGIC) {
		if (loud)
			xfs_warn(mp, "Invalid superblock magic number");
		error = -EINVAL;
		goto release_buf;
	}

	 
	if (sector_size > sbp->sb_sectsize) {
		if (loud)
			xfs_warn(mp, "device supports %u byte sectors (not %u)",
				sector_size, sbp->sb_sectsize);
		error = -ENOSYS;
		goto release_buf;
	}

	if (buf_ops == NULL) {
		 
		xfs_buf_relse(bp);
		sector_size = sbp->sb_sectsize;
		buf_ops = loud ? &xfs_sb_buf_ops : &xfs_sb_quiet_buf_ops;
		goto reread;
	}

	mp->m_features |= xfs_sb_version_to_features(sbp);
	xfs_reinit_percpu_counters(mp);

	 
	bp->b_ops = &xfs_sb_buf_ops;

	mp->m_sb_bp = bp;
	xfs_buf_unlock(bp);
	return 0;

release_buf:
	xfs_buf_relse(bp);
	return error;
}

 
static inline int
xfs_check_new_dalign(
	struct xfs_mount	*mp,
	int			new_dalign,
	bool			*update_sb)
{
	struct xfs_sb		*sbp = &mp->m_sb;
	xfs_ino_t		calc_ino;

	calc_ino = xfs_ialloc_calc_rootino(mp, new_dalign);
	trace_xfs_check_new_dalign(mp, new_dalign, calc_ino);

	if (sbp->sb_rootino == calc_ino) {
		*update_sb = true;
		return 0;
	}

	xfs_warn(mp,
"Cannot change stripe alignment; would require moving root inode.");

	 
	xfs_warn(mp, "Skipping superblock stripe alignment update.");
	*update_sb = false;
	return 0;
}

 
STATIC int
xfs_validate_new_dalign(
	struct xfs_mount	*mp)
{
	if (mp->m_dalign == 0)
		return 0;

	 
	if ((BBTOB(mp->m_dalign) & mp->m_blockmask) ||
	    (BBTOB(mp->m_swidth) & mp->m_blockmask)) {
		xfs_warn(mp,
	"alignment check failed: sunit/swidth vs. blocksize(%d)",
			mp->m_sb.sb_blocksize);
		return -EINVAL;
	}

	 
	mp->m_dalign = XFS_BB_TO_FSBT(mp, mp->m_dalign);
	if (mp->m_dalign && (mp->m_sb.sb_agblocks % mp->m_dalign)) {
		xfs_warn(mp,
	"alignment check failed: sunit/swidth vs. agsize(%d)",
			mp->m_sb.sb_agblocks);
		return -EINVAL;
	}

	if (!mp->m_dalign) {
		xfs_warn(mp,
	"alignment check failed: sunit(%d) less than bsize(%d)",
			mp->m_dalign, mp->m_sb.sb_blocksize);
		return -EINVAL;
	}

	mp->m_swidth = XFS_BB_TO_FSBT(mp, mp->m_swidth);

	if (!xfs_has_dalign(mp)) {
		xfs_warn(mp,
"cannot change alignment: superblock does not support data alignment");
		return -EINVAL;
	}

	return 0;
}

 
STATIC int
xfs_update_alignment(
	struct xfs_mount	*mp)
{
	struct xfs_sb		*sbp = &mp->m_sb;

	if (mp->m_dalign) {
		bool		update_sb;
		int		error;

		if (sbp->sb_unit == mp->m_dalign &&
		    sbp->sb_width == mp->m_swidth)
			return 0;

		error = xfs_check_new_dalign(mp, mp->m_dalign, &update_sb);
		if (error || !update_sb)
			return error;

		sbp->sb_unit = mp->m_dalign;
		sbp->sb_width = mp->m_swidth;
		mp->m_update_sb = true;
	} else if (!xfs_has_noalign(mp) && xfs_has_dalign(mp)) {
		mp->m_dalign = sbp->sb_unit;
		mp->m_swidth = sbp->sb_width;
	}

	return 0;
}

 
void
xfs_set_low_space_thresholds(
	struct xfs_mount	*mp)
{
	uint64_t		dblocks = mp->m_sb.sb_dblocks;
	uint64_t		rtexts = mp->m_sb.sb_rextents;
	int			i;

	do_div(dblocks, 100);
	do_div(rtexts, 100);

	for (i = 0; i < XFS_LOWSP_MAX; i++) {
		mp->m_low_space[i] = dblocks * (i + 1);
		mp->m_low_rtexts[i] = rtexts * (i + 1);
	}
}

 
STATIC int
xfs_check_sizes(
	struct xfs_mount *mp)
{
	struct xfs_buf	*bp;
	xfs_daddr_t	d;
	int		error;

	d = (xfs_daddr_t)XFS_FSB_TO_BB(mp, mp->m_sb.sb_dblocks);
	if (XFS_BB_TO_FSB(mp, d) != mp->m_sb.sb_dblocks) {
		xfs_warn(mp, "filesystem size mismatch detected");
		return -EFBIG;
	}
	error = xfs_buf_read_uncached(mp->m_ddev_targp,
					d - XFS_FSS_TO_BB(mp, 1),
					XFS_FSS_TO_BB(mp, 1), 0, &bp, NULL);
	if (error) {
		xfs_warn(mp, "last sector read failed");
		return error;
	}
	xfs_buf_relse(bp);

	if (mp->m_logdev_targp == mp->m_ddev_targp)
		return 0;

	d = (xfs_daddr_t)XFS_FSB_TO_BB(mp, mp->m_sb.sb_logblocks);
	if (XFS_BB_TO_FSB(mp, d) != mp->m_sb.sb_logblocks) {
		xfs_warn(mp, "log size mismatch detected");
		return -EFBIG;
	}
	error = xfs_buf_read_uncached(mp->m_logdev_targp,
					d - XFS_FSB_TO_BB(mp, 1),
					XFS_FSB_TO_BB(mp, 1), 0, &bp, NULL);
	if (error) {
		xfs_warn(mp, "log device read failed");
		return error;
	}
	xfs_buf_relse(bp);
	return 0;
}

 
int
xfs_mount_reset_sbqflags(
	struct xfs_mount	*mp)
{
	mp->m_qflags = 0;

	 
	if (mp->m_sb.sb_qflags == 0)
		return 0;
	spin_lock(&mp->m_sb_lock);
	mp->m_sb.sb_qflags = 0;
	spin_unlock(&mp->m_sb_lock);

	if (!xfs_fs_writable(mp, SB_FREEZE_WRITE))
		return 0;

	return xfs_sync_sb(mp, false);
}

uint64_t
xfs_default_resblks(xfs_mount_t *mp)
{
	uint64_t resblks;

	 
	resblks = mp->m_sb.sb_dblocks;
	do_div(resblks, 20);
	resblks = min_t(uint64_t, resblks, 8192);
	return resblks;
}

 
STATIC int
xfs_check_summary_counts(
	struct xfs_mount	*mp)
{
	int			error = 0;

	 
	if (mp->m_sb.sb_inprogress) {
		xfs_err(mp, "sb_inprogress set after log recovery??");
		WARN_ON(1);
		return -EFSCORRUPTED;
	}

	 
	if (xfs_is_clean(mp) &&
	    (mp->m_sb.sb_fdblocks > mp->m_sb.sb_dblocks ||
	     !xfs_verify_icount(mp, mp->m_sb.sb_icount) ||
	     mp->m_sb.sb_ifree > mp->m_sb.sb_icount))
		xfs_fs_mark_sick(mp, XFS_SICK_FS_COUNTERS);

	 
	if ((xfs_has_lazysbcount(mp) && !xfs_is_clean(mp)) ||
	    xfs_fs_has_sickness(mp, XFS_SICK_FS_COUNTERS)) {
		error = xfs_initialize_perag_data(mp, mp->m_sb.sb_agcount);
		if (error)
			return error;
	}

	 
	if (xfs_has_realtime(mp) && !xfs_is_clean(mp)) {
		error = xfs_rtalloc_reinit_frextents(mp);
		if (error)
			return error;
	}

	return 0;
}

static void
xfs_unmount_check(
	struct xfs_mount	*mp)
{
	if (xfs_is_shutdown(mp))
		return;

	if (percpu_counter_sum(&mp->m_ifree) >
			percpu_counter_sum(&mp->m_icount)) {
		xfs_alert(mp, "ifree/icount mismatch at unmount");
		xfs_fs_mark_sick(mp, XFS_SICK_FS_COUNTERS);
	}
}

 
static void
xfs_unmount_flush_inodes(
	struct xfs_mount	*mp)
{
	xfs_log_force(mp, XFS_LOG_SYNC);
	xfs_extent_busy_wait_all(mp);
	flush_workqueue(xfs_discard_wq);

	set_bit(XFS_OPSTATE_UNMOUNTING, &mp->m_opstate);

	xfs_ail_push_all_sync(mp->m_ail);
	xfs_inodegc_stop(mp);
	cancel_delayed_work_sync(&mp->m_reclaim_work);
	xfs_reclaim_inodes(mp);
	xfs_health_unmount(mp);
}

static void
xfs_mount_setup_inode_geom(
	struct xfs_mount	*mp)
{
	struct xfs_ino_geometry *igeo = M_IGEO(mp);

	igeo->attr_fork_offset = xfs_bmap_compute_attr_offset(mp);
	ASSERT(igeo->attr_fork_offset < XFS_LITINO(mp));

	xfs_ialloc_setup_geometry(mp);
}

 
static inline void
xfs_agbtree_compute_maxlevels(
	struct xfs_mount	*mp)
{
	unsigned int		levels;

	levels = max(mp->m_alloc_maxlevels, M_IGEO(mp)->inobt_maxlevels);
	levels = max(levels, mp->m_rmap_maxlevels);
	mp->m_agbtree_maxlevels = max(levels, mp->m_refc_maxlevels);
}

 
int
xfs_mountfs(
	struct xfs_mount	*mp)
{
	struct xfs_sb		*sbp = &(mp->m_sb);
	struct xfs_inode	*rip;
	struct xfs_ino_geometry	*igeo = M_IGEO(mp);
	uint64_t		resblks;
	uint			quotamount = 0;
	uint			quotaflags = 0;
	int			error = 0;

	xfs_sb_mount_common(mp, sbp);

	 
	if (xfs_sb_has_mismatched_features2(sbp)) {
		xfs_warn(mp, "correcting sb_features alignment problem");
		sbp->sb_features2 |= sbp->sb_bad_features2;
		mp->m_update_sb = true;
	}


	 
	if (!(mp->m_sb.sb_versionnum & XFS_SB_VERSION_NLINKBIT)) {
		mp->m_sb.sb_versionnum |= XFS_SB_VERSION_NLINKBIT;
		mp->m_features |= XFS_FEAT_NLINK;
		mp->m_update_sb = true;
	}

	 
	error = xfs_validate_new_dalign(mp);
	if (error)
		goto out;

	xfs_alloc_compute_maxlevels(mp);
	xfs_bmap_compute_maxlevels(mp, XFS_DATA_FORK);
	xfs_bmap_compute_maxlevels(mp, XFS_ATTR_FORK);
	xfs_mount_setup_inode_geom(mp);
	xfs_rmapbt_compute_maxlevels(mp);
	xfs_refcountbt_compute_maxlevels(mp);

	xfs_agbtree_compute_maxlevels(mp);

	 
	error = xfs_update_alignment(mp);
	if (error)
		goto out;

	 
	mp->m_fail_unmount = true;

	error = xfs_sysfs_init(&mp->m_kobj, &xfs_mp_ktype,
			       NULL, mp->m_super->s_id);
	if (error)
		goto out;

	error = xfs_sysfs_init(&mp->m_stats.xs_kobj, &xfs_stats_ktype,
			       &mp->m_kobj, "stats");
	if (error)
		goto out_remove_sysfs;

	xchk_stats_register(mp->m_scrub_stats, mp->m_debugfs);

	error = xfs_error_sysfs_init(mp);
	if (error)
		goto out_remove_scrub_stats;

	error = xfs_errortag_init(mp);
	if (error)
		goto out_remove_error_sysfs;

	error = xfs_uuid_mount(mp);
	if (error)
		goto out_remove_errortag;

	 
	mp->m_allocsize_log =
		max_t(uint32_t, sbp->sb_blocklog, mp->m_allocsize_log);
	mp->m_allocsize_blocks = 1U << (mp->m_allocsize_log - sbp->sb_blocklog);

	 
	xfs_set_low_space_thresholds(mp);

	 
	if (xfs_has_sparseinodes(mp) &&
	    mp->m_sb.sb_spino_align !=
			XFS_B_TO_FSBT(mp, igeo->inode_cluster_size_raw)) {
		xfs_warn(mp,
	"Sparse inode block alignment (%u) must match cluster size (%llu).",
			 mp->m_sb.sb_spino_align,
			 XFS_B_TO_FSBT(mp, igeo->inode_cluster_size_raw));
		error = -EINVAL;
		goto out_remove_uuid;
	}

	 
	error = xfs_check_sizes(mp);
	if (error)
		goto out_remove_uuid;

	 
	error = xfs_rtmount_init(mp);
	if (error) {
		xfs_warn(mp, "RT mount failed");
		goto out_remove_uuid;
	}

	 
	mp->m_fixedfsid[0] =
		(get_unaligned_be16(&sbp->sb_uuid.b[8]) << 16) |
		 get_unaligned_be16(&sbp->sb_uuid.b[4]);
	mp->m_fixedfsid[1] = get_unaligned_be32(&sbp->sb_uuid.b[0]);

	error = xfs_da_mount(mp);
	if (error) {
		xfs_warn(mp, "Failed dir/attr init: %d", error);
		goto out_remove_uuid;
	}

	 
	xfs_trans_init(mp);

	 
	error = xfs_initialize_perag(mp, sbp->sb_agcount, mp->m_sb.sb_dblocks,
			&mp->m_maxagi);
	if (error) {
		xfs_warn(mp, "Failed per-ag init: %d", error);
		goto out_free_dir;
	}

	if (XFS_IS_CORRUPT(mp, !sbp->sb_logblocks)) {
		xfs_warn(mp, "no log defined");
		error = -EFSCORRUPTED;
		goto out_free_perag;
	}

	error = xfs_inodegc_register_shrinker(mp);
	if (error)
		goto out_fail_wait;

	 
	error = xfs_log_mount(mp, mp->m_logdev_targp,
			      XFS_FSB_TO_DADDR(mp, sbp->sb_logstart),
			      XFS_FSB_TO_BB(mp, sbp->sb_logblocks));
	if (error) {
		xfs_warn(mp, "log mount failed");
		goto out_inodegc_shrinker;
	}

	 
	xfs_inodegc_start(mp);
	xfs_blockgc_start(mp);

	 
	if (xfs_has_noattr2(mp)) {
		mp->m_features &= ~XFS_FEAT_ATTR2;
	} else if (!xfs_has_attr2(mp) &&
		   (mp->m_sb.sb_features2 & XFS_SB_VERSION2_ATTR2BIT)) {
		mp->m_features |= XFS_FEAT_ATTR2;
	}

	 
	error = xfs_iget(mp, NULL, sbp->sb_rootino, XFS_IGET_UNTRUSTED,
			 XFS_ILOCK_EXCL, &rip);
	if (error) {
		xfs_warn(mp,
			"Failed to read root inode 0x%llx, error %d",
			sbp->sb_rootino, -error);
		goto out_log_dealloc;
	}

	ASSERT(rip != NULL);

	if (XFS_IS_CORRUPT(mp, !S_ISDIR(VFS_I(rip)->i_mode))) {
		xfs_warn(mp, "corrupted root inode %llu: not a directory",
			(unsigned long long)rip->i_ino);
		xfs_iunlock(rip, XFS_ILOCK_EXCL);
		error = -EFSCORRUPTED;
		goto out_rele_rip;
	}
	mp->m_rootip = rip;	 

	xfs_iunlock(rip, XFS_ILOCK_EXCL);

	 
	error = xfs_rtmount_inodes(mp);
	if (error) {
		 
		xfs_warn(mp, "failed to read RT inodes");
		goto out_rele_rip;
	}

	 
	error = xfs_check_summary_counts(mp);
	if (error)
		goto out_rtunmount;

	 
	if (mp->m_update_sb && !xfs_is_readonly(mp)) {
		error = xfs_sync_sb(mp, false);
		if (error) {
			xfs_warn(mp, "failed to write sb changes");
			goto out_rtunmount;
		}
	}

	 
	if (XFS_IS_QUOTA_ON(mp)) {
		error = xfs_qm_newmount(mp, &quotamount, &quotaflags);
		if (error)
			goto out_rtunmount;
	} else {
		 
		if (mp->m_sb.sb_qflags & XFS_ALL_QUOTA_ACCT) {
			xfs_notice(mp, "resetting quota flags");
			error = xfs_mount_reset_sbqflags(mp);
			if (error)
				goto out_rtunmount;
		}
	}

	 
	error = xfs_fs_reserve_ag_blocks(mp);
	if (error && error == -ENOSPC)
		xfs_warn(mp,
	"ENOSPC reserving per-AG metadata pool, log recovery may fail.");
	error = xfs_log_mount_finish(mp);
	xfs_fs_unreserve_ag_blocks(mp);
	if (error) {
		xfs_warn(mp, "log mount finish failed");
		goto out_rtunmount;
	}

	 
	if (xfs_is_readonly(mp) && !xfs_has_norecovery(mp))
		xfs_log_clean(mp);

	 
	if (quotamount) {
		ASSERT(mp->m_qflags == 0);
		mp->m_qflags = quotaflags;

		xfs_qm_mount_quotas(mp);
	}

	 
	if (!xfs_is_readonly(mp)) {
		resblks = xfs_default_resblks(mp);
		error = xfs_reserve_blocks(mp, &resblks, NULL);
		if (error)
			xfs_warn(mp,
	"Unable to allocate reserve blocks. Continuing without reserve pool.");

		 
		error = xfs_fs_reserve_ag_blocks(mp);
		if (error && error != -ENOSPC)
			goto out_agresv;
	}

	return 0;

 out_agresv:
	xfs_fs_unreserve_ag_blocks(mp);
	xfs_qm_unmount_quotas(mp);
 out_rtunmount:
	xfs_rtunmount_inodes(mp);
 out_rele_rip:
	xfs_irele(rip);
	 
	xfs_qm_unmount(mp);

	 
	xfs_inodegc_flush(mp);

	 
	xfs_unmount_flush_inodes(mp);
 out_log_dealloc:
	xfs_log_mount_cancel(mp);
 out_inodegc_shrinker:
	unregister_shrinker(&mp->m_inodegc_shrinker);
 out_fail_wait:
	if (mp->m_logdev_targp && mp->m_logdev_targp != mp->m_ddev_targp)
		xfs_buftarg_drain(mp->m_logdev_targp);
	xfs_buftarg_drain(mp->m_ddev_targp);
 out_free_perag:
	xfs_free_perag(mp);
 out_free_dir:
	xfs_da_unmount(mp);
 out_remove_uuid:
	xfs_uuid_unmount(mp);
 out_remove_errortag:
	xfs_errortag_del(mp);
 out_remove_error_sysfs:
	xfs_error_sysfs_del(mp);
 out_remove_scrub_stats:
	xchk_stats_unregister(mp->m_scrub_stats);
	xfs_sysfs_del(&mp->m_stats.xs_kobj);
 out_remove_sysfs:
	xfs_sysfs_del(&mp->m_kobj);
 out:
	return error;
}

 
void
xfs_unmountfs(
	struct xfs_mount	*mp)
{
	uint64_t		resblks;
	int			error;

	 
	xfs_inodegc_flush(mp);

	xfs_blockgc_stop(mp);
	xfs_fs_unreserve_ag_blocks(mp);
	xfs_qm_unmount_quotas(mp);
	xfs_rtunmount_inodes(mp);
	xfs_irele(mp->m_rootip);

	xfs_unmount_flush_inodes(mp);

	xfs_qm_unmount(mp);

	 
	resblks = 0;
	error = xfs_reserve_blocks(mp, &resblks, NULL);
	if (error)
		xfs_warn(mp, "Unable to free reserved block pool. "
				"Freespace may not be correct on next mount.");
	xfs_unmount_check(mp);

	xfs_log_unmount(mp);
	xfs_da_unmount(mp);
	xfs_uuid_unmount(mp);

#if defined(DEBUG)
	xfs_errortag_clearall(mp);
#endif
	unregister_shrinker(&mp->m_inodegc_shrinker);
	xfs_free_perag(mp);

	xfs_errortag_del(mp);
	xfs_error_sysfs_del(mp);
	xchk_stats_unregister(mp->m_scrub_stats);
	xfs_sysfs_del(&mp->m_stats.xs_kobj);
	xfs_sysfs_del(&mp->m_kobj);
}

 
bool
xfs_fs_writable(
	struct xfs_mount	*mp,
	int			level)
{
	ASSERT(level > SB_UNFROZEN);
	if ((mp->m_super->s_writers.frozen >= level) ||
	    xfs_is_shutdown(mp) || xfs_is_readonly(mp))
		return false;

	return true;
}

 
int
xfs_mod_freecounter(
	struct xfs_mount	*mp,
	struct percpu_counter	*counter,
	int64_t			delta,
	bool			rsvd)
{
	int64_t			lcounter;
	long long		res_used;
	uint64_t		set_aside = 0;
	s32			batch;
	bool			has_resv_pool;

	ASSERT(counter == &mp->m_fdblocks || counter == &mp->m_frextents);
	has_resv_pool = (counter == &mp->m_fdblocks);
	if (rsvd)
		ASSERT(has_resv_pool);

	if (delta > 0) {
		 
		if (likely(!has_resv_pool ||
			   mp->m_resblks == mp->m_resblks_avail)) {
			percpu_counter_add(counter, delta);
			return 0;
		}

		spin_lock(&mp->m_sb_lock);
		res_used = (long long)(mp->m_resblks - mp->m_resblks_avail);

		if (res_used > delta) {
			mp->m_resblks_avail += delta;
		} else {
			delta -= res_used;
			mp->m_resblks_avail = mp->m_resblks;
			percpu_counter_add(counter, delta);
		}
		spin_unlock(&mp->m_sb_lock);
		return 0;
	}

	 
	if (__percpu_counter_compare(counter, 2 * XFS_FDBLOCKS_BATCH,
				     XFS_FDBLOCKS_BATCH) < 0)
		batch = 1;
	else
		batch = XFS_FDBLOCKS_BATCH;

	 
	if (has_resv_pool)
		set_aside = xfs_fdblocks_unavailable(mp);
	percpu_counter_add_batch(counter, delta, batch);
	if (__percpu_counter_compare(counter, set_aside,
				     XFS_FDBLOCKS_BATCH) >= 0) {
		 
		return 0;
	}

	 
	spin_lock(&mp->m_sb_lock);
	percpu_counter_add(counter, -delta);
	if (!has_resv_pool || !rsvd)
		goto fdblocks_enospc;

	lcounter = (long long)mp->m_resblks_avail + delta;
	if (lcounter >= 0) {
		mp->m_resblks_avail = lcounter;
		spin_unlock(&mp->m_sb_lock);
		return 0;
	}
	xfs_warn_once(mp,
"Reserve blocks depleted! Consider increasing reserve pool size.");

fdblocks_enospc:
	spin_unlock(&mp->m_sb_lock);
	return -ENOSPC;
}

 
void
xfs_freesb(
	struct xfs_mount	*mp)
{
	struct xfs_buf		*bp = mp->m_sb_bp;

	xfs_buf_lock(bp);
	mp->m_sb_bp = NULL;
	xfs_buf_relse(bp);
}

 
int
xfs_dev_is_read_only(
	struct xfs_mount	*mp,
	char			*message)
{
	if (xfs_readonly_buftarg(mp->m_ddev_targp) ||
	    xfs_readonly_buftarg(mp->m_logdev_targp) ||
	    (mp->m_rtdev_targp && xfs_readonly_buftarg(mp->m_rtdev_targp))) {
		xfs_notice(mp, "%s required on read-only device.", message);
		xfs_notice(mp, "write access unavailable, cannot proceed.");
		return -EROFS;
	}
	return 0;
}

 
void
xfs_force_summary_recalc(
	struct xfs_mount	*mp)
{
	if (!xfs_has_lazysbcount(mp))
		return;

	xfs_fs_mark_sick(mp, XFS_SICK_FS_COUNTERS);
}

 
int
xfs_add_incompat_log_feature(
	struct xfs_mount	*mp,
	uint32_t		feature)
{
	struct xfs_dsb		*dsb;
	int			error;

	ASSERT(hweight32(feature) == 1);
	ASSERT(!(feature & XFS_SB_FEAT_INCOMPAT_LOG_UNKNOWN));

	 
	error = xfs_log_force(mp, XFS_LOG_SYNC);
	if (error)
		return error;
	xfs_ail_push_all(mp->m_ail);

	 
	xfs_buf_lock(mp->m_sb_bp);
	xfs_buf_hold(mp->m_sb_bp);

	if (xfs_is_shutdown(mp)) {
		error = -EIO;
		goto rele;
	}

	if (xfs_sb_has_incompat_log_feature(&mp->m_sb, feature))
		goto rele;

	 
	dsb = mp->m_sb_bp->b_addr;
	xfs_sb_to_disk(dsb, &mp->m_sb);
	dsb->sb_features_log_incompat |= cpu_to_be32(feature);
	error = xfs_bwrite(mp->m_sb_bp);
	if (error)
		goto shutdown;

	 
	xfs_sb_add_incompat_log_features(&mp->m_sb, feature);
	xfs_buf_relse(mp->m_sb_bp);

	 
	return xfs_sync_sb(mp, false);
shutdown:
	xfs_force_shutdown(mp, SHUTDOWN_META_IO_ERROR);
rele:
	xfs_buf_relse(mp->m_sb_bp);
	return error;
}

 
bool
xfs_clear_incompat_log_features(
	struct xfs_mount	*mp)
{
	bool			ret = false;

	if (!xfs_has_crc(mp) ||
	    !xfs_sb_has_incompat_log_feature(&mp->m_sb,
				XFS_SB_FEAT_INCOMPAT_LOG_ALL) ||
	    xfs_is_shutdown(mp))
		return false;

	 
	xfs_buf_lock(mp->m_sb_bp);
	xfs_buf_hold(mp->m_sb_bp);

	if (xfs_sb_has_incompat_log_feature(&mp->m_sb,
				XFS_SB_FEAT_INCOMPAT_LOG_ALL)) {
		xfs_sb_remove_incompat_log_features(&mp->m_sb);
		ret = true;
	}

	xfs_buf_relse(mp->m_sb_bp);
	return ret;
}

 
#define XFS_DELALLOC_BATCH	(4096)
void
xfs_mod_delalloc(
	struct xfs_mount	*mp,
	int64_t			delta)
{
	percpu_counter_add_batch(&mp->m_delalloc_blks, delta,
			XFS_DELALLOC_BATCH);
}
