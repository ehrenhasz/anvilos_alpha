
 

 

#include <linux/spinlock.h>
#include <asm/unaligned.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#include <linux/sunrpc/debug.h>
#include <linux/sunrpc/svc_rdma.h>

#include "xprt_rdma.h"
#include <trace/events/rpcrdma.h>

static void svc_rdma_wc_send(struct ib_cq *cq, struct ib_wc *wc);

static void svc_rdma_send_cid_init(struct svcxprt_rdma *rdma,
				   struct rpc_rdma_cid *cid)
{
	cid->ci_queue_id = rdma->sc_sq_cq->res.id;
	cid->ci_completion_id = atomic_inc_return(&rdma->sc_completion_ids);
}

static struct svc_rdma_send_ctxt *
svc_rdma_send_ctxt_alloc(struct svcxprt_rdma *rdma)
{
	int node = ibdev_to_node(rdma->sc_cm_id->device);
	struct svc_rdma_send_ctxt *ctxt;
	dma_addr_t addr;
	void *buffer;
	int i;

	ctxt = kmalloc_node(struct_size(ctxt, sc_sges, rdma->sc_max_send_sges),
			    GFP_KERNEL, node);
	if (!ctxt)
		goto fail0;
	buffer = kmalloc_node(rdma->sc_max_req_size, GFP_KERNEL, node);
	if (!buffer)
		goto fail1;
	addr = ib_dma_map_single(rdma->sc_pd->device, buffer,
				 rdma->sc_max_req_size, DMA_TO_DEVICE);
	if (ib_dma_mapping_error(rdma->sc_pd->device, addr))
		goto fail2;

	svc_rdma_send_cid_init(rdma, &ctxt->sc_cid);

	ctxt->sc_send_wr.next = NULL;
	ctxt->sc_send_wr.wr_cqe = &ctxt->sc_cqe;
	ctxt->sc_send_wr.sg_list = ctxt->sc_sges;
	ctxt->sc_send_wr.send_flags = IB_SEND_SIGNALED;
	ctxt->sc_cqe.done = svc_rdma_wc_send;
	ctxt->sc_xprt_buf = buffer;
	xdr_buf_init(&ctxt->sc_hdrbuf, ctxt->sc_xprt_buf,
		     rdma->sc_max_req_size);
	ctxt->sc_sges[0].addr = addr;

	for (i = 0; i < rdma->sc_max_send_sges; i++)
		ctxt->sc_sges[i].lkey = rdma->sc_pd->local_dma_lkey;
	return ctxt;

fail2:
	kfree(buffer);
fail1:
	kfree(ctxt);
fail0:
	return NULL;
}

 
void svc_rdma_send_ctxts_destroy(struct svcxprt_rdma *rdma)
{
	struct svc_rdma_send_ctxt *ctxt;
	struct llist_node *node;

	while ((node = llist_del_first(&rdma->sc_send_ctxts)) != NULL) {
		ctxt = llist_entry(node, struct svc_rdma_send_ctxt, sc_node);
		ib_dma_unmap_single(rdma->sc_pd->device,
				    ctxt->sc_sges[0].addr,
				    rdma->sc_max_req_size,
				    DMA_TO_DEVICE);
		kfree(ctxt->sc_xprt_buf);
		kfree(ctxt);
	}
}

 
struct svc_rdma_send_ctxt *svc_rdma_send_ctxt_get(struct svcxprt_rdma *rdma)
{
	struct svc_rdma_send_ctxt *ctxt;
	struct llist_node *node;

	spin_lock(&rdma->sc_send_lock);
	node = llist_del_first(&rdma->sc_send_ctxts);
	if (!node)
		goto out_empty;
	ctxt = llist_entry(node, struct svc_rdma_send_ctxt, sc_node);
	spin_unlock(&rdma->sc_send_lock);

out:
	rpcrdma_set_xdrlen(&ctxt->sc_hdrbuf, 0);
	xdr_init_encode(&ctxt->sc_stream, &ctxt->sc_hdrbuf,
			ctxt->sc_xprt_buf, NULL);

