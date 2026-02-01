
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_btree.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_ag.h"
#include "xfs_health.h"
#include "scrub/scrub.h"
#include "scrub/health.h"

 

 

enum xchk_health_group {
	XHG_FS = 1,
	XHG_RT,
	XHG_AG,
	XHG_INO,
};

struct xchk_health_map {
	enum xchk_health_group	group;
	unsigned int		sick_mask;
};

static const struct xchk_health_map type_to_health_flag[XFS_SCRUB_TYPE_NR] = {
	[XFS_SCRUB_TYPE_SB]		= { XHG_AG,  XFS_SICK_AG_SB },
	[XFS_SCRUB_TYPE_AGF]		= { XHG_AG,  XFS_SICK_AG_AGF },
	[XFS_SCRUB_TYPE_AGFL]		= { XHG_AG,  XFS_SICK_AG_AGFL },
	[XFS_SCRUB_TYPE_AGI]		= { XHG_AG,  XFS_SICK_AG_AGI },
	[XFS_SCRUB_TYPE_BNOBT]		= { XHG_AG,  XFS_SICK_AG_BNOBT },
	[XFS_SCRUB_TYPE_CNTBT]		= { XHG_AG,  XFS_SICK_AG_CNTBT },
	[XFS_SCRUB_TYPE_INOBT]		= { XHG_AG,  XFS_SICK_AG_INOBT },
	[XFS_SCRUB_TYPE_FINOBT]		= { XHG_AG,  XFS_SICK_AG_FINOBT },
	[XFS_SCRUB_TYPE_RMAPBT]		= { XHG_AG,  XFS_SICK_AG_RMAPBT },
	[XFS_SCRUB_TYPE_REFCNTBT]	= { XHG_AG,  XFS_SICK_AG_REFCNTBT },
	[XFS_SCRUB_TYPE_INODE]		= { XHG_INO, XFS_SICK_INO_CORE },
	[XFS_SCRUB_TYPE_BMBTD]		= { XHG_INO, XFS_SICK_INO_BMBTD },
	[XFS_SCRUB_TYPE_BMBTA]		= { XHG_INO, XFS_SICK_INO_BMBTA },
	[XFS_SCRUB_TYPE_BMBTC]		= { XHG_INO, XFS_SICK_INO_BMBTC },
	[XFS_SCRUB_TYPE_DIR]		= { XHG_INO, XFS_SICK_INO_DIR },
	[XFS_SCRUB_TYPE_XATTR]		= { XHG_INO, XFS_SICK_INO_XATTR },
	[XFS_SCRUB_TYPE_SYMLINK]	= { XHG_INO, XFS_SICK_INO_SYMLINK },
	[XFS_SCRUB_TYPE_PARENT]		= { XHG_INO, XFS_SICK_INO_PARENT },
	[XFS_SCRUB_TYPE_RTBITMAP]	= { XHG_RT,  XFS_SICK_RT_BITMAP },
	[XFS_SCRUB_TYPE_RTSUM]		= { XHG_RT,  XFS_SICK_RT_SUMMARY },
	[XFS_SCRUB_TYPE_UQUOTA]		= { XHG_FS,  XFS_SICK_FS_UQUOTA },
	[XFS_SCRUB_TYPE_GQUOTA]		= { XHG_FS,  XFS_SICK_FS_GQUOTA },
	[XFS_SCRUB_TYPE_PQUOTA]		= { XHG_FS,  XFS_SICK_FS_PQUOTA },
	[XFS_SCRUB_TYPE_FSCOUNTERS]	= { XHG_FS,  XFS_SICK_FS_COUNTERS },
};

 
unsigned int
xchk_health_mask_for_scrub_type(
	__u32			scrub_type)
{
	return type_to_health_flag[scrub_type].sick_mask;
}

 
void
xchk_update_health(
	struct xfs_scrub	*sc)
{
	struct xfs_perag	*pag;
	bool			bad;

	if (!sc->sick_mask)
		return;

	bad = (sc->sm->sm_flags & (XFS_SCRUB_OFLAG_CORRUPT |
				   XFS_SCRUB_OFLAG_XCORRUPT));
	switch (type_to_health_flag[sc->sm->sm_type].group) {
	case XHG_AG:
		pag = xfs_perag_get(sc->mp, sc->sm->sm_agno);
		if (bad)
			xfs_ag_mark_sick(pag, sc->sick_mask);
		else
			xfs_ag_mark_healthy(pag, sc->sick_mask);
		xfs_perag_put(pag);
		break;
	case XHG_INO:
		if (!sc->ip)
			return;
		if (bad)
			xfs_inode_mark_sick(sc->ip, sc->sick_mask);
		else
			xfs_inode_mark_healthy(sc->ip, sc->sick_mask);
		break;
	case XHG_FS:
		if (bad)
			xfs_fs_mark_sick(sc->mp, sc->sick_mask);
		else
			xfs_fs_mark_healthy(sc->mp, sc->sick_mask);
		break;
	case XHG_RT:
		if (bad)
			xfs_rt_mark_sick(sc->mp, sc->sick_mask);
		else
			xfs_rt_mark_healthy(sc->mp, sc->sick_mask);
		break;
	default:
		ASSERT(0);
		break;
	}
}

 
bool
xchk_ag_btree_healthy_enough(
	struct xfs_scrub	*sc,
	struct xfs_perag	*pag,
	xfs_btnum_t		btnum)
{
	unsigned int		mask = 0;

	 
	switch (btnum) {
	case XFS_BTNUM_BNO:
		if (sc->sm->sm_type == XFS_SCRUB_TYPE_BNOBT)
			return true;
		mask = XFS_SICK_AG_BNOBT;
		break;
	case XFS_BTNUM_CNT:
		if (sc->sm->sm_type == XFS_SCRUB_TYPE_CNTBT)
			return true;
		mask = XFS_SICK_AG_CNTBT;
		break;
	case XFS_BTNUM_INO:
		if (sc->sm->sm_type == XFS_SCRUB_TYPE_INOBT)
			return true;
		mask = XFS_SICK_AG_INOBT;
		break;
	case XFS_BTNUM_FINO:
		if (sc->sm->sm_type == XFS_SCRUB_TYPE_FINOBT)
			return true;
		mask = XFS_SICK_AG_FINOBT;
		break;
	case XFS_BTNUM_RMAP:
		if (sc->sm->sm_type == XFS_SCRUB_TYPE_RMAPBT)
			return true;
		mask = XFS_SICK_AG_RMAPBT;
		break;
	case XFS_BTNUM_REFC:
		if (sc->sm->sm_type == XFS_SCRUB_TYPE_REFCNTBT)
			return true;
		mask = XFS_SICK_AG_REFCNTBT;
		break;
	default:
		ASSERT(0);
		return true;
	}

	 
	if ((sc->flags & XREP_ALREADY_FIXED) &&
	    type_to_health_flag[sc->sm->sm_type].group == XHG_AG)
		mask &= ~sc->sick_mask;

	if (xfs_ag_has_sickness(pag, mask)) {
		sc->sm->sm_flags |= XFS_SCRUB_OFLAG_XFAIL;
		return false;
	}

	return true;
}
