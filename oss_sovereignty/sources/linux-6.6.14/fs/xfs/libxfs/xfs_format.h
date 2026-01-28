

#ifndef __XFS_FORMAT_H__
#define __XFS_FORMAT_H__



struct xfs_mount;
struct xfs_trans;
struct xfs_inode;
struct xfs_buf;
struct xfs_ifork;


#define	XFS_SB_MAGIC		0x58465342	
#define	XFS_SB_VERSION_1	1		
#define	XFS_SB_VERSION_2	2		
#define	XFS_SB_VERSION_3	3		
#define	XFS_SB_VERSION_4	4		
#define	XFS_SB_VERSION_5	5		
#define	XFS_SB_VERSION_NUMBITS		0x000f
#define	XFS_SB_VERSION_ALLFBITS		0xfff0
#define	XFS_SB_VERSION_ATTRBIT		0x0010
#define	XFS_SB_VERSION_NLINKBIT		0x0020
#define	XFS_SB_VERSION_QUOTABIT		0x0040
#define	XFS_SB_VERSION_ALIGNBIT		0x0080
#define	XFS_SB_VERSION_DALIGNBIT	0x0100
#define	XFS_SB_VERSION_SHAREDBIT	0x0200
#define XFS_SB_VERSION_LOGV2BIT		0x0400
#define XFS_SB_VERSION_SECTORBIT	0x0800
#define	XFS_SB_VERSION_EXTFLGBIT	0x1000
#define	XFS_SB_VERSION_DIRV2BIT		0x2000
#define	XFS_SB_VERSION_BORGBIT		0x4000	
#define	XFS_SB_VERSION_MOREBITSBIT	0x8000


#define XFS_XATTR_SIZE_MAX (1 << 16)


#define	XFS_SB_VERSION_OKBITS		\
	((XFS_SB_VERSION_NUMBITS | XFS_SB_VERSION_ALLFBITS) & \
		~XFS_SB_VERSION_SHAREDBIT)


#define XFS_SB_VERSION2_RESERVED1BIT	0x00000001
#define XFS_SB_VERSION2_LAZYSBCOUNTBIT	0x00000002	
#define XFS_SB_VERSION2_RESERVED4BIT	0x00000004
#define XFS_SB_VERSION2_ATTR2BIT	0x00000008	
#define XFS_SB_VERSION2_PARENTBIT	0x00000010	
#define XFS_SB_VERSION2_PROJID32BIT	0x00000080	
#define XFS_SB_VERSION2_CRCBIT		0x00000100	
#define XFS_SB_VERSION2_FTYPE		0x00000200	

#define	XFS_SB_VERSION2_OKBITS		\
	(XFS_SB_VERSION2_LAZYSBCOUNTBIT	| \
	 XFS_SB_VERSION2_ATTR2BIT	| \
	 XFS_SB_VERSION2_PROJID32BIT	| \
	 XFS_SB_VERSION2_FTYPE)


#define XFSLABEL_MAX			12


typedef struct xfs_sb {
	uint32_t	sb_magicnum;	
	uint32_t	sb_blocksize;	
	xfs_rfsblock_t	sb_dblocks;	
	xfs_rfsblock_t	sb_rblocks;	
	xfs_rtblock_t	sb_rextents;	
	uuid_t		sb_uuid;	
	xfs_fsblock_t	sb_logstart;	
	xfs_ino_t	sb_rootino;	
	xfs_ino_t	sb_rbmino;	
	xfs_ino_t	sb_rsumino;	
	xfs_agblock_t	sb_rextsize;	
	xfs_agblock_t	sb_agblocks;	
	xfs_agnumber_t	sb_agcount;	
	xfs_extlen_t	sb_rbmblocks;	
	xfs_extlen_t	sb_logblocks;	
	uint16_t	sb_versionnum;	
	uint16_t	sb_sectsize;	
	uint16_t	sb_inodesize;	
	uint16_t	sb_inopblock;	
	char		sb_fname[XFSLABEL_MAX]; 
	uint8_t		sb_blocklog;	
	uint8_t		sb_sectlog;	
	uint8_t		sb_inodelog;	
	uint8_t		sb_inopblog;	
	uint8_t		sb_agblklog;	
	uint8_t		sb_rextslog;	
	uint8_t		sb_inprogress;	
	uint8_t		sb_imax_pct;	
					
	
	uint64_t	sb_icount;	
	uint64_t	sb_ifree;	
	uint64_t	sb_fdblocks;	
	uint64_t	sb_frextents;	
	
	xfs_ino_t	sb_uquotino;	
	xfs_ino_t	sb_gquotino;	
	uint16_t	sb_qflags;	
	uint8_t		sb_flags;	
	uint8_t		sb_shared_vn;	
	xfs_extlen_t	sb_inoalignmt;	
	uint32_t	sb_unit;	
	uint32_t	sb_width;	
	uint8_t		sb_dirblklog;	
	uint8_t		sb_logsectlog;	
	uint16_t	sb_logsectsize;	
	uint32_t	sb_logsunit;	
	uint32_t	sb_features2;	

	
	uint32_t	sb_bad_features2;

	

	
	uint32_t	sb_features_compat;
	uint32_t	sb_features_ro_compat;
	uint32_t	sb_features_incompat;
	uint32_t	sb_features_log_incompat;

	uint32_t	sb_crc;		
	xfs_extlen_t	sb_spino_align;	

	xfs_ino_t	sb_pquotino;	
	xfs_lsn_t	sb_lsn;		
	uuid_t		sb_meta_uuid;	

	
} xfs_sb_t;

#define XFS_SB_CRC_OFF		offsetof(struct xfs_sb, sb_crc)


struct xfs_dsb {
	__be32		sb_magicnum;	
	__be32		sb_blocksize;	
	__be64		sb_dblocks;	
	__be64		sb_rblocks;	
	__be64		sb_rextents;	
	uuid_t		sb_uuid;	
	__be64		sb_logstart;	
	__be64		sb_rootino;	
	__be64		sb_rbmino;	
	__be64		sb_rsumino;	
	__be32		sb_rextsize;	
	__be32		sb_agblocks;	
	__be32		sb_agcount;	
	__be32		sb_rbmblocks;	
	__be32		sb_logblocks;	
	__be16		sb_versionnum;	
	__be16		sb_sectsize;	
	__be16		sb_inodesize;	
	__be16		sb_inopblock;	
	char		sb_fname[XFSLABEL_MAX]; 
	__u8		sb_blocklog;	
	__u8		sb_sectlog;	
	__u8		sb_inodelog;	
	__u8		sb_inopblog;	
	__u8		sb_agblklog;	
	__u8		sb_rextslog;	
	__u8		sb_inprogress;	
	__u8		sb_imax_pct;	
					
	
	__be64		sb_icount;	
	__be64		sb_ifree;	
	__be64		sb_fdblocks;	
	__be64		sb_frextents;	
	
	__be64		sb_uquotino;	
	__be64		sb_gquotino;	
	__be16		sb_qflags;	
	__u8		sb_flags;	
	__u8		sb_shared_vn;	
	__be32		sb_inoalignmt;	
	__be32		sb_unit;	
	__be32		sb_width;	
	__u8		sb_dirblklog;	
	__u8		sb_logsectlog;	
	__be16		sb_logsectsize;	
	__be32		sb_logsunit;	
	__be32		sb_features2;	
	
