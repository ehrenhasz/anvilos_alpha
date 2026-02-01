
 
#ifndef	__XFS_TRANS_RESV_H__
#define	__XFS_TRANS_RESV_H__

struct xfs_mount;

 
struct xfs_trans_res {
	uint	tr_logres;	 
	int	tr_logcount;	 
	int	tr_logflags;	 
};

struct xfs_trans_resv {
	struct xfs_trans_res	tr_write;	 
	struct xfs_trans_res	tr_itruncate;	 
	struct xfs_trans_res	tr_rename;	 
	struct xfs_trans_res	tr_link;	 
	struct xfs_trans_res	tr_remove;	 
	struct xfs_trans_res	tr_symlink;	 
	struct xfs_trans_res	tr_create;	 
	struct xfs_trans_res	tr_create_tmpfile;  
	struct xfs_trans_res	tr_mkdir;	 
	struct xfs_trans_res	tr_ifree;	 
	struct xfs_trans_res	tr_ichange;	 
	struct xfs_trans_res	tr_growdata;	 
	struct xfs_trans_res	tr_addafork;	 
	struct xfs_trans_res	tr_writeid;	 
	struct xfs_trans_res	tr_attrinval;	 
	struct xfs_trans_res	tr_attrsetm;	 
	struct xfs_trans_res	tr_attrsetrt;	 
	struct xfs_trans_res	tr_attrrm;	 
	struct xfs_trans_res	tr_clearagi;	 
	struct xfs_trans_res	tr_growrtalloc;	 
	struct xfs_trans_res	tr_growrtzero;	 
	struct xfs_trans_res	tr_growrtfree;	 
	struct xfs_trans_res	tr_qm_setqlim;	 
	struct xfs_trans_res	tr_qm_dqalloc;	 
	struct xfs_trans_res	tr_sb;		 
	struct xfs_trans_res	tr_fsyncts;	 
};

 
#define M_RES(mp)	(&(mp)->m_resv)

 
#define	XFS_DIROP_LOG_RES(mp)	\
	(XFS_FSB_TO_B(mp, XFS_DAENTER_BLOCKS(mp, XFS_DATA_FORK)) + \
	 (XFS_FSB_TO_B(mp, XFS_DAENTER_BMAPS(mp, XFS_DATA_FORK) + 1)))
#define	XFS_DIROP_LOG_COUNT(mp)	\
	(XFS_DAENTER_BLOCKS(mp, XFS_DATA_FORK) + \
	 XFS_DAENTER_BMAPS(mp, XFS_DATA_FORK) + 1)

 
#define	XFS_DEFAULT_LOG_COUNT		1
#define	XFS_DEFAULT_PERM_LOG_COUNT	2
#define	XFS_ITRUNCATE_LOG_COUNT		2
#define XFS_INACTIVE_LOG_COUNT		2
#define	XFS_CREATE_LOG_COUNT		2
#define	XFS_CREATE_TMPFILE_LOG_COUNT	2
#define	XFS_MKDIR_LOG_COUNT		3
#define	XFS_SYMLINK_LOG_COUNT		3
#define	XFS_REMOVE_LOG_COUNT		2
#define	XFS_LINK_LOG_COUNT		2
#define	XFS_RENAME_LOG_COUNT		2
#define	XFS_WRITE_LOG_COUNT		2
#define	XFS_ADDAFORK_LOG_COUNT		2
#define	XFS_ATTRINVAL_LOG_COUNT		1
#define	XFS_ATTRSET_LOG_COUNT		3
#define	XFS_ATTRRM_LOG_COUNT		3

 
#define	XFS_ITRUNCATE_LOG_COUNT_REFLINK	8
#define	XFS_WRITE_LOG_COUNT_REFLINK	8

void xfs_trans_resv_calc(struct xfs_mount *mp, struct xfs_trans_resv *resp);
uint xfs_allocfree_block_count(struct xfs_mount *mp, uint num_ops);

unsigned int xfs_calc_itruncate_reservation_minlogsize(struct xfs_mount *mp);
unsigned int xfs_calc_write_reservation_minlogsize(struct xfs_mount *mp);
unsigned int xfs_calc_qm_dqalloc_reservation_minlogsize(struct xfs_mount *mp);

#endif	 
