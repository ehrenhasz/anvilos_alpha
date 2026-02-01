
 

#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/slab.h>
#include <linux/usb.h>

#include "cdnsp-gadget.h"
#include "cdnsp-trace.h"

static void cdnsp_free_stream_info(struct cdnsp_device *pdev,
				   struct cdnsp_ep *pep);
 
static struct cdnsp_segment *cdnsp_segment_alloc(struct cdnsp_device *pdev,
						 unsigned int cycle_state,
						 unsigned int max_packet,
						 gfp_t flags)
{
	struct cdnsp_segment *seg;
	dma_addr_t dma;
	int i;

	seg = kzalloc(sizeof(*seg), flags);
	if (!seg)
		return NULL;

	seg->trbs = dma_pool_zalloc(pdev->segment_pool, flags, &dma);
	if (!seg->trbs) {
		kfree(seg);
		return NULL;
	}

	if (max_packet) {
		seg->bounce_buf = kzalloc(max_packet, flags | GFP_DMA);
		if (!seg->bounce_buf)
			goto free_dma;
	}

	 
	if (cycle_state == 0) {
		for (i = 0; i < TRBS_PER_SEGMENT; i++)
			seg->trbs[i].link.control |= cpu_to_le32(TRB_CYCLE);
	}
	seg->dma = dma;
	seg->next = NULL;

	return seg;

free_dma:
	dma_pool_free(pdev->segment_pool, seg->trbs, dma);
	kfree(seg);

	return NULL;
}

static void cdnsp_segment_free(struct cdnsp_device *pdev,
			       struct cdnsp_segment *seg)
{
	if (seg->trbs)
		dma_pool_free(pdev->segment_pool, seg->trbs, seg->dma);

	kfree(seg->bounce_buf);
	kfree(seg);
}

static void cdnsp_free_segments_for_ring(struct cdnsp_device *pdev,
					 struct cdnsp_segment *first)
{
	struct cdnsp_segment *seg;

	seg = first->next;

	while (seg != first) {
		struct cdnsp_segment *next = seg->next;

		cdnsp_segment_free(pdev, seg);
		seg = next;
	}

	cdnsp_segment_free(pdev, first);
}

 
static void cdnsp_link_segments(struct cdnsp_device *pdev,
				struct cdnsp_segment *prev,
				struct cdnsp_segment *next,
				enum cdnsp_ring_type type)
{
	struct cdnsp_link_trb *link;
	u32 val;

	if (!prev || !next)
		return;

	prev->next = next;
	if (type != TYPE_EVENT) {
		link = &prev->trbs[TRBS_PER_SEGMENT - 1].link;
		link->segment_ptr = cpu_to_le64(next->dma);

		 
		val = le32_to_cpu(link->control);
		val &= ~TRB_TYPE_BITMASK;
		val |= TRB_TYPE(TRB_LINK);
		link->control = cpu_to_le32(val);
	}
}

 
static void cdnsp_link_rings(struct cdnsp_device *pdev,
			     struct cdnsp_ring *ring,
			     struct cdnsp_segment *first,
			     struct cdnsp_segment *last,
			     unsigned int num_segs)
{
	struct cdnsp_segment *next;

	if (!ring || !first || !last)
		return;

	next = ring->enq_seg->next;
	cdnsp_link_segments(pdev, ring->enq_seg, first, ring->type);
	cdnsp_link_segments(pdev, last, next, ring->type);
	ring->num_segs += num_segs;
	ring->num_trbs_free += (TRBS_PER_SEGMENT - 1) * num_segs;

	if (ring->type != TYPE_EVENT && ring->enq_seg == ring->last_seg) {
		ring->last_seg->trbs[TRBS_PER_SEGMENT - 1].link.control &=
			~cpu_to_le32(LINK_TOGGLE);
		last->trbs[TRBS_PER_SEGMENT - 1].link.control |=
			cpu_to_le32(LINK_TOGGLE);
		ring->last_seg = last;
	}
}

 
static int cdnsp_insert_segment_mapping(struct radix_tree_root *trb_address_map,
					struct cdnsp_ring *ring,
					struct cdnsp_segment *seg,
					gfp_t mem_flags)
{
	unsigned long key;
	int ret;

	key = (unsigned long)(seg->dma >> TRB_SEGMENT_SHIFT);

	 
	if (radix_tree_lookup(trb_address_map, key))
		return 0;

	ret = radix_tree_maybe_preload(mem_flags);
	if (ret)
		return ret;

	ret = radix_tree_insert(trb_address_map, key, ring);
	radix_tree_preload_end();

	return ret;
}

static void cdnsp_remove_segment_mapping(struct radix_tree_root *trb_address_map,
					 struct cdnsp_segment *seg)
{
	unsigned long key;

	key = (unsigned long)(seg->dma >> TRB_SEGMENT_SHIFT);
	if (radix_tree_lookup(trb_address_map, key))
		radix_tree_delete(trb_address_map, key);
}