	ctxt->sc_send_wr.num_sge = 0;
	ctxt->sc_cur_sge_no = 0;
	ctxt->sc_page_count = 0;
	return ctxt;

out_empty:
	spin_unlock(&rdma->sc_send_lock);
	ctxt = svc_rdma_send_ctxt_alloc(rdma);
	if (!ctxt)
		return NULL;
	goto out;
}

 
void svc_rdma_send_ctxt_put(struct svcxprt_rdma *rdma,
			    struct svc_rdma_send_ctxt *ctxt)
{
	struct ib_device *device = rdma->sc_cm_id->device;
	unsigned int i;

	if (ctxt->sc_page_count)
		release_pages(ctxt->sc_pages, ctxt->sc_page_count);

	 
	for (i = 1; i < ctxt->sc_send_wr.num_sge; i++) {
		ib_dma_unmap_page(device,
				  ctxt->sc_sges[i].addr,
				  ctxt->sc_sges[i].length,
				  DMA_TO_DEVICE);
		trace_svcrdma_dma_unmap_page(rdma,
					     ctxt->sc_sges[i].addr,
					     ctxt->sc_sges[i].length);
	}

	llist_add(&ctxt->sc_node, &rdma->sc_send_ctxts);
}

 
void svc_rdma_wake_send_waiters(struct svcxprt_rdma *rdma, int avail)
{
	atomic_add(avail, &rdma->sc_sq_avail);
	smp_mb__after_atomic();
	if (unlikely(waitqueue_active(&rdma->sc_send_wait)))
		wake_up(&rdma->sc_send_wait);
}

 
static void svc_rdma_wc_send(struct ib_cq *cq, struct ib_wc *wc)
{
	struct svcxprt_rdma *rdma = cq->cq_context;
	struct ib_cqe *cqe = wc->wr_cqe;
	struct svc_rdma_send_ctxt *ctxt =
		container_of(cqe, struct svc_rdma_send_ctxt, sc_cqe);

	svc_rdma_wake_send_waiters(rdma, 1);

	if (unlikely(wc->status != IB_WC_SUCCESS))
		goto flushed;

	trace_svcrdma_wc_send(wc, &ctxt->sc_cid);
	svc_rdma_send_ctxt_put(rdma, ctxt);
	return;

flushed:
	if (wc->status != IB_WC_WR_FLUSH_ERR)
		trace_svcrdma_wc_send_err(wc, &ctxt->sc_cid);
	else
		trace_svcrdma_wc_send_flush(wc, &ctxt->sc_cid);
	svc_rdma_send_ctxt_put(rdma, ctxt);
	svc_xprt_deferred_close(&rdma->sc_xprt);
}

 
int svc_rdma_send(struct svcxprt_rdma *rdma, struct svc_rdma_send_ctxt *ctxt)
{
	struct ib_send_wr *wr = &ctxt->sc_send_wr;
	int ret;

	might_sleep();

	 
	ib_dma_sync_single_for_device(rdma->sc_pd->device,
				      wr->sg_list[0].addr,
				      wr->sg_list[0].length,
				      DMA_TO_DEVICE);

	 
	while (1) {
		if ((atomic_dec_return(&rdma->sc_sq_avail) < 0)) {
			percpu_counter_inc(&svcrdma_stat_sq_starve);
			trace_svcrdma_sq_full(rdma);
			atomic_inc(&rdma->sc_sq_avail);
			wait_event(rdma->sc_send_wait,
				   atomic_read(&rdma->sc_sq_avail) > 1);
			if (test_bit(XPT_CLOSE, &rdma->sc_xprt.xpt_flags))
				return -ENOTCONN;
			trace_svcrdma_sq_retry(rdma);
			continue;
		}

		trace_svcrdma_post_send(ctxt);
		ret = ib_post_send(rdma->sc_qp, wr, NULL);
		if (ret)
			break;
		return 0;
	}

	trace_svcrdma_sq_post_err(rdma, ret);
	svc_xprt_deferred_close(&rdma->sc_xprt);
	wake_up(&rdma->sc_send_wait);
	return ret;
}

 
static ssize_t svc_rdma_encode_read_list(struct svc_rdma_send_ctxt *sctxt)
{
	 
	return xdr_stream_encode_item_absent(&sctxt->sc_stream);
}

 
static ssize_t svc_rdma_encode_write_segment(struct svc_rdma_send_ctxt *sctxt,
					     const struct svc_rdma_chunk *chunk,
					     u32 *remaining, unsigned int segno)
{
	const struct svc_rdma_segment *segment = &chunk->ch_segments[segno];
	const size_t len = rpcrdma_segment_maxsz * sizeof(__be32);
	u32 length;
	__be32 *p;

	p = xdr_reserve_space(&sctxt->sc_stream, len);
	if (!p)
		return -EMSGSIZE;

	length = min_t(u32, *remaining, segment->rs_length);
	*remaining -= length;
	xdr_encode_rdma_segment(p, segment->rs_handle, length,
				segment->rs_offset);
	trace_svcrdma_encode_wseg(sctxt, segno, segment->rs_handle, length,
				  segment->rs_offset);
	return len;
}

 
static ssize_t svc_rdma_encode_write_chunk(struct svc_rdma_send_ctxt *sctxt,
					   const struct svc_rdma_chunk *chunk)
{
	u32 remaining = chunk->ch_payload_length;
	unsigned int segno;
	ssize_t len, ret;

	len = 0;
	ret = xdr_stream_encode_item_present(&sctxt->sc_stream);
	if (ret < 0)
		return ret;
	len += ret;

	ret = xdr_stream_encode_u32(&sctxt->sc_stream, chunk->ch_segcount);
	if (ret < 0)
		return ret;
	len += ret;

	for (segno = 0; segno < chunk->ch_segcount; segno++) {
		ret = svc_rdma_encode_write_segment(sctxt, chunk, &remaining, segno);
		if (ret < 0)
			return ret;
		len += ret;
	}

	return len;
}

 
static ssize_t svc_rdma_encode_write_list(struct svc_rdma_recv_ctxt *rctxt,
					  struct svc_rdma_send_ctxt *sctxt)
{
	struct svc_rdma_chunk *chunk;
	ssize_t len, ret;

