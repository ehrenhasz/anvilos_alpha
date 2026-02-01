
 

 

#include <linux/highmem.h>

#include <linux/sunrpc/svc_rdma.h>

#include "xprt_rdma.h"
#include <trace/events/rpcrdma.h>

 
static unsigned int rpcrdma_max_call_header_size(unsigned int maxsegs)
{
	unsigned int size;

	 
	size = RPCRDMA_HDRLEN_MIN;

	 
	size += maxsegs * rpcrdma_readchunk_maxsz * sizeof(__be32);

	 
	size += sizeof(__be32);	 
	size += rpcrdma_segment_maxsz * sizeof(__be32);
	size += sizeof(__be32);	 

	return size;
}

 
static unsigned int rpcrdma_max_reply_header_size(unsigned int maxsegs)
{
	unsigned int size;

	 
	size = RPCRDMA_HDRLEN_MIN;

	 
	size += sizeof(__be32);		 
	size += maxsegs * rpcrdma_segment_maxsz * sizeof(__be32);
	size += sizeof(__be32);	 

	return size;
}

 
void rpcrdma_set_max_header_sizes(struct rpcrdma_ep *ep)
{
	unsigned int maxsegs = ep->re_max_rdma_segs;

	ep->re_max_inline_send =
		ep->re_inline_send - rpcrdma_max_call_header_size(maxsegs);
	ep->re_max_inline_recv =
		ep->re_inline_recv - rpcrdma_max_reply_header_size(maxsegs);
}

 
static bool rpcrdma_args_inline(struct rpcrdma_xprt *r_xprt,
				struct rpc_rqst *rqst)
{
	struct xdr_buf *xdr = &rqst->rq_snd_buf;
	struct rpcrdma_ep *ep = r_xprt->rx_ep;
	unsigned int count, remaining, offset;

	if (xdr->len > ep->re_max_inline_send)
		return false;

	if (xdr->page_len) {
		remaining = xdr->page_len;
		offset = offset_in_page(xdr->page_base);
		count = RPCRDMA_MIN_SEND_SGES;
		while (remaining) {
			remaining -= min_t(unsigned int,
					   PAGE_SIZE - offset, remaining);
			offset = 0;
			if (++count > ep->re_attr.cap.max_send_sge)
				return false;
		}
	}

	return true;
}

 
static bool rpcrdma_results_inline(struct rpcrdma_xprt *r_xprt,
				   struct rpc_rqst *rqst)
{
	return rqst->rq_rcv_buf.buflen <= r_xprt->rx_ep->re_max_inline_recv;
}

 
static bool
rpcrdma_nonpayload_inline(const struct rpcrdma_xprt *r_xprt,
			  const struct rpc_rqst *rqst)
{
	const struct xdr_buf *buf = &rqst->rq_rcv_buf;

	return (buf->head[0].iov_len + buf->tail[0].iov_len) <
		r_xprt->rx_ep->re_max_inline_recv;
}

 
static noinline int
rpcrdma_alloc_sparse_pages(struct xdr_buf *buf)
{
	struct page **ppages;
	int len;

	len = buf->page_len;
	ppages = buf->pages + (buf->page_base >> PAGE_SHIFT);
	while (len > 0) {
		if (!*ppages)
			*ppages = alloc_page(GFP_NOWAIT | __GFP_NOWARN);
		if (!*ppages)
			return -ENOBUFS;
		ppages++;
		len -= PAGE_SIZE;
	}

	return 0;
}

 
static struct rpcrdma_mr_seg *
rpcrdma_convert_kvec(struct kvec *vec, struct rpcrdma_mr_seg *seg,
		     unsigned int *n)
{
	seg->mr_page = virt_to_page(vec->iov_base);
	seg->mr_offset = offset_in_page(vec->iov_base);
	seg->mr_len = vec->iov_len;
	++seg;
	++(*n);
	return seg;
}

 

static int
rpcrdma_convert_iovs(struct rpcrdma_xprt *r_xprt, struct xdr_buf *xdrbuf,
		     unsigned int pos, enum rpcrdma_chunktype type,
		     struct rpcrdma_mr_seg *seg)
{
	unsigned long page_base;
	unsigned int len, n;
	struct page **ppages;

	n = 0;
	if (pos == 0)
		seg = rpcrdma_convert_kvec(&xdrbuf->head[0], seg, &n);

	len = xdrbuf->page_len;
	ppages = xdrbuf->pages + (xdrbuf->page_base >> PAGE_SHIFT);
	page_base = offset_in_page(xdrbuf->page_base);
	while (len) {
		seg->mr_page = *ppages;
		seg->mr_offset = page_base;
		seg->mr_len = min_t(u32, PAGE_SIZE - page_base, len);
		len -= seg->mr_len;
		++ppages;
		++seg;
		++n;
		page_base = 0;
	}

	if (type == rpcrdma_readch || type == rpcrdma_writech)
		goto out;

