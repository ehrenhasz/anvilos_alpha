
#include <linux/gfp.h>
#include <linux/workqueue.h>
#include <crypto/internal/skcipher.h>

#include "nitrox_common.h"
#include "nitrox_dev.h"
#include "nitrox_req.h"
#include "nitrox_csr.h"

 
#define MIN_UDD_LEN 16
 
#define FDATA_SIZE 32
 
#define SOLICIT_BASE_DPORT 256

#define REQ_NOT_POSTED 1
#define REQ_BACKLOG    2
#define REQ_POSTED     3

 

static inline int incr_index(int index, int count, int max)
{
	if ((index + count) >= max)
		index = index + count - max;
	else
		index += count;

	return index;
}

static void softreq_unmap_sgbufs(struct nitrox_softreq *sr)
{
	struct nitrox_device *ndev = sr->ndev;
	struct device *dev = DEV(ndev);


	dma_unmap_sg(dev, sr->in.sg, sg_nents(sr->in.sg),
		     DMA_BIDIRECTIONAL);
	dma_unmap_single(dev, sr->in.sgcomp_dma, sr->in.sgcomp_len,
			 DMA_TO_DEVICE);
	kfree(sr->in.sgcomp);
	sr->in.sg = NULL;
	sr->in.sgmap_cnt = 0;

	dma_unmap_sg(dev, sr->out.sg, sg_nents(sr->out.sg),
		     DMA_BIDIRECTIONAL);
	dma_unmap_single(dev, sr->out.sgcomp_dma, sr->out.sgcomp_len,
			 DMA_TO_DEVICE);
	kfree(sr->out.sgcomp);
	sr->out.sg = NULL;
	sr->out.sgmap_cnt = 0;
}

static void softreq_destroy(struct nitrox_softreq *sr)
{
	softreq_unmap_sgbufs(sr);
	kfree(sr);
}

 
static int create_sg_component(struct nitrox_softreq *sr,
			       struct nitrox_sgtable *sgtbl, int map_nents)
{
	struct nitrox_device *ndev = sr->ndev;
	struct nitrox_sgcomp *sgcomp;
	struct scatterlist *sg;
	dma_addr_t dma;
	size_t sz_comp;
	int i, j, nr_sgcomp;

	nr_sgcomp = roundup(map_nents, 4) / 4;

	 
	sz_comp = nr_sgcomp * sizeof(*sgcomp);
	sgcomp = kzalloc(sz_comp, sr->gfp);
	if (!sgcomp)
		return -ENOMEM;

	sgtbl->sgcomp = sgcomp;

	sg = sgtbl->sg;
	 
	for (i = 0; i < nr_sgcomp; i++) {
		for (j = 0; j < 4 && sg; j++) {
			sgcomp[i].len[j] = cpu_to_be16(sg_dma_len(sg));
			sgcomp[i].dma[j] = cpu_to_be64(sg_dma_address(sg));
			sg = sg_next(sg);
		}
	}
	 
	dma = dma_map_single(DEV(ndev), sgtbl->sgcomp, sz_comp, DMA_TO_DEVICE);
	if (dma_mapping_error(DEV(ndev), dma)) {
		kfree(sgtbl->sgcomp);
		sgtbl->sgcomp = NULL;
		return -ENOMEM;
	}

	sgtbl->sgcomp_dma = dma;
	sgtbl->sgcomp_len = sz_comp;

	return 0;
}

 
static int dma_map_inbufs(struct nitrox_softreq *sr,
			  struct se_crypto_request *req)
{
	struct device *dev = DEV(sr->ndev);
	struct scatterlist *sg;
	int i, nents, ret = 0;

	nents = dma_map_sg(dev, req->src, sg_nents(req->src),
			   DMA_BIDIRECTIONAL);
	if (!nents)
		return -EINVAL;

	for_each_sg(req->src, sg, nents, i)
		sr->in.total_bytes += sg_dma_len(sg);

