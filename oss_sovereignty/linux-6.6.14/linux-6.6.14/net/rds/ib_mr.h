#ifndef _RDS_IB_MR_H
#define _RDS_IB_MR_H
#include <linux/kernel.h>
#include "rds.h"
#include "ib.h"
#define RDS_MR_1M_POOL_SIZE		(8192 / 2)
#define RDS_MR_1M_MSG_SIZE		256
#define RDS_MR_8K_MSG_SIZE		2
#define RDS_MR_8K_SCALE			(256 / (RDS_MR_8K_MSG_SIZE + 1))
#define RDS_MR_8K_POOL_SIZE		(RDS_MR_8K_SCALE * (8192 / 2))
enum rds_ib_fr_state {
	FRMR_IS_FREE,	 
	FRMR_IS_INUSE,	 
	FRMR_IS_STALE,	 
};
struct rds_ib_frmr {
	struct ib_mr		*mr;
	enum rds_ib_fr_state	fr_state;
	bool			fr_inv;
	wait_queue_head_t	fr_inv_done;
	bool			fr_reg;
	wait_queue_head_t	fr_reg_done;
	struct ib_send_wr	fr_wr;
	unsigned int		dma_npages;
	unsigned int		sg_byte_len;
};
struct rds_ib_mr {
	struct delayed_work		work;
	struct rds_ib_device		*device;
	struct rds_ib_mr_pool		*pool;
	struct rds_ib_connection	*ic;
	struct llist_node		llnode;
	struct list_head		unmap_list;
	unsigned int			remap_count;
	struct scatterlist		*sg;
	unsigned int			sg_len;
	int				sg_dma_len;
	u8				odp:1;
	union {
		struct rds_ib_frmr	frmr;
		struct ib_mr		*mr;
	} u;
};
struct rds_ib_mr_pool {
	unsigned int            pool_type;
	struct mutex		flush_lock;	 
	struct delayed_work	flush_worker;	 
	atomic_t		item_count;	 
	atomic_t		dirty_count;	 
	struct llist_head	drop_list;	 
	struct llist_head	free_list;	 
	struct llist_head	clean_list;	 
	wait_queue_head_t	flush_wait;
	spinlock_t		clean_lock;	 
	atomic_t		free_pinned;	 
	unsigned long		max_items;
	unsigned long		max_items_soft;
	unsigned long		max_free_pinned;
	unsigned int		max_pages;
};
extern struct workqueue_struct *rds_ib_mr_wq;
extern bool prefer_frmr;
struct rds_ib_mr_pool *rds_ib_create_mr_pool(struct rds_ib_device *rds_dev,
					     int npages);
void rds_ib_get_mr_info(struct rds_ib_device *rds_ibdev,
			struct rds_info_rdma_connection *iinfo);
void rds6_ib_get_mr_info(struct rds_ib_device *rds_ibdev,
			 struct rds6_info_rdma_connection *iinfo6);
void rds_ib_destroy_mr_pool(struct rds_ib_mr_pool *);
void *rds_ib_get_mr(struct scatterlist *sg, unsigned long nents,
		    struct rds_sock *rs, u32 *key_ret,
		    struct rds_connection *conn, u64 start, u64 length,
		    int need_odp);
void rds_ib_sync_mr(void *trans_private, int dir);
void rds_ib_free_mr(void *trans_private, int invalidate);
void rds_ib_flush_mrs(void);
int rds_ib_mr_init(void);
void rds_ib_mr_exit(void);
u32 rds_ib_get_lkey(void *trans_private);
void __rds_ib_teardown_mr(struct rds_ib_mr *);
void rds_ib_teardown_mr(struct rds_ib_mr *);
struct rds_ib_mr *rds_ib_reuse_mr(struct rds_ib_mr_pool *);
int rds_ib_flush_mr_pool(struct rds_ib_mr_pool *, int, struct rds_ib_mr **);
struct rds_ib_mr *rds_ib_try_reuse_ibmr(struct rds_ib_mr_pool *);
struct rds_ib_mr *rds_ib_reg_frmr(struct rds_ib_device *rds_ibdev,
				  struct rds_ib_connection *ic,
				  struct scatterlist *sg,
				  unsigned long nents, u32 *key);
void rds_ib_unreg_frmr(struct list_head *list, unsigned int *nfreed,
		       unsigned long *unpinned, unsigned int goal);
void rds_ib_free_frmr_list(struct rds_ib_mr *);
#endif
