
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_log_format.h"
#include "xfs_trans.h"
#include "xfs_inode.h"
#include "xfs_quota.h"
#include "xfs_qm.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_scrub.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/trace.h"
#include "scrub/repair.h"
#include "scrub/health.h"
#include "scrub/stats.h"
#include "scrub/xfile.h"

 

 
static int
xchk_probe(
	struct xfs_scrub	*sc)
{
	int			error = 0;

	if (xchk_should_terminate(sc, &error))
		return error;

	return 0;
}

 

static inline void
xchk_fsgates_disable(
	struct xfs_scrub	*sc)
{
	if (!(sc->flags & XCHK_FSGATES_ALL))
		return;

	trace_xchk_fsgates_disable(sc, sc->flags & XCHK_FSGATES_ALL);

	if (sc->flags & XCHK_FSGATES_DRAIN)
		xfs_drain_wait_disable();

	sc->flags &= ~XCHK_FSGATES_ALL;
}

 
STATIC int
xchk_teardown(
	struct xfs_scrub	*sc,
	int			error)
{
	xchk_ag_free(sc, &sc->sa);
	if (sc->tp) {
		if (error == 0 && (sc->sm->sm_flags & XFS_SCRUB_IFLAG_REPAIR))
			error = xfs_trans_commit(sc->tp);
		else
			xfs_trans_cancel(sc->tp);
		sc->tp = NULL;
	}
	if (sc->ip) {
		if (sc->ilock_flags)
			xchk_iunlock(sc, sc->ilock_flags);
		xchk_irele(sc, sc->ip);
		sc->ip = NULL;
	}
	if (sc->flags & XCHK_HAVE_FREEZE_PROT) {
		sc->flags &= ~XCHK_HAVE_FREEZE_PROT;
		mnt_drop_write_file(sc->file);
	}
	if (sc->xfile) {
		xfile_destroy(sc->xfile);
		sc->xfile = NULL;
	}
	if (sc->buf) {
		if (sc->buf_cleanup)
			sc->buf_cleanup(sc->buf);
		kvfree(sc->buf);
		sc->buf_cleanup = NULL;
		sc->buf = NULL;
	}

	xchk_fsgates_disable(sc);
	return error;
}

 

static const struct xchk_meta_ops meta_scrub_ops[] = {
	[XFS_SCRUB_TYPE_PROBE] = {	 
		.type	= ST_NONE,
		.setup	= xchk_setup_fs,
		.scrub	= xchk_probe,
		.repair = xrep_probe,
	},
	[XFS_SCRUB_TYPE_SB] = {		 
		.type	= ST_PERAG,
		.setup	= xchk_setup_agheader,
		.scrub	= xchk_superblock,
		.repair	= xrep_superblock,
	},
	[XFS_SCRUB_TYPE_AGF] = {	 
		.type	= ST_PERAG,
		.setup	= xchk_setup_agheader,
		.scrub	= xchk_agf,
		.repair	= xrep_agf,
	},
	[XFS_SCRUB_TYPE_AGFL]= {	 
		.type	= ST_PERAG,
		.setup	= xchk_setup_agheader,
		.scrub	= xchk_agfl,
		.repair	= xrep_agfl,
	},
	[XFS_SCRUB_TYPE_AGI] = {	 
		.type	= ST_PERAG,
		.setup	= xchk_setup_agheader,
		.scrub	= xchk_agi,
		.repair	= xrep_agi,
	},
	[XFS_SCRUB_TYPE_BNOBT] = {	 
		.type	= ST_PERAG,
		.setup	= xchk_setup_ag_allocbt,
		.scrub	= xchk_bnobt,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_CNTBT] = {	 
		.type	= ST_PERAG,
		.setup	= xchk_setup_ag_allocbt,
		.scrub	= xchk_cntbt,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_INOBT] = {	 
		.type	= ST_PERAG,
		.setup	= xchk_setup_ag_iallocbt,
		.scrub	= xchk_inobt,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_FINOBT] = {	 
		.type	= ST_PERAG,
		.setup	= xchk_setup_ag_iallocbt,
		.scrub	= xchk_finobt,
		.has	= xfs_has_finobt,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_RMAPBT] = {	 
		.type	= ST_PERAG,
		.setup	= xchk_setup_ag_rmapbt,
		.scrub	= xchk_rmapbt,
		.has	= xfs_has_rmapbt,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_REFCNTBT] = {	 
		.type	= ST_PERAG,
		.setup	= xchk_setup_ag_refcountbt,
		.scrub	= xchk_refcountbt,
		.has	= xfs_has_reflink,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_INODE] = {	 
		.type	= ST_INODE,
		.setup	= xchk_setup_inode,
		.scrub	= xchk_inode,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_BMBTD] = {	 
		.type	= ST_INODE,
		.setup	= xchk_setup_inode_bmap,
		.scrub	= xchk_bmap_data,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_BMBTA] = {	 
		.type	= ST_INODE,
		.setup	= xchk_setup_inode_bmap,
		.scrub	= xchk_bmap_attr,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_BMBTC] = {	 
		.type	= ST_INODE,
		.setup	= xchk_setup_inode_bmap,
		.scrub	= xchk_bmap_cow,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_DIR] = {	 
		.type	= ST_INODE,
		.setup	= xchk_setup_directory,
		.scrub	= xchk_directory,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_XATTR] = {	 
		.type	= ST_INODE,
		.setup	= xchk_setup_xattr,
		.scrub	= xchk_xattr,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_SYMLINK] = {	 
		.type	= ST_INODE,
		.setup	= xchk_setup_symlink,
		.scrub	= xchk_symlink,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_PARENT] = {	 
		.type	= ST_INODE,
		.setup	= xchk_setup_parent,
		.scrub	= xchk_parent,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_RTBITMAP] = {	 
		.type	= ST_FS,
		.setup	= xchk_setup_rtbitmap,
		.scrub	= xchk_rtbitmap,
		.has	= xfs_has_realtime,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_RTSUM] = {	 
		.type	= ST_FS,
		.setup	= xchk_setup_rtsummary,
		.scrub	= xchk_rtsummary,
		.has	= xfs_has_realtime,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_UQUOTA] = {	 
		.type	= ST_FS,
		.setup	= xchk_setup_quota,
		.scrub	= xchk_quota,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_GQUOTA] = {	 
		.type	= ST_FS,
		.setup	= xchk_setup_quota,
		.scrub	= xchk_quota,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_PQUOTA] = {	 
		.type	= ST_FS,
		.setup	= xchk_setup_quota,
		.scrub	= xchk_quota,
		.repair	= xrep_notsupported,
	},
	[XFS_SCRUB_TYPE_FSCOUNTERS] = {	 
		.type	= ST_FS,
		.setup	= xchk_setup_fscounters,
		.scrub	= xchk_fscounters,
		.repair	= xrep_notsupported,
	},
};

