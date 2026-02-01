
 

 

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/unaligned.h>
#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/debug.h>
#include <linux/sunrpc/rpc_rdma.h>
#include <linux/sunrpc/svc_rdma.h>

#include "xprt_rdma.h"
#include <trace/events/rpcrdma.h>

static void svc_rdma_wc_receive(struct ib_cq *cq, struct ib_wc *wc);

static inline struct svc_rdma_recv_ctxt *
svc_rdma_next_recv_ctxt(struct list_head *list)
{
	return list_first_entry_or_null(list, struct svc_rdma_recv_ctxt,
					rc_list);
}

static void svc_rdma_recv_cid_init(struct svcxprt_rdma *rdma,
				   struct rpc_rdma_cid *cid)
{
	cid->ci_queue_id = rdma->sc_rq_cq->res.id;
	cid->ci_completion_id = atomic_inc_return(&rdma->sc_completion_ids);
}

static struct svc_rdma_recv_ctxt *
svc_rdma_recv_ctxt_alloc(struct svcxprt_rdma *rdma)
{
	int node = ibdev_to_node(rdma->sc_cm_id->device);
	struct svc_rdma_recv_ctxt *ctxt;
	dma_addr_t addr;
	void *buffer;

	ctxt = kmalloc_node(sizeof(*ctxt), GFP_KERNEL, node);
	if (!ctxt)
		goto fail0;
	buffer = kmalloc_node(rdma->sc_max_req_size, GFP_KERNEL, node);
	if (!buffer)
		goto fail1;
	addr = ib_dma_map_single(rdma->sc_pd->device, buffer,
				 rdma->sc_max_req_size, DMA_FROM_DEVICE);
	if (ib_dma_mapping_error(rdma->sc_pd->device, addr))
		goto fail2;

	svc_rdma_recv_cid_init(rdma, &ctxt->rc_cid);
	pcl_init(&ctxt->rc_call_pcl);
	pcl_init(&ctxt->rc_read_pcl);
	pcl_init(&ctxt->rc_write_pcl);
	pcl_init(&ctxt->rc_reply_pcl);

	ctxt->rc_recv_wr.next = NULL;
	ctxt->rc_recv_wr.wr_cqe = &ctxt->rc_cqe;
	ctxt->rc_recv_wr.sg_list = &ctxt->rc_recv_sge;
	ctxt->rc_recv_wr.num_sge = 1;
	ctxt->rc_cqe.done = svc_rdma_wc_receive;
	ctxt->rc_recv_sge.addr = addr;
	ctxt->rc_recv_sge.length = rdma->sc_max_req_size;
	ctxt->rc_recv_sge.lkey = rdma->sc_pd->local_dma_lkey;
	ctxt->rc_recv_buf = buffer;
	return ctxt;

fail2:
	kfree(buffer);
fail1:
	kfree(ctxt);
fail0:
	return NULL;
}

static void svc_rdma_recv_ctxt_destroy(struct svcxprt_rdma *rdma,
				       struct svc_rdma_recv_ctxt *ctxt)
{
	ib_dma_unmap_single(rdma->sc_pd->device, ctxt->rc_recv_sge.addr,
			    ctxt->rc_recv_sge.length, DMA_FROM_DEVICE);
	kfree(ctxt->rc_recv_buf);
	kfree(ctxt);
}

 
void svc_rdma_recv_ctxts_destroy(struct svcxprt_rdma *rdma)
{
	struct svc_rdma_recv_ctxt *ctxt;
	struct llist_node *node;

	while ((node = llist_del_first(&rdma->sc_recv_ctxts))) {
		ctxt = llist_entry(node, struct svc_rdma_recv_ctxt, rc_node);
		svc_rdma_recv_ctxt_destroy(rdma, ctxt);
	}
}

 
struct svc_rdma_recv_ctxt *svc_rdma_recv_ctxt_get(struct svcxprt_rdma *rdma)
{
	struct svc_rdma_recv_ctxt *ctxt;
	struct llist_node *node;