	__be32		sb_bad_features2;

	

	
	__be32		sb_features_compat;
	__be32		sb_features_ro_compat;
	__be32		sb_features_incompat;
	__be32		sb_features_log_incompat;

	__le32		sb_crc;		
	__be32		sb_spino_align;	

	__be64		sb_pquotino;	
	__be64		sb_lsn;		
	uuid_t		sb_meta_uuid;	

	
};


#define XFS_SBF_NOFLAGS		0x00	
#define XFS_SBF_READONLY	0x01	


#define XFS_SB_MAX_SHARED_VN	0

#define	XFS_SB_VERSION_NUM(sbp)	((sbp)->sb_versionnum & XFS_SB_VERSION_NUMBITS)

static inline bool xfs_sb_is_v5(struct xfs_sb *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_5;
}


static inline bool xfs_sb_has_mismatched_features2(struct xfs_sb *sbp)
{
	return sbp->sb_bad_features2 != sbp->sb_features2;
}

static inline bool xfs_sb_version_hasmorebits(struct xfs_sb *sbp)
{
	return xfs_sb_is_v5(sbp) ||
	       (sbp->sb_versionnum & XFS_SB_VERSION_MOREBITSBIT);
}

static inline void xfs_sb_version_addattr(struct xfs_sb *sbp)
{
	sbp->sb_versionnum |= XFS_SB_VERSION_ATTRBIT;
}

static inline void xfs_sb_version_addquota(struct xfs_sb *sbp)
{
	sbp->sb_versionnum |= XFS_SB_VERSION_QUOTABIT;
}

static inline void xfs_sb_version_addattr2(struct xfs_sb *sbp)
{
	sbp->sb_versionnum |= XFS_SB_VERSION_MOREBITSBIT;
	sbp->sb_features2 |= XFS_SB_VERSION2_ATTR2BIT;
}

static inline void xfs_sb_version_addprojid32(struct xfs_sb *sbp)
{
	sbp->sb_versionnum |= XFS_SB_VERSION_MOREBITSBIT;
	sbp->sb_features2 |= XFS_SB_VERSION2_PROJID32BIT;
}


#define XFS_SB_FEAT_COMPAT_ALL 0
#define XFS_SB_FEAT_COMPAT_UNKNOWN	~XFS_SB_FEAT_COMPAT_ALL
static inline bool
xfs_sb_has_compat_feature(
	struct xfs_sb	*sbp,
	uint32_t	feature)
{
	return (sbp->sb_features_compat & feature) != 0;
}

#define XFS_SB_FEAT_RO_COMPAT_FINOBT   (1 << 0)		
#define XFS_SB_FEAT_RO_COMPAT_RMAPBT   (1 << 1)		
#define XFS_SB_FEAT_RO_COMPAT_REFLINK  (1 << 2)		
#define XFS_SB_FEAT_RO_COMPAT_INOBTCNT (1 << 3)		
#define XFS_SB_FEAT_RO_COMPAT_ALL \
		(XFS_SB_FEAT_RO_COMPAT_FINOBT | \
		 XFS_SB_FEAT_RO_COMPAT_RMAPBT | \
		 XFS_SB_FEAT_RO_COMPAT_REFLINK| \
		 XFS_SB_FEAT_RO_COMPAT_INOBTCNT)
#define XFS_SB_FEAT_RO_COMPAT_UNKNOWN	~XFS_SB_FEAT_RO_COMPAT_ALL
static inline bool
xfs_sb_has_ro_compat_feature(
	struct xfs_sb	*sbp,
	uint32_t	feature)
{
	return (sbp->sb_features_ro_compat & feature) != 0;
}

#define XFS_SB_FEAT_INCOMPAT_FTYPE	(1 << 0)	
#define XFS_SB_FEAT_INCOMPAT_SPINODES	(1 << 1)	
#define XFS_SB_FEAT_INCOMPAT_META_UUID	(1 << 2)	
#define XFS_SB_FEAT_INCOMPAT_BIGTIME	(1 << 3)	
#define XFS_SB_FEAT_INCOMPAT_NEEDSREPAIR (1 << 4)	
#define XFS_SB_FEAT_INCOMPAT_NREXT64	(1 << 5)	
#define XFS_SB_FEAT_INCOMPAT_ALL \
		(XFS_SB_FEAT_INCOMPAT_FTYPE|	\
		 XFS_SB_FEAT_INCOMPAT_SPINODES|	\
		 XFS_SB_FEAT_INCOMPAT_META_UUID| \
		 XFS_SB_FEAT_INCOMPAT_BIGTIME| \
		 XFS_SB_FEAT_INCOMPAT_NEEDSREPAIR| \
		 XFS_SB_FEAT_INCOMPAT_NREXT64)

#define XFS_SB_FEAT_INCOMPAT_UNKNOWN	~XFS_SB_FEAT_INCOMPAT_ALL
static inline bool
xfs_sb_has_incompat_feature(
	struct xfs_sb	*sbp,
	uint32_t	feature)
{
	return (sbp->sb_features_incompat & feature) != 0;
}

#define XFS_SB_FEAT_INCOMPAT_LOG_XATTRS   (1 << 0)	
#define XFS_SB_FEAT_INCOMPAT_LOG_ALL \
	(XFS_SB_FEAT_INCOMPAT_LOG_XATTRS)
#define XFS_SB_FEAT_INCOMPAT_LOG_UNKNOWN	~XFS_SB_FEAT_INCOMPAT_LOG_ALL
static inline bool
xfs_sb_has_incompat_log_feature(
	struct xfs_sb	*sbp,
	uint32_t	feature)
{
	return (sbp->sb_features_log_incompat & feature) != 0;
}

static inline void
xfs_sb_remove_incompat_log_features(
	struct xfs_sb	*sbp)
{
	sbp->sb_features_log_incompat &= ~XFS_SB_FEAT_INCOMPAT_LOG_ALL;
}

static inline void
xfs_sb_add_incompat_log_features(
	struct xfs_sb	*sbp,
	unsigned int	features)
{
	sbp->sb_features_log_incompat |= features;
}

static inline bool xfs_sb_version_haslogxattrs(struct xfs_sb *sbp)
{
	return xfs_sb_is_v5(sbp) && (sbp->sb_features_log_incompat &
		 XFS_SB_FEAT_INCOMPAT_LOG_XATTRS);
}

static inline bool
xfs_is_quota_inode(struct xfs_sb *sbp, xfs_ino_t ino)
{
	return (ino == sbp->sb_uquotino ||
		ino == sbp->sb_gquotino ||
		ino == sbp->sb_pquotino);
}

#define XFS_SB_DADDR		((xfs_daddr_t)0) 
#define	XFS_SB_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_SB_DADDR)

#define	XFS_HDR_BLOCK(mp,d)	((xfs_agblock_t)XFS_BB_TO_FSBT(mp,d))
#define	XFS_DADDR_TO_FSB(mp,d)	XFS_AGB_TO_FSB(mp, \
			xfs_daddr_to_agno(mp,d), xfs_daddr_to_agbno(mp,d))
#define	XFS_FSB_TO_DADDR(mp,fsbno)	XFS_AGB_TO_DADDR(mp, \
			XFS_FSB_TO_AGNO(mp,fsbno), XFS_FSB_TO_AGBNO(mp,fsbno))


#define XFS_FSS_TO_BB(mp,sec)	((sec) << (mp)->m_sectbb_log)