	if (xdrbuf->tail[0].iov_len)
		rpcrdma_convert_kvec(&xdrbuf->tail[0], seg, &n);

out:
	if (unlikely(n > RPCRDMA_MAX_SEGS))
		return -EIO;
	return n;
}

static int
encode_rdma_segment(struct xdr_stream *xdr, struct rpcrdma_mr *mr)
{
	__be32 *p;

	p = xdr_reserve_space(xdr, 4 * sizeof(*p));
	if (unlikely(!p))
		return -EMSGSIZE;

	xdr_encode_rdma_segment(p, mr->mr_handle, mr->mr_length, mr->mr_offset);
	return 0;
}

static int
encode_read_segment(struct xdr_stream *xdr, struct rpcrdma_mr *mr,
		    u32 position)
{
	__be32 *p;

	p = xdr_reserve_space(xdr, 6 * sizeof(*p));
	if (unlikely(!p))
		return -EMSGSIZE;

	*p++ = xdr_one;			 
	xdr_encode_read_segment(p, position, mr->mr_handle, mr->mr_length,
				mr->mr_offset);
	return 0;
}

static struct rpcrdma_mr_seg *rpcrdma_mr_prepare(struct rpcrdma_xprt *r_xprt,
						 struct rpcrdma_req *req,
						 struct rpcrdma_mr_seg *seg,
						 int nsegs, bool writing,
						 struct rpcrdma_mr **mr)
{
	*mr = rpcrdma_mr_pop(&req->rl_free_mrs);
	if (!*mr) {
		*mr = rpcrdma_mr_get(r_xprt);
		if (!*mr)
			goto out_getmr_err;
		(*mr)->mr_req = req;
	}

	rpcrdma_mr_push(*mr, &req->rl_registered);
	return frwr_map(r_xprt, seg, nsegs, writing, req->rl_slot.rq_xid, *mr);

out_getmr_err:
	trace_xprtrdma_nomrs_err(r_xprt, req);
	xprt_wait_for_buffer_space(&r_xprt->rx_xprt);
	rpcrdma_mrs_refresh(r_xprt);
	return ERR_PTR(-EAGAIN);
}

 
static int rpcrdma_encode_read_list(struct rpcrdma_xprt *r_xprt,
				    struct rpcrdma_req *req,
				    struct rpc_rqst *rqst,
				    enum rpcrdma_chunktype rtype)
{
	struct xdr_stream *xdr = &req->rl_stream;
	struct rpcrdma_mr_seg *seg;
	struct rpcrdma_mr *mr;
	unsigned int pos;
	int nsegs;

	if (rtype == rpcrdma_noch_pullup || rtype == rpcrdma_noch_mapped)
		goto done;

	pos = rqst->rq_snd_buf.head[0].iov_len;
	if (rtype == rpcrdma_areadch)
		pos = 0;
	seg = req->rl_segments;
	nsegs = rpcrdma_convert_iovs(r_xprt, &rqst->rq_snd_buf, pos,
				     rtype, seg);
	if (nsegs < 0)
		return nsegs;

	do {
		seg = rpcrdma_mr_prepare(r_xprt, req, seg, nsegs, false, &mr);
		if (IS_ERR(seg))
			return PTR_ERR(seg);

		if (encode_read_segment(xdr, mr, pos) < 0)
			return -EMSGSIZE;

		trace_xprtrdma_chunk_read(rqst->rq_task, pos, mr, nsegs);
		r_xprt->rx_stats.read_chunk_count++;
		nsegs -= mr->mr_nents;
	} while (nsegs);

done:
	if (xdr_stream_encode_item_absent(xdr) < 0)
		return -EMSGSIZE;
	return 0;
}

 
static int rpcrdma_encode_write_list(struct rpcrdma_xprt *r_xprt,
				     struct rpcrdma_req *req,
				     struct rpc_rqst *rqst,
				     enum rpcrdma_chunktype wtype)
{
	struct xdr_stream *xdr = &req->rl_stream;
	struct rpcrdma_ep *ep = r_xprt->rx_ep;
	struct rpcrdma_mr_seg *seg;
	struct rpcrdma_mr *mr;
	int nsegs, nchunks;
	__be32 *segcount;

	if (wtype != rpcrdma_writech)
		goto done;

	seg = req->rl_segments;
	nsegs = rpcrdma_convert_iovs(r_xprt, &rqst->rq_rcv_buf,
				     rqst->rq_rcv_buf.head[0].iov_len,
				     wtype, seg);
	if (nsegs < 0)
		return nsegs;

	if (xdr_stream_encode_item_present(xdr) < 0)
		return -EMSGSIZE;
	segcount = xdr_reserve_space(xdr, sizeof(*segcount));
	if (unlikely(!segcount))
		return -EMSGSIZE;
	 

