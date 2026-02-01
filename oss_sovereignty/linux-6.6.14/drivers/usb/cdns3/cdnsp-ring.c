
 

 

#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/irq.h>

#include "cdnsp-trace.h"
#include "cdnsp-gadget.h"

 
dma_addr_t cdnsp_trb_virt_to_dma(struct cdnsp_segment *seg,
				 union cdnsp_trb *trb)
{
	unsigned long segment_offset = trb - seg->trbs;

	if (trb < seg->trbs || segment_offset >= TRBS_PER_SEGMENT)
		return 0;

	return seg->dma + (segment_offset * sizeof(*trb));
}

static bool cdnsp_trb_is_noop(union cdnsp_trb *trb)
{
	return TRB_TYPE_NOOP_LE32(trb->generic.field[3]);
}

static bool cdnsp_trb_is_link(union cdnsp_trb *trb)
{
	return TRB_TYPE_LINK_LE32(trb->link.control);
}

bool cdnsp_last_trb_on_seg(struct cdnsp_segment *seg, union cdnsp_trb *trb)
{
	return trb == &seg->trbs[TRBS_PER_SEGMENT - 1];
}

bool cdnsp_last_trb_on_ring(struct cdnsp_ring *ring,
			    struct cdnsp_segment *seg,
			    union cdnsp_trb *trb)
{
	return cdnsp_last_trb_on_seg(seg, trb) && (seg->next == ring->first_seg);
}

static bool cdnsp_link_trb_toggles_cycle(union cdnsp_trb *trb)
{
	return le32_to_cpu(trb->link.control) & LINK_TOGGLE;
}

static void cdnsp_trb_to_noop(union cdnsp_trb *trb, u32 noop_type)
{
	if (cdnsp_trb_is_link(trb)) {
		 
		trb->link.control &= cpu_to_le32(~TRB_CHAIN);
	} else {
		trb->generic.field[0] = 0;
		trb->generic.field[1] = 0;
		trb->generic.field[2] = 0;
		 
		trb->generic.field[3] &= cpu_to_le32(TRB_CYCLE);
		trb->generic.field[3] |= cpu_to_le32(TRB_TYPE(noop_type));
	}
}

 
static void cdnsp_next_trb(struct cdnsp_device *pdev,
			   struct cdnsp_ring *ring,
			   struct cdnsp_segment **seg,
			   union cdnsp_trb **trb)
{
	if (cdnsp_trb_is_link(*trb)) {
		*seg = (*seg)->next;
		*trb = ((*seg)->trbs);
	} else {
		(*trb)++;
	}
}

 
void cdnsp_inc_deq(struct cdnsp_device *pdev, struct cdnsp_ring *ring)
{
	 
	if (ring->type == TYPE_EVENT) {
		if (!cdnsp_last_trb_on_seg(ring->deq_seg, ring->dequeue)) {
			ring->dequeue++;
			goto out;
		}

		if (cdnsp_last_trb_on_ring(ring, ring->deq_seg, ring->dequeue))
			ring->cycle_state ^= 1;

		ring->deq_seg = ring->deq_seg->next;
		ring->dequeue = ring->deq_seg->trbs;
		goto out;
	}

	 
	if (!cdnsp_trb_is_link(ring->dequeue)) {
		ring->dequeue++;
		ring->num_trbs_free++;
	}
	while (cdnsp_trb_is_link(ring->dequeue)) {
		ring->deq_seg = ring->deq_seg->next;
		ring->dequeue = ring->deq_seg->trbs;
	}
out:
	trace_cdnsp_inc_deq(ring);
}

 
static void cdnsp_inc_enq(struct cdnsp_device *pdev,
			  struct cdnsp_ring *ring,
			  bool more_trbs_coming)
{
	union cdnsp_trb *next;
	u32 chain;

	chain = le32_to_cpu(ring->enqueue->generic.field[3]) & TRB_CHAIN;

	 
	if (!cdnsp_trb_is_link(ring->enqueue))
		ring->num_trbs_free--;
	next = ++(ring->enqueue);

	 
	while (cdnsp_trb_is_link(next)) {
		 
		if (!chain && !more_trbs_coming)
			break;

		next->link.control &= cpu_to_le32(~TRB_CHAIN);
		next->link.control |= cpu_to_le32(chain);

		 
		wmb();
		next->link.control ^= cpu_to_le32(TRB_CYCLE);

		 
		if (cdnsp_link_trb_toggles_cycle(next))
			ring->cycle_state ^= 1;

		ring->enq_seg = ring->enq_seg->next;
		ring->enqueue = ring->enq_seg->trbs;
		next = ring->enqueue;
	}

	trace_cdnsp_inc_enq(ring);
}

 
static bool cdnsp_room_on_ring(struct cdnsp_device *pdev,
			       struct cdnsp_ring *ring,
			       unsigned int num_trbs)
{
	int num_trbs_in_deq_seg;

	if (ring->num_trbs_free < num_trbs)
		return false;

	if (ring->type != TYPE_COMMAND && ring->type != TYPE_EVENT) {
		num_trbs_in_deq_seg = ring->dequeue - ring->deq_seg->trbs;

		if (ring->num_trbs_free < num_trbs + num_trbs_in_deq_seg)
			return false;
	}

	return true;
}

 
static void cdnsp_force_l0_go(struct cdnsp_device *pdev)
{
	if (pdev->active_port == &pdev->usb2_port && pdev->gadget.lpm_capable)
		cdnsp_set_link_state(pdev, &pdev->active_port->regs->portsc, XDEV_U0);
}

 
void cdnsp_ring_cmd_db(struct cdnsp_device *pdev)
{
	writel(DB_VALUE_CMD, &pdev->dba->cmd_db);
}

 
static bool cdnsp_ring_ep_doorbell(struct cdnsp_device *pdev,
				   struct cdnsp_ep *pep,
				   unsigned int stream_id)
{
	__le32 __iomem *reg_addr = &pdev->dba->ep_db;
	unsigned int ep_state = pep->ep_state;
	unsigned int db_value;

	 
	if (ep_state & EP_HALTED || !(ep_state & EP_ENABLED))
		return false;

	 
	if (pep->ep_state & EP_HAS_STREAMS) {
		if (pep->stream_info.drbls_count >= 2)
			return false;

		pep->stream_info.drbls_count++;
	}

	pep->ep_state &= ~EP_STOPPED;

	if (pep->idx == 0 && pdev->ep0_stage == CDNSP_DATA_STAGE &&
	    !pdev->ep0_expect_in)
		db_value = DB_VALUE_EP0_OUT(pep->idx, stream_id);
	else
		db_value = DB_VALUE(pep->idx, stream_id);

	trace_cdnsp_tr_drbl(pep, stream_id);

	writel(db_value, reg_addr);

	cdnsp_force_l0_go(pdev);

	 
	return true;
}

 
static struct cdnsp_ring *cdnsp_get_transfer_ring(struct cdnsp_device *pdev,
						  struct cdnsp_ep *pep,
						  unsigned int stream_id)
{
	if (!(pep->ep_state & EP_HAS_STREAMS))
		return pep->ring;

	if (stream_id == 0 || stream_id >= pep->stream_info.num_streams) {
		dev_err(pdev->dev, "ERR: %s ring doesn't exist for SID: %d.\n",
			pep->name, stream_id);
		return NULL;
	}

	return pep->stream_info.stream_rings[stream_id];
}

static struct cdnsp_ring *
	cdnsp_request_to_transfer_ring(struct cdnsp_device *pdev,
				       struct cdnsp_request *preq)
{
	return cdnsp_get_transfer_ring(pdev, preq->pep,
				       preq->request.stream_id);
}

 
void cdnsp_ring_doorbell_for_active_rings(struct cdnsp_device *pdev,
					  struct cdnsp_ep *pep)
{
	struct cdnsp_stream_info *stream_info;
	unsigned int stream_id;
	int ret;

	if (pep->ep_state & EP_DIS_IN_RROGRESS)
		return;

	 
	if (!(pep->ep_state & EP_HAS_STREAMS) && pep->number) {
		if (pep->ring && !list_empty(&pep->ring->td_list))
			cdnsp_ring_ep_doorbell(pdev, pep, 0);
		return;
	}

	stream_info = &pep->stream_info;

