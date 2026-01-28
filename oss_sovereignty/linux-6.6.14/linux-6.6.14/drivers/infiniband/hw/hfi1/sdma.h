#ifndef _HFI1_SDMA_H
#define _HFI1_SDMA_H
#include <linux/types.h>
#include <linux/list.h>
#include <asm/byteorder.h>
#include <linux/workqueue.h>
#include <linux/rculist.h>
#include "hfi.h"
#include "verbs.h"
#include "sdma_txreq.h"
#define MAX_DESC 64
#define MAX_SDMA_PKT_SIZE ((16 * 1024) - 1)
#define SDMA_MAP_NONE          0
#define SDMA_MAP_SINGLE        1
#define SDMA_MAP_PAGE          2
#define SDMA_AHG_VALUE_MASK          0xffff
#define SDMA_AHG_VALUE_SHIFT         0
#define SDMA_AHG_INDEX_MASK          0xf
#define SDMA_AHG_INDEX_SHIFT         16
#define SDMA_AHG_FIELD_LEN_MASK      0xf
#define SDMA_AHG_FIELD_LEN_SHIFT     20
#define SDMA_AHG_FIELD_START_MASK    0x1f
#define SDMA_AHG_FIELD_START_SHIFT   24
#define SDMA_AHG_UPDATE_ENABLE_MASK  0x1
#define SDMA_AHG_UPDATE_ENABLE_SHIFT 31
#define SDMA_AHG_NO_AHG              0
#define SDMA_AHG_COPY                1
#define SDMA_AHG_APPLY_UPDATE1       2
#define SDMA_AHG_APPLY_UPDATE2       3
#define SDMA_AHG_APPLY_UPDATE3       4
#define SDMA_DESC0_FIRST_DESC_FLAG      BIT_ULL(63)
#define SDMA_DESC0_LAST_DESC_FLAG       BIT_ULL(62)
#define SDMA_DESC0_BYTE_COUNT_SHIFT     48
#define SDMA_DESC0_BYTE_COUNT_WIDTH     14
#define SDMA_DESC0_BYTE_COUNT_MASK \
	((1ULL << SDMA_DESC0_BYTE_COUNT_WIDTH) - 1)
#define SDMA_DESC0_BYTE_COUNT_SMASK \
	(SDMA_DESC0_BYTE_COUNT_MASK << SDMA_DESC0_BYTE_COUNT_SHIFT)
#define SDMA_DESC0_PHY_ADDR_SHIFT       0
#define SDMA_DESC0_PHY_ADDR_WIDTH       48
#define SDMA_DESC0_PHY_ADDR_MASK \
	((1ULL << SDMA_DESC0_PHY_ADDR_WIDTH) - 1)
#define SDMA_DESC0_PHY_ADDR_SMASK \
	(SDMA_DESC0_PHY_ADDR_MASK << SDMA_DESC0_PHY_ADDR_SHIFT)
#define SDMA_DESC1_HEADER_UPDATE1_SHIFT 32
#define SDMA_DESC1_HEADER_UPDATE1_WIDTH 32
#define SDMA_DESC1_HEADER_UPDATE1_MASK \
	((1ULL << SDMA_DESC1_HEADER_UPDATE1_WIDTH) - 1)
#define SDMA_DESC1_HEADER_UPDATE1_SMASK \
	(SDMA_DESC1_HEADER_UPDATE1_MASK << SDMA_DESC1_HEADER_UPDATE1_SHIFT)
#define SDMA_DESC1_HEADER_MODE_SHIFT    13
#define SDMA_DESC1_HEADER_MODE_WIDTH    3
#define SDMA_DESC1_HEADER_MODE_MASK \
	((1ULL << SDMA_DESC1_HEADER_MODE_WIDTH) - 1)
#define SDMA_DESC1_HEADER_MODE_SMASK \
	(SDMA_DESC1_HEADER_MODE_MASK << SDMA_DESC1_HEADER_MODE_SHIFT)
