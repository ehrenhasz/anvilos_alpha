 
 
#ifndef __XFS_BMAP_H__
#define	__XFS_BMAP_H__

struct getbmap;
struct xfs_bmbt_irec;
struct xfs_ifork;
struct xfs_inode;
struct xfs_mount;
struct xfs_trans;
struct xfs_alloc_arg;

 
struct xfs_bmalloca {
	struct xfs_trans	*tp;	 
	struct xfs_inode	*ip;	 
	struct xfs_bmbt_irec	prev;	 
	struct xfs_bmbt_irec	got;	 

	xfs_fileoff_t		offset;	 
	xfs_extlen_t		length;	 
	xfs_fsblock_t		blkno;	 

	struct xfs_btree_cur	*cur;	 
	struct xfs_iext_cursor	icur;	 
	int			nallocs; 
	int			logflags; 

	xfs_extlen_t		total;	 
	xfs_extlen_t		minlen;	 
	xfs_extlen_t		minleft;  
	bool			eof;	 
	bool			wasdel;	 
	bool			aeof;	 
	bool			conv;	 
	int			datatype; 
	uint32_t		flags;
};

#define	XFS_BMAP_MAX_NMAP	4

 
#define XFS_BMAPI_ENTIRE	(1u << 0)  
#define XFS_BMAPI_METADATA	(1u << 1)  
#define XFS_BMAPI_ATTRFORK	(1u << 2)  
#define XFS_BMAPI_PREALLOC	(1u << 3)  
#define XFS_BMAPI_CONTIG	(1u << 4)  
 
#define XFS_BMAPI_CONVERT	(1u << 5)

 
#define XFS_BMAPI_ZERO		(1u << 6)

 
#define XFS_BMAPI_REMAP		(1u << 7)

 
#define XFS_BMAPI_COWFORK	(1u << 8)

 
#define XFS_BMAPI_NODISCARD	(1u << 9)

 
#define XFS_BMAPI_NORMAP	(1u << 10)

#define XFS_BMAPI_FLAGS \
	{ XFS_BMAPI_ENTIRE,	"ENTIRE" }, \
	{ XFS_BMAPI_METADATA,	"METADATA" }, \
	{ XFS_BMAPI_ATTRFORK,	"ATTRFORK" }, \
	{ XFS_BMAPI_PREALLOC,	"PREALLOC" }, \
	{ XFS_BMAPI_CONTIG,	"CONTIG" }, \
	{ XFS_BMAPI_CONVERT,	"CONVERT" }, \
	{ XFS_BMAPI_ZERO,	"ZERO" }, \
	{ XFS_BMAPI_REMAP,	"REMAP" }, \
	{ XFS_BMAPI_COWFORK,	"COWFORK" }, \
	{ XFS_BMAPI_NODISCARD,	"NODISCARD" }, \
	{ XFS_BMAPI_NORMAP,	"NORMAP" }


static inline int xfs_bmapi_aflag(int w)
{
	return (w == XFS_ATTR_FORK ? XFS_BMAPI_ATTRFORK :
	       (w == XFS_COW_FORK ? XFS_BMAPI_COWFORK : 0));
}

static inline int xfs_bmapi_whichfork(uint32_t bmapi_flags)
{
	if (bmapi_flags & XFS_BMAPI_COWFORK)
		return XFS_COW_FORK;
	else if (bmapi_flags & XFS_BMAPI_ATTRFORK)
		return XFS_ATTR_FORK;
	return XFS_DATA_FORK;
}

 
#define	DELAYSTARTBLOCK		((xfs_fsblock_t)-1LL)
#define	HOLESTARTBLOCK		((xfs_fsblock_t)-2LL)

 
#define BMAP_LEFT_CONTIG	(1u << 0)
#define BMAP_RIGHT_CONTIG	(1u << 1)
#define BMAP_LEFT_FILLING	(1u << 2)
#define BMAP_RIGHT_FILLING	(1u << 3)
#define BMAP_LEFT_DELAY		(1u << 4)
#define BMAP_RIGHT_DELAY	(1u << 5)
#define BMAP_LEFT_VALID		(1u << 6)
#define BMAP_RIGHT_VALID	(1u << 7)
#define BMAP_ATTRFORK		(1u << 8)
#define BMAP_COWFORK		(1u << 9)

#define XFS_BMAP_EXT_FLAGS \
	{ BMAP_LEFT_CONTIG,	"LC" }, \
	{ BMAP_RIGHT_CONTIG,	"RC" }, \
	{ BMAP_LEFT_FILLING,	"LF" }, \
	{ BMAP_RIGHT_FILLING,	"RF" }, \
	{ BMAP_ATTRFORK,	"ATTR" }, \
	{ BMAP_COWFORK,		"COW" }

 
static inline bool xfs_bmap_is_real_extent(const struct xfs_bmbt_irec *irec)
{
	return irec->br_startblock != HOLESTARTBLOCK &&
		irec->br_startblock != DELAYSTARTBLOCK &&
		!isnullstartblock(irec->br_startblock);
}

 
static inline bool xfs_bmap_is_written_extent(struct xfs_bmbt_irec *irec)
{
	return xfs_bmap_is_real_extent(irec) &&
	       irec->br_state != XFS_EXT_UNWRITTEN;
}

 
#define xfs_valid_startblock(ip, startblock) \
	((startblock) != 0 || XFS_IS_REALTIME_INODE(ip))

int	xfs_bmap_longest_free_extent(struct xfs_perag *pag,
		struct xfs_trans *tp, xfs_extlen_t *blen);