	nchunks = 0;
	do {
		seg = rpcrdma_mr_prepare(r_xprt, req, seg, nsegs, true, &mr);
		if (IS_ERR(seg))
			return PTR_ERR(seg);

		if (encode_rdma_segment(xdr, mr) < 0)
			return -EMSGSIZE;

		trace_xprtrdma_chunk_write(rqst->rq_task, mr, nsegs);
		r_xprt->rx_stats.write_chunk_count++;
		r_xprt->rx_stats.total_rdma_request += mr->mr_length;
		nchunks++;
		nsegs -= mr->mr_nents;
	} while (nsegs);

	if (xdr_pad_size(rqst->rq_rcv_buf.page_len)) {
		if (encode_rdma_segment(xdr, ep->re_write_pad_mr) < 0)
			return -EMSGSIZE;

		trace_xprtrdma_chunk_wp(rqst->rq_task, ep->re_write_pad_mr,
					nsegs);
		r_xprt->rx_stats.write_chunk_count++;
		r_xprt->rx_stats.total_rdma_request += mr->mr_length;
		nchunks++;
		nsegs -= mr->mr_nents;
	}

	 
	*segcount = cpu_to_be32(nchunks);

done:
	if (xdr_stream_encode_item_absent(xdr) < 0)
		return -EMSGSIZE;
	return 0;
}

 
static int rpcrdma_encode_reply_chunk(struct rpcrdma_xprt *r_xprt,
				      struct rpcrdma_req *req,
				      struct rpc_rqst *rqst,
				      enum rpcrdma_chunktype wtype)
{
	struct xdr_stream *xdr = &req->rl_stream;
	struct rpcrdma_mr_seg *seg;
	struct rpcrdma_mr *mr;
	int nsegs, nchunks;
	__be32 *segcount;

	if (wtype != rpcrdma_replych) {
		if (xdr_stream_encode_item_absent(xdr) < 0)
			return -EMSGSIZE;
		return 0;
	}

	seg = req->rl_segments;
	nsegs = rpcrdma_convert_iovs(r_xprt, &rqst->rq_rcv_buf, 0, wtype, seg);
	if (nsegs < 0)
		return nsegs;

	if (xdr_stream_encode_item_present(xdr) < 0)
		return -EMSGSIZE;
	segcount = xdr_reserve_space(xdr, sizeof(*segcount));
	if (unlikely(!segcount))
		return -EMSGSIZE;
	 

	nchunks = 0;
	do {
		seg = rpcrdma_mr_prepare(r_xprt, req, seg, nsegs, true, &mr);
		if (IS_ERR(seg))
			return PTR_ERR(seg);

		if (encode_rdma_segment(xdr, mr) < 0)
			return -EMSGSIZE;

		trace_xprtrdma_chunk_reply(rqst->rq_task, mr, nsegs);
		r_xprt->rx_stats.reply_chunk_count++;
		r_xprt->rx_stats.total_rdma_request += mr->mr_length;
		nchunks++;
		nsegs -= mr->mr_nents;
	} while (nsegs);

	 
	*segcount = cpu_to_be32(nchunks);

	return 0;
}

static void rpcrdma_sendctx_done(struct kref *kref)
{
	struct rpcrdma_req *req =
		container_of(kref, struct rpcrdma_req, rl_kref);
	struct rpcrdma_rep *rep = req->rl_reply;

	rpcrdma_complete_rqst(rep);
	rep->rr_rxprt->rx_stats.reply_waits_for_send++;
}

 
void rpcrdma_sendctx_unmap(struct rpcrdma_sendctx *sc)
{
	struct rpcrdma_regbuf *rb = sc->sc_req->rl_sendbuf;
	struct ib_sge *sge;

	if (!sc->sc_unmap_count)
		return;

	 
	for (sge = &sc->sc_sges[2]; sc->sc_unmap_count;
	     ++sge, --sc->sc_unmap_count)
		ib_dma_unmap_page(rdmab_device(rb), sge->addr, sge->length,
				  DMA_TO_DEVICE);

	kref_put(&sc->sc_req->rl_kref, rpcrdma_sendctx_done);
}

 
static void rpcrdma_prepare_hdr_sge(struct rpcrdma_xprt *r_xprt,
				    struct rpcrdma_req *req, u32 len)
{
	struct rpcrdma_sendctx *sc = req->rl_sendctx;
	struct rpcrdma_regbuf *rb = req->rl_rdmabuf;
	struct ib_sge *sge = &sc->sc_sges[req->rl_wr.num_sge++];

	sge->addr = rdmab_addr(rb);
	sge->length = len;
	sge->lkey = rdmab_lkey(rb);

	ib_dma_sync_single_for_device(rdmab_device(rb), sge->addr, sge->length,
				      DMA_TO_DEVICE);
}

 
static bool rpcrdma_prepare_head_iov(struct rpcrdma_xprt *r_xprt,
				     struct rpcrdma_req *req, unsigned int len)
{
	struct rpcrdma_sendctx *sc = req->rl_sendctx;
	struct ib_sge *sge = &sc->sc_sges[req->rl_wr.num_sge++];
	struct rpcrdma_regbuf *rb = req->rl_sendbuf;

