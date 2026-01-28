

#ifndef __XFS_ITABLE_H__
#define	__XFS_ITABLE_H__


struct xfs_ibulk {
	struct xfs_mount	*mp;
	struct mnt_idmap	*idmap;
	void __user		*ubuffer; 
	xfs_ino_t		startino; 
	unsigned int		icount;   
	unsigned int		ocount;   
	unsigned int		flags;    
};


#define XFS_IBULK_SAME_AG	(1U << 0)


#define XFS_IBULK_NREXT64	(1U << 1)


static inline int
xfs_ibulk_advance(
	struct xfs_ibulk	*breq,
	size_t			bytes)
{
	char __user		*b = breq->ubuffer;

	breq->ubuffer = b + bytes;
	breq->ocount++;
	return breq->ocount == breq->icount ? -ECANCELED : 0;
}





typedef int (*bulkstat_one_fmt_pf)(struct xfs_ibulk *breq,
		const struct xfs_bulkstat *bstat);

int xfs_bulkstat_one(struct xfs_ibulk *breq, bulkstat_one_fmt_pf formatter);
int xfs_bulkstat(struct xfs_ibulk *breq, bulkstat_one_fmt_pf formatter);
void xfs_bulkstat_to_bstat(struct xfs_mount *mp, struct xfs_bstat *bs1,
		const struct xfs_bulkstat *bstat);

typedef int (*inumbers_fmt_pf)(struct xfs_ibulk *breq,
		const struct xfs_inumbers *igrp);

int xfs_inumbers(struct xfs_ibulk *breq, inumbers_fmt_pf formatter);
void xfs_inumbers_to_inogrp(struct xfs_inogrp *ig1,
		const struct xfs_inumbers *ig);

#endif	