static int cdnsp_update_stream_segment_mapping(struct radix_tree_root *trb_address_map,
					       struct cdnsp_ring *ring,
					       struct cdnsp_segment *first_seg,
					       struct cdnsp_segment *last_seg,
					       gfp_t mem_flags)
{
	struct cdnsp_segment *failed_seg;
	struct cdnsp_segment *seg;
	int ret;

	seg = first_seg;
	do {
		ret = cdnsp_insert_segment_mapping(trb_address_map, ring, seg,
						   mem_flags);
		if (ret)
			goto remove_streams;
		if (seg == last_seg)
			return 0;
		seg = seg->next;
	} while (seg != first_seg);

	return 0;

remove_streams:
	failed_seg = seg;
	seg = first_seg;
	do {
		cdnsp_remove_segment_mapping(trb_address_map, seg);
		if (seg == failed_seg)
			return ret;
		seg = seg->next;
	} while (seg != first_seg);

	return ret;
}

static void cdnsp_remove_stream_mapping(struct cdnsp_ring *ring)
{
	struct cdnsp_segment *seg;

	seg = ring->first_seg;
	do {
		cdnsp_remove_segment_mapping(ring->trb_address_map, seg);
		seg = seg->next;
	} while (seg != ring->first_seg);
}

static int cdnsp_update_stream_mapping(struct cdnsp_ring *ring)
{
	return cdnsp_update_stream_segment_mapping(ring->trb_address_map, ring,
			ring->first_seg, ring->last_seg, GFP_ATOMIC);
}

static void cdnsp_ring_free(struct cdnsp_device *pdev, struct cdnsp_ring *ring)
{
	if (!ring)
		return;

	trace_cdnsp_ring_free(ring);

	if (ring->first_seg) {
		if (ring->type == TYPE_STREAM)
			cdnsp_remove_stream_mapping(ring);

		cdnsp_free_segments_for_ring(pdev, ring->first_seg);
	}

	kfree(ring);
}

void cdnsp_initialize_ring_info(struct cdnsp_ring *ring)
{
	ring->enqueue = ring->first_seg->trbs;
	ring->enq_seg = ring->first_seg;
	ring->dequeue = ring->enqueue;
	ring->deq_seg = ring->first_seg;

	 
	ring->cycle_state = 1;

	 
	ring->num_trbs_free = ring->num_segs * (TRBS_PER_SEGMENT - 1) - 1;
}

 
static int cdnsp_alloc_segments_for_ring(struct cdnsp_device *pdev,
					 struct cdnsp_segment **first,
					 struct cdnsp_segment **last,
					 unsigned int num_segs,
					 unsigned int cycle_state,
					 enum cdnsp_ring_type type,
					 unsigned int max_packet,
					 gfp_t flags)
{
	struct cdnsp_segment *prev;

	 
	prev = cdnsp_segment_alloc(pdev, cycle_state, max_packet, flags);
	if (!prev)
		return -ENOMEM;

	num_segs--;
	*first = prev;

	 
	while (num_segs > 0) {
		struct cdnsp_segment	*next;

		next = cdnsp_segment_alloc(pdev, cycle_state,
					   max_packet, flags);
		if (!next) {
			cdnsp_free_segments_for_ring(pdev, *first);
			return -ENOMEM;
		}

		cdnsp_link_segments(pdev, prev, next, type);

		prev = next;
		num_segs--;
	}

	cdnsp_link_segments(pdev, prev, *first, type);
	*last = prev;

	return 0;
}

 
static struct cdnsp_ring *cdnsp_ring_alloc(struct cdnsp_device *pdev,
					   unsigned int num_segs,
					   enum cdnsp_ring_type type,
					   unsigned int max_packet,
					   gfp_t flags)
{
	struct cdnsp_ring *ring;
	int ret;

	ring = kzalloc(sizeof *(ring), flags);
	if (!ring)
		return NULL;

	ring->num_segs = num_segs;
	ring->bounce_buf_len = max_packet;
	INIT_LIST_HEAD(&ring->td_list);
	ring->type = type;

	if (num_segs == 0)
		return ring;

	ret = cdnsp_alloc_segments_for_ring(pdev, &ring->first_seg,
					    &ring->last_seg, num_segs,
					    1, type, max_packet, flags);
	if (ret)
		goto fail;

	 
	if (type != TYPE_EVENT)
		ring->last_seg->trbs[TRBS_PER_SEGMENT - 1].link.control |=
			cpu_to_le32(LINK_TOGGLE);

	cdnsp_initialize_ring_info(ring);
	trace_cdnsp_ring_alloc(ring);
	return ring;
fail:
	kfree(ring);
	return NULL;
}

