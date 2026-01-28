#ifndef __XFS_TYPES_H__
#define	__XFS_TYPES_H__
typedef uint32_t	prid_t;		 
typedef uint32_t	xfs_agblock_t;	 
typedef uint32_t	xfs_agino_t;	 
typedef uint32_t	xfs_extlen_t;	 
typedef uint32_t	xfs_agnumber_t;	 
typedef uint64_t	xfs_extnum_t;	 
typedef uint32_t	xfs_aextnum_t;	 
typedef int64_t		xfs_fsize_t;	 
typedef uint64_t	xfs_ufsize_t;	 
typedef int32_t		xfs_suminfo_t;	 
typedef uint32_t	xfs_rtword_t;	 
typedef int64_t		xfs_lsn_t;	 
typedef int64_t		xfs_csn_t;	 
typedef uint32_t	xfs_dablk_t;	 
typedef uint32_t	xfs_dahash_t;	 
typedef uint64_t	xfs_fsblock_t;	 
typedef uint64_t	xfs_rfsblock_t;	 
typedef uint64_t	xfs_rtblock_t;	 
typedef uint64_t	xfs_fileoff_t;	 
typedef uint64_t	xfs_filblks_t;	 
typedef int64_t		xfs_srtblock_t;	 
typedef void *		xfs_failaddr_t;
#define	NULLFSBLOCK	((xfs_fsblock_t)-1)
#define	NULLRFSBLOCK	((xfs_rfsblock_t)-1)
#define	NULLRTBLOCK	((xfs_rtblock_t)-1)
#define	NULLFILEOFF	((xfs_fileoff_t)-1)
#define	NULLAGBLOCK	((xfs_agblock_t)-1)
#define	NULLAGNUMBER	((xfs_agnumber_t)-1)
#define NULLCOMMITLSN	((xfs_lsn_t)-1)
#define	NULLFSINO	((xfs_ino_t)-1)
#define	NULLAGINO	((xfs_agino_t)-1)
#define XFS_MIN_BLOCKSIZE_LOG	9	 
#define XFS_MAX_BLOCKSIZE_LOG	16	 
#define XFS_MIN_BLOCKSIZE	(1 << XFS_MIN_BLOCKSIZE_LOG)
#define XFS_MAX_BLOCKSIZE	(1 << XFS_MAX_BLOCKSIZE_LOG)
#define XFS_MIN_CRC_BLOCKSIZE	(1 << (XFS_MIN_BLOCKSIZE_LOG + 1))
#define XFS_MIN_SECTORSIZE_LOG	9	 
#define XFS_MAX_SECTORSIZE_LOG	15	 
#define XFS_MIN_SECTORSIZE	(1 << XFS_MIN_SECTORSIZE_LOG)
#define XFS_MAX_SECTORSIZE	(1 << XFS_MAX_SECTORSIZE_LOG)
#define	XFS_DATA_FORK	0
#define	XFS_ATTR_FORK	1
#define	XFS_COW_FORK	2
#define XFS_WHICHFORK_STRINGS \
	{ XFS_DATA_FORK, 	"data" }, \
	{ XFS_ATTR_FORK,	"attr" }, \
	{ XFS_COW_FORK,		"cow" }
#define MINDBTPTRS	3
#define MINABTPTRS	2
#define MAXNAMELEN	256
typedef enum {
	XFS_LOOKUP_EQi, XFS_LOOKUP_LEi, XFS_LOOKUP_GEi
} xfs_lookup_t;
#define XFS_AG_BTREE_CMP_FORMAT_STR \
	{ XFS_LOOKUP_EQi,	"eq" }, \
	{ XFS_LOOKUP_LEi,	"le" }, \
	{ XFS_LOOKUP_GEi,	"ge" }
typedef enum {
	XFS_BTNUM_BNOi, XFS_BTNUM_CNTi, XFS_BTNUM_RMAPi, XFS_BTNUM_BMAPi,
	XFS_BTNUM_INOi, XFS_BTNUM_FINOi, XFS_BTNUM_REFCi, XFS_BTNUM_MAX
} xfs_btnum_t;
#define XFS_BTNUM_STRINGS \
	{ XFS_BTNUM_BNOi,	"bnobt" }, \
	{ XFS_BTNUM_CNTi,	"cntbt" }, \
	{ XFS_BTNUM_RMAPi,	"rmapbt" }, \
	{ XFS_BTNUM_BMAPi,	"bmbt" }, \
	{ XFS_BTNUM_INOi,	"inobt" }, \
	{ XFS_BTNUM_FINOi,	"finobt" }, \
	{ XFS_BTNUM_REFCi,	"refcbt" }
