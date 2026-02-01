 
 
#ifndef __FSL_QBMAN_PORTAL_H
#define __FSL_QBMAN_PORTAL_H

#include <soc/fsl/dpaa2-fd.h>

#define QMAN_REV_4000   0x04000000
#define QMAN_REV_4100   0x04010000
#define QMAN_REV_4101   0x04010001
#define QMAN_REV_5000   0x05000000

#define QMAN_REV_MASK   0xffff0000

struct dpaa2_dq;
struct qbman_swp;

 
struct qbman_swp_desc {
	void *cena_bar;  
	void __iomem *cinh_bar;  
	u32 qman_version;
	u32 qman_clk;
	u32 qman_256_cycles_per_ns;
};

#define QBMAN_SWP_INTERRUPT_EQRI 0x01
#define QBMAN_SWP_INTERRUPT_EQDI 0x02
#define QBMAN_SWP_INTERRUPT_DQRI 0x04
#define QBMAN_SWP_INTERRUPT_RCRI 0x08
#define QBMAN_SWP_INTERRUPT_RCDI 0x10
#define QBMAN_SWP_INTERRUPT_VDCI 0x20

 
struct qbman_pull_desc {
	u8 verb;
	u8 numf;
	u8 tok;
	u8 reserved;
	__le32 dq_src;
	__le64 rsp_addr;
	u64 rsp_addr_virt;
	u8 padding[40];
};

enum qbman_pull_type_e {
	 
	qbman_pull_type_prio = 1,
	 
	qbman_pull_type_active,
	 
	qbman_pull_type_active_noics
};

 
#define QBMAN_RESULT_MASK      0x7f
#define QBMAN_RESULT_DQ        0x60
#define QBMAN_RESULT_FQRN      0x21
#define QBMAN_RESULT_FQRNI     0x22
#define QBMAN_RESULT_FQPN      0x24
#define QBMAN_RESULT_FQDAN     0x25
#define QBMAN_RESULT_CDAN      0x26
#define QBMAN_RESULT_CSCN_MEM  0x27
#define QBMAN_RESULT_CGCU      0x28
#define QBMAN_RESULT_BPSCN     0x29
#define QBMAN_RESULT_CSCN_WQ   0x2a

 
#define QBMAN_FQ_SCHEDULE	0x48
#define QBMAN_FQ_FORCE		0x49
#define QBMAN_FQ_XON		0x4d
#define QBMAN_FQ_XOFF		0x4e

 
struct qbman_eq_desc {
	u8 verb;
	u8 dca;
	__le16 seqnum;
	__le16 orpid;
	__le16 reserved1;
	__le32 tgtid;
	__le32 tag;
	__le16 qdbin;
	u8 qpri;
	u8 reserved[3];
	u8 wae;
	u8 rspid;
	__le64 rsp_addr;
};

struct qbman_eq_desc_with_fd {
	struct qbman_eq_desc desc;
	u8 fd[32];
};

 
struct qbman_release_desc {
	u8 verb;
	u8 reserved;
	__le16 bpid;
	__le32 reserved2;
	__le64 buf[7];
};

 
#define QBMAN_MC_RSLT_OK      0xf0

#define CODE_CDAN_WE_EN    0x1
#define CODE_CDAN_WE_CTX   0x4

 
struct qbman_swp {
	const struct qbman_swp_desc *desc;
	void *addr_cena;
	void __iomem *addr_cinh;

	 
	struct {
		u32 valid_bit;  
	} mc;

	 
	struct {
		u32 valid_bit;  
	} mr;

	 
	u32 sdq;

	 
	struct {
		atomic_t available;  
		u32 valid_bit;  
		struct dpaa2_dq *storage;  
	} vdq;

	 
	struct {
		u32 next_idx;
		u32 valid_bit;
		u8 dqrr_size;
		int reset_bug;  
	} dqrr;

	struct {
		u32 pi;
		u32 pi_vb;
		u32 pi_ring_size;
		u32 pi_ci_mask;
		u32 ci;
		int available;
		u32 pend;
		u32 no_pfdr;
	} eqcr;

