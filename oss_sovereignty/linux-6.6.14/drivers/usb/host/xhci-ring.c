
 

 

#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include "xhci.h"
#include "xhci-trace.h"

static int queue_command(struct xhci_hcd *xhci, struct xhci_command *cmd,
			 u32 field1, u32 field2,
			 u32 field3, u32 field4, bool command_must_succeed);

 
dma_addr_t xhci_trb_virt_to_dma(struct xhci_segment *seg,
		union xhci_trb *trb)
{
	unsigned long segment_offset;

	if (!seg || !trb || trb < seg->trbs)
		return 0;
	 
	segment_offset = trb - seg->trbs;
	if (segment_offset >= TRBS_PER_SEGMENT)
		return 0;
	return seg->dma + (segment_offset * sizeof(*trb));
}

static bool trb_is_noop(union xhci_trb *trb)
{
	return TRB_TYPE_NOOP_LE32(trb->generic.field[3]);
}

static bool trb_is_link(union xhci_trb *trb)
{
	return TRB_TYPE_LINK_LE32(trb->link.control);
}

static bool last_trb_on_seg(struct xhci_segment *seg, union xhci_trb *trb)
{
	return trb == &seg->trbs[TRBS_PER_SEGMENT - 1];
}

static bool last_trb_on_ring(struct xhci_ring *ring,
			struct xhci_segment *seg, union xhci_trb *trb)
{
	return last_trb_on_seg(seg, trb) && (seg->next == ring->first_seg);
}

static bool link_trb_toggles_cycle(union xhci_trb *trb)
{
	return le32_to_cpu(trb->link.control) & LINK_TOGGLE;
}

static bool last_td_in_urb(struct xhci_td *td)
{
	struct urb_priv *urb_priv = td->urb->hcpriv;

	return urb_priv->num_tds_done == urb_priv->num_tds;
}

static void inc_td_cnt(struct urb *urb)
{
	struct urb_priv *urb_priv = urb->hcpriv;

	urb_priv->num_tds_done++;
}

static void trb_to_noop(union xhci_trb *trb, u32 noop_type)
{
	if (trb_is_link(trb)) {
		 
		trb->link.control &= cpu_to_le32(~TRB_CHAIN);
	} else {
		trb->generic.field[0] = 0;
		trb->generic.field[1] = 0;
		trb->generic.field[2] = 0;
		 
		trb->generic.field[3] &= cpu_to_le32(TRB_CYCLE);
		trb->generic.field[3] |= cpu_to_le32(TRB_TYPE(noop_type));
	}
}

 
static void next_trb(struct xhci_hcd *xhci,
		struct xhci_ring *ring,
		struct xhci_segment **seg,
		union xhci_trb **trb)
{
	if (trb_is_link(*trb)) {
		*seg = (*seg)->next;
		*trb = ((*seg)->trbs);
	} else {
		(*trb)++;
	}
}

 
void inc_deq(struct xhci_hcd *xhci, struct xhci_ring *ring)
{
	unsigned int link_trb_count = 0;

	 
	if (ring->type == TYPE_EVENT) {
		if (!last_trb_on_seg(ring->deq_seg, ring->dequeue)) {
			ring->dequeue++;
			goto out;
		}
		if (last_trb_on_ring(ring, ring->deq_seg, ring->dequeue))
			ring->cycle_state ^= 1;
		ring->deq_seg = ring->deq_seg->next;
		ring->dequeue = ring->deq_seg->trbs;
		goto out;
	}

	 
	if (!trb_is_link(ring->dequeue)) {
		if (last_trb_on_seg(ring->deq_seg, ring->dequeue))
			xhci_warn(xhci, "Missing link TRB at end of segment\n");
		else
			ring->dequeue++;
	}

	while (trb_is_link(ring->dequeue)) {
		ring->deq_seg = ring->deq_seg->next;
		ring->dequeue = ring->deq_seg->trbs;

		if (link_trb_count++ > ring->num_segs) {
			xhci_warn(xhci, "Ring is an endless link TRB loop\n");
			break;
		}
	}
out:
	trace_xhci_inc_deq(ring);

	return;
}

 
static void inc_enq(struct xhci_hcd *xhci, struct xhci_ring *ring,
			bool more_trbs_coming)
{
	u32 chain;
	union xhci_trb *next;
	unsigned int link_trb_count = 0;

	chain = le32_to_cpu(ring->enqueue->generic.field[3]) & TRB_CHAIN;

	if (last_trb_on_seg(ring->enq_seg, ring->enqueue)) {
		xhci_err(xhci, "Tried to move enqueue past ring segment\n");
		return;
	}

	next = ++(ring->enqueue);

	 
	while (trb_is_link(next)) {

		 
		if (!chain && !more_trbs_coming)
			break;

		 
		if (!(ring->type == TYPE_ISOC &&
		      (xhci->quirks & XHCI_AMD_0x96_HOST)) &&
		    !xhci_link_trb_quirk(xhci)) {
			next->link.control &= cpu_to_le32(~TRB_CHAIN);
			next->link.control |= cpu_to_le32(chain);
		}
		 
		wmb();
		next->link.control ^= cpu_to_le32(TRB_CYCLE);

		 
		if (link_trb_toggles_cycle(next))
			ring->cycle_state ^= 1;

		ring->enq_seg = ring->enq_seg->next;
		ring->enqueue = ring->enq_seg->trbs;
		next = ring->enqueue;

		if (link_trb_count++ > ring->num_segs) {
			xhci_warn(xhci, "%s: Ring link TRB loop\n", __func__);
			break;
		}
	}

	trace_xhci_inc_enq(ring);
}

 
static unsigned int xhci_num_trbs_free(struct xhci_hcd *xhci, struct xhci_ring *ring)
{
	struct xhci_segment *enq_seg = ring->enq_seg;
	union xhci_trb *enq = ring->enqueue;
	union xhci_trb *last_on_seg;
	unsigned int free = 0;
	int i = 0;

	 
	if (trb_is_link(enq)) {
		enq_seg = enq_seg->next;
		enq = enq_seg->trbs;
	}

	 
	if (enq == ring->dequeue)
		return ring->num_segs * (TRBS_PER_SEGMENT - 1);

	do {
		if (ring->deq_seg == enq_seg && ring->dequeue >= enq)
			return free + (ring->dequeue - enq);
		last_on_seg = &enq_seg->trbs[TRBS_PER_SEGMENT - 1];
		free += last_on_seg - enq;
		enq_seg = enq_seg->next;
		enq = enq_seg->trbs;
	} while (i++ <= ring->num_segs);

	return free;
}

 

static unsigned int xhci_ring_expansion_needed(struct xhci_hcd *xhci, struct xhci_ring *ring,
					       unsigned int num_trbs)
{
	struct xhci_segment *seg;
	int trbs_past_seg;
	int enq_used;
	int new_segs;

	enq_used = ring->enqueue - ring->enq_seg->trbs;

	 
	trbs_past_seg = enq_used + num_trbs - (TRBS_PER_SEGMENT - 1);

	if (trbs_past_seg <= 0)
		return 0;

	 
	if (trb_is_link(ring->enqueue) && ring->enq_seg->next->trbs == ring->dequeue)
		return 0;

	new_segs = 1 + (trbs_past_seg / (TRBS_PER_SEGMENT - 1));
	seg = ring->enq_seg;

	while (new_segs > 0) {
		seg = seg->next;
		if (seg == ring->deq_seg) {
			xhci_dbg(xhci, "Ring expansion by %d segments needed\n",
				 new_segs);
			xhci_dbg(xhci, "Adding %d trbs moves enq %d trbs into deq seg\n",
				 num_trbs, trbs_past_seg % TRBS_PER_SEGMENT);
			return new_segs;
		}
		new_segs--;
	}

	return 0;
}

 
void xhci_ring_cmd_db(struct xhci_hcd *xhci)
{
	if (!(xhci->cmd_ring_state & CMD_RING_STATE_RUNNING))
		return;

	xhci_dbg(xhci, "// Ding dong!\n");

	trace_xhci_ring_host_doorbell(0, DB_VALUE_HOST);

	writel(DB_VALUE_HOST, &xhci->dba->doorbell[0]);
	 
	readl(&xhci->dba->doorbell[0]);
}

static bool xhci_mod_cmd_timer(struct xhci_hcd *xhci, unsigned long delay)
{
	return mod_delayed_work(system_wq, &xhci->cmd_timer, delay);
}

static struct xhci_command *xhci_next_queued_cmd(struct xhci_hcd *xhci)
{
	return list_first_entry_or_null(&xhci->cmd_list, struct xhci_command,
					cmd_list);
}

 
static void xhci_handle_stopped_cmd_ring(struct xhci_hcd *xhci,
					 struct xhci_command *cur_cmd)
{
	struct xhci_command *i_cmd;

	 
	list_for_each_entry(i_cmd, &xhci->cmd_list, cmd_list) {

		if (i_cmd->status != COMP_COMMAND_ABORTED)
			continue;

		i_cmd->status = COMP_COMMAND_RING_STOPPED;

		xhci_dbg(xhci, "Turn aborted command %p to no-op\n",
			 i_cmd->command_trb);

		trb_to_noop(i_cmd->command_trb, TRB_CMD_NOOP);

		 
	}

	xhci->cmd_ring_state = CMD_RING_STATE_RUNNING;

	 
	if ((xhci->cmd_ring->dequeue != xhci->cmd_ring->enqueue) &&
	    !(xhci->xhc_state & XHCI_STATE_DYING)) {
		xhci->current_cmd = cur_cmd;
		xhci_mod_cmd_timer(xhci, XHCI_CMD_DEFAULT_TIMEOUT);
		xhci_ring_cmd_db(xhci);
	}
}

 
static int xhci_abort_cmd_ring(struct xhci_hcd *xhci, unsigned long flags)
{
	struct xhci_segment *new_seg	= xhci->cmd_ring->deq_seg;
	union xhci_trb *new_deq		= xhci->cmd_ring->dequeue;
	u64 crcr;
	int ret;

	xhci_dbg(xhci, "Abort command ring\n");

	reinit_completion(&xhci->cmd_ring_stop_completion);

	 
	next_trb(xhci, NULL, &new_seg, &new_deq);
	if (trb_is_link(new_deq))
		next_trb(xhci, NULL, &new_seg, &new_deq);

	crcr = xhci_trb_virt_to_dma(new_seg, new_deq);
	xhci_write_64(xhci, crcr | CMD_RING_ABORT, &xhci->op_regs->cmd_ring);

	 
	ret = xhci_handshake(&xhci->op_regs->cmd_ring,
			CMD_RING_RUNNING, 0, 5 * 1000 * 1000);
	if (ret < 0) {
		xhci_err(xhci, "Abort failed to stop command ring: %d\n", ret);
		xhci_halt(xhci);
		xhci_hc_died(xhci);
		return ret;
	}
	 
	spin_unlock_irqrestore(&xhci->lock, flags);
	ret = wait_for_completion_timeout(&xhci->cmd_ring_stop_completion,
					  msecs_to_jiffies(2000));
	spin_lock_irqsave(&xhci->lock, flags);
	if (!ret) {
		xhci_dbg(xhci, "No stop event for abort, ring start fail?\n");
		xhci_cleanup_command_queue(xhci);
	} else {
		xhci_handle_stopped_cmd_ring(xhci, xhci_next_queued_cmd(xhci));
	}
	return 0;
}

void xhci_ring_ep_doorbell(struct xhci_hcd *xhci,
		unsigned int slot_id,
		unsigned int ep_index,
		unsigned int stream_id)
{
	__le32 __iomem *db_addr = &xhci->dba->doorbell[slot_id];
	struct xhci_virt_ep *ep = &xhci->devs[slot_id]->eps[ep_index];
	unsigned int ep_state = ep->ep_state;

	 
	if ((ep_state & EP_STOP_CMD_PENDING) || (ep_state & SET_DEQ_PENDING) ||
	    (ep_state & EP_HALTED) || (ep_state & EP_CLEARING_TT))
		return;

	trace_xhci_ring_ep_doorbell(slot_id, DB_VALUE(ep_index, stream_id));

	writel(DB_VALUE(ep_index, stream_id), db_addr);
	 
	readl(db_addr);
}

 
static void ring_doorbell_for_active_rings(struct xhci_hcd *xhci,
		unsigned int slot_id,
		unsigned int ep_index)
{
	unsigned int stream_id;
	struct xhci_virt_ep *ep;

	ep = &xhci->devs[slot_id]->eps[ep_index];

	 
	if (!(ep->ep_state & EP_HAS_STREAMS)) {
		if (ep->ring && !(list_empty(&ep->ring->td_list)))
			xhci_ring_ep_doorbell(xhci, slot_id, ep_index, 0);
		return;
	}

	for (stream_id = 1; stream_id < ep->stream_info->num_streams;
			stream_id++) {
		struct xhci_stream_info *stream_info = ep->stream_info;
		if (!list_empty(&stream_info->stream_rings[stream_id]->td_list))
			xhci_ring_ep_doorbell(xhci, slot_id, ep_index,
						stream_id);
	}
}

void xhci_ring_doorbell_for_active_rings(struct xhci_hcd *xhci,
		unsigned int slot_id,
		unsigned int ep_index)
{
	ring_doorbell_for_active_rings(xhci, slot_id, ep_index);
}

static struct xhci_virt_ep *xhci_get_virt_ep(struct xhci_hcd *xhci,
					     unsigned int slot_id,
					     unsigned int ep_index)
{
	if (slot_id == 0 || slot_id >= MAX_HC_SLOTS) {
		xhci_warn(xhci, "Invalid slot_id %u\n", slot_id);
		return NULL;
	}
	if (ep_index >= EP_CTX_PER_DEV) {
		xhci_warn(xhci, "Invalid endpoint index %u\n", ep_index);
		return NULL;
	}
	if (!xhci->devs[slot_id]) {
		xhci_warn(xhci, "No xhci virt device for slot_id %u\n", slot_id);
		return NULL;
	}

	return &xhci->devs[slot_id]->eps[ep_index];
}

static struct xhci_ring *xhci_virt_ep_to_ring(struct xhci_hcd *xhci,
					      struct xhci_virt_ep *ep,
					      unsigned int stream_id)
{
	 
	if (!(ep->ep_state & EP_HAS_STREAMS))
		return ep->ring;

	if (!ep->stream_info)
		return NULL;

	if (stream_id == 0 || stream_id >= ep->stream_info->num_streams) {
		xhci_warn(xhci, "Invalid stream_id %u request for slot_id %u ep_index %u\n",
			  stream_id, ep->vdev->slot_id, ep->ep_index);
		return NULL;
	}

	return ep->stream_info->stream_rings[stream_id];
}

 
struct xhci_ring *xhci_triad_to_transfer_ring(struct xhci_hcd *xhci,
		unsigned int slot_id, unsigned int ep_index,
		unsigned int stream_id)
{
	struct xhci_virt_ep *ep;

	ep = xhci_get_virt_ep(xhci, slot_id, ep_index);
	if (!ep)
		return NULL;

	return xhci_virt_ep_to_ring(xhci, ep, stream_id);
}


 
static u64 xhci_get_hw_deq(struct xhci_hcd *xhci, struct xhci_virt_device *vdev,
			   unsigned int ep_index, unsigned int stream_id)
{
	struct xhci_ep_ctx *ep_ctx;
	struct xhci_stream_ctx *st_ctx;
	struct xhci_virt_ep *ep;

	ep = &vdev->eps[ep_index];

	if (ep->ep_state & EP_HAS_STREAMS) {
		st_ctx = &ep->stream_info->stream_ctx_array[stream_id];
		return le64_to_cpu(st_ctx->stream_ring);
	}
	ep_ctx = xhci_get_ep_ctx(xhci, vdev->out_ctx, ep_index);
	return le64_to_cpu(ep_ctx->deq);
}