static int
xchk_validate_inputs(
	struct xfs_mount		*mp,
	struct xfs_scrub_metadata	*sm)
{
	int				error;
	const struct xchk_meta_ops	*ops;

	error = -EINVAL;
	 
	sm->sm_flags &= ~XFS_SCRUB_FLAGS_OUT;
	if (sm->sm_flags & ~XFS_SCRUB_FLAGS_IN)
		goto out;
	 
	if (memchr_inv(sm->sm_reserved, 0, sizeof(sm->sm_reserved)))
		goto out;

	error = -ENOENT;
	 
	if (sm->sm_type >= XFS_SCRUB_TYPE_NR)
		goto out;
	ops = &meta_scrub_ops[sm->sm_type];
	if (ops->setup == NULL || ops->scrub == NULL)
		goto out;
	 
	if (ops->has && !ops->has(mp))
		goto out;

	error = -EINVAL;
	 
	switch (ops->type) {
	case ST_NONE:
	case ST_FS:
		if (sm->sm_ino || sm->sm_gen || sm->sm_agno)
			goto out;
		break;
	case ST_PERAG:
		if (sm->sm_ino || sm->sm_gen ||
		    sm->sm_agno >= mp->m_sb.sb_agcount)
			goto out;
		break;
	case ST_INODE:
		if (sm->sm_agno || (sm->sm_gen && !sm->sm_ino))
			goto out;
		break;
	default:
		goto out;
	}

	 
	if ((sm->sm_flags & XFS_SCRUB_IFLAG_FORCE_REBUILD) &&
	    !(sm->sm_flags & XFS_SCRUB_IFLAG_REPAIR))
		return -EINVAL;

	 
	if (sm->sm_flags & XFS_SCRUB_IFLAG_REPAIR) {
		error = -EOPNOTSUPP;
		if (!xfs_has_crc(mp))
			goto out;

		error = -EROFS;
		if (xfs_is_readonly(mp))
			goto out;
	}

	error = 0;
out:
	return error;
}

#ifdef CONFIG_XFS_ONLINE_REPAIR
static inline void xchk_postmortem(struct xfs_scrub *sc)
{
	 
	if ((sc->sm->sm_flags & XFS_SCRUB_IFLAG_REPAIR) &&
	    (sc->sm->sm_flags & (XFS_SCRUB_OFLAG_CORRUPT |
				 XFS_SCRUB_OFLAG_XCORRUPT)))
		xrep_failure(sc->mp);
}
#else
static inline void xchk_postmortem(struct xfs_scrub *sc)
{
	 
	if (sc->sm->sm_flags & (XFS_SCRUB_OFLAG_CORRUPT |
				XFS_SCRUB_OFLAG_XCORRUPT))
		xfs_alert_ratelimited(sc->mp,
				"Corruption detected during scrub.");
}
#endif  

 
int
xfs_scrub_metadata(
	struct file			*file,
	struct xfs_scrub_metadata	*sm)
{
	struct xchk_stats_run		run = { };
	struct xfs_scrub		*sc;
	struct xfs_mount		*mp = XFS_I(file_inode(file))->i_mount;
	u64				check_start;
	int				error = 0;

