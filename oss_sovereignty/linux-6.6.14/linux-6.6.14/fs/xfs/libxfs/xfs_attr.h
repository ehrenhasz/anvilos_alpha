#ifndef __XFS_ATTR_H__
#define	__XFS_ATTR_H__
struct xfs_inode;
struct xfs_da_args;
struct xfs_attr_list_context;
#define	ATTR_MAX_VALUELEN	(64*1024)	 
struct xfs_attrlist_cursor_kern {
	__u32	hashval;	 
	__u32	blkno;		 
	__u32	offset;		 
	__u16	pad1;		 
	__u8	pad2;		 
	__u8	initted;	 
};
typedef void (*put_listent_func_t)(struct xfs_attr_list_context *, int,
			      unsigned char *, int, int);
struct xfs_attr_list_context {
	struct xfs_trans	*tp;
	struct xfs_inode	*dp;		 
	struct xfs_attrlist_cursor_kern cursor;	 
	void			*buffer;	 
	int			seen_enough;
	bool			allow_incomplete;
	ssize_t			count;		 
	int			dupcnt;		 
	int			bufsize;	 
	int			firstu;		 
	unsigned int		attr_filter;	 
	int			resynch;	 
	put_listent_func_t	put_listent;	 
	int			index;		 
};
enum xfs_delattr_state {
	XFS_DAS_UNINIT		= 0,	 
	XFS_DAS_SF_ADD,			 
	XFS_DAS_SF_REMOVE,		 
	XFS_DAS_LEAF_ADD,		 
	XFS_DAS_LEAF_REMOVE,		 
	XFS_DAS_NODE_ADD,		 
	XFS_DAS_NODE_REMOVE,		 
	XFS_DAS_LEAF_SET_RMT,		 
	XFS_DAS_LEAF_ALLOC_RMT,		 
	XFS_DAS_LEAF_REPLACE,		 
	XFS_DAS_LEAF_REMOVE_OLD,	 
	XFS_DAS_LEAF_REMOVE_RMT,	 
	XFS_DAS_LEAF_REMOVE_ATTR,	 
	XFS_DAS_NODE_SET_RMT,		 
	XFS_DAS_NODE_ALLOC_RMT,		 
	XFS_DAS_NODE_REPLACE,		 
	XFS_DAS_NODE_REMOVE_OLD,	 
	XFS_DAS_NODE_REMOVE_RMT,	 
	XFS_DAS_NODE_REMOVE_ATTR,	 
	XFS_DAS_DONE,			 
};
#define XFS_DAS_STRINGS	\
	{ XFS_DAS_UNINIT,		"XFS_DAS_UNINIT" }, \
	{ XFS_DAS_SF_ADD,		"XFS_DAS_SF_ADD" }, \
	{ XFS_DAS_SF_REMOVE,		"XFS_DAS_SF_REMOVE" }, \
	{ XFS_DAS_LEAF_ADD,		"XFS_DAS_LEAF_ADD" }, \
	{ XFS_DAS_LEAF_REMOVE,		"XFS_DAS_LEAF_REMOVE" }, \
	{ XFS_DAS_NODE_ADD,		"XFS_DAS_NODE_ADD" }, \
	{ XFS_DAS_NODE_REMOVE,		"XFS_DAS_NODE_REMOVE" }, \
	{ XFS_DAS_LEAF_SET_RMT,		"XFS_DAS_LEAF_SET_RMT" }, \
	{ XFS_DAS_LEAF_ALLOC_RMT,	"XFS_DAS_LEAF_ALLOC_RMT" }, \
	{ XFS_DAS_LEAF_REPLACE,		"XFS_DAS_LEAF_REPLACE" }, \
	{ XFS_DAS_LEAF_REMOVE_OLD,	"XFS_DAS_LEAF_REMOVE_OLD" }, \
	{ XFS_DAS_LEAF_REMOVE_RMT,	"XFS_DAS_LEAF_REMOVE_RMT" }, \
	{ XFS_DAS_LEAF_REMOVE_ATTR,	"XFS_DAS_LEAF_REMOVE_ATTR" }, \
	{ XFS_DAS_NODE_SET_RMT,		"XFS_DAS_NODE_SET_RMT" }, \
	{ XFS_DAS_NODE_ALLOC_RMT,	"XFS_DAS_NODE_ALLOC_RMT" },  \
	{ XFS_DAS_NODE_REPLACE,		"XFS_DAS_NODE_REPLACE" },  \
	{ XFS_DAS_NODE_REMOVE_OLD,	"XFS_DAS_NODE_REMOVE_OLD" }, \
	{ XFS_DAS_NODE_REMOVE_RMT,	"XFS_DAS_NODE_REMOVE_RMT" }, \
	{ XFS_DAS_NODE_REMOVE_ATTR,	"XFS_DAS_NODE_REMOVE_ATTR" }, \
	{ XFS_DAS_DONE,			"XFS_DAS_DONE" }