#define SDMA_DESC1_HEADER_INDEX_SHIFT   8
#define SDMA_DESC1_HEADER_INDEX_WIDTH   5
#define SDMA_DESC1_HEADER_INDEX_MASK \
	((1ULL << SDMA_DESC1_HEADER_INDEX_WIDTH) - 1)
#define SDMA_DESC1_HEADER_INDEX_SMASK \
	(SDMA_DESC1_HEADER_INDEX_MASK << SDMA_DESC1_HEADER_INDEX_SHIFT)
#define SDMA_DESC1_HEADER_DWS_SHIFT     4
#define SDMA_DESC1_HEADER_DWS_WIDTH     4
#define SDMA_DESC1_HEADER_DWS_MASK \
	((1ULL << SDMA_DESC1_HEADER_DWS_WIDTH) - 1)
#define SDMA_DESC1_HEADER_DWS_SMASK \
	(SDMA_DESC1_HEADER_DWS_MASK << SDMA_DESC1_HEADER_DWS_SHIFT)
#define SDMA_DESC1_GENERATION_SHIFT     2
#define SDMA_DESC1_GENERATION_WIDTH     2
#define SDMA_DESC1_GENERATION_MASK \
	((1ULL << SDMA_DESC1_GENERATION_WIDTH) - 1)
#define SDMA_DESC1_GENERATION_SMASK \
	(SDMA_DESC1_GENERATION_MASK << SDMA_DESC1_GENERATION_SHIFT)
