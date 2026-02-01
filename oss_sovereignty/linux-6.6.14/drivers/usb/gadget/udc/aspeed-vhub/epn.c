
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/prefetch.h>
#include <linux/clk.h>
#include <linux/usb/gadget.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/dma-mapping.h>

#include "vhub.h"

#define EXTRA_CHECKS

#ifdef EXTRA_CHECKS
#define CHECK(ep, expr, fmt...)					\
	do {							\
		if (!(expr)) EPDBG(ep, "CHECK:" fmt);		\
	} while(0)
#else
#define CHECK(ep, expr, fmt...)	do { } while(0)
#endif

static void ast_vhub_epn_kick(struct ast_vhub_ep *ep, struct ast_vhub_req *req)
{
	unsigned int act = req->req.actual;
	unsigned int len = req->req.length;
	unsigned int chunk;

	 
	WARN_ON(req->active);

	 
	chunk = len - act;
	if (chunk > ep->ep.maxpacket)
		chunk = ep->ep.maxpacket;
	else if ((chunk < ep->ep.maxpacket) || !req->req.zero)
		req->last_desc = 1;

	EPVDBG(ep, "kick req %p act=%d/%d chunk=%d last=%d\n",
	       req, act, len, chunk, req->last_desc);

	 
	if (!req->req.dma) {

		 
		if (ep->epn.is_in) {
			memcpy(ep->buf, req->req.buf + act, chunk);
			vhub_dma_workaround(ep->buf);
		}
		writel(ep->buf_dma, ep->epn.regs + AST_VHUB_EP_DESC_BASE);
	} else {
		if (ep->epn.is_in)
			vhub_dma_workaround(req->req.buf);
		writel(req->req.dma + act, ep->epn.regs + AST_VHUB_EP_DESC_BASE);
	}

	 
	req->active = true;
	writel(VHUB_EP_DMA_SET_TX_SIZE(chunk),
	       ep->epn.regs + AST_VHUB_EP_DESC_STATUS);
	writel(VHUB_EP_DMA_SET_TX_SIZE(chunk) | VHUB_EP_DMA_SINGLE_KICK,
	       ep->epn.regs + AST_VHUB_EP_DESC_STATUS);
}

static void ast_vhub_epn_handle_ack(struct ast_vhub_ep *ep)
{
	struct ast_vhub_req *req;
	unsigned int len;
	int status = 0;
	u32 stat;

	 
	stat = readl(ep->epn.regs + AST_VHUB_EP_DESC_STATUS);

	 
	req = list_first_entry_or_null(&ep->queue, struct ast_vhub_req, queue);

	EPVDBG(ep, "ACK status=%08x is_in=%d, req=%p (active=%d)\n",
	       stat, ep->epn.is_in, req, req ? req->active : 0);

	 
	if (!req)
		return;

	 
	if (!req->active)
		goto next_chunk;

	 
	if (VHUB_EP_DMA_RPTR(stat) != 0) {
		EPDBG(ep, "DMA read pointer not 0 !\n");
		return;
	}

	 
	req->active = false;

	 
	len = VHUB_EP_DMA_TX_SIZE(stat);

	 
	if (!req->req.dma && !ep->epn.is_in && len) {
		if (req->req.actual + len > req->req.length) {
			req->last_desc = 1;
			status = -EOVERFLOW;
			goto done;
		} else {
			memcpy(req->req.buf + req->req.actual, ep->buf, len);
		}
	}
	 
	req->req.actual += len;

	 
	if (len < ep->ep.maxpacket)
		req->last_desc = 1;

done:
	 
	if (req->last_desc >= 0) {
		ast_vhub_done(ep, req, status);
		req = list_first_entry_or_null(&ep->queue, struct ast_vhub_req,
					       queue);

		 
		if (!req || req->active)
			return;
	}

 next_chunk:
	ast_vhub_epn_kick(ep, req);
}

