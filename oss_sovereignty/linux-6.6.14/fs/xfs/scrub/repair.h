#ifndef __XFS_SCRUB_REPAIR_H__
#define __XFS_SCRUB_REPAIR_H__
#include "xfs_quota_defs.h"
struct xchk_stats_run;
static inline int xrep_notsupported(struct xfs_scrub *sc)
{
	return -EOPNOTSUPP;
}
#ifdef CONFIG_XFS_ONLINE_REPAIR
#define XREP_MAX_ITRUNCATE_EFIS	(128)
int xrep_attempt(struct xfs_scrub *sc, struct xchk_stats_run *run);
void xrep_failure(struct xfs_mount *mp);
int xrep_roll_ag_trans(struct xfs_scrub *sc);
int xrep_defer_finish(struct xfs_scrub *sc);
bool xrep_ag_has_space(struct xfs_perag *pag, xfs_extlen_t nr_blocks,
		enum xfs_ag_resv_type type);
xfs_extlen_t xrep_calc_ag_resblks(struct xfs_scrub *sc);
struct xbitmap;
struct xagb_bitmap;
int xrep_fix_freelist(struct xfs_scrub *sc, bool can_shrink);
struct xrep_find_ag_btree {
	uint64_t			rmap_owner;
	const struct xfs_buf_ops	*buf_ops;
	unsigned int			maxlevels;
	xfs_agblock_t			root;
	unsigned int			height;
};
int xrep_find_ag_btree_roots(struct xfs_scrub *sc, struct xfs_buf *agf_bp,
		struct xrep_find_ag_btree *btree_info, struct xfs_buf *agfl_bp);
void xrep_force_quotacheck(struct xfs_scrub *sc, xfs_dqtype_t type);
int xrep_ino_dqattach(struct xfs_scrub *sc);
int xrep_probe(struct xfs_scrub *sc);
int xrep_superblock(struct xfs_scrub *sc);
int xrep_agf(struct xfs_scrub *sc);
int xrep_agfl(struct xfs_scrub *sc);
int xrep_agi(struct xfs_scrub *sc);
#else
static inline int
xrep_attempt(
	struct xfs_scrub	*sc,
	struct xchk_stats_run	*run)
{
	return -EOPNOTSUPP;
}
static inline void xrep_failure(struct xfs_mount *mp) {}
static inline xfs_extlen_t
xrep_calc_ag_resblks(
	struct xfs_scrub	*sc)
{
	return 0;
}
#define xrep_probe			xrep_notsupported
#define xrep_superblock			xrep_notsupported
#define xrep_agf			xrep_notsupported
#define xrep_agfl			xrep_notsupported
#define xrep_agi			xrep_notsupported
#endif  
#endif	 