void cdnsp_free_endpoint_rings(struct cdnsp_device *pdev, struct cdnsp_ep *pep)
{
	cdnsp_ring_free(pdev, pep->ring);
	pep->ring = NULL;
	cdnsp_free_stream_info(pdev, pep);
}

 
int cdnsp_ring_expansion(struct cdnsp_device *pdev,
			 struct cdnsp_ring *ring,
			 unsigned int num_trbs,
			 gfp_t flags)
{
	unsigned int num_segs_needed;
	struct cdnsp_segment *first;
	struct cdnsp_segment *last;
	unsigned int num_segs;
	int ret;

	num_segs_needed = (num_trbs + (TRBS_PER_SEGMENT - 1) - 1) /
			(TRBS_PER_SEGMENT - 1);

	 
	num_segs = max(ring->num_segs, num_segs_needed);

	ret = cdnsp_alloc_segments_for_ring(pdev, &first, &last, num_segs,
					    ring->cycle_state, ring->type,
					    ring->bounce_buf_len, flags);
	if (ret)
		return -ENOMEM;

	if (ring->type == TYPE_STREAM)
		ret = cdnsp_update_stream_segment_mapping(ring->trb_address_map,
							  ring, first,
							  last, flags);

	if (ret) {
		cdnsp_free_segments_for_ring(pdev, first);

		return ret;
	}

	cdnsp_link_rings(pdev, ring, first, last, num_segs);
	trace_cdnsp_ring_expansion(ring);

	return 0;
}

static int cdnsp_init_device_ctx(struct cdnsp_device *pdev)
{
	int size = HCC_64BYTE_CONTEXT(pdev->hcc_params) ? 2048 : 1024;

	pdev->out_ctx.type = CDNSP_CTX_TYPE_DEVICE;
	pdev->out_ctx.size = size;
	pdev->out_ctx.ctx_size = CTX_SIZE(pdev->hcc_params);
	pdev->out_ctx.bytes = dma_pool_zalloc(pdev->device_pool, GFP_ATOMIC,
					      &pdev->out_ctx.dma);

	if (!pdev->out_ctx.bytes)
		return -ENOMEM;

	pdev->in_ctx.type = CDNSP_CTX_TYPE_INPUT;
	pdev->in_ctx.ctx_size = pdev->out_ctx.ctx_size;
	pdev->in_ctx.size = size + pdev->out_ctx.ctx_size;
	pdev->in_ctx.bytes = dma_pool_zalloc(pdev->device_pool, GFP_ATOMIC,
					     &pdev->in_ctx.dma);

	if (!pdev->in_ctx.bytes) {
		dma_pool_free(pdev->device_pool, pdev->out_ctx.bytes,
			      pdev->out_ctx.dma);
		return -ENOMEM;
	}

	return 0;
}

struct cdnsp_input_control_ctx
	*cdnsp_get_input_control_ctx(struct cdnsp_container_ctx *ctx)
{
	if (ctx->type != CDNSP_CTX_TYPE_INPUT)
		return NULL;

	return (struct cdnsp_input_control_ctx *)ctx->bytes;
}

struct cdnsp_slot_ctx *cdnsp_get_slot_ctx(struct cdnsp_container_ctx *ctx)
{
	if (ctx->type == CDNSP_CTX_TYPE_DEVICE)
		return (struct cdnsp_slot_ctx *)ctx->bytes;

	return (struct cdnsp_slot_ctx *)(ctx->bytes + ctx->ctx_size);
}

struct cdnsp_ep_ctx *cdnsp_get_ep_ctx(struct cdnsp_container_ctx *ctx,
				      unsigned int ep_index)
{
	 
	ep_index++;
	if (ctx->type == CDNSP_CTX_TYPE_INPUT)
		ep_index++;

	return (struct cdnsp_ep_ctx *)(ctx->bytes + (ep_index * ctx->ctx_size));
}

static void cdnsp_free_stream_ctx(struct cdnsp_device *pdev,
				  struct cdnsp_ep *pep)
{
	dma_pool_free(pdev->device_pool, pep->stream_info.stream_ctx_array,
		      pep->stream_info.ctx_array_dma);
}

 
static struct cdnsp_stream_ctx
	*cdnsp_alloc_stream_ctx(struct cdnsp_device *pdev, struct cdnsp_ep *pep)
{
	size_t size = sizeof(struct cdnsp_stream_ctx) *
		      pep->stream_info.num_stream_ctxs;

	if (size > CDNSP_CTX_SIZE)
		return NULL;

	 
	return dma_pool_zalloc(pdev->device_pool, GFP_DMA32 | GFP_ATOMIC,
			       &pep->stream_info.ctx_array_dma);
}

struct cdnsp_ring *cdnsp_dma_to_transfer_ring(struct cdnsp_ep *pep, u64 address)
{
	if (pep->ep_state & EP_HAS_STREAMS)
		return radix_tree_lookup(&pep->stream_info.trb_address_map,
					 address >> TRB_SEGMENT_SHIFT);