static inline unsigned int ast_vhub_count_free_descs(struct ast_vhub_ep *ep)
{
	 
	return (ep->epn.d_last + AST_VHUB_DESCS_COUNT - ep->epn.d_next - 1) &
		(AST_VHUB_DESCS_COUNT - 1);
}

static void ast_vhub_epn_kick_desc(struct ast_vhub_ep *ep,
				   struct ast_vhub_req *req)
{
	struct ast_vhub_desc *desc = NULL;
	unsigned int act = req->act_count;
	unsigned int len = req->req.length;
	unsigned int chunk;

	 
	req->active = true;

	 
	if (req->last_desc >= 0)
		return;

	EPVDBG(ep, "kick act=%d/%d chunk_max=%d free_descs=%d\n",
	       act, len, ep->epn.chunk_max, ast_vhub_count_free_descs(ep));

	 
	while (ast_vhub_count_free_descs(ep) && req->last_desc < 0) {
		unsigned int d_num;

		 
		d_num = ep->epn.d_next;
		desc = &ep->epn.descs[d_num];
		ep->epn.d_next = (d_num + 1) & (AST_VHUB_DESCS_COUNT - 1);

		 
		chunk = len - act;
		if (chunk <= ep->epn.chunk_max) {
			 
			if (!chunk || !req->req.zero || (chunk % ep->ep.maxpacket) != 0)
				req->last_desc = d_num;
		} else {
			chunk = ep->epn.chunk_max;
		}

		EPVDBG(ep, " chunk: act=%d/%d chunk=%d last=%d desc=%d free=%d\n",
		       act, len, chunk, req->last_desc, d_num,
		       ast_vhub_count_free_descs(ep));

		 
		desc->w0 = cpu_to_le32(req->req.dma + act);

		 

		 
		desc->w1 = cpu_to_le32(VHUB_DSC1_IN_SET_LEN(chunk));
		if (req->last_desc >= 0 || !ast_vhub_count_free_descs(ep))
			desc->w1 |= cpu_to_le32(VHUB_DSC1_IN_INTERRUPT);

		 
		req->act_count = act = act + chunk;
	}

	if (likely(desc))
		vhub_dma_workaround(desc);

	 
	writel(VHUB_EP_DMA_SET_CPU_WPTR(ep->epn.d_next),
	       ep->epn.regs + AST_VHUB_EP_DESC_STATUS);

	EPVDBG(ep, "HW kicked, d_next=%d dstat=%08x\n",
	       ep->epn.d_next, readl(ep->epn.regs + AST_VHUB_EP_DESC_STATUS));
}

static void ast_vhub_epn_handle_ack_desc(struct ast_vhub_ep *ep)
{
	struct ast_vhub_req *req;
	unsigned int len, d_last;
	u32 stat, stat1;

	 
	do {
		stat = readl(ep->epn.regs + AST_VHUB_EP_DESC_STATUS);
		stat1 = readl(ep->epn.regs + AST_VHUB_EP_DESC_STATUS);
	} while(stat != stat1);

	 
	d_last = VHUB_EP_DMA_RPTR(stat);

	 
	req = list_first_entry_or_null(&ep->queue, struct ast_vhub_req, queue);

	EPVDBG(ep, "ACK status=%08x is_in=%d ep->d_last=%d..%d\n",
	       stat, ep->epn.is_in, ep->epn.d_last, d_last);

	 
	while (ep->epn.d_last != d_last) {
		struct ast_vhub_desc *desc;
		unsigned int d_num;
		bool is_last_desc;

		 
		d_num = ep->epn.d_last;
		desc = &ep->epn.descs[d_num];
		ep->epn.d_last = (d_num + 1) & (AST_VHUB_DESCS_COUNT - 1);

		 
		len = VHUB_DSC1_IN_LEN(le32_to_cpu(desc->w1));

		EPVDBG(ep, " desc %d len=%d req=%p (act=%d)\n",
		       d_num, len, req, req ? req->active : 0);

		 
		if (!req || !req->active)
			continue;

		 
		req->req.actual += len;

		 
		is_last_desc = req->last_desc == d_num;
		CHECK(ep, is_last_desc == (len < ep->ep.maxpacket ||
					   (req->req.actual >= req->req.length &&
					    !req->req.zero)),
		      "Last packet discrepancy: last_desc=%d len=%d r.act=%d "
		      "r.len=%d r.zero=%d mp=%d\n",
		      is_last_desc, len, req->req.actual, req->req.length,
		      req->req.zero, ep->ep.maxpacket);

		if (is_last_desc) {
			 
			CHECK(ep, d_last == ep->epn.d_last,
			      "DMA read ptr mismatch %d vs %d\n",
			      d_last, ep->epn.d_last);

			 
			ast_vhub_done(ep, req, 0);
			req = list_first_entry_or_null(&ep->queue,
						       struct ast_vhub_req,
						       queue);
			break;
		}
	}

	 
	if (req)
		ast_vhub_epn_kick_desc(ep, req);
}