#define SDMA_DESC1_INT_REQ_FLAG         BIT_ULL(1)
#define SDMA_DESC1_HEAD_TO_HOST_FLAG    BIT_ULL(0)
enum sdma_states {
	sdma_state_s00_hw_down,
	sdma_state_s10_hw_start_up_halt_wait,
	sdma_state_s15_hw_start_up_clean_wait,
	sdma_state_s20_idle,
	sdma_state_s30_sw_clean_up_wait,
	sdma_state_s40_hw_clean_up_wait,
	sdma_state_s50_hw_halt_wait,
	sdma_state_s60_idle_halt_wait,
	sdma_state_s80_hw_freeze,
	sdma_state_s82_freeze_sw_clean,
	sdma_state_s99_running,
};
enum sdma_events {
	sdma_event_e00_go_hw_down,
	sdma_event_e10_go_hw_start,
	sdma_event_e15_hw_halt_done,
	sdma_event_e25_hw_clean_up_done,
	sdma_event_e30_go_running,
	sdma_event_e40_sw_cleaned,
	sdma_event_e50_hw_cleaned,
	sdma_event_e60_hw_halted,
	sdma_event_e70_go_idle,
	sdma_event_e80_hw_freeze,
	sdma_event_e81_hw_frozen,
	sdma_event_e82_hw_unfreeze,
	sdma_event_e85_link_down,
	sdma_event_e90_sw_halted,
};
struct sdma_set_state_action {
	unsigned op_enable:1;
	unsigned op_intenable:1;
	unsigned op_halt:1;
	unsigned op_cleanup:1;
	unsigned go_s99_running_tofalse:1;
	unsigned go_s99_running_totrue:1;
};
struct sdma_state {
	struct kref          kref;
	struct completion    comp;
	enum sdma_states current_state;
	unsigned             current_op;
	unsigned             go_s99_running;
	enum sdma_states previous_state;
	unsigned             previous_op;
	enum sdma_events last_event;
};
struct hw_sdma_desc {
	__le64 qw[2];
};
struct sdma_engine {
	struct hfi1_devdata *dd;
	struct hfi1_pportdata *ppd;
	void __iomem *tail_csr;
	u64 imask;			 
	u64 idle_mask;
	u64 progress_mask;
	u64 int_mask;
	volatile __le64      *head_dma;  
	dma_addr_t            head_phys;
	struct hw_sdma_desc *descq;
	unsigned descq_full_count;
	struct sdma_txreq **tx_ring;
	dma_addr_t            descq_phys;
	u32 sdma_mask;
	struct sdma_state state;
	int cpu;
	u8 sdma_shift;
	u8 this_idx;  
	spinlock_t senddmactrl_lock;
	u64 p_senddmactrl;		 
	spinlock_t            tail_lock ____cacheline_aligned_in_smp;
#ifdef CONFIG_HFI1_DEBUG_SDMA_ORDER
	u64                   tail_sn;
#endif
	u32                   descq_tail;
	unsigned long         ahg_bits;
	u16                   desc_avail;
	u16                   tx_tail;
	u16 descq_cnt;
	seqlock_t            head_lock ____cacheline_aligned_in_smp;
#ifdef CONFIG_HFI1_DEBUG_SDMA_ORDER
	u64                   head_sn;
#endif
	u32                   descq_head;
	u16                   tx_head;
	u64                   last_status;
	u64                     err_cnt;
	u64                     sdma_int_cnt;
	u64                     idle_int_cnt;
	u64                     progress_int_cnt;
	seqlock_t            waitlock;
	struct list_head      dmawait;
	struct tasklet_struct sdma_hw_clean_up_task
		____cacheline_aligned_in_smp;
	struct tasklet_struct sdma_sw_clean_up_task
		____cacheline_aligned_in_smp;
	struct work_struct err_halt_worker;
	struct timer_list     err_progress_check_timer;
	u32                   progress_check_head;
	struct work_struct flush_worker;
	spinlock_t flushlist_lock;
	struct list_head flushlist;
	struct cpumask cpu_mask;
	struct kobject kobj;
	u32 msix_intr;
};
int sdma_init(struct hfi1_devdata *dd, u8 port);
void sdma_start(struct hfi1_devdata *dd);
void sdma_exit(struct hfi1_devdata *dd);
void sdma_clean(struct hfi1_devdata *dd, size_t num_engines);
void sdma_all_running(struct hfi1_devdata *dd);
void sdma_all_idle(struct hfi1_devdata *dd);
void sdma_freeze_notify(struct hfi1_devdata *dd, int go_idle);
void sdma_freeze(struct hfi1_devdata *dd);
void sdma_unfreeze(struct hfi1_devdata *dd);
void sdma_wait(struct hfi1_devdata *dd);
static inline int sdma_empty(struct sdma_engine *sde)
{
	return sde->descq_tail == sde->descq_head;
}
static inline u16 sdma_descq_freecnt(struct sdma_engine *sde)
{
	return sde->descq_cnt -
		(sde->descq_tail -
		 READ_ONCE(sde->descq_head)) - 1;
}
static inline u16 sdma_descq_inprocess(struct sdma_engine *sde)
{
	return sde->descq_cnt - sdma_descq_freecnt(sde);
}
static inline int __sdma_running(struct sdma_engine *engine)
{
	return engine->state.current_state == sdma_state_s99_running;
}
static inline int sdma_running(struct sdma_engine *engine)
{
	unsigned long flags;
	int ret;
	spin_lock_irqsave(&engine->tail_lock, flags);
	ret = __sdma_running(engine);
	spin_unlock_irqrestore(&engine->tail_lock, flags);
	return ret;
}
void _sdma_txreq_ahgadd(
	struct sdma_txreq *tx,
	u8 num_ahg,
	u8 ahg_entry,
	u32 *ahg,
	u8 ahg_hlen);
