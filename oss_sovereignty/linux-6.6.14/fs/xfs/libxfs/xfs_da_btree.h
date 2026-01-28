#ifndef __XFS_DA_BTREE_H__
#define	__XFS_DA_BTREE_H__
struct xfs_inode;
struct xfs_trans;
struct xfs_da_geometry {
	unsigned int	blksize;	 
	unsigned int	fsbcount;	 
	uint8_t		fsblog;		 
	uint8_t		blklog;		 
	unsigned int	node_hdr_size;	 
	unsigned int	node_ents;	 
	unsigned int	magicpct;	 
	xfs_dablk_t	datablk;	 
	unsigned int	leaf_hdr_size;	 
	unsigned int	leaf_max_ents;	 
	xfs_dablk_t	leafblk;	 
	unsigned int	free_hdr_size;	 
	unsigned int	free_max_bests;	 
	xfs_dablk_t	freeblk;	 
	xfs_extnum_t	max_extents;	 
	xfs_dir2_data_aoff_t data_first_offset;
	size_t		data_entry_offset;
};
enum xfs_dacmp {
	XFS_CMP_DIFFERENT,	 
	XFS_CMP_EXACT,		 
	XFS_CMP_CASE		 
};
typedef struct xfs_da_args {
	struct xfs_da_geometry *geo;	 
	const uint8_t		*name;		 
	int		namelen;	 
	uint8_t		filetype;	 
	void		*value;		 
	int		valuelen;	 
	unsigned int	attr_filter;	 
	unsigned int	attr_flags;	 
	xfs_dahash_t	hashval;	 
	xfs_ino_t	inumber;	 
	struct xfs_inode *dp;		 
	struct xfs_trans *trans;	 
	xfs_extlen_t	total;		 
	int		whichfork;	 
	xfs_dablk_t	blkno;		 
	int		index;		 
	xfs_dablk_t	rmtblkno;	 
	int		rmtblkcnt;	 
	int		rmtvaluelen;	 
	xfs_dablk_t	blkno2;		 
	int		index2;		 
	xfs_dablk_t	rmtblkno2;	 
	int		rmtblkcnt2;	 
	int		rmtvaluelen2;	 
	uint32_t	op_flags;	 
	enum xfs_dacmp	cmpresult;	 
} xfs_da_args_t;
#define XFS_DA_OP_JUSTCHECK	(1u << 0)  
#define XFS_DA_OP_REPLACE	(1u << 1)  
#define XFS_DA_OP_ADDNAME	(1u << 2)  
#define XFS_DA_OP_OKNOENT	(1u << 3)  
#define XFS_DA_OP_CILOOKUP	(1u << 4)  
#define XFS_DA_OP_NOTIME	(1u << 5)  
#define XFS_DA_OP_REMOVE	(1u << 6)  
#define XFS_DA_OP_RECOVERY	(1u << 7)  
#define XFS_DA_OP_LOGGED	(1u << 8)  
#define XFS_DA_OP_FLAGS \
	{ XFS_DA_OP_JUSTCHECK,	"JUSTCHECK" }, \
	{ XFS_DA_OP_REPLACE,	"REPLACE" }, \
	{ XFS_DA_OP_ADDNAME,	"ADDNAME" }, \
	{ XFS_DA_OP_OKNOENT,	"OKNOENT" }, \
	{ XFS_DA_OP_CILOOKUP,	"CILOOKUP" }, \
	{ XFS_DA_OP_NOTIME,	"NOTIME" }, \
	{ XFS_DA_OP_REMOVE,	"REMOVE" }, \
	{ XFS_DA_OP_RECOVERY,	"RECOVERY" }, \
	{ XFS_DA_OP_LOGGED,	"LOGGED" }