	if (!rpcrdma_regbuf_dma_map(r_xprt, rb))
		return false;

	sge->addr = rdmab_addr(rb);
	sge->length = len;
	sge->lkey = rdmab_lkey(rb);

	ib_dma_sync_single_for_device(rdmab_device(rb), sge->addr, sge->length,
				      DMA_TO_DEVICE);
	return true;
}

 
static bool rpcrdma_prepare_pagelist(struct rpcrdma_req *req,
				     struct xdr_buf *xdr)
{
	struct rpcrdma_sendctx *sc = req->rl_sendctx;
	struct rpcrdma_regbuf *rb = req->rl_sendbuf;
	unsigned int page_base, len, remaining;
	struct page **ppages;
	struct ib_sge *sge;

	ppages = xdr->pages + (xdr->page_base >> PAGE_SHIFT);
	page_base = offset_in_page(xdr->page_base);
	remaining = xdr->page_len;
	while (remaining) {
		sge = &sc->sc_sges[req->rl_wr.num_sge++];
		len = min_t(unsigned int, PAGE_SIZE - page_base, remaining);
		sge->addr = ib_dma_map_page(rdmab_device(rb), *ppages,
					    page_base, len, DMA_TO_DEVICE);
		if (ib_dma_mapping_error(rdmab_device(rb), sge->addr))
			goto out_mapping_err;

		sge->length = len;
		sge->lkey = rdmab_lkey(rb);

		sc->sc_unmap_count++;
		ppages++;
		remaining -= len;
		page_base = 0;
	}

	return true;

out_mapping_err:
	trace_xprtrdma_dma_maperr(sge->addr);
	return false;
}

 
static bool rpcrdma_prepare_tail_iov(struct rpcrdma_req *req,
				     struct xdr_buf *xdr,
				     unsigned int page_base, unsigned int len)
{
	struct rpcrdma_sendctx *sc = req->rl_sendctx;
	struct ib_sge *sge = &sc->sc_sges[req->rl_wr.num_sge++];
	struct rpcrdma_regbuf *rb = req->rl_sendbuf;
	struct page *page = virt_to_page(xdr->tail[0].iov_base);

	sge->addr = ib_dma_map_page(rdmab_device(rb), page, page_base, len,
				    DMA_TO_DEVICE);
	if (ib_dma_mapping_error(rdmab_device(rb), sge->addr))
		goto out_mapping_err;

	sge->length = len;
	sge->lkey = rdmab_lkey(rb);
	++sc->sc_unmap_count;
	return true;

out_mapping_err:
	trace_xprtrdma_dma_maperr(sge->addr);
	return false;
}

 
static void rpcrdma_pullup_tail_iov(struct rpcrdma_xprt *r_xprt,
				    struct rpcrdma_req *req,
				    struct xdr_buf *xdr)
{
	unsigned char *dst;

	dst = (unsigned char *)xdr->head[0].iov_base;
	dst += xdr->head[0].iov_len + xdr->page_len;
	memmove(dst, xdr->tail[0].iov_base, xdr->tail[0].iov_len);
	r_xprt->rx_stats.pullup_copy_count += xdr->tail[0].iov_len;
}

 
static void rpcrdma_pullup_pagelist(struct rpcrdma_xprt *r_xprt,
				    struct rpcrdma_req *req,
				    struct xdr_buf *xdr)
{
	unsigned int len, page_base, remaining;
	struct page **ppages;
	unsigned char *src, *dst;

	dst = (unsigned char *)xdr->head[0].iov_base;
	dst += xdr->head[0].iov_len;
	ppages = xdr->pages + (xdr->page_base >> PAGE_SHIFT);
	page_base = offset_in_page(xdr->page_base);
	remaining = xdr->page_len;
	while (remaining) {
		src = page_address(*ppages);
		src += page_base;
		len = min_t(unsigned int, PAGE_SIZE - page_base, remaining);
		memcpy(dst, src, len);
		r_xprt->rx_stats.pullup_copy_count += len;

		ppages++;
		dst += len;
		remaining -= len;
		page_base = 0;
	}
}

 
static bool rpcrdma_prepare_noch_pullup(struct rpcrdma_xprt *r_xprt,
					struct rpcrdma_req *req,
					struct xdr_buf *xdr)
{
	if (unlikely(xdr->tail[0].iov_len))
		rpcrdma_pullup_tail_iov(r_xprt, req, xdr);

	if (unlikely(xdr->page_len))
		rpcrdma_pullup_pagelist(r_xprt, req, xdr);

	 
	return rpcrdma_prepare_head_iov(r_xprt, req, xdr->len);
}

static bool rpcrdma_prepare_noch_mapped(struct rpcrdma_xprt *r_xprt,
					struct rpcrdma_req *req,
					struct xdr_buf *xdr)
{
	struct kvec *tail = &xdr->tail[0];