#define	XFS_FSB_TO_BB(mp,fsbno)	((fsbno) << (mp)->m_blkbb_log)
#define	XFS_BB_TO_FSB(mp,bb)	\
	(((bb) + (XFS_FSB_TO_BB(mp,1) - 1)) >> (mp)->m_blkbb_log)
#define	XFS_BB_TO_FSBT(mp,bb)	((bb) >> (mp)->m_blkbb_log)


#define XFS_FSB_TO_B(mp,fsbno)	((xfs_fsize_t)(fsbno) << (mp)->m_sb.sb_blocklog)
#define XFS_B_TO_FSB(mp,b)	\
	((((uint64_t)(b)) + (mp)->m_blockmask) >> (mp)->m_sb.sb_blocklog)
#define XFS_B_TO_FSBT(mp,b)	(((uint64_t)(b)) >> (mp)->m_sb.sb_blocklog)


#define	XFS_AGF_MAGIC	0x58414746	
#define	XFS_AGI_MAGIC	0x58414749	
#define	XFS_AGFL_MAGIC	0x5841464c	
#define	XFS_AGF_VERSION	1
#define	XFS_AGI_VERSION	1

#define	XFS_AGF_GOOD_VERSION(v)	((v) == XFS_AGF_VERSION)
#define	XFS_AGI_GOOD_VERSION(v)	((v) == XFS_AGI_VERSION)


#define	XFS_BTNUM_AGF	((int)XFS_BTNUM_RMAPi + 1)



typedef struct xfs_agf {
	
	__be32		agf_magicnum;	
	__be32		agf_versionnum;	
	__be32		agf_seqno;	
	__be32		agf_length;	
	
	__be32		agf_roots[XFS_BTNUM_AGF];	
	__be32		agf_levels[XFS_BTNUM_AGF];	

	__be32		agf_flfirst;	
	__be32		agf_fllast;	
	__be32		agf_flcount;	
	__be32		agf_freeblks;	

	__be32		agf_longest;	
	__be32		agf_btreeblks;	
	uuid_t		agf_uuid;	

	__be32		agf_rmap_blocks;	
	__be32		agf_refcount_blocks;	

	__be32		agf_refcount_root;	
	__be32		agf_refcount_level;	

	
	__be64		agf_spare64[14];

	
	__be64		agf_lsn;	
	__be32		agf_crc;	
	__be32		agf_spare2;

	
} xfs_agf_t;

#define XFS_AGF_CRC_OFF		offsetof(struct xfs_agf, agf_crc)

#define	XFS_AGF_MAGICNUM	(1u << 0)
#define	XFS_AGF_VERSIONNUM	(1u << 1)
#define	XFS_AGF_SEQNO		(1u << 2)
#define	XFS_AGF_LENGTH		(1u << 3)
#define	XFS_AGF_ROOTS		(1u << 4)
#define	XFS_AGF_LEVELS		(1u << 5)
#define	XFS_AGF_FLFIRST		(1u << 6)
#define	XFS_AGF_FLLAST		(1u << 7)
#define	XFS_AGF_FLCOUNT		(1u << 8)
#define	XFS_AGF_FREEBLKS	(1u << 9)
#define	XFS_AGF_LONGEST		(1u << 10)
#define	XFS_AGF_BTREEBLKS	(1u << 11)
#define	XFS_AGF_UUID		(1u << 12)
#define	XFS_AGF_RMAP_BLOCKS	(1u << 13)
#define	XFS_AGF_REFCOUNT_BLOCKS	(1u << 14)
#define	XFS_AGF_REFCOUNT_ROOT	(1u << 15)
#define	XFS_AGF_REFCOUNT_LEVEL	(1u << 16)
#define	XFS_AGF_SPARE64		(1u << 17)
#define	XFS_AGF_NUM_BITS	18
#define	XFS_AGF_ALL_BITS	((1u << XFS_AGF_NUM_BITS) - 1)

#define XFS_AGF_FLAGS \
	{ XFS_AGF_MAGICNUM,	"MAGICNUM" }, \
	{ XFS_AGF_VERSIONNUM,	"VERSIONNUM" }, \
	{ XFS_AGF_SEQNO,	"SEQNO" }, \
	{ XFS_AGF_LENGTH,	"LENGTH" }, \
	{ XFS_AGF_ROOTS,	"ROOTS" }, \
	{ XFS_AGF_LEVELS,	"LEVELS" }, \
	{ XFS_AGF_FLFIRST,	"FLFIRST" }, \
	{ XFS_AGF_FLLAST,	"FLLAST" }, \
	{ XFS_AGF_FLCOUNT,	"FLCOUNT" }, \
	{ XFS_AGF_FREEBLKS,	"FREEBLKS" }, \
	{ XFS_AGF_LONGEST,	"LONGEST" }, \
	{ XFS_AGF_BTREEBLKS,	"BTREEBLKS" }, \
	{ XFS_AGF_UUID,		"UUID" }, \
	{ XFS_AGF_RMAP_BLOCKS,	"RMAP_BLOCKS" }, \
	{ XFS_AGF_REFCOUNT_BLOCKS,	"REFCOUNT_BLOCKS" }, \
	{ XFS_AGF_REFCOUNT_ROOT,	"REFCOUNT_ROOT" }, \
	{ XFS_AGF_REFCOUNT_LEVEL,	"REFCOUNT_LEVEL" }, \
	{ XFS_AGF_SPARE64,	"SPARE64" }


#define XFS_AGF_DADDR(mp)	((xfs_daddr_t)(1 << (mp)->m_sectbb_log))
#define	XFS_AGF_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_AGF_DADDR(mp))


#define	XFS_AGI_UNLINKED_BUCKETS	64

typedef struct xfs_agi {
	
	__be32		agi_magicnum;	
	__be32		agi_versionnum;	
	__be32		agi_seqno;	
	__be32		agi_length;	
	
	__be32		agi_count;	
	__be32		agi_root;	
	__be32		agi_level;	
	__be32		agi_freecount;	

	__be32		agi_newino;	
	__be32		agi_dirino;	
	
	__be32		agi_unlinked[XFS_AGI_UNLINKED_BUCKETS];
	
	uuid_t		agi_uuid;	
	__be32		agi_crc;	
	__be32		agi_pad32;
	__be64		agi_lsn;	

	__be32		agi_free_root; 
	__be32		agi_free_level;

	__be32		agi_iblocks;	
	__be32		agi_fblocks;	

	
} xfs_agi_t;

#define XFS_AGI_CRC_OFF		offsetof(struct xfs_agi, agi_crc)

#define	XFS_AGI_MAGICNUM	(1u << 0)
#define	XFS_AGI_VERSIONNUM	(1u << 1)
#define	XFS_AGI_SEQNO		(1u << 2)
#define	XFS_AGI_LENGTH		(1u << 3)
#define	XFS_AGI_COUNT		(1u << 4)
#define	XFS_AGI_ROOT		(1u << 5)
#define	XFS_AGI_LEVEL		(1u << 6)
#define	XFS_AGI_FREECOUNT	(1u << 7)
#define	XFS_AGI_NEWINO		(1u << 8)
#define	XFS_AGI_DIRINO		(1u << 9)
#define	XFS_AGI_UNLINKED	(1u << 10)
#define	XFS_AGI_NUM_BITS_R1	11	
#define	XFS_AGI_ALL_BITS_R1	((1u << XFS_AGI_NUM_BITS_R1) - 1)
#define	XFS_AGI_FREE_ROOT	(1u << 11)
#define	XFS_AGI_FREE_LEVEL	(1u << 12)
#define	XFS_AGI_IBLOCKS		(1u << 13) 
#define	XFS_AGI_NUM_BITS_R2	14


