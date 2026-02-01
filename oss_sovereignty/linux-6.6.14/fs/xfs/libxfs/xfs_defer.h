 
 
#ifndef __XFS_DEFER_H__
#define	__XFS_DEFER_H__

struct xfs_btree_cur;
struct xfs_defer_op_type;
struct xfs_defer_capture;

 
enum xfs_defer_ops_type {
	XFS_DEFER_OPS_TYPE_BMAP,
	XFS_DEFER_OPS_TYPE_REFCOUNT,
	XFS_DEFER_OPS_TYPE_RMAP,
	XFS_DEFER_OPS_TYPE_FREE,
	XFS_DEFER_OPS_TYPE_AGFL_FREE,
	XFS_DEFER_OPS_TYPE_ATTR,
	XFS_DEFER_OPS_TYPE_MAX,
};

 
struct xfs_defer_pending {
	struct list_head		dfp_list;	 
	struct list_head		dfp_work;	 
	struct xfs_log_item		*dfp_intent;	 
	struct xfs_log_item		*dfp_done;	 
	unsigned int			dfp_count;	 
	enum xfs_defer_ops_type		dfp_type;
};

void xfs_defer_add(struct xfs_trans *tp, enum xfs_defer_ops_type type,
		struct list_head *h);
int xfs_defer_finish_noroll(struct xfs_trans **tp);
int xfs_defer_finish(struct xfs_trans **tp);
void xfs_defer_cancel(struct xfs_trans *);
void xfs_defer_move(struct xfs_trans *dtp, struct xfs_trans *stp);

 
struct xfs_defer_op_type {
	struct xfs_log_item *(*create_intent)(struct xfs_trans *tp,
			struct list_head *items, unsigned int count, bool sort);
	void (*abort_intent)(struct xfs_log_item *intent);
	struct xfs_log_item *(*create_done)(struct xfs_trans *tp,
			struct xfs_log_item *intent, unsigned int count);
	int (*finish_item)(struct xfs_trans *tp, struct xfs_log_item *done,
			struct list_head *item, struct xfs_btree_cur **state);
	void (*finish_cleanup)(struct xfs_trans *tp,
			struct xfs_btree_cur *state, int error);
	void (*cancel_item)(struct list_head *item);
	unsigned int		max_items;
};

extern const struct xfs_defer_op_type xfs_bmap_update_defer_type;
extern const struct xfs_defer_op_type xfs_refcount_update_defer_type;
extern const struct xfs_defer_op_type xfs_rmap_update_defer_type;
extern const struct xfs_defer_op_type xfs_extent_free_defer_type;
extern const struct xfs_defer_op_type xfs_agfl_free_defer_type;
extern const struct xfs_defer_op_type xfs_attr_defer_type;


 
#define XFS_DEFER_OPS_NR_INODES	2	 
#define XFS_DEFER_OPS_NR_BUFS	2	 

 
struct xfs_defer_resources {
	 
	struct xfs_buf		*dr_bp[XFS_DEFER_OPS_NR_BUFS];

	 
	struct xfs_inode	*dr_ip[XFS_DEFER_OPS_NR_INODES];

	 
	unsigned short		dr_bufs;

	 
	unsigned short		dr_ordered;

	 
	unsigned short		dr_inos;
};

 
struct xfs_defer_capture {
	 
	struct list_head	dfc_list;

	 
	struct list_head	dfc_dfops;
	unsigned int		dfc_tpflags;

	 
	unsigned int		dfc_blkres;
	unsigned int		dfc_rtxres;

	 
	unsigned int		dfc_logres;

	struct xfs_defer_resources dfc_held;
};

 
int xfs_defer_ops_capture_and_commit(struct xfs_trans *tp,
		struct list_head *capture_list);
void xfs_defer_ops_continue(struct xfs_defer_capture *d, struct xfs_trans *tp,
		struct xfs_defer_resources *dres);
void xfs_defer_ops_capture_free(struct xfs_mount *mp,
		struct xfs_defer_capture *d);
void xfs_defer_resources_rele(struct xfs_defer_resources *dres);

int __init xfs_defer_init_item_caches(void);
void xfs_defer_destroy_item_caches(void);

#endif  