static int xhci_move_dequeue_past_td(struct xhci_hcd *xhci,
				unsigned int slot_id, unsigned int ep_index,
				unsigned int stream_id, struct xhci_td *td)
{
	struct xhci_virt_device *dev = xhci->devs[slot_id];
	struct xhci_virt_ep *ep = &dev->eps[ep_index];
	struct xhci_ring *ep_ring;
	struct xhci_command *cmd;
	struct xhci_segment *new_seg;
	union xhci_trb *new_deq;
	int new_cycle;
	dma_addr_t addr;
	u64 hw_dequeue;
	bool cycle_found = false;
	bool td_last_trb_found = false;
	u32 trb_sct = 0;
	int ret;

	ep_ring = xhci_triad_to_transfer_ring(xhci, slot_id,
			ep_index, stream_id);
	if (!ep_ring) {
		xhci_warn(xhci, "WARN can't find new dequeue, invalid stream ID %u\n",
			  stream_id);
		return -ENODEV;
	}
	 
	if (!td) {
		if (list_empty(&ep_ring->td_list)) {
			new_seg = ep_ring->enq_seg;
			new_deq = ep_ring->enqueue;
			new_cycle = ep_ring->cycle_state;
			xhci_dbg(xhci, "ep ring empty, Set new dequeue = enqueue");
			goto deq_found;
		} else {
			xhci_warn(xhci, "Can't find new dequeue state, missing td\n");
			return -EINVAL;
		}
	}

	hw_dequeue = xhci_get_hw_deq(xhci, dev, ep_index, stream_id);
	new_seg = ep_ring->deq_seg;
	new_deq = ep_ring->dequeue;
	new_cycle = hw_dequeue & 0x1;

	 
	do {
		if (!cycle_found && xhci_trb_virt_to_dma(new_seg, new_deq)
		    == (dma_addr_t)(hw_dequeue & ~0xf)) {
			cycle_found = true;
			if (td_last_trb_found)
				break;
		}
		if (new_deq == td->last_trb)
			td_last_trb_found = true;

		if (cycle_found && trb_is_link(new_deq) &&
		    link_trb_toggles_cycle(new_deq))
			new_cycle ^= 0x1;

		next_trb(xhci, ep_ring, &new_seg, &new_deq);

		 
		if (new_deq == ep->ring->dequeue) {
			xhci_err(xhci, "Error: Failed finding new dequeue state\n");
			return -EINVAL;
		}

	} while (!cycle_found || !td_last_trb_found);

deq_found:

	 
	addr = xhci_trb_virt_to_dma(new_seg, new_deq);
	if (addr == 0) {
		xhci_warn(xhci, "Can't find dma of new dequeue ptr\n");
		xhci_warn(xhci, "deq seg = %p, deq ptr = %p\n", new_seg, new_deq);
		return -EINVAL;
	}

	if ((ep->ep_state & SET_DEQ_PENDING)) {
		xhci_warn(xhci, "Set TR Deq already pending, don't submit for 0x%pad\n",
			  &addr);
		return -EBUSY;
	}

	 
	cmd = xhci_alloc_command(xhci, false, GFP_ATOMIC);
	if (!cmd) {
		xhci_warn(xhci, "Can't alloc Set TR Deq cmd 0x%pad\n", &addr);
		return -ENOMEM;
	}

	if (stream_id)
		trb_sct = SCT_FOR_TRB(SCT_PRI_TR);
	ret = queue_command(xhci, cmd,
		lower_32_bits(addr) | trb_sct | new_cycle,
		upper_32_bits(addr),
		STREAM_ID_FOR_TRB(stream_id), SLOT_ID_FOR_TRB(slot_id) |
		EP_ID_FOR_TRB(ep_index) | TRB_TYPE(TRB_SET_DEQ), false);
	if (ret < 0) {
		xhci_free_command(xhci, cmd);
		return ret;
	}
	ep->queued_deq_seg = new_seg;
	ep->queued_deq_ptr = new_deq;

	xhci_dbg_trace(xhci, trace_xhci_dbg_cancel_urb,
		       "Set TR Deq ptr 0x%llx, cycle %u\n", addr, new_cycle);

	 
	ep->ep_state |= SET_DEQ_PENDING;
	xhci_ring_cmd_db(xhci);
	return 0;
}

 
static void td_to_noop(struct xhci_hcd *xhci, struct xhci_ring *ep_ring,
		       struct xhci_td *td, bool flip_cycle)
{
	struct xhci_segment *seg	= td->start_seg;
	union xhci_trb *trb		= td->first_trb;

	while (1) {
		trb_to_noop(trb, TRB_TR_NOOP);

		 
		if (flip_cycle && trb != td->first_trb && trb != td->last_trb)
			trb->generic.field[3] ^= cpu_to_le32(TRB_CYCLE);

		if (trb == td->last_trb)
			break;

		next_trb(xhci, ep_ring, &seg, &trb);
	}
}

 
static void xhci_giveback_urb_in_irq(struct xhci_hcd *xhci,
				     struct xhci_td *cur_td, int status)
{
	struct urb	*urb		= cur_td->urb;
	struct urb_priv	*urb_priv	= urb->hcpriv;
	struct usb_hcd	*hcd		= bus_to_hcd(urb->dev->bus);

	if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) {
		xhci_to_hcd(xhci)->self.bandwidth_isoc_reqs--;
		if (xhci_to_hcd(xhci)->self.bandwidth_isoc_reqs	== 0) {
			if (xhci->quirks & XHCI_AMD_PLL_FIX)
				usb_amd_quirk_pll_enable();
		}
	}
	xhci_urb_free_priv(urb_priv);
	usb_hcd_unlink_urb_from_ep(hcd, urb);
	trace_xhci_urb_giveback(urb);
	usb_hcd_giveback_urb(hcd, urb, status);
}

static void xhci_unmap_td_bounce_buffer(struct xhci_hcd *xhci,
		struct xhci_ring *ring, struct xhci_td *td)
{
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;
	struct xhci_segment *seg = td->bounce_seg;
	struct urb *urb = td->urb;
	size_t len;

	if (!ring || !seg || !urb)
		return;

	if (usb_urb_dir_out(urb)) {
		dma_unmap_single(dev, seg->bounce_dma, ring->bounce_buf_len,
				 DMA_TO_DEVICE);
		return;
	}

	dma_unmap_single(dev, seg->bounce_dma, ring->bounce_buf_len,
			 DMA_FROM_DEVICE);
	 
	if (urb->num_sgs) {
		len = sg_pcopy_from_buffer(urb->sg, urb->num_sgs, seg->bounce_buf,
					   seg->bounce_len, seg->bounce_offs);
		if (len != seg->bounce_len)
			xhci_warn(xhci, "WARN Wrong bounce buffer read length: %zu != %d\n",
				  len, seg->bounce_len);
	} else {
		memcpy(urb->transfer_buffer + seg->bounce_offs, seg->bounce_buf,
		       seg->bounce_len);
	}
	seg->bounce_len = 0;
	seg->bounce_offs = 0;
}

static int xhci_td_cleanup(struct xhci_hcd *xhci, struct xhci_td *td,
			   struct xhci_ring *ep_ring, int status)
{
	struct urb *urb = NULL;

	 
	urb = td->urb;

	 
	xhci_unmap_td_bounce_buffer(xhci, ep_ring, td);

	 
	if (urb->actual_length > urb->transfer_buffer_length) {
		xhci_warn(xhci, "URB req %u and actual %u transfer length mismatch\n",
			  urb->transfer_buffer_length, urb->actual_length);
		urb->actual_length = 0;
		status = 0;
	}
	 
	if (!list_empty(&td->td_list))
		list_del_init(&td->td_list);
	 
	if (!list_empty(&td->cancelled_td_list))
		list_del_init(&td->cancelled_td_list);

	inc_td_cnt(urb);
	 
	if (last_td_in_urb(td)) {
		if ((urb->actual_length != urb->transfer_buffer_length &&
		     (urb->transfer_flags & URB_SHORT_NOT_OK)) ||
		    (status != 0 && !usb_endpoint_xfer_isoc(&urb->ep->desc)))
			xhci_dbg(xhci, "Giveback URB %p, len = %d, expected = %d, status = %d\n",
				 urb, urb->actual_length,
				 urb->transfer_buffer_length, status);

		 
		if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS)
			status = 0;
		xhci_giveback_urb_in_irq(xhci, td, status);
	}

	return 0;
}


 
static void xhci_giveback_invalidated_tds(struct xhci_virt_ep *ep)
{
	struct xhci_ring *ring;
	struct xhci_td *td, *tmp_td;

	list_for_each_entry_safe(td, tmp_td, &ep->cancelled_td_list,
				 cancelled_td_list) {

		ring = xhci_urb_to_transfer_ring(ep->xhci, td->urb);

		if (td->cancel_status == TD_CLEARED) {
			xhci_dbg(ep->xhci, "%s: Giveback cancelled URB %p TD\n",
				 __func__, td->urb);
			xhci_td_cleanup(ep->xhci, td, ring, td->status);
		} else {
			xhci_dbg(ep->xhci, "%s: Keep cancelled URB %p TD as cancel_status is %d\n",
				 __func__, td->urb, td->cancel_status);
		}
		if (ep->xhci->xhc_state & XHCI_STATE_DYING)
			return;
	}
}

static int xhci_reset_halted_ep(struct xhci_hcd *xhci, unsigned int slot_id,
				unsigned int ep_index, enum xhci_ep_reset_type reset_type)
{
	struct xhci_command *command;
	int ret = 0;

	command = xhci_alloc_command(xhci, false, GFP_ATOMIC);
	if (!command) {
		ret = -ENOMEM;
		goto done;
	}

	xhci_dbg(xhci, "%s-reset ep %u, slot %u\n",
		 (reset_type == EP_HARD_RESET) ? "Hard" : "Soft",
		 ep_index, slot_id);

	ret = xhci_queue_reset_ep(xhci, command, slot_id, ep_index, reset_type);
done:
	if (ret)
		xhci_err(xhci, "ERROR queuing reset endpoint for slot %d ep_index %d, %d\n",
			 slot_id, ep_index, ret);
	return ret;
}

static int xhci_handle_halted_endpoint(struct xhci_hcd *xhci,
				struct xhci_virt_ep *ep,
				struct xhci_td *td,
				enum xhci_ep_reset_type reset_type)
{
	unsigned int slot_id = ep->vdev->slot_id;
	int err;

	 
	if (ep->vdev->flags & VDEV_PORT_ERROR)
		return -ENODEV;

	 
	if (reset_type == EP_HARD_RESET) {
		ep->ep_state |= EP_HARD_CLEAR_TOGGLE;
		if (td && list_empty(&td->cancelled_td_list)) {
			list_add_tail(&td->cancelled_td_list, &ep->cancelled_td_list);
			td->cancel_status = TD_HALTED;
		}
	}

	if (ep->ep_state & EP_HALTED) {
		xhci_dbg(xhci, "Reset ep command for ep_index %d already pending\n",
			 ep->ep_index);
		return 0;
	}

	err = xhci_reset_halted_ep(xhci, slot_id, ep->ep_index, reset_type);
	if (err)
		return err;

	ep->ep_state |= EP_HALTED;

	xhci_ring_cmd_db(xhci);

	return 0;
}

 

static int xhci_invalidate_cancelled_tds(struct xhci_virt_ep *ep)
{
	struct xhci_hcd		*xhci;
	struct xhci_td		*td = NULL;
	struct xhci_td		*tmp_td = NULL;
	struct xhci_td		*cached_td = NULL;
	struct xhci_ring	*ring;
	u64			hw_deq;
	unsigned int		slot_id = ep->vdev->slot_id;
	int			err;

	xhci = ep->xhci;

	list_for_each_entry_safe(td, tmp_td, &ep->cancelled_td_list, cancelled_td_list) {
		xhci_dbg_trace(xhci, trace_xhci_dbg_cancel_urb,
			       "Removing canceled TD starting at 0x%llx (dma) in stream %u URB %p",
			       (unsigned long long)xhci_trb_virt_to_dma(
				       td->start_seg, td->first_trb),
			       td->urb->stream_id, td->urb);
		list_del_init(&td->td_list);
		ring = xhci_urb_to_transfer_ring(xhci, td->urb);
		if (!ring) {
			xhci_warn(xhci, "WARN Cancelled URB %p has invalid stream ID %u.\n",
				  td->urb, td->urb->stream_id);
			continue;
		}
		 
		hw_deq = xhci_get_hw_deq(xhci, ep->vdev, ep->ep_index,
					 td->urb->stream_id);
		hw_deq &= ~0xf;

		if (td->cancel_status == TD_HALTED ||
		    trb_in_td(xhci, td->start_seg, td->first_trb, td->last_trb, hw_deq, false)) {
			switch (td->cancel_status) {
			case TD_CLEARED:  
			case TD_CLEARING_CACHE:  
				break;
			case TD_DIRTY:  
			case TD_HALTED:
				td->cancel_status = TD_CLEARING_CACHE;
				if (cached_td)
					 
					xhci_dbg(xhci,
						 "Move dq past stream %u URB %p instead of stream %u URB %p\n",
						 td->urb->stream_id, td->urb,
						 cached_td->urb->stream_id, cached_td->urb);
				cached_td = td;
				break;
			}
		} else {
			td_to_noop(xhci, ring, td, false);
			td->cancel_status = TD_CLEARED;
		}
	}

	 
	if (!cached_td)
		return 0;

	err = xhci_move_dequeue_past_td(xhci, slot_id, ep->ep_index,
					cached_td->urb->stream_id,
					cached_td);
	if (err) {
		 
		list_for_each_entry_safe(td, tmp_td, &ep->cancelled_td_list, cancelled_td_list) {
			if (td->cancel_status != TD_CLEARING_CACHE)
				continue;
			xhci_dbg(xhci, "Failed to clear cancelled cached URB %p, mark clear anyway\n",
				 td->urb);
			td_to_noop(xhci, ring, td, false);
			td->cancel_status = TD_CLEARED;
		}
	}
	return 0;
}

 
static struct xhci_td *find_halted_td(struct xhci_virt_ep *ep)
{
	struct xhci_td	*td;
	u64		hw_deq;

	if (!list_empty(&ep->ring->td_list)) {  
		hw_deq = xhci_get_hw_deq(ep->xhci, ep->vdev, ep->ep_index, 0);
		hw_deq &= ~0xf;
		td = list_first_entry(&ep->ring->td_list, struct xhci_td, td_list);
		if (trb_in_td(ep->xhci, td->start_seg, td->first_trb,
				td->last_trb, hw_deq, false))
			return td;
	}
	return NULL;
}

 
static void xhci_handle_cmd_stop_ep(struct xhci_hcd *xhci, int slot_id,
				    union xhci_trb *trb, u32 comp_code)
{
	unsigned int ep_index;
	struct xhci_virt_ep *ep;
	struct xhci_ep_ctx *ep_ctx;
	struct xhci_td *td = NULL;
	enum xhci_ep_reset_type reset_type;
	struct xhci_command *command;
	int err;

	if (unlikely(TRB_TO_SUSPEND_PORT(le32_to_cpu(trb->generic.field[3])))) {
		if (!xhci->devs[slot_id])
			xhci_warn(xhci, "Stop endpoint command completion for disabled slot %u\n",
				  slot_id);
		return;
	}

	ep_index = TRB_TO_EP_INDEX(le32_to_cpu(trb->generic.field[3]));
	ep = xhci_get_virt_ep(xhci, slot_id, ep_index);
	if (!ep)
		return;

	ep_ctx = xhci_get_ep_ctx(xhci, ep->vdev->out_ctx, ep_index);

	trace_xhci_handle_cmd_stop_ep(ep_ctx);

	if (comp_code == COMP_CONTEXT_STATE_ERROR) {
	 
		switch (GET_EP_CTX_STATE(ep_ctx)) {
		case EP_STATE_HALTED:
			xhci_dbg(xhci, "Stop ep completion raced with stall, reset ep\n");
			if (ep->ep_state & EP_HAS_STREAMS) {
				reset_type = EP_SOFT_RESET;
			} else {
				reset_type = EP_HARD_RESET;
				td = find_halted_td(ep);
				if (td)
					td->status = -EPROTO;
			}
			 
			err = xhci_handle_halted_endpoint(xhci, ep, td, reset_type);
			if (err)
				break;
			ep->ep_state &= ~EP_STOP_CMD_PENDING;
			return;
		case EP_STATE_RUNNING:
			 
			xhci_dbg(xhci, "Stop ep completion ctx error, ep is running\n");

			command = xhci_alloc_command(xhci, false, GFP_ATOMIC);
			if (!command) {
				ep->ep_state &= ~EP_STOP_CMD_PENDING;
				return;
			}
			xhci_queue_stop_endpoint(xhci, command, slot_id, ep_index, 0);
			xhci_ring_cmd_db(xhci);

			return;
		default:
			break;
		}
	}

	 
	xhci_invalidate_cancelled_tds(ep);
	ep->ep_state &= ~EP_STOP_CMD_PENDING;

	 
	xhci_giveback_invalidated_tds(ep);
	ring_doorbell_for_active_rings(xhci, slot_id, ep_index);
}

static void xhci_kill_ring_urbs(struct xhci_hcd *xhci, struct xhci_ring *ring)
{
	struct xhci_td *cur_td;
	struct xhci_td *tmp;

	list_for_each_entry_safe(cur_td, tmp, &ring->td_list, td_list) {
		list_del_init(&cur_td->td_list);

		if (!list_empty(&cur_td->cancelled_td_list))
			list_del_init(&cur_td->cancelled_td_list);

		xhci_unmap_td_bounce_buffer(xhci, ring, cur_td);

		inc_td_cnt(cur_td->urb);
		if (last_td_in_urb(cur_td))
			xhci_giveback_urb_in_irq(xhci, cur_td, -ESHUTDOWN);
	}
}