void ast_vhub_epn_ack_irq(struct ast_vhub_ep *ep)
{
	if (ep->epn.desc_mode)
		ast_vhub_epn_handle_ack_desc(ep);
	else
		ast_vhub_epn_handle_ack(ep);
}

static int ast_vhub_epn_queue(struct usb_ep* u_ep, struct usb_request *u_req,
			      gfp_t gfp_flags)
{
	struct ast_vhub_req *req = to_ast_req(u_req);
	struct ast_vhub_ep *ep = to_ast_ep(u_ep);
	struct ast_vhub *vhub = ep->vhub;
	unsigned long flags;
	bool empty;
	int rc;

	 
	if (!u_req || !u_req->complete || !u_req->buf) {
		dev_warn(&vhub->pdev->dev, "Bogus EPn request ! u_req=%p\n", u_req);
		if (u_req) {
			dev_warn(&vhub->pdev->dev, "complete=%p internal=%d\n",
				 u_req->complete, req->internal);
		}
		return -EINVAL;
	}

	 
	if (!ep->epn.enabled || !u_ep->desc || !ep->dev || !ep->d_idx ||
	    !ep->dev->enabled) {
		EPDBG(ep, "Enqueuing request on wrong or disabled EP\n");
		return -ESHUTDOWN;
	}

	 
	if (ep->epn.desc_mode ||
	    ((((unsigned long)u_req->buf & 7) == 0) &&
	     (ep->epn.is_in || !(u_req->length & (u_ep->maxpacket - 1))))) {
		rc = usb_gadget_map_request_by_dev(&vhub->pdev->dev, u_req,
					    ep->epn.is_in);
		if (rc) {
			dev_warn(&vhub->pdev->dev,
				 "Request mapping failure %d\n", rc);
			return rc;
		}
	} else
		u_req->dma = 0;

	EPVDBG(ep, "enqueue req @%p\n", req);
	EPVDBG(ep, " l=%d dma=0x%x zero=%d noshort=%d noirq=%d is_in=%d\n",
	       u_req->length, (u32)u_req->dma, u_req->zero,
	       u_req->short_not_ok, u_req->no_interrupt,
	       ep->epn.is_in);

	 
	u_req->status = -EINPROGRESS;
	u_req->actual = 0;
	req->act_count = 0;
	req->active = false;
	req->last_desc = -1;
	spin_lock_irqsave(&vhub->lock, flags);
	empty = list_empty(&ep->queue);

	 
	list_add_tail(&req->queue, &ep->queue);
	if (empty) {
		if (ep->epn.desc_mode)
			ast_vhub_epn_kick_desc(ep, req);
		else
			ast_vhub_epn_kick(ep, req);
	}
	spin_unlock_irqrestore(&vhub->lock, flags);

	return 0;
}