	for (stream_id = 1; stream_id < stream_info->num_streams; stream_id++) {
		struct cdnsp_td *td, *td_temp;
		struct cdnsp_ring *ep_ring;

		if (stream_info->drbls_count >= 2)
			return;

		ep_ring = cdnsp_get_transfer_ring(pdev, pep, stream_id);
		if (!ep_ring)
			continue;

		if (!ep_ring->stream_active || ep_ring->stream_rejected)
			continue;

		list_for_each_entry_safe(td, td_temp, &ep_ring->td_list,
					 td_list) {
			if (td->drbl)
				continue;

			ret = cdnsp_ring_ep_doorbell(pdev, pep, stream_id);
			if (ret)
				td->drbl = 1;
		}
	}
}

 
static u64 cdnsp_get_hw_deq(struct cdnsp_device *pdev,
			    unsigned int ep_index,
			    unsigned int stream_id)
{
	struct cdnsp_stream_ctx *st_ctx;
	struct cdnsp_ep *pep;

	pep = &pdev->eps[stream_id];

	if (pep->ep_state & EP_HAS_STREAMS) {
		st_ctx = &pep->stream_info.stream_ctx_array[stream_id];
		return le64_to_cpu(st_ctx->stream_ring);
	}

	return le64_to_cpu(pep->out_ctx->deq);
}

 
static void cdnsp_find_new_dequeue_state(struct cdnsp_device *pdev,
					 struct cdnsp_ep *pep,
					 unsigned int stream_id,
					 struct cdnsp_td *cur_td,
					 struct cdnsp_dequeue_state *state)
{
	bool td_last_trb_found = false;
	struct cdnsp_segment *new_seg;
	struct cdnsp_ring *ep_ring;
	union cdnsp_trb *new_deq;
	bool cycle_found = false;
	u64 hw_dequeue;

	ep_ring = cdnsp_get_transfer_ring(pdev, pep, stream_id);
	if (!ep_ring)
		return;

	 
	hw_dequeue = cdnsp_get_hw_deq(pdev, pep->idx, stream_id);
	new_seg = ep_ring->deq_seg;
	new_deq = ep_ring->dequeue;
	state->new_cycle_state = hw_dequeue & 0x1;
	state->stream_id = stream_id;

	 
	do {
		if (!cycle_found && cdnsp_trb_virt_to_dma(new_seg, new_deq)
		    == (dma_addr_t)(hw_dequeue & ~0xf)) {
			cycle_found = true;

			if (td_last_trb_found)
				break;
		}

		if (new_deq == cur_td->last_trb)
			td_last_trb_found = true;

		if (cycle_found && cdnsp_trb_is_link(new_deq) &&
		    cdnsp_link_trb_toggles_cycle(new_deq))
			state->new_cycle_state ^= 0x1;

		cdnsp_next_trb(pdev, ep_ring, &new_seg, &new_deq);

		 
		if (new_deq == pep->ring->dequeue) {
			dev_err(pdev->dev,
				"Error: Failed finding new dequeue state\n");
			state->new_deq_seg = NULL;
			state->new_deq_ptr = NULL;
			return;
		}

	} while (!cycle_found || !td_last_trb_found);

	state->new_deq_seg = new_seg;
	state->new_deq_ptr = new_deq;

	trace_cdnsp_new_deq_state(state);
}

 
static void cdnsp_td_to_noop(struct cdnsp_device *pdev,
			     struct cdnsp_ring *ep_ring,
			     struct cdnsp_td *td,
			     bool flip_cycle)
{
	struct cdnsp_segment *seg = td->start_seg;
	union cdnsp_trb *trb = td->first_trb;

	while (1) {
		cdnsp_trb_to_noop(trb, TRB_TR_NOOP);

		 
		if (flip_cycle && trb != td->first_trb && trb != td->last_trb)
			trb->generic.field[3] ^= cpu_to_le32(TRB_CYCLE);

		if (trb == td->last_trb)
			break;

		cdnsp_next_trb(pdev, ep_ring, &seg, &trb);
	}
}

 
static struct cdnsp_segment *cdnsp_trb_in_td(struct cdnsp_device *pdev,
					     struct cdnsp_segment *start_seg,
					     union cdnsp_trb *start_trb,
					     union cdnsp_trb *end_trb,
					     dma_addr_t suspect_dma)
{
	struct cdnsp_segment *cur_seg;
	union cdnsp_trb *temp_trb;
	dma_addr_t end_seg_dma;
	dma_addr_t end_trb_dma;
	dma_addr_t start_dma;

	start_dma = cdnsp_trb_virt_to_dma(start_seg, start_trb);
	cur_seg = start_seg;

	do {
		if (start_dma == 0)
			return NULL;

		temp_trb = &cur_seg->trbs[TRBS_PER_SEGMENT - 1];
		 
		end_seg_dma = cdnsp_trb_virt_to_dma(cur_seg, temp_trb);
		 
		end_trb_dma = cdnsp_trb_virt_to_dma(cur_seg, end_trb);

		trace_cdnsp_looking_trb_in_td(suspect_dma, start_dma,
					      end_trb_dma, cur_seg->dma,
					      end_seg_dma);

		if (end_trb_dma > 0) {
			 
			if (start_dma <= end_trb_dma) {
				if (suspect_dma >= start_dma &&
				    suspect_dma <= end_trb_dma) {
					return cur_seg;
				}
			} else {
				 
				if ((suspect_dma >= start_dma &&
				     suspect_dma <= end_seg_dma) ||
				    (suspect_dma >= cur_seg->dma &&
				     suspect_dma <= end_trb_dma)) {
					return cur_seg;
				}
			}

			return NULL;
		}

		 
		if (suspect_dma >= start_dma && suspect_dma <= end_seg_dma)
			return cur_seg;

		cur_seg = cur_seg->next;
		start_dma = cdnsp_trb_virt_to_dma(cur_seg, &cur_seg->trbs[0]);
	} while (cur_seg != start_seg);

	return NULL;
}

static void cdnsp_unmap_td_bounce_buffer(struct cdnsp_device *pdev,
					 struct cdnsp_ring *ring,
					 struct cdnsp_td *td)
{
	struct cdnsp_segment *seg = td->bounce_seg;
	struct cdnsp_request *preq;
	size_t len;

	if (!seg)
		return;

	preq = td->preq;

	trace_cdnsp_bounce_unmap(td->preq, seg->bounce_len, seg->bounce_offs,
				 seg->bounce_dma, 0);

	if (!preq->direction) {
		dma_unmap_single(pdev->dev, seg->bounce_dma,
				 ring->bounce_buf_len,  DMA_TO_DEVICE);
		return;
	}

	dma_unmap_single(pdev->dev, seg->bounce_dma, ring->bounce_buf_len,
			 DMA_FROM_DEVICE);

	 
	len = sg_pcopy_from_buffer(preq->request.sg, preq->request.num_sgs,
				   seg->bounce_buf, seg->bounce_len,
				   seg->bounce_offs);
	if (len != seg->bounce_len)
		dev_warn(pdev->dev, "WARN Wrong bounce buffer read length: %zu != %d\n",
			 len, seg->bounce_len);

	seg->bounce_len = 0;
	seg->bounce_offs = 0;
}

static int cdnsp_cmd_set_deq(struct cdnsp_device *pdev,
			     struct cdnsp_ep *pep,
			     struct cdnsp_dequeue_state *deq_state)
{
	struct cdnsp_ring *ep_ring;
	int ret;

	if (!deq_state->new_deq_ptr || !deq_state->new_deq_seg) {
		cdnsp_ring_doorbell_for_active_rings(pdev, pep);
		return 0;
	}

	cdnsp_queue_new_dequeue_state(pdev, pep, deq_state);
	cdnsp_ring_cmd_db(pdev);
	ret = cdnsp_wait_for_cmd_compl(pdev);

	trace_cdnsp_handle_cmd_set_deq(cdnsp_get_slot_ctx(&pdev->out_ctx));
	trace_cdnsp_handle_cmd_set_deq_ep(pep->out_ctx);

	 
	ep_ring = cdnsp_get_transfer_ring(pdev, pep, deq_state->stream_id);

	if (cdnsp_trb_is_link(ep_ring->dequeue)) {
		ep_ring->deq_seg = ep_ring->deq_seg->next;
		ep_ring->dequeue = ep_ring->deq_seg->trbs;
	}

	while (ep_ring->dequeue != deq_state->new_deq_ptr) {
		ep_ring->num_trbs_free++;
		ep_ring->dequeue++;

		if (cdnsp_trb_is_link(ep_ring->dequeue)) {
			if (ep_ring->dequeue == deq_state->new_deq_ptr)
				break;

			ep_ring->deq_seg = ep_ring->deq_seg->next;
			ep_ring->dequeue = ep_ring->deq_seg->trbs;
		}
	}

	 
	if (ret)
		return -ESHUTDOWN;

	 
	cdnsp_ring_doorbell_for_active_rings(pdev, pep);

	return 0;
}

int cdnsp_remove_request(struct cdnsp_device *pdev,
			 struct cdnsp_request *preq,
			 struct cdnsp_ep *pep)
{
	struct cdnsp_dequeue_state deq_state;
	struct cdnsp_td *cur_td = NULL;
	struct cdnsp_ring *ep_ring;
	struct cdnsp_segment *seg;
	int status = -ECONNRESET;
	int ret = 0;
	u64 hw_deq;

	memset(&deq_state, 0, sizeof(deq_state));

	trace_cdnsp_remove_request(pep->out_ctx);
	trace_cdnsp_remove_request_td(preq);

	cur_td = &preq->td;
	ep_ring = cdnsp_request_to_transfer_ring(pdev, preq);

	 
	hw_deq = cdnsp_get_hw_deq(pdev, pep->idx, preq->request.stream_id);
	hw_deq &= ~0xf;