#define XFS_AGI_DADDR(mp)	((xfs_daddr_t)(2 << (mp)->m_sectbb_log))
#define	XFS_AGI_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_AGI_DADDR(mp))


#define XFS_AGFL_DADDR(mp)	((xfs_daddr_t)(3 << (mp)->m_sectbb_log))
#define	XFS_AGFL_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_AGFL_DADDR(mp))
#define	XFS_BUF_TO_AGFL(bp)	((struct xfs_agfl *)((bp)->b_addr))

struct xfs_agfl {
	__be32		agfl_magicnum;
	__be32		agfl_seqno;
	uuid_t		agfl_uuid;
	__be64		agfl_lsn;
	__be32		agfl_crc;
} __attribute__((packed));

#define XFS_AGFL_CRC_OFF	offsetof(struct xfs_agfl, agfl_crc)

#define XFS_AGB_TO_FSB(mp,agno,agbno)	\
	(((xfs_fsblock_t)(agno) << (mp)->m_sb.sb_agblklog) | (agbno))
#define	XFS_FSB_TO_AGNO(mp,fsbno)	\
	((xfs_agnumber_t)((fsbno) >> (mp)->m_sb.sb_agblklog))
#define	XFS_FSB_TO_AGBNO(mp,fsbno)	\
	((xfs_agblock_t)((fsbno) & xfs_mask32lo((mp)->m_sb.sb_agblklog)))
#define	XFS_AGB_TO_DADDR(mp,agno,agbno)	\
	((xfs_daddr_t)XFS_FSB_TO_BB(mp, \
		(xfs_fsblock_t)(agno) * (mp)->m_sb.sb_agblocks + (agbno)))
#define	XFS_AG_DADDR(mp,agno,d)		(XFS_AGB_TO_DADDR(mp, agno, 0) + (d))


#define	XFS_AG_CHECK_DADDR(mp,d,len)	\
	((len) == 1 ? \
	    ASSERT((d) == XFS_SB_DADDR || \
		   xfs_daddr_to_agbno(mp, d) != XFS_SB_DADDR) : \
	    ASSERT(xfs_daddr_to_agno(mp, d) == \
		   xfs_daddr_to_agno(mp, (d) + (len) - 1)))


typedef __be64 xfs_timestamp_t;


struct xfs_legacy_timestamp {
	__be32		t_sec;		
	__be32		t_nsec;		
};


#define XFS_LEGACY_TIME_MIN	((int64_t)S32_MIN)


#define XFS_LEGACY_TIME_MAX	((int64_t)S32_MAX)


#define XFS_BIGTIME_TIME_MIN	((int64_t)0)


#define XFS_BIGTIME_TIME_MAX	((int64_t)((-1ULL / NSEC_PER_SEC) & ~0x3ULL))


#define XFS_BIGTIME_EPOCH_OFFSET	(-(int64_t)S32_MIN)


static inline uint64_t xfs_unix_to_bigtime(time64_t unix_seconds)
{
	return (uint64_t)unix_seconds + XFS_BIGTIME_EPOCH_OFFSET;
}


static inline time64_t xfs_bigtime_to_unix(uint64_t ondisk_seconds)
{
	return (time64_t)ondisk_seconds - XFS_BIGTIME_EPOCH_OFFSET;
}


#define	XFS_DINODE_MAGIC		0x494e	
struct xfs_dinode {
	__be16		di_magic;	
	__be16		di_mode;	
	__u8		di_version;	
	__u8		di_format;	
	__be16		di_onlink;	
	__be32		di_uid;		
	__be32		di_gid;		
	__be32		di_nlink;	
	__be16		di_projid_lo;	
	__be16		di_projid_hi;	
	union {
		
		__be64	di_big_nextents;

		
		__be64	di_v3_pad;

		
		struct {
			__u8	di_v2_pad[6];
			__be16	di_flushiter;
		};
	};
	xfs_timestamp_t	di_atime;	
	xfs_timestamp_t	di_mtime;	
	xfs_timestamp_t	di_ctime;	
	__be64		di_size;	
	__be64		di_nblocks;	
	__be32		di_extsize;	
	union {
		
		struct {
			__be32	di_nextents;
			__be16	di_anextents;
		} __packed;

		
		struct {
			__be32	di_big_anextents;
			__be16	di_nrext64_pad;
		} __packed;
	} __packed;
	__u8		di_forkoff;	
	__s8		di_aformat;	
	__be32		di_dmevmask;	
	__be16		di_dmstate;	
	__be16		di_flags;	
	__be32		di_gen;		

	
	__be32		di_next_unlinked;

	
	__le32		di_crc;		
	__be64		di_changecount;	
	__be64		di_lsn;		
	__be64		di_flags2;	
	__be32		di_cowextsize;	
	__u8		di_pad2[12];	

	
	xfs_timestamp_t	di_crtime;	
	__be64		di_ino;		
	uuid_t		di_uuid;	

	
};

#define XFS_DINODE_CRC_OFF	offsetof(struct xfs_dinode, di_crc)

#define DI_MAX_FLUSH 0xffff


static inline uint xfs_dinode_size(int version)
{
	if (version == 3)
		return sizeof(struct xfs_dinode);
	return offsetof(struct xfs_dinode, di_crc);
}


#define	XFS_MAXLINK		((1U << 31) - 1U)


enum xfs_dinode_fmt {
	XFS_DINODE_FMT_DEV,		
	XFS_DINODE_FMT_LOCAL,		
	XFS_DINODE_FMT_EXTENTS,		
	XFS_DINODE_FMT_BTREE,		
	XFS_DINODE_FMT_UUID		
};

#define XFS_INODE_FORMAT_STR \
	{ XFS_DINODE_FMT_DEV,		"dev" }, \
	{ XFS_DINODE_FMT_LOCAL,		"local" }, \
	{ XFS_DINODE_FMT_EXTENTS,	"extent" }, \
	{ XFS_DINODE_FMT_BTREE,		"btree" }, \
	{ XFS_DINODE_FMT_UUID,		"uuid" }


#define XFS_MAX_EXTCNT_DATA_FORK_LARGE	((xfs_extnum_t)((1ULL << 48) - 1))
#define XFS_MAX_EXTCNT_ATTR_FORK_LARGE	((xfs_extnum_t)((1ULL << 32) - 1))
#define XFS_MAX_EXTCNT_DATA_FORK_SMALL	((xfs_extnum_t)((1ULL << 31) - 1))
#define XFS_MAX_EXTCNT_ATTR_FORK_SMALL	((xfs_extnum_t)((1ULL << 15) - 1))


#define XFS_MAX_EXTCNT_UPGRADE_NR	\
	min(XFS_MAX_EXTCNT_ATTR_FORK_LARGE - XFS_MAX_EXTCNT_ATTR_FORK_SMALL,	\
	    XFS_MAX_EXTCNT_DATA_FORK_LARGE - XFS_MAX_EXTCNT_DATA_FORK_SMALL)


#define	XFS_DINODE_MIN_LOG	8
#define	XFS_DINODE_MAX_LOG	11
#define	XFS_DINODE_MIN_SIZE	(1 << XFS_DINODE_MIN_LOG)
#define	XFS_DINODE_MAX_SIZE	(1 << XFS_DINODE_MAX_LOG)