	if (!rpcrdma_prepare_head_iov(r_xprt, req, xdr->head[0].iov_len))
		return false;
	if (xdr->page_len)
		if (!rpcrdma_prepare_pagelist(req, xdr))
			return false;
	if (tail->iov_len)
		if (!rpcrdma_prepare_tail_iov(req, xdr,
					      offset_in_page(tail->iov_base),
					      tail->iov_len))
			return false;

	if (req->rl_sendctx->sc_unmap_count)
		kref_get(&req->rl_kref);
	return true;
}

static bool rpcrdma_prepare_readch(struct rpcrdma_xprt *r_xprt,
				   struct rpcrdma_req *req,
				   struct xdr_buf *xdr)
{
	if (!rpcrdma_prepare_head_iov(r_xprt, req, xdr->head[0].iov_len))
		return false;

	 

	 
	if (xdr->tail[0].iov_len > 3) {
		unsigned int page_base, len;

		 
		page_base = offset_in_page(xdr->tail[0].iov_base);
		len = xdr->tail[0].iov_len;
		page_base += len & 3;
		len -= len & 3;
		if (!rpcrdma_prepare_tail_iov(req, xdr, page_base, len))
			return false;
		kref_get(&req->rl_kref);
	}

	return true;
}

 
inline int rpcrdma_prepare_send_sges(struct rpcrdma_xprt *r_xprt,
				     struct rpcrdma_req *req, u32 hdrlen,
				     struct xdr_buf *xdr,
				     enum rpcrdma_chunktype rtype)
{
	int ret;

	ret = -EAGAIN;
	req->rl_sendctx = rpcrdma_sendctx_get_locked(r_xprt);
	if (!req->rl_sendctx)
		goto out_nosc;
	req->rl_sendctx->sc_unmap_count = 0;
	req->rl_sendctx->sc_req = req;
	kref_init(&req->rl_kref);
	req->rl_wr.wr_cqe = &req->rl_sendctx->sc_cqe;
	req->rl_wr.sg_list = req->rl_sendctx->sc_sges;
	req->rl_wr.num_sge = 0;
	req->rl_wr.opcode = IB_WR_SEND;

	rpcrdma_prepare_hdr_sge(r_xprt, req, hdrlen);

	ret = -EIO;
	switch (rtype) {
	case rpcrdma_noch_pullup:
		if (!rpcrdma_prepare_noch_pullup(r_xprt, req, xdr))
			goto out_unmap;
		break;
	case rpcrdma_noch_mapped:
		if (!rpcrdma_prepare_noch_mapped(r_xprt, req, xdr))
			goto out_unmap;
		break;
	case rpcrdma_readch:
		if (!rpcrdma_prepare_readch(r_xprt, req, xdr))
			goto out_unmap;
		break;
	case rpcrdma_areadch:
		break;
	default:
		goto out_unmap;
	}

	return 0;

out_unmap:
	rpcrdma_sendctx_unmap(req->rl_sendctx);
out_nosc:
	trace_xprtrdma_prepsend_failed(&req->rl_slot, ret);
	return ret;
}

 
int
rpcrdma_marshal_req(struct rpcrdma_xprt *r_xprt, struct rpc_rqst *rqst)
{
	struct rpcrdma_req *req = rpcr_to_rdmar(rqst);
	struct xdr_stream *xdr = &req->rl_stream;
	enum rpcrdma_chunktype rtype, wtype;
	struct xdr_buf *buf = &rqst->rq_snd_buf;
	bool ddp_allowed;
	__be32 *p;
	int ret;

	if (unlikely(rqst->rq_rcv_buf.flags & XDRBUF_SPARSE_PAGES)) {
		ret = rpcrdma_alloc_sparse_pages(&rqst->rq_rcv_buf);
		if (ret)
			return ret;
	}

	rpcrdma_set_xdrlen(&req->rl_hdrbuf, 0);
	xdr_init_encode(xdr, &req->rl_hdrbuf, rdmab_data(req->rl_rdmabuf),
			rqst);

	 
	ret = -EMSGSIZE;
	p = xdr_reserve_space(xdr, 4 * sizeof(*p));
	if (!p)
		goto out_err;
	*p++ = rqst->rq_xid;
	*p++ = rpcrdma_version;
	*p++ = r_xprt->rx_buf.rb_max_requests;

	 
	ddp_allowed = !test_bit(RPCAUTH_AUTH_DATATOUCH,
				&rqst->rq_cred->cr_auth->au_flags);

	 
	if (rpcrdma_results_inline(r_xprt, rqst))
		wtype = rpcrdma_noch;
	else if ((ddp_allowed && rqst->rq_rcv_buf.flags & XDRBUF_READ) &&
		 rpcrdma_nonpayload_inline(r_xprt, rqst))
		wtype = rpcrdma_writech;
	else
		wtype = rpcrdma_replych;

	 
	if (rpcrdma_args_inline(r_xprt, rqst)) {
		*p++ = rdma_msg;
		rtype = buf->len < rdmab_length(req->rl_sendbuf) ?
			rpcrdma_noch_pullup : rpcrdma_noch_mapped;
	} else if (ddp_allowed && buf->flags & XDRBUF_WRITE) {
		*p++ = rdma_msg;
		rtype = rpcrdma_readch;
	} else {
		r_xprt->rx_stats.nomsg_call_count++;
		*p++ = rdma_nomsg;
		rtype = rpcrdma_areadch;
	}

	 
	ret = rpcrdma_encode_read_list(r_xprt, req, rqst, rtype);
	if (ret)
		goto out_err;
	ret = rpcrdma_encode_write_list(r_xprt, req, rqst, wtype);
	if (ret)
		goto out_err;
	ret = rpcrdma_encode_reply_chunk(r_xprt, req, rqst, wtype);
	if (ret)
		goto out_err;

	ret = rpcrdma_prepare_send_sges(r_xprt, req, req->rl_hdrbuf.len,
					buf, rtype);
	if (ret)
		goto out_err;

	trace_xprtrdma_marshal(req, rtype, wtype);
	return 0;

out_err:
	trace_xprtrdma_marshal_failed(rqst, ret);
	r_xprt->rx_stats.failed_marshal_count++;
	frwr_reset(req);
	return ret;
}