	return pep->ring;
}

 
int cdnsp_alloc_stream_info(struct cdnsp_device *pdev,
			    struct cdnsp_ep *pep,
			    unsigned int num_stream_ctxs,
			    unsigned int num_streams)
{
	struct cdnsp_stream_info *stream_info;
	struct cdnsp_ring *cur_ring;
	u32 cur_stream;
	u64 addr;
	int ret;
	int mps;

	stream_info = &pep->stream_info;
	stream_info->num_streams = num_streams;
	stream_info->num_stream_ctxs = num_stream_ctxs;

	 
	stream_info->stream_rings = kcalloc(num_streams,
					    sizeof(struct cdnsp_ring *),
					    GFP_ATOMIC);
	if (!stream_info->stream_rings)
		return -ENOMEM;

	 
	stream_info->stream_ctx_array = cdnsp_alloc_stream_ctx(pdev, pep);
	if (!stream_info->stream_ctx_array)
		goto cleanup_stream_rings;

	memset(stream_info->stream_ctx_array, 0,
	       sizeof(struct cdnsp_stream_ctx) * num_stream_ctxs);
	INIT_RADIX_TREE(&stream_info->trb_address_map, GFP_ATOMIC);
	mps = usb_endpoint_maxp(pep->endpoint.desc);

	 
	for (cur_stream = 1; cur_stream < num_streams; cur_stream++) {
		cur_ring = cdnsp_ring_alloc(pdev, 2, TYPE_STREAM, mps,
					    GFP_ATOMIC);
		stream_info->stream_rings[cur_stream] = cur_ring;

		if (!cur_ring)
			goto cleanup_rings;

		cur_ring->stream_id = cur_stream;
		cur_ring->trb_address_map = &stream_info->trb_address_map;

		 
		addr = cur_ring->first_seg->dma | SCT_FOR_CTX(SCT_PRI_TR) |
		       cur_ring->cycle_state;

		stream_info->stream_ctx_array[cur_stream].stream_ring =
			cpu_to_le64(addr);

		trace_cdnsp_set_stream_ring(cur_ring);

		ret = cdnsp_update_stream_mapping(cur_ring);
		if (ret)
			goto cleanup_rings;
	}

	return 0;

cleanup_rings:
	for (cur_stream = 1; cur_stream < num_streams; cur_stream++) {
		cur_ring = stream_info->stream_rings[cur_stream];
		if (cur_ring) {
			cdnsp_ring_free(pdev, cur_ring);
			stream_info->stream_rings[cur_stream] = NULL;
		}
	}

cleanup_stream_rings:
	kfree(pep->stream_info.stream_rings);

	return -ENOMEM;
}

 
static void cdnsp_free_stream_info(struct cdnsp_device *pdev,
				   struct cdnsp_ep *pep)
{
	struct cdnsp_stream_info *stream_info = &pep->stream_info;
	struct cdnsp_ring *cur_ring;
	int cur_stream;

	if (!(pep->ep_state & EP_HAS_STREAMS))
		return;

	for (cur_stream = 1; cur_stream < stream_info->num_streams;
	     cur_stream++) {
		cur_ring = stream_info->stream_rings[cur_stream];
		if (cur_ring) {
			cdnsp_ring_free(pdev, cur_ring);
			stream_info->stream_rings[cur_stream] = NULL;
		}
	}

	if (stream_info->stream_ctx_array)
		cdnsp_free_stream_ctx(pdev, pep);

	kfree(stream_info->stream_rings);
	pep->ep_state &= ~EP_HAS_STREAMS;
}

 
static void cdnsp_free_priv_device(struct cdnsp_device *pdev)
{
	pdev->dcbaa->dev_context_ptrs[1] = 0;

	cdnsp_free_endpoint_rings(pdev, &pdev->eps[0]);

	if (pdev->in_ctx.bytes)
		dma_pool_free(pdev->device_pool, pdev->in_ctx.bytes,
			      pdev->in_ctx.dma);

	if (pdev->out_ctx.bytes)
		dma_pool_free(pdev->device_pool, pdev->out_ctx.bytes,
			      pdev->out_ctx.dma);

	pdev->in_ctx.bytes = NULL;
	pdev->out_ctx.bytes = NULL;
}

static int cdnsp_alloc_priv_device(struct cdnsp_device *pdev)
{
	int ret;

	ret = cdnsp_init_device_ctx(pdev);
	if (ret)
		return ret;

	 
	pdev->eps[0].ring = cdnsp_ring_alloc(pdev, 2, TYPE_CTRL, 0, GFP_ATOMIC);
	if (!pdev->eps[0].ring)
		goto fail;

	 
	pdev->dcbaa->dev_context_ptrs[1] = cpu_to_le64(pdev->out_ctx.dma);
	pdev->cmd.in_ctx = &pdev->in_ctx;

	trace_cdnsp_alloc_priv_device(pdev);
	return 0;
fail:
	dma_pool_free(pdev->device_pool, pdev->out_ctx.bytes,
		      pdev->out_ctx.dma);
	dma_pool_free(pdev->device_pool, pdev->in_ctx.bytes,
		      pdev->in_ctx.dma);

	return ret;
}