#define XFS_DINODE_SIZE(mp) \
	(xfs_has_v3inodes(mp) ? \
		sizeof(struct xfs_dinode) : \
		offsetof(struct xfs_dinode, di_crc))
#define XFS_LITINO(mp) \
	((mp)->m_sb.sb_inodesize - XFS_DINODE_SIZE(mp))


#define XFS_DFORK_BOFF(dip)		((int)((dip)->di_forkoff << 3))

#define XFS_DFORK_DSIZE(dip,mp) \
	((dip)->di_forkoff ? XFS_DFORK_BOFF(dip) : XFS_LITINO(mp))
#define XFS_DFORK_ASIZE(dip,mp) \
	((dip)->di_forkoff ? XFS_LITINO(mp) - XFS_DFORK_BOFF(dip) : 0)
#define XFS_DFORK_SIZE(dip,mp,w) \
	((w) == XFS_DATA_FORK ? \
		XFS_DFORK_DSIZE(dip, mp) : \
		XFS_DFORK_ASIZE(dip, mp))

#define XFS_DFORK_MAXEXT(dip, mp, w) \
	(XFS_DFORK_SIZE(dip, mp, w) / sizeof(struct xfs_bmbt_rec))


#define XFS_DFORK_DPTR(dip) \
	((char *)dip + xfs_dinode_size(dip->di_version))
#define XFS_DFORK_APTR(dip)	\
	(XFS_DFORK_DPTR(dip) + XFS_DFORK_BOFF(dip))
#define XFS_DFORK_PTR(dip,w)	\
	((w) == XFS_DATA_FORK ? XFS_DFORK_DPTR(dip) : XFS_DFORK_APTR(dip))

#define XFS_DFORK_FORMAT(dip,w) \
	((w) == XFS_DATA_FORK ? \
		(dip)->di_format : \
		(dip)->di_aformat)


static inline xfs_dev_t xfs_dinode_get_rdev(struct xfs_dinode *dip)
{
	return be32_to_cpu(*(__be32 *)XFS_DFORK_DPTR(dip));
}

static inline void xfs_dinode_put_rdev(struct xfs_dinode *dip, xfs_dev_t rdev)
{
	*(__be32 *)XFS_DFORK_DPTR(dip) = cpu_to_be32(rdev);
}


#define XFS_DIFLAG_REALTIME_BIT  0	
#define XFS_DIFLAG_PREALLOC_BIT  1	
#define XFS_DIFLAG_NEWRTBM_BIT   2	
#define XFS_DIFLAG_IMMUTABLE_BIT 3	
#define XFS_DIFLAG_APPEND_BIT    4	
#define XFS_DIFLAG_SYNC_BIT      5	
#define XFS_DIFLAG_NOATIME_BIT   6	
#define XFS_DIFLAG_NODUMP_BIT    7	
#define XFS_DIFLAG_RTINHERIT_BIT 8	
#define XFS_DIFLAG_PROJINHERIT_BIT   9	
#define XFS_DIFLAG_NOSYMLINKS_BIT   10	
#define XFS_DIFLAG_EXTSIZE_BIT      11	
#define XFS_DIFLAG_EXTSZINHERIT_BIT 12	
#define XFS_DIFLAG_NODEFRAG_BIT     13	
#define XFS_DIFLAG_FILESTREAM_BIT   14  


#define XFS_DIFLAG_REALTIME      (1 << XFS_DIFLAG_REALTIME_BIT)
#define XFS_DIFLAG_PREALLOC      (1 << XFS_DIFLAG_PREALLOC_BIT)
#define XFS_DIFLAG_NEWRTBM       (1 << XFS_DIFLAG_NEWRTBM_BIT)
#define XFS_DIFLAG_IMMUTABLE     (1 << XFS_DIFLAG_IMMUTABLE_BIT)
#define XFS_DIFLAG_APPEND        (1 << XFS_DIFLAG_APPEND_BIT)
#define XFS_DIFLAG_SYNC          (1 << XFS_DIFLAG_SYNC_BIT)
#define XFS_DIFLAG_NOATIME       (1 << XFS_DIFLAG_NOATIME_BIT)
#define XFS_DIFLAG_NODUMP        (1 << XFS_DIFLAG_NODUMP_BIT)
#define XFS_DIFLAG_RTINHERIT     (1 << XFS_DIFLAG_RTINHERIT_BIT)
#define XFS_DIFLAG_PROJINHERIT   (1 << XFS_DIFLAG_PROJINHERIT_BIT)
#define XFS_DIFLAG_NOSYMLINKS    (1 << XFS_DIFLAG_NOSYMLINKS_BIT)
#define XFS_DIFLAG_EXTSIZE       (1 << XFS_DIFLAG_EXTSIZE_BIT)
#define XFS_DIFLAG_EXTSZINHERIT  (1 << XFS_DIFLAG_EXTSZINHERIT_BIT)
#define XFS_DIFLAG_NODEFRAG      (1 << XFS_DIFLAG_NODEFRAG_BIT)
#define XFS_DIFLAG_FILESTREAM    (1 << XFS_DIFLAG_FILESTREAM_BIT)

#define XFS_DIFLAG_ANY \
	(XFS_DIFLAG_REALTIME | XFS_DIFLAG_PREALLOC | XFS_DIFLAG_NEWRTBM | \
	 XFS_DIFLAG_IMMUTABLE | XFS_DIFLAG_APPEND | XFS_DIFLAG_SYNC | \
	 XFS_DIFLAG_NOATIME | XFS_DIFLAG_NODUMP | XFS_DIFLAG_RTINHERIT | \
	 XFS_DIFLAG_PROJINHERIT | XFS_DIFLAG_NOSYMLINKS | XFS_DIFLAG_EXTSIZE | \
	 XFS_DIFLAG_EXTSZINHERIT | XFS_DIFLAG_NODEFRAG | XFS_DIFLAG_FILESTREAM)


#define XFS_DIFLAG2_DAX_BIT	0	
#define XFS_DIFLAG2_REFLINK_BIT	1	
#define XFS_DIFLAG2_COWEXTSIZE_BIT   2  
#define XFS_DIFLAG2_BIGTIME_BIT	3	
#define XFS_DIFLAG2_NREXT64_BIT 4	

#define XFS_DIFLAG2_DAX		(1 << XFS_DIFLAG2_DAX_BIT)
#define XFS_DIFLAG2_REFLINK     (1 << XFS_DIFLAG2_REFLINK_BIT)
#define XFS_DIFLAG2_COWEXTSIZE  (1 << XFS_DIFLAG2_COWEXTSIZE_BIT)
#define XFS_DIFLAG2_BIGTIME	(1 << XFS_DIFLAG2_BIGTIME_BIT)
#define XFS_DIFLAG2_NREXT64	(1 << XFS_DIFLAG2_NREXT64_BIT)

#define XFS_DIFLAG2_ANY \
	(XFS_DIFLAG2_DAX | XFS_DIFLAG2_REFLINK | XFS_DIFLAG2_COWEXTSIZE | \
	 XFS_DIFLAG2_BIGTIME | XFS_DIFLAG2_NREXT64)

static inline bool xfs_dinode_has_bigtime(const struct xfs_dinode *dip)
{
	return dip->di_version >= 3 &&
	       (dip->di_flags2 & cpu_to_be64(XFS_DIFLAG2_BIGTIME));
}