	seg = cdnsp_trb_in_td(pdev, cur_td->start_seg, cur_td->first_trb,
			      cur_td->last_trb, hw_deq);

	if (seg && (pep->ep_state & EP_ENABLED))
		cdnsp_find_new_dequeue_state(pdev, pep, preq->request.stream_id,
					     cur_td, &deq_state);
	else
		cdnsp_td_to_noop(pdev, ep_ring, cur_td, false);

	 
	list_del_init(&cur_td->td_list);
	ep_ring->num_tds--;
	pep->stream_info.td_count--;

	 
	if (pdev->cdnsp_state & CDNSP_STATE_DISCONNECT_PENDING) {
		status = -ESHUTDOWN;
		ret = cdnsp_cmd_set_deq(pdev, pep, &deq_state);
	}

	cdnsp_unmap_td_bounce_buffer(pdev, ep_ring, cur_td);
	cdnsp_gadget_giveback(pep, cur_td->preq, status);

	return ret;
}

static int cdnsp_update_port_id(struct cdnsp_device *pdev, u32 port_id)
{
	struct cdnsp_port *port = pdev->active_port;
	u8 old_port = 0;

	if (port && port->port_num == port_id)
		return 0;

	if (port)
		old_port = port->port_num;

	if (port_id == pdev->usb2_port.port_num) {
		port = &pdev->usb2_port;
	} else if (port_id == pdev->usb3_port.port_num) {
		port  = &pdev->usb3_port;
	} else {
		dev_err(pdev->dev, "Port event with invalid port ID %d\n",
			port_id);
		return -EINVAL;
	}

	if (port_id != old_port) {
		cdnsp_disable_slot(pdev);
		pdev->active_port = port;
		cdnsp_enable_slot(pdev);
	}

	if (port_id == pdev->usb2_port.port_num)
		cdnsp_set_usb2_hardware_lpm(pdev, NULL, 1);
	else
		writel(PORT_U1_TIMEOUT(1) | PORT_U2_TIMEOUT(1),
		       &pdev->usb3_port.regs->portpmsc);

	return 0;
}

static void cdnsp_handle_port_status(struct cdnsp_device *pdev,
				     union cdnsp_trb *event)
{
	struct cdnsp_port_regs __iomem *port_regs;
	u32 portsc, cmd_regs;
	bool port2 = false;
	u32 link_state;
	u32 port_id;

	 
	if (GET_COMP_CODE(le32_to_cpu(event->generic.field[2])) != COMP_SUCCESS)
		dev_err(pdev->dev, "ERR: incorrect PSC event\n");

	port_id = GET_PORT_ID(le32_to_cpu(event->generic.field[0]));

	if (cdnsp_update_port_id(pdev, port_id))
		goto cleanup;

	port_regs = pdev->active_port->regs;

	if (port_id == pdev->usb2_port.port_num)
		port2 = true;

new_event:
	portsc = readl(&port_regs->portsc);
	writel(cdnsp_port_state_to_neutral(portsc) |
	       (portsc & PORT_CHANGE_BITS), &port_regs->portsc);

	trace_cdnsp_handle_port_status(pdev->active_port->port_num, portsc);

	pdev->gadget.speed = cdnsp_port_speed(portsc);
	link_state = portsc & PORT_PLS_MASK;

	 
	if ((portsc & PORT_PLC)) {
		if (!(pdev->cdnsp_state & CDNSP_WAKEUP_PENDING)  &&
		    link_state == XDEV_RESUME) {
			cmd_regs = readl(&pdev->op_regs->command);
			if (!(cmd_regs & CMD_R_S))
				goto cleanup;

			if (DEV_SUPERSPEED_ANY(portsc)) {
				cdnsp_set_link_state(pdev, &port_regs->portsc,
						     XDEV_U0);

				cdnsp_resume_gadget(pdev);
			}
		}

		if ((pdev->cdnsp_state & CDNSP_WAKEUP_PENDING) &&
		    link_state == XDEV_U0) {
			pdev->cdnsp_state &= ~CDNSP_WAKEUP_PENDING;

			cdnsp_force_header_wakeup(pdev, 1);
			cdnsp_ring_cmd_db(pdev);
			cdnsp_wait_for_cmd_compl(pdev);
		}

		if (link_state == XDEV_U0 && pdev->link_state == XDEV_U3 &&
		    !DEV_SUPERSPEED_ANY(portsc))
			cdnsp_resume_gadget(pdev);

		if (link_state == XDEV_U3 &&  pdev->link_state != XDEV_U3)
			cdnsp_suspend_gadget(pdev);

		pdev->link_state = link_state;
	}

	if (portsc & PORT_CSC) {
		 
		if (pdev->gadget.connected && !(portsc & PORT_CONNECT))
			cdnsp_disconnect_gadget(pdev);

		 
		if (portsc & PORT_CONNECT) {
			if (!port2)
				cdnsp_irq_reset(pdev);

			usb_gadget_set_state(&pdev->gadget, USB_STATE_ATTACHED);
		}
	}

	 
	if ((portsc & (PORT_RC | PORT_WRC)) && (portsc & PORT_CONNECT)) {
		cdnsp_irq_reset(pdev);
		pdev->u1_allowed = 0;
		pdev->u2_allowed = 0;
		pdev->may_wakeup = 0;
	}

	if (portsc & PORT_CEC)
		dev_err(pdev->dev, "Port Over Current detected\n");

	if (portsc & PORT_CEC)
		dev_err(pdev->dev, "Port Configure Error detected\n");

	if (readl(&port_regs->portsc) & PORT_CHANGE_BITS)
		goto new_event;

cleanup:
	cdnsp_inc_deq(pdev, pdev->event_ring);
}

static void cdnsp_td_cleanup(struct cdnsp_device *pdev,
			     struct cdnsp_td *td,
			     struct cdnsp_ring *ep_ring,
			     int *status)
{
	struct cdnsp_request *preq = td->preq;

	 
	cdnsp_unmap_td_bounce_buffer(pdev, ep_ring, td);

	 
	if (preq->request.actual > preq->request.length) {
		preq->request.actual = 0;
		*status = 0;
	}

	list_del_init(&td->td_list);
	ep_ring->num_tds--;
	preq->pep->stream_info.td_count--;

	cdnsp_gadget_giveback(preq->pep, preq, *status);
}

static void cdnsp_finish_td(struct cdnsp_device *pdev,
			    struct cdnsp_td *td,
			    struct cdnsp_transfer_event *event,
			    struct cdnsp_ep *ep,
			    int *status)
{
	struct cdnsp_ring *ep_ring;
	u32 trb_comp_code;

	ep_ring = cdnsp_dma_to_transfer_ring(ep, le64_to_cpu(event->buffer));
	trb_comp_code = GET_COMP_CODE(le32_to_cpu(event->transfer_len));

	if (trb_comp_code == COMP_STOPPED_LENGTH_INVALID ||
	    trb_comp_code == COMP_STOPPED ||
	    trb_comp_code == COMP_STOPPED_SHORT_PACKET) {
		 
		return;
	}

	 
	while (ep_ring->dequeue != td->last_trb)
		cdnsp_inc_deq(pdev, ep_ring);

	cdnsp_inc_deq(pdev, ep_ring);

	cdnsp_td_cleanup(pdev, td, ep_ring, status);
}

 
static int cdnsp_sum_trb_lengths(struct cdnsp_device *pdev,
				 struct cdnsp_ring *ring,
				 union cdnsp_trb *stop_trb)
{
	struct cdnsp_segment *seg = ring->deq_seg;
	union cdnsp_trb *trb = ring->dequeue;
	u32 sum;

	for (sum = 0; trb != stop_trb; cdnsp_next_trb(pdev, ring, &seg, &trb)) {
		if (!cdnsp_trb_is_noop(trb) && !cdnsp_trb_is_link(trb))
			sum += TRB_LEN(le32_to_cpu(trb->generic.field[2]));
	}
	return sum;
}

static int cdnsp_giveback_first_trb(struct cdnsp_device *pdev,
				    struct cdnsp_ep *pep,
				    unsigned int stream_id,
				    int start_cycle,
				    struct cdnsp_generic_trb *start_trb)
{
	 
	wmb();

	if (start_cycle)
		start_trb->field[3] |= cpu_to_le32(start_cycle);
	else
		start_trb->field[3] &= cpu_to_le32(~TRB_CYCLE);

	if ((pep->ep_state & EP_HAS_STREAMS) &&
	    !pep->stream_info.first_prime_det) {
		trace_cdnsp_wait_for_prime(pep, stream_id);
		return 0;
	}