typedef struct xfs_da_state_blk {
	struct xfs_buf	*bp;		 
	xfs_dablk_t	blkno;		 
	xfs_daddr_t	disk_blkno;	 
	int		index;		 
	xfs_dahash_t	hashval;	 
	int		magic;		 
} xfs_da_state_blk_t;
typedef struct xfs_da_state_path {
	int			active;		 
	xfs_da_state_blk_t	blk[XFS_DA_NODE_MAXDEPTH];
} xfs_da_state_path_t;
typedef struct xfs_da_state {
	xfs_da_args_t		*args;		 
	struct xfs_mount	*mp;		 
	xfs_da_state_path_t	path;		 
	xfs_da_state_path_t	altpath;	 
	unsigned char		inleaf;		 
	unsigned char		extravalid;	 
	unsigned char		extraafter;	 
	xfs_da_state_blk_t	extrablk;	 
} xfs_da_state_t;
struct xfs_da3_icnode_hdr {
	uint32_t		forw;
	uint32_t		back;
	uint16_t		magic;
	uint16_t		count;
	uint16_t		level;
	struct xfs_da_node_entry *btree;
};
#define XFS_DA_LOGOFF(BASE, ADDR)	((char *)(ADDR) - (char *)(BASE))
#define XFS_DA_LOGRANGE(BASE, ADDR, SIZE)	\
		(uint)(XFS_DA_LOGOFF(BASE, ADDR)), \
		(uint)(XFS_DA_LOGOFF(BASE, ADDR)+(SIZE)-1)
int	xfs_da3_node_create(struct xfs_da_args *args, xfs_dablk_t blkno,
			    int level, struct xfs_buf **bpp, int whichfork);
int	xfs_da3_split(xfs_da_state_t *state);
int	xfs_da3_join(xfs_da_state_t *state);
void	xfs_da3_fixhashpath(struct xfs_da_state *state,
			    struct xfs_da_state_path *path_to_to_fix);
int	xfs_da3_node_lookup_int(xfs_da_state_t *state, int *result);
int	xfs_da3_path_shift(xfs_da_state_t *state, xfs_da_state_path_t *path,
					 int forward, int release, int *result);
int	xfs_da3_blk_link(xfs_da_state_t *state, xfs_da_state_blk_t *old_blk,
				       xfs_da_state_blk_t *new_blk);
int	xfs_da3_node_read(struct xfs_trans *tp, struct xfs_inode *dp,
			xfs_dablk_t bno, struct xfs_buf **bpp, int whichfork);
int	xfs_da3_node_read_mapped(struct xfs_trans *tp, struct xfs_inode *dp,
			xfs_daddr_t mappedbno, struct xfs_buf **bpp,
			int whichfork);
#define XFS_DABUF_MAP_HOLE_OK	(1u << 0)
int	xfs_da_grow_inode(xfs_da_args_t *args, xfs_dablk_t *new_blkno);
int	xfs_da_grow_inode_int(struct xfs_da_args *args, xfs_fileoff_t *bno,
			      int count);
int	xfs_da_get_buf(struct xfs_trans *trans, struct xfs_inode *dp,
		xfs_dablk_t bno, struct xfs_buf **bp, int whichfork);
int	xfs_da_read_buf(struct xfs_trans *trans, struct xfs_inode *dp,
		xfs_dablk_t bno, unsigned int flags, struct xfs_buf **bpp,
		int whichfork, const struct xfs_buf_ops *ops);
int	xfs_da_reada_buf(struct xfs_inode *dp, xfs_dablk_t bno,
		unsigned int flags, int whichfork,
		const struct xfs_buf_ops *ops);
int	xfs_da_shrink_inode(xfs_da_args_t *args, xfs_dablk_t dead_blkno,
					  struct xfs_buf *dead_buf);
uint xfs_da_hashname(const uint8_t *name_string, int name_length);
enum xfs_dacmp xfs_da_compname(struct xfs_da_args *args,
				const unsigned char *name, int len);
struct xfs_da_state *xfs_da_state_alloc(struct xfs_da_args *args);
void xfs_da_state_free(xfs_da_state_t *state);
void xfs_da_state_reset(struct xfs_da_state *state, struct xfs_da_args *args);
void	xfs_da3_node_hdr_from_disk(struct xfs_mount *mp,
		struct xfs_da3_icnode_hdr *to, struct xfs_da_intnode *from);
void	xfs_da3_node_hdr_to_disk(struct xfs_mount *mp,
		struct xfs_da_intnode *to, struct xfs_da3_icnode_hdr *from);
extern struct kmem_cache	*xfs_da_state_cache;
#endif	 