void cdnsp_copy_ep0_dequeue_into_input_ctx(struct cdnsp_device *pdev)
{
	struct cdnsp_ep_ctx *ep0_ctx = pdev->eps[0].in_ctx;
	struct cdnsp_ring *ep_ring = pdev->eps[0].ring;
	dma_addr_t dma;

	dma = cdnsp_trb_virt_to_dma(ep_ring->enq_seg, ep_ring->enqueue);
	ep0_ctx->deq = cpu_to_le64(dma | ep_ring->cycle_state);
}

 
int cdnsp_setup_addressable_priv_dev(struct cdnsp_device *pdev)
{
	struct cdnsp_slot_ctx *slot_ctx;
	struct cdnsp_ep_ctx *ep0_ctx;
	u32 max_packets, port;

	ep0_ctx = cdnsp_get_ep_ctx(&pdev->in_ctx, 0);
	slot_ctx = cdnsp_get_slot_ctx(&pdev->in_ctx);

	 
	slot_ctx->dev_info |= cpu_to_le32(LAST_CTX(1));

	switch (pdev->gadget.speed) {
	case USB_SPEED_SUPER_PLUS:
		slot_ctx->dev_info |= cpu_to_le32(SLOT_SPEED_SSP);
		max_packets = MAX_PACKET(512);
		break;
	case USB_SPEED_SUPER:
		slot_ctx->dev_info |= cpu_to_le32(SLOT_SPEED_SS);
		max_packets = MAX_PACKET(512);
		break;
	case USB_SPEED_HIGH:
		slot_ctx->dev_info |= cpu_to_le32(SLOT_SPEED_HS);
		max_packets = MAX_PACKET(64);
		break;
	case USB_SPEED_FULL:
		slot_ctx->dev_info |= cpu_to_le32(SLOT_SPEED_FS);
		max_packets = MAX_PACKET(64);
		break;
	default:
		 
		return -EINVAL;
	}

	port = DEV_PORT(pdev->active_port->port_num);
	slot_ctx->dev_port |= cpu_to_le32(port);
	slot_ctx->dev_state = cpu_to_le32((pdev->device_address &
					   DEV_ADDR_MASK));
	ep0_ctx->tx_info = cpu_to_le32(EP_AVG_TRB_LENGTH(0x8));
	ep0_ctx->ep_info2 = cpu_to_le32(EP_TYPE(CTRL_EP));
	ep0_ctx->ep_info2 |= cpu_to_le32(MAX_BURST(0) | ERROR_COUNT(3) |
					 max_packets);

	ep0_ctx->deq = cpu_to_le64(pdev->eps[0].ring->first_seg->dma |
				   pdev->eps[0].ring->cycle_state);

	trace_cdnsp_setup_addressable_priv_device(pdev);

	return 0;
}

 
static unsigned int cdnsp_parse_exponent_interval(struct usb_gadget *g,
						  struct cdnsp_ep *pep)
{
	unsigned int interval;

	interval = clamp_val(pep->endpoint.desc->bInterval, 1, 16) - 1;
	if (interval != pep->endpoint.desc->bInterval - 1)
		dev_warn(&g->dev, "ep %s - rounding interval to %d %sframes\n",
			 pep->name, 1 << interval,
			 g->speed == USB_SPEED_FULL ? "" : "micro");

	 
	if (g->speed == USB_SPEED_FULL)
		interval += 3;	 

	 
	if (interval > 12)
		interval = 12;

	return interval;
}

 
static unsigned int cdnsp_microframes_to_exponent(struct usb_gadget *g,
						  struct cdnsp_ep *pep,
						  unsigned int desc_interval,
						  unsigned int min_exponent,
						  unsigned int max_exponent)
{
	unsigned int interval;

	interval = fls(desc_interval) - 1;
	return clamp_val(interval, min_exponent, max_exponent);
}

 
static unsigned int cdnsp_get_endpoint_interval(struct usb_gadget *g,
						struct cdnsp_ep *pep)
{
	unsigned int interval = 0;

	switch (g->speed) {
	case USB_SPEED_HIGH:
	case USB_SPEED_SUPER_PLUS:
	case USB_SPEED_SUPER:
		if (usb_endpoint_xfer_int(pep->endpoint.desc) ||
		    usb_endpoint_xfer_isoc(pep->endpoint.desc))
			interval = cdnsp_parse_exponent_interval(g, pep);
		break;
	case USB_SPEED_FULL:
		if (usb_endpoint_xfer_isoc(pep->endpoint.desc)) {
			interval = cdnsp_parse_exponent_interval(g, pep);
		} else if (usb_endpoint_xfer_int(pep->endpoint.desc)) {
			interval = pep->endpoint.desc->bInterval << 3;
			interval = cdnsp_microframes_to_exponent(g, pep,
								 interval,
								 3, 10);
		}

		break;
	default:
		WARN_ON(1);
	}

	return interval;
}

 
static u32 cdnsp_get_endpoint_mult(struct usb_gadget *g, struct cdnsp_ep *pep)
{
	if (g->speed < USB_SPEED_SUPER ||
	    !usb_endpoint_xfer_isoc(pep->endpoint.desc))
		return 0;

	return pep->endpoint.comp_desc->bmAttributes;
}

static u32 cdnsp_get_endpoint_max_burst(struct usb_gadget *g,
					struct cdnsp_ep *pep)
{
	 
	if (g->speed >= USB_SPEED_SUPER)
		return pep->endpoint.comp_desc->bMaxBurst;