static void ast_vhub_stop_active_req(struct ast_vhub_ep *ep,
				     bool restart_ep)
{
	u32 state, reg, loops;

	 
	if (ep->epn.desc_mode)
		writel(VHUB_EP_DMA_CTRL_RESET, ep->epn.regs + AST_VHUB_EP_DMA_CTLSTAT);
	else
		writel(0, ep->epn.regs + AST_VHUB_EP_DMA_CTLSTAT);

	 
	for (loops = 0; loops < 1000; loops++) {
		state = readl(ep->epn.regs + AST_VHUB_EP_DMA_CTLSTAT);
		state = VHUB_EP_DMA_PROC_STATUS(state);
		if (state == EP_DMA_PROC_RX_IDLE ||
		    state == EP_DMA_PROC_TX_IDLE)
			break;
		udelay(1);
	}
	if (loops >= 1000)
		dev_warn(&ep->vhub->pdev->dev, "Timeout waiting for DMA\n");

	 
	if (!restart_ep)
		return;

	 
	if (ep->epn.desc_mode) {
		 
		reg = VHUB_EP_DMA_SET_RPTR(ep->epn.d_next) |
			VHUB_EP_DMA_SET_CPU_WPTR(ep->epn.d_next);
		writel(reg, ep->epn.regs + AST_VHUB_EP_DESC_STATUS);

		 
		writel(ep->epn.dma_conf,
		       ep->epn.regs + AST_VHUB_EP_DMA_CTLSTAT);
	} else {
		 
		writel(ep->epn.dma_conf,
		       ep->epn.regs + AST_VHUB_EP_DMA_CTLSTAT);
	}
}

static int ast_vhub_epn_dequeue(struct usb_ep* u_ep, struct usb_request *u_req)
{
	struct ast_vhub_ep *ep = to_ast_ep(u_ep);
	struct ast_vhub *vhub = ep->vhub;
	struct ast_vhub_req *req = NULL, *iter;
	unsigned long flags;
	int rc = -EINVAL;

	spin_lock_irqsave(&vhub->lock, flags);

	 
	list_for_each_entry(iter, &ep->queue, queue) {
		if (&iter->req != u_req)
			continue;
		req = iter;
		break;
	}

	if (req) {
		EPVDBG(ep, "dequeue req @%p active=%d\n",
		       req, req->active);
		if (req->active)
			ast_vhub_stop_active_req(ep, true);
		ast_vhub_done(ep, req, -ECONNRESET);
		rc = 0;
	}

	spin_unlock_irqrestore(&vhub->lock, flags);
	return rc;
}

void ast_vhub_update_epn_stall(struct ast_vhub_ep *ep)
{
	u32 reg;

	if (WARN_ON(ep->d_idx == 0))
		return;
	reg = readl(ep->epn.regs + AST_VHUB_EP_CONFIG);
	if (ep->epn.stalled || ep->epn.wedged)
		reg |= VHUB_EP_CFG_STALL_CTRL;
	else
		reg &= ~VHUB_EP_CFG_STALL_CTRL;
	writel(reg, ep->epn.regs + AST_VHUB_EP_CONFIG);

	if (!ep->epn.stalled && !ep->epn.wedged)
		writel(VHUB_EP_TOGGLE_SET_EPNUM(ep->epn.g_idx),
		       ep->vhub->regs + AST_VHUB_EP_TOGGLE);
}

static int ast_vhub_set_halt_and_wedge(struct usb_ep* u_ep, bool halt,
				      bool wedge)
{
	struct ast_vhub_ep *ep = to_ast_ep(u_ep);
	struct ast_vhub *vhub = ep->vhub;
	unsigned long flags;

	EPDBG(ep, "Set halt (%d) & wedge (%d)\n", halt, wedge);

	if (!u_ep || !u_ep->desc)
		return -EINVAL;
	if (ep->d_idx == 0)
		return 0;
	if (ep->epn.is_iso)
		return -EOPNOTSUPP;

	spin_lock_irqsave(&vhub->lock, flags);

	 
	if (halt && ep->epn.is_in && !list_empty(&ep->queue)) {
		spin_unlock_irqrestore(&vhub->lock, flags);
		return -EAGAIN;
	}
	ep->epn.stalled = halt;
	ep->epn.wedged = wedge;
	ast_vhub_update_epn_stall(ep);

	spin_unlock_irqrestore(&vhub->lock, flags);

	return 0;
}