static inline int sdma_txinit_ahg(
	struct sdma_txreq *tx,
	u16 flags,
	u16 tlen,
	u8 ahg_entry,
	u8 num_ahg,
	u32 *ahg,
	u8 ahg_hlen,
	void (*cb)(struct sdma_txreq *, int))
{
	if (tlen == 0)
		return -ENODATA;
	if (tlen > MAX_SDMA_PKT_SIZE)
		return -EMSGSIZE;
	tx->desc_limit = ARRAY_SIZE(tx->descs);
	tx->descp = &tx->descs[0];
	INIT_LIST_HEAD(&tx->list);
	tx->num_desc = 0;
	tx->flags = flags;
	tx->complete = cb;
	tx->coalesce_buf = NULL;
	tx->wait = NULL;
	tx->packet_len = tlen;
	tx->tlen = tx->packet_len;
	tx->descs[0].qw[0] = SDMA_DESC0_FIRST_DESC_FLAG;
	tx->descs[0].qw[1] = 0;
	if (flags & SDMA_TXREQ_F_AHG_COPY)
		tx->descs[0].qw[1] |=
			(((u64)ahg_entry & SDMA_DESC1_HEADER_INDEX_MASK)
				<< SDMA_DESC1_HEADER_INDEX_SHIFT) |
			(((u64)SDMA_AHG_COPY & SDMA_DESC1_HEADER_MODE_MASK)
				<< SDMA_DESC1_HEADER_MODE_SHIFT);
	else if (flags & SDMA_TXREQ_F_USE_AHG && num_ahg)
		_sdma_txreq_ahgadd(tx, num_ahg, ahg_entry, ahg, ahg_hlen);
	return 0;
}
static inline int sdma_txinit(
	struct sdma_txreq *tx,
	u16 flags,
	u16 tlen,
	void (*cb)(struct sdma_txreq *, int))
{
	return sdma_txinit_ahg(tx, flags, tlen, 0, 0, NULL, 0, cb);
}
static inline int sdma_mapping_type(struct sdma_desc *d)
{
	return (d->qw[1] & SDMA_DESC1_GENERATION_SMASK)
		>> SDMA_DESC1_GENERATION_SHIFT;
}
static inline size_t sdma_mapping_len(struct sdma_desc *d)
{
	return (d->qw[0] & SDMA_DESC0_BYTE_COUNT_SMASK)
		>> SDMA_DESC0_BYTE_COUNT_SHIFT;
}
static inline dma_addr_t sdma_mapping_addr(struct sdma_desc *d)
{
	return (d->qw[0] & SDMA_DESC0_PHY_ADDR_SMASK)
		>> SDMA_DESC0_PHY_ADDR_SHIFT;
}
static inline void make_tx_sdma_desc(
	struct sdma_txreq *tx,
	int type,
	dma_addr_t addr,
	size_t len,
	void *pinning_ctx,
	void (*ctx_get)(void *),
	void (*ctx_put)(void *))
{
	struct sdma_desc *desc = &tx->descp[tx->num_desc];
	if (!tx->num_desc) {
		desc->qw[1] |= ((u64)type & SDMA_DESC1_GENERATION_MASK)
				<< SDMA_DESC1_GENERATION_SHIFT;
	} else {
		desc->qw[0] = 0;
		desc->qw[1] = ((u64)type & SDMA_DESC1_GENERATION_MASK)
				<< SDMA_DESC1_GENERATION_SHIFT;
	}
	desc->qw[0] |= (((u64)addr & SDMA_DESC0_PHY_ADDR_MASK)
				<< SDMA_DESC0_PHY_ADDR_SHIFT) |
			(((u64)len & SDMA_DESC0_BYTE_COUNT_MASK)
				<< SDMA_DESC0_BYTE_COUNT_SHIFT);
	desc->pinning_ctx = pinning_ctx;
	desc->ctx_put = ctx_put;
	if (pinning_ctx && ctx_get)
		ctx_get(pinning_ctx);
}
int ext_coal_sdma_tx_descs(struct hfi1_devdata *dd, struct sdma_txreq *tx,
			   int type, void *kvaddr, struct page *page,
			   unsigned long offset, u16 len);
