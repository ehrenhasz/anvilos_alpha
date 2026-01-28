

#ifndef __XFS_QUOTA_DEFS_H__
#define __XFS_QUOTA_DEFS_H__




typedef uint64_t	xfs_qcnt_t;

typedef uint8_t		xfs_dqtype_t;

#define XFS_DQTYPE_STRINGS \
	{ XFS_DQTYPE_USER,	"USER" }, \
	{ XFS_DQTYPE_PROJ,	"PROJ" }, \
	{ XFS_DQTYPE_GROUP,	"GROUP" }, \
	{ XFS_DQTYPE_BIGTIME,	"BIGTIME" }


#define XFS_DQFLAG_DIRTY	(1u << 0)	
#define XFS_DQFLAG_FREEING	(1u << 1)	

#define XFS_DQFLAG_STRINGS \
	{ XFS_DQFLAG_DIRTY,	"DIRTY" }, \
	{ XFS_DQFLAG_FREEING,	"FREEING" }


#define XFS_DQUOT_LOGRES(mp)	\
	((sizeof(struct xfs_dq_logformat) + sizeof(struct xfs_disk_dquot)) * 6)

#define XFS_IS_QUOTA_ON(mp)		((mp)->m_qflags & XFS_ALL_QUOTA_ACCT)
#define XFS_IS_UQUOTA_ON(mp)		((mp)->m_qflags & XFS_UQUOTA_ACCT)
#define XFS_IS_PQUOTA_ON(mp)		((mp)->m_qflags & XFS_PQUOTA_ACCT)
#define XFS_IS_GQUOTA_ON(mp)		((mp)->m_qflags & XFS_GQUOTA_ACCT)
#define XFS_IS_UQUOTA_ENFORCED(mp)	((mp)->m_qflags & XFS_UQUOTA_ENFD)
#define XFS_IS_GQUOTA_ENFORCED(mp)	((mp)->m_qflags & XFS_GQUOTA_ENFD)
#define XFS_IS_PQUOTA_ENFORCED(mp)	((mp)->m_qflags & XFS_PQUOTA_ENFD)


#define XFS_QMOPT_UQUOTA	(1u << 0) 
#define XFS_QMOPT_GQUOTA	(1u << 1) 
#define XFS_QMOPT_PQUOTA	(1u << 2) 
#define XFS_QMOPT_FORCE_RES	(1u << 3) 
#define XFS_QMOPT_SBVERSION	(1u << 4) 


#define XFS_QMOPT_RES_REGBLKS	(1u << 7)
#define XFS_QMOPT_RES_RTBLKS	(1u << 8)
#define XFS_QMOPT_BCOUNT	(1u << 9)
#define XFS_QMOPT_ICOUNT	(1u << 10)
#define XFS_QMOPT_RTBCOUNT	(1u << 11)
#define XFS_QMOPT_DELBCOUNT	(1u << 12)
#define XFS_QMOPT_DELRTBCOUNT	(1u << 13)
#define XFS_QMOPT_RES_INOS	(1u << 14)


#define XFS_QMOPT_INHERIT	(1u << 31)

#define XFS_QMOPT_FLAGS \
	{ XFS_QMOPT_UQUOTA,		"UQUOTA" }, \
	{ XFS_QMOPT_PQUOTA,		"PQUOTA" }, \
	{ XFS_QMOPT_FORCE_RES,		"FORCE_RES" }, \
	{ XFS_QMOPT_SBVERSION,		"SBVERSION" }, \
	{ XFS_QMOPT_GQUOTA,		"GQUOTA" }, \
	{ XFS_QMOPT_INHERIT,		"INHERIT" }, \
	{ XFS_QMOPT_RES_REGBLKS,	"RES_REGBLKS" }, \
	{ XFS_QMOPT_RES_RTBLKS,		"RES_RTBLKS" }, \
	{ XFS_QMOPT_BCOUNT,		"BCOUNT" }, \
	{ XFS_QMOPT_ICOUNT,		"ICOUNT" }, \
	{ XFS_QMOPT_RTBCOUNT,		"RTBCOUNT" }, \
	{ XFS_QMOPT_DELBCOUNT,		"DELBCOUNT" }, \
	{ XFS_QMOPT_DELRTBCOUNT,	"DELRTBCOUNT" }, \
	{ XFS_QMOPT_RES_INOS,		"RES_INOS" }


#define XFS_TRANS_DQ_RES_BLKS	XFS_QMOPT_RES_REGBLKS
#define XFS_TRANS_DQ_RES_RTBLKS	XFS_QMOPT_RES_RTBLKS
#define XFS_TRANS_DQ_RES_INOS	XFS_QMOPT_RES_INOS
#define XFS_TRANS_DQ_BCOUNT	XFS_QMOPT_BCOUNT
#define XFS_TRANS_DQ_DELBCOUNT	XFS_QMOPT_DELBCOUNT
#define XFS_TRANS_DQ_ICOUNT	XFS_QMOPT_ICOUNT
#define XFS_TRANS_DQ_RTBCOUNT	XFS_QMOPT_RTBCOUNT
#define XFS_TRANS_DQ_DELRTBCOUNT XFS_QMOPT_DELRTBCOUNT


#define XFS_QMOPT_QUOTALL	\
		(XFS_QMOPT_UQUOTA | XFS_QMOPT_PQUOTA | XFS_QMOPT_GQUOTA)
#define XFS_QMOPT_RESBLK_MASK	(XFS_QMOPT_RES_REGBLKS | XFS_QMOPT_RES_RTBLKS)


extern xfs_failaddr_t xfs_dquot_verify(struct xfs_mount *mp,
		struct xfs_disk_dquot *ddq, xfs_dqid_t id);
extern xfs_failaddr_t xfs_dqblk_verify(struct xfs_mount *mp,
		struct xfs_dqblk *dqb, xfs_dqid_t id);
extern int xfs_calc_dquots_per_chunk(unsigned int nbblks);
extern void xfs_dqblk_repair(struct xfs_mount *mp, struct xfs_dqblk *dqb,
		xfs_dqid_t id, xfs_dqtype_t type);

struct xfs_dquot;
time64_t xfs_dquot_from_disk_ts(struct xfs_disk_dquot *ddq,
		__be32 dtimer);
__be32 xfs_dquot_to_disk_ts(struct xfs_dquot *ddq, time64_t timer);

#endif	