static int ast_vhub_epn_set_halt(struct usb_ep *u_ep, int value)
{
	return ast_vhub_set_halt_and_wedge(u_ep, value != 0, false);
}

static int ast_vhub_epn_set_wedge(struct usb_ep *u_ep)
{
	return ast_vhub_set_halt_and_wedge(u_ep, true, true);
}

static int ast_vhub_epn_disable(struct usb_ep* u_ep)
{
	struct ast_vhub_ep *ep = to_ast_ep(u_ep);
	struct ast_vhub *vhub = ep->vhub;
	unsigned long flags;
	u32 imask, ep_ier;

	EPDBG(ep, "Disabling !\n");

	spin_lock_irqsave(&vhub->lock, flags);

	ep->epn.enabled = false;

	 
	ast_vhub_stop_active_req(ep, false);

	 
	writel(0, ep->epn.regs + AST_VHUB_EP_CONFIG);

	 
	imask = VHUB_EP_IRQ(ep->epn.g_idx);
	ep_ier = readl(vhub->regs + AST_VHUB_EP_ACK_IER);
	ep_ier &= ~imask;
	writel(ep_ier, vhub->regs + AST_VHUB_EP_ACK_IER);
	writel(imask, vhub->regs + AST_VHUB_EP_ACK_ISR);

	 
	ast_vhub_nuke(ep, -ESHUTDOWN);

	 
	ep->ep.desc = NULL;

	spin_unlock_irqrestore(&vhub->lock, flags);

	return 0;
}