	spinlock_t access_spinlock;

	 
	u32 irq_threshold;
	u32 irq_holdoff;
	int use_adaptive_rx_coalesce;
};

 
extern
int (*qbman_swp_enqueue_ptr)(struct qbman_swp *s,
			     const struct qbman_eq_desc *d,
			     const struct dpaa2_fd *fd);
extern
int (*qbman_swp_enqueue_multiple_ptr)(struct qbman_swp *s,
				      const struct qbman_eq_desc *d,
				      const struct dpaa2_fd *fd,
				      uint32_t *flags,
				      int num_frames);
extern
int (*qbman_swp_enqueue_multiple_desc_ptr)(struct qbman_swp *s,
					   const struct qbman_eq_desc *d,
					   const struct dpaa2_fd *fd,
					   int num_frames);
extern
int (*qbman_swp_pull_ptr)(struct qbman_swp *s, struct qbman_pull_desc *d);
extern
const struct dpaa2_dq *(*qbman_swp_dqrr_next_ptr)(struct qbman_swp *s);
extern
int (*qbman_swp_release_ptr)(struct qbman_swp *s,
			     const struct qbman_release_desc *d,
			     const u64 *buffers,
			     unsigned int num_buffers);

 
struct qbman_swp *qbman_swp_init(const struct qbman_swp_desc *d);
void qbman_swp_finish(struct qbman_swp *p);
u32 qbman_swp_interrupt_read_status(struct qbman_swp *p);
void qbman_swp_interrupt_clear_status(struct qbman_swp *p, u32 mask);
u32 qbman_swp_interrupt_get_trigger(struct qbman_swp *p);
void qbman_swp_interrupt_set_trigger(struct qbman_swp *p, u32 mask);
int qbman_swp_interrupt_get_inhibit(struct qbman_swp *p);
void qbman_swp_interrupt_set_inhibit(struct qbman_swp *p, int inhibit);

void qbman_swp_push_get(struct qbman_swp *p, u8 channel_idx, int *enabled);
void qbman_swp_push_set(struct qbman_swp *p, u8 channel_idx, int enable);

void qbman_pull_desc_clear(struct qbman_pull_desc *d);
void qbman_pull_desc_set_storage(struct qbman_pull_desc *d,
				 struct dpaa2_dq *storage,
				 dma_addr_t storage_phys,
				 int stash);
void qbman_pull_desc_set_numframes(struct qbman_pull_desc *d, u8 numframes);
void qbman_pull_desc_set_fq(struct qbman_pull_desc *d, u32 fqid);
void qbman_pull_desc_set_wq(struct qbman_pull_desc *d, u32 wqid,
			    enum qbman_pull_type_e dct);
void qbman_pull_desc_set_channel(struct qbman_pull_desc *d, u32 chid,
				 enum qbman_pull_type_e dct);

void qbman_swp_dqrr_consume(struct qbman_swp *s, const struct dpaa2_dq *dq);

int qbman_result_has_new_result(struct qbman_swp *p, const struct dpaa2_dq *dq);

void qbman_eq_desc_clear(struct qbman_eq_desc *d);
void qbman_eq_desc_set_no_orp(struct qbman_eq_desc *d, int respond_success);
void qbman_eq_desc_set_token(struct qbman_eq_desc *d, u8 token);
void qbman_eq_desc_set_fq(struct qbman_eq_desc *d, u32 fqid);
void qbman_eq_desc_set_qd(struct qbman_eq_desc *d, u32 qdid,
			  u32 qd_bin, u32 qd_prio);


void qbman_release_desc_clear(struct qbman_release_desc *d);
void qbman_release_desc_set_bpid(struct qbman_release_desc *d, u16 bpid);
void qbman_release_desc_set_rcdi(struct qbman_release_desc *d, int enable);

int qbman_swp_acquire(struct qbman_swp *s, u16 bpid, u64 *buffers,
		      unsigned int num_buffers);
int qbman_swp_alt_fq_state(struct qbman_swp *s, u32 fqid,
			   u8 alt_fq_verb);
int qbman_swp_CDAN_set(struct qbman_swp *s, u16 channelid,
		       u8 we_mask, u8 cdan_en,
		       u64 ctx);