static void xhci_kill_endpoint_urbs(struct xhci_hcd *xhci,
		int slot_id, int ep_index)
{
	struct xhci_td *cur_td;
	struct xhci_td *tmp;
	struct xhci_virt_ep *ep;
	struct xhci_ring *ring;

	ep = xhci_get_virt_ep(xhci, slot_id, ep_index);
	if (!ep)
		return;

	if ((ep->ep_state & EP_HAS_STREAMS) ||
			(ep->ep_state & EP_GETTING_NO_STREAMS)) {
		int stream_id;

		for (stream_id = 1; stream_id < ep->stream_info->num_streams;
				stream_id++) {
			ring = ep->stream_info->stream_rings[stream_id];
			if (!ring)
				continue;

			xhci_dbg_trace(xhci, trace_xhci_dbg_cancel_urb,
					"Killing URBs for slot ID %u, ep index %u, stream %u",
					slot_id, ep_index, stream_id);
			xhci_kill_ring_urbs(xhci, ring);
		}
	} else {
		ring = ep->ring;
		if (!ring)
			return;
		xhci_dbg_trace(xhci, trace_xhci_dbg_cancel_urb,
				"Killing URBs for slot ID %u, ep index %u",
				slot_id, ep_index);
		xhci_kill_ring_urbs(xhci, ring);
	}

	list_for_each_entry_safe(cur_td, tmp, &ep->cancelled_td_list,
			cancelled_td_list) {
		list_del_init(&cur_td->cancelled_td_list);
		inc_td_cnt(cur_td->urb);

		if (last_td_in_urb(cur_td))
			xhci_giveback_urb_in_irq(xhci, cur_td, -ESHUTDOWN);
	}
}

 
void xhci_hc_died(struct xhci_hcd *xhci)
{
	int i, j;

	if (xhci->xhc_state & XHCI_STATE_DYING)
		return;

	xhci_err(xhci, "xHCI host controller not responding, assume dead\n");
	xhci->xhc_state |= XHCI_STATE_DYING;

	xhci_cleanup_command_queue(xhci);

	 
	for (i = 0; i <= HCS_MAX_SLOTS(xhci->hcs_params1); i++) {
		if (!xhci->devs[i])
			continue;
		for (j = 0; j < 31; j++)
			xhci_kill_endpoint_urbs(xhci, i, j);
	}

	 
	if (!(xhci->xhc_state & XHCI_STATE_REMOVING))
		usb_hc_died(xhci_to_hcd(xhci));
}

static void update_ring_for_set_deq_completion(struct xhci_hcd *xhci,
		struct xhci_virt_device *dev,
		struct xhci_ring *ep_ring,
		unsigned int ep_index)
{
	union xhci_trb *dequeue_temp;

	dequeue_temp = ep_ring->dequeue;

	 
	if (trb_is_link(ep_ring->dequeue)) {
		ep_ring->deq_seg = ep_ring->deq_seg->next;
		ep_ring->dequeue = ep_ring->deq_seg->trbs;
	}

	while (ep_ring->dequeue != dev->eps[ep_index].queued_deq_ptr) {
		 
		ep_ring->dequeue++;
		if (trb_is_link(ep_ring->dequeue)) {
			if (ep_ring->dequeue ==
					dev->eps[ep_index].queued_deq_ptr)
				break;
			ep_ring->deq_seg = ep_ring->deq_seg->next;
			ep_ring->dequeue = ep_ring->deq_seg->trbs;
		}
		if (ep_ring->dequeue == dequeue_temp) {
			xhci_dbg(xhci, "Unable to find new dequeue pointer\n");
			break;
		}
	}
}

 
static void xhci_handle_cmd_set_deq(struct xhci_hcd *xhci, int slot_id,
		union xhci_trb *trb, u32 cmd_comp_code)
{
	unsigned int ep_index;
	unsigned int stream_id;
	struct xhci_ring *ep_ring;
	struct xhci_virt_ep *ep;
	struct xhci_ep_ctx *ep_ctx;
	struct xhci_slot_ctx *slot_ctx;
	struct xhci_td *td, *tmp_td;

	ep_index = TRB_TO_EP_INDEX(le32_to_cpu(trb->generic.field[3]));
	stream_id = TRB_TO_STREAM_ID(le32_to_cpu(trb->generic.field[2]));
	ep = xhci_get_virt_ep(xhci, slot_id, ep_index);
	if (!ep)
		return;

	ep_ring = xhci_virt_ep_to_ring(xhci, ep, stream_id);
	if (!ep_ring) {
		xhci_warn(xhci, "WARN Set TR deq ptr command for freed stream ID %u\n",
				stream_id);
		 
		goto cleanup;
	}

	ep_ctx = xhci_get_ep_ctx(xhci, ep->vdev->out_ctx, ep_index);
	slot_ctx = xhci_get_slot_ctx(xhci, ep->vdev->out_ctx);
	trace_xhci_handle_cmd_set_deq(slot_ctx);
	trace_xhci_handle_cmd_set_deq_ep(ep_ctx);

	if (cmd_comp_code != COMP_SUCCESS) {
		unsigned int ep_state;
		unsigned int slot_state;

		switch (cmd_comp_code) {
		case COMP_TRB_ERROR:
			xhci_warn(xhci, "WARN Set TR Deq Ptr cmd invalid because of stream ID configuration\n");
			break;
		case COMP_CONTEXT_STATE_ERROR:
			xhci_warn(xhci, "WARN Set TR Deq Ptr cmd failed due to incorrect slot or ep state.\n");
			ep_state = GET_EP_CTX_STATE(ep_ctx);
			slot_state = le32_to_cpu(slot_ctx->dev_state);
			slot_state = GET_SLOT_STATE(slot_state);
			xhci_dbg_trace(xhci, trace_xhci_dbg_cancel_urb,
					"Slot state = %u, EP state = %u",
					slot_state, ep_state);
			break;
		case COMP_SLOT_NOT_ENABLED_ERROR:
			xhci_warn(xhci, "WARN Set TR Deq Ptr cmd failed because slot %u was not enabled.\n",
					slot_id);
			break;
		default:
			xhci_warn(xhci, "WARN Set TR Deq Ptr cmd with unknown completion code of %u.\n",
					cmd_comp_code);
			break;
		}
		 
	} else {
		u64 deq;
		 
		if (ep->ep_state & EP_HAS_STREAMS) {
			struct xhci_stream_ctx *ctx =
				&ep->stream_info->stream_ctx_array[stream_id];
			deq = le64_to_cpu(ctx->stream_ring) & SCTX_DEQ_MASK;
		} else {
			deq = le64_to_cpu(ep_ctx->deq) & ~EP_CTX_CYCLE_MASK;
		}
		xhci_dbg_trace(xhci, trace_xhci_dbg_cancel_urb,
			"Successful Set TR Deq Ptr cmd, deq = @%08llx", deq);
		if (xhci_trb_virt_to_dma(ep->queued_deq_seg,
					 ep->queued_deq_ptr) == deq) {
			 
			update_ring_for_set_deq_completion(xhci, ep->vdev,
				ep_ring, ep_index);
		} else {
			xhci_warn(xhci, "Mismatch between completed Set TR Deq Ptr command & xHCI internal state.\n");
			xhci_warn(xhci, "ep deq seg = %p, deq ptr = %p\n",
				  ep->queued_deq_seg, ep->queued_deq_ptr);
		}
	}
	 
	list_for_each_entry_safe(td, tmp_td, &ep->cancelled_td_list,
				 cancelled_td_list) {
		ep_ring = xhci_urb_to_transfer_ring(ep->xhci, td->urb);
		if (td->cancel_status == TD_CLEARING_CACHE) {
			td->cancel_status = TD_CLEARED;
			xhci_dbg(ep->xhci, "%s: Giveback cancelled URB %p TD\n",
				 __func__, td->urb);
			xhci_td_cleanup(ep->xhci, td, ep_ring, td->status);
		} else {
			xhci_dbg(ep->xhci, "%s: Keep cancelled URB %p TD as cancel_status is %d\n",
				 __func__, td->urb, td->cancel_status);
		}
	}
cleanup:
	ep->ep_state &= ~SET_DEQ_PENDING;
	ep->queued_deq_seg = NULL;
	ep->queued_deq_ptr = NULL;
	 
	ring_doorbell_for_active_rings(xhci, slot_id, ep_index);
}

static void xhci_handle_cmd_reset_ep(struct xhci_hcd *xhci, int slot_id,
		union xhci_trb *trb, u32 cmd_comp_code)
{
	struct xhci_virt_ep *ep;
	struct xhci_ep_ctx *ep_ctx;
	unsigned int ep_index;

	ep_index = TRB_TO_EP_INDEX(le32_to_cpu(trb->generic.field[3]));
	ep = xhci_get_virt_ep(xhci, slot_id, ep_index);
	if (!ep)
		return;

	ep_ctx = xhci_get_ep_ctx(xhci, ep->vdev->out_ctx, ep_index);
	trace_xhci_handle_cmd_reset_ep(ep_ctx);

	 
	xhci_dbg_trace(xhci, trace_xhci_dbg_reset_ep,
		"Ignoring reset ep completion code of %u", cmd_comp_code);

	 
	xhci_invalidate_cancelled_tds(ep);

	 
	ep->ep_state &= ~EP_HALTED;

	xhci_giveback_invalidated_tds(ep);

	 
	if ((le32_to_cpu(trb->generic.field[3])) & TRB_TSP)
		ring_doorbell_for_active_rings(xhci, slot_id, ep_index);
}

static void xhci_handle_cmd_enable_slot(struct xhci_hcd *xhci, int slot_id,
		struct xhci_command *command, u32 cmd_comp_code)
{
	if (cmd_comp_code == COMP_SUCCESS)
		command->slot_id = slot_id;
	else
		command->slot_id = 0;
}

static void xhci_handle_cmd_disable_slot(struct xhci_hcd *xhci, int slot_id)
{
	struct xhci_virt_device *virt_dev;
	struct xhci_slot_ctx *slot_ctx;

	virt_dev = xhci->devs[slot_id];
	if (!virt_dev)
		return;

	slot_ctx = xhci_get_slot_ctx(xhci, virt_dev->out_ctx);
	trace_xhci_handle_cmd_disable_slot(slot_ctx);

	if (xhci->quirks & XHCI_EP_LIMIT_QUIRK)
		 
		xhci_free_device_endpoint_resources(xhci, virt_dev, true);
}

static void xhci_handle_cmd_config_ep(struct xhci_hcd *xhci, int slot_id,
		u32 cmd_comp_code)
{
	struct xhci_virt_device *virt_dev;
	struct xhci_input_control_ctx *ctrl_ctx;
	struct xhci_ep_ctx *ep_ctx;
	unsigned int ep_index;
	u32 add_flags;

	 

	virt_dev = xhci->devs[slot_id];
	if (!virt_dev)
		return;
	ctrl_ctx = xhci_get_input_control_ctx(virt_dev->in_ctx);
	if (!ctrl_ctx) {
		xhci_warn(xhci, "Could not get input context, bad type.\n");
		return;
	}

	add_flags = le32_to_cpu(ctrl_ctx->add_flags);

	 
	ep_index = xhci_last_valid_endpoint(add_flags) - 1;

	ep_ctx = xhci_get_ep_ctx(xhci, virt_dev->out_ctx, ep_index);
	trace_xhci_handle_cmd_config_ep(ep_ctx);

	return;
}

static void xhci_handle_cmd_addr_dev(struct xhci_hcd *xhci, int slot_id)
{
	struct xhci_virt_device *vdev;
	struct xhci_slot_ctx *slot_ctx;

	vdev = xhci->devs[slot_id];
	if (!vdev)
		return;
	slot_ctx = xhci_get_slot_ctx(xhci, vdev->out_ctx);
	trace_xhci_handle_cmd_addr_dev(slot_ctx);
}

static void xhci_handle_cmd_reset_dev(struct xhci_hcd *xhci, int slot_id)
{
	struct xhci_virt_device *vdev;
	struct xhci_slot_ctx *slot_ctx;

	vdev = xhci->devs[slot_id];
	if (!vdev) {
		xhci_warn(xhci, "Reset device command completion for disabled slot %u\n",
			  slot_id);
		return;
	}
	slot_ctx = xhci_get_slot_ctx(xhci, vdev->out_ctx);
	trace_xhci_handle_cmd_reset_dev(slot_ctx);

	xhci_dbg(xhci, "Completed reset device command.\n");
}

static void xhci_handle_cmd_nec_get_fw(struct xhci_hcd *xhci,
		struct xhci_event_cmd *event)
{
	if (!(xhci->quirks & XHCI_NEC_HOST)) {
		xhci_warn(xhci, "WARN NEC_GET_FW command on non-NEC host\n");
		return;
	}
	xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
			"NEC firmware version %2x.%02x",
			NEC_FW_MAJOR(le32_to_cpu(event->status)),
			NEC_FW_MINOR(le32_to_cpu(event->status)));
}

static void xhci_complete_del_and_free_cmd(struct xhci_command *cmd, u32 status)
{
	list_del(&cmd->cmd_list);

	if (cmd->completion) {
		cmd->status = status;
		complete(cmd->completion);
	} else {
		kfree(cmd);
	}
}

void xhci_cleanup_command_queue(struct xhci_hcd *xhci)
{
	struct xhci_command *cur_cmd, *tmp_cmd;
	xhci->current_cmd = NULL;
	list_for_each_entry_safe(cur_cmd, tmp_cmd, &xhci->cmd_list, cmd_list)
		xhci_complete_del_and_free_cmd(cur_cmd, COMP_COMMAND_ABORTED);
}

void xhci_handle_command_timeout(struct work_struct *work)
{
	struct xhci_hcd	*xhci;
	unsigned long	flags;
	char		str[XHCI_MSG_MAX];
	u64		hw_ring_state;
	u32		cmd_field3;
	u32		usbsts;

	xhci = container_of(to_delayed_work(work), struct xhci_hcd, cmd_timer);

	spin_lock_irqsave(&xhci->lock, flags);

	 
	if (!xhci->current_cmd || delayed_work_pending(&xhci->cmd_timer)) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		return;
	}

	cmd_field3 = le32_to_cpu(xhci->current_cmd->command_trb->generic.field[3]);
	usbsts = readl(&xhci->op_regs->status);
	xhci_dbg(xhci, "Command timeout, USBSTS:%s\n", xhci_decode_usbsts(str, usbsts));

	 
	if (TRB_FIELD_TO_TYPE(cmd_field3) == TRB_STOP_RING) {
		struct xhci_virt_ep	*ep;

		xhci_warn(xhci, "xHCI host not responding to stop endpoint command\n");

		ep = xhci_get_virt_ep(xhci, TRB_TO_SLOT_ID(cmd_field3),
				      TRB_TO_EP_INDEX(cmd_field3));
		if (ep)
			ep->ep_state &= ~EP_STOP_CMD_PENDING;

		xhci_halt(xhci);
		xhci_hc_died(xhci);
		goto time_out_completed;
	}

	 
	xhci->current_cmd->status = COMP_COMMAND_ABORTED;

	 
	hw_ring_state = xhci_read_64(xhci, &xhci->op_regs->cmd_ring);
	if (hw_ring_state == ~(u64)0) {
		xhci_hc_died(xhci);
		goto time_out_completed;
	}

	if ((xhci->cmd_ring_state & CMD_RING_STATE_RUNNING) &&
	    (hw_ring_state & CMD_RING_RUNNING))  {
		 
		xhci->cmd_ring_state = CMD_RING_STATE_ABORTED;
		xhci_dbg(xhci, "Command timeout\n");
		xhci_abort_cmd_ring(xhci, flags);
		goto time_out_completed;
	}

	 
	if (xhci->xhc_state & XHCI_STATE_REMOVING) {
		xhci_dbg(xhci, "host removed, ring start fail?\n");
		xhci_cleanup_command_queue(xhci);

		goto time_out_completed;
	}

	 
	xhci_dbg(xhci, "Command timeout on stopped ring\n");
	xhci_handle_stopped_cmd_ring(xhci, xhci->current_cmd);

time_out_completed:
	spin_unlock_irqrestore(&xhci->lock, flags);
	return;
}

static void handle_cmd_completion(struct xhci_hcd *xhci,
		struct xhci_event_cmd *event)
{
	unsigned int slot_id = TRB_TO_SLOT_ID(le32_to_cpu(event->flags));
	u64 cmd_dma;
	dma_addr_t cmd_dequeue_dma;
	u32 cmd_comp_code;
	union xhci_trb *cmd_trb;
	struct xhci_command *cmd;
	u32 cmd_type;

	if (slot_id >= MAX_HC_SLOTS) {
		xhci_warn(xhci, "Invalid slot_id %u\n", slot_id);
		return;
	}

	cmd_dma = le64_to_cpu(event->cmd_trb);
	cmd_trb = xhci->cmd_ring->dequeue;