struct xfs_name {
	const unsigned char	*name;
	int			len;
	int			type;
};
typedef uint32_t	xfs_dqid_t;
#define	XFS_NBBYLOG	3		 
#define	XFS_WORDLOG	2		 
#define	XFS_NBWORDLOG	(XFS_NBBYLOG + XFS_WORDLOG)
#define	XFS_NBWORD	(1 << XFS_NBWORDLOG)
#define	XFS_WORDMASK	((1 << XFS_WORDLOG) - 1)
struct xfs_iext_cursor {
	struct xfs_iext_leaf	*leaf;
	int			pos;
};
typedef enum {
	XFS_EXT_NORM, XFS_EXT_UNWRITTEN,
} xfs_exntst_t;
typedef struct xfs_bmbt_irec
{
	xfs_fileoff_t	br_startoff;	 
	xfs_fsblock_t	br_startblock;	 
	xfs_filblks_t	br_blockcount;	 
	xfs_exntst_t	br_state;	 
} xfs_bmbt_irec_t;
enum xfs_refc_domain {
	XFS_REFC_DOMAIN_SHARED = 0,
	XFS_REFC_DOMAIN_COW,
};
#define XFS_REFC_DOMAIN_STRINGS \
	{ XFS_REFC_DOMAIN_SHARED,	"shared" }, \
	{ XFS_REFC_DOMAIN_COW,		"cow" }
struct xfs_refcount_irec {
	xfs_agblock_t	rc_startblock;	 
	xfs_extlen_t	rc_blockcount;	 
	xfs_nlink_t	rc_refcount;	 
	enum xfs_refc_domain	rc_domain;  
};
#define XFS_RMAP_ATTR_FORK		(1 << 0)
#define XFS_RMAP_BMBT_BLOCK		(1 << 1)
#define XFS_RMAP_UNWRITTEN		(1 << 2)
#define XFS_RMAP_KEY_FLAGS		(XFS_RMAP_ATTR_FORK | \
					 XFS_RMAP_BMBT_BLOCK)
#define XFS_RMAP_REC_FLAGS		(XFS_RMAP_UNWRITTEN)
struct xfs_rmap_irec {
	xfs_agblock_t	rm_startblock;	 
	xfs_extlen_t	rm_blockcount;	 
	uint64_t	rm_owner;	 
	uint64_t	rm_offset;	 
	unsigned int	rm_flags;	 
};
enum xfs_ag_resv_type {
	XFS_AG_RESV_NONE = 0,
	XFS_AG_RESV_AGFL,
	XFS_AG_RESV_METADATA,
	XFS_AG_RESV_RMAPBT,
};
enum xbtree_recpacking {
	XBTREE_RECPACKING_EMPTY = 0,
	XBTREE_RECPACKING_SPARSE,
	XBTREE_RECPACKING_FULL,
};
struct xfs_mount;
bool xfs_verify_fsbno(struct xfs_mount *mp, xfs_fsblock_t fsbno);
bool xfs_verify_fsbext(struct xfs_mount *mp, xfs_fsblock_t fsbno,
		xfs_fsblock_t len);
bool xfs_verify_ino(struct xfs_mount *mp, xfs_ino_t ino);
bool xfs_internal_inum(struct xfs_mount *mp, xfs_ino_t ino);
bool xfs_verify_dir_ino(struct xfs_mount *mp, xfs_ino_t ino);
bool xfs_verify_rtbno(struct xfs_mount *mp, xfs_rtblock_t rtbno);
bool xfs_verify_rtext(struct xfs_mount *mp, xfs_rtblock_t rtbno,
		xfs_rtblock_t len);
bool xfs_verify_icount(struct xfs_mount *mp, unsigned long long icount);
bool xfs_verify_dablk(struct xfs_mount *mp, xfs_fileoff_t off);
void xfs_icount_range(struct xfs_mount *mp, unsigned long long *min,
		unsigned long long *max);
bool xfs_verify_fileoff(struct xfs_mount *mp, xfs_fileoff_t off);
bool xfs_verify_fileext(struct xfs_mount *mp, xfs_fileoff_t off,
		xfs_fileoff_t len);
#endif	 
