
 
#ifndef __XFS_DQUOT_ITEM_H__
#define __XFS_DQUOT_ITEM_H__

struct xfs_dquot;
struct xfs_trans;
struct xfs_mount;

struct xfs_dq_logitem {
	struct xfs_log_item	qli_item;	 
	struct xfs_dquot	*qli_dquot;	 
	xfs_lsn_t		qli_flush_lsn;	 
};

void xfs_qm_dquot_logitem_init(struct xfs_dquot *dqp);

#endif	 
