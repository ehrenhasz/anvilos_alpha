 

#ifndef MTHCA_PROVIDER_H
#define MTHCA_PROVIDER_H

#include <rdma/ib_verbs.h>
#include <rdma/ib_pack.h>

#define MTHCA_MPT_FLAG_ATOMIC        (1 << 14)
#define MTHCA_MPT_FLAG_REMOTE_WRITE  (1 << 13)
#define MTHCA_MPT_FLAG_REMOTE_READ   (1 << 12)
#define MTHCA_MPT_FLAG_LOCAL_WRITE   (1 << 11)
#define MTHCA_MPT_FLAG_LOCAL_READ    (1 << 10)

struct mthca_buf_list {
	void *buf;
	DEFINE_DMA_UNMAP_ADDR(mapping);
};

union mthca_buf {
	struct mthca_buf_list direct;
	struct mthca_buf_list *page_list;
};

struct mthca_uar {
	unsigned long pfn;
	int           index;
};

struct mthca_user_db_table;

struct mthca_ucontext {
	struct ib_ucontext          ibucontext;
	struct mthca_uar            uar;
	struct mthca_user_db_table *db_tab;
	int			    reg_mr_warned;
};

struct mthca_mtt;

struct mthca_mr {
	struct ib_mr      ibmr;
	struct ib_umem   *umem;
	struct mthca_mtt *mtt;
};

struct mthca_pd {
	struct ib_pd    ibpd;
	u32             pd_num;
	atomic_t        sqp_count;
	struct mthca_mr ntmr;
	int             privileged;
};

struct mthca_eq {
	struct mthca_dev      *dev;
	int                    eqn;
	u32                    eqn_mask;
	u32                    cons_index;
	u16                    msi_x_vector;
	u16                    msi_x_entry;
	int                    have_irq;
	int                    nent;
	struct mthca_buf_list *page_list;
	struct mthca_mr        mr;
	char		       irq_name[IB_DEVICE_NAME_MAX];
};

struct mthca_av;

enum mthca_ah_type {
	MTHCA_AH_ON_HCA,
	MTHCA_AH_PCI_POOL,
	MTHCA_AH_KMALLOC
};

struct mthca_ah {
	struct ib_ah       ibah;
	enum mthca_ah_type type;
	u32                key;
	struct mthca_av   *av;
	dma_addr_t         avdma;
};

 

struct mthca_cq_buf {
	union mthca_buf		queue;
	struct mthca_mr		mr;
	int			is_direct;
};

struct mthca_cq_resize {
	struct mthca_cq_buf	buf;
	int			cqe;
	enum {
		CQ_RESIZE_ALLOC,
		CQ_RESIZE_READY,
		CQ_RESIZE_SWAPPED
	}			state;
};

struct mthca_cq {
	struct ib_cq		ibcq;
	spinlock_t		lock;
	int			refcount;
	int			cqn;
	u32			cons_index;
	struct mthca_cq_buf	buf;
	struct mthca_cq_resize *resize_buf;
	int			is_kernel;

	 
	int			set_ci_db_index;
	__be32		       *set_ci_db;
	int			arm_db_index;
	__be32		       *arm_db;
	int			arm_sn;

	wait_queue_head_t	wait;
	struct mutex		mutex;
};

struct mthca_srq {
	struct ib_srq		ibsrq;
	spinlock_t		lock;
	int			refcount;
	int			srqn;
	int			max;
	int			max_gs;
	int			wqe_shift;
	int			first_free;
	int			last_free;
	u16			counter;   
	int			db_index;  
	__be32		       *db;        
	void		       *last;

	int			is_direct;
	u64		       *wrid;
	union mthca_buf		queue;
	struct mthca_mr		mr;

	wait_queue_head_t	wait;
	struct mutex		mutex;
};

struct mthca_wq {
	spinlock_t lock;
	int        max;
	unsigned   next_ind;
	unsigned   last_comp;
	unsigned   head;
	unsigned   tail;
	void      *last;
	int        max_gs;
	int        wqe_shift;

	int        db_index;	 
	__be32    *db;
};

struct mthca_sqp {
	int             pkey_index;
	u32             qkey;
	u32             send_psn;
	struct ib_ud_header ud_header;
	int             header_buf_size;
	void           *header_buf;
	dma_addr_t      header_dma;
};

struct mthca_qp {
	struct ib_qp           ibqp;
	int                    refcount;
	u32                    qpn;
	int                    is_direct;
	u8                     port;  
	u8                     alt_port;  
	u8                     transport;
	u8                     state;
	u8                     atomic_rd_en;
	u8                     resp_depth;

	struct mthca_mr        mr;

	struct mthca_wq        rq;
	struct mthca_wq        sq;
	enum ib_sig_type       sq_policy;
	int                    send_wqe_offset;
	int                    max_inline_data;

	u64                   *wrid;
	union mthca_buf	       queue;

	wait_queue_head_t      wait;
	struct mutex	       mutex;
	struct mthca_sqp *sqp;
};

static inline struct mthca_ucontext *to_mucontext(struct ib_ucontext *ibucontext)
{
	return container_of(ibucontext, struct mthca_ucontext, ibucontext);
}

static inline struct mthca_mr *to_mmr(struct ib_mr *ibmr)
{
	return container_of(ibmr, struct mthca_mr, ibmr);
}

static inline struct mthca_pd *to_mpd(struct ib_pd *ibpd)
{
	return container_of(ibpd, struct mthca_pd, ibpd);
}

static inline struct mthca_ah *to_mah(struct ib_ah *ibah)
{
	return container_of(ibah, struct mthca_ah, ibah);
}

static inline struct mthca_cq *to_mcq(struct ib_cq *ibcq)
{
	return container_of(ibcq, struct mthca_cq, ibcq);
}

static inline struct mthca_srq *to_msrq(struct ib_srq *ibsrq)
{
	return container_of(ibsrq, struct mthca_srq, ibsrq);
}

static inline struct mthca_qp *to_mqp(struct ib_qp *ibqp)
{
	return container_of(ibqp, struct mthca_qp, ibqp);
}

#endif  