static void __rpcrdma_update_cwnd_locked(struct rpc_xprt *xprt,
					 struct rpcrdma_buffer *buf,
					 u32 grant)
{
	buf->rb_credits = grant;
	xprt->cwnd = grant << RPC_CWNDSHIFT;
}

static void rpcrdma_update_cwnd(struct rpcrdma_xprt *r_xprt, u32 grant)
{
	struct rpc_xprt *xprt = &r_xprt->rx_xprt;

	spin_lock(&xprt->transport_lock);
	__rpcrdma_update_cwnd_locked(xprt, &r_xprt->rx_buf, grant);
	spin_unlock(&xprt->transport_lock);
}

 
void rpcrdma_reset_cwnd(struct rpcrdma_xprt *r_xprt)
{
	struct rpc_xprt *xprt = &r_xprt->rx_xprt;

	spin_lock(&xprt->transport_lock);
	xprt->cong = 0;
	__rpcrdma_update_cwnd_locked(xprt, &r_xprt->rx_buf, 1);
	spin_unlock(&xprt->transport_lock);
}

 
static unsigned long
rpcrdma_inline_fixup(struct rpc_rqst *rqst, char *srcp, int copy_len, int pad)
{
	unsigned long fixup_copy_count;
	int i, npages, curlen;
	char *destp;
	struct page **ppages;
	int page_base;

	 
	rqst->rq_rcv_buf.head[0].iov_base = srcp;
	rqst->rq_private_buf.head[0].iov_base = srcp;

	 
	curlen = rqst->rq_rcv_buf.head[0].iov_len;
	if (curlen > copy_len)
		curlen = copy_len;
	srcp += curlen;
	copy_len -= curlen;

	ppages = rqst->rq_rcv_buf.pages +
		(rqst->rq_rcv_buf.page_base >> PAGE_SHIFT);
	page_base = offset_in_page(rqst->rq_rcv_buf.page_base);
	fixup_copy_count = 0;
	if (copy_len && rqst->rq_rcv_buf.page_len) {
		int pagelist_len;

		pagelist_len = rqst->rq_rcv_buf.page_len;
		if (pagelist_len > copy_len)
			pagelist_len = copy_len;
		npages = PAGE_ALIGN(page_base + pagelist_len) >> PAGE_SHIFT;
		for (i = 0; i < npages; i++) {
			curlen = PAGE_SIZE - page_base;
			if (curlen > pagelist_len)
				curlen = pagelist_len;

			destp = kmap_atomic(ppages[i]);
			memcpy(destp + page_base, srcp, curlen);
			flush_dcache_page(ppages[i]);
			kunmap_atomic(destp);
			srcp += curlen;
			copy_len -= curlen;
			fixup_copy_count += curlen;
			pagelist_len -= curlen;
			if (!pagelist_len)
				break;
			page_base = 0;
		}

		 
		if (pad)
			srcp -= pad;
	}

	 
	if (copy_len || pad) {
		rqst->rq_rcv_buf.tail[0].iov_base = srcp;
		rqst->rq_private_buf.tail[0].iov_base = srcp;
	}

	if (fixup_copy_count)
		trace_xprtrdma_fixup(rqst, fixup_copy_count);
	return fixup_copy_count;
}

 
static bool
rpcrdma_is_bcall(struct rpcrdma_xprt *r_xprt, struct rpcrdma_rep *rep)
#if defined(CONFIG_SUNRPC_BACKCHANNEL)
{
	struct rpc_xprt *xprt = &r_xprt->rx_xprt;
	struct xdr_stream *xdr = &rep->rr_stream;
	__be32 *p;

	if (rep->rr_proc != rdma_msg)
		return false;

	 
	p = xdr_inline_decode(xdr, 0);

	 
	if (xdr_item_is_present(p++))
		return false;
	if (xdr_item_is_present(p++))
		return false;
	if (xdr_item_is_present(p++))
		return false;

	 
	if (*p++ != rep->rr_xid)
		return false;
	if (*p != cpu_to_be32(RPC_CALL))
		return false;

	 
	if (xprt->bc_serv == NULL)
		return false;

	 
	p = xdr_inline_decode(xdr, 3 * sizeof(*p));
	if (unlikely(!p))
		return true;

	rpcrdma_bc_receive_call(r_xprt, rep);
	return true;
}
#else	 
{
	return false;
}
#endif	 