static inline bool xfs_dinode_has_large_extent_counts(
	const struct xfs_dinode *dip)
{
	return dip->di_version >= 3 &&
	       (dip->di_flags2 & cpu_to_be64(XFS_DIFLAG2_NREXT64));
}


#define	XFS_INO_MASK(k)			(uint32_t)((1ULL << (k)) - 1)
#define	XFS_INO_OFFSET_BITS(mp)		(mp)->m_sb.sb_inopblog
#define	XFS_INO_AGBNO_BITS(mp)		(mp)->m_sb.sb_agblklog
#define	XFS_INO_AGINO_BITS(mp)		((mp)->m_ino_geo.agino_log)
#define	XFS_INO_AGNO_BITS(mp)		(mp)->m_agno_log
#define	XFS_INO_BITS(mp)		\
	XFS_INO_AGNO_BITS(mp) + XFS_INO_AGINO_BITS(mp)
#define	XFS_INO_TO_AGNO(mp,i)		\
	((xfs_agnumber_t)((i) >> XFS_INO_AGINO_BITS(mp)))
#define	XFS_INO_TO_AGINO(mp,i)		\
	((xfs_agino_t)(i) & XFS_INO_MASK(XFS_INO_AGINO_BITS(mp)))
#define	XFS_INO_TO_AGBNO(mp,i)		\
	(((xfs_agblock_t)(i) >> XFS_INO_OFFSET_BITS(mp)) & \
		XFS_INO_MASK(XFS_INO_AGBNO_BITS(mp)))
#define	XFS_INO_TO_OFFSET(mp,i)		\
	((int)(i) & XFS_INO_MASK(XFS_INO_OFFSET_BITS(mp)))
#define	XFS_INO_TO_FSB(mp,i)		\
	XFS_AGB_TO_FSB(mp, XFS_INO_TO_AGNO(mp,i), XFS_INO_TO_AGBNO(mp,i))
#define	XFS_AGINO_TO_INO(mp,a,i)	\
	(((xfs_ino_t)(a) << XFS_INO_AGINO_BITS(mp)) | (i))
#define	XFS_AGINO_TO_AGBNO(mp,i)	((i) >> XFS_INO_OFFSET_BITS(mp))
#define	XFS_AGINO_TO_OFFSET(mp,i)	\
	((i) & XFS_INO_MASK(XFS_INO_OFFSET_BITS(mp)))
#define	XFS_OFFBNO_TO_AGINO(mp,b,o)	\
	((xfs_agino_t)(((b) << XFS_INO_OFFSET_BITS(mp)) | (o)))
#define	XFS_FSB_TO_INO(mp, b)	((xfs_ino_t)((b) << XFS_INO_OFFSET_BITS(mp)))
#define	XFS_AGB_TO_AGINO(mp, b)	((xfs_agino_t)((b) << XFS_INO_OFFSET_BITS(mp)))

#define	XFS_MAXINUMBER		((xfs_ino_t)((1ULL << 56) - 1ULL))
#define	XFS_MAXINUMBER_32	((xfs_ino_t)((1ULL << 32) - 1ULL))




#define	XFS_MAX_RTEXTSIZE	(1024 * 1024 * 1024)	
#define	XFS_DFL_RTEXTSIZE	(64 * 1024)	        
#define	XFS_MIN_RTEXTSIZE	(4 * 1024)		

#define	XFS_BLOCKSIZE(mp)	((mp)->m_sb.sb_blocksize)
#define	XFS_BLOCKMASK(mp)	((mp)->m_blockmask)
#define	XFS_BLOCKWSIZE(mp)	((mp)->m_blockwsize)
#define	XFS_BLOCKWMASK(mp)	((mp)->m_blockwmask)


#define	XFS_SUMOFFS(mp,ls,bb)	((int)((ls) * (mp)->m_sb.sb_rbmblocks + (bb)))
#define	XFS_SUMOFFSTOBLOCK(mp,s)	\
	(((s) * (uint)sizeof(xfs_suminfo_t)) >> (mp)->m_sb.sb_blocklog)
#define	XFS_SUMPTR(mp,bp,so)	\
	((xfs_suminfo_t *)((bp)->b_addr + \
		(((so) * (uint)sizeof(xfs_suminfo_t)) & XFS_BLOCKMASK(mp))))

#define	XFS_BITTOBLOCK(mp,bi)	((bi) >> (mp)->m_blkbit_log)
#define	XFS_BLOCKTOBIT(mp,bb)	((bb) << (mp)->m_blkbit_log)
#define	XFS_BITTOWORD(mp,bi)	\
	((int)(((bi) >> XFS_NBWORDLOG) & XFS_BLOCKWMASK(mp)))

#define	XFS_RTMIN(a,b)	((a) < (b) ? (a) : (b))
#define	XFS_RTMAX(a,b)	((a) > (b) ? (a) : (b))

#define	XFS_RTLOBIT(w)	xfs_lowbit32(w)
#define	XFS_RTHIBIT(w)	xfs_highbit32(w)

#define	XFS_RTBLOCKLOG(b)	xfs_highbit64(b)


#define XFS_DQUOT_MAGIC		0x4451		
#define XFS_DQUOT_VERSION	(uint8_t)0x01	

#define XFS_DQTYPE_USER		(1u << 0)	
#define XFS_DQTYPE_PROJ		(1u << 1)	
#define XFS_DQTYPE_GROUP	(1u << 2)	
#define XFS_DQTYPE_BIGTIME	(1u << 7)	


#define XFS_DQTYPE_REC_MASK	(XFS_DQTYPE_USER | \
				 XFS_DQTYPE_PROJ | \
				 XFS_DQTYPE_GROUP)

#define XFS_DQTYPE_ANY		(XFS_DQTYPE_REC_MASK | \
				 XFS_DQTYPE_BIGTIME)




#define XFS_DQ_LEGACY_EXPIRY_MIN	((int64_t)1)


#define XFS_DQ_LEGACY_EXPIRY_MAX	((int64_t)U32_MAX)


#define XFS_DQ_BIGTIME_EXPIRY_MIN	(XFS_DQ_LEGACY_EXPIRY_MIN)


#define XFS_DQ_BIGTIME_EXPIRY_MAX	((int64_t)4074815106U)


#define XFS_DQ_BIGTIME_SHIFT	(2)
#define XFS_DQ_BIGTIME_SLACK	((int64_t)(1ULL << XFS_DQ_BIGTIME_SHIFT) - 1)


static inline uint32_t xfs_dq_unix_to_bigtime(time64_t unix_seconds)
{
	
	return ((uint64_t)unix_seconds + XFS_DQ_BIGTIME_SLACK) >>
			XFS_DQ_BIGTIME_SHIFT;
}


static inline time64_t xfs_dq_bigtime_to_unix(uint32_t ondisk_seconds)
{
	return (time64_t)ondisk_seconds << XFS_DQ_BIGTIME_SHIFT;
}


#define XFS_DQ_GRACE_MIN		((int64_t)0)
#define XFS_DQ_GRACE_MAX		((int64_t)U32_MAX)


struct xfs_disk_dquot {
	__be16		d_magic;	
	__u8		d_version;	
	__u8		d_type;		
	__be32		d_id;		
	__be64		d_blk_hardlimit;
	__be64		d_blk_softlimit;
	__be64		d_ino_hardlimit;
	__be64		d_ino_softlimit;
	__be64		d_bcount;	
	__be64		d_icount;	
	__be32		d_itimer;	
	__be32		d_btimer;	
	__be16		d_iwarns;	
	__be16		d_bwarns;	
	__be32		d_pad0;		
	__be64		d_rtb_hardlimit;
	__be64		d_rtb_softlimit;
	__be64		d_rtbcount;	
	__be32		d_rtbtimer;	
	__be16		d_rtbwarns;	
	__be16		d_pad;
};


