 
 
#ifndef _HFI1_USER_SDMA_H
#define _HFI1_USER_SDMA_H

#include <linux/device.h>
#include <linux/wait.h>

#include "common.h"
#include "iowait.h"
#include "user_exp_rcv.h"
#include "mmu_rb.h"
#include "pinning.h"
#include "sdma.h"

 
#define MAX_VECTORS_PER_REQ 8
 
#define MAX_PKTS_PER_QUEUE 16

#define num_pages(x) (1 + ((((x) - 1) & PAGE_MASK) >> PAGE_SHIFT))

#define req_opcode(x) \
	(((x) >> HFI1_SDMA_REQ_OPCODE_SHIFT) & HFI1_SDMA_REQ_OPCODE_MASK)
#define req_version(x) \
	(((x) >> HFI1_SDMA_REQ_VERSION_SHIFT) & HFI1_SDMA_REQ_OPCODE_MASK)
#define req_iovcnt(x) \
	(((x) >> HFI1_SDMA_REQ_IOVCNT_SHIFT) & HFI1_SDMA_REQ_IOVCNT_MASK)

 
#define BTH_SEQ_MASK 0x7ffull

#define AHG_KDETH_INTR_SHIFT 12
#define AHG_KDETH_SH_SHIFT   13
#define AHG_KDETH_ARRAY_SIZE  9

#define PBC2LRH(x) ((((x) & 0xfff) << 2) - 4)
#define LRH2PBC(x) ((((x) >> 2) + 1) & 0xfff)

 
static inline int ahg_header_set(u32 *arr, int idx, size_t array_size,
				 u8 dw, u8 bit, u8 width, u16 value)
{
	if ((size_t)idx >= array_size)
		return -ERANGE;
	arr[idx++] = sdma_build_ahg_descriptor(value, dw, bit, width);
	return idx;
}

 
#define TXREQ_FLAGS_REQ_ACK   BIT(0)       
#define TXREQ_FLAGS_REQ_DISABLE_SH BIT(1)  

enum pkt_q_sdma_state {
	SDMA_PKT_Q_ACTIVE,
	SDMA_PKT_Q_DEFERRED,
};

#define SDMA_IOWAIT_TIMEOUT 1000  

#define SDMA_DBG(req, fmt, ...)				     \
	hfi1_cdbg(SDMA, "[%u:%u:%u:%u] " fmt, (req)->pq->dd->unit, \
		 (req)->pq->ctxt, (req)->pq->subctxt, (req)->info.comp_idx, \
		 ##__VA_ARGS__)

struct hfi1_user_sdma_pkt_q {
	u16 ctxt;
	u16 subctxt;
	u16 n_max_reqs;
	atomic_t n_reqs;
	u16 reqidx;
	struct hfi1_devdata *dd;
	struct kmem_cache *txreq_cache;
	struct user_sdma_request *reqs;
	unsigned long *req_in_use;
	struct iowait busy;
	enum pkt_q_sdma_state state;
	wait_queue_head_t wait;
	unsigned long unpinned;
	struct mmu_rb_handler *handler;
	atomic_t n_locked;
};

struct hfi1_user_sdma_comp_q {
	u16 nentries;
	struct hfi1_sdma_comp_entry *comps;
};

struct user_sdma_iovec {
	struct list_head list;
	struct iovec iov;
	 
	u64 offset;
};

 
struct evict_data {
	u32 cleared;	 
	u32 target;	 
};

struct user_sdma_request {
	 
	struct hfi1_pkt_header hdr;

	 
	struct hfi1_user_sdma_pkt_q *pq ____cacheline_aligned_in_smp;
	struct hfi1_user_sdma_comp_q *cq;
	 
	struct sdma_engine *sde;
	struct sdma_req_info info;
	 
	u32 *tids;
	 
	u32 data_len;
	 
	u16 n_tids;
	 
	u8 data_iovs;
	s8 ahg_idx;

	 
	u16 seqcomp ____cacheline_aligned_in_smp;
	u16 seqsubmitted;

	 
	struct list_head txps ____cacheline_aligned_in_smp;
	u16 seqnum;
	 
	u32 tidoffset;
	 
	u32 koffset;
	u32 sent;
	 
	u16 tididx;
	 
	u8 iov_idx;
	u8 has_error;

	struct user_sdma_iovec iovs[MAX_VECTORS_PER_REQ];
} ____cacheline_aligned_in_smp;

 
struct user_sdma_txreq {
	 
	struct hfi1_pkt_header hdr;
	struct sdma_txreq txreq;
	struct list_head list;
	struct user_sdma_request *req;
	u16 flags;
	u16 seqnum;
};

int hfi1_user_sdma_alloc_queues(struct hfi1_ctxtdata *uctxt,
				struct hfi1_filedata *fd);
int hfi1_user_sdma_free_queues(struct hfi1_filedata *fd,
			       struct hfi1_ctxtdata *uctxt);
int hfi1_user_sdma_process_request(struct hfi1_filedata *fd,
				   struct iovec *iovec, unsigned long dim,
				   unsigned long *count);
#endif  