int _pad_sdma_tx_descs(struct hfi1_devdata *, struct sdma_txreq *);
void __sdma_txclean(struct hfi1_devdata *, struct sdma_txreq *);
static inline void sdma_txclean(struct hfi1_devdata *dd, struct sdma_txreq *tx)
{
	if (tx->num_desc)
		__sdma_txclean(dd, tx);
}
static inline void _sdma_close_tx(struct hfi1_devdata *dd,
				  struct sdma_txreq *tx)
{
	u16 last_desc = tx->num_desc - 1;
	tx->descp[last_desc].qw[0] |= SDMA_DESC0_LAST_DESC_FLAG;
	tx->descp[last_desc].qw[1] |= dd->default_desc1;
	if (tx->flags & SDMA_TXREQ_F_URGENT)
		tx->descp[last_desc].qw[1] |= (SDMA_DESC1_HEAD_TO_HOST_FLAG |
					       SDMA_DESC1_INT_REQ_FLAG);
}
static inline int _sdma_txadd_daddr(
	struct hfi1_devdata *dd,
	int type,
	struct sdma_txreq *tx,
	dma_addr_t addr,
	u16 len,
	void *pinning_ctx,
	void (*ctx_get)(void *),
	void (*ctx_put)(void *))
{
	int rval = 0;
	make_tx_sdma_desc(
		tx,
		type,
		addr, len,
		pinning_ctx, ctx_get, ctx_put);
	WARN_ON(len > tx->tlen);
	tx->num_desc++;
	tx->tlen -= len;
	if (!tx->tlen) {
		if (tx->packet_len & (sizeof(u32) - 1)) {
			rval = _pad_sdma_tx_descs(dd, tx);
			if (rval)
				return rval;
		} else {
			_sdma_close_tx(dd, tx);
		}
	}
	return rval;
}
static inline int sdma_txadd_page(
	struct hfi1_devdata *dd,
	struct sdma_txreq *tx,
	struct page *page,
	unsigned long offset,
	u16 len,
	void *pinning_ctx,
	void (*ctx_get)(void *),
	void (*ctx_put)(void *))
{
	dma_addr_t addr;
	int rval;
	if ((unlikely(tx->num_desc == tx->desc_limit))) {
		rval = ext_coal_sdma_tx_descs(dd, tx, SDMA_MAP_PAGE,
					      NULL, page, offset, len);
		if (rval <= 0)
			return rval;
	}
	addr = dma_map_page(
		       &dd->pcidev->dev,
		       page,
		       offset,
		       len,
		       DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(&dd->pcidev->dev, addr))) {
		__sdma_txclean(dd, tx);
		return -ENOSPC;
	}
	return _sdma_txadd_daddr(dd, SDMA_MAP_PAGE, tx, addr, len,
				 pinning_ctx, ctx_get, ctx_put);
}
static inline int sdma_txadd_daddr(
	struct hfi1_devdata *dd,
	struct sdma_txreq *tx,
	dma_addr_t addr,
	u16 len)
{
	int rval;
	if ((unlikely(tx->num_desc == tx->desc_limit))) {
		rval = ext_coal_sdma_tx_descs(dd, tx, SDMA_MAP_NONE,
					      NULL, NULL, 0, 0);
		if (rval <= 0)
			return rval;
	}
	return _sdma_txadd_daddr(dd, SDMA_MAP_NONE, tx, addr, len,
				 NULL, NULL, NULL);
}
static inline int sdma_txadd_kvaddr(
	struct hfi1_devdata *dd,
	struct sdma_txreq *tx,
	void *kvaddr,
	u16 len)
{
	dma_addr_t addr;
	int rval;
	if ((unlikely(tx->num_desc == tx->desc_limit))) {
		rval = ext_coal_sdma_tx_descs(dd, tx, SDMA_MAP_SINGLE,
					      kvaddr, NULL, 0, len);
		if (rval <= 0)
			return rval;
	}
	addr = dma_map_single(
		       &dd->pcidev->dev,
		       kvaddr,
		       len,
		       DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(&dd->pcidev->dev, addr))) {
		__sdma_txclean(dd, tx);
		return -ENOSPC;
	}
	return _sdma_txadd_daddr(dd, SDMA_MAP_SINGLE, tx, addr, len,
				 NULL, NULL, NULL);
}
struct iowait_work;
int sdma_send_txreq(struct sdma_engine *sde,
		    struct iowait_work *wait,
		    struct sdma_txreq *tx,
		    bool pkts_sent);