	return cdnsp_ring_ep_doorbell(pdev, pep, stream_id);
}

 
static void cdnsp_process_ctrl_td(struct cdnsp_device *pdev,
				  struct cdnsp_td *td,
				  union cdnsp_trb *event_trb,
				  struct cdnsp_transfer_event *event,
				  struct cdnsp_ep *pep,
				  int *status)
{
	struct cdnsp_ring *ep_ring;
	u32 remaining;
	u32 trb_type;

	trb_type = TRB_FIELD_TO_TYPE(le32_to_cpu(event_trb->generic.field[3]));
	ep_ring = cdnsp_dma_to_transfer_ring(pep, le64_to_cpu(event->buffer));
	remaining = EVENT_TRB_LEN(le32_to_cpu(event->transfer_len));

	 
	if (trb_type == TRB_DATA) {
		td->request_length_set = true;
		td->preq->request.actual = td->preq->request.length - remaining;
	}

	 
	if (!td->request_length_set)
		td->preq->request.actual = td->preq->request.length;

	if (pdev->ep0_stage == CDNSP_DATA_STAGE && pep->number == 0 &&
	    pdev->three_stage_setup) {
		td = list_entry(ep_ring->td_list.next, struct cdnsp_td,
				td_list);
		pdev->ep0_stage = CDNSP_STATUS_STAGE;

		cdnsp_giveback_first_trb(pdev, pep, 0, ep_ring->cycle_state,
					 &td->last_trb->generic);
		return;
	}

	*status = 0;

	cdnsp_finish_td(pdev, td, event, pep, status);
}

 
static void cdnsp_process_isoc_td(struct cdnsp_device *pdev,
				  struct cdnsp_td *td,
				  union cdnsp_trb *ep_trb,
				  struct cdnsp_transfer_event *event,
				  struct cdnsp_ep *pep,
				  int status)
{
	struct cdnsp_request *preq = td->preq;
	u32 remaining, requested, ep_trb_len;
	bool sum_trbs_for_length = false;
	struct cdnsp_ring *ep_ring;
	u32 trb_comp_code;
	u32 td_length;

	ep_ring = cdnsp_dma_to_transfer_ring(pep, le64_to_cpu(event->buffer));
	trb_comp_code = GET_COMP_CODE(le32_to_cpu(event->transfer_len));
	remaining = EVENT_TRB_LEN(le32_to_cpu(event->transfer_len));
	ep_trb_len = TRB_LEN(le32_to_cpu(ep_trb->generic.field[2]));

	requested = preq->request.length;

	 
	switch (trb_comp_code) {
	case COMP_SUCCESS:
		preq->request.status = 0;
		break;
	case COMP_SHORT_PACKET:
		preq->request.status = 0;
		sum_trbs_for_length = true;
		break;
	case COMP_ISOCH_BUFFER_OVERRUN:
	case COMP_BABBLE_DETECTED_ERROR:
		preq->request.status = -EOVERFLOW;
		break;
	case COMP_STOPPED:
		sum_trbs_for_length = true;
		break;
	case COMP_STOPPED_SHORT_PACKET:
		 
		preq->request.status  = 0;
		requested = remaining;
		break;
	case COMP_STOPPED_LENGTH_INVALID:
		requested = 0;
		remaining = 0;
		break;
	default:
		sum_trbs_for_length = true;
		preq->request.status = -1;
		break;
	}

	if (sum_trbs_for_length) {
		td_length = cdnsp_sum_trb_lengths(pdev, ep_ring, ep_trb);
		td_length += ep_trb_len - remaining;
	} else {
		td_length = requested;
	}

	td->preq->request.actual += td_length;

	cdnsp_finish_td(pdev, td, event, pep, &status);
}

static void cdnsp_skip_isoc_td(struct cdnsp_device *pdev,
			       struct cdnsp_td *td,
			       struct cdnsp_transfer_event *event,
			       struct cdnsp_ep *pep,
			       int status)
{
	struct cdnsp_ring *ep_ring;

	ep_ring = cdnsp_dma_to_transfer_ring(pep, le64_to_cpu(event->buffer));
	td->preq->request.status = -EXDEV;
	td->preq->request.actual = 0;

	 
	while (ep_ring->dequeue != td->last_trb)
		cdnsp_inc_deq(pdev, ep_ring);

	cdnsp_inc_deq(pdev, ep_ring);

	cdnsp_td_cleanup(pdev, td, ep_ring, &status);
}

 
static void cdnsp_process_bulk_intr_td(struct cdnsp_device *pdev,
				       struct cdnsp_td *td,
				       union cdnsp_trb *ep_trb,
				       struct cdnsp_transfer_event *event,
				       struct cdnsp_ep *ep,
				       int *status)
{
	u32 remaining, requested, ep_trb_len;
	struct cdnsp_ring *ep_ring;
	u32 trb_comp_code;

	ep_ring = cdnsp_dma_to_transfer_ring(ep, le64_to_cpu(event->buffer));
	trb_comp_code = GET_COMP_CODE(le32_to_cpu(event->transfer_len));
	remaining = EVENT_TRB_LEN(le32_to_cpu(event->transfer_len));
	ep_trb_len = TRB_LEN(le32_to_cpu(ep_trb->generic.field[2]));
	requested = td->preq->request.length;

	switch (trb_comp_code) {
	case COMP_SUCCESS:
	case COMP_SHORT_PACKET:
		*status = 0;
		break;
	case COMP_STOPPED_SHORT_PACKET:
		td->preq->request.actual = remaining;
		goto finish_td;
	case COMP_STOPPED_LENGTH_INVALID:
		 
		ep_trb_len = 0;
		remaining = 0;
		break;
	}

	if (ep_trb == td->last_trb)
		ep_trb_len = requested - remaining;
	else
		ep_trb_len = cdnsp_sum_trb_lengths(pdev, ep_ring, ep_trb) +
						   ep_trb_len - remaining;
	td->preq->request.actual = ep_trb_len;

finish_td:
	ep->stream_info.drbls_count--;

	cdnsp_finish_td(pdev, td, event, ep, status);
}

static void cdnsp_handle_tx_nrdy(struct cdnsp_device *pdev,
				 struct cdnsp_transfer_event *event)
{
	struct cdnsp_generic_trb *generic;
	struct cdnsp_ring *ep_ring;
	struct cdnsp_ep *pep;
	int cur_stream;
	int ep_index;
	int host_sid;
	int dev_sid;

	generic = (struct cdnsp_generic_trb *)event;
	ep_index = TRB_TO_EP_ID(le32_to_cpu(event->flags)) - 1;
	dev_sid = TRB_TO_DEV_STREAM(le32_to_cpu(generic->field[0]));
	host_sid = TRB_TO_HOST_STREAM(le32_to_cpu(generic->field[2]));

	pep = &pdev->eps[ep_index];

	if (!(pep->ep_state & EP_HAS_STREAMS))
		return;

	if (host_sid == STREAM_PRIME_ACK) {
		pep->stream_info.first_prime_det = 1;
		for (cur_stream = 1; cur_stream < pep->stream_info.num_streams;
		    cur_stream++) {
			ep_ring = pep->stream_info.stream_rings[cur_stream];
			ep_ring->stream_active = 1;
			ep_ring->stream_rejected = 0;
		}
	}

	if (host_sid == STREAM_REJECTED) {
		struct cdnsp_td *td, *td_temp;

		pep->stream_info.drbls_count--;
		ep_ring = pep->stream_info.stream_rings[dev_sid];
		ep_ring->stream_active = 0;
		ep_ring->stream_rejected = 1;

		list_for_each_entry_safe(td, td_temp, &ep_ring->td_list,
					 td_list) {
			td->drbl = 0;
		}
	}

	cdnsp_ring_doorbell_for_active_rings(pdev, pep);
}

 
static int cdnsp_handle_tx_event(struct cdnsp_device *pdev,
				 struct cdnsp_transfer_event *event)
{
	const struct usb_endpoint_descriptor *desc;
	bool handling_skipped_tds = false;
	struct cdnsp_segment *ep_seg;
	struct cdnsp_ring *ep_ring;
	int status = -EINPROGRESS;
	union cdnsp_trb *ep_trb;
	dma_addr_t ep_trb_dma;
	struct cdnsp_ep *pep;
	struct cdnsp_td *td;
	u32 trb_comp_code;
	int invalidate;
	int ep_index;

	invalidate = le32_to_cpu(event->flags) & TRB_EVENT_INVALIDATE;
	ep_index = TRB_TO_EP_ID(le32_to_cpu(event->flags)) - 1;
	trb_comp_code = GET_COMP_CODE(le32_to_cpu(event->transfer_len));
	ep_trb_dma = le64_to_cpu(event->buffer);

	pep = &pdev->eps[ep_index];
	ep_ring = cdnsp_dma_to_transfer_ring(pep, le64_to_cpu(event->buffer));

	 
	if (invalidate || !pdev->gadget.connected)
		goto cleanup;

	if (GET_EP_CTX_STATE(pep->out_ctx) == EP_STATE_DISABLED) {
		trace_cdnsp_ep_disabled(pep->out_ctx);
		goto err_out;
	}

	 
	if (!ep_ring) {
		switch (trb_comp_code) {
		case COMP_INVALID_STREAM_TYPE_ERROR:
		case COMP_INVALID_STREAM_ID_ERROR:
		case COMP_RING_UNDERRUN:
		case COMP_RING_OVERRUN:
			goto cleanup;
		default:
			dev_err(pdev->dev, "ERROR: %s event for unknown ring\n",
				pep->name);
			goto err_out;
		}
	}

	 
	switch (trb_comp_code) {
	case COMP_BABBLE_DETECTED_ERROR:
		status = -EOVERFLOW;
		break;
	case COMP_RING_UNDERRUN:
	case COMP_RING_OVERRUN:
		 
		goto cleanup;
	case COMP_MISSED_SERVICE_ERROR:
		 
		pep->skip = true;
		break;
	}