	if (g->speed == USB_SPEED_HIGH &&
	    (usb_endpoint_xfer_isoc(pep->endpoint.desc) ||
	     usb_endpoint_xfer_int(pep->endpoint.desc)))
		return usb_endpoint_maxp_mult(pep->endpoint.desc) - 1;

	return 0;
}

static u32 cdnsp_get_endpoint_type(const struct usb_endpoint_descriptor *desc)
{
	int in;

	in = usb_endpoint_dir_in(desc);

	switch (usb_endpoint_type(desc)) {
	case USB_ENDPOINT_XFER_CONTROL:
		return CTRL_EP;
	case USB_ENDPOINT_XFER_BULK:
		return in ? BULK_IN_EP : BULK_OUT_EP;
	case USB_ENDPOINT_XFER_ISOC:
		return in ? ISOC_IN_EP : ISOC_OUT_EP;
	case USB_ENDPOINT_XFER_INT:
		return in ? INT_IN_EP : INT_OUT_EP;
	}

	return 0;
}

 
static u32 cdnsp_get_max_esit_payload(struct usb_gadget *g,
				      struct cdnsp_ep *pep)
{
	int max_packet;
	int max_burst;

	 
	if (usb_endpoint_xfer_control(pep->endpoint.desc) ||
	    usb_endpoint_xfer_bulk(pep->endpoint.desc))
		return 0;

	 
	if (g->speed >= USB_SPEED_SUPER_PLUS &&
	    USB_SS_SSP_ISOC_COMP(pep->endpoint.desc->bmAttributes))
		return le16_to_cpu(pep->endpoint.comp_desc->wBytesPerInterval);
	 
	else if (g->speed >= USB_SPEED_SUPER)
		return le16_to_cpu(pep->endpoint.comp_desc->wBytesPerInterval);

	max_packet = usb_endpoint_maxp(pep->endpoint.desc);
	max_burst = usb_endpoint_maxp_mult(pep->endpoint.desc);

	 
	return max_packet * max_burst;
}

int cdnsp_endpoint_init(struct cdnsp_device *pdev,
			struct cdnsp_ep *pep,
			gfp_t mem_flags)
{
	enum cdnsp_ring_type ring_type;
	struct cdnsp_ep_ctx *ep_ctx;
	unsigned int err_count = 0;
	unsigned int avg_trb_len;
	unsigned int max_packet;
	unsigned int max_burst;
	unsigned int interval;
	u32 max_esit_payload;
	unsigned int mult;
	u32 endpoint_type;
	int ret;

	ep_ctx = pep->in_ctx;

	endpoint_type = cdnsp_get_endpoint_type(pep->endpoint.desc);
	if (!endpoint_type)
		return -EINVAL;

	ring_type = usb_endpoint_type(pep->endpoint.desc);

	 
	max_esit_payload = cdnsp_get_max_esit_payload(&pdev->gadget, pep);
	interval = cdnsp_get_endpoint_interval(&pdev->gadget, pep);
	mult = cdnsp_get_endpoint_mult(&pdev->gadget, pep);
	max_packet = usb_endpoint_maxp(pep->endpoint.desc);
	max_burst = cdnsp_get_endpoint_max_burst(&pdev->gadget, pep);
	avg_trb_len = max_esit_payload;

	 
	if (!usb_endpoint_xfer_isoc(pep->endpoint.desc))
		err_count = 3;
	if (usb_endpoint_xfer_bulk(pep->endpoint.desc) &&
	    pdev->gadget.speed == USB_SPEED_HIGH)
		max_packet = 512;
	 
	if (usb_endpoint_xfer_control(pep->endpoint.desc))
		avg_trb_len = 8;

	 
	pep->ring = cdnsp_ring_alloc(pdev, 2, ring_type, max_packet, mem_flags);
	if (!pep->ring)
		return -ENOMEM;

	pep->skip = false;

	 
	ep_ctx->ep_info = cpu_to_le32(EP_MAX_ESIT_PAYLOAD_HI(max_esit_payload) |
				EP_INTERVAL(interval) | EP_MULT(mult));
	ep_ctx->ep_info2 = cpu_to_le32(EP_TYPE(endpoint_type) |
				MAX_PACKET(max_packet) | MAX_BURST(max_burst) |
				ERROR_COUNT(err_count));
	ep_ctx->deq = cpu_to_le64(pep->ring->first_seg->dma |
				  pep->ring->cycle_state);

	ep_ctx->tx_info = cpu_to_le32(EP_MAX_ESIT_PAYLOAD_LO(max_esit_payload) |
				EP_AVG_TRB_LENGTH(avg_trb_len));

	if (usb_endpoint_xfer_bulk(pep->endpoint.desc) &&
	    pdev->gadget.speed > USB_SPEED_HIGH) {
		ret = cdnsp_alloc_streams(pdev, pep);
		if (ret < 0)
			return ret;
	}

	return 0;
}

void cdnsp_endpoint_zero(struct cdnsp_device *pdev, struct cdnsp_ep *pep)
{
	pep->in_ctx->ep_info = 0;
	pep->in_ctx->ep_info2 = 0;
	pep->in_ctx->deq = 0;
	pep->in_ctx->tx_info = 0;
}