int sdma_send_txlist(struct sdma_engine *sde,
		     struct iowait_work *wait,
		     struct list_head *tx_list,
		     u16 *count_out);
int sdma_ahg_alloc(struct sdma_engine *sde);
void sdma_ahg_free(struct sdma_engine *sde, int ahg_index);
static inline u32 sdma_build_ahg_descriptor(
	u16 data,
	u8 dwindex,
	u8 startbit,
	u8 bits)
{
	return (u32)(1UL << SDMA_AHG_UPDATE_ENABLE_SHIFT |
		((startbit & SDMA_AHG_FIELD_START_MASK) <<
		SDMA_AHG_FIELD_START_SHIFT) |
		((bits & SDMA_AHG_FIELD_LEN_MASK) <<
		SDMA_AHG_FIELD_LEN_SHIFT) |
		((dwindex & SDMA_AHG_INDEX_MASK) <<
		SDMA_AHG_INDEX_SHIFT) |
		((data & SDMA_AHG_VALUE_MASK) <<
		SDMA_AHG_VALUE_SHIFT));
}
static inline unsigned sdma_progress(struct sdma_engine *sde, unsigned seq,
				     struct sdma_txreq *tx)
{
	if (read_seqretry(&sde->head_lock, seq)) {
		sde->desc_avail = sdma_descq_freecnt(sde);
		if (tx->num_desc > sde->desc_avail)
			return 0;
		return 1;
	}
	return 0;
}
void sdma_engine_error(struct sdma_engine *sde, u64 status);
void sdma_engine_interrupt(struct sdma_engine *sde, u64 status);
struct sdma_map_elem {
	u32 mask;
	struct sdma_engine *sde[];
};
struct sdma_vl_map {
	s8 engine_to_vl[TXE_NUM_SDMA_ENGINES];
	struct rcu_head list;
	u32 mask;
	u8 actual_vls;
	u8 vls;
	struct sdma_map_elem *map[];
};
int sdma_map_init(
	struct hfi1_devdata *dd,
	u8 port,
	u8 num_vls,
	u8 *vl_engines);
void _sdma_engine_progress_schedule(struct sdma_engine *sde);
static inline void sdma_engine_progress_schedule(
	struct sdma_engine *sde)
{
	if (!sde || sdma_descq_inprocess(sde) < (sde->descq_cnt / 8))
		return;
	_sdma_engine_progress_schedule(sde);
}
struct sdma_engine *sdma_select_engine_sc(
	struct hfi1_devdata *dd,
	u32 selector,
	u8 sc5);
struct sdma_engine *sdma_select_engine_vl(
	struct hfi1_devdata *dd,
	u32 selector,
	u8 vl);
struct sdma_engine *sdma_select_user_engine(struct hfi1_devdata *dd,
					    u32 selector, u8 vl);
ssize_t sdma_get_cpu_to_sde_map(struct sdma_engine *sde, char *buf);
ssize_t sdma_set_cpu_to_sde_map(struct sdma_engine *sde, const char *buf,
				size_t count);
int sdma_engine_get_vl(struct sdma_engine *sde);
void sdma_seqfile_dump_sde(struct seq_file *s, struct sdma_engine *);
void sdma_seqfile_dump_cpu_list(struct seq_file *s, struct hfi1_devdata *dd,
				unsigned long cpuid);
#ifdef CONFIG_SDMA_VERBOSITY
void sdma_dumpstate(struct sdma_engine *);
#endif
static inline char *slashstrip(char *s)
{
	char *r = s;
	while (*s)
		if (*s++ == '/')
			r = s;
	return r;
}
u16 sdma_get_descq_cnt(void);
extern uint mod_num_sdma;
void sdma_update_lmc(struct hfi1_devdata *dd, u64 mask, u32 lid);
#endif