void *qbman_swp_mc_start(struct qbman_swp *p);
void qbman_swp_mc_submit(struct qbman_swp *p, void *cmd, u8 cmd_verb);
void *qbman_swp_mc_result(struct qbman_swp *p);

 
static inline int
qbman_swp_enqueue(struct qbman_swp *s, const struct qbman_eq_desc *d,
		  const struct dpaa2_fd *fd)
{
	return qbman_swp_enqueue_ptr(s, d, fd);
}

 
static inline int
qbman_swp_enqueue_multiple(struct qbman_swp *s,
			   const struct qbman_eq_desc *d,
			   const struct dpaa2_fd *fd,
			   uint32_t *flags,
			   int num_frames)
{
	return qbman_swp_enqueue_multiple_ptr(s, d, fd, flags, num_frames);
}

 
static inline int
qbman_swp_enqueue_multiple_desc(struct qbman_swp *s,
				const struct qbman_eq_desc *d,
				const struct dpaa2_fd *fd,
				int num_frames)
{
	return qbman_swp_enqueue_multiple_desc_ptr(s, d, fd, num_frames);
}

 
static inline int qbman_result_is_DQ(const struct dpaa2_dq *dq)
{
	return ((dq->dq.verb & QBMAN_RESULT_MASK) == QBMAN_RESULT_DQ);
}

 
static inline int qbman_result_is_SCN(const struct dpaa2_dq *dq)
{
	return !qbman_result_is_DQ(dq);
}

 
static inline int qbman_result_is_FQDAN(const struct dpaa2_dq *dq)
{
	return ((dq->dq.verb & QBMAN_RESULT_MASK) == QBMAN_RESULT_FQDAN);
}

 
static inline int qbman_result_is_CDAN(const struct dpaa2_dq *dq)
{
	return ((dq->dq.verb & QBMAN_RESULT_MASK) == QBMAN_RESULT_CDAN);
}

 
static inline int qbman_result_is_CSCN(const struct dpaa2_dq *dq)
{
	return ((dq->dq.verb & QBMAN_RESULT_MASK) == QBMAN_RESULT_CSCN_WQ);
}

 
static inline int qbman_result_is_BPSCN(const struct dpaa2_dq *dq)
{
	return ((dq->dq.verb & QBMAN_RESULT_MASK) == QBMAN_RESULT_BPSCN);
}

 
static inline int qbman_result_is_CGCU(const struct dpaa2_dq *dq)
{
	return ((dq->dq.verb & QBMAN_RESULT_MASK) == QBMAN_RESULT_CGCU);
}

 
static inline int qbman_result_is_FQRN(const struct dpaa2_dq *dq)
{
	return ((dq->dq.verb & QBMAN_RESULT_MASK) == QBMAN_RESULT_FQRN);
}

 
static inline int qbman_result_is_FQRNI(const struct dpaa2_dq *dq)
{
	return ((dq->dq.verb & QBMAN_RESULT_MASK) == QBMAN_RESULT_FQRNI);
}

  
static inline int qbman_result_is_FQPN(const struct dpaa2_dq *dq)
{
	return ((dq->dq.verb & QBMAN_RESULT_MASK) == QBMAN_RESULT_FQPN);
}

 
static inline u8 qbman_result_SCN_state(const struct dpaa2_dq *scn)
{
	return scn->scn.state;
}