	trace_xhci_handle_command(xhci->cmd_ring, &cmd_trb->generic);

	cmd_dequeue_dma = xhci_trb_virt_to_dma(xhci->cmd_ring->deq_seg,
			cmd_trb);
	 
	if (!cmd_dequeue_dma || cmd_dma != (u64)cmd_dequeue_dma) {
		xhci_warn(xhci,
			  "ERROR mismatched command completion event\n");
		return;
	}

	cmd = list_first_entry(&xhci->cmd_list, struct xhci_command, cmd_list);

	cancel_delayed_work(&xhci->cmd_timer);

	cmd_comp_code = GET_COMP_CODE(le32_to_cpu(event->status));

	 
	if (cmd_comp_code == COMP_COMMAND_RING_STOPPED) {
		complete_all(&xhci->cmd_ring_stop_completion);
		return;
	}

	if (cmd->command_trb != xhci->cmd_ring->dequeue) {
		xhci_err(xhci,
			 "Command completion event does not match command\n");
		return;
	}

	 
	if (cmd_comp_code == COMP_COMMAND_ABORTED) {
		xhci->cmd_ring_state = CMD_RING_STATE_STOPPED;
		if (cmd->status == COMP_COMMAND_ABORTED) {
			if (xhci->current_cmd == cmd)
				xhci->current_cmd = NULL;
			goto event_handled;
		}
	}

	cmd_type = TRB_FIELD_TO_TYPE(le32_to_cpu(cmd_trb->generic.field[3]));
	switch (cmd_type) {
	case TRB_ENABLE_SLOT:
		xhci_handle_cmd_enable_slot(xhci, slot_id, cmd, cmd_comp_code);
		break;
	case TRB_DISABLE_SLOT:
		xhci_handle_cmd_disable_slot(xhci, slot_id);
		break;
	case TRB_CONFIG_EP:
		if (!cmd->completion)
			xhci_handle_cmd_config_ep(xhci, slot_id, cmd_comp_code);
		break;
	case TRB_EVAL_CONTEXT:
		break;
	case TRB_ADDR_DEV:
		xhci_handle_cmd_addr_dev(xhci, slot_id);
		break;
	case TRB_STOP_RING:
		WARN_ON(slot_id != TRB_TO_SLOT_ID(
				le32_to_cpu(cmd_trb->generic.field[3])));
		if (!cmd->completion)
			xhci_handle_cmd_stop_ep(xhci, slot_id, cmd_trb,
						cmd_comp_code);
		break;
	case TRB_SET_DEQ:
		WARN_ON(slot_id != TRB_TO_SLOT_ID(
				le32_to_cpu(cmd_trb->generic.field[3])));
		xhci_handle_cmd_set_deq(xhci, slot_id, cmd_trb, cmd_comp_code);
		break;
	case TRB_CMD_NOOP:
		 
		if (cmd->status == COMP_COMMAND_RING_STOPPED)
			cmd_comp_code = COMP_COMMAND_RING_STOPPED;
		break;
	case TRB_RESET_EP:
		WARN_ON(slot_id != TRB_TO_SLOT_ID(
				le32_to_cpu(cmd_trb->generic.field[3])));
		xhci_handle_cmd_reset_ep(xhci, slot_id, cmd_trb, cmd_comp_code);
		break;
	case TRB_RESET_DEV:
		 
		slot_id = TRB_TO_SLOT_ID(
				le32_to_cpu(cmd_trb->generic.field[3]));
		xhci_handle_cmd_reset_dev(xhci, slot_id);
		break;
	case TRB_NEC_GET_FW:
		xhci_handle_cmd_nec_get_fw(xhci, event);
		break;
	default:
		 
		xhci_info(xhci, "INFO unknown command type %d\n", cmd_type);
		break;
	}

	 
	if (!list_is_singular(&xhci->cmd_list)) {
		xhci->current_cmd = list_first_entry(&cmd->cmd_list,
						struct xhci_command, cmd_list);
		xhci_mod_cmd_timer(xhci, XHCI_CMD_DEFAULT_TIMEOUT);
	} else if (xhci->current_cmd == cmd) {
		xhci->current_cmd = NULL;
	}

event_handled:
	xhci_complete_del_and_free_cmd(cmd, cmd_comp_code);

	inc_deq(xhci, xhci->cmd_ring);
}

static void handle_vendor_event(struct xhci_hcd *xhci,
				union xhci_trb *event, u32 trb_type)
{
	xhci_dbg(xhci, "Vendor specific event TRB type = %u\n", trb_type);
	if (trb_type == TRB_NEC_CMD_COMP && (xhci->quirks & XHCI_NEC_HOST))
		handle_cmd_completion(xhci, &event->event_cmd);
}

static void handle_device_notification(struct xhci_hcd *xhci,
		union xhci_trb *event)
{
	u32 slot_id;
	struct usb_device *udev;

	slot_id = TRB_TO_SLOT_ID(le32_to_cpu(event->generic.field[3]));
	if (!xhci->devs[slot_id]) {
		xhci_warn(xhci, "Device Notification event for "
				"unused slot %u\n", slot_id);
		return;
	}

	xhci_dbg(xhci, "Device Wake Notification event for slot ID %u\n",
			slot_id);
	udev = xhci->devs[slot_id]->udev;
	if (udev && udev->parent)
		usb_wakeup_notification(udev->parent, udev->portnum);
}

 
static void xhci_cavium_reset_phy_quirk(struct xhci_hcd *xhci)
{
	struct usb_hcd *hcd = xhci_to_hcd(xhci);
	u32 pll_lock_check;
	u32 retry_count = 4;

	do {
		 
		writel(0x6F, hcd->regs + 0x1048);
		udelay(10);
		 
		writel(0x7F, hcd->regs + 0x1048);
		udelay(200);
		pll_lock_check = readl(hcd->regs + 0x1070);
	} while (!(pll_lock_check & 0x1) && --retry_count);
}

static void handle_port_status(struct xhci_hcd *xhci,
			       struct xhci_interrupter *ir,
			       union xhci_trb *event)
{
	struct usb_hcd *hcd;
	u32 port_id;
	u32 portsc, cmd_reg;
	int max_ports;
	int slot_id;
	unsigned int hcd_portnum;
	struct xhci_bus_state *bus_state;
	bool bogus_port_status = false;
	struct xhci_port *port;

	 
	if (GET_COMP_CODE(le32_to_cpu(event->generic.field[2])) != COMP_SUCCESS)
		xhci_warn(xhci,
			  "WARN: xHC returned failed port status event\n");

	port_id = GET_PORT_ID(le32_to_cpu(event->generic.field[0]));
	max_ports = HCS_MAX_PORTS(xhci->hcs_params1);

	if ((port_id <= 0) || (port_id > max_ports)) {
		xhci_warn(xhci, "Port change event with invalid port ID %d\n",
			  port_id);
		inc_deq(xhci, ir->event_ring);
		return;
	}

	port = &xhci->hw_ports[port_id - 1];
	if (!port || !port->rhub || port->hcd_portnum == DUPLICATE_ENTRY) {
		xhci_warn(xhci, "Port change event, no port for port ID %u\n",
			  port_id);
		bogus_port_status = true;
		goto cleanup;
	}

	 
	if (port->rhub == &xhci->usb3_rhub && xhci->shared_hcd == NULL) {
		xhci_dbg(xhci, "ignore port event for removed USB3 hcd\n");
		bogus_port_status = true;
		goto cleanup;
	}

	hcd = port->rhub->hcd;
	bus_state = &port->rhub->bus_state;
	hcd_portnum = port->hcd_portnum;
	portsc = readl(port->addr);

	xhci_dbg(xhci, "Port change event, %d-%d, id %d, portsc: 0x%x\n",
		 hcd->self.busnum, hcd_portnum + 1, port_id, portsc);

	trace_xhci_handle_port_status(hcd_portnum, portsc);

	if (hcd->state == HC_STATE_SUSPENDED) {
		xhci_dbg(xhci, "resume root hub\n");
		usb_hcd_resume_root_hub(hcd);
	}

	if (hcd->speed >= HCD_USB3 &&
	    (portsc & PORT_PLS_MASK) == XDEV_INACTIVE) {
		slot_id = xhci_find_slot_id_by_port(hcd, xhci, hcd_portnum + 1);
		if (slot_id && xhci->devs[slot_id])
			xhci->devs[slot_id]->flags |= VDEV_PORT_ERROR;
	}

	if ((portsc & PORT_PLC) && (portsc & PORT_PLS_MASK) == XDEV_RESUME) {
		xhci_dbg(xhci, "port resume event for port %d\n", port_id);

		cmd_reg = readl(&xhci->op_regs->command);
		if (!(cmd_reg & CMD_RUN)) {
			xhci_warn(xhci, "xHC is not running.\n");
			goto cleanup;
		}

		if (DEV_SUPERSPEED_ANY(portsc)) {
			xhci_dbg(xhci, "remote wake SS port %d\n", port_id);
			 
			bus_state->port_remote_wakeup |= 1 << hcd_portnum;
			xhci_test_and_clear_bit(xhci, port, PORT_PLC);
			usb_hcd_start_port_resume(&hcd->self, hcd_portnum);
			xhci_set_link_state(xhci, port, XDEV_U0);
			 
			bogus_port_status = true;
			goto cleanup;
		} else if (!test_bit(hcd_portnum, &bus_state->resuming_ports)) {
			xhci_dbg(xhci, "resume HS port %d\n", port_id);
			port->resume_timestamp = jiffies +
				msecs_to_jiffies(USB_RESUME_TIMEOUT);
			set_bit(hcd_portnum, &bus_state->resuming_ports);
			 
			set_bit(HCD_FLAG_POLL_RH, &hcd->flags);
			mod_timer(&hcd->rh_timer,
				  port->resume_timestamp);
			usb_hcd_start_port_resume(&hcd->self, hcd_portnum);
			bogus_port_status = true;
		}
	}

	if ((portsc & PORT_PLC) &&
	    DEV_SUPERSPEED_ANY(portsc) &&
	    ((portsc & PORT_PLS_MASK) == XDEV_U0 ||
	     (portsc & PORT_PLS_MASK) == XDEV_U1 ||
	     (portsc & PORT_PLS_MASK) == XDEV_U2)) {
		xhci_dbg(xhci, "resume SS port %d finished\n", port_id);
		complete(&port->u3exit_done);
		 
		slot_id = xhci_find_slot_id_by_port(hcd, xhci, hcd_portnum + 1);
		if (slot_id && xhci->devs[slot_id])
			xhci_ring_device(xhci, slot_id);
		if (bus_state->port_remote_wakeup & (1 << hcd_portnum)) {
			xhci_test_and_clear_bit(xhci, port, PORT_PLC);
			usb_wakeup_notification(hcd->self.root_hub,
					hcd_portnum + 1);
			bogus_port_status = true;
			goto cleanup;
		}
	}

	 
	if (hcd->speed < HCD_USB3 && port->rexit_active) {
		complete(&port->rexit_done);
		port->rexit_active = false;
		bogus_port_status = true;
		goto cleanup;
	}

	if (hcd->speed < HCD_USB3) {
		xhci_test_and_clear_bit(xhci, port, PORT_PLC);
		if ((xhci->quirks & XHCI_RESET_PLL_ON_DISCONNECT) &&
		    (portsc & PORT_CSC) && !(portsc & PORT_CONNECT))
			xhci_cavium_reset_phy_quirk(xhci);
	}

cleanup:
	 
	inc_deq(xhci, ir->event_ring);

	 
	if (bogus_port_status)
		return;

	 
	xhci_dbg(xhci, "%s: starting usb%d port polling.\n",
		 __func__, hcd->self.busnum);
	set_bit(HCD_FLAG_POLL_RH, &hcd->flags);
	spin_unlock(&xhci->lock);
	 
	usb_hcd_poll_rh_status(hcd);
	spin_lock(&xhci->lock);
}

 
struct xhci_segment *trb_in_td(struct xhci_hcd *xhci,
		struct xhci_segment *start_seg,
		union xhci_trb	*start_trb,
		union xhci_trb	*end_trb,
		dma_addr_t	suspect_dma,
		bool		debug)
{
	dma_addr_t start_dma;
	dma_addr_t end_seg_dma;
	dma_addr_t end_trb_dma;
	struct xhci_segment *cur_seg;

	start_dma = xhci_trb_virt_to_dma(start_seg, start_trb);
	cur_seg = start_seg;

	do {
		if (start_dma == 0)
			return NULL;
		 
		end_seg_dma = xhci_trb_virt_to_dma(cur_seg,
				&cur_seg->trbs[TRBS_PER_SEGMENT - 1]);
		 
		end_trb_dma = xhci_trb_virt_to_dma(cur_seg, end_trb);

		if (debug)
			xhci_warn(xhci,
				"Looking for event-dma %016llx trb-start %016llx trb-end %016llx seg-start %016llx seg-end %016llx\n",
				(unsigned long long)suspect_dma,
				(unsigned long long)start_dma,
				(unsigned long long)end_trb_dma,
				(unsigned long long)cur_seg->dma,
				(unsigned long long)end_seg_dma);

		if (end_trb_dma > 0) {
			 
			if (start_dma <= end_trb_dma) {
				if (suspect_dma >= start_dma && suspect_dma <= end_trb_dma)
					return cur_seg;
			} else {
				 
				if ((suspect_dma >= start_dma &&
							suspect_dma <= end_seg_dma) ||
						(suspect_dma >= cur_seg->dma &&
						 suspect_dma <= end_trb_dma))
					return cur_seg;
			}
			return NULL;
		} else {
			 
			if (suspect_dma >= start_dma && suspect_dma <= end_seg_dma)
				return cur_seg;
		}
		cur_seg = cur_seg->next;
		start_dma = xhci_trb_virt_to_dma(cur_seg, &cur_seg->trbs[0]);
	} while (cur_seg != start_seg);

	return NULL;
}

static void xhci_clear_hub_tt_buffer(struct xhci_hcd *xhci, struct xhci_td *td,
		struct xhci_virt_ep *ep)
{
	 
	if (td->urb->dev->tt && !usb_pipeint(td->urb->pipe) &&
	    (td->urb->dev->tt->hub != xhci_to_hcd(xhci)->self.root_hub) &&
	    !(ep->ep_state & EP_CLEARING_TT)) {
		ep->ep_state |= EP_CLEARING_TT;
		td->urb->ep->hcpriv = td->urb->dev;
		if (usb_hub_clear_tt_buffer(td->urb))
			ep->ep_state &= ~EP_CLEARING_TT;
	}
}

 
static int xhci_requires_manual_halt_cleanup(struct xhci_hcd *xhci,
		struct xhci_ep_ctx *ep_ctx,
		unsigned int trb_comp_code)
{
	 
	if (trb_comp_code == COMP_USB_TRANSACTION_ERROR ||
			trb_comp_code == COMP_BABBLE_DETECTED_ERROR ||
			trb_comp_code == COMP_SPLIT_TRANSACTION_ERROR)
		 
		if (GET_EP_CTX_STATE(ep_ctx) == EP_STATE_HALTED)
			return 1;

	return 0;
}

int xhci_is_vendor_info_code(struct xhci_hcd *xhci, unsigned int trb_comp_code)
{
	if (trb_comp_code >= 224 && trb_comp_code <= 255) {
		 
		xhci_dbg(xhci, "Vendor defined info completion code %u\n",
				trb_comp_code);
		xhci_dbg(xhci, "Treating code as success.\n");
		return 1;
	}
	return 0;
}

static int finish_td(struct xhci_hcd *xhci, struct xhci_virt_ep *ep,
		     struct xhci_ring *ep_ring, struct xhci_td *td,
		     u32 trb_comp_code)
{
	struct xhci_ep_ctx *ep_ctx;

	ep_ctx = xhci_get_ep_ctx(xhci, ep->vdev->out_ctx, ep->ep_index);

	switch (trb_comp_code) {
	case COMP_STOPPED_LENGTH_INVALID:
	case COMP_STOPPED_SHORT_PACKET:
	case COMP_STOPPED:
		 
		return 0;
	case COMP_USB_TRANSACTION_ERROR:
	case COMP_BABBLE_DETECTED_ERROR:
	case COMP_SPLIT_TRANSACTION_ERROR:
		 
		if (GET_EP_CTX_STATE(ep_ctx) != EP_STATE_HALTED) {
			 
			if ((ep->ep_state & EP_HALTED) &&
			    !list_empty(&td->cancelled_td_list)) {
				xhci_dbg(xhci, "Already resolving halted ep for 0x%llx\n",
					 (unsigned long long)xhci_trb_virt_to_dma(
						 td->start_seg, td->first_trb));
				return 0;
			}
			 
			break;
		}
		 
		xhci_clear_hub_tt_buffer(xhci, td, ep);
		xhci_handle_halted_endpoint(xhci, ep, td, EP_HARD_RESET);
		return 0;
	case COMP_STALL_ERROR:
		 
		if (ep->ep_index != 0)
			xhci_clear_hub_tt_buffer(xhci, td, ep);

		xhci_handle_halted_endpoint(xhci, ep, td, EP_HARD_RESET);

		return 0;  
	default:
		break;
	}

	 
	ep_ring->dequeue = td->last_trb;
	ep_ring->deq_seg = td->last_trb_seg;
	inc_deq(xhci, ep_ring);

	return xhci_td_cleanup(xhci, td, ep_ring, td->status);
}

 
static int sum_trb_lengths(struct xhci_hcd *xhci, struct xhci_ring *ring,
			   union xhci_trb *stop_trb)
{
	u32 sum;
	union xhci_trb *trb = ring->dequeue;
	struct xhci_segment *seg = ring->deq_seg;

