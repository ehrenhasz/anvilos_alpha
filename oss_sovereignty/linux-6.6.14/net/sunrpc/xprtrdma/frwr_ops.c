
 

 

 

 

#include <linux/sunrpc/svc_rdma.h>

#include "xprt_rdma.h"
#include <trace/events/rpcrdma.h>

static void frwr_cid_init(struct rpcrdma_ep *ep,
			  struct rpcrdma_mr *mr)
{
	struct rpc_rdma_cid *cid = &mr->mr_cid;

	cid->ci_queue_id = ep->re_attr.send_cq->res.id;
	cid->ci_completion_id = mr->mr_ibmr->res.id;
}

static void frwr_mr_unmap(struct rpcrdma_xprt *r_xprt, struct rpcrdma_mr *mr)
{
	if (mr->mr_device) {
		trace_xprtrdma_mr_unmap(mr);
		ib_dma_unmap_sg(mr->mr_device, mr->mr_sg, mr->mr_nents,
				mr->mr_dir);
		mr->mr_device = NULL;
	}
}

 
void frwr_mr_release(struct rpcrdma_mr *mr)
{
	int rc;

	frwr_mr_unmap(mr->mr_xprt, mr);

	rc = ib_dereg_mr(mr->mr_ibmr);
	if (rc)
		trace_xprtrdma_frwr_dereg(mr, rc);
	kfree(mr->mr_sg);
	kfree(mr);
}

static void frwr_mr_put(struct rpcrdma_mr *mr)
{
	frwr_mr_unmap(mr->mr_xprt, mr);

	 
	rpcrdma_mr_push(mr, &mr->mr_req->rl_free_mrs);
}

 
void frwr_reset(struct rpcrdma_req *req)
{
	struct rpcrdma_mr *mr;

	while ((mr = rpcrdma_mr_pop(&req->rl_registered)))
		frwr_mr_put(mr);
}

 
int frwr_mr_init(struct rpcrdma_xprt *r_xprt, struct rpcrdma_mr *mr)
{
	struct rpcrdma_ep *ep = r_xprt->rx_ep;
	unsigned int depth = ep->re_max_fr_depth;
	struct scatterlist *sg;
	struct ib_mr *frmr;

	sg = kcalloc_node(depth, sizeof(*sg), XPRTRDMA_GFP_FLAGS,
			  ibdev_to_node(ep->re_id->device));
	if (!sg)
		return -ENOMEM;

	frmr = ib_alloc_mr(ep->re_pd, ep->re_mrtype, depth);
	if (IS_ERR(frmr))
		goto out_mr_err;

	mr->mr_xprt = r_xprt;
	mr->mr_ibmr = frmr;
	mr->mr_device = NULL;
	INIT_LIST_HEAD(&mr->mr_list);
	init_completion(&mr->mr_linv_done);
	frwr_cid_init(ep, mr);

	sg_init_table(sg, depth);
	mr->mr_sg = sg;
	return 0;

out_mr_err:
	kfree(sg);
	trace_xprtrdma_frwr_alloc(mr, PTR_ERR(frmr));
	return PTR_ERR(frmr);
}

 
int frwr_query_device(struct rpcrdma_ep *ep, const struct ib_device *device)
{
	const struct ib_device_attr *attrs = &device->attrs;
	int max_qp_wr, depth, delta;
	unsigned int max_sge;

	if (!(attrs->device_cap_flags & IB_DEVICE_MEM_MGT_EXTENSIONS) ||
	    attrs->max_fast_reg_page_list_len == 0) {
		pr_err("rpcrdma: 'frwr' mode is not supported by device %s\n",
		       device->name);
		return -EINVAL;
	}

	max_sge = min_t(unsigned int, attrs->max_send_sge,
			RPCRDMA_MAX_SEND_SGES);
	if (max_sge < RPCRDMA_MIN_SEND_SGES) {
		pr_err("rpcrdma: HCA provides only %u send SGEs\n", max_sge);
		return -ENOMEM;
	}
	ep->re_attr.cap.max_send_sge = max_sge;
	ep->re_attr.cap.max_recv_sge = 1;

	ep->re_mrtype = IB_MR_TYPE_MEM_REG;
	if (attrs->kernel_cap_flags & IBK_SG_GAPS_REG)
		ep->re_mrtype = IB_MR_TYPE_SG_GAPS;

	 
	if (attrs->max_sge_rd > RPCRDMA_MAX_HDR_SEGS)
		ep->re_max_fr_depth = attrs->max_sge_rd;
	else
		ep->re_max_fr_depth = attrs->max_fast_reg_page_list_len;
	if (ep->re_max_fr_depth > RPCRDMA_MAX_DATA_SEGS)
		ep->re_max_fr_depth = RPCRDMA_MAX_DATA_SEGS;

	 
	depth = 7;

	 
	if (ep->re_max_fr_depth < RPCRDMA_MAX_DATA_SEGS) {
		delta = RPCRDMA_MAX_DATA_SEGS - ep->re_max_fr_depth;
		do {
			depth += 2;  
			delta -= ep->re_max_fr_depth;
		} while (delta > 0);
	}

	max_qp_wr = attrs->max_qp_wr;
	max_qp_wr -= RPCRDMA_BACKWARD_WRS;
	max_qp_wr -= 1;
	if (max_qp_wr < RPCRDMA_MIN_SLOT_TABLE)
		return -ENOMEM;
	if (ep->re_max_requests > max_qp_wr)
		ep->re_max_requests = max_qp_wr;
	ep->re_attr.cap.max_send_wr = ep->re_max_requests * depth;
	if (ep->re_attr.cap.max_send_wr > max_qp_wr) {
		ep->re_max_requests = max_qp_wr / depth;
		if (!ep->re_max_requests)
			return -ENOMEM;
		ep->re_attr.cap.max_send_wr = ep->re_max_requests * depth;
	}
	ep->re_attr.cap.max_send_wr += RPCRDMA_BACKWARD_WRS;
	ep->re_attr.cap.max_send_wr += 1;  
	ep->re_attr.cap.max_recv_wr = ep->re_max_requests;
	ep->re_attr.cap.max_recv_wr += RPCRDMA_BACKWARD_WRS;
	ep->re_attr.cap.max_recv_wr += RPCRDMA_MAX_RECV_BATCH;
	ep->re_attr.cap.max_recv_wr += 1;  

	ep->re_max_rdma_segs =
		DIV_ROUND_UP(RPCRDMA_MAX_DATA_SEGS, ep->re_max_fr_depth);
	 
	ep->re_max_rdma_segs += 2;
	if (ep->re_max_rdma_segs > RPCRDMA_MAX_HDR_SEGS)
		ep->re_max_rdma_segs = RPCRDMA_MAX_HDR_SEGS;

	 
	if ((ep->re_max_rdma_segs * ep->re_max_fr_depth) < RPCRDMA_MAX_SEGS)
		return -ENOMEM;

	return 0;
}

 
struct rpcrdma_mr_seg *frwr_map(struct rpcrdma_xprt *r_xprt,
				struct rpcrdma_mr_seg *seg,
				int nsegs, bool writing, __be32 xid,
				struct rpcrdma_mr *mr)
{
	struct rpcrdma_ep *ep = r_xprt->rx_ep;
	struct ib_reg_wr *reg_wr;
	int i, n, dma_nents;
	struct ib_mr *ibmr;
	u8 key;