	sr->in.sg = req->src;
	sr->in.sgmap_cnt = nents;
	ret = create_sg_component(sr, &sr->in, sr->in.sgmap_cnt);
	if (ret)
		goto incomp_err;

	return 0;

incomp_err:
	dma_unmap_sg(dev, req->src, sg_nents(req->src), DMA_BIDIRECTIONAL);
	sr->in.sgmap_cnt = 0;
	return ret;
}

static int dma_map_outbufs(struct nitrox_softreq *sr,
			   struct se_crypto_request *req)
{
	struct device *dev = DEV(sr->ndev);
	int nents, ret = 0;

	nents = dma_map_sg(dev, req->dst, sg_nents(req->dst),
			   DMA_BIDIRECTIONAL);
	if (!nents)
		return -EINVAL;

	sr->out.sg = req->dst;
	sr->out.sgmap_cnt = nents;
	ret = create_sg_component(sr, &sr->out, sr->out.sgmap_cnt);
	if (ret)
		goto outcomp_map_err;

	return 0;

outcomp_map_err:
	dma_unmap_sg(dev, req->dst, sg_nents(req->dst), DMA_BIDIRECTIONAL);
	sr->out.sgmap_cnt = 0;
	sr->out.sg = NULL;
	return ret;
}

static inline int softreq_map_iobuf(struct nitrox_softreq *sr,
				    struct se_crypto_request *creq)
{
	int ret;

	ret = dma_map_inbufs(sr, creq);
	if (ret)
		return ret;

	ret = dma_map_outbufs(sr, creq);
	if (ret)
		softreq_unmap_sgbufs(sr);

	return ret;
}

static inline void backlog_list_add(struct nitrox_softreq *sr,
				    struct nitrox_cmdq *cmdq)
{
	INIT_LIST_HEAD(&sr->backlog);

	spin_lock_bh(&cmdq->backlog_qlock);
	list_add_tail(&sr->backlog, &cmdq->backlog_head);
	atomic_inc(&cmdq->backlog_count);
	atomic_set(&sr->status, REQ_BACKLOG);
	spin_unlock_bh(&cmdq->backlog_qlock);
}

static inline void response_list_add(struct nitrox_softreq *sr,
				     struct nitrox_cmdq *cmdq)
{
	INIT_LIST_HEAD(&sr->response);

	spin_lock_bh(&cmdq->resp_qlock);
	list_add_tail(&sr->response, &cmdq->response_head);
	spin_unlock_bh(&cmdq->resp_qlock);
}

static inline void response_list_del(struct nitrox_softreq *sr,
				     struct nitrox_cmdq *cmdq)
{
	spin_lock_bh(&cmdq->resp_qlock);
	list_del(&sr->response);
	spin_unlock_bh(&cmdq->resp_qlock);
}

static struct nitrox_softreq *
get_first_response_entry(struct nitrox_cmdq *cmdq)
{
	return list_first_entry_or_null(&cmdq->response_head,
					struct nitrox_softreq, response);
}

static inline bool cmdq_full(struct nitrox_cmdq *cmdq, int qlen)
{
	if (atomic_inc_return(&cmdq->pending_count) > qlen) {
		atomic_dec(&cmdq->pending_count);
		 
		smp_mb__after_atomic();
		return true;
	}
	 
	smp_mb__after_atomic();
	return false;
}

 
static void post_se_instr(struct nitrox_softreq *sr,
			  struct nitrox_cmdq *cmdq)
{
	struct nitrox_device *ndev = sr->ndev;
	int idx;
	u8 *ent;

	spin_lock_bh(&cmdq->cmd_qlock);

	idx = cmdq->write_idx;
	 
	ent = cmdq->base + (idx * cmdq->instr_size);
	memcpy(ent, &sr->instr, cmdq->instr_size);

	atomic_set(&sr->status, REQ_POSTED);
	response_list_add(sr, cmdq);
	sr->tstamp = jiffies;
	 
	dma_wmb();

	 
	writeq(1, cmdq->dbell_csr_addr);

	cmdq->write_idx = incr_index(idx, 1, ndev->qlen);

