
 

#include <linux/sunrpc/svc_rdma.h>
#include <linux/sunrpc/rpc_rdma.h>

#include "xprt_rdma.h"
#include <trace/events/rpcrdma.h>

 
void pcl_free(struct svc_rdma_pcl *pcl)
{
	while (!list_empty(&pcl->cl_chunks)) {
		struct svc_rdma_chunk *chunk;

		chunk = pcl_first_chunk(pcl);
		list_del(&chunk->ch_list);
		kfree(chunk);
	}
}

static struct svc_rdma_chunk *pcl_alloc_chunk(u32 segcount, u32 position)
{
	struct svc_rdma_chunk *chunk;

	chunk = kmalloc(struct_size(chunk, ch_segments, segcount), GFP_KERNEL);
	if (!chunk)
		return NULL;

	chunk->ch_position = position;
	chunk->ch_length = 0;
	chunk->ch_payload_length = 0;
	chunk->ch_segcount = 0;
	return chunk;
}

static struct svc_rdma_chunk *
pcl_lookup_position(struct svc_rdma_pcl *pcl, u32 position)
{
	struct svc_rdma_chunk *pos;

	pcl_for_each_chunk(pos, pcl) {
		if (pos->ch_position == position)
			return pos;
	}
	return NULL;
}

static void pcl_insert_position(struct svc_rdma_pcl *pcl,
				struct svc_rdma_chunk *chunk)
{
	struct svc_rdma_chunk *pos;

	pcl_for_each_chunk(pos, pcl) {
		if (pos->ch_position > chunk->ch_position)
			break;
	}
	__list_add(&chunk->ch_list, pos->ch_list.prev, &pos->ch_list);
	pcl->cl_count++;
}

static void pcl_set_read_segment(const struct svc_rdma_recv_ctxt *rctxt,
				 struct svc_rdma_chunk *chunk,
				 u32 handle, u32 length, u64 offset)
{
	struct svc_rdma_segment *segment;

	segment = &chunk->ch_segments[chunk->ch_segcount];
	segment->rs_handle = handle;
	segment->rs_length = length;
	segment->rs_offset = offset;

	trace_svcrdma_decode_rseg(&rctxt->rc_cid, chunk, segment);

	chunk->ch_length += length;
	chunk->ch_segcount++;
}

 
bool pcl_alloc_call(struct svc_rdma_recv_ctxt *rctxt, __be32 *p)
{
	struct svc_rdma_pcl *pcl = &rctxt->rc_call_pcl;
	unsigned int i, segcount = pcl->cl_count;

	pcl->cl_count = 0;
	for (i = 0; i < segcount; i++) {
		struct svc_rdma_chunk *chunk;
		u32 position, handle, length;
		u64 offset;

		p++;	 
		p = xdr_decode_read_segment(p, &position, &handle,
					    &length, &offset);
		if (position != 0)
			continue;

		if (pcl_is_empty(pcl)) {
			chunk = pcl_alloc_chunk(segcount, position);
			if (!chunk)
				return false;
			pcl_insert_position(pcl, chunk);
		} else {
			chunk = list_first_entry(&pcl->cl_chunks,
						 struct svc_rdma_chunk,
						 ch_list);
		}

		pcl_set_read_segment(rctxt, chunk, handle, length, offset);
	}

	return true;
}

 
bool pcl_alloc_read(struct svc_rdma_recv_ctxt *rctxt, __be32 *p)
{
	struct svc_rdma_pcl *pcl = &rctxt->rc_read_pcl;
	unsigned int i, segcount = pcl->cl_count;

	pcl->cl_count = 0;
	for (i = 0; i < segcount; i++) {
		struct svc_rdma_chunk *chunk;
		u32 position, handle, length;
		u64 offset;

		p++;	 
		p = xdr_decode_read_segment(p, &position, &handle,
					    &length, &offset);
		if (position == 0)
			continue;

		chunk = pcl_lookup_position(pcl, position);
		if (!chunk) {
			chunk = pcl_alloc_chunk(segcount, position);
			if (!chunk)
				return false;
			pcl_insert_position(pcl, chunk);
		}

		pcl_set_read_segment(rctxt, chunk, handle, length, offset);
	}

	return true;
}

 
bool pcl_alloc_write(struct svc_rdma_recv_ctxt *rctxt,
		     struct svc_rdma_pcl *pcl, __be32 *p)
{
	struct svc_rdma_segment *segment;
	struct svc_rdma_chunk *chunk;
	unsigned int i, j;
	u32 segcount;

	for (i = 0; i < pcl->cl_count; i++) {
		p++;	 
		segcount = be32_to_cpup(p++);

		chunk = pcl_alloc_chunk(segcount, 0);
		if (!chunk)
			return false;
		list_add_tail(&chunk->ch_list, &pcl->cl_chunks);

		for (j = 0; j < segcount; j++) {
			segment = &chunk->ch_segments[j];
			p = xdr_decode_rdma_segment(p, &segment->rs_handle,
						    &segment->rs_length,
						    &segment->rs_offset);
			trace_svcrdma_decode_wseg(&rctxt->rc_cid, chunk, j);

			chunk->ch_length += segment->rs_length;
			chunk->ch_segcount++;
		}
	}
	return true;
}

static int pcl_process_region(const struct xdr_buf *xdr,
			      unsigned int offset, unsigned int length,
			      int (*actor)(const struct xdr_buf *, void *),
			      void *data)
{
	struct xdr_buf subbuf;

	if (!length)
		return 0;
	if (xdr_buf_subsegment(xdr, &subbuf, offset, length))
		return -EMSGSIZE;
	return actor(&subbuf, data);
}

 
int pcl_process_nonpayloads(const struct svc_rdma_pcl *pcl,
			    const struct xdr_buf *xdr,
			    int (*actor)(const struct xdr_buf *, void *),
			    void *data)
{
	struct svc_rdma_chunk *chunk, *next;
	unsigned int start;
	int ret;

	chunk = pcl_first_chunk(pcl);

	 
	if (!chunk || !chunk->ch_payload_length)
		return actor(xdr, data);

	 
	ret = pcl_process_region(xdr, 0, chunk->ch_position, actor, data);
	if (ret < 0)
		return ret;

	 
	while ((next = pcl_next_chunk(pcl, chunk))) {
		if (!next->ch_payload_length)
			break;

		start = pcl_chunk_end_offset(chunk);
		ret = pcl_process_region(xdr, start, next->ch_position - start,
					 actor, data);
		if (ret < 0)
			return ret;

		chunk = next;
	}

	 
	start = pcl_chunk_end_offset(chunk);
	ret = pcl_process_region(xdr, start, xdr->len - start, actor, data);
	if (ret < 0)
		return ret;

	return 0;
}