	if (nsegs > ep->re_max_fr_depth)
		nsegs = ep->re_max_fr_depth;
	for (i = 0; i < nsegs;) {
		sg_set_page(&mr->mr_sg[i], seg->mr_page,
			    seg->mr_len, seg->mr_offset);

		++seg;
		++i;
		if (ep->re_mrtype == IB_MR_TYPE_SG_GAPS)
			continue;
		if ((i < nsegs && seg->mr_offset) ||
		    offset_in_page((seg-1)->mr_offset + (seg-1)->mr_len))
			break;
	}
	mr->mr_dir = rpcrdma_data_dir(writing);
	mr->mr_nents = i;

	dma_nents = ib_dma_map_sg(ep->re_id->device, mr->mr_sg, mr->mr_nents,
				  mr->mr_dir);
	if (!dma_nents)
		goto out_dmamap_err;
	mr->mr_device = ep->re_id->device;

	ibmr = mr->mr_ibmr;
	n = ib_map_mr_sg(ibmr, mr->mr_sg, dma_nents, NULL, PAGE_SIZE);
	if (n != dma_nents)
		goto out_mapmr_err;

	ibmr->iova &= 0x00000000ffffffff;
	ibmr->iova |= ((u64)be32_to_cpu(xid)) << 32;
	key = (u8)(ibmr->rkey & 0x000000FF);
	ib_update_fast_reg_key(ibmr, ++key);

	reg_wr = &mr->mr_regwr;
	reg_wr->mr = ibmr;
	reg_wr->key = ibmr->rkey;
	reg_wr->access = writing ?
			 IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE :
			 IB_ACCESS_REMOTE_READ;

	mr->mr_handle = ibmr->rkey;
	mr->mr_length = ibmr->length;
	mr->mr_offset = ibmr->iova;
	trace_xprtrdma_mr_map(mr);