static int ast_vhub_epn_enable(struct usb_ep* u_ep,
			       const struct usb_endpoint_descriptor *desc)
{
	struct ast_vhub_ep *ep = to_ast_ep(u_ep);
	struct ast_vhub_dev *dev;
	struct ast_vhub *vhub;
	u16 maxpacket, type;
	unsigned long flags;
	u32 ep_conf, ep_ier, imask;

	 
	if (!u_ep || !desc)
		return -EINVAL;

	maxpacket = usb_endpoint_maxp(desc);
	if (!ep->d_idx || !ep->dev ||
	    desc->bDescriptorType != USB_DT_ENDPOINT ||
	    maxpacket == 0 || maxpacket > ep->ep.maxpacket) {
		EPDBG(ep, "Invalid EP enable,d_idx=%d,dev=%p,type=%d,mp=%d/%d\n",
		      ep->d_idx, ep->dev, desc->bDescriptorType,
		      maxpacket, ep->ep.maxpacket);
		return -EINVAL;
	}
	if (ep->d_idx != usb_endpoint_num(desc)) {
		EPDBG(ep, "EP number mismatch !\n");
		return -EINVAL;
	}

	if (ep->epn.enabled) {
		EPDBG(ep, "Already enabled\n");
		return -EBUSY;
	}
	dev = ep->dev;
	vhub = ep->vhub;

	 
	if (!dev->driver) {
		EPDBG(ep, "Bogus device state: driver=%p speed=%d\n",
		       dev->driver, dev->gadget.speed);
		return -ESHUTDOWN;
	}

	 
	ep->epn.is_in = usb_endpoint_dir_in(desc);
	ep->ep.maxpacket = maxpacket;
	type = usb_endpoint_type(desc);
	ep->epn.d_next = ep->epn.d_last = 0;
	ep->epn.is_iso = false;
	ep->epn.stalled = false;
	ep->epn.wedged = false;

	EPDBG(ep, "Enabling [%s] %s num %d maxpacket=%d\n",
	      ep->epn.is_in ? "in" : "out", usb_ep_type_string(type),
	      usb_endpoint_num(desc), maxpacket);

	 
	ep->epn.desc_mode = ep->epn.descs && ep->epn.is_in;
	if (ep->epn.desc_mode)
		memset(ep->epn.descs, 0, 8 * AST_VHUB_DESCS_COUNT);

	 
	ep->epn.chunk_max = ep->ep.maxpacket;
	if (ep->epn.is_in) {
		ep->epn.chunk_max <<= 3;
		while (ep->epn.chunk_max > 4095)
			ep->epn.chunk_max -= ep->ep.maxpacket;
	}

	switch(type) {
	case USB_ENDPOINT_XFER_CONTROL:
		EPDBG(ep, "Only one control endpoint\n");
		return -EINVAL;
	case USB_ENDPOINT_XFER_INT:
		ep_conf = VHUB_EP_CFG_SET_TYPE(EP_TYPE_INT);
		break;
	case USB_ENDPOINT_XFER_BULK:
		ep_conf = VHUB_EP_CFG_SET_TYPE(EP_TYPE_BULK);
		break;
	case USB_ENDPOINT_XFER_ISOC:
		ep_conf = VHUB_EP_CFG_SET_TYPE(EP_TYPE_ISO);
		ep->epn.is_iso = true;
		break;
	default:
		return -EINVAL;
	}

	 
	if (maxpacket < 1024)
		ep_conf |= VHUB_EP_CFG_SET_MAX_PKT(maxpacket);
	if (!ep->epn.is_in)
		ep_conf |= VHUB_EP_CFG_DIR_OUT;
	ep_conf |= VHUB_EP_CFG_SET_EP_NUM(usb_endpoint_num(desc));
	ep_conf |= VHUB_EP_CFG_ENABLE;
	ep_conf |= VHUB_EP_CFG_SET_DEV(dev->index + 1);
	EPVDBG(ep, "config=%08x\n", ep_conf);

	spin_lock_irqsave(&vhub->lock, flags);

	 
	writel(0, ep->epn.regs + AST_VHUB_EP_CONFIG);
	writel(VHUB_EP_DMA_CTRL_RESET,
	       ep->epn.regs + AST_VHUB_EP_DMA_CTLSTAT);

	 
	writel(ep_conf, ep->epn.regs + AST_VHUB_EP_CONFIG);

	if (ep->epn.desc_mode) {
		 
		writel(0, ep->epn.regs + AST_VHUB_EP_DESC_STATUS);

		 
		writel(ep->epn.descs_dma,
		       ep->epn.regs + AST_VHUB_EP_DESC_BASE);

		 
		ep->epn.dma_conf = VHUB_EP_DMA_DESC_MODE;
		if (ep->epn.is_in)
			ep->epn.dma_conf |= VHUB_EP_DMA_IN_LONG_MODE;

		 
		writel(ep->epn.dma_conf | VHUB_EP_DMA_CTRL_RESET,
		       ep->epn.regs + AST_VHUB_EP_DMA_CTLSTAT);

		 
		writel(ep->epn.dma_conf,
		       ep->epn.regs + AST_VHUB_EP_DMA_CTLSTAT);
	} else {
		 
		ep->epn.dma_conf = VHUB_EP_DMA_SINGLE_STAGE;

		 
		writel(ep->epn.dma_conf | VHUB_EP_DMA_CTRL_RESET,
		       ep->epn.regs + AST_VHUB_EP_DMA_CTLSTAT);
		writel(ep->epn.dma_conf,
		       ep->epn.regs + AST_VHUB_EP_DMA_CTLSTAT);
		writel(0, ep->epn.regs + AST_VHUB_EP_DESC_STATUS);
	}

	 
	writel(VHUB_EP_TOGGLE_SET_EPNUM(ep->epn.g_idx),
	       vhub->regs + AST_VHUB_EP_TOGGLE);

	 
	imask = VHUB_EP_IRQ(ep->epn.g_idx);
	writel(imask, vhub->regs + AST_VHUB_EP_ACK_ISR);
	ep_ier = readl(vhub->regs + AST_VHUB_EP_ACK_IER);
	ep_ier |= imask;
	writel(ep_ier, vhub->regs + AST_VHUB_EP_ACK_IER);

	 
	ep->epn.enabled = true;

	spin_unlock_irqrestore(&vhub->lock, flags);

	return 0;
}

