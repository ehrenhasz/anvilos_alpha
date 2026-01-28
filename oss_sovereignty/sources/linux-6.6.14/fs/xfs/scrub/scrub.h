

#ifndef __XFS_SCRUB_SCRUB_H__
#define __XFS_SCRUB_SCRUB_H__

struct xfs_scrub;


#define XCHK_GFP_FLAGS	((__force gfp_t)(GFP_KERNEL | __GFP_NOWARN | \
					 __GFP_RETRY_MAYFAIL))


enum xchk_type {
	ST_NONE = 1,	
	ST_PERAG,	
	ST_FS,		
	ST_INODE,	
};

struct xchk_meta_ops {
	
	int		(*setup)(struct xfs_scrub *sc);

	
	int		(*scrub)(struct xfs_scrub *);

	
	int		(*repair)(struct xfs_scrub *);

	
	bool		(*has)(struct xfs_mount *);

	
	enum xchk_type	type;
};


struct xchk_ag {
	struct xfs_perag	*pag;

	
	struct xfs_buf		*agf_bp;
	struct xfs_buf		*agi_bp;

	
	struct xfs_btree_cur	*bno_cur;
	struct xfs_btree_cur	*cnt_cur;
	struct xfs_btree_cur	*ino_cur;
	struct xfs_btree_cur	*fino_cur;
	struct xfs_btree_cur	*rmap_cur;
	struct xfs_btree_cur	*refc_cur;
};

struct xfs_scrub {
	
	struct xfs_mount		*mp;
	struct xfs_scrub_metadata	*sm;
	const struct xchk_meta_ops	*ops;
	struct xfs_trans		*tp;

	
	struct file			*file;

	
	struct xfs_inode		*ip;

	
	void				*buf;

	
	void				(*buf_cleanup)(void *buf);

	
	struct xfile			*xfile;

	
	uint				ilock_flags;

	
	unsigned int			flags;

	
	unsigned int			sick_mask;

	
	struct xchk_ag			sa;
};


#define XCHK_TRY_HARDER		(1U << 0)  
#define XCHK_HAVE_FREEZE_PROT	(1U << 1)  
#define XCHK_FSGATES_DRAIN	(1U << 2)  
#define XCHK_NEED_DRAIN		(1U << 3)  
#define XREP_ALREADY_FIXED	(1U << 31) 


#define XCHK_FSGATES_ALL	(XCHK_FSGATES_DRAIN)


int xchk_tester(struct xfs_scrub *sc);
int xchk_superblock(struct xfs_scrub *sc);
int xchk_agf(struct xfs_scrub *sc);
int xchk_agfl(struct xfs_scrub *sc);
int xchk_agi(struct xfs_scrub *sc);
int xchk_bnobt(struct xfs_scrub *sc);
int xchk_cntbt(struct xfs_scrub *sc);
int xchk_inobt(struct xfs_scrub *sc);
int xchk_finobt(struct xfs_scrub *sc);
int xchk_rmapbt(struct xfs_scrub *sc);
int xchk_refcountbt(struct xfs_scrub *sc);
int xchk_inode(struct xfs_scrub *sc);
int xchk_bmap_data(struct xfs_scrub *sc);
int xchk_bmap_attr(struct xfs_scrub *sc);
int xchk_bmap_cow(struct xfs_scrub *sc);
int xchk_directory(struct xfs_scrub *sc);
int xchk_xattr(struct xfs_scrub *sc);
int xchk_symlink(struct xfs_scrub *sc);
int xchk_parent(struct xfs_scrub *sc);
#ifdef CONFIG_XFS_RT
int xchk_rtbitmap(struct xfs_scrub *sc);
int xchk_rtsummary(struct xfs_scrub *sc);
#else
static inline int
xchk_rtbitmap(struct xfs_scrub *sc)
{
	return -ENOENT;
}
static inline int
xchk_rtsummary(struct xfs_scrub *sc)
{
	return -ENOENT;
}
#endif
#ifdef CONFIG_XFS_QUOTA
int xchk_quota(struct xfs_scrub *sc);
#else
static inline int
xchk_quota(struct xfs_scrub *sc)
{
	return -ENOENT;
}
#endif
int xchk_fscounters(struct xfs_scrub *sc);


void xchk_xref_is_used_space(struct xfs_scrub *sc, xfs_agblock_t agbno,
		xfs_extlen_t len);
void xchk_xref_is_not_inode_chunk(struct xfs_scrub *sc, xfs_agblock_t agbno,
		xfs_extlen_t len);
void xchk_xref_is_inode_chunk(struct xfs_scrub *sc, xfs_agblock_t agbno,
		xfs_extlen_t len);
void xchk_xref_is_only_owned_by(struct xfs_scrub *sc, xfs_agblock_t agbno,
		xfs_extlen_t len, const struct xfs_owner_info *oinfo);
void xchk_xref_is_not_owned_by(struct xfs_scrub *sc, xfs_agblock_t agbno,
		xfs_extlen_t len, const struct xfs_owner_info *oinfo);
void xchk_xref_has_no_owner(struct xfs_scrub *sc, xfs_agblock_t agbno,
		xfs_extlen_t len);
void xchk_xref_is_cow_staging(struct xfs_scrub *sc, xfs_agblock_t bno,
		xfs_extlen_t len);
void xchk_xref_is_not_shared(struct xfs_scrub *sc, xfs_agblock_t bno,
		xfs_extlen_t len);
void xchk_xref_is_not_cow_staging(struct xfs_scrub *sc, xfs_agblock_t bno,
		xfs_extlen_t len);
#ifdef CONFIG_XFS_RT
void xchk_xref_is_used_rt_space(struct xfs_scrub *sc, xfs_rtblock_t rtbno,
		xfs_extlen_t len);
#else
# define xchk_xref_is_used_rt_space(sc, rtbno, len) do { } while (0)
#endif

#endif	