	node = llist_del_first(&rdma->sc_recv_ctxts);
	if (!node)
		goto out_empty;
	ctxt = llist_entry(node, struct svc_rdma_recv_ctxt, rc_node);

out:
	ctxt->rc_page_count = 0;
	return ctxt;

out_empty:
	ctxt = svc_rdma_recv_ctxt_alloc(rdma);
	if (!ctxt)
		return NULL;
	goto out;
}

 
void svc_rdma_recv_ctxt_put(struct svcxprt_rdma *rdma,
			    struct svc_rdma_recv_ctxt *ctxt)
{
	pcl_free(&ctxt->rc_call_pcl);
	pcl_free(&ctxt->rc_read_pcl);
	pcl_free(&ctxt->rc_write_pcl);
	pcl_free(&ctxt->rc_reply_pcl);

	llist_add(&ctxt->rc_node, &rdma->sc_recv_ctxts);
}

 
void svc_rdma_release_ctxt(struct svc_xprt *xprt, void *vctxt)
{
	struct svc_rdma_recv_ctxt *ctxt = vctxt;
	struct svcxprt_rdma *rdma =
		container_of(xprt, struct svcxprt_rdma, sc_xprt);

	if (ctxt)
		svc_rdma_recv_ctxt_put(rdma, ctxt);
}

static bool svc_rdma_refresh_recvs(struct svcxprt_rdma *rdma,
				   unsigned int wanted)
{
	const struct ib_recv_wr *bad_wr = NULL;
	struct svc_rdma_recv_ctxt *ctxt;
	struct ib_recv_wr *recv_chain;
	int ret;

	if (test_bit(XPT_CLOSE, &rdma->sc_xprt.xpt_flags))
		return false;

	recv_chain = NULL;
	while (wanted--) {
		ctxt = svc_rdma_recv_ctxt_get(rdma);
		if (!ctxt)
			break;

		trace_svcrdma_post_recv(ctxt);
		ctxt->rc_recv_wr.next = recv_chain;
		recv_chain = &ctxt->rc_recv_wr;
		rdma->sc_pending_recvs++;
	}
	if (!recv_chain)
		return false;

	ret = ib_post_recv(rdma->sc_qp, recv_chain, &bad_wr);
	if (ret)
		goto err_free;
	return true;

err_free:
	trace_svcrdma_rq_post_err(rdma, ret);
	while (bad_wr) {
		ctxt = container_of(bad_wr, struct svc_rdma_recv_ctxt,
				    rc_recv_wr);
		bad_wr = bad_wr->next;
		svc_rdma_recv_ctxt_put(rdma, ctxt);
	}
	 
	return false;
}

 
bool svc_rdma_post_recvs(struct svcxprt_rdma *rdma)
{
	return svc_rdma_refresh_recvs(rdma, rdma->sc_max_requests);
}

 
static void svc_rdma_wc_receive(struct ib_cq *cq, struct ib_wc *wc)
{
	struct svcxprt_rdma *rdma = cq->cq_context;
	struct ib_cqe *cqe = wc->wr_cqe;
	struct svc_rdma_recv_ctxt *ctxt;

	rdma->sc_pending_recvs--;

	 
	ctxt = container_of(cqe, struct svc_rdma_recv_ctxt, rc_cqe);

	if (wc->status != IB_WC_SUCCESS)
		goto flushed;
	trace_svcrdma_wc_recv(wc, &ctxt->rc_cid);

	 
	if (rdma->sc_pending_recvs < rdma->sc_max_requests)
		if (!svc_rdma_refresh_recvs(rdma, rdma->sc_recv_batch))
			goto dropped;

	 
	ctxt->rc_byte_len = wc->byte_len;

	spin_lock(&rdma->sc_rq_dto_lock);
	list_add_tail(&ctxt->rc_list, &rdma->sc_rq_dto_q);
	 
	set_bit(XPT_DATA, &rdma->sc_xprt.xpt_flags);
	spin_unlock(&rdma->sc_rq_dto_lock);
	if (!test_bit(RDMAXPRT_CONN_PENDING, &rdma->sc_flags))
		svc_xprt_enqueue(&rdma->sc_xprt);
	return;

flushed:
	if (wc->status == IB_WC_WR_FLUSH_ERR)
		trace_svcrdma_wc_recv_flush(wc, &ctxt->rc_cid);
	else
		trace_svcrdma_wc_recv_err(wc, &ctxt->rc_cid);
dropped:
	svc_rdma_recv_ctxt_put(rdma, ctxt);
	svc_xprt_deferred_close(&rdma->sc_xprt);
}

 
void svc_rdma_flush_recv_queues(struct svcxprt_rdma *rdma)
{
	struct svc_rdma_recv_ctxt *ctxt;

	while ((ctxt = svc_rdma_next_recv_ctxt(&rdma->sc_rq_dto_q))) {
		list_del(&ctxt->rc_list);
		svc_rdma_recv_ctxt_put(rdma, ctxt);
	}
}