	do {
		 
		if (list_empty(&ep_ring->td_list)) {
			 
			if (!(trb_comp_code == COMP_STOPPED ||
			      trb_comp_code == COMP_STOPPED_LENGTH_INVALID ||
			      ep_ring->last_td_was_short))
				trace_cdnsp_trb_without_td(ep_ring,
					(struct cdnsp_generic_trb *)event);

			if (pep->skip) {
				pep->skip = false;
				trace_cdnsp_ep_list_empty_with_skip(pep, 0);
			}

			goto cleanup;
		}

		td = list_entry(ep_ring->td_list.next, struct cdnsp_td,
				td_list);

		 
		ep_seg = cdnsp_trb_in_td(pdev, ep_ring->deq_seg,
					 ep_ring->dequeue, td->last_trb,
					 ep_trb_dma);

		desc = td->preq->pep->endpoint.desc;

		if (ep_seg) {
			ep_trb = &ep_seg->trbs[(ep_trb_dma - ep_seg->dma)
					       / sizeof(*ep_trb)];

			trace_cdnsp_handle_transfer(ep_ring,
					(struct cdnsp_generic_trb *)ep_trb);

			if (pep->skip && usb_endpoint_xfer_isoc(desc) &&
			    td->last_trb != ep_trb)
				return -EAGAIN;
		}

		 
		if (!ep_seg && (trb_comp_code == COMP_STOPPED ||
				trb_comp_code == COMP_STOPPED_LENGTH_INVALID)) {
			pep->skip = false;
			goto cleanup;
		}

		if (!ep_seg) {
			if (!pep->skip || !usb_endpoint_xfer_isoc(desc)) {
				 
				dev_err(pdev->dev,
					"ERROR Transfer event TRB DMA ptr not "
					"part of current TD ep_index %d "
					"comp_code %u\n", ep_index,
					trb_comp_code);
				return -EINVAL;
			}

			cdnsp_skip_isoc_td(pdev, td, event, pep, status);
			goto cleanup;
		}

		if (trb_comp_code == COMP_SHORT_PACKET)
			ep_ring->last_td_was_short = true;
		else
			ep_ring->last_td_was_short = false;

		if (pep->skip) {
			pep->skip = false;
			cdnsp_skip_isoc_td(pdev, td, event, pep, status);
			goto cleanup;
		}

		if (cdnsp_trb_is_noop(ep_trb))
			goto cleanup;

		if (usb_endpoint_xfer_control(desc))
			cdnsp_process_ctrl_td(pdev, td, ep_trb, event, pep,
					      &status);
		else if (usb_endpoint_xfer_isoc(desc))
			cdnsp_process_isoc_td(pdev, td, ep_trb, event, pep,
					      status);
		else
			cdnsp_process_bulk_intr_td(pdev, td, ep_trb, event, pep,
						   &status);
cleanup:
		handling_skipped_tds = pep->skip;

		 
		if (!handling_skipped_tds)
			cdnsp_inc_deq(pdev, pdev->event_ring);

	 
	} while (handling_skipped_tds);
	return 0;

err_out:
	dev_err(pdev->dev, "@%016llx %08x %08x %08x %08x\n",
		(unsigned long long)
		cdnsp_trb_virt_to_dma(pdev->event_ring->deq_seg,
				      pdev->event_ring->dequeue),
		 lower_32_bits(le64_to_cpu(event->buffer)),
		 upper_32_bits(le64_to_cpu(event->buffer)),
		 le32_to_cpu(event->transfer_len),
		 le32_to_cpu(event->flags));
	return -EINVAL;
}

 
static bool cdnsp_handle_event(struct cdnsp_device *pdev)
{
	unsigned int comp_code;
	union cdnsp_trb *event;
	bool update_ptrs = true;
	u32 cycle_bit;
	int ret = 0;
	u32 flags;

	event = pdev->event_ring->dequeue;
	flags = le32_to_cpu(event->event_cmd.flags);
	cycle_bit = (flags & TRB_CYCLE);

	 
	if (cycle_bit != pdev->event_ring->cycle_state)
		return false;

	trace_cdnsp_handle_event(pdev->event_ring, &event->generic);

	 
	rmb();

	switch (flags & TRB_TYPE_BITMASK) {
	case TRB_TYPE(TRB_COMPLETION):
		 
		cdnsp_inc_deq(pdev, pdev->cmd_ring);
		break;
	case TRB_TYPE(TRB_PORT_STATUS):
		cdnsp_handle_port_status(pdev, event);
		update_ptrs = false;
		break;
	case TRB_TYPE(TRB_TRANSFER):
		ret = cdnsp_handle_tx_event(pdev, &event->trans_event);
		if (ret >= 0)
			update_ptrs = false;
		break;
	case TRB_TYPE(TRB_SETUP):
		pdev->ep0_stage = CDNSP_SETUP_STAGE;
		pdev->setup_id = TRB_SETUPID_TO_TYPE(flags);
		pdev->setup_speed = TRB_SETUP_SPEEDID(flags);
		pdev->setup = *((struct usb_ctrlrequest *)
				&event->trans_event.buffer);

		cdnsp_setup_analyze(pdev);
		break;
	case TRB_TYPE(TRB_ENDPOINT_NRDY):
		cdnsp_handle_tx_nrdy(pdev, &event->trans_event);
		break;
	case TRB_TYPE(TRB_HC_EVENT): {
		comp_code = GET_COMP_CODE(le32_to_cpu(event->generic.field[2]));

		switch (comp_code) {
		case COMP_EVENT_RING_FULL_ERROR:
			dev_err(pdev->dev, "Event Ring Full\n");
			break;
		default:
			dev_err(pdev->dev, "Controller error code 0x%02x\n",
				comp_code);
		}

		break;
	}
	case TRB_TYPE(TRB_MFINDEX_WRAP):
	case TRB_TYPE(TRB_DRB_OVERFLOW):
		break;
	default:
		dev_warn(pdev->dev, "ERROR unknown event type %ld\n",
			 TRB_FIELD_TO_TYPE(flags));
	}

	if (update_ptrs)
		 
		cdnsp_inc_deq(pdev, pdev->event_ring);

	 
	return true;
}

irqreturn_t cdnsp_thread_irq_handler(int irq, void *data)
{
	struct cdnsp_device *pdev = (struct cdnsp_device *)data;
	union cdnsp_trb *event_ring_deq;
	unsigned long flags;
	int counter = 0;

	local_bh_disable();
	spin_lock_irqsave(&pdev->lock, flags);

	if (pdev->cdnsp_state & (CDNSP_STATE_HALTED | CDNSP_STATE_DYING)) {
		 
		if (pdev->gadget_driver)
			cdnsp_died(pdev);

		spin_unlock_irqrestore(&pdev->lock, flags);
		local_bh_enable();
		return IRQ_HANDLED;
	}

	event_ring_deq = pdev->event_ring->dequeue;

	while (cdnsp_handle_event(pdev)) {
		if (++counter >= TRBS_PER_EV_DEQ_UPDATE) {
			cdnsp_update_erst_dequeue(pdev, event_ring_deq, 0);
			event_ring_deq = pdev->event_ring->dequeue;
			counter = 0;
		}
	}

	cdnsp_update_erst_dequeue(pdev, event_ring_deq, 1);

	spin_unlock_irqrestore(&pdev->lock, flags);
	local_bh_enable();

	return IRQ_HANDLED;
}