	return seg;

out_dmamap_err:
	trace_xprtrdma_frwr_sgerr(mr, i);
	return ERR_PTR(-EIO);

out_mapmr_err:
	trace_xprtrdma_frwr_maperr(mr, n);
	return ERR_PTR(-EIO);
}

 
static void frwr_wc_fastreg(struct ib_cq *cq, struct ib_wc *wc)
{
	struct ib_cqe *cqe = wc->wr_cqe;
	struct rpcrdma_mr *mr = container_of(cqe, struct rpcrdma_mr, mr_cqe);

	 
	trace_xprtrdma_wc_fastreg(wc, &mr->mr_cid);

	rpcrdma_flush_disconnect(cq->cq_context, wc);
}

 
int frwr_send(struct rpcrdma_xprt *r_xprt, struct rpcrdma_req *req)
{
	struct ib_send_wr *post_wr, *send_wr = &req->rl_wr;
	struct rpcrdma_ep *ep = r_xprt->rx_ep;
	struct rpcrdma_mr *mr;
	unsigned int num_wrs;
	int ret;

	num_wrs = 1;
	post_wr = send_wr;
	list_for_each_entry(mr, &req->rl_registered, mr_list) {
		trace_xprtrdma_mr_fastreg(mr);

		mr->mr_cqe.done = frwr_wc_fastreg;
		mr->mr_regwr.wr.next = post_wr;
		mr->mr_regwr.wr.wr_cqe = &mr->mr_cqe;
		mr->mr_regwr.wr.num_sge = 0;
		mr->mr_regwr.wr.opcode = IB_WR_REG_MR;
		mr->mr_regwr.wr.send_flags = 0;
		post_wr = &mr->mr_regwr.wr;
		++num_wrs;
	}

	if ((kref_read(&req->rl_kref) > 1) || num_wrs > ep->re_send_count) {
		send_wr->send_flags |= IB_SEND_SIGNALED;
		ep->re_send_count = min_t(unsigned int, ep->re_send_batch,
					  num_wrs - ep->re_send_count);
	} else {
		send_wr->send_flags &= ~IB_SEND_SIGNALED;
		ep->re_send_count -= num_wrs;
	}

	trace_xprtrdma_post_send(req);
	ret = ib_post_send(ep->re_id->qp, post_wr, NULL);
	if (ret)
		trace_xprtrdma_post_send_err(r_xprt, req, ret);
	return ret;
}

 
void frwr_reminv(struct rpcrdma_rep *rep, struct list_head *mrs)
{
	struct rpcrdma_mr *mr;

	list_for_each_entry(mr, mrs, mr_list)
		if (mr->mr_handle == rep->rr_inv_rkey) {
			list_del_init(&mr->mr_list);
			trace_xprtrdma_mr_reminv(mr);
			frwr_mr_put(mr);
			break;	 
		}
}