static int decode_rdma_segment(struct xdr_stream *xdr, u32 *length)
{
	u32 handle;
	u64 offset;
	__be32 *p;

	p = xdr_inline_decode(xdr, 4 * sizeof(*p));
	if (unlikely(!p))
		return -EIO;

	xdr_decode_rdma_segment(p, &handle, length, &offset);
	trace_xprtrdma_decode_seg(handle, *length, offset);
	return 0;
}

static int decode_write_chunk(struct xdr_stream *xdr, u32 *length)
{
	u32 segcount, seglength;
	__be32 *p;

	p = xdr_inline_decode(xdr, sizeof(*p));
	if (unlikely(!p))
		return -EIO;

	*length = 0;
	segcount = be32_to_cpup(p);
	while (segcount--) {
		if (decode_rdma_segment(xdr, &seglength))
			return -EIO;
		*length += seglength;
	}

	return 0;
}

 
static int decode_read_list(struct xdr_stream *xdr)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, sizeof(*p));
	if (unlikely(!p))
		return -EIO;
	if (unlikely(xdr_item_is_present(p)))
		return -EIO;
	return 0;
}

 
static int decode_write_list(struct xdr_stream *xdr, u32 *length)
{
	u32 chunklen;
	bool first;
	__be32 *p;

	*length = 0;
	first = true;
	do {
		p = xdr_inline_decode(xdr, sizeof(*p));
		if (unlikely(!p))
			return -EIO;
		if (xdr_item_is_absent(p))
			break;
		if (!first)
			return -EIO;

		if (decode_write_chunk(xdr, &chunklen))
			return -EIO;
		*length += chunklen;
		first = false;
	} while (true);
	return 0;
}

static int decode_reply_chunk(struct xdr_stream *xdr, u32 *length)
{
	__be32 *p;

	p = xdr_inline_decode(xdr, sizeof(*p));
	if (unlikely(!p))
		return -EIO;

	*length = 0;
	if (xdr_item_is_present(p))
		if (decode_write_chunk(xdr, length))
			return -EIO;
	return 0;
}

static int
rpcrdma_decode_msg(struct rpcrdma_xprt *r_xprt, struct rpcrdma_rep *rep,
		   struct rpc_rqst *rqst)
{
	struct xdr_stream *xdr = &rep->rr_stream;
	u32 writelist, replychunk, rpclen;
	char *base;

	 
	if (decode_read_list(xdr))
		return -EIO;
	if (decode_write_list(xdr, &writelist))
		return -EIO;
	if (decode_reply_chunk(xdr, &replychunk))
		return -EIO;

	 
	if (unlikely(replychunk))
		return -EIO;

	 
	base = (char *)xdr_inline_decode(xdr, 0);
	rpclen = xdr_stream_remaining(xdr);
	r_xprt->rx_stats.fixup_copy_count +=
		rpcrdma_inline_fixup(rqst, base, rpclen, writelist & 3);

	r_xprt->rx_stats.total_rdma_reply += writelist;
	return rpclen + xdr_align_size(writelist);
}

static noinline int
rpcrdma_decode_nomsg(struct rpcrdma_xprt *r_xprt, struct rpcrdma_rep *rep)
{
	struct xdr_stream *xdr = &rep->rr_stream;
	u32 writelist, replychunk;

	 
	if (decode_read_list(xdr))
		return -EIO;
	if (decode_write_list(xdr, &writelist))
		return -EIO;
	if (decode_reply_chunk(xdr, &replychunk))
		return -EIO;

	 
	if (unlikely(writelist))
		return -EIO;
	if (unlikely(!replychunk))
		return -EIO;

	 
	r_xprt->rx_stats.total_rdma_reply += replychunk;
	return replychunk;
}

static noinline int
rpcrdma_decode_error(struct rpcrdma_xprt *r_xprt, struct rpcrdma_rep *rep,
		     struct rpc_rqst *rqst)
{
	struct xdr_stream *xdr = &rep->rr_stream;
	__be32 *p;

	p = xdr_inline_decode(xdr, sizeof(*p));
	if (unlikely(!p))
		return -EIO;

	switch (*p) {
	case err_vers:
		p = xdr_inline_decode(xdr, 2 * sizeof(*p));
		if (!p)
			break;
		trace_xprtrdma_err_vers(rqst, p, p + 1);
		break;
	case err_chunk:
		trace_xprtrdma_err_chunk(rqst);
		break;
	default:
		trace_xprtrdma_err_unrecognized(rqst, p);
	}