irqreturn_t cdnsp_irq_handler(int irq, void *priv)
{
	struct cdnsp_device *pdev = (struct cdnsp_device *)priv;
	u32 irq_pending;
	u32 status;

	status = readl(&pdev->op_regs->status);

	if (status == ~(u32)0) {
		cdnsp_died(pdev);
		return IRQ_HANDLED;
	}

	if (!(status & STS_EINT))
		return IRQ_NONE;

	writel(status | STS_EINT, &pdev->op_regs->status);
	irq_pending = readl(&pdev->ir_set->irq_pending);
	irq_pending |= IMAN_IP;
	writel(irq_pending, &pdev->ir_set->irq_pending);

	if (status & STS_FATAL) {
		cdnsp_died(pdev);
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

 
static void cdnsp_queue_trb(struct cdnsp_device *pdev, struct cdnsp_ring *ring,
			    bool more_trbs_coming, u32 field1, u32 field2,
			    u32 field3, u32 field4)
{
	struct cdnsp_generic_trb *trb;

	trb = &ring->enqueue->generic;

	trb->field[0] = cpu_to_le32(field1);
	trb->field[1] = cpu_to_le32(field2);
	trb->field[2] = cpu_to_le32(field3);
	trb->field[3] = cpu_to_le32(field4);

	trace_cdnsp_queue_trb(ring, trb);
	cdnsp_inc_enq(pdev, ring, more_trbs_coming);
}

 
static int cdnsp_prepare_ring(struct cdnsp_device *pdev,
			      struct cdnsp_ring *ep_ring,
			      u32 ep_state, unsigned
			      int num_trbs,
			      gfp_t mem_flags)
{
	unsigned int num_trbs_needed;

	 
	switch (ep_state) {
	case EP_STATE_STOPPED:
	case EP_STATE_RUNNING:
	case EP_STATE_HALTED:
		break;
	default:
		dev_err(pdev->dev, "ERROR: incorrect endpoint state\n");
		return -EINVAL;
	}

	while (1) {
		if (cdnsp_room_on_ring(pdev, ep_ring, num_trbs))
			break;

		trace_cdnsp_no_room_on_ring("try ring expansion");

		num_trbs_needed = num_trbs - ep_ring->num_trbs_free;
		if (cdnsp_ring_expansion(pdev, ep_ring, num_trbs_needed,
					 mem_flags)) {
			dev_err(pdev->dev, "Ring expansion failed\n");
			return -ENOMEM;
		}
	}

	while (cdnsp_trb_is_link(ep_ring->enqueue)) {
		ep_ring->enqueue->link.control |= cpu_to_le32(TRB_CHAIN);
		 
		wmb();
		ep_ring->enqueue->link.control ^= cpu_to_le32(TRB_CYCLE);

		 
		if (cdnsp_link_trb_toggles_cycle(ep_ring->enqueue))
			ep_ring->cycle_state ^= 1;
		ep_ring->enq_seg = ep_ring->enq_seg->next;
		ep_ring->enqueue = ep_ring->enq_seg->trbs;
	}
	return 0;
}

static int cdnsp_prepare_transfer(struct cdnsp_device *pdev,
				  struct cdnsp_request *preq,
				  unsigned int num_trbs)
{
	struct cdnsp_ring *ep_ring;
	int ret;

	ep_ring = cdnsp_get_transfer_ring(pdev, preq->pep,
					  preq->request.stream_id);
	if (!ep_ring)
		return -EINVAL;

	ret = cdnsp_prepare_ring(pdev, ep_ring,
				 GET_EP_CTX_STATE(preq->pep->out_ctx),
				 num_trbs, GFP_ATOMIC);
	if (ret)
		return ret;

	INIT_LIST_HEAD(&preq->td.td_list);
	preq->td.preq = preq;

	 
	list_add_tail(&preq->td.td_list, &ep_ring->td_list);
	ep_ring->num_tds++;
	preq->pep->stream_info.td_count++;

	preq->td.start_seg = ep_ring->enq_seg;
	preq->td.first_trb = ep_ring->enqueue;

	return 0;
}

static unsigned int cdnsp_count_trbs(u64 addr, u64 len)
{
	unsigned int num_trbs;

	num_trbs = DIV_ROUND_UP(len + (addr & (TRB_MAX_BUFF_SIZE - 1)),
				TRB_MAX_BUFF_SIZE);
	if (num_trbs == 0)
		num_trbs++;

	return num_trbs;
}

static unsigned int count_trbs_needed(struct cdnsp_request *preq)
{
	return cdnsp_count_trbs(preq->request.dma, preq->request.length);
}

static unsigned int count_sg_trbs_needed(struct cdnsp_request *preq)
{
	unsigned int i, len, full_len, num_trbs = 0;
	struct scatterlist *sg;

	full_len = preq->request.length;

	for_each_sg(preq->request.sg, sg, preq->request.num_sgs, i) {
		len = sg_dma_len(sg);
		num_trbs += cdnsp_count_trbs(sg_dma_address(sg), len);
		len = min(len, full_len);
		full_len -= len;
		if (full_len == 0)
			break;
	}

	return num_trbs;
}

static void cdnsp_check_trb_math(struct cdnsp_request *preq, int running_total)
{
	if (running_total != preq->request.length)
		dev_err(preq->pep->pdev->dev,
			"%s - Miscalculated tx length, "
			"queued %#x, asked for %#x (%d)\n",
			preq->pep->name, running_total,
			preq->request.length, preq->request.actual);
}

 
static u32 cdnsp_td_remainder(struct cdnsp_device *pdev,
			      int transferred,
			      int trb_buff_len,
			      unsigned int td_total_len,
			      struct cdnsp_request *preq,
			      bool more_trbs_coming,
			      bool zlp)
{
	u32 maxp, total_packet_count;

	 
	if (zlp)
		return 1;

	 
	if (!more_trbs_coming || (transferred == 0 && trb_buff_len == 0) ||
	    trb_buff_len == td_total_len)
		return 0;

	maxp = usb_endpoint_maxp(preq->pep->endpoint.desc);
	total_packet_count = DIV_ROUND_UP(td_total_len, maxp);

	 
	return (total_packet_count - ((transferred + trb_buff_len) / maxp));
}

static int cdnsp_align_td(struct cdnsp_device *pdev,
			  struct cdnsp_request *preq, u32 enqd_len,
			  u32 *trb_buff_len, struct cdnsp_segment *seg)
{
	struct device *dev = pdev->dev;
	unsigned int unalign;
	unsigned int max_pkt;
	u32 new_buff_len;

	max_pkt = usb_endpoint_maxp(preq->pep->endpoint.desc);
	unalign = (enqd_len + *trb_buff_len) % max_pkt;

	 
	if (unalign == 0)
		return 0;

	 
	if (*trb_buff_len > unalign) {
		*trb_buff_len -= unalign;
		trace_cdnsp_bounce_align_td_split(preq, *trb_buff_len,
						  enqd_len, 0, unalign);
		return 0;
	}

	 
	new_buff_len = max_pkt - (enqd_len % max_pkt);

	if (new_buff_len > (preq->request.length - enqd_len))
		new_buff_len = (preq->request.length - enqd_len);

	 
	if (preq->direction) {
		sg_pcopy_to_buffer(preq->request.sg,
				   preq->request.num_mapped_sgs,
				   seg->bounce_buf, new_buff_len, enqd_len);
		seg->bounce_dma = dma_map_single(dev, seg->bounce_buf,
						 max_pkt, DMA_TO_DEVICE);
	} else {
		seg->bounce_dma = dma_map_single(dev, seg->bounce_buf,
						 max_pkt, DMA_FROM_DEVICE);
	}

	if (dma_mapping_error(dev, seg->bounce_dma)) {
		 
		dev_warn(pdev->dev,
			 "Failed mapping bounce buffer, not aligning\n");
		return 0;
	}

	*trb_buff_len = new_buff_len;
	seg->bounce_len = new_buff_len;
	seg->bounce_offs = enqd_len;

	trace_cdnsp_bounce_map(preq, new_buff_len, enqd_len, seg->bounce_dma,
			       unalign);

	 
	return 1;
}

int cdnsp_queue_bulk_tx(struct cdnsp_device *pdev, struct cdnsp_request *preq)
{
	unsigned int enqd_len, block_len, trb_buff_len, full_len;
	unsigned int start_cycle, num_sgs = 0;
	struct cdnsp_generic_trb *start_trb;
	u32 field, length_field, remainder;
	struct scatterlist *sg = NULL;
	bool more_trbs_coming = true;
	bool need_zero_pkt = false;
	bool zero_len_trb = false;
	struct cdnsp_ring *ring;
	bool first_trb = true;
	unsigned int num_trbs;
	struct cdnsp_ep *pep;
	u64 addr, send_addr;
	int sent_len, ret;

	ring = cdnsp_request_to_transfer_ring(pdev, preq);
	if (!ring)
		return -EINVAL;

	full_len = preq->request.length;

	if (preq->request.num_sgs) {
		num_sgs = preq->request.num_sgs;
		sg = preq->request.sg;
		addr = (u64)sg_dma_address(sg);
		block_len = sg_dma_len(sg);
		num_trbs = count_sg_trbs_needed(preq);
	} else {
		num_trbs = count_trbs_needed(preq);
		addr = (u64)preq->request.dma;
		block_len = full_len;
	}

	pep = preq->pep;

	 
	if (preq->request.zero && preq->request.length &&
	    IS_ALIGNED(full_len, usb_endpoint_maxp(pep->endpoint.desc))) {
		need_zero_pkt = true;
		num_trbs++;
	}

	ret = cdnsp_prepare_transfer(pdev, preq, num_trbs);
	if (ret)
		return ret;

	 
	start_trb = &ring->enqueue->generic;
	start_cycle = ring->cycle_state;
	send_addr = addr;

	 
	for (enqd_len = 0; zero_len_trb || first_trb || enqd_len < full_len;
	     enqd_len += trb_buff_len) {
		field = TRB_TYPE(TRB_NORMAL);

		 
		trb_buff_len = TRB_BUFF_LEN_UP_TO_BOUNDARY(addr);
		trb_buff_len = min(trb_buff_len, block_len);
		if (enqd_len + trb_buff_len > full_len)
			trb_buff_len = full_len - enqd_len;

		 
		if (first_trb) {
			first_trb = false;
			if (start_cycle == 0)
				field |= TRB_CYCLE;
		} else {
			field |= ring->cycle_state;
		}

		 
		if (enqd_len + trb_buff_len < full_len || need_zero_pkt) {
			field |= TRB_CHAIN;
			if (cdnsp_trb_is_link(ring->enqueue + 1)) {
				if (cdnsp_align_td(pdev, preq, enqd_len,
						   &trb_buff_len,
						   ring->enq_seg)) {
					send_addr = ring->enq_seg->bounce_dma;
					 
					preq->td.bounce_seg = ring->enq_seg;
				}
			}
		}

		if (enqd_len + trb_buff_len >= full_len) {
			if (need_zero_pkt && !zero_len_trb) {
				zero_len_trb = true;
			} else {
				zero_len_trb = false;
				field &= ~TRB_CHAIN;
				field |= TRB_IOC;
				more_trbs_coming = false;
				need_zero_pkt = false;
				preq->td.last_trb = ring->enqueue;
			}
		}

		 
		if (!preq->direction)
			field |= TRB_ISP;

		 
		remainder = cdnsp_td_remainder(pdev, enqd_len, trb_buff_len,
					       full_len, preq,
					       more_trbs_coming,
					       zero_len_trb);

		length_field = TRB_LEN(trb_buff_len) | TRB_TD_SIZE(remainder) |
			TRB_INTR_TARGET(0);

		cdnsp_queue_trb(pdev, ring, more_trbs_coming,
				lower_32_bits(send_addr),
				upper_32_bits(send_addr),
				length_field,
				field);

		addr += trb_buff_len;
		sent_len = trb_buff_len;
		while (sg && sent_len >= block_len) {
			 
			--num_sgs;
			sent_len -= block_len;
			if (num_sgs != 0) {
				sg = sg_next(sg);
				block_len = sg_dma_len(sg);
				addr = (u64)sg_dma_address(sg);
				addr += sent_len;
			}
		}
		block_len -= sent_len;
		send_addr = addr;
	}

	cdnsp_check_trb_math(preq, enqd_len);
	ret = cdnsp_giveback_first_trb(pdev, pep, preq->request.stream_id,
				       start_cycle, start_trb);

	if (ret)
		preq->td.drbl = 1;

	return 0;
}

int cdnsp_queue_ctrl_tx(struct cdnsp_device *pdev, struct cdnsp_request *preq)
{
	u32 field, length_field, zlp = 0;
	struct cdnsp_ep *pep = preq->pep;
	struct cdnsp_ring *ep_ring;
	int num_trbs;
	u32 maxp;
	int ret;

	ep_ring = cdnsp_request_to_transfer_ring(pdev, preq);
	if (!ep_ring)
		return -EINVAL;

	 
	num_trbs = (pdev->three_stage_setup) ? 2 : 1;

	maxp = usb_endpoint_maxp(pep->endpoint.desc);

	if (preq->request.zero && preq->request.length &&
	    (preq->request.length % maxp == 0)) {
		num_trbs++;
		zlp = 1;
	}

	ret = cdnsp_prepare_transfer(pdev, preq, num_trbs);
	if (ret)
		return ret;

	 
	if (preq->request.length > 0) {
		field = TRB_TYPE(TRB_DATA);

		if (zlp)
			field |= TRB_CHAIN;
		else
			field |= TRB_IOC | (pdev->ep0_expect_in ? 0 : TRB_ISP);

		if (pdev->ep0_expect_in)
			field |= TRB_DIR_IN;

		length_field = TRB_LEN(preq->request.length) |
			       TRB_TD_SIZE(zlp) | TRB_INTR_TARGET(0);

		cdnsp_queue_trb(pdev, ep_ring, true,
				lower_32_bits(preq->request.dma),
				upper_32_bits(preq->request.dma), length_field,
				field | ep_ring->cycle_state |
				TRB_SETUPID(pdev->setup_id) |
				pdev->setup_speed);

		if (zlp) {
			field = TRB_TYPE(TRB_NORMAL) | TRB_IOC;

			if (!pdev->ep0_expect_in)
				field = TRB_ISP;

			cdnsp_queue_trb(pdev, ep_ring, true,
					lower_32_bits(preq->request.dma),
					upper_32_bits(preq->request.dma), 0,
					field | ep_ring->cycle_state |
					TRB_SETUPID(pdev->setup_id) |
					pdev->setup_speed);
		}

		pdev->ep0_stage = CDNSP_DATA_STAGE;
	}

	 
	preq->td.last_trb = ep_ring->enqueue;

	 
	if (preq->request.length == 0)
		field = ep_ring->cycle_state;
	else
		field = (ep_ring->cycle_state ^ 1);

	if (preq->request.length > 0 && pdev->ep0_expect_in)
		field |= TRB_DIR_IN;

	if (pep->ep_state & EP0_HALTED_STATUS) {
		pep->ep_state &= ~EP0_HALTED_STATUS;
		field |= TRB_SETUPSTAT(TRB_SETUPSTAT_STALL);
	} else {
		field |= TRB_SETUPSTAT(TRB_SETUPSTAT_ACK);
	}

	cdnsp_queue_trb(pdev, ep_ring, false, 0, 0, TRB_INTR_TARGET(0),
			field | TRB_IOC | TRB_SETUPID(pdev->setup_id) |
			TRB_TYPE(TRB_STATUS) | pdev->setup_speed);

	cdnsp_ring_ep_doorbell(pdev, pep, preq->request.stream_id);

	return 0;
}

int cdnsp_cmd_stop_ep(struct cdnsp_device *pdev, struct cdnsp_ep *pep)
{
	u32 ep_state = GET_EP_CTX_STATE(pep->out_ctx);
	int ret = 0;

	if (ep_state == EP_STATE_STOPPED || ep_state == EP_STATE_DISABLED ||
	    ep_state == EP_STATE_HALTED) {
		trace_cdnsp_ep_stopped_or_disabled(pep->out_ctx);
		goto ep_stopped;
	}

	cdnsp_queue_stop_endpoint(pdev, pep->idx);
	cdnsp_ring_cmd_db(pdev);
	ret = cdnsp_wait_for_cmd_compl(pdev);

	trace_cdnsp_handle_cmd_stop_ep(pep->out_ctx);

ep_stopped:
	pep->ep_state |= EP_STOPPED;
	return ret;
}

int cdnsp_cmd_flush_ep(struct cdnsp_device *pdev, struct cdnsp_ep *pep)
{
	int ret;

	cdnsp_queue_flush_endpoint(pdev, pep->idx);
	cdnsp_ring_cmd_db(pdev);
	ret = cdnsp_wait_for_cmd_compl(pdev);

	trace_cdnsp_handle_cmd_flush_ep(pep->out_ctx);

	return ret;
}

 
static unsigned int cdnsp_get_burst_count(struct cdnsp_device *pdev,
					  struct cdnsp_request *preq,
					  unsigned int total_packet_count)
{
	unsigned int max_burst;

	if (pdev->gadget.speed < USB_SPEED_SUPER)
		return 0;

	max_burst = preq->pep->endpoint.comp_desc->bMaxBurst;
	return DIV_ROUND_UP(total_packet_count, max_burst + 1) - 1;
}

 
static unsigned int
	cdnsp_get_last_burst_packet_count(struct cdnsp_device *pdev,
					  struct cdnsp_request *preq,
					  unsigned int total_packet_count)
{
	unsigned int max_burst;
	unsigned int residue;

	if (pdev->gadget.speed >= USB_SPEED_SUPER) {
		 
		max_burst = preq->pep->endpoint.comp_desc->bMaxBurst;
		residue = total_packet_count % (max_burst + 1);

		 
		if (residue == 0)
			return max_burst;

		return residue - 1;
	}
	if (total_packet_count == 0)
		return 0;

	return total_packet_count - 1;
}

 
int cdnsp_queue_isoc_tx(struct cdnsp_device *pdev,
			struct cdnsp_request *preq)
{
	unsigned int trb_buff_len, td_len, td_remain_len, block_len;
	unsigned int burst_count, last_burst_pkt;
	unsigned int total_pkt_count, max_pkt;
	struct cdnsp_generic_trb *start_trb;
	struct scatterlist *sg = NULL;
	bool more_trbs_coming = true;
	struct cdnsp_ring *ep_ring;
	unsigned int num_sgs = 0;
	int running_total = 0;
	u32 field, length_field;
	u64 addr, send_addr;
	int start_cycle;
	int trbs_per_td;
	int i, sent_len, ret;

	ep_ring = preq->pep->ring;

	td_len = preq->request.length;

	if (preq->request.num_sgs) {
		num_sgs = preq->request.num_sgs;
		sg = preq->request.sg;
		addr = (u64)sg_dma_address(sg);
		block_len = sg_dma_len(sg);
		trbs_per_td = count_sg_trbs_needed(preq);
	} else {
		addr = (u64)preq->request.dma;
		block_len = td_len;
		trbs_per_td = count_trbs_needed(preq);
	}

	ret = cdnsp_prepare_transfer(pdev, preq, trbs_per_td);
	if (ret)
		return ret;

	start_trb = &ep_ring->enqueue->generic;
	start_cycle = ep_ring->cycle_state;
	td_remain_len = td_len;
	send_addr = addr;

	max_pkt = usb_endpoint_maxp(preq->pep->endpoint.desc);
	total_pkt_count = DIV_ROUND_UP(td_len, max_pkt);

	 
	if (total_pkt_count == 0)
		total_pkt_count++;

	burst_count = cdnsp_get_burst_count(pdev, preq, total_pkt_count);
	last_burst_pkt = cdnsp_get_last_burst_packet_count(pdev, preq,
							   total_pkt_count);

	 
	field = TRB_TYPE(TRB_ISOC) | TRB_TLBPC(last_burst_pkt) |
		TRB_SIA | TRB_TBC(burst_count);

	if (!start_cycle)
		field |= TRB_CYCLE;

	 
	for (i = 0; i < trbs_per_td; i++) {
		u32 remainder;

		 
		trb_buff_len = TRB_BUFF_LEN_UP_TO_BOUNDARY(addr);
		trb_buff_len = min(trb_buff_len, block_len);
		if (trb_buff_len > td_remain_len)
			trb_buff_len = td_remain_len;

		 
		remainder = cdnsp_td_remainder(pdev, running_total,
					       trb_buff_len, td_len, preq,
					       more_trbs_coming, 0);

		length_field = TRB_LEN(trb_buff_len) | TRB_TD_SIZE(remainder) |
			TRB_INTR_TARGET(0);

		 
		if (i) {
			field = TRB_TYPE(TRB_NORMAL) | ep_ring->cycle_state;
			length_field |= TRB_TD_SIZE(remainder);
		} else {
			length_field |= TRB_TD_SIZE_TBC(burst_count);
		}

		 
		if (usb_endpoint_dir_out(preq->pep->endpoint.desc))
			field |= TRB_ISP;

		 
		if (i < trbs_per_td - 1) {
			more_trbs_coming = true;
			field |= TRB_CHAIN;
		} else {
			more_trbs_coming = false;
			preq->td.last_trb = ep_ring->enqueue;
			field |= TRB_IOC;
		}

		cdnsp_queue_trb(pdev, ep_ring, more_trbs_coming,
				lower_32_bits(send_addr), upper_32_bits(send_addr),
				length_field, field);

		running_total += trb_buff_len;
		addr += trb_buff_len;
		td_remain_len -= trb_buff_len;

		sent_len = trb_buff_len;
		while (sg && sent_len >= block_len) {
			 
			--num_sgs;
			sent_len -= block_len;
			if (num_sgs != 0) {
				sg = sg_next(sg);
				block_len = sg_dma_len(sg);
				addr = (u64)sg_dma_address(sg);
				addr += sent_len;
			}
		}
		block_len -= sent_len;
		send_addr = addr;
	}

	 
	if (running_total != td_len) {
		dev_err(pdev->dev, "ISOC TD length unmatch\n");
		ret = -EINVAL;
		goto cleanup;
	}

	cdnsp_giveback_first_trb(pdev, preq->pep, preq->request.stream_id,
				 start_cycle, start_trb);

	return 0;

cleanup:
	 
	list_del_init(&preq->td.td_list);
	ep_ring->num_tds--;

	 
	preq->td.last_trb = ep_ring->enqueue;
	 
	cdnsp_td_to_noop(pdev, ep_ring, &preq->td, true);

	 
	ep_ring->enqueue = preq->td.first_trb;
	ep_ring->enq_seg = preq->td.start_seg;
	ep_ring->cycle_state = start_cycle;
	return ret;
}

 
 
static void cdnsp_queue_command(struct cdnsp_device *pdev,
				u32 field1,
				u32 field2,
				u32 field3,
				u32 field4)
{
	cdnsp_prepare_ring(pdev, pdev->cmd_ring, EP_STATE_RUNNING, 1,
			   GFP_ATOMIC);

	pdev->cmd.command_trb = pdev->cmd_ring->enqueue;

	cdnsp_queue_trb(pdev, pdev->cmd_ring, false, field1, field2,
			field3, field4 | pdev->cmd_ring->cycle_state);
}

 
void cdnsp_queue_slot_control(struct cdnsp_device *pdev, u32 trb_type)
{
	cdnsp_queue_command(pdev, 0, 0, 0, TRB_TYPE(trb_type) |
			    SLOT_ID_FOR_TRB(pdev->slot_id));
}

 
void cdnsp_queue_address_device(struct cdnsp_device *pdev,
				dma_addr_t in_ctx_ptr,
				enum cdnsp_setup_dev setup)
{
	cdnsp_queue_command(pdev, lower_32_bits(in_ctx_ptr),
			    upper_32_bits(in_ctx_ptr), 0,
			    TRB_TYPE(TRB_ADDR_DEV) |
			    SLOT_ID_FOR_TRB(pdev->slot_id) |
			    (setup == SETUP_CONTEXT_ONLY ? TRB_BSR : 0));
}

 
void cdnsp_queue_reset_device(struct cdnsp_device *pdev)
{
	cdnsp_queue_command(pdev, 0, 0, 0, TRB_TYPE(TRB_RESET_DEV) |
			    SLOT_ID_FOR_TRB(pdev->slot_id));
}

 
void cdnsp_queue_configure_endpoint(struct cdnsp_device *pdev,
				    dma_addr_t in_ctx_ptr)
{
	cdnsp_queue_command(pdev, lower_32_bits(in_ctx_ptr),
			    upper_32_bits(in_ctx_ptr), 0,
			    TRB_TYPE(TRB_CONFIG_EP) |
			    SLOT_ID_FOR_TRB(pdev->slot_id));
}

 
void cdnsp_queue_stop_endpoint(struct cdnsp_device *pdev, unsigned int ep_index)
{
	cdnsp_queue_command(pdev, 0, 0, 0, SLOT_ID_FOR_TRB(pdev->slot_id) |
			    EP_ID_FOR_TRB(ep_index) | TRB_TYPE(TRB_STOP_RING));
}

 
void cdnsp_queue_new_dequeue_state(struct cdnsp_device *pdev,
				   struct cdnsp_ep *pep,
				   struct cdnsp_dequeue_state *deq_state)
{
	u32 trb_stream_id = STREAM_ID_FOR_TRB(deq_state->stream_id);
	u32 trb_slot_id = SLOT_ID_FOR_TRB(pdev->slot_id);
	u32 type = TRB_TYPE(TRB_SET_DEQ);
	u32 trb_sct = 0;
	dma_addr_t addr;

	addr = cdnsp_trb_virt_to_dma(deq_state->new_deq_seg,
				     deq_state->new_deq_ptr);

	if (deq_state->stream_id)
		trb_sct = SCT_FOR_TRB(SCT_PRI_TR);

	cdnsp_queue_command(pdev, lower_32_bits(addr) | trb_sct |
			    deq_state->new_cycle_state, upper_32_bits(addr),
			    trb_stream_id, trb_slot_id |
			    EP_ID_FOR_TRB(pep->idx) | type);
}

void cdnsp_queue_reset_ep(struct cdnsp_device *pdev, unsigned int ep_index)
{
	return cdnsp_queue_command(pdev, 0, 0, 0,
				   SLOT_ID_FOR_TRB(pdev->slot_id) |
				   EP_ID_FOR_TRB(ep_index) |
				   TRB_TYPE(TRB_RESET_EP));
}

 
void cdnsp_queue_halt_endpoint(struct cdnsp_device *pdev, unsigned int ep_index)
{
	cdnsp_queue_command(pdev, 0, 0, 0, TRB_TYPE(TRB_HALT_ENDPOINT) |
			    SLOT_ID_FOR_TRB(pdev->slot_id) |
			    EP_ID_FOR_TRB(ep_index));
}

 
void  cdnsp_queue_flush_endpoint(struct cdnsp_device *pdev,
				 unsigned int ep_index)
{
	cdnsp_queue_command(pdev, 0, 0, 0, TRB_TYPE(TRB_FLUSH_ENDPOINT) |
			    SLOT_ID_FOR_TRB(pdev->slot_id) |
			    EP_ID_FOR_TRB(ep_index));
}

void cdnsp_force_header_wakeup(struct cdnsp_device *pdev, int intf_num)
{
	u32 lo, mid;

	lo = TRB_FH_TO_PACKET_TYPE(TRB_FH_TR_PACKET) |
	     TRB_FH_TO_DEVICE_ADDRESS(pdev->device_address);
	mid = TRB_FH_TR_PACKET_DEV_NOT |
	      TRB_FH_TO_NOT_TYPE(TRB_FH_TR_PACKET_FUNCTION_WAKE) |
	      TRB_FH_TO_INTERFACE(intf_num);

	cdnsp_queue_command(pdev, lo, mid, 0,
			    TRB_TYPE(TRB_FORCE_HEADER) | SET_PORT_ID(2));
}