static int cdnsp_alloc_erst(struct cdnsp_device *pdev,
			    struct cdnsp_ring *evt_ring,
			    struct cdnsp_erst *erst)
{
	struct cdnsp_erst_entry *entry;
	struct cdnsp_segment *seg;
	unsigned int val;
	size_t size;

	size = sizeof(struct cdnsp_erst_entry) * evt_ring->num_segs;
	erst->entries = dma_alloc_coherent(pdev->dev, size,
					   &erst->erst_dma_addr, GFP_KERNEL);
	if (!erst->entries)
		return -ENOMEM;

	erst->num_entries = evt_ring->num_segs;

	seg = evt_ring->first_seg;
	for (val = 0; val < evt_ring->num_segs; val++) {
		entry = &erst->entries[val];
		entry->seg_addr = cpu_to_le64(seg->dma);
		entry->seg_size = cpu_to_le32(TRBS_PER_SEGMENT);
		entry->rsvd = 0;
		seg = seg->next;
	}

	return 0;
}

static void cdnsp_free_erst(struct cdnsp_device *pdev, struct cdnsp_erst *erst)
{
	size_t size = sizeof(struct cdnsp_erst_entry) * (erst->num_entries);
	struct device *dev = pdev->dev;

	if (erst->entries)
		dma_free_coherent(dev, size, erst->entries,
				  erst->erst_dma_addr);

	erst->entries = NULL;
}

void cdnsp_mem_cleanup(struct cdnsp_device *pdev)
{
	struct device *dev = pdev->dev;

	cdnsp_free_priv_device(pdev);
	cdnsp_free_erst(pdev, &pdev->erst);

	if (pdev->event_ring)
		cdnsp_ring_free(pdev, pdev->event_ring);

	pdev->event_ring = NULL;

	if (pdev->cmd_ring)
		cdnsp_ring_free(pdev, pdev->cmd_ring);

	pdev->cmd_ring = NULL;

	dma_pool_destroy(pdev->segment_pool);
	pdev->segment_pool = NULL;
	dma_pool_destroy(pdev->device_pool);
	pdev->device_pool = NULL;

	dma_free_coherent(dev, sizeof(*pdev->dcbaa),
			  pdev->dcbaa, pdev->dcbaa->dma);

	pdev->dcbaa = NULL;

	pdev->usb2_port.exist = 0;
	pdev->usb3_port.exist = 0;
	pdev->usb2_port.port_num = 0;
	pdev->usb3_port.port_num = 0;
	pdev->active_port = NULL;
}

static void cdnsp_set_event_deq(struct cdnsp_device *pdev)
{
	dma_addr_t deq;
	u64 temp;

	deq = cdnsp_trb_virt_to_dma(pdev->event_ring->deq_seg,
				    pdev->event_ring->dequeue);

	 
	temp = cdnsp_read_64(&pdev->ir_set->erst_dequeue);
	temp &= ERST_PTR_MASK;

	 
	temp &= ~ERST_EHB;

	cdnsp_write_64(((u64)deq & (u64)~ERST_PTR_MASK) | temp,
		       &pdev->ir_set->erst_dequeue);
}