struct xfs_dqblk {
	struct xfs_disk_dquot	dd_diskdq; 
	char			dd_fill[4];

	
	__be32		  dd_crc;	
	__be64		  dd_lsn;	
	uuid_t		  dd_uuid;	
};

#define XFS_DQUOT_CRC_OFF	offsetof(struct xfs_dqblk, dd_crc)


#define XFS_DQUOT_CLUSTER_SIZE_FSB	(xfs_filblks_t)1


#define XFS_SYMLINK_MAGIC	0x58534c4d	

struct xfs_dsymlink_hdr {
	__be32	sl_magic;
	__be32	sl_offset;
	__be32	sl_bytes;
	__be32	sl_crc;
	uuid_t	sl_uuid;
	__be64	sl_owner;
	__be64	sl_blkno;
	__be64	sl_lsn;
};

#define XFS_SYMLINK_CRC_OFF	offsetof(struct xfs_dsymlink_hdr, sl_crc)

#define XFS_SYMLINK_MAXLEN	1024

#define XFS_SYMLINK_MAPS 3

#define XFS_SYMLINK_BUF_SPACE(mp, bufsize)	\
	((bufsize) - (xfs_has_crc((mp)) ? \
			sizeof(struct xfs_dsymlink_hdr) : 0))



#define	XFS_ABTB_MAGIC		0x41425442	
#define	XFS_ABTB_CRC_MAGIC	0x41423342	
#define	XFS_ABTC_MAGIC		0x41425443	
#define	XFS_ABTC_CRC_MAGIC	0x41423343	


typedef struct xfs_alloc_rec {
	__be32		ar_startblock;	
	__be32		ar_blockcount;	
} xfs_alloc_rec_t, xfs_alloc_key_t;

typedef struct xfs_alloc_rec_incore {
	xfs_agblock_t	ar_startblock;	
	xfs_extlen_t	ar_blockcount;	
} xfs_alloc_rec_incore_t;


typedef __be32 xfs_alloc_ptr_t;


#define	XFS_BNO_BLOCK(mp)	((xfs_agblock_t)(XFS_AGFL_BLOCK(mp) + 1))
#define	XFS_CNT_BLOCK(mp)	((xfs_agblock_t)(XFS_BNO_BLOCK(mp) + 1))



#define	XFS_IBT_MAGIC		0x49414254	
#define	XFS_IBT_CRC_MAGIC	0x49414233	
#define	XFS_FIBT_MAGIC		0x46494254	
#define	XFS_FIBT_CRC_MAGIC	0x46494233	

typedef uint64_t	xfs_inofree_t;
#define	XFS_INODES_PER_CHUNK		(NBBY * sizeof(xfs_inofree_t))
#define	XFS_INODES_PER_CHUNK_LOG	(XFS_NBBYLOG + 3)
#define	XFS_INOBT_ALL_FREE		((xfs_inofree_t)-1)
#define	XFS_INOBT_MASK(i)		((xfs_inofree_t)1 << (i))

#define XFS_INOBT_HOLEMASK_FULL		0	
#define XFS_INOBT_HOLEMASK_BITS		(NBBY * sizeof(uint16_t))
#define XFS_INODES_PER_HOLEMASK_BIT	\
	(XFS_INODES_PER_CHUNK / (NBBY * sizeof(uint16_t)))

static inline xfs_inofree_t xfs_inobt_maskn(int i, int n)
{
	return ((n >= XFS_INODES_PER_CHUNK ? 0 : XFS_INOBT_MASK(n)) - 1) << i;
}


typedef struct xfs_inobt_rec {
	__be32		ir_startino;	
	union {
		struct {
			__be32	ir_freecount;	
		} f;
		struct {
			__be16	ir_holemask;
			__u8	ir_count;	
			__u8	ir_freecount;	
		} sp;
	} ir_u;
	__be64		ir_free;	
} xfs_inobt_rec_t;

typedef struct xfs_inobt_rec_incore {
	xfs_agino_t	ir_startino;	
	uint16_t	ir_holemask;	
	uint8_t		ir_count;	
	uint8_t		ir_freecount;	
	xfs_inofree_t	ir_free;	
} xfs_inobt_rec_incore_t;

static inline bool xfs_inobt_issparse(uint16_t holemask)
{
	
	return holemask;
}


typedef struct xfs_inobt_key {
	__be32		ir_startino;	
} xfs_inobt_key_t;


typedef __be32 xfs_inobt_ptr_t;


#define	XFS_IBT_BLOCK(mp)		((xfs_agblock_t)(XFS_CNT_BLOCK(mp) + 1))
#define	XFS_FIBT_BLOCK(mp)		((xfs_agblock_t)(XFS_IBT_BLOCK(mp) + 1))


#define	XFS_RMAP_CRC_MAGIC	0x524d4233	


#define XFS_OWNER_INFO_ATTR_FORK	(1 << 0)
#define XFS_OWNER_INFO_BMBT_BLOCK	(1 << 1)
struct xfs_owner_info {
	uint64_t		oi_owner;
	xfs_fileoff_t		oi_offset;
	unsigned int		oi_flags;
};


#define XFS_RMAP_OWN_NULL	(-1ULL)	
#define XFS_RMAP_OWN_UNKNOWN	(-2ULL)	
#define XFS_RMAP_OWN_FS		(-3ULL)	
#define XFS_RMAP_OWN_LOG	(-4ULL)	
#define XFS_RMAP_OWN_AG		(-5ULL)	
#define XFS_RMAP_OWN_INOBT	(-6ULL)	
#define XFS_RMAP_OWN_INODES	(-7ULL)	
#define XFS_RMAP_OWN_REFC	(-8ULL) 
#define XFS_RMAP_OWN_COW	(-9ULL) 
#define XFS_RMAP_OWN_MIN	(-10ULL) 

#define XFS_RMAP_NON_INODE_OWNER(owner)	(!!((owner) & (1ULL << 63)))


struct xfs_rmap_rec {
	__be32		rm_startblock;	
	__be32		rm_blockcount;	
	__be64		rm_owner;	
	__be64		rm_offset;	
};


#define XFS_RMAP_OFF_ATTR_FORK	((uint64_t)1ULL << 63)
#define XFS_RMAP_OFF_BMBT_BLOCK	((uint64_t)1ULL << 62)
#define XFS_RMAP_OFF_UNWRITTEN	((uint64_t)1ULL << 61)

#define XFS_RMAP_LEN_MAX	((uint32_t)~0U)
#define XFS_RMAP_OFF_FLAGS	(XFS_RMAP_OFF_ATTR_FORK | \
				 XFS_RMAP_OFF_BMBT_BLOCK | \
				 XFS_RMAP_OFF_UNWRITTEN)
#define XFS_RMAP_OFF_MASK	((uint64_t)0x3FFFFFFFFFFFFFULL)

#define XFS_RMAP_OFF(off)		((off) & XFS_RMAP_OFF_MASK)