static void frwr_mr_done(struct ib_wc *wc, struct rpcrdma_mr *mr)
{
	if (likely(wc->status == IB_WC_SUCCESS))
		frwr_mr_put(mr);
}

 
static void frwr_wc_localinv(struct ib_cq *cq, struct ib_wc *wc)
{
	struct ib_cqe *cqe = wc->wr_cqe;
	struct rpcrdma_mr *mr = container_of(cqe, struct rpcrdma_mr, mr_cqe);

	 
	trace_xprtrdma_wc_li(wc, &mr->mr_cid);
	frwr_mr_done(wc, mr);

	rpcrdma_flush_disconnect(cq->cq_context, wc);
}

 
static void frwr_wc_localinv_wake(struct ib_cq *cq, struct ib_wc *wc)
{
	struct ib_cqe *cqe = wc->wr_cqe;
	struct rpcrdma_mr *mr = container_of(cqe, struct rpcrdma_mr, mr_cqe);

	 
	trace_xprtrdma_wc_li_wake(wc, &mr->mr_cid);
	frwr_mr_done(wc, mr);
	complete(&mr->mr_linv_done);

	rpcrdma_flush_disconnect(cq->cq_context, wc);
}

 
void frwr_unmap_sync(struct rpcrdma_xprt *r_xprt, struct rpcrdma_req *req)
{
	struct ib_send_wr *first, **prev, *last;
	struct rpcrdma_ep *ep = r_xprt->rx_ep;
	const struct ib_send_wr *bad_wr;
	struct rpcrdma_mr *mr;
	int rc;

	 
	prev = &first;
	mr = rpcrdma_mr_pop(&req->rl_registered);
	do {
		trace_xprtrdma_mr_localinv(mr);
		r_xprt->rx_stats.local_inv_needed++;

		last = &mr->mr_invwr;
		last->next = NULL;
		last->wr_cqe = &mr->mr_cqe;
		last->sg_list = NULL;
		last->num_sge = 0;
		last->opcode = IB_WR_LOCAL_INV;
		last->send_flags = IB_SEND_SIGNALED;
		last->ex.invalidate_rkey = mr->mr_handle;

		last->wr_cqe->done = frwr_wc_localinv;

		*prev = last;
		prev = &last->next;
	} while ((mr = rpcrdma_mr_pop(&req->rl_registered)));

	mr = container_of(last, struct rpcrdma_mr, mr_invwr);

	 
	last->wr_cqe->done = frwr_wc_localinv_wake;
	reinit_completion(&mr->mr_linv_done);

	 
	bad_wr = NULL;
	rc = ib_post_send(ep->re_id->qp, first, &bad_wr);

	 
	if (bad_wr != first)
		wait_for_completion(&mr->mr_linv_done);
	if (!rc)
		return;

	 
	trace_xprtrdma_post_linv_err(req, rc);

	 
	rpcrdma_force_disconnect(ep);
}

 
static void frwr_wc_localinv_done(struct ib_cq *cq, struct ib_wc *wc)
{
	struct ib_cqe *cqe = wc->wr_cqe;
	struct rpcrdma_mr *mr = container_of(cqe, struct rpcrdma_mr, mr_cqe);
	struct rpcrdma_rep *rep;

	 
	trace_xprtrdma_wc_li_done(wc, &mr->mr_cid);

	 
	rep = mr->mr_req->rl_reply;
	smp_rmb();

	if (wc->status != IB_WC_SUCCESS) {
		if (rep)
			rpcrdma_unpin_rqst(rep);
		rpcrdma_flush_disconnect(cq->cq_context, wc);
		return;
	}
	frwr_mr_put(mr);
	rpcrdma_complete_rqst(rep);
}

 
void frwr_unmap_async(struct rpcrdma_xprt *r_xprt, struct rpcrdma_req *req)
{
	struct ib_send_wr *first, *last, **prev;
	struct rpcrdma_ep *ep = r_xprt->rx_ep;
	struct rpcrdma_mr *mr;
	int rc;

	 
	prev = &first;
	mr = rpcrdma_mr_pop(&req->rl_registered);
	do {
		trace_xprtrdma_mr_localinv(mr);
		r_xprt->rx_stats.local_inv_needed++;

		last = &mr->mr_invwr;
		last->next = NULL;
		last->wr_cqe = &mr->mr_cqe;
		last->sg_list = NULL;
		last->num_sge = 0;
		last->opcode = IB_WR_LOCAL_INV;
		last->send_flags = IB_SEND_SIGNALED;
		last->ex.invalidate_rkey = mr->mr_handle;

		last->wr_cqe->done = frwr_wc_localinv;

		*prev = last;
		prev = &last->next;
	} while ((mr = rpcrdma_mr_pop(&req->rl_registered)));

	 
	last->wr_cqe->done = frwr_wc_localinv_done;

	 
	rc = ib_post_send(ep->re_id->qp, first, NULL);
	if (!rc)
		return;

	 
	trace_xprtrdma_post_linv_err(req, rc);

	 
	rpcrdma_unpin_rqst(req->rl_reply);

	 
	rpcrdma_force_disconnect(ep);
}

 
int frwr_wp_create(struct rpcrdma_xprt *r_xprt)
{
	struct rpcrdma_ep *ep = r_xprt->rx_ep;
	struct rpcrdma_mr_seg seg;
	struct rpcrdma_mr *mr;

	mr = rpcrdma_mr_get(r_xprt);
	if (!mr)
		return -EAGAIN;
	mr->mr_req = NULL;
	ep->re_write_pad_mr = mr;

	seg.mr_len = XDR_UNIT;
	seg.mr_page = virt_to_page(ep->re_write_pad);
	seg.mr_offset = offset_in_page(ep->re_write_pad);
	if (IS_ERR(frwr_map(r_xprt, &seg, 1, true, xdr_zero, mr)))
		return -EIO;
	trace_xprtrdma_mr_fastreg(mr);

	mr->mr_cqe.done = frwr_wc_fastreg;
	mr->mr_regwr.wr.next = NULL;
	mr->mr_regwr.wr.wr_cqe = &mr->mr_cqe;
	mr->mr_regwr.wr.num_sge = 0;
	mr->mr_regwr.wr.opcode = IB_WR_REG_MR;
	mr->mr_regwr.wr.send_flags = 0;

	return ib_post_send(ep->re_id->qp, &mr->mr_regwr.wr, NULL);
}