	spin_unlock_bh(&cmdq->cmd_qlock);

	 
	atomic64_inc(&ndev->stats.posted);
}

static int post_backlog_cmds(struct nitrox_cmdq *cmdq)
{
	struct nitrox_device *ndev = cmdq->ndev;
	struct nitrox_softreq *sr, *tmp;
	int ret = 0;

	if (!atomic_read(&cmdq->backlog_count))
		return 0;

	spin_lock_bh(&cmdq->backlog_qlock);

	list_for_each_entry_safe(sr, tmp, &cmdq->backlog_head, backlog) {
		 
		if (unlikely(cmdq_full(cmdq, ndev->qlen))) {
			ret = -ENOSPC;
			break;
		}
		 
		list_del(&sr->backlog);
		atomic_dec(&cmdq->backlog_count);
		 
		smp_mb__after_atomic();

		 
		post_se_instr(sr, cmdq);
	}
	spin_unlock_bh(&cmdq->backlog_qlock);

	return ret;
}

static int nitrox_enqueue_request(struct nitrox_softreq *sr)
{
	struct nitrox_cmdq *cmdq = sr->cmdq;
	struct nitrox_device *ndev = sr->ndev;

	 
	post_backlog_cmds(cmdq);

	if (unlikely(cmdq_full(cmdq, ndev->qlen))) {
		if (!(sr->flags & CRYPTO_TFM_REQ_MAY_BACKLOG)) {
			 
			atomic64_inc(&ndev->stats.dropped);
			return -ENOSPC;
		}
		 
		backlog_list_add(sr, cmdq);
		return -EINPROGRESS;
	}
	post_se_instr(sr, cmdq);

	return -EINPROGRESS;
}

 
int nitrox_process_se_request(struct nitrox_device *ndev,
			      struct se_crypto_request *req,
			      completion_t callback,
			      void *cb_arg)
{
	struct nitrox_softreq *sr;
	dma_addr_t ctx_handle = 0;
	int qno, ret = 0;

	if (!nitrox_ready(ndev))
		return -ENODEV;

	sr = kzalloc(sizeof(*sr), req->gfp);
	if (!sr)
		return -ENOMEM;

	sr->ndev = ndev;
	sr->flags = req->flags;
	sr->gfp = req->gfp;
	sr->callback = callback;
	sr->cb_arg = cb_arg;

	atomic_set(&sr->status, REQ_NOT_POSTED);

	sr->resp.orh = req->orh;
	sr->resp.completion = req->comp;

	ret = softreq_map_iobuf(sr, req);
	if (ret) {
		kfree(sr);
		return ret;
	}

	 
	if (req->ctx_handle) {
		struct ctx_hdr *hdr;
		u8 *ctx_ptr;

		ctx_ptr = (u8 *)(uintptr_t)req->ctx_handle;
		hdr = (struct ctx_hdr *)(ctx_ptr - sizeof(struct ctx_hdr));
		ctx_handle = hdr->ctx_dma;
	}

	 
	qno = smp_processor_id() % ndev->nr_queues;

	sr->cmdq = &ndev->pkt_inq[qno];

	 

	 
	 
	sr->instr.dptr0 = cpu_to_be64(sr->in.sgcomp_dma);

	 
	sr->instr.ih.value = 0;
	sr->instr.ih.s.g = 1;
	sr->instr.ih.s.gsz = sr->in.sgmap_cnt;
	sr->instr.ih.s.ssz = sr->out.sgmap_cnt;
	sr->instr.ih.s.fsz = FDATA_SIZE + sizeof(struct gphdr);
	sr->instr.ih.s.tlen = sr->instr.ih.s.fsz + sr->in.total_bytes;
	sr->instr.ih.bev = cpu_to_be64(sr->instr.ih.value);

	 
	sr->instr.irh.value[0] = 0;
	sr->instr.irh.s.uddl = MIN_UDD_LEN;
	 
	sr->instr.irh.s.ctxl = (req->ctrl.s.ctxl / 8);
	 
	sr->instr.irh.s.destport = SOLICIT_BASE_DPORT + qno;
	sr->instr.irh.s.ctxc = req->ctrl.s.ctxc;
	sr->instr.irh.s.arg = req->ctrl.s.arg;
	sr->instr.irh.s.opcode = req->opcode;
	sr->instr.irh.bev[0] = cpu_to_be64(sr->instr.irh.value[0]);

	 
	sr->instr.irh.s.ctxp = cpu_to_be64(ctx_handle);

	 
	sr->instr.slc.value[0] = 0;
	sr->instr.slc.s.ssz = sr->out.sgmap_cnt;
	sr->instr.slc.bev[0] = cpu_to_be64(sr->instr.slc.value[0]);

	 
	sr->instr.slc.s.rptr = cpu_to_be64(sr->out.sgcomp_dma);

	 
	sr->instr.fdata[0] = *((u64 *)&req->gph);
	sr->instr.fdata[1] = 0;

	ret = nitrox_enqueue_request(sr);
	if (ret == -ENOSPC)
		goto send_fail;

	return ret;

send_fail:
	softreq_destroy(sr);
	return ret;
}