static void svc_rdma_build_arg_xdr(struct svc_rqst *rqstp,
				   struct svc_rdma_recv_ctxt *ctxt)
{
	struct xdr_buf *arg = &rqstp->rq_arg;

	arg->head[0].iov_base = ctxt->rc_recv_buf;
	arg->head[0].iov_len = ctxt->rc_byte_len;
	arg->tail[0].iov_base = NULL;
	arg->tail[0].iov_len = 0;
	arg->page_len = 0;
	arg->page_base = 0;
	arg->buflen = ctxt->rc_byte_len;
	arg->len = ctxt->rc_byte_len;
}

 
static bool xdr_count_read_segments(struct svc_rdma_recv_ctxt *rctxt, __be32 *p)
{
	rctxt->rc_call_pcl.cl_count = 0;
	rctxt->rc_read_pcl.cl_count = 0;
	while (xdr_item_is_present(p)) {
		u32 position, handle, length;
		u64 offset;

		p = xdr_inline_decode(&rctxt->rc_stream,
				      rpcrdma_readseg_maxsz * sizeof(*p));
		if (!p)
			return false;

		xdr_decode_read_segment(p, &position, &handle,
					    &length, &offset);
		if (position) {
			if (position & 3)
				return false;
			++rctxt->rc_read_pcl.cl_count;
		} else {
			++rctxt->rc_call_pcl.cl_count;
		}

		p = xdr_inline_decode(&rctxt->rc_stream, sizeof(*p));
		if (!p)
			return false;
	}
	return true;
}

 
static bool xdr_check_read_list(struct svc_rdma_recv_ctxt *rctxt)
{
	__be32 *p;

	p = xdr_inline_decode(&rctxt->rc_stream, sizeof(*p));
	if (!p)
		return false;
	if (!xdr_count_read_segments(rctxt, p))
		return false;
	if (!pcl_alloc_call(rctxt, p))
		return false;
	return pcl_alloc_read(rctxt, p);
}

static bool xdr_check_write_chunk(struct svc_rdma_recv_ctxt *rctxt)
{
	u32 segcount;
	__be32 *p;

	if (xdr_stream_decode_u32(&rctxt->rc_stream, &segcount))
		return false;

	 
	p = xdr_inline_decode(&rctxt->rc_stream,
			      segcount * rpcrdma_segment_maxsz * sizeof(*p));
	return p != NULL;
}

 
static bool xdr_count_write_chunks(struct svc_rdma_recv_ctxt *rctxt, __be32 *p)
{
	rctxt->rc_write_pcl.cl_count = 0;
	while (xdr_item_is_present(p)) {
		if (!xdr_check_write_chunk(rctxt))
			return false;
		++rctxt->rc_write_pcl.cl_count;
		p = xdr_inline_decode(&rctxt->rc_stream, sizeof(*p));
		if (!p)
			return false;
	}
	return true;
}

 
static bool xdr_check_write_list(struct svc_rdma_recv_ctxt *rctxt)
{
	__be32 *p;

	p = xdr_inline_decode(&rctxt->rc_stream, sizeof(*p));
	if (!p)
		return false;
	if (!xdr_count_write_chunks(rctxt, p))
		return false;
	if (!pcl_alloc_write(rctxt, &rctxt->rc_write_pcl, p))
		return false;

	rctxt->rc_cur_result_payload = pcl_first_chunk(&rctxt->rc_write_pcl);
	return true;
}

 
static bool xdr_check_reply_chunk(struct svc_rdma_recv_ctxt *rctxt)
{
	__be32 *p;

	p = xdr_inline_decode(&rctxt->rc_stream, sizeof(*p));
	if (!p)
		return false;

	if (!xdr_item_is_present(p))
		return true;
	if (!xdr_check_write_chunk(rctxt))
		return false;

	rctxt->rc_reply_pcl.cl_count = 1;
	return pcl_alloc_write(rctxt, &rctxt->rc_reply_pcl, p);
}

 
static void svc_rdma_get_inv_rkey(struct svcxprt_rdma *rdma,
				  struct svc_rdma_recv_ctxt *ctxt)
{
	struct svc_rdma_segment *segment;
	struct svc_rdma_chunk *chunk;
	u32 inv_rkey;