	len = 0;
	pcl_for_each_chunk(chunk, &rctxt->rc_write_pcl) {
		ret = svc_rdma_encode_write_chunk(sctxt, chunk);
		if (ret < 0)
			return ret;
		len += ret;
	}

	 
	ret = xdr_stream_encode_item_absent(&sctxt->sc_stream);
	if (ret < 0)
		return ret;

	return len + ret;
}

 
static ssize_t
svc_rdma_encode_reply_chunk(struct svc_rdma_recv_ctxt *rctxt,
			    struct svc_rdma_send_ctxt *sctxt,
			    unsigned int length)
{
	struct svc_rdma_chunk *chunk;

	if (pcl_is_empty(&rctxt->rc_reply_pcl))
		return xdr_stream_encode_item_absent(&sctxt->sc_stream);

	chunk = pcl_first_chunk(&rctxt->rc_reply_pcl);
	if (length > chunk->ch_length)
		return -E2BIG;

	chunk->ch_payload_length = length;
	return svc_rdma_encode_write_chunk(sctxt, chunk);
}

struct svc_rdma_map_data {
	struct svcxprt_rdma		*md_rdma;
	struct svc_rdma_send_ctxt	*md_ctxt;
};

 
static int svc_rdma_page_dma_map(void *data, struct page *page,
				 unsigned long offset, unsigned int len)
{
	struct svc_rdma_map_data *args = data;
	struct svcxprt_rdma *rdma = args->md_rdma;
	struct svc_rdma_send_ctxt *ctxt = args->md_ctxt;
	struct ib_device *dev = rdma->sc_cm_id->device;
	dma_addr_t dma_addr;

	++ctxt->sc_cur_sge_no;

	dma_addr = ib_dma_map_page(dev, page, offset, len, DMA_TO_DEVICE);
	if (ib_dma_mapping_error(dev, dma_addr))
		goto out_maperr;

	trace_svcrdma_dma_map_page(rdma, dma_addr, len);
	ctxt->sc_sges[ctxt->sc_cur_sge_no].addr = dma_addr;
	ctxt->sc_sges[ctxt->sc_cur_sge_no].length = len;
	ctxt->sc_send_wr.num_sge++;
	return 0;

out_maperr:
	trace_svcrdma_dma_map_err(rdma, dma_addr, len);
	return -EIO;
}

 
static int svc_rdma_iov_dma_map(void *data, const struct kvec *iov)
{
	if (!iov->iov_len)
		return 0;
	return svc_rdma_page_dma_map(data, virt_to_page(iov->iov_base),
				     offset_in_page(iov->iov_base),
				     iov->iov_len);
}

 
static int svc_rdma_xb_dma_map(const struct xdr_buf *xdr, void *data)
{
	unsigned int len, remaining;
	unsigned long pageoff;
	struct page **ppages;
	int ret;

	ret = svc_rdma_iov_dma_map(data, &xdr->head[0]);
	if (ret < 0)
		return ret;

	ppages = xdr->pages + (xdr->page_base >> PAGE_SHIFT);
	pageoff = offset_in_page(xdr->page_base);
	remaining = xdr->page_len;
	while (remaining) {
		len = min_t(u32, PAGE_SIZE - pageoff, remaining);

		ret = svc_rdma_page_dma_map(data, *ppages++, pageoff, len);
		if (ret < 0)
			return ret;

		remaining -= len;
		pageoff = 0;
	}

	ret = svc_rdma_iov_dma_map(data, &xdr->tail[0]);
	if (ret < 0)
		return ret;

	return xdr->len;
}