static void ast_vhub_epn_dispose(struct usb_ep *u_ep)
{
	struct ast_vhub_ep *ep = to_ast_ep(u_ep);

	if (WARN_ON(!ep->dev || !ep->d_idx))
		return;

	EPDBG(ep, "Releasing endpoint\n");

	 
	list_del_init(&ep->ep.ep_list);

	 
	ep->dev->epns[ep->d_idx - 1] = NULL;

	 
	kfree(ep->ep.name);
	ep->ep.name = NULL;
	dma_free_coherent(&ep->vhub->pdev->dev,
			  AST_VHUB_EPn_MAX_PACKET +
			  8 * AST_VHUB_DESCS_COUNT,
			  ep->buf, ep->buf_dma);
	ep->buf = NULL;
	ep->epn.descs = NULL;

	 
	ep->dev = NULL;
}

static const struct usb_ep_ops ast_vhub_epn_ops = {
	.enable		= ast_vhub_epn_enable,
	.disable	= ast_vhub_epn_disable,
	.dispose	= ast_vhub_epn_dispose,
	.queue		= ast_vhub_epn_queue,
	.dequeue	= ast_vhub_epn_dequeue,
	.set_halt	= ast_vhub_epn_set_halt,
	.set_wedge	= ast_vhub_epn_set_wedge,
	.alloc_request	= ast_vhub_alloc_request,
	.free_request	= ast_vhub_free_request,
};

struct ast_vhub_ep *ast_vhub_alloc_epn(struct ast_vhub_dev *d, u8 addr)
{
	struct ast_vhub *vhub = d->vhub;
	struct ast_vhub_ep *ep;
	unsigned long flags;
	int i;

	 
	spin_lock_irqsave(&vhub->lock, flags);
	for (i = 0; i < vhub->max_epns; i++)
		if (vhub->epns[i].dev == NULL)
			break;
	if (i >= vhub->max_epns) {
		spin_unlock_irqrestore(&vhub->lock, flags);
		return NULL;
	}

	 
	ep = &vhub->epns[i];
	ep->dev = d;
	spin_unlock_irqrestore(&vhub->lock, flags);

	DDBG(d, "Allocating gen EP %d for addr %d\n", i, addr);
	INIT_LIST_HEAD(&ep->queue);
	ep->d_idx = addr;
	ep->vhub = vhub;
	ep->ep.ops = &ast_vhub_epn_ops;
	ep->ep.name = kasprintf(GFP_KERNEL, "ep%d", addr);
	d->epns[addr-1] = ep;
	ep->epn.g_idx = i;
	ep->epn.regs = vhub->regs + 0x200 + (i * 0x10);

	ep->buf = dma_alloc_coherent(&vhub->pdev->dev,
				     AST_VHUB_EPn_MAX_PACKET +
				     8 * AST_VHUB_DESCS_COUNT,
				     &ep->buf_dma, GFP_KERNEL);
	if (!ep->buf) {
		kfree(ep->ep.name);
		ep->ep.name = NULL;
		return NULL;
	}
	ep->epn.descs = ep->buf + AST_VHUB_EPn_MAX_PACKET;
	ep->epn.descs_dma = ep->buf_dma + AST_VHUB_EPn_MAX_PACKET;

	usb_ep_set_maxpacket_limit(&ep->ep, AST_VHUB_EPn_MAX_PACKET);
	list_add_tail(&ep->ep.ep_list, &d->gadget.ep_list);
	ep->ep.caps.type_iso = true;
	ep->ep.caps.type_bulk = true;
	ep->ep.caps.type_int = true;
	ep->ep.caps.dir_in = true;
	ep->ep.caps.dir_out = true;

	return ep;
}