#define XFS_RMAP_IS_BMBT_BLOCK(off)	(!!((off) & XFS_RMAP_OFF_BMBT_BLOCK))
#define XFS_RMAP_IS_ATTR_FORK(off)	(!!((off) & XFS_RMAP_OFF_ATTR_FORK))
#define XFS_RMAP_IS_UNWRITTEN(len)	(!!((off) & XFS_RMAP_OFF_UNWRITTEN))

#define RMAPBT_STARTBLOCK_BITLEN	32
#define RMAPBT_BLOCKCOUNT_BITLEN	32
#define RMAPBT_OWNER_BITLEN		64
#define RMAPBT_ATTRFLAG_BITLEN		1
#define RMAPBT_BMBTFLAG_BITLEN		1
#define RMAPBT_EXNTFLAG_BITLEN		1
#define RMAPBT_UNUSED_OFFSET_BITLEN	7
#define RMAPBT_OFFSET_BITLEN		54


struct xfs_rmap_key {
	__be32		rm_startblock;	
	__be64		rm_owner;	
	__be64		rm_offset;	
} __attribute__((packed));


typedef __be32 xfs_rmap_ptr_t;

#define	XFS_RMAP_BLOCK(mp) \
	(xfs_has_finobt(((mp))) ? \
	 XFS_FIBT_BLOCK(mp) + 1 : \
	 XFS_IBT_BLOCK(mp) + 1)


#define	XFS_REFC_CRC_MAGIC	0x52334643	

unsigned int xfs_refc_block(struct xfs_mount *mp);




#define XFS_REFC_COWFLAG		(1U << 31)
#define REFCNTBT_COWFLAG_BITLEN		1
#define REFCNTBT_AGBLOCK_BITLEN		31

struct xfs_refcount_rec {
	__be32		rc_startblock;	
	__be32		rc_blockcount;	
	__be32		rc_refcount;	
};

struct xfs_refcount_key {
	__be32		rc_startblock;	
};

#define MAXREFCOUNT	((xfs_nlink_t)~0U)
#define MAXREFCEXTLEN	((xfs_extlen_t)~0U)


typedef __be32 xfs_refcount_ptr_t;



#define XFS_BMAP_MAGIC		0x424d4150	
#define XFS_BMAP_CRC_MAGIC	0x424d4133	


typedef struct xfs_bmdr_block {
	__be16		bb_level;	
	__be16		bb_numrecs;	
} xfs_bmdr_block_t;


#define BMBT_EXNTFLAG_BITLEN	1
#define BMBT_STARTOFF_BITLEN	54
#define BMBT_STARTBLOCK_BITLEN	52
#define BMBT_BLOCKCOUNT_BITLEN	21

#define BMBT_STARTOFF_MASK	((1ULL << BMBT_STARTOFF_BITLEN) - 1)
#define BMBT_BLOCKCOUNT_MASK	((1ULL << BMBT_BLOCKCOUNT_BITLEN) - 1)

#define XFS_MAX_BMBT_EXTLEN	((xfs_extlen_t)(BMBT_BLOCKCOUNT_MASK))


#define XFS_MAX_FILEOFF		(BMBT_STARTOFF_MASK + BMBT_BLOCKCOUNT_MASK)

typedef struct xfs_bmbt_rec {
	__be64			l0, l1;
} xfs_bmbt_rec_t;

typedef uint64_t	xfs_bmbt_rec_base_t;	
typedef xfs_bmbt_rec_t xfs_bmdr_rec_t;


#define STARTBLOCKVALBITS	17
#define STARTBLOCKMASKBITS	(15 + 20)
#define STARTBLOCKMASK		\
	(((((xfs_fsblock_t)1) << STARTBLOCKMASKBITS) - 1) << STARTBLOCKVALBITS)

static inline int isnullstartblock(xfs_fsblock_t x)
{
	return ((x) & STARTBLOCKMASK) == STARTBLOCKMASK;
}

static inline xfs_fsblock_t nullstartblock(int k)
{
	ASSERT(k < (1 << STARTBLOCKVALBITS));
	return STARTBLOCKMASK | (k);
}

static inline xfs_filblks_t startblockval(xfs_fsblock_t x)
{
	return (xfs_filblks_t)((x) & ~STARTBLOCKMASK);
}


typedef struct xfs_bmbt_key {
	__be64		br_startoff;	
} xfs_bmbt_key_t, xfs_bmdr_key_t;


typedef __be64 xfs_bmbt_ptr_t, xfs_bmdr_ptr_t;




struct xfs_btree_block_shdr {
	__be32		bb_leftsib;
	__be32		bb_rightsib;

	__be64		bb_blkno;
	__be64		bb_lsn;
	uuid_t		bb_uuid;
	__be32		bb_owner;
	__le32		bb_crc;
};


struct xfs_btree_block_lhdr {
	__be64		bb_leftsib;
	__be64		bb_rightsib;

	__be64		bb_blkno;
	__be64		bb_lsn;
	uuid_t		bb_uuid;
	__be64		bb_owner;
	__le32		bb_crc;
	__be32		bb_pad; 
};

struct xfs_btree_block {
	__be32		bb_magic;	
	__be16		bb_level;	
	__be16		bb_numrecs;	
	union {
		struct xfs_btree_block_shdr s;
		struct xfs_btree_block_lhdr l;
	} bb_u;				
};


#define XFS_BTREE_SBLOCK_LEN \
	(offsetof(struct xfs_btree_block, bb_u) + \
	 offsetof(struct xfs_btree_block_shdr, bb_blkno))

#define XFS_BTREE_LBLOCK_LEN \
	(offsetof(struct xfs_btree_block, bb_u) + \
	 offsetof(struct xfs_btree_block_lhdr, bb_blkno))


#define XFS_BTREE_SBLOCK_CRC_LEN \
	(offsetof(struct xfs_btree_block, bb_u) + \
	 sizeof(struct xfs_btree_block_shdr))
#define XFS_BTREE_LBLOCK_CRC_LEN \
	(offsetof(struct xfs_btree_block, bb_u) + \
	 sizeof(struct xfs_btree_block_lhdr))

#define XFS_BTREE_SBLOCK_CRC_OFF \
	offsetof(struct xfs_btree_block, bb_u.s.bb_crc)
#define XFS_BTREE_LBLOCK_CRC_OFF \
	offsetof(struct xfs_btree_block, bb_u.l.bb_crc)


struct xfs_acl_entry {
	__be32	ae_tag;
	__be32	ae_id;
	__be16	ae_perm;
	__be16	ae_pad;		
};

struct xfs_acl {
	__be32			acl_cnt;
	struct xfs_acl_entry	acl_entry[];
};


#define XFS_ACL_MAX_ENTRIES(mp)	\
	(xfs_has_crc(mp) \
		?  (XFS_XATTR_SIZE_MAX - sizeof(struct xfs_acl)) / \
						sizeof(struct xfs_acl_entry) \
		: 25)

#define XFS_ACL_SIZE(cnt) \
	(sizeof(struct xfs_acl) + \
		sizeof(struct xfs_acl_entry) * cnt)

#define XFS_ACL_MAX_SIZE(mp) \
	XFS_ACL_SIZE(XFS_ACL_MAX_ENTRIES((mp)))



#define SGI_ACL_FILE		"SGI_ACL_FILE"
#define SGI_ACL_DEFAULT		"SGI_ACL_DEFAULT"
#define SGI_ACL_FILE_SIZE	(sizeof(SGI_ACL_FILE)-1)
#define SGI_ACL_DEFAULT_SIZE	(sizeof(SGI_ACL_DEFAULT)-1)

#endif 
