
 
#ifndef __XFS_SCRUB_HEALTH_H__
#define __XFS_SCRUB_HEALTH_H__

unsigned int xchk_health_mask_for_scrub_type(__u32 scrub_type);
void xchk_update_health(struct xfs_scrub *sc);
bool xchk_ag_btree_healthy_enough(struct xfs_scrub *sc, struct xfs_perag *pag,
		xfs_btnum_t btnum);

#endif  