	for (sum = 0; trb != stop_trb; next_trb(xhci, ring, &seg, &trb)) {
		if (!trb_is_noop(trb) && !trb_is_link(trb))
			sum += TRB_LEN(le32_to_cpu(trb->generic.field[2]));
	}
	return sum;
}

 
static int process_ctrl_td(struct xhci_hcd *xhci, struct xhci_virt_ep *ep,
		struct xhci_ring *ep_ring,  struct xhci_td *td,
			   union xhci_trb *ep_trb, struct xhci_transfer_event *event)
{
	struct xhci_ep_ctx *ep_ctx;
	u32 trb_comp_code;
	u32 remaining, requested;
	u32 trb_type;

	trb_type = TRB_FIELD_TO_TYPE(le32_to_cpu(ep_trb->generic.field[3]));
	ep_ctx = xhci_get_ep_ctx(xhci, ep->vdev->out_ctx, ep->ep_index);
	trb_comp_code = GET_COMP_CODE(le32_to_cpu(event->transfer_len));
	requested = td->urb->transfer_buffer_length;
	remaining = EVENT_TRB_LEN(le32_to_cpu(event->transfer_len));

	switch (trb_comp_code) {
	case COMP_SUCCESS:
		if (trb_type != TRB_STATUS) {
			xhci_warn(xhci, "WARN: Success on ctrl %s TRB without IOC set?\n",
				  (trb_type == TRB_DATA) ? "data" : "setup");
			td->status = -ESHUTDOWN;
			break;
		}
		td->status = 0;
		break;
	case COMP_SHORT_PACKET:
		td->status = 0;
		break;
	case COMP_STOPPED_SHORT_PACKET:
		if (trb_type == TRB_DATA || trb_type == TRB_NORMAL)
			td->urb->actual_length = remaining;
		else
			xhci_warn(xhci, "WARN: Stopped Short Packet on ctrl setup or status TRB\n");
		goto finish_td;
	case COMP_STOPPED:
		switch (trb_type) {
		case TRB_SETUP:
			td->urb->actual_length = 0;
			goto finish_td;
		case TRB_DATA:
		case TRB_NORMAL:
			td->urb->actual_length = requested - remaining;
			goto finish_td;
		case TRB_STATUS:
			td->urb->actual_length = requested;
			goto finish_td;
		default:
			xhci_warn(xhci, "WARN: unexpected TRB Type %d\n",
				  trb_type);
			goto finish_td;
		}
	case COMP_STOPPED_LENGTH_INVALID:
		goto finish_td;
	default:
		if (!xhci_requires_manual_halt_cleanup(xhci,
						       ep_ctx, trb_comp_code))
			break;
		xhci_dbg(xhci, "TRB error %u, halted endpoint index = %u\n",
			 trb_comp_code, ep->ep_index);
		fallthrough;
	case COMP_STALL_ERROR:
		 
		if (trb_type == TRB_DATA || trb_type == TRB_NORMAL)
			td->urb->actual_length = requested - remaining;
		else if (!td->urb_length_set)
			td->urb->actual_length = 0;
		goto finish_td;
	}

	 
	if (trb_type == TRB_SETUP)
		goto finish_td;

	 
	if (trb_type == TRB_DATA ||
		trb_type == TRB_NORMAL) {
		td->urb_length_set = true;
		td->urb->actual_length = requested - remaining;
		xhci_dbg(xhci, "Waiting for status stage event\n");
		return 0;
	}

	 
	if (!td->urb_length_set)
		td->urb->actual_length = requested;

finish_td:
	return finish_td(xhci, ep, ep_ring, td, trb_comp_code);
}

 
static int process_isoc_td(struct xhci_hcd *xhci, struct xhci_virt_ep *ep,
		struct xhci_ring *ep_ring, struct xhci_td *td,
		union xhci_trb *ep_trb, struct xhci_transfer_event *event)
{
	struct urb_priv *urb_priv;
	int idx;
	struct usb_iso_packet_descriptor *frame;
	u32 trb_comp_code;
	bool sum_trbs_for_length = false;
	u32 remaining, requested, ep_trb_len;
	int short_framestatus;

	trb_comp_code = GET_COMP_CODE(le32_to_cpu(event->transfer_len));
	urb_priv = td->urb->hcpriv;
	idx = urb_priv->num_tds_done;
	frame = &td->urb->iso_frame_desc[idx];
	requested = frame->length;
	remaining = EVENT_TRB_LEN(le32_to_cpu(event->transfer_len));
	ep_trb_len = TRB_LEN(le32_to_cpu(ep_trb->generic.field[2]));
	short_framestatus = td->urb->transfer_flags & URB_SHORT_NOT_OK ?
		-EREMOTEIO : 0;

	 
	switch (trb_comp_code) {
	case COMP_SUCCESS:
		if (remaining) {
			frame->status = short_framestatus;
			if (xhci->quirks & XHCI_TRUST_TX_LENGTH)
				sum_trbs_for_length = true;
			break;
		}
		frame->status = 0;
		break;
	case COMP_SHORT_PACKET:
		frame->status = short_framestatus;
		sum_trbs_for_length = true;
		break;
	case COMP_BANDWIDTH_OVERRUN_ERROR:
		frame->status = -ECOMM;
		break;
	case COMP_ISOCH_BUFFER_OVERRUN:
	case COMP_BABBLE_DETECTED_ERROR:
		frame->status = -EOVERFLOW;
		break;
	case COMP_INCOMPATIBLE_DEVICE_ERROR:
	case COMP_STALL_ERROR:
		frame->status = -EPROTO;
		break;
	case COMP_USB_TRANSACTION_ERROR:
		frame->status = -EPROTO;
		if (ep_trb != td->last_trb)
			return 0;
		break;
	case COMP_STOPPED:
		sum_trbs_for_length = true;
		break;
	case COMP_STOPPED_SHORT_PACKET:
		 
		frame->status = short_framestatus;
		requested = remaining;
		break;
	case COMP_STOPPED_LENGTH_INVALID:
		requested = 0;
		remaining = 0;
		break;
	default:
		sum_trbs_for_length = true;
		frame->status = -1;
		break;
	}

	if (sum_trbs_for_length)
		frame->actual_length = sum_trb_lengths(xhci, ep->ring, ep_trb) +
			ep_trb_len - remaining;
	else
		frame->actual_length = requested;

	td->urb->actual_length += frame->actual_length;

	return finish_td(xhci, ep, ep_ring, td, trb_comp_code);
}

static int skip_isoc_td(struct xhci_hcd *xhci, struct xhci_td *td,
			struct xhci_virt_ep *ep, int status)
{
	struct urb_priv *urb_priv;
	struct usb_iso_packet_descriptor *frame;
	int idx;

	urb_priv = td->urb->hcpriv;
	idx = urb_priv->num_tds_done;
	frame = &td->urb->iso_frame_desc[idx];

	 
	frame->status = -EXDEV;

	 
	frame->actual_length = 0;

	 
	ep->ring->dequeue = td->last_trb;
	ep->ring->deq_seg = td->last_trb_seg;
	inc_deq(xhci, ep->ring);

	return xhci_td_cleanup(xhci, td, ep->ring, status);
}

 
static int process_bulk_intr_td(struct xhci_hcd *xhci, struct xhci_virt_ep *ep,
		struct xhci_ring *ep_ring, struct xhci_td *td,
		union xhci_trb *ep_trb, struct xhci_transfer_event *event)
{
	struct xhci_slot_ctx *slot_ctx;
	u32 trb_comp_code;
	u32 remaining, requested, ep_trb_len;

	slot_ctx = xhci_get_slot_ctx(xhci, ep->vdev->out_ctx);
	trb_comp_code = GET_COMP_CODE(le32_to_cpu(event->transfer_len));
	remaining = EVENT_TRB_LEN(le32_to_cpu(event->transfer_len));
	ep_trb_len = TRB_LEN(le32_to_cpu(ep_trb->generic.field[2]));
	requested = td->urb->transfer_buffer_length;

	switch (trb_comp_code) {
	case COMP_SUCCESS:
		ep->err_count = 0;
		 
		if (ep_trb != td->last_trb || remaining) {
			xhci_warn(xhci, "WARN Successful completion on short TX\n");
			xhci_dbg(xhci, "ep %#x - asked for %d bytes, %d bytes untransferred\n",
				 td->urb->ep->desc.bEndpointAddress,
				 requested, remaining);
		}
		td->status = 0;
		break;
	case COMP_SHORT_PACKET:
		xhci_dbg(xhci, "ep %#x - asked for %d bytes, %d bytes untransferred\n",
			 td->urb->ep->desc.bEndpointAddress,
			 requested, remaining);
		td->status = 0;
		break;
	case COMP_STOPPED_SHORT_PACKET:
		td->urb->actual_length = remaining;
		goto finish_td;
	case COMP_STOPPED_LENGTH_INVALID:
		 
		ep_trb_len	= 0;
		remaining	= 0;
		break;
	case COMP_USB_TRANSACTION_ERROR:
		if (xhci->quirks & XHCI_NO_SOFT_RETRY ||
		    (ep->err_count++ > MAX_SOFT_RETRY) ||
		    le32_to_cpu(slot_ctx->tt_info) & TT_SLOT)
			break;

		td->status = 0;

		xhci_handle_halted_endpoint(xhci, ep, td, EP_SOFT_RESET);
		return 0;
	default:
		 
		break;
	}

	if (ep_trb == td->last_trb)
		td->urb->actual_length = requested - remaining;
	else
		td->urb->actual_length =
			sum_trb_lengths(xhci, ep_ring, ep_trb) +
			ep_trb_len - remaining;
finish_td:
	if (remaining > requested) {
		xhci_warn(xhci, "bad transfer trb length %d in event trb\n",
			  remaining);
		td->urb->actual_length = 0;
	}

	return finish_td(xhci, ep, ep_ring, td, trb_comp_code);
}

 
static int handle_tx_event(struct xhci_hcd *xhci,
			   struct xhci_interrupter *ir,
			   struct xhci_transfer_event *event)
{
	struct xhci_virt_ep *ep;
	struct xhci_ring *ep_ring;
	unsigned int slot_id;
	int ep_index;
	struct xhci_td *td = NULL;
	dma_addr_t ep_trb_dma;
	struct xhci_segment *ep_seg;
	union xhci_trb *ep_trb;
	int status = -EINPROGRESS;
	struct xhci_ep_ctx *ep_ctx;
	u32 trb_comp_code;
	int td_num = 0;
	bool handling_skipped_tds = false;

	slot_id = TRB_TO_SLOT_ID(le32_to_cpu(event->flags));
	ep_index = TRB_TO_EP_ID(le32_to_cpu(event->flags)) - 1;
	trb_comp_code = GET_COMP_CODE(le32_to_cpu(event->transfer_len));
	ep_trb_dma = le64_to_cpu(event->buffer);

	ep = xhci_get_virt_ep(xhci, slot_id, ep_index);
	if (!ep) {
		xhci_err(xhci, "ERROR Invalid Transfer event\n");
		goto err_out;
	}

	ep_ring = xhci_dma_to_transfer_ring(ep, ep_trb_dma);
	ep_ctx = xhci_get_ep_ctx(xhci, ep->vdev->out_ctx, ep_index);

	if (GET_EP_CTX_STATE(ep_ctx) == EP_STATE_DISABLED) {
		xhci_err(xhci,
			 "ERROR Transfer event for disabled endpoint slot %u ep %u\n",
			  slot_id, ep_index);
		goto err_out;
	}

	 
	if (!ep_ring) {
		switch (trb_comp_code) {
		case COMP_STALL_ERROR:
		case COMP_USB_TRANSACTION_ERROR:
		case COMP_INVALID_STREAM_TYPE_ERROR:
		case COMP_INVALID_STREAM_ID_ERROR:
			xhci_dbg(xhci, "Stream transaction error ep %u no id\n",
				 ep_index);
			if (ep->err_count++ > MAX_SOFT_RETRY)
				xhci_handle_halted_endpoint(xhci, ep, NULL,
							    EP_HARD_RESET);
			else
				xhci_handle_halted_endpoint(xhci, ep, NULL,
							    EP_SOFT_RESET);
			goto cleanup;
		case COMP_RING_UNDERRUN:
		case COMP_RING_OVERRUN:
		case COMP_STOPPED_LENGTH_INVALID:
			goto cleanup;
		default:
			xhci_err(xhci, "ERROR Transfer event for unknown stream ring slot %u ep %u\n",
				 slot_id, ep_index);
			goto err_out;
		}
	}

	 
	if (ep->skip)
		td_num += list_count_nodes(&ep_ring->td_list);

	 
	switch (trb_comp_code) {
	 
	case COMP_SUCCESS:
		if (EVENT_TRB_LEN(le32_to_cpu(event->transfer_len)) == 0)
			break;
		if (xhci->quirks & XHCI_TRUST_TX_LENGTH ||
		    ep_ring->last_td_was_short)
			trb_comp_code = COMP_SHORT_PACKET;
		else
			xhci_warn_ratelimited(xhci,
					      "WARN Successful completion on short TX for slot %u ep %u: needs XHCI_TRUST_TX_LENGTH quirk?\n",
					      slot_id, ep_index);
		break;
	case COMP_SHORT_PACKET:
		break;
	 
	case COMP_STOPPED:
		xhci_dbg(xhci, "Stopped on Transfer TRB for slot %u ep %u\n",
			 slot_id, ep_index);
		break;
	case COMP_STOPPED_LENGTH_INVALID:
		xhci_dbg(xhci,
			 "Stopped on No-op or Link TRB for slot %u ep %u\n",
			 slot_id, ep_index);
		break;
	case COMP_STOPPED_SHORT_PACKET:
		xhci_dbg(xhci,
			 "Stopped with short packet transfer detected for slot %u ep %u\n",
			 slot_id, ep_index);
		break;
	 
	case COMP_STALL_ERROR:
		xhci_dbg(xhci, "Stalled endpoint for slot %u ep %u\n", slot_id,
			 ep_index);
		status = -EPIPE;
		break;
	case COMP_SPLIT_TRANSACTION_ERROR:
		xhci_dbg(xhci, "Split transaction error for slot %u ep %u\n",
			 slot_id, ep_index);
		status = -EPROTO;
		break;
	case COMP_USB_TRANSACTION_ERROR:
		xhci_dbg(xhci, "Transfer error for slot %u ep %u on endpoint\n",
			 slot_id, ep_index);
		status = -EPROTO;
		break;
	case COMP_BABBLE_DETECTED_ERROR:
		xhci_dbg(xhci, "Babble error for slot %u ep %u on endpoint\n",
			 slot_id, ep_index);
		status = -EOVERFLOW;
		break;
	 
	case COMP_TRB_ERROR:
		xhci_warn(xhci,
			  "WARN: TRB error for slot %u ep %u on endpoint\n",
			  slot_id, ep_index);
		status = -EILSEQ;
		break;
	 
	case COMP_DATA_BUFFER_ERROR:
		xhci_warn(xhci,
			  "WARN: HC couldn't access mem fast enough for slot %u ep %u\n",
			  slot_id, ep_index);
		status = -ENOSR;
		break;
	case COMP_BANDWIDTH_OVERRUN_ERROR:
		xhci_warn(xhci,
			  "WARN: bandwidth overrun event for slot %u ep %u on endpoint\n",
			  slot_id, ep_index);
		break;
	case COMP_ISOCH_BUFFER_OVERRUN:
		xhci_warn(xhci,
			  "WARN: buffer overrun event for slot %u ep %u on endpoint",
			  slot_id, ep_index);
		break;
	case COMP_RING_UNDERRUN:
		 
		xhci_dbg(xhci, "underrun event on endpoint\n");
		if (!list_empty(&ep_ring->td_list))
			xhci_dbg(xhci, "Underrun Event for slot %d ep %d "
					"still with TDs queued?\n",
				 TRB_TO_SLOT_ID(le32_to_cpu(event->flags)),
				 ep_index);
		goto cleanup;
	case COMP_RING_OVERRUN:
		xhci_dbg(xhci, "overrun event on endpoint\n");
		if (!list_empty(&ep_ring->td_list))
			xhci_dbg(xhci, "Overrun Event for slot %d ep %d "
					"still with TDs queued?\n",
				 TRB_TO_SLOT_ID(le32_to_cpu(event->flags)),
				 ep_index);
		goto cleanup;
	case COMP_MISSED_SERVICE_ERROR:
		 
		ep->skip = true;
		xhci_dbg(xhci,
			 "Miss service interval error for slot %u ep %u, set skip flag\n",
			 slot_id, ep_index);
		goto cleanup;
	case COMP_NO_PING_RESPONSE_ERROR:
		ep->skip = true;
		xhci_dbg(xhci,
			 "No Ping response error for slot %u ep %u, Skip one Isoc TD\n",
			 slot_id, ep_index);
		goto cleanup;

	case COMP_INCOMPATIBLE_DEVICE_ERROR:
		 
		xhci_warn(xhci,
			  "WARN: detect an incompatible device for slot %u ep %u",
			  slot_id, ep_index);
		status = -EPROTO;
		break;
	default:
		if (xhci_is_vendor_info_code(xhci, trb_comp_code)) {
			status = 0;
			break;
		}
		xhci_warn(xhci,
			  "ERROR Unknown event condition %u for slot %u ep %u , HC probably busted\n",
			  trb_comp_code, slot_id, ep_index);
		goto cleanup;
	}