static inline int cmd_timeout(unsigned long tstamp, unsigned long timeout)
{
	return time_after_eq(jiffies, (tstamp + timeout));
}

void backlog_qflush_work(struct work_struct *work)
{
	struct nitrox_cmdq *cmdq;

	cmdq = container_of(work, struct nitrox_cmdq, backlog_qflush);
	post_backlog_cmds(cmdq);
}

static bool sr_completed(struct nitrox_softreq *sr)
{
	u64 orh = READ_ONCE(*sr->resp.orh);
	unsigned long timeout = jiffies + msecs_to_jiffies(1);

	if ((orh != PENDING_SIG) && (orh & 0xff))
		return true;

	while (READ_ONCE(*sr->resp.completion) == PENDING_SIG) {
		if (time_after(jiffies, timeout)) {
			pr_err("comp not done\n");
			return false;
		}
	}

	return true;
}

 
static void process_response_list(struct nitrox_cmdq *cmdq)
{
	struct nitrox_device *ndev = cmdq->ndev;
	struct nitrox_softreq *sr;
	int req_completed = 0, err = 0, budget;
	completion_t callback;
	void *cb_arg;

	 
	budget = atomic_read(&cmdq->pending_count);

	while (req_completed < budget) {
		sr = get_first_response_entry(cmdq);
		if (!sr)
			break;

		if (atomic_read(&sr->status) != REQ_POSTED)
			break;

		 
		if (!sr_completed(sr)) {
			 
			if (!cmd_timeout(sr->tstamp, ndev->timeout))
				break;
			dev_err_ratelimited(DEV(ndev),
					    "Request timeout, orh 0x%016llx\n",
					    READ_ONCE(*sr->resp.orh));
		}
		atomic_dec(&cmdq->pending_count);
		atomic64_inc(&ndev->stats.completed);
		 
		smp_mb__after_atomic();
		 
		response_list_del(sr, cmdq);
		 
		err = READ_ONCE(*sr->resp.orh) & 0xff;
		callback = sr->callback;
		cb_arg = sr->cb_arg;
		softreq_destroy(sr);
		if (callback)
			callback(cb_arg, err);

		req_completed++;
	}
}

 
void pkt_slc_resp_tasklet(unsigned long data)
{
	struct nitrox_q_vector *qvec = (void *)(uintptr_t)(data);
	struct nitrox_cmdq *cmdq = qvec->cmdq;
	union nps_pkt_slc_cnts slc_cnts;

	 
	slc_cnts.value = readq(cmdq->compl_cnt_csr_addr);
	 
	slc_cnts.s.resend = 1;

	process_response_list(cmdq);

	 
	writeq(slc_cnts.value, cmdq->compl_cnt_csr_addr);

	if (atomic_read(&cmdq->backlog_count))
		schedule_work(&cmdq->backlog_qflush);
}
