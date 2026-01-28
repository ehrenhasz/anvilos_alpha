

#ifndef	__XFS_LOG_RECOVER_H__
#define __XFS_LOG_RECOVER_H__


struct xlog_recover_item;


enum xlog_recover_reorder {
	XLOG_REORDER_BUFFER_LIST,
	XLOG_REORDER_ITEM_LIST,
	XLOG_REORDER_INODE_BUFFER_LIST,
	XLOG_REORDER_CANCEL_LIST,
};

struct xlog_recover_item_ops {
	uint16_t	item_type;	

	
	enum xlog_recover_reorder (*reorder)(struct xlog_recover_item *item);

	
	void (*ra_pass2)(struct xlog *log, struct xlog_recover_item *item);

	
	int (*commit_pass1)(struct xlog *log, struct xlog_recover_item *item);

	
	int (*commit_pass2)(struct xlog *log, struct list_head *buffer_list,
			    struct xlog_recover_item *item, xfs_lsn_t lsn);
};

extern const struct xlog_recover_item_ops xlog_icreate_item_ops;
extern const struct xlog_recover_item_ops xlog_buf_item_ops;
extern const struct xlog_recover_item_ops xlog_inode_item_ops;
extern const struct xlog_recover_item_ops xlog_dquot_item_ops;
extern const struct xlog_recover_item_ops xlog_quotaoff_item_ops;
extern const struct xlog_recover_item_ops xlog_bui_item_ops;
extern const struct xlog_recover_item_ops xlog_bud_item_ops;
extern const struct xlog_recover_item_ops xlog_efi_item_ops;
extern const struct xlog_recover_item_ops xlog_efd_item_ops;
extern const struct xlog_recover_item_ops xlog_rui_item_ops;
extern const struct xlog_recover_item_ops xlog_rud_item_ops;
extern const struct xlog_recover_item_ops xlog_cui_item_ops;
extern const struct xlog_recover_item_ops xlog_cud_item_ops;
extern const struct xlog_recover_item_ops xlog_attri_item_ops;
extern const struct xlog_recover_item_ops xlog_attrd_item_ops;



#define XLOG_RHASH_BITS  4
#define XLOG_RHASH_SIZE	16
#define XLOG_RHASH_SHIFT 2
#define XLOG_RHASH(tid)	\
	((((uint32_t)tid)>>XLOG_RHASH_SHIFT) & (XLOG_RHASH_SIZE-1))

#define XLOG_MAX_REGIONS_IN_ITEM   (XFS_MAX_BLOCKSIZE / XFS_BLF_CHUNK / 2 + 1)



struct xlog_recover_item {
	struct list_head	ri_list;
	int			ri_cnt;	
	int			ri_total;	
	struct xfs_log_iovec	*ri_buf;	
	const struct xlog_recover_item_ops *ri_ops;
};

struct xlog_recover {
	struct hlist_node	r_list;
	xlog_tid_t		r_log_tid;	
	xfs_trans_header_t	r_theader;	
	int			r_state;	
	xfs_lsn_t		r_lsn;		
	struct list_head	r_itemq;	
};

#define ITEM_TYPE(i)	(*(unsigned short *)(i)->ri_buf[0].i_addr)

#define	XLOG_RECOVER_CRCPASS	0
#define	XLOG_RECOVER_PASS1	1
#define	XLOG_RECOVER_PASS2	2

void xlog_buf_readahead(struct xlog *log, xfs_daddr_t blkno, uint len,
		const struct xfs_buf_ops *ops);
bool xlog_is_buffer_cancelled(struct xlog *log, xfs_daddr_t blkno, uint len);

int xlog_recover_iget(struct xfs_mount *mp, xfs_ino_t ino,
		struct xfs_inode **ipp);
void xlog_recover_release_intent(struct xlog *log, unsigned short intent_type,
		uint64_t intent_id);
int xlog_alloc_buf_cancel_table(struct xlog *log);
void xlog_free_buf_cancel_table(struct xlog *log);

#ifdef DEBUG
void xlog_check_buf_cancel_table(struct xlog *log);
#else
#define xlog_check_buf_cancel_table(log) do { } while (0)
#endif


static inline struct xfs_trans_res
xlog_recover_resv(const struct xfs_trans_res *r)
{
	struct xfs_trans_res ret = {
		.tr_logres	= r->tr_logres,
		.tr_logcount	= 1,
		.tr_logflags	= r->tr_logflags,
	};

	return ret;
}

#endif	