void	xfs_trim_extent(struct xfs_bmbt_irec *irec, xfs_fileoff_t bno,
		xfs_filblks_t len);
unsigned int xfs_bmap_compute_attr_offset(struct xfs_mount *mp);
int	xfs_bmap_add_attrfork(struct xfs_inode *ip, int size, int rsvd);
void	xfs_bmap_local_to_extents_empty(struct xfs_trans *tp,
		struct xfs_inode *ip, int whichfork);
void	xfs_bmap_compute_maxlevels(struct xfs_mount *mp, int whichfork);
int	xfs_bmap_first_unused(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_extlen_t len, xfs_fileoff_t *unused, int whichfork);
int	xfs_bmap_last_before(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t *last_block, int whichfork);
int	xfs_bmap_last_offset(struct xfs_inode *ip, xfs_fileoff_t *unused,
		int whichfork);
int	xfs_bmapi_read(struct xfs_inode *ip, xfs_fileoff_t bno,
		xfs_filblks_t len, struct xfs_bmbt_irec *mval,
		int *nmap, uint32_t flags);
int	xfs_bmapi_write(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t bno, xfs_filblks_t len, uint32_t flags,
		xfs_extlen_t total, struct xfs_bmbt_irec *mval, int *nmap);
int	__xfs_bunmapi(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t bno, xfs_filblks_t *rlen, uint32_t flags,
		xfs_extnum_t nexts);
int	xfs_bunmapi(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t bno, xfs_filblks_t len, uint32_t flags,
		xfs_extnum_t nexts, int *done);
int	xfs_bmap_del_extent_delay(struct xfs_inode *ip, int whichfork,
		struct xfs_iext_cursor *cur, struct xfs_bmbt_irec *got,
		struct xfs_bmbt_irec *del);
void	xfs_bmap_del_extent_cow(struct xfs_inode *ip,
		struct xfs_iext_cursor *cur, struct xfs_bmbt_irec *got,
		struct xfs_bmbt_irec *del);
uint	xfs_default_attroffset(struct xfs_inode *ip);
int	xfs_bmap_collapse_extents(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t *next_fsb, xfs_fileoff_t offset_shift_fsb,
		bool *done);
int	xfs_bmap_can_insert_extents(struct xfs_inode *ip, xfs_fileoff_t off,
		xfs_fileoff_t shift);
int	xfs_bmap_insert_extents(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t *next_fsb, xfs_fileoff_t offset_shift_fsb,
		bool *done, xfs_fileoff_t stop_fsb);
int	xfs_bmap_split_extent(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t split_offset);
int	xfs_bmapi_reserve_delalloc(struct xfs_inode *ip, int whichfork,
		xfs_fileoff_t off, xfs_filblks_t len, xfs_filblks_t prealloc,
		struct xfs_bmbt_irec *got, struct xfs_iext_cursor *cur,
		int eof);
int	xfs_bmapi_convert_delalloc(struct xfs_inode *ip, int whichfork,
		xfs_off_t offset, struct iomap *iomap, unsigned int *seq);
int	xfs_bmap_add_extent_unwritten_real(struct xfs_trans *tp,
		struct xfs_inode *ip, int whichfork,
		struct xfs_iext_cursor *icur, struct xfs_btree_cur **curp,
		struct xfs_bmbt_irec *new, int *logflagsp);
xfs_extlen_t xfs_bmapi_minleft(struct xfs_trans *tp, struct xfs_inode *ip,
		int fork);
int	xfs_bmap_btalloc_low_space(struct xfs_bmalloca *ap,
		struct xfs_alloc_arg *args);

enum xfs_bmap_intent_type {
	XFS_BMAP_MAP = 1,
	XFS_BMAP_UNMAP,
};

struct xfs_bmap_intent {
	struct list_head			bi_list;
	enum xfs_bmap_intent_type		bi_type;
	int					bi_whichfork;
	struct xfs_inode			*bi_owner;
	struct xfs_perag			*bi_pag;
	struct xfs_bmbt_irec			bi_bmap;
};

void xfs_bmap_update_get_group(struct xfs_mount *mp,
		struct xfs_bmap_intent *bi);

int	xfs_bmap_finish_one(struct xfs_trans *tp, struct xfs_bmap_intent *bi);
void	xfs_bmap_map_extent(struct xfs_trans *tp, struct xfs_inode *ip,
		struct xfs_bmbt_irec *imap);
void	xfs_bmap_unmap_extent(struct xfs_trans *tp, struct xfs_inode *ip,
		struct xfs_bmbt_irec *imap);

static inline uint32_t xfs_bmap_fork_to_state(int whichfork)
{
	switch (whichfork) {
	case XFS_ATTR_FORK:
		return BMAP_ATTRFORK;
	case XFS_COW_FORK:
		return BMAP_COWFORK;
	default:
		return 0;
	}
}

xfs_failaddr_t xfs_bmap_validate_extent(struct xfs_inode *ip, int whichfork,
		struct xfs_bmbt_irec *irec);
int xfs_bmap_complain_bad_rec(struct xfs_inode *ip, int whichfork,
		xfs_failaddr_t fa, const struct xfs_bmbt_irec *irec);

int	xfs_bmapi_remap(struct xfs_trans *tp, struct xfs_inode *ip,
		xfs_fileoff_t bno, xfs_filblks_t len, xfs_fsblock_t startblock,
		uint32_t flags);

extern struct kmem_cache	*xfs_bmap_intent_cache;

int __init xfs_bmap_intent_init_cache(void);
void xfs_bmap_intent_destroy_cache(void);

#endif	 