	BUILD_BUG_ON(sizeof(meta_scrub_ops) !=
		(sizeof(struct xchk_meta_ops) * XFS_SCRUB_TYPE_NR));

	trace_xchk_start(XFS_I(file_inode(file)), sm, error);

	 
	error = -ESHUTDOWN;
	if (xfs_is_shutdown(mp))
		goto out;
	error = -ENOTRECOVERABLE;
	if (xfs_has_norecovery(mp))
		goto out;

	error = xchk_validate_inputs(mp, sm);
	if (error)
		goto out;

	xfs_warn_mount(mp, XFS_OPSTATE_WARNED_SCRUB,
 "EXPERIMENTAL online scrub feature in use. Use at your own risk!");

	sc = kzalloc(sizeof(struct xfs_scrub), XCHK_GFP_FLAGS);
	if (!sc) {
		error = -ENOMEM;
		goto out;
	}

	sc->mp = mp;
	sc->file = file;
	sc->sm = sm;
	sc->ops = &meta_scrub_ops[sm->sm_type];
	sc->sick_mask = xchk_health_mask_for_scrub_type(sm->sm_type);
retry_op:
	 
	if (sm->sm_flags & XFS_SCRUB_IFLAG_REPAIR) {
		error = mnt_want_write_file(sc->file);
		if (error)
			goto out_sc;

		sc->flags |= XCHK_HAVE_FREEZE_PROT;
	}

	 
	error = sc->ops->setup(sc);
	if (error == -EDEADLOCK && !(sc->flags & XCHK_TRY_HARDER))
		goto try_harder;
	if (error == -ECHRNG && !(sc->flags & XCHK_NEED_DRAIN))
		goto need_drain;
	if (error)
		goto out_teardown;

	 
	check_start = xchk_stats_now();
	error = sc->ops->scrub(sc);
	run.scrub_ns += xchk_stats_elapsed_ns(check_start);
	if (error == -EDEADLOCK && !(sc->flags & XCHK_TRY_HARDER))
		goto try_harder;
	if (error == -ECHRNG && !(sc->flags & XCHK_NEED_DRAIN))
		goto need_drain;
	if (error || (sm->sm_flags & XFS_SCRUB_OFLAG_INCOMPLETE))
		goto out_teardown;

	xchk_update_health(sc);

	if ((sc->sm->sm_flags & XFS_SCRUB_IFLAG_REPAIR) &&
	    !(sc->flags & XREP_ALREADY_FIXED)) {
		bool needs_fix = xchk_needs_repair(sc->sm);

		 
		if (sc->sm->sm_flags & XFS_SCRUB_IFLAG_FORCE_REBUILD)
			needs_fix = true;

		 
		if (XFS_TEST_ERROR(needs_fix, mp, XFS_ERRTAG_FORCE_SCRUB_REPAIR))
			needs_fix = true;

		 
		if (!needs_fix) {
			sc->sm->sm_flags |= XFS_SCRUB_OFLAG_NO_REPAIR_NEEDED;
			goto out_nofix;
		}

		 
		error = xrep_attempt(sc, &run);
		if (error == -EAGAIN) {
			 
			error = xchk_teardown(sc, 0);
			if (error) {
				xrep_failure(mp);
				goto out_sc;
			}
			goto retry_op;
		}
	}

out_nofix:
	xchk_postmortem(sc);
out_teardown:
	error = xchk_teardown(sc, error);
out_sc:
	if (error != -ENOENT)
		xchk_stats_merge(mp, sm, &run);
	kfree(sc);
out:
	trace_xchk_done(XFS_I(file_inode(file)), sm, error);
	if (error == -EFSCORRUPTED || error == -EFSBADCRC) {
		sm->sm_flags |= XFS_SCRUB_OFLAG_CORRUPT;
		error = 0;
	}
	return error;
need_drain:
	error = xchk_teardown(sc, 0);
	if (error)
		goto out_sc;
	sc->flags |= XCHK_NEED_DRAIN;
	run.retries++;
	goto retry_op;
try_harder:
	 
	error = xchk_teardown(sc, 0);
	if (error)
		goto out_sc;
	sc->flags |= XCHK_TRY_HARDER;
	run.retries++;
	goto retry_op;
}