struct xfs_attri_log_nameval;
struct xfs_attr_intent {
	struct list_head		xattri_list;
	struct xfs_da_state		*xattri_da_state;
	struct xfs_da_args		*xattri_da_args;
	struct xfs_attri_log_nameval	*xattri_nameval;
	enum xfs_delattr_state		xattri_dela_state;
	unsigned int			xattri_op_flags;
	xfs_dablk_t			xattri_lblkno;
	int				xattri_blkcnt;
	struct xfs_bmbt_irec		xattri_map;
};
int xfs_attr_inactive(struct xfs_inode *dp);
int xfs_attr_list_ilocked(struct xfs_attr_list_context *);
int xfs_attr_list(struct xfs_attr_list_context *);
int xfs_inode_hasattr(struct xfs_inode *ip);
bool xfs_attr_is_leaf(struct xfs_inode *ip);
int xfs_attr_get_ilocked(struct xfs_da_args *args);
int xfs_attr_get(struct xfs_da_args *args);
int xfs_attr_set(struct xfs_da_args *args);
int xfs_attr_set_iter(struct xfs_attr_intent *attr);
int xfs_attr_remove_iter(struct xfs_attr_intent *attr);
bool xfs_attr_namecheck(const void *name, size_t length);
int xfs_attr_calc_size(struct xfs_da_args *args, int *local);
void xfs_init_attr_trans(struct xfs_da_args *args, struct xfs_trans_res *tres,
			 unsigned int *total);
static inline bool
xfs_attr_is_shortform(
	struct xfs_inode    *ip)
{
	return ip->i_af.if_format == XFS_DINODE_FMT_LOCAL ||
	       (ip->i_af.if_format == XFS_DINODE_FMT_EXTENTS &&
		ip->i_af.if_nextents == 0);
}
static inline enum xfs_delattr_state
xfs_attr_init_add_state(struct xfs_da_args *args)
{
	if (!xfs_inode_has_attr_fork(args->dp))
		return XFS_DAS_DONE;
	args->op_flags |= XFS_DA_OP_ADDNAME;
	if (xfs_attr_is_shortform(args->dp))
		return XFS_DAS_SF_ADD;
	if (xfs_attr_is_leaf(args->dp))
		return XFS_DAS_LEAF_ADD;
	return XFS_DAS_NODE_ADD;
}
static inline enum xfs_delattr_state
xfs_attr_init_remove_state(struct xfs_da_args *args)
{
	args->op_flags |= XFS_DA_OP_REMOVE;
	if (xfs_attr_is_shortform(args->dp))
		return XFS_DAS_SF_REMOVE;
	if (xfs_attr_is_leaf(args->dp))
		return XFS_DAS_LEAF_REMOVE;
	return XFS_DAS_NODE_REMOVE;
}
static inline enum xfs_delattr_state
xfs_attr_init_replace_state(struct xfs_da_args *args)
{
	args->op_flags |= XFS_DA_OP_ADDNAME | XFS_DA_OP_REPLACE;
	if (args->op_flags & XFS_DA_OP_LOGGED)
		return xfs_attr_init_remove_state(args);
	return xfs_attr_init_add_state(args);
}
extern struct kmem_cache *xfs_attr_intent_cache;
int __init xfs_attr_intent_init_cache(void);
void xfs_attr_intent_destroy_cache(void);
#endif	 