	do {
		 
		if (list_empty(&ep_ring->td_list)) {
			 

			if (!(trb_comp_code == COMP_STOPPED ||
			      trb_comp_code == COMP_STOPPED_LENGTH_INVALID ||
			      ep_ring->last_td_was_short)) {
				xhci_warn(xhci, "WARN Event TRB for slot %d ep %d with no TDs queued?\n",
						TRB_TO_SLOT_ID(le32_to_cpu(event->flags)),
						ep_index);
			}
			if (ep->skip) {
				ep->skip = false;
				xhci_dbg(xhci, "td_list is empty while skip flag set. Clear skip flag for slot %u ep %u.\n",
					 slot_id, ep_index);
			}
			if (trb_comp_code == COMP_STALL_ERROR ||
			    xhci_requires_manual_halt_cleanup(xhci, ep_ctx,
							      trb_comp_code)) {
				xhci_handle_halted_endpoint(xhci, ep, NULL,
							    EP_HARD_RESET);
			}
			goto cleanup;
		}

		 
		if (ep->skip && td_num == 0) {
			ep->skip = false;
			xhci_dbg(xhci, "All tds on the ep_ring skipped. Clear skip flag for slot %u ep %u.\n",
				 slot_id, ep_index);
			goto cleanup;
		}

		td = list_first_entry(&ep_ring->td_list, struct xhci_td,
				      td_list);
		if (ep->skip)
			td_num--;

		 
		ep_seg = trb_in_td(xhci, ep_ring->deq_seg, ep_ring->dequeue,
				td->last_trb, ep_trb_dma, false);

		 
		if (!ep_seg && (trb_comp_code == COMP_STOPPED ||
			   trb_comp_code == COMP_STOPPED_LENGTH_INVALID)) {
			goto cleanup;
		}

		if (!ep_seg) {
			if (!ep->skip ||
			    !usb_endpoint_xfer_isoc(&td->urb->ep->desc)) {
				 
				if ((xhci->quirks & XHCI_SPURIOUS_SUCCESS) &&
						ep_ring->last_td_was_short) {
					ep_ring->last_td_was_short = false;
					goto cleanup;
				}
				 
				xhci_err(xhci,
					"ERROR Transfer event TRB DMA ptr not "
					"part of current TD ep_index %d "
					"comp_code %u\n", ep_index,
					trb_comp_code);
				trb_in_td(xhci, ep_ring->deq_seg,
					  ep_ring->dequeue, td->last_trb,
					  ep_trb_dma, true);
				return -ESHUTDOWN;
			}

			skip_isoc_td(xhci, td, ep, status);
			goto cleanup;
		}
		if (trb_comp_code == COMP_SHORT_PACKET)
			ep_ring->last_td_was_short = true;
		else
			ep_ring->last_td_was_short = false;

		if (ep->skip) {
			xhci_dbg(xhci,
				 "Found td. Clear skip flag for slot %u ep %u.\n",
				 slot_id, ep_index);
			ep->skip = false;
		}

		ep_trb = &ep_seg->trbs[(ep_trb_dma - ep_seg->dma) /
						sizeof(*ep_trb)];

		trace_xhci_handle_transfer(ep_ring,
				(struct xhci_generic_trb *) ep_trb);

		 

		if (trb_is_noop(ep_trb)) {
			if (trb_comp_code == COMP_STALL_ERROR ||
			    xhci_requires_manual_halt_cleanup(xhci, ep_ctx,
							      trb_comp_code))
				xhci_handle_halted_endpoint(xhci, ep, td,
							    EP_HARD_RESET);
			goto cleanup;
		}

		td->status = status;

		 
		if (usb_endpoint_xfer_control(&td->urb->ep->desc))
			process_ctrl_td(xhci, ep, ep_ring, td, ep_trb, event);
		else if (usb_endpoint_xfer_isoc(&td->urb->ep->desc))
			process_isoc_td(xhci, ep, ep_ring, td, ep_trb, event);
		else
			process_bulk_intr_td(xhci, ep, ep_ring, td, ep_trb, event);
cleanup:
		handling_skipped_tds = ep->skip &&
			trb_comp_code != COMP_MISSED_SERVICE_ERROR &&
			trb_comp_code != COMP_NO_PING_RESPONSE_ERROR;

		 
		if (!handling_skipped_tds)
			inc_deq(xhci, ir->event_ring);

	 
	} while (handling_skipped_tds);

	return 0;

err_out:
	xhci_err(xhci, "@%016llx %08x %08x %08x %08x\n",
		 (unsigned long long) xhci_trb_virt_to_dma(
			 ir->event_ring->deq_seg,
			 ir->event_ring->dequeue),
		 lower_32_bits(le64_to_cpu(event->buffer)),
		 upper_32_bits(le64_to_cpu(event->buffer)),
		 le32_to_cpu(event->transfer_len),
		 le32_to_cpu(event->flags));
	return -ENODEV;
}

 
static int xhci_handle_event(struct xhci_hcd *xhci, struct xhci_interrupter *ir)
{
	union xhci_trb *event;
	int update_ptrs = 1;
	u32 trb_type;
	int ret;

	 
	if (!ir || !ir->event_ring || !ir->event_ring->dequeue) {
		xhci_err(xhci, "ERROR interrupter not ready\n");
		return -ENOMEM;
	}

	event = ir->event_ring->dequeue;
	 
	if ((le32_to_cpu(event->event_cmd.flags) & TRB_CYCLE) !=
	    ir->event_ring->cycle_state)
		return 0;

	trace_xhci_handle_event(ir->event_ring, &event->generic);

	 
	rmb();
	trb_type = TRB_FIELD_TO_TYPE(le32_to_cpu(event->event_cmd.flags));
	 

	switch (trb_type) {
	case TRB_COMPLETION:
		handle_cmd_completion(xhci, &event->event_cmd);
		break;
	case TRB_PORT_STATUS:
		handle_port_status(xhci, ir, event);
		update_ptrs = 0;
		break;
	case TRB_TRANSFER:
		ret = handle_tx_event(xhci, ir, &event->trans_event);
		if (ret >= 0)
			update_ptrs = 0;
		break;
	case TRB_DEV_NOTE:
		handle_device_notification(xhci, event);
		break;
	default:
		if (trb_type >= TRB_VENDOR_DEFINED_LOW)
			handle_vendor_event(xhci, event, trb_type);
		else
			xhci_warn(xhci, "ERROR unknown event type %d\n", trb_type);
	}
	 
	if (xhci->xhc_state & XHCI_STATE_DYING) {
		xhci_dbg(xhci, "xHCI host dying, returning from "
				"event handler.\n");
		return 0;
	}

	if (update_ptrs)
		 
		inc_deq(xhci, ir->event_ring);

	 
	return 1;
}

 
static void xhci_update_erst_dequeue(struct xhci_hcd *xhci,
				     struct xhci_interrupter *ir,
				     union xhci_trb *event_ring_deq,
				     bool clear_ehb)
{
	u64 temp_64;
	dma_addr_t deq;

	temp_64 = xhci_read_64(xhci, &ir->ir_set->erst_dequeue);
	 
	if (event_ring_deq != ir->event_ring->dequeue) {
		deq = xhci_trb_virt_to_dma(ir->event_ring->deq_seg,
				ir->event_ring->dequeue);
		if (deq == 0)
			xhci_warn(xhci, "WARN something wrong with SW event ring dequeue ptr\n");
		 
		if ((temp_64 & (u64) ~ERST_PTR_MASK) ==
				((u64) deq & (u64) ~ERST_PTR_MASK))
			return;

		 
		temp_64 &= ERST_DESI_MASK;
		temp_64 |= ((u64) deq & (u64) ~ERST_PTR_MASK);
	}

	 
	if (clear_ehb)
		temp_64 |= ERST_EHB;
	xhci_write_64(xhci, temp_64, &ir->ir_set->erst_dequeue);
}

 
irqreturn_t xhci_irq(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	union xhci_trb *event_ring_deq;
	struct xhci_interrupter *ir;
	irqreturn_t ret = IRQ_NONE;
	u64 temp_64;
	u32 status;
	int event_loop = 0;

	spin_lock(&xhci->lock);
	 
	status = readl(&xhci->op_regs->status);
	if (status == ~(u32)0) {
		xhci_hc_died(xhci);
		ret = IRQ_HANDLED;
		goto out;
	}

	if (!(status & STS_EINT))
		goto out;

	if (status & STS_HCE) {
		xhci_warn(xhci, "WARNING: Host Controller Error\n");
		goto out;
	}

	if (status & STS_FATAL) {
		xhci_warn(xhci, "WARNING: Host System Error\n");
		xhci_halt(xhci);
		ret = IRQ_HANDLED;
		goto out;
	}

	 
	status |= STS_EINT;
	writel(status, &xhci->op_regs->status);

	 
	ir = xhci->interrupter;
	if (!hcd->msi_enabled) {
		u32 irq_pending;
		irq_pending = readl(&ir->ir_set->irq_pending);
		irq_pending |= IMAN_IP;
		writel(irq_pending, &ir->ir_set->irq_pending);
	}

	if (xhci->xhc_state & XHCI_STATE_DYING ||
	    xhci->xhc_state & XHCI_STATE_HALTED) {
		xhci_dbg(xhci, "xHCI dying, ignoring interrupt. "
				"Shouldn't IRQs be disabled?\n");
		 
		temp_64 = xhci_read_64(xhci, &ir->ir_set->erst_dequeue);
		xhci_write_64(xhci, temp_64 | ERST_EHB,
				&ir->ir_set->erst_dequeue);
		ret = IRQ_HANDLED;
		goto out;
	}

	event_ring_deq = ir->event_ring->dequeue;
	 
	while (xhci_handle_event(xhci, ir) > 0) {
		if (event_loop++ < TRBS_PER_SEGMENT / 2)
			continue;
		xhci_update_erst_dequeue(xhci, ir, event_ring_deq, false);
		event_ring_deq = ir->event_ring->dequeue;

		 
		if (xhci->isoc_bei_interval > AVOID_BEI_INTERVAL_MIN)
			xhci->isoc_bei_interval = xhci->isoc_bei_interval / 2;

		event_loop = 0;
	}

	xhci_update_erst_dequeue(xhci, ir, event_ring_deq, true);
	ret = IRQ_HANDLED;

out:
	spin_unlock(&xhci->lock);

	return ret;
}

irqreturn_t xhci_msi_irq(int irq, void *hcd)
{
	return xhci_irq(hcd);
}
EXPORT_SYMBOL_GPL(xhci_msi_irq);

 

 
static void queue_trb(struct xhci_hcd *xhci, struct xhci_ring *ring,
		bool more_trbs_coming,
		u32 field1, u32 field2, u32 field3, u32 field4)
{
	struct xhci_generic_trb *trb;

	trb = &ring->enqueue->generic;
	trb->field[0] = cpu_to_le32(field1);
	trb->field[1] = cpu_to_le32(field2);
	trb->field[2] = cpu_to_le32(field3);
	 
	wmb();
	trb->field[3] = cpu_to_le32(field4);

	trace_xhci_queue_trb(ring, trb);

	inc_enq(xhci, ring, more_trbs_coming);
}

 
static int prepare_ring(struct xhci_hcd *xhci, struct xhci_ring *ep_ring,
		u32 ep_state, unsigned int num_trbs, gfp_t mem_flags)
{
	unsigned int link_trb_count = 0;
	unsigned int new_segs = 0;

	 
	switch (ep_state) {
	case EP_STATE_DISABLED:
		 
		xhci_warn(xhci, "WARN urb submitted to disabled ep\n");
		return -ENOENT;
	case EP_STATE_ERROR:
		xhci_warn(xhci, "WARN waiting for error on ep to be cleared\n");
		 
		 
		return -EINVAL;
	case EP_STATE_HALTED:
		xhci_dbg(xhci, "WARN halted endpoint, queueing URB anyway.\n");
		break;
	case EP_STATE_STOPPED:
	case EP_STATE_RUNNING:
		break;
	default:
		xhci_err(xhci, "ERROR unknown endpoint state for ep\n");
		 
		return -EINVAL;
	}

	if (ep_ring != xhci->cmd_ring) {
		new_segs = xhci_ring_expansion_needed(xhci, ep_ring, num_trbs);
	} else if (xhci_num_trbs_free(xhci, ep_ring) <= num_trbs) {
		xhci_err(xhci, "Do not support expand command ring\n");
		return -ENOMEM;
	}

	if (new_segs) {
		xhci_dbg_trace(xhci, trace_xhci_dbg_ring_expansion,
				"ERROR no room on ep ring, try ring expansion");
		if (xhci_ring_expansion(xhci, ep_ring, new_segs, mem_flags)) {
			xhci_err(xhci, "Ring expansion failed\n");
			return -ENOMEM;
		}
	}

	while (trb_is_link(ep_ring->enqueue)) {
		 
		if (!xhci_link_trb_quirk(xhci) &&
		    !(ep_ring->type == TYPE_ISOC &&
		      (xhci->quirks & XHCI_AMD_0x96_HOST)))
			ep_ring->enqueue->link.control &=
				cpu_to_le32(~TRB_CHAIN);
		else
			ep_ring->enqueue->link.control |=
				cpu_to_le32(TRB_CHAIN);

		wmb();
		ep_ring->enqueue->link.control ^= cpu_to_le32(TRB_CYCLE);

		 
		if (link_trb_toggles_cycle(ep_ring->enqueue))
			ep_ring->cycle_state ^= 1;

		ep_ring->enq_seg = ep_ring->enq_seg->next;
		ep_ring->enqueue = ep_ring->enq_seg->trbs;

		 
		if (link_trb_count++ > ep_ring->num_segs) {
			xhci_warn(xhci, "Ring is an endless link TRB loop\n");
			return -EINVAL;
		}
	}

	if (last_trb_on_seg(ep_ring->enq_seg, ep_ring->enqueue)) {
		xhci_warn(xhci, "Missing link TRB at end of ring segment\n");
		return -EINVAL;
	}

	return 0;
}

static int prepare_transfer(struct xhci_hcd *xhci,
		struct xhci_virt_device *xdev,
		unsigned int ep_index,
		unsigned int stream_id,
		unsigned int num_trbs,
		struct urb *urb,
		unsigned int td_index,
		gfp_t mem_flags)
{
	int ret;
	struct urb_priv *urb_priv;
	struct xhci_td	*td;
	struct xhci_ring *ep_ring;
	struct xhci_ep_ctx *ep_ctx = xhci_get_ep_ctx(xhci, xdev->out_ctx, ep_index);

	ep_ring = xhci_triad_to_transfer_ring(xhci, xdev->slot_id, ep_index,
					      stream_id);
	if (!ep_ring) {
		xhci_dbg(xhci, "Can't prepare ring for bad stream ID %u\n",
				stream_id);
		return -EINVAL;
	}

	ret = prepare_ring(xhci, ep_ring, GET_EP_CTX_STATE(ep_ctx),
			   num_trbs, mem_flags);
	if (ret)
		return ret;

	urb_priv = urb->hcpriv;
	td = &urb_priv->td[td_index];

	INIT_LIST_HEAD(&td->td_list);
	INIT_LIST_HEAD(&td->cancelled_td_list);

	if (td_index == 0) {
		ret = usb_hcd_link_urb_to_ep(bus_to_hcd(urb->dev->bus), urb);
		if (unlikely(ret))
			return ret;
	}

	td->urb = urb;
	 
	list_add_tail(&td->td_list, &ep_ring->td_list);
	td->start_seg = ep_ring->enq_seg;
	td->first_trb = ep_ring->enqueue;

	return 0;
}

