

#ifndef __XFS_RTALLOC_H__
#define	__XFS_RTALLOC_H__



struct xfs_mount;
struct xfs_trans;


struct xfs_rtalloc_rec {
	xfs_rtblock_t		ar_startext;
	xfs_rtblock_t		ar_extcount;
};

typedef int (*xfs_rtalloc_query_range_fn)(
	struct xfs_mount		*mp,
	struct xfs_trans		*tp,
	const struct xfs_rtalloc_rec	*rec,
	void				*priv);

#ifdef CONFIG_XFS_RT



int					
xfs_rtallocate_extent(
	struct xfs_trans	*tp,	
	xfs_rtblock_t		bno,	
	xfs_extlen_t		minlen,	
	xfs_extlen_t		maxlen,	
	xfs_extlen_t		*len,	
	int			wasdel,	
	xfs_extlen_t		prod,	
	xfs_rtblock_t		*rtblock); 


int					
xfs_rtfree_extent(
	struct xfs_trans	*tp,	
	xfs_rtblock_t		bno,	
	xfs_extlen_t		len);	


int					
xfs_rtmount_init(
	struct xfs_mount	*mp);	
void
xfs_rtunmount_inodes(
	struct xfs_mount	*mp);


int					
xfs_rtmount_inodes(
	struct xfs_mount	*mp);	


int					
xfs_rtpick_extent(
	struct xfs_mount	*mp,	
	struct xfs_trans	*tp,	
	xfs_extlen_t		len,	
	xfs_rtblock_t		*pick);	


int
xfs_growfs_rt(
	struct xfs_mount	*mp,	
	xfs_growfs_rt_t		*in);	


int xfs_rtbuf_get(struct xfs_mount *mp, struct xfs_trans *tp,
		  xfs_rtblock_t block, int issum, struct xfs_buf **bpp);
int xfs_rtcheck_range(struct xfs_mount *mp, struct xfs_trans *tp,
		      xfs_rtblock_t start, xfs_extlen_t len, int val,
		      xfs_rtblock_t *new, int *stat);
int xfs_rtfind_back(struct xfs_mount *mp, struct xfs_trans *tp,
		    xfs_rtblock_t start, xfs_rtblock_t limit,
		    xfs_rtblock_t *rtblock);
int xfs_rtfind_forw(struct xfs_mount *mp, struct xfs_trans *tp,
		    xfs_rtblock_t start, xfs_rtblock_t limit,
		    xfs_rtblock_t *rtblock);
int xfs_rtmodify_range(struct xfs_mount *mp, struct xfs_trans *tp,
		       xfs_rtblock_t start, xfs_extlen_t len, int val);
int xfs_rtmodify_summary_int(struct xfs_mount *mp, struct xfs_trans *tp,
			     int log, xfs_rtblock_t bbno, int delta,
			     struct xfs_buf **rbpp, xfs_fsblock_t *rsb,
			     xfs_suminfo_t *sum);
int xfs_rtmodify_summary(struct xfs_mount *mp, struct xfs_trans *tp, int log,
			 xfs_rtblock_t bbno, int delta, struct xfs_buf **rbpp,
			 xfs_fsblock_t *rsb);
int xfs_rtfree_range(struct xfs_mount *mp, struct xfs_trans *tp,
		     xfs_rtblock_t start, xfs_extlen_t len,
		     struct xfs_buf **rbpp, xfs_fsblock_t *rsb);
int xfs_rtalloc_query_range(struct xfs_mount *mp, struct xfs_trans *tp,
		const struct xfs_rtalloc_rec *low_rec,
		const struct xfs_rtalloc_rec *high_rec,
		xfs_rtalloc_query_range_fn fn, void *priv);
int xfs_rtalloc_query_all(struct xfs_mount *mp, struct xfs_trans *tp,
			  xfs_rtalloc_query_range_fn fn,
			  void *priv);
bool xfs_verify_rtbno(struct xfs_mount *mp, xfs_rtblock_t rtbno);
int xfs_rtalloc_extent_is_free(struct xfs_mount *mp, struct xfs_trans *tp,
			       xfs_rtblock_t start, xfs_extlen_t len,
			       bool *is_free);
int xfs_rtalloc_reinit_frextents(struct xfs_mount *mp);
#else
# define xfs_rtallocate_extent(t,b,min,max,l,f,p,rb)    (ENOSYS)
# define xfs_rtfree_extent(t,b,l)                       (ENOSYS)
# define xfs_rtpick_extent(m,t,l,rb)                    (ENOSYS)
# define xfs_growfs_rt(mp,in)                           (ENOSYS)
# define xfs_rtalloc_query_range(t,l,h,f,p)             (ENOSYS)
# define xfs_rtalloc_query_all(m,t,f,p)                 (ENOSYS)
# define xfs_rtbuf_get(m,t,b,i,p)                       (ENOSYS)
# define xfs_verify_rtbno(m, r)			(false)
# define xfs_rtalloc_extent_is_free(m,t,s,l,i)          (ENOSYS)
# define xfs_rtalloc_reinit_frextents(m)                (0)
static inline int		
xfs_rtmount_init(
	xfs_mount_t	*mp)	
{
	if (mp->m_sb.sb_rblocks == 0)
		return 0;

	xfs_warn(mp, "Not built with CONFIG_XFS_RT");
	return -ENOSYS;
}
# define xfs_rtmount_inodes(m)  (((mp)->m_sb.sb_rblocks == 0)? 0 : (ENOSYS))
# define xfs_rtunmount_inodes(m)
#endif	

#endif	