struct svc_rdma_pullup_data {
	u8		*pd_dest;
	unsigned int	pd_length;
	unsigned int	pd_num_sges;
};

 
static int svc_rdma_xb_count_sges(const struct xdr_buf *xdr,
				  void *data)
{
	struct svc_rdma_pullup_data *args = data;
	unsigned int remaining;
	unsigned long offset;

	if (xdr->head[0].iov_len)
		++args->pd_num_sges;

	offset = offset_in_page(xdr->page_base);
	remaining = xdr->page_len;
	while (remaining) {
		++args->pd_num_sges;
		remaining -= min_t(u32, PAGE_SIZE - offset, remaining);
		offset = 0;
	}

	if (xdr->tail[0].iov_len)
		++args->pd_num_sges;

	args->pd_length += xdr->len;
	return 0;
}

 
static bool svc_rdma_pull_up_needed(const struct svcxprt_rdma *rdma,
				    const struct svc_rdma_send_ctxt *sctxt,
				    const struct svc_rdma_recv_ctxt *rctxt,
				    const struct xdr_buf *xdr)
{
	 
	struct svc_rdma_pullup_data args = {
		.pd_length	= sctxt->sc_hdrbuf.len,
		.pd_num_sges	= 1,
	};
	int ret;

	ret = pcl_process_nonpayloads(&rctxt->rc_write_pcl, xdr,
				      svc_rdma_xb_count_sges, &args);
	if (ret < 0)
		return false;

	if (args.pd_length < RPCRDMA_PULLUP_THRESH)
		return true;
	return args.pd_num_sges >= rdma->sc_max_send_sges;
}

 
static int svc_rdma_xb_linearize(const struct xdr_buf *xdr,
				 void *data)
{
	struct svc_rdma_pullup_data *args = data;
	unsigned int len, remaining;
	unsigned long pageoff;
	struct page **ppages;

	if (xdr->head[0].iov_len) {
		memcpy(args->pd_dest, xdr->head[0].iov_base, xdr->head[0].iov_len);
		args->pd_dest += xdr->head[0].iov_len;
	}

	ppages = xdr->pages + (xdr->page_base >> PAGE_SHIFT);
	pageoff = offset_in_page(xdr->page_base);
	remaining = xdr->page_len;
	while (remaining) {
		len = min_t(u32, PAGE_SIZE - pageoff, remaining);
		memcpy(args->pd_dest, page_address(*ppages) + pageoff, len);
		remaining -= len;
		args->pd_dest += len;
		pageoff = 0;
		ppages++;
	}

	if (xdr->tail[0].iov_len) {
		memcpy(args->pd_dest, xdr->tail[0].iov_base, xdr->tail[0].iov_len);
		args->pd_dest += xdr->tail[0].iov_len;
	}

	args->pd_length += xdr->len;
	return 0;
}

 
static int svc_rdma_pull_up_reply_msg(const struct svcxprt_rdma *rdma,
				      struct svc_rdma_send_ctxt *sctxt,
				      const struct svc_rdma_recv_ctxt *rctxt,
				      const struct xdr_buf *xdr)
{
	struct svc_rdma_pullup_data args = {
		.pd_dest	= sctxt->sc_xprt_buf + sctxt->sc_hdrbuf.len,
	};
	int ret;

	ret = pcl_process_nonpayloads(&rctxt->rc_write_pcl, xdr,
				      svc_rdma_xb_linearize, &args);
	if (ret < 0)
		return ret;

	sctxt->sc_sges[0].length = sctxt->sc_hdrbuf.len + args.pd_length;
	trace_svcrdma_send_pullup(sctxt, args.pd_length);
	return 0;
}

 
int svc_rdma_map_reply_msg(struct svcxprt_rdma *rdma,
			   struct svc_rdma_send_ctxt *sctxt,
			   const struct svc_rdma_recv_ctxt *rctxt,
			   const struct xdr_buf *xdr)
{
	struct svc_rdma_map_data args = {
		.md_rdma	= rdma,
		.md_ctxt	= sctxt,
	};

	 
	sctxt->sc_send_wr.num_sge = 1;
	sctxt->sc_sges[0].length = sctxt->sc_hdrbuf.len;

	 
	if (!pcl_is_empty(&rctxt->rc_reply_pcl))
		return 0;

	 
	if (svc_rdma_pull_up_needed(rdma, sctxt, rctxt, xdr))
		return svc_rdma_pull_up_reply_msg(rdma, sctxt, rctxt, xdr);

	return pcl_process_nonpayloads(&rctxt->rc_write_pcl, xdr,
				       svc_rdma_xb_dma_map, &args);
}

 
static void svc_rdma_save_io_pages(struct svc_rqst *rqstp,
				   struct svc_rdma_send_ctxt *ctxt)
{
	int i, pages = rqstp->rq_next_page - rqstp->rq_respages;