	ctxt->rc_inv_rkey = 0;

	if (!rdma->sc_snd_w_inv)
		return;

	inv_rkey = 0;
	pcl_for_each_chunk(chunk, &ctxt->rc_call_pcl) {
		pcl_for_each_segment(segment, chunk) {
			if (inv_rkey == 0)
				inv_rkey = segment->rs_handle;
			else if (inv_rkey != segment->rs_handle)
				return;
		}
	}
	pcl_for_each_chunk(chunk, &ctxt->rc_read_pcl) {
		pcl_for_each_segment(segment, chunk) {
			if (inv_rkey == 0)
				inv_rkey = segment->rs_handle;
			else if (inv_rkey != segment->rs_handle)
				return;
		}
	}
	pcl_for_each_chunk(chunk, &ctxt->rc_write_pcl) {
		pcl_for_each_segment(segment, chunk) {
			if (inv_rkey == 0)
				inv_rkey = segment->rs_handle;
			else if (inv_rkey != segment->rs_handle)
				return;
		}
	}
	pcl_for_each_chunk(chunk, &ctxt->rc_reply_pcl) {
		pcl_for_each_segment(segment, chunk) {
			if (inv_rkey == 0)
				inv_rkey = segment->rs_handle;
			else if (inv_rkey != segment->rs_handle)
				return;
		}
	}
	ctxt->rc_inv_rkey = inv_rkey;
}

 
static int svc_rdma_xdr_decode_req(struct xdr_buf *rq_arg,
				   struct svc_rdma_recv_ctxt *rctxt)
{
	__be32 *p, *rdma_argp;
	unsigned int hdr_len;

	rdma_argp = rq_arg->head[0].iov_base;
	xdr_init_decode(&rctxt->rc_stream, rq_arg, rdma_argp, NULL);

	p = xdr_inline_decode(&rctxt->rc_stream,
			      rpcrdma_fixed_maxsz * sizeof(*p));
	if (unlikely(!p))
		goto out_short;
	p++;
	if (*p != rpcrdma_version)
		goto out_version;
	p += 2;
	rctxt->rc_msgtype = *p;
	switch (rctxt->rc_msgtype) {
	case rdma_msg:
		break;
	case rdma_nomsg:
		break;
	case rdma_done:
		goto out_drop;
	case rdma_error:
		goto out_drop;
	default:
		goto out_proc;
	}

	if (!xdr_check_read_list(rctxt))
		goto out_inval;
	if (!xdr_check_write_list(rctxt))
		goto out_inval;
	if (!xdr_check_reply_chunk(rctxt))
		goto out_inval;

	rq_arg->head[0].iov_base = rctxt->rc_stream.p;
	hdr_len = xdr_stream_pos(&rctxt->rc_stream);
	rq_arg->head[0].iov_len -= hdr_len;
	rq_arg->len -= hdr_len;
	trace_svcrdma_decode_rqst(rctxt, rdma_argp, hdr_len);
	return hdr_len;

out_short:
	trace_svcrdma_decode_short_err(rctxt, rq_arg->len);
	return -EINVAL;

out_version:
	trace_svcrdma_decode_badvers_err(rctxt, rdma_argp);
	return -EPROTONOSUPPORT;

out_drop:
	trace_svcrdma_decode_drop_err(rctxt, rdma_argp);
	return 0;

out_proc:
	trace_svcrdma_decode_badproc_err(rctxt, rdma_argp);
	return -EINVAL;

out_inval:
	trace_svcrdma_decode_parse_err(rctxt, rdma_argp);
	return -EINVAL;
}