static void cdnsp_add_in_port(struct cdnsp_device *pdev,
			      struct cdnsp_port *port,
			      __le32 __iomem *addr)
{
	u32 temp, port_offset, port_count;

	temp = readl(addr);
	port->maj_rev = CDNSP_EXT_PORT_MAJOR(temp);
	port->min_rev = CDNSP_EXT_PORT_MINOR(temp);

	 
	temp = readl(addr + 2);
	port_offset = CDNSP_EXT_PORT_OFF(temp);
	port_count = CDNSP_EXT_PORT_COUNT(temp);

	trace_cdnsp_port_info(addr, port_offset, port_count, port->maj_rev);

	port->port_num = port_offset;
	port->exist = 1;
}

 
static int cdnsp_setup_port_arrays(struct cdnsp_device *pdev)
{
	void __iomem *base;
	u32 offset;
	int i;

	base = &pdev->cap_regs->hc_capbase;
	offset = cdnsp_find_next_ext_cap(base, 0,
					 EXT_CAP_CFG_DEV_20PORT_CAP_ID);
	pdev->port20_regs = base + offset;

	offset = cdnsp_find_next_ext_cap(base, 0, D_XEC_CFG_3XPORT_CAP);
	pdev->port3x_regs =  base + offset;

	offset = 0;
	base = &pdev->cap_regs->hc_capbase;

	 
	for (i = 0; i < 2; i++) {
		u32 temp;

		offset = cdnsp_find_next_ext_cap(base, offset,
						 EXT_CAPS_PROTOCOL);
		temp = readl(base + offset);

		if (CDNSP_EXT_PORT_MAJOR(temp) == 0x03 &&
		    !pdev->usb3_port.port_num)
			cdnsp_add_in_port(pdev, &pdev->usb3_port,
					  base + offset);

		if (CDNSP_EXT_PORT_MAJOR(temp) == 0x02 &&
		    !pdev->usb2_port.port_num)
			cdnsp_add_in_port(pdev, &pdev->usb2_port,
					  base + offset);
	}

	if (!pdev->usb2_port.exist || !pdev->usb3_port.exist) {
		dev_err(pdev->dev, "Error: Only one port detected\n");
		return -ENODEV;
	}

	trace_cdnsp_init("Found USB 2.0 ports and  USB 3.0 ports.");

	pdev->usb2_port.regs = (struct cdnsp_port_regs __iomem *)
			       (&pdev->op_regs->port_reg_base + NUM_PORT_REGS *
				(pdev->usb2_port.port_num - 1));

	pdev->usb3_port.regs = (struct cdnsp_port_regs __iomem *)
			       (&pdev->op_regs->port_reg_base + NUM_PORT_REGS *
				(pdev->usb3_port.port_num - 1));

	return 0;
}

 
int cdnsp_mem_init(struct cdnsp_device *pdev)
{
	struct device *dev = pdev->dev;
	int ret = -ENOMEM;
	unsigned int val;
	dma_addr_t dma;
	u32 page_size;
	u64 val_64;

	 
	page_size = 1 << 12;

	val = readl(&pdev->op_regs->config_reg);
	val |= ((val & ~MAX_DEVS) | CDNSP_DEV_MAX_SLOTS) | CONFIG_U3E;
	writel(val, &pdev->op_regs->config_reg);

	 
	pdev->dcbaa = dma_alloc_coherent(dev, sizeof(*pdev->dcbaa),
					 &dma, GFP_KERNEL);
	if (!pdev->dcbaa)
		return -ENOMEM;

	pdev->dcbaa->dma = dma;

	cdnsp_write_64(dma, &pdev->op_regs->dcbaa_ptr);

	 
	pdev->segment_pool = dma_pool_create("CDNSP ring segments", dev,
					     TRB_SEGMENT_SIZE, TRB_SEGMENT_SIZE,
					     page_size);
	if (!pdev->segment_pool)
		goto release_dcbaa;

	pdev->device_pool = dma_pool_create("CDNSP input/output contexts", dev,
					    CDNSP_CTX_SIZE, 64, page_size);
	if (!pdev->device_pool)
		goto destroy_segment_pool;


	 
	pdev->cmd_ring = cdnsp_ring_alloc(pdev, 1, TYPE_COMMAND, 0, GFP_KERNEL);
	if (!pdev->cmd_ring)
		goto destroy_device_pool;

	 
	val_64 = cdnsp_read_64(&pdev->op_regs->cmd_ring);
	val_64 = (val_64 & (u64)CMD_RING_RSVD_BITS) |
		 (pdev->cmd_ring->first_seg->dma & (u64)~CMD_RING_RSVD_BITS) |
		 pdev->cmd_ring->cycle_state;
	cdnsp_write_64(val_64, &pdev->op_regs->cmd_ring);

	val = readl(&pdev->cap_regs->db_off);
	val &= DBOFF_MASK;
	pdev->dba = (void __iomem *)pdev->cap_regs + val;

	 
	pdev->ir_set = &pdev->run_regs->ir_set[0];

	 
	pdev->event_ring = cdnsp_ring_alloc(pdev, ERST_NUM_SEGS, TYPE_EVENT,
					    0, GFP_KERNEL);
	if (!pdev->event_ring)
		goto free_cmd_ring;

	ret = cdnsp_alloc_erst(pdev, pdev->event_ring, &pdev->erst);
	if (ret)
		goto free_event_ring;

	 
	val = readl(&pdev->ir_set->erst_size);
	val &= ERST_SIZE_MASK;
	val |= ERST_NUM_SEGS;
	writel(val, &pdev->ir_set->erst_size);

	 
	val_64 = cdnsp_read_64(&pdev->ir_set->erst_base);
	val_64 &= ERST_PTR_MASK;
	val_64 |= (pdev->erst.erst_dma_addr & (u64)~ERST_PTR_MASK);
	cdnsp_write_64(val_64, &pdev->ir_set->erst_base);

	 
	cdnsp_set_event_deq(pdev);

	ret = cdnsp_setup_port_arrays(pdev);
	if (ret)
		goto free_erst;

	ret = cdnsp_alloc_priv_device(pdev);
	if (ret) {
		dev_err(pdev->dev,
			"Could not allocate cdnsp_device data structures\n");
		goto free_erst;
	}

	return 0;

free_erst:
	cdnsp_free_erst(pdev, &pdev->erst);
free_event_ring:
	cdnsp_ring_free(pdev, pdev->event_ring);
free_cmd_ring:
	cdnsp_ring_free(pdev, pdev->cmd_ring);
destroy_device_pool:
	dma_pool_destroy(pdev->device_pool);
destroy_segment_pool:
	dma_pool_destroy(pdev->segment_pool);
release_dcbaa:
	dma_free_coherent(dev, sizeof(*pdev->dcbaa), pdev->dcbaa,
			  pdev->dcbaa->dma);

	cdnsp_reset(pdev);

	return ret;
}