	ctxt->sc_page_count += pages;
	for (i = 0; i < pages; i++) {
		ctxt->sc_pages[i] = rqstp->rq_respages[i];
		rqstp->rq_respages[i] = NULL;
	}

	 
	rqstp->rq_next_page = rqstp->rq_respages;
}

 
static int svc_rdma_send_reply_msg(struct svcxprt_rdma *rdma,
				   struct svc_rdma_send_ctxt *sctxt,
				   const struct svc_rdma_recv_ctxt *rctxt,
				   struct svc_rqst *rqstp)
{
	int ret;

	ret = svc_rdma_map_reply_msg(rdma, sctxt, rctxt, &rqstp->rq_res);
	if (ret < 0)
		return ret;

	svc_rdma_save_io_pages(rqstp, sctxt);

	if (rctxt->rc_inv_rkey) {
		sctxt->sc_send_wr.opcode = IB_WR_SEND_WITH_INV;
		sctxt->sc_send_wr.ex.invalidate_rkey = rctxt->rc_inv_rkey;
	} else {
		sctxt->sc_send_wr.opcode = IB_WR_SEND;
	}

	return svc_rdma_send(rdma, sctxt);
}

 
void svc_rdma_send_error_msg(struct svcxprt_rdma *rdma,
			     struct svc_rdma_send_ctxt *sctxt,
			     struct svc_rdma_recv_ctxt *rctxt,
			     int status)
{
	__be32 *rdma_argp = rctxt->rc_recv_buf;
	__be32 *p;

	rpcrdma_set_xdrlen(&sctxt->sc_hdrbuf, 0);
	xdr_init_encode(&sctxt->sc_stream, &sctxt->sc_hdrbuf,
			sctxt->sc_xprt_buf, NULL);

	p = xdr_reserve_space(&sctxt->sc_stream,
			      rpcrdma_fixed_maxsz * sizeof(*p));
	if (!p)
		goto put_ctxt;

	*p++ = *rdma_argp;
	*p++ = *(rdma_argp + 1);
	*p++ = rdma->sc_fc_credits;
	*p = rdma_error;

	switch (status) {
	case -EPROTONOSUPPORT:
		p = xdr_reserve_space(&sctxt->sc_stream, 3 * sizeof(*p));
		if (!p)
			goto put_ctxt;

		*p++ = err_vers;
		*p++ = rpcrdma_version;
		*p = rpcrdma_version;
		trace_svcrdma_err_vers(*rdma_argp);
		break;
	default:
		p = xdr_reserve_space(&sctxt->sc_stream, sizeof(*p));
		if (!p)
			goto put_ctxt;

		*p = err_chunk;
		trace_svcrdma_err_chunk(*rdma_argp);
	}

	 
	sctxt->sc_send_wr.num_sge = 1;
	sctxt->sc_send_wr.opcode = IB_WR_SEND;
	sctxt->sc_sges[0].length = sctxt->sc_hdrbuf.len;
	if (svc_rdma_send(rdma, sctxt))
		goto put_ctxt;
	return;

put_ctxt:
	svc_rdma_send_ctxt_put(rdma, sctxt);
}

 
int svc_rdma_sendto(struct svc_rqst *rqstp)
{
	struct svc_xprt *xprt = rqstp->rq_xprt;
	struct svcxprt_rdma *rdma =
		container_of(xprt, struct svcxprt_rdma, sc_xprt);
	struct svc_rdma_recv_ctxt *rctxt = rqstp->rq_xprt_ctxt;
	__be32 *rdma_argp = rctxt->rc_recv_buf;
	struct svc_rdma_send_ctxt *sctxt;
	unsigned int rc_size;
	__be32 *p;
	int ret;

	ret = -ENOTCONN;
	if (svc_xprt_is_dead(xprt))
		goto drop_connection;

	ret = -ENOMEM;
	sctxt = svc_rdma_send_ctxt_get(rdma);
	if (!sctxt)
		goto drop_connection;

	ret = -EMSGSIZE;
	p = xdr_reserve_space(&sctxt->sc_stream,
			      rpcrdma_fixed_maxsz * sizeof(*p));
	if (!p)
		goto put_ctxt;

	ret = svc_rdma_send_reply_chunk(rdma, rctxt, &rqstp->rq_res);
	if (ret < 0)
		goto reply_chunk;
	rc_size = ret;

	*p++ = *rdma_argp;
	*p++ = *(rdma_argp + 1);
	*p++ = rdma->sc_fc_credits;
	*p = pcl_is_empty(&rctxt->rc_reply_pcl) ? rdma_msg : rdma_nomsg;

	ret = svc_rdma_encode_read_list(sctxt);
	if (ret < 0)
		goto put_ctxt;
	ret = svc_rdma_encode_write_list(rctxt, sctxt);
	if (ret < 0)
		goto put_ctxt;
	ret = svc_rdma_encode_reply_chunk(rctxt, sctxt, rc_size);
	if (ret < 0)
		goto put_ctxt;

	ret = svc_rdma_send_reply_msg(rdma, sctxt, rctxt, rqstp);
	if (ret < 0)
		goto put_ctxt;
	return 0;

reply_chunk:
	if (ret != -E2BIG && ret != -EINVAL)
		goto put_ctxt;

	 
	svc_rdma_save_io_pages(rqstp, sctxt);
	svc_rdma_send_error_msg(rdma, sctxt, rctxt, ret);
	return 0;

put_ctxt:
	svc_rdma_send_ctxt_put(rdma, sctxt);
drop_connection:
	trace_svcrdma_send_err(rqstp, ret);
	svc_xprt_deferred_close(&rdma->sc_xprt);
	return -ENOTCONN;
}

 
int svc_rdma_result_payload(struct svc_rqst *rqstp, unsigned int offset,
			    unsigned int length)
{
	struct svc_rdma_recv_ctxt *rctxt = rqstp->rq_xprt_ctxt;
	struct svc_rdma_chunk *chunk;
	struct svcxprt_rdma *rdma;
	struct xdr_buf subbuf;
	int ret;

	chunk = rctxt->rc_cur_result_payload;
	if (!length || !chunk)
		return 0;
	rctxt->rc_cur_result_payload =
		pcl_next_chunk(&rctxt->rc_write_pcl, chunk);
	if (length > chunk->ch_length)
		return -E2BIG;

	chunk->ch_position = offset;
	chunk->ch_payload_length = length;

	if (xdr_buf_subsegment(&rqstp->rq_res, &subbuf, offset, length))
		return -EMSGSIZE;

	rdma = container_of(rqstp->rq_xprt, struct svcxprt_rdma, sc_xprt);
	ret = svc_rdma_send_write_chunk(rdma, chunk, &subbuf);
	if (ret < 0)
		return ret;
	return 0;
}