static void svc_rdma_send_error(struct svcxprt_rdma *rdma,
				struct svc_rdma_recv_ctxt *rctxt,
				int status)
{
	struct svc_rdma_send_ctxt *sctxt;

	sctxt = svc_rdma_send_ctxt_get(rdma);
	if (!sctxt)
		return;
	svc_rdma_send_error_msg(rdma, sctxt, rctxt, status);
}

 
static bool svc_rdma_is_reverse_direction_reply(struct svc_xprt *xprt,
						struct svc_rdma_recv_ctxt *rctxt)
{
	__be32 *p = rctxt->rc_recv_buf;

	if (!xprt->xpt_bc_xprt)
		return false;

	if (rctxt->rc_msgtype != rdma_msg)
		return false;

	if (!pcl_is_empty(&rctxt->rc_call_pcl))
		return false;
	if (!pcl_is_empty(&rctxt->rc_read_pcl))
		return false;
	if (!pcl_is_empty(&rctxt->rc_write_pcl))
		return false;
	if (!pcl_is_empty(&rctxt->rc_reply_pcl))
		return false;

	 
	if (*(p + 8) == cpu_to_be32(RPC_CALL))
		return false;

	return true;
}

 
int svc_rdma_recvfrom(struct svc_rqst *rqstp)
{
	struct svc_xprt *xprt = rqstp->rq_xprt;
	struct svcxprt_rdma *rdma_xprt =
		container_of(xprt, struct svcxprt_rdma, sc_xprt);
	struct svc_rdma_recv_ctxt *ctxt;
	int ret;

	 
	rqstp->rq_respages = rqstp->rq_pages;
	rqstp->rq_next_page = rqstp->rq_respages;

	rqstp->rq_xprt_ctxt = NULL;

	ctxt = NULL;
	spin_lock(&rdma_xprt->sc_rq_dto_lock);
	ctxt = svc_rdma_next_recv_ctxt(&rdma_xprt->sc_rq_dto_q);
	if (ctxt)
		list_del(&ctxt->rc_list);
	else
		 
		clear_bit(XPT_DATA, &xprt->xpt_flags);
	spin_unlock(&rdma_xprt->sc_rq_dto_lock);

	 
	svc_xprt_received(xprt);
	if (!ctxt)
		return 0;

	percpu_counter_inc(&svcrdma_stat_recv);
	ib_dma_sync_single_for_cpu(rdma_xprt->sc_pd->device,
				   ctxt->rc_recv_sge.addr, ctxt->rc_byte_len,
				   DMA_FROM_DEVICE);
	svc_rdma_build_arg_xdr(rqstp, ctxt);

	ret = svc_rdma_xdr_decode_req(&rqstp->rq_arg, ctxt);
	if (ret < 0)
		goto out_err;
	if (ret == 0)
		goto out_drop;

	if (svc_rdma_is_reverse_direction_reply(xprt, ctxt))
		goto out_backchannel;

	svc_rdma_get_inv_rkey(rdma_xprt, ctxt);

	if (!pcl_is_empty(&ctxt->rc_read_pcl) ||
	    !pcl_is_empty(&ctxt->rc_call_pcl)) {
		ret = svc_rdma_process_read_list(rdma_xprt, rqstp, ctxt);
		if (ret < 0)
			goto out_readfail;
	}

	rqstp->rq_xprt_ctxt = ctxt;
	rqstp->rq_prot = IPPROTO_MAX;
	svc_xprt_copy_addrs(rqstp, xprt);
	set_bit(RQ_SECURE, &rqstp->rq_flags);
	return rqstp->rq_arg.len;

out_err:
	svc_rdma_send_error(rdma_xprt, ctxt, ret);
	svc_rdma_recv_ctxt_put(rdma_xprt, ctxt);
	return 0;

out_readfail:
	if (ret == -EINVAL)
		svc_rdma_send_error(rdma_xprt, ctxt, ret);
	svc_rdma_recv_ctxt_put(rdma_xprt, ctxt);
	svc_xprt_deferred_close(xprt);
	return -ENOTCONN;

out_backchannel:
	svc_rdma_handle_bc_reply(rqstp, ctxt);
out_drop:
	svc_rdma_recv_ctxt_put(rdma_xprt, ctxt);
	return 0;
}
