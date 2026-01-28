#ifndef HFI1_SDMA_TXREQ_H
#define HFI1_SDMA_TXREQ_H
#define NUM_DESC 6
struct sdma_desc {
	u64 qw[2];
	void *pinning_ctx;
	void (*ctx_put)(void *ctx);
};
#define SDMA_TXREQ_S_OK        0
#define SDMA_TXREQ_S_SENDERROR 1
#define SDMA_TXREQ_S_ABORTED   2
#define SDMA_TXREQ_S_SHUTDOWN  3
#define SDMA_TXREQ_F_URGENT       0x0001
#define SDMA_TXREQ_F_AHG_COPY     0x0002
#define SDMA_TXREQ_F_USE_AHG      0x0004
#define SDMA_TXREQ_F_VIP          0x0010
struct sdma_txreq;
typedef void (*callback_t)(struct sdma_txreq *, int);
struct iowait;
struct sdma_txreq {
	struct list_head list;
	struct sdma_desc *descp;
	void *coalesce_buf;
	struct iowait *wait;
	callback_t                  complete;
#ifdef CONFIG_HFI1_DEBUG_SDMA_ORDER
	u64 sn;
#endif
	u16                         packet_len;
	u16                         tlen;
	u16                         num_desc;
	u16                         desc_limit;
	u16                         next_descq_idx;
	u16 coalesce_idx;
	u16                         flags;
	struct sdma_desc descs[NUM_DESC];
};
static inline int sdma_txreq_built(struct sdma_txreq *tx)
{
	return tx->num_desc;
}
#endif                           