#define SCN_RID_MASK 0x00FFFFFF

 
static inline u32 qbman_result_SCN_rid(const struct dpaa2_dq *scn)
{
	return le32_to_cpu(scn->scn.rid_tok) & SCN_RID_MASK;
}

 
static inline u64 qbman_result_SCN_ctx(const struct dpaa2_dq *scn)
{
	return le64_to_cpu(scn->scn.ctx);
}

 
static inline int qbman_swp_fq_schedule(struct qbman_swp *s, u32 fqid)
{
	return qbman_swp_alt_fq_state(s, fqid, QBMAN_FQ_SCHEDULE);
}

 
static inline int qbman_swp_fq_force(struct qbman_swp *s, u32 fqid)
{
	return qbman_swp_alt_fq_state(s, fqid, QBMAN_FQ_FORCE);
}

 
static inline int qbman_swp_fq_xon(struct qbman_swp *s, u32 fqid)
{
	return qbman_swp_alt_fq_state(s, fqid, QBMAN_FQ_XON);
}

 
static inline int qbman_swp_fq_xoff(struct qbman_swp *s, u32 fqid)
{
	return qbman_swp_alt_fq_state(s, fqid, QBMAN_FQ_XOFF);
}

 

 
static inline int qbman_swp_CDAN_set_context(struct qbman_swp *s, u16 channelid,
					     u64 ctx)
{
	return qbman_swp_CDAN_set(s, channelid,
				  CODE_CDAN_WE_CTX,
				  0, ctx);
}

 
static inline int qbman_swp_CDAN_enable(struct qbman_swp *s, u16 channelid)
{
	return qbman_swp_CDAN_set(s, channelid,
				  CODE_CDAN_WE_EN,
				  1, 0);
}

 
static inline int qbman_swp_CDAN_disable(struct qbman_swp *s, u16 channelid)
{
	return qbman_swp_CDAN_set(s, channelid,
				  CODE_CDAN_WE_EN,
				  0, 0);
}

 
static inline int qbman_swp_CDAN_set_context_enable(struct qbman_swp *s,
						    u16 channelid,
						    u64 ctx)
{
	return qbman_swp_CDAN_set(s, channelid,
				  CODE_CDAN_WE_EN | CODE_CDAN_WE_CTX,
				  1, ctx);
}

 
static inline void *qbman_swp_mc_complete(struct qbman_swp *swp, void *cmd,
					  u8 cmd_verb)
{
	int loopvar = 2000;

	qbman_swp_mc_submit(swp, cmd, cmd_verb);

	do {
		cmd = qbman_swp_mc_result(swp);
	} while (!cmd && loopvar--);

	WARN_ON(!loopvar);

	return cmd;
}

 
struct qbman_fq_query_np_rslt {
	u8 verb;
	u8 rslt;
	u8 st1;
	u8 st2;
	u8 reserved[2];
	__le16 od1_sfdr;
	__le16 od2_sfdr;
	__le16 od3_sfdr;
	__le16 ra1_sfdr;
	__le16 ra2_sfdr;
	__le32 pfdr_hptr;
	__le32 pfdr_tptr;
	__le32 frm_cnt;
	__le32 byte_cnt;
	__le16 ics_surp;
	u8 is;
	u8 reserved2[29];
};

int qbman_fq_query_state(struct qbman_swp *s, u32 fqid,
			 struct qbman_fq_query_np_rslt *r);
u32 qbman_fq_state_frame_count(const struct qbman_fq_query_np_rslt *r);
u32 qbman_fq_state_byte_count(const struct qbman_fq_query_np_rslt *r);

struct qbman_bp_query_rslt {
	u8 verb;
	u8 rslt;
	u8 reserved[4];
	u8 bdi;
	u8 state;
	__le32 fill;
	__le32 hdotr;
	__le16 swdet;
	__le16 swdxt;
	__le16 hwdet;
	__le16 hwdxt;
	__le16 swset;
	__le16 swsxt;
	__le16 vbpid;
	__le16 icid;
	__le64 bpscn_addr;
	__le64 bpscn_ctx;
	__le16 hw_targ;
	u8 dbe;
	u8 reserved2;
	u8 sdcnt;
	u8 hdcnt;
	u8 sscnt;
	u8 reserved3[9];
};

int qbman_bp_query(struct qbman_swp *s, u16 bpid,
		   struct qbman_bp_query_rslt *r);

u32 qbman_bp_info_num_free_bufs(struct qbman_bp_query_rslt *a);

 
static inline int qbman_swp_release(struct qbman_swp *s,
				    const struct qbman_release_desc *d,
				    const u64 *buffers,
				    unsigned int num_buffers)
{
	return qbman_swp_release_ptr(s, d, buffers, num_buffers);
}

 
static inline int qbman_swp_pull(struct qbman_swp *s,
				 struct qbman_pull_desc *d)
{
	return qbman_swp_pull_ptr(s, d);
}

 
static inline const struct dpaa2_dq *qbman_swp_dqrr_next(struct qbman_swp *s)
{
	return qbman_swp_dqrr_next_ptr(s);
}

int qbman_swp_set_irq_coalescing(struct qbman_swp *p, u32 irq_threshold,
				 u32 irq_holdoff);

void qbman_swp_get_irq_coalescing(struct qbman_swp *p, u32 *irq_threshold,
				  u32 *irq_holdoff);

#endif  
