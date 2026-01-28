#ifndef __XFS_SCRUB_REAP_H__
#define __XFS_SCRUB_REAP_H__
int xrep_reap_agblocks(struct xfs_scrub *sc, struct xagb_bitmap *bitmap,
		const struct xfs_owner_info *oinfo, enum xfs_ag_resv_type type);
#endif  