unsigned int count_trbs(u64 addr, u64 len)
{
	unsigned int num_trbs;

	num_trbs = DIV_ROUND_UP(len + (addr & (TRB_MAX_BUFF_SIZE - 1)),
			TRB_MAX_BUFF_SIZE);
	if (num_trbs == 0)
		num_trbs++;

	return num_trbs;
}

static inline unsigned int count_trbs_needed(struct urb *urb)
{
	return count_trbs(urb->transfer_dma, urb->transfer_buffer_length);
}

static unsigned int count_sg_trbs_needed(struct urb *urb)
{
	struct scatterlist *sg;
	unsigned int i, len, full_len, num_trbs = 0;

	full_len = urb->transfer_buffer_length;

	for_each_sg(urb->sg, sg, urb->num_mapped_sgs, i) {
		len = sg_dma_len(sg);
		num_trbs += count_trbs(sg_dma_address(sg), len);
		len = min_t(unsigned int, len, full_len);
		full_len -= len;
		if (full_len == 0)
			break;
	}

	return num_trbs;
}

static unsigned int count_isoc_trbs_needed(struct urb *urb, int i)
{
	u64 addr, len;

	addr = (u64) (urb->transfer_dma + urb->iso_frame_desc[i].offset);
	len = urb->iso_frame_desc[i].length;

	return count_trbs(addr, len);
}

static void check_trb_math(struct urb *urb, int running_total)
{
	if (unlikely(running_total != urb->transfer_buffer_length))
		dev_err(&urb->dev->dev, "%s - ep %#x - Miscalculated tx length, "
				"queued %#x (%d), asked for %#x (%d)\n",
				__func__,
				urb->ep->desc.bEndpointAddress,
				running_total, running_total,
				urb->transfer_buffer_length,
				urb->transfer_buffer_length);
}

static void giveback_first_trb(struct xhci_hcd *xhci, int slot_id,
		unsigned int ep_index, unsigned int stream_id, int start_cycle,
		struct xhci_generic_trb *start_trb)
{
	 
	wmb();
	if (start_cycle)
		start_trb->field[3] |= cpu_to_le32(start_cycle);
	else
		start_trb->field[3] &= cpu_to_le32(~TRB_CYCLE);
	xhci_ring_ep_doorbell(xhci, slot_id, ep_index, stream_id);
}

static void check_interval(struct xhci_hcd *xhci, struct urb *urb,
						struct xhci_ep_ctx *ep_ctx)
{
	int xhci_interval;
	int ep_interval;

	xhci_interval = EP_INTERVAL_TO_UFRAMES(le32_to_cpu(ep_ctx->ep_info));
	ep_interval = urb->interval;

	 
	if (urb->dev->speed == USB_SPEED_LOW ||
			urb->dev->speed == USB_SPEED_FULL)
		ep_interval *= 8;

	 
	if (xhci_interval != ep_interval) {
		dev_dbg_ratelimited(&urb->dev->dev,
				"Driver uses different interval (%d microframe%s) than xHCI (%d microframe%s)\n",
				ep_interval, ep_interval == 1 ? "" : "s",
				xhci_interval, xhci_interval == 1 ? "" : "s");
		urb->interval = xhci_interval;
		 
		if (urb->dev->speed == USB_SPEED_LOW ||
				urb->dev->speed == USB_SPEED_FULL)
			urb->interval /= 8;
	}
}

 
int xhci_queue_intr_tx(struct xhci_hcd *xhci, gfp_t mem_flags,
		struct urb *urb, int slot_id, unsigned int ep_index)
{
	struct xhci_ep_ctx *ep_ctx;

	ep_ctx = xhci_get_ep_ctx(xhci, xhci->devs[slot_id]->out_ctx, ep_index);
	check_interval(xhci, urb, ep_ctx);

	return xhci_queue_bulk_tx(xhci, mem_flags, urb, slot_id, ep_index);
}

 
static u32 xhci_td_remainder(struct xhci_hcd *xhci, int transferred,
			      int trb_buff_len, unsigned int td_total_len,
			      struct urb *urb, bool more_trbs_coming)
{
	u32 maxp, total_packet_count;

	 
	if (xhci->hci_version < 0x100 && !(xhci->quirks & XHCI_MTK_HOST))
		return ((td_total_len - transferred) >> 10);

	 
	if (!more_trbs_coming || (transferred == 0 && trb_buff_len == 0) ||
	    trb_buff_len == td_total_len)
		return 0;

	 
	if ((xhci->quirks & XHCI_MTK_HOST) && (xhci->hci_version < 0x100))
		trb_buff_len = 0;

	maxp = usb_endpoint_maxp(&urb->ep->desc);
	total_packet_count = DIV_ROUND_UP(td_total_len, maxp);

	 
	return (total_packet_count - ((transferred + trb_buff_len) / maxp));
}


static int xhci_align_td(struct xhci_hcd *xhci, struct urb *urb, u32 enqd_len,
			 u32 *trb_buff_len, struct xhci_segment *seg)
{
	struct device *dev = xhci_to_hcd(xhci)->self.sysdev;
	unsigned int unalign;
	unsigned int max_pkt;
	u32 new_buff_len;
	size_t len;

	max_pkt = usb_endpoint_maxp(&urb->ep->desc);
	unalign = (enqd_len + *trb_buff_len) % max_pkt;

	 
	if (unalign == 0)
		return 0;

	xhci_dbg(xhci, "Unaligned %d bytes, buff len %d\n",
		 unalign, *trb_buff_len);

	 
	if (*trb_buff_len > unalign) {
		*trb_buff_len -= unalign;
		xhci_dbg(xhci, "split align, new buff len %d\n", *trb_buff_len);
		return 0;
	}

	 
	new_buff_len = max_pkt - (enqd_len % max_pkt);

	if (new_buff_len > (urb->transfer_buffer_length - enqd_len))
		new_buff_len = (urb->transfer_buffer_length - enqd_len);

	 
	if (usb_urb_dir_out(urb)) {
		if (urb->num_sgs) {
			len = sg_pcopy_to_buffer(urb->sg, urb->num_sgs,
						 seg->bounce_buf, new_buff_len, enqd_len);
			if (len != new_buff_len)
				xhci_warn(xhci, "WARN Wrong bounce buffer write length: %zu != %d\n",
					  len, new_buff_len);
		} else {
			memcpy(seg->bounce_buf, urb->transfer_buffer + enqd_len, new_buff_len);
		}

		seg->bounce_dma = dma_map_single(dev, seg->bounce_buf,
						 max_pkt, DMA_TO_DEVICE);
	} else {
		seg->bounce_dma = dma_map_single(dev, seg->bounce_buf,
						 max_pkt, DMA_FROM_DEVICE);
	}

	if (dma_mapping_error(dev, seg->bounce_dma)) {
		 
		xhci_warn(xhci, "Failed mapping bounce buffer, not aligning\n");
		return 0;
	}
	*trb_buff_len = new_buff_len;
	seg->bounce_len = new_buff_len;
	seg->bounce_offs = enqd_len;

	xhci_dbg(xhci, "Bounce align, new buff len %d\n", *trb_buff_len);

	return 1;
}

 
int xhci_queue_bulk_tx(struct xhci_hcd *xhci, gfp_t mem_flags,
		struct urb *urb, int slot_id, unsigned int ep_index)
{
	struct xhci_ring *ring;
	struct urb_priv *urb_priv;
	struct xhci_td *td;
	struct xhci_generic_trb *start_trb;
	struct scatterlist *sg = NULL;
	bool more_trbs_coming = true;
	bool need_zero_pkt = false;
	bool first_trb = true;
	unsigned int num_trbs;
	unsigned int start_cycle, num_sgs = 0;
	unsigned int enqd_len, block_len, trb_buff_len, full_len;
	int sent_len, ret;
	u32 field, length_field, remainder;
	u64 addr, send_addr;

	ring = xhci_urb_to_transfer_ring(xhci, urb);
	if (!ring)
		return -EINVAL;

	full_len = urb->transfer_buffer_length;
	 
	if (urb->num_sgs && !(urb->transfer_flags & URB_DMA_MAP_SINGLE)) {
		num_sgs = urb->num_mapped_sgs;
		sg = urb->sg;
		addr = (u64) sg_dma_address(sg);
		block_len = sg_dma_len(sg);
		num_trbs = count_sg_trbs_needed(urb);
	} else {
		num_trbs = count_trbs_needed(urb);
		addr = (u64) urb->transfer_dma;
		block_len = full_len;
	}
	ret = prepare_transfer(xhci, xhci->devs[slot_id],
			ep_index, urb->stream_id,
			num_trbs, urb, 0, mem_flags);
	if (unlikely(ret < 0))
		return ret;

	urb_priv = urb->hcpriv;

	 
	if (urb->transfer_flags & URB_ZERO_PACKET && urb_priv->num_tds > 1)
		need_zero_pkt = true;

	td = &urb_priv->td[0];

	 
	start_trb = &ring->enqueue->generic;
	start_cycle = ring->cycle_state;
	send_addr = addr;

	 
	for (enqd_len = 0; first_trb || enqd_len < full_len;
			enqd_len += trb_buff_len) {
		field = TRB_TYPE(TRB_NORMAL);

		 
		trb_buff_len = TRB_BUFF_LEN_UP_TO_BOUNDARY(addr);
		trb_buff_len = min_t(unsigned int, trb_buff_len, block_len);

		if (enqd_len + trb_buff_len > full_len)
			trb_buff_len = full_len - enqd_len;

		 
		if (first_trb) {
			first_trb = false;
			if (start_cycle == 0)
				field |= TRB_CYCLE;
		} else
			field |= ring->cycle_state;

		 
		if (enqd_len + trb_buff_len < full_len) {
			field |= TRB_CHAIN;
			if (trb_is_link(ring->enqueue + 1)) {
				if (xhci_align_td(xhci, urb, enqd_len,
						  &trb_buff_len,
						  ring->enq_seg)) {
					send_addr = ring->enq_seg->bounce_dma;
					 
					td->bounce_seg = ring->enq_seg;
				}
			}
		}
		if (enqd_len + trb_buff_len >= full_len) {
			field &= ~TRB_CHAIN;
			field |= TRB_IOC;
			more_trbs_coming = false;
			td->last_trb = ring->enqueue;
			td->last_trb_seg = ring->enq_seg;
			if (xhci_urb_suitable_for_idt(urb)) {
				memcpy(&send_addr, urb->transfer_buffer,
				       trb_buff_len);
				le64_to_cpus(&send_addr);
				field |= TRB_IDT;
			}
		}

		 
		if (usb_urb_dir_in(urb))
			field |= TRB_ISP;

		 
		remainder = xhci_td_remainder(xhci, enqd_len, trb_buff_len,
					      full_len, urb, more_trbs_coming);

		length_field = TRB_LEN(trb_buff_len) |
			TRB_TD_SIZE(remainder) |
			TRB_INTR_TARGET(0);

		queue_trb(xhci, ring, more_trbs_coming | need_zero_pkt,
				lower_32_bits(send_addr),
				upper_32_bits(send_addr),
				length_field,
				field);
		td->num_trbs++;
		addr += trb_buff_len;
		sent_len = trb_buff_len;

		while (sg && sent_len >= block_len) {
			 
			--num_sgs;
			sent_len -= block_len;
			sg = sg_next(sg);
			if (num_sgs != 0 && sg) {
				block_len = sg_dma_len(sg);
				addr = (u64) sg_dma_address(sg);
				addr += sent_len;
			}
		}
		block_len -= sent_len;
		send_addr = addr;
	}

	if (need_zero_pkt) {
		ret = prepare_transfer(xhci, xhci->devs[slot_id],
				       ep_index, urb->stream_id,
				       1, urb, 1, mem_flags);
		urb_priv->td[1].last_trb = ring->enqueue;
		urb_priv->td[1].last_trb_seg = ring->enq_seg;
		field = TRB_TYPE(TRB_NORMAL) | ring->cycle_state | TRB_IOC;
		queue_trb(xhci, ring, 0, 0, 0, TRB_INTR_TARGET(0), field);
		urb_priv->td[1].num_trbs++;
	}

	check_trb_math(urb, enqd_len);
	giveback_first_trb(xhci, slot_id, ep_index, urb->stream_id,
			start_cycle, start_trb);
	return 0;
}

 
int xhci_queue_ctrl_tx(struct xhci_hcd *xhci, gfp_t mem_flags,
		struct urb *urb, int slot_id, unsigned int ep_index)
{
	struct xhci_ring *ep_ring;
	int num_trbs;
	int ret;
	struct usb_ctrlrequest *setup;
	struct xhci_generic_trb *start_trb;
	int start_cycle;
	u32 field;
	struct urb_priv *urb_priv;
	struct xhci_td *td;

	ep_ring = xhci_urb_to_transfer_ring(xhci, urb);
	if (!ep_ring)
		return -EINVAL;

	 
	if (!urb->setup_packet)
		return -EINVAL;

	 
	num_trbs = 2;
	 
	if (urb->transfer_buffer_length > 0)
		num_trbs++;
	ret = prepare_transfer(xhci, xhci->devs[slot_id],
			ep_index, urb->stream_id,
			num_trbs, urb, 0, mem_flags);
	if (ret < 0)
		return ret;

	urb_priv = urb->hcpriv;
	td = &urb_priv->td[0];
	td->num_trbs = num_trbs;

	 
	start_trb = &ep_ring->enqueue->generic;
	start_cycle = ep_ring->cycle_state;

	 
	 
	setup = (struct usb_ctrlrequest *) urb->setup_packet;
	field = 0;
	field |= TRB_IDT | TRB_TYPE(TRB_SETUP);
	if (start_cycle == 0)
		field |= 0x1;

	 
	if ((xhci->hci_version >= 0x100) || (xhci->quirks & XHCI_MTK_HOST)) {
		if (urb->transfer_buffer_length > 0) {
			if (setup->bRequestType & USB_DIR_IN)
				field |= TRB_TX_TYPE(TRB_DATA_IN);
			else
				field |= TRB_TX_TYPE(TRB_DATA_OUT);
		}
	}

	queue_trb(xhci, ep_ring, true,
		  setup->bRequestType | setup->bRequest << 8 | le16_to_cpu(setup->wValue) << 16,
		  le16_to_cpu(setup->wIndex) | le16_to_cpu(setup->wLength) << 16,
		  TRB_LEN(8) | TRB_INTR_TARGET(0),
		   
		  field);

	 
	 
	if (usb_urb_dir_in(urb))
		field = TRB_ISP | TRB_TYPE(TRB_DATA);
	else
		field = TRB_TYPE(TRB_DATA);

	if (urb->transfer_buffer_length > 0) {
		u32 length_field, remainder;
		u64 addr;

		if (xhci_urb_suitable_for_idt(urb)) {
			memcpy(&addr, urb->transfer_buffer,
			       urb->transfer_buffer_length);
			le64_to_cpus(&addr);
			field |= TRB_IDT;
		} else {
			addr = (u64) urb->transfer_dma;
		}

		remainder = xhci_td_remainder(xhci, 0,
				urb->transfer_buffer_length,
				urb->transfer_buffer_length,
				urb, 1);
		length_field = TRB_LEN(urb->transfer_buffer_length) |
				TRB_TD_SIZE(remainder) |
				TRB_INTR_TARGET(0);
		if (setup->bRequestType & USB_DIR_IN)
			field |= TRB_DIR_IN;
		queue_trb(xhci, ep_ring, true,
				lower_32_bits(addr),
				upper_32_bits(addr),
				length_field,
				field | ep_ring->cycle_state);
	}

	 
	td->last_trb = ep_ring->enqueue;
	td->last_trb_seg = ep_ring->enq_seg;

	 
	 
	if (urb->transfer_buffer_length > 0 && setup->bRequestType & USB_DIR_IN)
		field = 0;
	else
		field = TRB_DIR_IN;
	queue_trb(xhci, ep_ring, false,
			0,
			0,
			TRB_INTR_TARGET(0),
			 
			field | TRB_IOC | TRB_TYPE(TRB_STATUS) | ep_ring->cycle_state);

	giveback_first_trb(xhci, slot_id, ep_index, 0,
			start_cycle, start_trb);
	return 0;
}

 
static unsigned int xhci_get_burst_count(struct xhci_hcd *xhci,
		struct urb *urb, unsigned int total_packet_count)
{
	unsigned int max_burst;

	if (xhci->hci_version < 0x100 || urb->dev->speed < USB_SPEED_SUPER)
		return 0;

	max_burst = urb->ep->ss_ep_comp.bMaxBurst;
	return DIV_ROUND_UP(total_packet_count, max_burst + 1) - 1;
}

 
static unsigned int xhci_get_last_burst_packet_count(struct xhci_hcd *xhci,
		struct urb *urb, unsigned int total_packet_count)
{
	unsigned int max_burst;
	unsigned int residue;