	return -EIO;
}

 
void rpcrdma_unpin_rqst(struct rpcrdma_rep *rep)
{
	struct rpc_xprt *xprt = &rep->rr_rxprt->rx_xprt;
	struct rpc_rqst *rqst = rep->rr_rqst;
	struct rpcrdma_req *req = rpcr_to_rdmar(rqst);

	req->rl_reply = NULL;
	rep->rr_rqst = NULL;

	spin_lock(&xprt->queue_lock);
	xprt_unpin_rqst(rqst);
	spin_unlock(&xprt->queue_lock);
}

 
void rpcrdma_complete_rqst(struct rpcrdma_rep *rep)
{
	struct rpcrdma_xprt *r_xprt = rep->rr_rxprt;
	struct rpc_xprt *xprt = &r_xprt->rx_xprt;
	struct rpc_rqst *rqst = rep->rr_rqst;
	int status;

	switch (rep->rr_proc) {
	case rdma_msg:
		status = rpcrdma_decode_msg(r_xprt, rep, rqst);
		break;
	case rdma_nomsg:
		status = rpcrdma_decode_nomsg(r_xprt, rep);
		break;
	case rdma_error:
		status = rpcrdma_decode_error(r_xprt, rep, rqst);
		break;
	default:
		status = -EIO;
	}
	if (status < 0)
		goto out_badheader;

out:
	spin_lock(&xprt->queue_lock);
	xprt_complete_rqst(rqst->rq_task, status);
	xprt_unpin_rqst(rqst);
	spin_unlock(&xprt->queue_lock);
	return;

out_badheader:
	trace_xprtrdma_reply_hdr_err(rep);
	r_xprt->rx_stats.bad_reply_count++;
	rqst->rq_task->tk_status = status;
	status = 0;
	goto out;
}

static void rpcrdma_reply_done(struct kref *kref)
{
	struct rpcrdma_req *req =
		container_of(kref, struct rpcrdma_req, rl_kref);

	rpcrdma_complete_rqst(req->rl_reply);
}

 
void rpcrdma_reply_handler(struct rpcrdma_rep *rep)
{
	struct rpcrdma_xprt *r_xprt = rep->rr_rxprt;
	struct rpc_xprt *xprt = &r_xprt->rx_xprt;
	struct rpcrdma_buffer *buf = &r_xprt->rx_buf;
	struct rpcrdma_req *req;
	struct rpc_rqst *rqst;
	u32 credits;
	__be32 *p;

	 
	if (xprt->reestablish_timeout)
		xprt->reestablish_timeout = 0;

	 
	xdr_init_decode(&rep->rr_stream, &rep->rr_hdrbuf,
			rep->rr_hdrbuf.head[0].iov_base, NULL);
	p = xdr_inline_decode(&rep->rr_stream, 4 * sizeof(*p));
	if (unlikely(!p))
		goto out_shortreply;
	rep->rr_xid = *p++;
	rep->rr_vers = *p++;
	credits = be32_to_cpu(*p++);
	rep->rr_proc = *p++;

	if (rep->rr_vers != rpcrdma_version)
		goto out_badversion;

	if (rpcrdma_is_bcall(r_xprt, rep))
		return;

	 
	spin_lock(&xprt->queue_lock);
	rqst = xprt_lookup_rqst(xprt, rep->rr_xid);
	if (!rqst)
		goto out_norqst;
	xprt_pin_rqst(rqst);
	spin_unlock(&xprt->queue_lock);

	if (credits == 0)
		credits = 1;	 
	else if (credits > r_xprt->rx_ep->re_max_requests)
		credits = r_xprt->rx_ep->re_max_requests;
	rpcrdma_post_recvs(r_xprt, credits + (buf->rb_bc_srv_max_requests << 1),
			   false);
	if (buf->rb_credits != credits)
		rpcrdma_update_cwnd(r_xprt, credits);

	req = rpcr_to_rdmar(rqst);
	if (unlikely(req->rl_reply))
		rpcrdma_rep_put(buf, req->rl_reply);
	req->rl_reply = rep;
	rep->rr_rqst = rqst;

	trace_xprtrdma_reply(rqst->rq_task, rep, credits);

	if (rep->rr_wc_flags & IB_WC_WITH_INVALIDATE)
		frwr_reminv(rep, &req->rl_registered);
	if (!list_empty(&req->rl_registered))
		frwr_unmap_async(r_xprt, req);
		 
	else
		kref_put(&req->rl_kref, rpcrdma_reply_done);
	return;

out_badversion:
	trace_xprtrdma_reply_vers_err(rep);
	goto out;

out_norqst:
	spin_unlock(&xprt->queue_lock);
	trace_xprtrdma_reply_rqst_err(rep);
	goto out;

out_shortreply:
	trace_xprtrdma_reply_short_err(rep);

out:
	rpcrdma_rep_put(buf, rep);
}