	if (xhci->hci_version < 0x100)
		return 0;

	if (urb->dev->speed >= USB_SPEED_SUPER) {
		 
		max_burst = urb->ep->ss_ep_comp.bMaxBurst;
		residue = total_packet_count % (max_burst + 1);
		 
		if (residue == 0)
			return max_burst;
		return residue - 1;
	}
	if (total_packet_count == 0)
		return 0;
	return total_packet_count - 1;
}

 
static int xhci_get_isoc_frame_id(struct xhci_hcd *xhci,
		struct urb *urb, int index)
{
	int start_frame, ist, ret = 0;
	int start_frame_id, end_frame_id, current_frame_id;

	if (urb->dev->speed == USB_SPEED_LOW ||
			urb->dev->speed == USB_SPEED_FULL)
		start_frame = urb->start_frame + index * urb->interval;
	else
		start_frame = (urb->start_frame + index * urb->interval) >> 3;

	 
	ist = HCS_IST(xhci->hcs_params2) & 0x7;
	if (HCS_IST(xhci->hcs_params2) & (1 << 3))
		ist <<= 3;

	 
	current_frame_id = readl(&xhci->run_regs->microframe_index);
	start_frame_id = roundup(current_frame_id + ist + 1, 8);
	end_frame_id = rounddown(current_frame_id + 895 * 8, 8);

	start_frame &= 0x7ff;
	start_frame_id = (start_frame_id >> 3) & 0x7ff;
	end_frame_id = (end_frame_id >> 3) & 0x7ff;

	xhci_dbg(xhci, "%s: index %d, reg 0x%x start_frame_id 0x%x, end_frame_id 0x%x, start_frame 0x%x\n",
		 __func__, index, readl(&xhci->run_regs->microframe_index),
		 start_frame_id, end_frame_id, start_frame);

	if (start_frame_id < end_frame_id) {
		if (start_frame > end_frame_id ||
				start_frame < start_frame_id)
			ret = -EINVAL;
	} else if (start_frame_id > end_frame_id) {
		if ((start_frame > end_frame_id &&
				start_frame < start_frame_id))
			ret = -EINVAL;
	} else {
			ret = -EINVAL;
	}

	if (index == 0) {
		if (ret == -EINVAL || start_frame == start_frame_id) {
			start_frame = start_frame_id + 1;
			if (urb->dev->speed == USB_SPEED_LOW ||
					urb->dev->speed == USB_SPEED_FULL)
				urb->start_frame = start_frame;
			else
				urb->start_frame = start_frame << 3;
			ret = 0;
		}
	}

	if (ret) {
		xhci_warn(xhci, "Frame ID %d (reg %d, index %d) beyond range (%d, %d)\n",
				start_frame, current_frame_id, index,
				start_frame_id, end_frame_id);
		xhci_warn(xhci, "Ignore frame ID field, use SIA bit instead\n");
		return ret;
	}

	return start_frame;
}

 
static bool trb_block_event_intr(struct xhci_hcd *xhci, int num_tds, int i)
{
	if (xhci->hci_version < 0x100)
		return false;
	 
	if (i == num_tds - 1)
		return false;
	 
	if (i && xhci->quirks & XHCI_AVOID_BEI)
		return !!(i % xhci->isoc_bei_interval);

	return true;
}

 
static int xhci_queue_isoc_tx(struct xhci_hcd *xhci, gfp_t mem_flags,
		struct urb *urb, int slot_id, unsigned int ep_index)
{
	struct xhci_ring *ep_ring;
	struct urb_priv *urb_priv;
	struct xhci_td *td;
	int num_tds, trbs_per_td;
	struct xhci_generic_trb *start_trb;
	bool first_trb;
	int start_cycle;
	u32 field, length_field;
	int running_total, trb_buff_len, td_len, td_remain_len, ret;
	u64 start_addr, addr;
	int i, j;
	bool more_trbs_coming;
	struct xhci_virt_ep *xep;
	int frame_id;

	xep = &xhci->devs[slot_id]->eps[ep_index];
	ep_ring = xhci->devs[slot_id]->eps[ep_index].ring;

	num_tds = urb->number_of_packets;
	if (num_tds < 1) {
		xhci_dbg(xhci, "Isoc URB with zero packets?\n");
		return -EINVAL;
	}
	start_addr = (u64) urb->transfer_dma;
	start_trb = &ep_ring->enqueue->generic;
	start_cycle = ep_ring->cycle_state;

	urb_priv = urb->hcpriv;
	 
	for (i = 0; i < num_tds; i++) {
		unsigned int total_pkt_count, max_pkt;
		unsigned int burst_count, last_burst_pkt_count;
		u32 sia_frame_id;

		first_trb = true;
		running_total = 0;
		addr = start_addr + urb->iso_frame_desc[i].offset;
		td_len = urb->iso_frame_desc[i].length;
		td_remain_len = td_len;
		max_pkt = usb_endpoint_maxp(&urb->ep->desc);
		total_pkt_count = DIV_ROUND_UP(td_len, max_pkt);

		 
		if (total_pkt_count == 0)
			total_pkt_count++;
		burst_count = xhci_get_burst_count(xhci, urb, total_pkt_count);
		last_burst_pkt_count = xhci_get_last_burst_packet_count(xhci,
							urb, total_pkt_count);

		trbs_per_td = count_isoc_trbs_needed(urb, i);

		ret = prepare_transfer(xhci, xhci->devs[slot_id], ep_index,
				urb->stream_id, trbs_per_td, urb, i, mem_flags);
		if (ret < 0) {
			if (i == 0)
				return ret;
			goto cleanup;
		}
		td = &urb_priv->td[i];
		td->num_trbs = trbs_per_td;
		 
		sia_frame_id = TRB_SIA;
		if (!(urb->transfer_flags & URB_ISO_ASAP) &&
		    HCC_CFC(xhci->hcc_params)) {
			frame_id = xhci_get_isoc_frame_id(xhci, urb, i);
			if (frame_id >= 0)
				sia_frame_id = TRB_FRAME_ID(frame_id);
		}
		 
		field = TRB_TYPE(TRB_ISOC) |
			TRB_TLBPC(last_burst_pkt_count) |
			sia_frame_id |
			(i ? ep_ring->cycle_state : !start_cycle);

		 
		if (!xep->use_extended_tbc)
			field |= TRB_TBC(burst_count);

		 
		for (j = 0; j < trbs_per_td; j++) {
			u32 remainder = 0;

			 
			if (!first_trb)
				field = TRB_TYPE(TRB_NORMAL) |
					ep_ring->cycle_state;

			 
			if (usb_urb_dir_in(urb))
				field |= TRB_ISP;

			 
			if (j < trbs_per_td - 1) {
				more_trbs_coming = true;
				field |= TRB_CHAIN;
			} else {
				more_trbs_coming = false;
				td->last_trb = ep_ring->enqueue;
				td->last_trb_seg = ep_ring->enq_seg;
				field |= TRB_IOC;
				if (trb_block_event_intr(xhci, num_tds, i))
					field |= TRB_BEI;
			}
			 
			trb_buff_len = TRB_BUFF_LEN_UP_TO_BOUNDARY(addr);
			if (trb_buff_len > td_remain_len)
				trb_buff_len = td_remain_len;

			 
			remainder = xhci_td_remainder(xhci, running_total,
						   trb_buff_len, td_len,
						   urb, more_trbs_coming);

			length_field = TRB_LEN(trb_buff_len) |
				TRB_INTR_TARGET(0);

			 
			if (first_trb && xep->use_extended_tbc)
				length_field |= TRB_TD_SIZE_TBC(burst_count);
			else
				length_field |= TRB_TD_SIZE(remainder);
			first_trb = false;

			queue_trb(xhci, ep_ring, more_trbs_coming,
				lower_32_bits(addr),
				upper_32_bits(addr),
				length_field,
				field);
			running_total += trb_buff_len;

			addr += trb_buff_len;
			td_remain_len -= trb_buff_len;
		}

		 
		if (running_total != td_len) {
			xhci_err(xhci, "ISOC TD length unmatch\n");
			ret = -EINVAL;
			goto cleanup;
		}
	}

	 
	if (HCC_CFC(xhci->hcc_params))
		xep->next_frame_id = urb->start_frame + num_tds * urb->interval;

	if (xhci_to_hcd(xhci)->self.bandwidth_isoc_reqs == 0) {
		if (xhci->quirks & XHCI_AMD_PLL_FIX)
			usb_amd_quirk_pll_disable();
	}
	xhci_to_hcd(xhci)->self.bandwidth_isoc_reqs++;

	giveback_first_trb(xhci, slot_id, ep_index, urb->stream_id,
			start_cycle, start_trb);
	return 0;
cleanup:
	 

	for (i--; i >= 0; i--)
		list_del_init(&urb_priv->td[i].td_list);

	 
	urb_priv->td[0].last_trb = ep_ring->enqueue;
	 
	td_to_noop(xhci, ep_ring, &urb_priv->td[0], true);

	 
	ep_ring->enqueue = urb_priv->td[0].first_trb;
	ep_ring->enq_seg = urb_priv->td[0].start_seg;
	ep_ring->cycle_state = start_cycle;
	usb_hcd_unlink_urb_from_ep(bus_to_hcd(urb->dev->bus), urb);
	return ret;
}

 
int xhci_queue_isoc_tx_prepare(struct xhci_hcd *xhci, gfp_t mem_flags,
		struct urb *urb, int slot_id, unsigned int ep_index)
{
	struct xhci_virt_device *xdev;
	struct xhci_ring *ep_ring;
	struct xhci_ep_ctx *ep_ctx;
	int start_frame;
	int num_tds, num_trbs, i;
	int ret;
	struct xhci_virt_ep *xep;
	int ist;

	xdev = xhci->devs[slot_id];
	xep = &xhci->devs[slot_id]->eps[ep_index];
	ep_ring = xdev->eps[ep_index].ring;
	ep_ctx = xhci_get_ep_ctx(xhci, xdev->out_ctx, ep_index);

	num_trbs = 0;
	num_tds = urb->number_of_packets;
	for (i = 0; i < num_tds; i++)
		num_trbs += count_isoc_trbs_needed(urb, i);

	 
	ret = prepare_ring(xhci, ep_ring, GET_EP_CTX_STATE(ep_ctx),
			   num_trbs, mem_flags);
	if (ret)
		return ret;

	 
	check_interval(xhci, urb, ep_ctx);

	 
	if (HCC_CFC(xhci->hcc_params) && !list_empty(&ep_ring->td_list)) {
		if (GET_EP_CTX_STATE(ep_ctx) ==	EP_STATE_RUNNING) {
			urb->start_frame = xep->next_frame_id;
			goto skip_start_over;
		}
	}

	start_frame = readl(&xhci->run_regs->microframe_index);
	start_frame &= 0x3fff;
	 
	ist = HCS_IST(xhci->hcs_params2) & 0x7;
	if (HCS_IST(xhci->hcs_params2) & (1 << 3))
		ist <<= 3;
	start_frame += ist + XHCI_CFC_DELAY;
	start_frame = roundup(start_frame, 8);

	 
	if (urb->dev->speed == USB_SPEED_LOW ||
			urb->dev->speed == USB_SPEED_FULL) {
		start_frame = roundup(start_frame, urb->interval << 3);
		urb->start_frame = start_frame >> 3;
	} else {
		start_frame = roundup(start_frame, urb->interval);
		urb->start_frame = start_frame;
	}

skip_start_over:

	return xhci_queue_isoc_tx(xhci, mem_flags, urb, slot_id, ep_index);
}

 

 
static int queue_command(struct xhci_hcd *xhci, struct xhci_command *cmd,
			 u32 field1, u32 field2,
			 u32 field3, u32 field4, bool command_must_succeed)
{
	int reserved_trbs = xhci->cmd_ring_reserved_trbs;
	int ret;

	if ((xhci->xhc_state & XHCI_STATE_DYING) ||
		(xhci->xhc_state & XHCI_STATE_HALTED)) {
		xhci_dbg(xhci, "xHCI dying or halted, can't queue_command\n");
		return -ESHUTDOWN;
	}

	if (!command_must_succeed)
		reserved_trbs++;

	ret = prepare_ring(xhci, xhci->cmd_ring, EP_STATE_RUNNING,
			reserved_trbs, GFP_ATOMIC);
	if (ret < 0) {
		xhci_err(xhci, "ERR: No room for command on command ring\n");
		if (command_must_succeed)
			xhci_err(xhci, "ERR: Reserved TRB counting for "
					"unfailable commands failed.\n");
		return ret;
	}

	cmd->command_trb = xhci->cmd_ring->enqueue;

	 
	if (list_empty(&xhci->cmd_list)) {
		xhci->current_cmd = cmd;
		xhci_mod_cmd_timer(xhci, XHCI_CMD_DEFAULT_TIMEOUT);
	}

	list_add_tail(&cmd->cmd_list, &xhci->cmd_list);

	queue_trb(xhci, xhci->cmd_ring, false, field1, field2, field3,
			field4 | xhci->cmd_ring->cycle_state);
	return 0;
}

 
int xhci_queue_slot_control(struct xhci_hcd *xhci, struct xhci_command *cmd,
		u32 trb_type, u32 slot_id)
{
	return queue_command(xhci, cmd, 0, 0, 0,
			TRB_TYPE(trb_type) | SLOT_ID_FOR_TRB(slot_id), false);
}

 
int xhci_queue_address_device(struct xhci_hcd *xhci, struct xhci_command *cmd,
		dma_addr_t in_ctx_ptr, u32 slot_id, enum xhci_setup_dev setup)
{
	return queue_command(xhci, cmd, lower_32_bits(in_ctx_ptr),
			upper_32_bits(in_ctx_ptr), 0,
			TRB_TYPE(TRB_ADDR_DEV) | SLOT_ID_FOR_TRB(slot_id)
			| (setup == SETUP_CONTEXT_ONLY ? TRB_BSR : 0), false);
}

int xhci_queue_vendor_command(struct xhci_hcd *xhci, struct xhci_command *cmd,
		u32 field1, u32 field2, u32 field3, u32 field4)
{
	return queue_command(xhci, cmd, field1, field2, field3, field4, false);
}

 
int xhci_queue_reset_device(struct xhci_hcd *xhci, struct xhci_command *cmd,
		u32 slot_id)
{
	return queue_command(xhci, cmd, 0, 0, 0,
			TRB_TYPE(TRB_RESET_DEV) | SLOT_ID_FOR_TRB(slot_id),
			false);
}

 
int xhci_queue_configure_endpoint(struct xhci_hcd *xhci,
		struct xhci_command *cmd, dma_addr_t in_ctx_ptr,
		u32 slot_id, bool command_must_succeed)
{
	return queue_command(xhci, cmd, lower_32_bits(in_ctx_ptr),
			upper_32_bits(in_ctx_ptr), 0,
			TRB_TYPE(TRB_CONFIG_EP) | SLOT_ID_FOR_TRB(slot_id),
			command_must_succeed);
}

 
int xhci_queue_evaluate_context(struct xhci_hcd *xhci, struct xhci_command *cmd,
		dma_addr_t in_ctx_ptr, u32 slot_id, bool command_must_succeed)
{
	return queue_command(xhci, cmd, lower_32_bits(in_ctx_ptr),
			upper_32_bits(in_ctx_ptr), 0,
			TRB_TYPE(TRB_EVAL_CONTEXT) | SLOT_ID_FOR_TRB(slot_id),
			command_must_succeed);
}

 
int xhci_queue_stop_endpoint(struct xhci_hcd *xhci, struct xhci_command *cmd,
			     int slot_id, unsigned int ep_index, int suspend)
{
	u32 trb_slot_id = SLOT_ID_FOR_TRB(slot_id);
	u32 trb_ep_index = EP_ID_FOR_TRB(ep_index);
	u32 type = TRB_TYPE(TRB_STOP_RING);
	u32 trb_suspend = SUSPEND_PORT_FOR_TRB(suspend);

	return queue_command(xhci, cmd, 0, 0, 0,
			trb_slot_id | trb_ep_index | type | trb_suspend, false);
}

int xhci_queue_reset_ep(struct xhci_hcd *xhci, struct xhci_command *cmd,
			int slot_id, unsigned int ep_index,
			enum xhci_ep_reset_type reset_type)
{
	u32 trb_slot_id = SLOT_ID_FOR_TRB(slot_id);
	u32 trb_ep_index = EP_ID_FOR_TRB(ep_index);
	u32 type = TRB_TYPE(TRB_RESET_EP);

	if (reset_type == EP_SOFT_RESET)
		type |= TRB_TSP;

	return queue_command(xhci, cmd, 0, 0, 0,
			trb_slot_id | trb_ep_index | type, false);
}
