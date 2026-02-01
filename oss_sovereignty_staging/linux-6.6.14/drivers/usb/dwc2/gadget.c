
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/phy.h>
#include <linux/usb/composite.h>


#include "core.h"
#include "hw.h"

 
static inline struct dwc2_hsotg_req *our_req(struct usb_request *req)
{
	return container_of(req, struct dwc2_hsotg_req, req);
}

static inline struct dwc2_hsotg_ep *our_ep(struct usb_ep *ep)
{
	return container_of(ep, struct dwc2_hsotg_ep, ep);
}

static inline struct dwc2_hsotg *to_hsotg(struct usb_gadget *gadget)
{
	return container_of(gadget, struct dwc2_hsotg, gadget);
}

static inline void dwc2_set_bit(struct dwc2_hsotg *hsotg, u32 offset, u32 val)
{
	dwc2_writel(hsotg, dwc2_readl(hsotg, offset) | val, offset);
}

static inline void dwc2_clear_bit(struct dwc2_hsotg *hsotg, u32 offset, u32 val)
{
	dwc2_writel(hsotg, dwc2_readl(hsotg, offset) & ~val, offset);
}

static inline struct dwc2_hsotg_ep *index_to_ep(struct dwc2_hsotg *hsotg,
						u32 ep_index, u32 dir_in)
{
	if (dir_in)
		return hsotg->eps_in[ep_index];
	else
		return hsotg->eps_out[ep_index];
}

 
static void dwc2_hsotg_dump(struct dwc2_hsotg *hsotg);

 
static inline bool using_dma(struct dwc2_hsotg *hsotg)
{
	return hsotg->params.g_dma;
}

 
static inline bool using_desc_dma(struct dwc2_hsotg *hsotg)
{
	return hsotg->params.g_dma_desc;
}

 
static inline void dwc2_gadget_incr_frame_num(struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	u16 limit = DSTS_SOFFN_LIMIT;

	if (hsotg->gadget.speed != USB_SPEED_HIGH)
		limit >>= 3;

	hs_ep->target_frame += hs_ep->interval;
	if (hs_ep->target_frame > limit) {
		hs_ep->frame_overrun = true;
		hs_ep->target_frame &= limit;
	} else {
		hs_ep->frame_overrun = false;
	}
}

 
static inline void dwc2_gadget_dec_frame_num_by_one(struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	u16 limit = DSTS_SOFFN_LIMIT;

	if (hsotg->gadget.speed != USB_SPEED_HIGH)
		limit >>= 3;

	if (hs_ep->target_frame)
		hs_ep->target_frame -= 1;
	else
		hs_ep->target_frame = limit;
}

 
static void dwc2_hsotg_en_gsint(struct dwc2_hsotg *hsotg, u32 ints)
{
	u32 gsintmsk = dwc2_readl(hsotg, GINTMSK);
	u32 new_gsintmsk;

	new_gsintmsk = gsintmsk | ints;

	if (new_gsintmsk != gsintmsk) {
		dev_dbg(hsotg->dev, "gsintmsk now 0x%08x\n", new_gsintmsk);
		dwc2_writel(hsotg, new_gsintmsk, GINTMSK);
	}
}

 
static void dwc2_hsotg_disable_gsint(struct dwc2_hsotg *hsotg, u32 ints)
{
	u32 gsintmsk = dwc2_readl(hsotg, GINTMSK);
	u32 new_gsintmsk;

	new_gsintmsk = gsintmsk & ~ints;

	if (new_gsintmsk != gsintmsk)
		dwc2_writel(hsotg, new_gsintmsk, GINTMSK);
}

 
static void dwc2_hsotg_ctrl_epint(struct dwc2_hsotg *hsotg,
				  unsigned int ep, unsigned int dir_in,
				 unsigned int en)
{
	unsigned long flags;
	u32 bit = 1 << ep;
	u32 daint;

	if (!dir_in)
		bit <<= 16;

	local_irq_save(flags);
	daint = dwc2_readl(hsotg, DAINTMSK);
	if (en)
		daint |= bit;
	else
		daint &= ~bit;
	dwc2_writel(hsotg, daint, DAINTMSK);
	local_irq_restore(flags);
}

 
int dwc2_hsotg_tx_fifo_count(struct dwc2_hsotg *hsotg)
{
	if (hsotg->hw_params.en_multiple_tx_fifo)
		 
		return hsotg->hw_params.num_dev_in_eps;
	else
		 
		return hsotg->hw_params.num_dev_perio_in_ep;
}

 
int dwc2_hsotg_tx_fifo_total_depth(struct dwc2_hsotg *hsotg)
{
	int addr;
	int tx_addr_max;
	u32 np_tx_fifo_size;

	np_tx_fifo_size = min_t(u32, hsotg->hw_params.dev_nperio_tx_fifo_size,
				hsotg->params.g_np_tx_fifo_size);

	 
	tx_addr_max = hsotg->hw_params.total_fifo_size;

	addr = hsotg->params.g_rx_fifo_size + np_tx_fifo_size;
	if (tx_addr_max <= addr)
		return 0;

	return tx_addr_max - addr;
}

 
static void dwc2_gadget_wkup_alert_handler(struct dwc2_hsotg *hsotg)
{
	u32 gintsts2;
	u32 gintmsk2;

	gintsts2 = dwc2_readl(hsotg, GINTSTS2);
	gintmsk2 = dwc2_readl(hsotg, GINTMSK2);
	gintsts2 &= gintmsk2;

	if (gintsts2 & GINTSTS2_WKUP_ALERT_INT) {
		dev_dbg(hsotg->dev, "%s: Wkup_Alert_Int\n", __func__);
		dwc2_set_bit(hsotg, GINTSTS2, GINTSTS2_WKUP_ALERT_INT);
		dwc2_set_bit(hsotg, DCTL, DCTL_RMTWKUPSIG);
	}
}

 
int dwc2_hsotg_tx_fifo_average_depth(struct dwc2_hsotg *hsotg)
{
	int tx_fifo_count;
	int tx_fifo_depth;

	tx_fifo_depth = dwc2_hsotg_tx_fifo_total_depth(hsotg);

	tx_fifo_count = dwc2_hsotg_tx_fifo_count(hsotg);

	if (!tx_fifo_count)
		return tx_fifo_depth;
	else
		return tx_fifo_depth / tx_fifo_count;
}

 
static void dwc2_hsotg_init_fifo(struct dwc2_hsotg *hsotg)
{
	unsigned int ep;
	unsigned int addr;
	int timeout;

	u32 val;
	u32 *txfsz = hsotg->params.g_tx_fifo_size;

	 
	WARN_ON(hsotg->fifo_map);
	hsotg->fifo_map = 0;

	 
	dwc2_writel(hsotg, hsotg->params.g_rx_fifo_size, GRXFSIZ);
	dwc2_writel(hsotg, (hsotg->params.g_rx_fifo_size <<
		    FIFOSIZE_STARTADDR_SHIFT) |
		    (hsotg->params.g_np_tx_fifo_size << FIFOSIZE_DEPTH_SHIFT),
		    GNPTXFSIZ);

	 

	 
	addr = hsotg->params.g_rx_fifo_size + hsotg->params.g_np_tx_fifo_size;

	 
	for (ep = 1; ep < MAX_EPS_CHANNELS; ep++) {
		if (!txfsz[ep])
			continue;
		val = addr;
		val |= txfsz[ep] << FIFOSIZE_DEPTH_SHIFT;
		WARN_ONCE(addr + txfsz[ep] > hsotg->fifo_mem,
			  "insufficient fifo memory");
		addr += txfsz[ep];

		dwc2_writel(hsotg, val, DPTXFSIZN(ep));
		val = dwc2_readl(hsotg, DPTXFSIZN(ep));
	}

	dwc2_writel(hsotg, hsotg->hw_params.total_fifo_size |
		    addr << GDFIFOCFG_EPINFOBASE_SHIFT,
		    GDFIFOCFG);
	 

	dwc2_writel(hsotg, GRSTCTL_TXFNUM(0x10) | GRSTCTL_TXFFLSH |
	       GRSTCTL_RXFFLSH, GRSTCTL);

	 
	timeout = 100;
	while (1) {
		val = dwc2_readl(hsotg, GRSTCTL);

		if ((val & (GRSTCTL_TXFFLSH | GRSTCTL_RXFFLSH)) == 0)
			break;

		if (--timeout == 0) {
			dev_err(hsotg->dev,
				"%s: timeout flushing fifos (GRSTCTL=%08x)\n",
				__func__, val);
			break;
		}

		udelay(1);
	}

	dev_dbg(hsotg->dev, "FIFOs reset, timeout at %d\n", timeout);
}

 
static struct usb_request *dwc2_hsotg_ep_alloc_request(struct usb_ep *ep,
						       gfp_t flags)
{
	struct dwc2_hsotg_req *req;

	req = kzalloc(sizeof(*req), flags);
	if (!req)
		return NULL;

	INIT_LIST_HEAD(&req->queue);

	return &req->req;
}

 
static inline int is_ep_periodic(struct dwc2_hsotg_ep *hs_ep)
{
	return hs_ep->periodic;
}

 
static void dwc2_hsotg_unmap_dma(struct dwc2_hsotg *hsotg,
				 struct dwc2_hsotg_ep *hs_ep,
				struct dwc2_hsotg_req *hs_req)
{
	struct usb_request *req = &hs_req->req;

	usb_gadget_unmap_request(&hsotg->gadget, req, hs_ep->map_dir);
}

 
static int dwc2_gadget_alloc_ctrl_desc_chains(struct dwc2_hsotg *hsotg)
{
	hsotg->setup_desc[0] =
		dmam_alloc_coherent(hsotg->dev,
				    sizeof(struct dwc2_dma_desc),
				    &hsotg->setup_desc_dma[0],
				    GFP_KERNEL);
	if (!hsotg->setup_desc[0])
		goto fail;

	hsotg->setup_desc[1] =
		dmam_alloc_coherent(hsotg->dev,
				    sizeof(struct dwc2_dma_desc),
				    &hsotg->setup_desc_dma[1],
				    GFP_KERNEL);
	if (!hsotg->setup_desc[1])
		goto fail;

	hsotg->ctrl_in_desc =
		dmam_alloc_coherent(hsotg->dev,
				    sizeof(struct dwc2_dma_desc),
				    &hsotg->ctrl_in_desc_dma,
				    GFP_KERNEL);
	if (!hsotg->ctrl_in_desc)
		goto fail;

	hsotg->ctrl_out_desc =
		dmam_alloc_coherent(hsotg->dev,
				    sizeof(struct dwc2_dma_desc),
				    &hsotg->ctrl_out_desc_dma,
				    GFP_KERNEL);
	if (!hsotg->ctrl_out_desc)
		goto fail;

	return 0;

fail:
	return -ENOMEM;
}

 
static int dwc2_hsotg_write_fifo(struct dwc2_hsotg *hsotg,
				 struct dwc2_hsotg_ep *hs_ep,
				struct dwc2_hsotg_req *hs_req)
{
	bool periodic = is_ep_periodic(hs_ep);
	u32 gnptxsts = dwc2_readl(hsotg, GNPTXSTS);
	int buf_pos = hs_req->req.actual;
	int to_write = hs_ep->size_loaded;
	void *data;
	int can_write;
	int pkt_round;
	int max_transfer;

	to_write -= (buf_pos - hs_ep->last_load);

	 
	if (to_write == 0)
		return 0;

	if (periodic && !hsotg->dedicated_fifos) {
		u32 epsize = dwc2_readl(hsotg, DIEPTSIZ(hs_ep->index));
		int size_left;
		int size_done;

		 

		size_left = DXEPTSIZ_XFERSIZE_GET(epsize);

		 
		if (hs_ep->fifo_load != 0) {
			dwc2_hsotg_en_gsint(hsotg, GINTSTS_PTXFEMP);
			return -ENOSPC;
		}

		dev_dbg(hsotg->dev, "%s: left=%d, load=%d, fifo=%d, size %d\n",
			__func__, size_left,
			hs_ep->size_loaded, hs_ep->fifo_load, hs_ep->fifo_size);

		 
		size_done = hs_ep->size_loaded - size_left;

		 
		can_write = hs_ep->fifo_load - size_done;
		dev_dbg(hsotg->dev, "%s: => can_write1=%d\n",
			__func__, can_write);

		can_write = hs_ep->fifo_size - can_write;
		dev_dbg(hsotg->dev, "%s: => can_write2=%d\n",
			__func__, can_write);

		if (can_write <= 0) {
			dwc2_hsotg_en_gsint(hsotg, GINTSTS_PTXFEMP);
			return -ENOSPC;
		}
	} else if (hsotg->dedicated_fifos && hs_ep->index != 0) {
		can_write = dwc2_readl(hsotg,
				       DTXFSTS(hs_ep->fifo_index));

		can_write &= 0xffff;
		can_write *= 4;
	} else {
		if (GNPTXSTS_NP_TXQ_SPC_AVAIL_GET(gnptxsts) == 0) {
			dev_dbg(hsotg->dev,
				"%s: no queue slots available (0x%08x)\n",
				__func__, gnptxsts);

			dwc2_hsotg_en_gsint(hsotg, GINTSTS_NPTXFEMP);
			return -ENOSPC;
		}

		can_write = GNPTXSTS_NP_TXF_SPC_AVAIL_GET(gnptxsts);
		can_write *= 4;	 
	}

	max_transfer = hs_ep->ep.maxpacket * hs_ep->mc;

	dev_dbg(hsotg->dev, "%s: GNPTXSTS=%08x, can=%d, to=%d, max_transfer %d\n",
		__func__, gnptxsts, can_write, to_write, max_transfer);

	 
	if (can_write > 512 && !periodic)
		can_write = 512;

	 
	if (to_write > max_transfer) {
		to_write = max_transfer;

		 
		if (!hsotg->dedicated_fifos)
			dwc2_hsotg_en_gsint(hsotg,
					    periodic ? GINTSTS_PTXFEMP :
					   GINTSTS_NPTXFEMP);
	}

	 

	if (to_write > can_write) {
		to_write = can_write;
		pkt_round = to_write % max_transfer;

		 

		if (pkt_round)
			to_write -= pkt_round;

		 

		 
		if (!hsotg->dedicated_fifos)
			dwc2_hsotg_en_gsint(hsotg,
					    periodic ? GINTSTS_PTXFEMP :
					   GINTSTS_NPTXFEMP);
	}

	dev_dbg(hsotg->dev, "write %d/%d, can_write %d, done %d\n",
		to_write, hs_req->req.length, can_write, buf_pos);

	if (to_write <= 0)
		return -ENOSPC;

	hs_req->req.actual = buf_pos + to_write;
	hs_ep->total_data += to_write;

	if (periodic)
		hs_ep->fifo_load += to_write;

	to_write = DIV_ROUND_UP(to_write, 4);
	data = hs_req->req.buf + buf_pos;

	dwc2_writel_rep(hsotg, EPFIFO(hs_ep->index), data, to_write);

	return (to_write >= can_write) ? -ENOSPC : 0;
}

 
static unsigned int get_ep_limit(struct dwc2_hsotg_ep *hs_ep)
{
	int index = hs_ep->index;
	unsigned int maxsize;
	unsigned int maxpkt;

	if (index != 0) {
		maxsize = DXEPTSIZ_XFERSIZE_LIMIT + 1;
		maxpkt = DXEPTSIZ_PKTCNT_LIMIT + 1;
	} else {
		maxsize = 64 + 64;
		if (hs_ep->dir_in)
			maxpkt = DIEPTSIZ0_PKTCNT_LIMIT + 1;
		else
			maxpkt = 2;
	}

	 
	maxpkt--;
	maxsize--;

	 

	if ((maxpkt * hs_ep->ep.maxpacket) < maxsize)
		maxsize = maxpkt * hs_ep->ep.maxpacket;

	return maxsize;
}

 
static u32 dwc2_hsotg_read_frameno(struct dwc2_hsotg *hsotg)
{
	u32 dsts;

	dsts = dwc2_readl(hsotg, DSTS);
	dsts &= DSTS_SOFFN_MASK;
	dsts >>= DSTS_SOFFN_SHIFT;

	return dsts;
}

 
static unsigned int dwc2_gadget_get_chain_limit(struct dwc2_hsotg_ep *hs_ep)
{
	const struct usb_endpoint_descriptor *ep_desc = hs_ep->ep.desc;
	int is_isoc = hs_ep->isochronous;
	unsigned int maxsize;
	u32 mps = hs_ep->ep.maxpacket;
	int dir_in = hs_ep->dir_in;

	if (is_isoc)
		maxsize = (hs_ep->dir_in ? DEV_DMA_ISOC_TX_NBYTES_LIMIT :
					   DEV_DMA_ISOC_RX_NBYTES_LIMIT) *
					   MAX_DMA_DESC_NUM_HS_ISOC;
	else
		maxsize = DEV_DMA_NBYTES_LIMIT * MAX_DMA_DESC_NUM_GENERIC;

	 
	if (hs_ep->index)
		if (usb_endpoint_xfer_int(ep_desc) && !dir_in && (mps % 4))
			maxsize = mps * MAX_DMA_DESC_NUM_GENERIC;

	return maxsize;
}

 
static u32 dwc2_gadget_get_desc_params(struct dwc2_hsotg_ep *hs_ep, u32 *mask)
{
	const struct usb_endpoint_descriptor *ep_desc = hs_ep->ep.desc;
	u32 mps = hs_ep->ep.maxpacket;
	int dir_in = hs_ep->dir_in;
	u32 desc_size = 0;

	if (!hs_ep->index && !dir_in) {
		desc_size = mps;
		*mask = DEV_DMA_NBYTES_MASK;
	} else if (hs_ep->isochronous) {
		if (dir_in) {
			desc_size = DEV_DMA_ISOC_TX_NBYTES_LIMIT;
			*mask = DEV_DMA_ISOC_TX_NBYTES_MASK;
		} else {
			desc_size = DEV_DMA_ISOC_RX_NBYTES_LIMIT;
			*mask = DEV_DMA_ISOC_RX_NBYTES_MASK;
		}
	} else {
		desc_size = DEV_DMA_NBYTES_LIMIT;
		*mask = DEV_DMA_NBYTES_MASK;

		 
		desc_size -= desc_size % mps;
	}

	 
	if (hs_ep->index)
		if (usb_endpoint_xfer_int(ep_desc) && !dir_in && (mps % 4)) {
			desc_size = mps;
			*mask = DEV_DMA_NBYTES_MASK;
		}

	return desc_size;
}

static void dwc2_gadget_fill_nonisoc_xfer_ddma_one(struct dwc2_hsotg_ep *hs_ep,
						 struct dwc2_dma_desc **desc,
						 dma_addr_t dma_buff,
						 unsigned int len,
						 bool true_last)
{
	int dir_in = hs_ep->dir_in;
	u32 mps = hs_ep->ep.maxpacket;
	u32 maxsize = 0;
	u32 offset = 0;
	u32 mask = 0;
	int i;

	maxsize = dwc2_gadget_get_desc_params(hs_ep, &mask);

	hs_ep->desc_count = (len / maxsize) +
				((len % maxsize) ? 1 : 0);
	if (len == 0)
		hs_ep->desc_count = 1;

	for (i = 0; i < hs_ep->desc_count; ++i) {
		(*desc)->status = 0;
		(*desc)->status |= (DEV_DMA_BUFF_STS_HBUSY
				 << DEV_DMA_BUFF_STS_SHIFT);

		if (len > maxsize) {
			if (!hs_ep->index && !dir_in)
				(*desc)->status |= (DEV_DMA_L | DEV_DMA_IOC);

			(*desc)->status |=
				maxsize << DEV_DMA_NBYTES_SHIFT & mask;
			(*desc)->buf = dma_buff + offset;

			len -= maxsize;
			offset += maxsize;
		} else {
			if (true_last)
				(*desc)->status |= (DEV_DMA_L | DEV_DMA_IOC);

			if (dir_in)
				(*desc)->status |= (len % mps) ? DEV_DMA_SHORT :
					((hs_ep->send_zlp && true_last) ?
					DEV_DMA_SHORT : 0);

			(*desc)->status |=
				len << DEV_DMA_NBYTES_SHIFT & mask;
			(*desc)->buf = dma_buff + offset;
		}

		(*desc)->status &= ~DEV_DMA_BUFF_STS_MASK;
		(*desc)->status |= (DEV_DMA_BUFF_STS_HREADY
				 << DEV_DMA_BUFF_STS_SHIFT);
		(*desc)++;
	}
}

 
static void dwc2_gadget_config_nonisoc_xfer_ddma(struct dwc2_hsotg_ep *hs_ep,
						 dma_addr_t dma_buff,
						 unsigned int len)
{
	struct usb_request *ureq = NULL;
	struct dwc2_dma_desc *desc = hs_ep->desc_list;
	struct scatterlist *sg;
	int i;
	u8 desc_count = 0;

	if (hs_ep->req)
		ureq = &hs_ep->req->req;

	 
	if (!ureq || !ureq->num_sgs) {
		dwc2_gadget_fill_nonisoc_xfer_ddma_one(hs_ep, &desc,
			dma_buff, len, true);
		return;
	}

	 
	for_each_sg(ureq->sg, sg, ureq->num_sgs, i) {
		dwc2_gadget_fill_nonisoc_xfer_ddma_one(hs_ep, &desc,
			sg_dma_address(sg) + sg->offset, sg_dma_len(sg),
			sg_is_last(sg));
		desc_count += hs_ep->desc_count;
	}

	hs_ep->desc_count = desc_count;
}

 
static int dwc2_gadget_fill_isoc_desc(struct dwc2_hsotg_ep *hs_ep,
				      dma_addr_t dma_buff, unsigned int len)
{
	struct dwc2_dma_desc *desc;
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	u32 index;
	u32 mask = 0;
	u8 pid = 0;

	dwc2_gadget_get_desc_params(hs_ep, &mask);

	index = hs_ep->next_desc;
	desc = &hs_ep->desc_list[index];

	 
	if ((desc->status >> DEV_DMA_BUFF_STS_SHIFT) ==
	    DEV_DMA_BUFF_STS_HREADY) {
		dev_dbg(hsotg->dev, "%s: desc chain full\n", __func__);
		return 1;
	}

	 
	if (hs_ep->next_desc)
		hs_ep->desc_list[index - 1].status &= ~DEV_DMA_L;

	dev_dbg(hsotg->dev, "%s: Filling ep %d, dir %s isoc desc # %d\n",
		__func__, hs_ep->index, hs_ep->dir_in ? "in" : "out", index);

	desc->status = 0;
	desc->status |= (DEV_DMA_BUFF_STS_HBUSY	<< DEV_DMA_BUFF_STS_SHIFT);

	desc->buf = dma_buff;
	desc->status |= (DEV_DMA_L | DEV_DMA_IOC |
			 ((len << DEV_DMA_NBYTES_SHIFT) & mask));

	if (hs_ep->dir_in) {
		if (len)
			pid = DIV_ROUND_UP(len, hs_ep->ep.maxpacket);
		else
			pid = 1;
		desc->status |= ((pid << DEV_DMA_ISOC_PID_SHIFT) &
				 DEV_DMA_ISOC_PID_MASK) |
				((len % hs_ep->ep.maxpacket) ?
				 DEV_DMA_SHORT : 0) |
				((hs_ep->target_frame <<
				  DEV_DMA_ISOC_FRNUM_SHIFT) &
				 DEV_DMA_ISOC_FRNUM_MASK);
	}

	desc->status &= ~DEV_DMA_BUFF_STS_MASK;
	desc->status |= (DEV_DMA_BUFF_STS_HREADY << DEV_DMA_BUFF_STS_SHIFT);

	 
	if (hs_ep->dir_in)
		dwc2_gadget_incr_frame_num(hs_ep);

	 
	hs_ep->next_desc++;
	if (hs_ep->next_desc >= MAX_DMA_DESC_NUM_HS_ISOC)
		hs_ep->next_desc = 0;

	return 0;
}

 
static void dwc2_gadget_start_isoc_ddma(struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	struct dwc2_hsotg_req *hs_req, *treq;
	int index = hs_ep->index;
	int ret;
	int i;
	u32 dma_reg;
	u32 depctl;
	u32 ctrl;
	struct dwc2_dma_desc *desc;

	if (list_empty(&hs_ep->queue)) {
		hs_ep->target_frame = TARGET_FRAME_INITIAL;
		dev_dbg(hsotg->dev, "%s: No requests in queue\n", __func__);
		return;
	}

	 
	for (i = 0; i < MAX_DMA_DESC_NUM_HS_ISOC; i++) {
		desc = &hs_ep->desc_list[i];
		desc->status = 0;
		desc->status |= (DEV_DMA_BUFF_STS_HBUSY
				    << DEV_DMA_BUFF_STS_SHIFT);
	}

	hs_ep->next_desc = 0;
	list_for_each_entry_safe(hs_req, treq, &hs_ep->queue, queue) {
		dma_addr_t dma_addr = hs_req->req.dma;

		if (hs_req->req.num_sgs) {
			WARN_ON(hs_req->req.num_sgs > 1);
			dma_addr = sg_dma_address(hs_req->req.sg);
		}
		ret = dwc2_gadget_fill_isoc_desc(hs_ep, dma_addr,
						 hs_req->req.length);
		if (ret)
			break;
	}

	hs_ep->compl_desc = 0;
	depctl = hs_ep->dir_in ? DIEPCTL(index) : DOEPCTL(index);
	dma_reg = hs_ep->dir_in ? DIEPDMA(index) : DOEPDMA(index);

	 
	dwc2_writel(hsotg, hs_ep->desc_list_dma, dma_reg);

	ctrl = dwc2_readl(hsotg, depctl);
	ctrl |= DXEPCTL_EPENA | DXEPCTL_CNAK;
	dwc2_writel(hsotg, ctrl, depctl);
}

static bool dwc2_gadget_target_frame_elapsed(struct dwc2_hsotg_ep *hs_ep);
static void dwc2_hsotg_complete_request(struct dwc2_hsotg *hsotg,
					struct dwc2_hsotg_ep *hs_ep,
				       struct dwc2_hsotg_req *hs_req,
				       int result);

 
static void dwc2_hsotg_start_req(struct dwc2_hsotg *hsotg,
				 struct dwc2_hsotg_ep *hs_ep,
				struct dwc2_hsotg_req *hs_req,
				bool continuing)
{
	struct usb_request *ureq = &hs_req->req;
	int index = hs_ep->index;
	int dir_in = hs_ep->dir_in;
	u32 epctrl_reg;
	u32 epsize_reg;
	u32 epsize;
	u32 ctrl;
	unsigned int length;
	unsigned int packets;
	unsigned int maxreq;
	unsigned int dma_reg;

	if (index != 0) {
		if (hs_ep->req && !continuing) {
			dev_err(hsotg->dev, "%s: active request\n", __func__);
			WARN_ON(1);
			return;
		} else if (hs_ep->req != hs_req && continuing) {
			dev_err(hsotg->dev,
				"%s: continue different req\n", __func__);
			WARN_ON(1);
			return;
		}
	}

	dma_reg = dir_in ? DIEPDMA(index) : DOEPDMA(index);
	epctrl_reg = dir_in ? DIEPCTL(index) : DOEPCTL(index);
	epsize_reg = dir_in ? DIEPTSIZ(index) : DOEPTSIZ(index);

	dev_dbg(hsotg->dev, "%s: DxEPCTL=0x%08x, ep %d, dir %s\n",
		__func__, dwc2_readl(hsotg, epctrl_reg), index,
		hs_ep->dir_in ? "in" : "out");

	 
	ctrl = dwc2_readl(hsotg, epctrl_reg);

	if (index && ctrl & DXEPCTL_STALL) {
		dev_warn(hsotg->dev, "%s: ep%d is stalled\n", __func__, index);
		return;
	}

	length = ureq->length - ureq->actual;
	dev_dbg(hsotg->dev, "ureq->length:%d ureq->actual:%d\n",
		ureq->length, ureq->actual);

	if (!using_desc_dma(hsotg))
		maxreq = get_ep_limit(hs_ep);
	else
		maxreq = dwc2_gadget_get_chain_limit(hs_ep);

	if (length > maxreq) {
		int round = maxreq % hs_ep->ep.maxpacket;

		dev_dbg(hsotg->dev, "%s: length %d, max-req %d, r %d\n",
			__func__, length, maxreq, round);

		 
		if (round)
			maxreq -= round;

		length = maxreq;
	}

	if (length)
		packets = DIV_ROUND_UP(length, hs_ep->ep.maxpacket);
	else
		packets = 1;	 

	if (dir_in && index != 0)
		if (hs_ep->isochronous)
			epsize = DXEPTSIZ_MC(packets);
		else
			epsize = DXEPTSIZ_MC(1);
	else
		epsize = 0;

	 
	if (dir_in && ureq->zero && !continuing) {
		 
		if ((ureq->length >= hs_ep->ep.maxpacket) &&
		    !(ureq->length % hs_ep->ep.maxpacket))
			hs_ep->send_zlp = 1;
	}

	epsize |= DXEPTSIZ_PKTCNT(packets);
	epsize |= DXEPTSIZ_XFERSIZE(length);

	dev_dbg(hsotg->dev, "%s: %d@%d/%d, 0x%08x => 0x%08x\n",
		__func__, packets, length, ureq->length, epsize, epsize_reg);

	 
	hs_ep->req = hs_req;

	if (using_desc_dma(hsotg)) {
		u32 offset = 0;
		u32 mps = hs_ep->ep.maxpacket;

		 
		if (!dir_in) {
			if (!index)
				length = mps;
			else if (length % mps)
				length += (mps - (length % mps));
		}

		if (continuing)
			offset = ureq->actual;

		 
		dwc2_gadget_config_nonisoc_xfer_ddma(hs_ep, ureq->dma + offset,
						     length);

		 
		dwc2_writel(hsotg, hs_ep->desc_list_dma, dma_reg);

		dev_dbg(hsotg->dev, "%s: %08x pad => 0x%08x\n",
			__func__, (u32)hs_ep->desc_list_dma, dma_reg);
	} else {
		 
		dwc2_writel(hsotg, epsize, epsize_reg);

		if (using_dma(hsotg) && !continuing && (length != 0)) {
			 

			dwc2_writel(hsotg, ureq->dma, dma_reg);

			dev_dbg(hsotg->dev, "%s: %pad => 0x%08x\n",
				__func__, &ureq->dma, dma_reg);
		}
	}

	if (hs_ep->isochronous) {
		if (!dwc2_gadget_target_frame_elapsed(hs_ep)) {
			if (hs_ep->interval == 1) {
				if (hs_ep->target_frame & 0x1)
					ctrl |= DXEPCTL_SETODDFR;
				else
					ctrl |= DXEPCTL_SETEVENFR;
			}
			ctrl |= DXEPCTL_CNAK;
		} else {
			hs_req->req.frame_number = hs_ep->target_frame;
			hs_req->req.actual = 0;
			dwc2_hsotg_complete_request(hsotg, hs_ep, hs_req, -ENODATA);
			return;
		}
	}

	ctrl |= DXEPCTL_EPENA;	 

	dev_dbg(hsotg->dev, "ep0 state:%d\n", hsotg->ep0_state);

	 
	if (!(index == 0 && hsotg->ep0_state == DWC2_EP0_SETUP))
		ctrl |= DXEPCTL_CNAK;	 

	dev_dbg(hsotg->dev, "%s: DxEPCTL=0x%08x\n", __func__, ctrl);
	dwc2_writel(hsotg, ctrl, epctrl_reg);

	 
	hs_ep->size_loaded = length;
	hs_ep->last_load = ureq->actual;

	if (dir_in && !using_dma(hsotg)) {
		 
		hs_ep->fifo_load = 0;

		dwc2_hsotg_write_fifo(hsotg, hs_ep, hs_req);
	}

	 

	 
	if (!(dwc2_readl(hsotg, epctrl_reg) & DXEPCTL_EPENA))
		dev_dbg(hsotg->dev,
			"ep%d: failed to become enabled (DXEPCTL=0x%08x)?\n",
			 index, dwc2_readl(hsotg, epctrl_reg));

	dev_dbg(hsotg->dev, "%s: DXEPCTL=0x%08x\n",
		__func__, dwc2_readl(hsotg, epctrl_reg));

	 
	dwc2_hsotg_ctrl_epint(hsotg, hs_ep->index, hs_ep->dir_in, 1);
}

 
static int dwc2_hsotg_map_dma(struct dwc2_hsotg *hsotg,
			      struct dwc2_hsotg_ep *hs_ep,
			     struct usb_request *req)
{
	int ret;

	hs_ep->map_dir = hs_ep->dir_in;
	ret = usb_gadget_map_request(&hsotg->gadget, req, hs_ep->dir_in);
	if (ret)
		goto dma_error;

	return 0;

dma_error:
	dev_err(hsotg->dev, "%s: failed to map buffer %p, %d bytes\n",
		__func__, req->buf, req->length);

	return -EIO;
}

static int dwc2_hsotg_handle_unaligned_buf_start(struct dwc2_hsotg *hsotg,
						 struct dwc2_hsotg_ep *hs_ep,
						 struct dwc2_hsotg_req *hs_req)
{
	void *req_buf = hs_req->req.buf;

	 
	if (!using_dma(hsotg) || !((long)req_buf & 3))
		return 0;

	WARN_ON(hs_req->saved_req_buf);

	dev_dbg(hsotg->dev, "%s: %s: buf=%p length=%d\n", __func__,
		hs_ep->ep.name, req_buf, hs_req->req.length);

	hs_req->req.buf = kmalloc(hs_req->req.length, GFP_ATOMIC);
	if (!hs_req->req.buf) {
		hs_req->req.buf = req_buf;
		dev_err(hsotg->dev,
			"%s: unable to allocate memory for bounce buffer\n",
			__func__);
		return -ENOMEM;
	}

	 
	hs_req->saved_req_buf = req_buf;

	if (hs_ep->dir_in)
		memcpy(hs_req->req.buf, req_buf, hs_req->req.length);
	return 0;
}

static void
dwc2_hsotg_handle_unaligned_buf_complete(struct dwc2_hsotg *hsotg,
					 struct dwc2_hsotg_ep *hs_ep,
					 struct dwc2_hsotg_req *hs_req)
{
	 
	if (!using_dma(hsotg) || !hs_req->saved_req_buf)
		return;

	dev_dbg(hsotg->dev, "%s: %s: status=%d actual-length=%d\n", __func__,
		hs_ep->ep.name, hs_req->req.status, hs_req->req.actual);

	 
	if (!hs_ep->dir_in && !hs_req->req.status)
		memcpy(hs_req->saved_req_buf, hs_req->req.buf,
		       hs_req->req.actual);

	 
	kfree(hs_req->req.buf);

	hs_req->req.buf = hs_req->saved_req_buf;
	hs_req->saved_req_buf = NULL;
}

 
static bool dwc2_gadget_target_frame_elapsed(struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	u32 target_frame = hs_ep->target_frame;
	u32 current_frame = hsotg->frame_number;
	bool frame_overrun = hs_ep->frame_overrun;
	u16 limit = DSTS_SOFFN_LIMIT;

	if (hsotg->gadget.speed != USB_SPEED_HIGH)
		limit >>= 3;

	if (!frame_overrun && current_frame >= target_frame)
		return true;

	if (frame_overrun && current_frame >= target_frame &&
	    ((current_frame - target_frame) < limit / 2))
		return true;

	return false;
}

 
static int dwc2_gadget_set_ep0_desc_chain(struct dwc2_hsotg *hsotg,
					  struct dwc2_hsotg_ep *hs_ep)
{
	switch (hsotg->ep0_state) {
	case DWC2_EP0_SETUP:
	case DWC2_EP0_STATUS_OUT:
		hs_ep->desc_list = hsotg->setup_desc[0];
		hs_ep->desc_list_dma = hsotg->setup_desc_dma[0];
		break;
	case DWC2_EP0_DATA_IN:
	case DWC2_EP0_STATUS_IN:
		hs_ep->desc_list = hsotg->ctrl_in_desc;
		hs_ep->desc_list_dma = hsotg->ctrl_in_desc_dma;
		break;
	case DWC2_EP0_DATA_OUT:
		hs_ep->desc_list = hsotg->ctrl_out_desc;
		hs_ep->desc_list_dma = hsotg->ctrl_out_desc_dma;
		break;
	default:
		dev_err(hsotg->dev, "invalid EP 0 state in queue %d\n",
			hsotg->ep0_state);
		return -EINVAL;
	}

	return 0;
}

static int dwc2_hsotg_ep_queue(struct usb_ep *ep, struct usb_request *req,
			       gfp_t gfp_flags)
{
	struct dwc2_hsotg_req *hs_req = our_req(req);
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hs = hs_ep->parent;
	bool first;
	int ret;
	u32 maxsize = 0;
	u32 mask = 0;


	dev_dbg(hs->dev, "%s: req %p: %d@%p, noi=%d, zero=%d, snok=%d\n",
		ep->name, req, req->length, req->buf, req->no_interrupt,
		req->zero, req->short_not_ok);

	 
	if (hs->lx_state != DWC2_L0) {
		dev_dbg(hs->dev, "%s: submit request only in active state\n",
			__func__);
		return -EAGAIN;
	}

	 
	INIT_LIST_HEAD(&hs_req->queue);
	req->actual = 0;
	req->status = -EINPROGRESS;

	 
	if (hs_ep->isochronous &&
	    req->length > (hs_ep->mc * hs_ep->ep.maxpacket)) {
		dev_err(hs->dev, "req length > maxpacket*mc\n");
		return -EINVAL;
	}

	 
	if (using_desc_dma(hs) && hs_ep->isochronous) {
		maxsize = dwc2_gadget_get_desc_params(hs_ep, &mask);
		if (hs_ep->dir_in && req->length > maxsize) {
			dev_err(hs->dev, "wrong length %d (maxsize=%d)\n",
				req->length, maxsize);
			return -EINVAL;
		}

		if (!hs_ep->dir_in && req->length > hs_ep->ep.maxpacket) {
			dev_err(hs->dev, "ISOC OUT: wrong length %d (mps=%d)\n",
				req->length, hs_ep->ep.maxpacket);
			return -EINVAL;
		}
	}

	ret = dwc2_hsotg_handle_unaligned_buf_start(hs, hs_ep, hs_req);
	if (ret)
		return ret;

	 
	if (using_dma(hs)) {
		ret = dwc2_hsotg_map_dma(hs, hs_ep, req);
		if (ret)
			return ret;
	}
	 
	if (using_desc_dma(hs) && !hs_ep->index) {
		ret = dwc2_gadget_set_ep0_desc_chain(hs, hs_ep);
		if (ret)
			return ret;
	}

	first = list_empty(&hs_ep->queue);
	list_add_tail(&hs_req->queue, &hs_ep->queue);

	 
	if (using_desc_dma(hs) && hs_ep->isochronous) {
		if (hs_ep->target_frame != TARGET_FRAME_INITIAL) {
			dma_addr_t dma_addr = hs_req->req.dma;

			if (hs_req->req.num_sgs) {
				WARN_ON(hs_req->req.num_sgs > 1);
				dma_addr = sg_dma_address(hs_req->req.sg);
			}
			dwc2_gadget_fill_isoc_desc(hs_ep, dma_addr,
						   hs_req->req.length);
		}
		return 0;
	}

	 
	if (!hs_ep->index && !req->length && !hs_ep->dir_in &&
	    hs->ep0_state == DWC2_EP0_DATA_OUT)
		hs_ep->dir_in = 1;

	if (first) {
		if (!hs_ep->isochronous) {
			dwc2_hsotg_start_req(hs, hs_ep, hs_req, false);
			return 0;
		}

		 
		hs->frame_number = dwc2_hsotg_read_frameno(hs);
		while (dwc2_gadget_target_frame_elapsed(hs_ep)) {
			dwc2_gadget_incr_frame_num(hs_ep);
			 
			hs->frame_number = dwc2_hsotg_read_frameno(hs);
		}

		if (hs_ep->target_frame != TARGET_FRAME_INITIAL)
			dwc2_hsotg_start_req(hs, hs_ep, hs_req, false);
	}
	return 0;
}

static int dwc2_hsotg_ep_queue_lock(struct usb_ep *ep, struct usb_request *req,
				    gfp_t gfp_flags)
{
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hs = hs_ep->parent;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&hs->lock, flags);
	ret = dwc2_hsotg_ep_queue(ep, req, gfp_flags);
	spin_unlock_irqrestore(&hs->lock, flags);

	return ret;
}

static void dwc2_hsotg_ep_free_request(struct usb_ep *ep,
				       struct usb_request *req)
{
	struct dwc2_hsotg_req *hs_req = our_req(req);

	kfree(hs_req);
}

 
static void dwc2_hsotg_complete_oursetup(struct usb_ep *ep,
					 struct usb_request *req)
{
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hsotg = hs_ep->parent;

	dev_dbg(hsotg->dev, "%s: ep %p, req %p\n", __func__, ep, req);

	dwc2_hsotg_ep_free_request(ep, req);
}

 
static struct dwc2_hsotg_ep *ep_from_windex(struct dwc2_hsotg *hsotg,
					    u32 windex)
{
	int dir = (windex & USB_DIR_IN) ? 1 : 0;
	int idx = windex & 0x7F;

	if (windex >= 0x100)
		return NULL;

	if (idx > hsotg->num_of_eps)
		return NULL;

	return index_to_ep(hsotg, idx, dir);
}

 
int dwc2_hsotg_set_test_mode(struct dwc2_hsotg *hsotg, int testmode)
{
	int dctl = dwc2_readl(hsotg, DCTL);

	dctl &= ~DCTL_TSTCTL_MASK;
	switch (testmode) {
	case USB_TEST_J:
	case USB_TEST_K:
	case USB_TEST_SE0_NAK:
	case USB_TEST_PACKET:
	case USB_TEST_FORCE_ENABLE:
		dctl |= testmode << DCTL_TSTCTL_SHIFT;
		break;
	default:
		return -EINVAL;
	}
	dwc2_writel(hsotg, dctl, DCTL);
	return 0;
}

 
static int dwc2_hsotg_send_reply(struct dwc2_hsotg *hsotg,
				 struct dwc2_hsotg_ep *ep,
				void *buff,
				int length)
{
	struct usb_request *req;
	int ret;

	dev_dbg(hsotg->dev, "%s: buff %p, len %d\n", __func__, buff, length);

	req = dwc2_hsotg_ep_alloc_request(&ep->ep, GFP_ATOMIC);
	hsotg->ep0_reply = req;
	if (!req) {
		dev_warn(hsotg->dev, "%s: cannot alloc req\n", __func__);
		return -ENOMEM;
	}

	req->buf = hsotg->ep0_buff;
	req->length = length;
	 
	req->zero = 0;
	req->complete = dwc2_hsotg_complete_oursetup;

	if (length)
		memcpy(req->buf, buff, length);

	ret = dwc2_hsotg_ep_queue(&ep->ep, req, GFP_ATOMIC);
	if (ret) {
		dev_warn(hsotg->dev, "%s: cannot queue req\n", __func__);
		return ret;
	}

	return 0;
}

 
static int dwc2_hsotg_process_req_status(struct dwc2_hsotg *hsotg,
					 struct usb_ctrlrequest *ctrl)
{
	struct dwc2_hsotg_ep *ep0 = hsotg->eps_out[0];
	struct dwc2_hsotg_ep *ep;
	__le16 reply;
	u16 status;
	int ret;

	dev_dbg(hsotg->dev, "%s: USB_REQ_GET_STATUS\n", __func__);

	if (!ep0->dir_in) {
		dev_warn(hsotg->dev, "%s: direction out?\n", __func__);
		return -EINVAL;
	}

	switch (ctrl->bRequestType & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		status = hsotg->gadget.is_selfpowered <<
			 USB_DEVICE_SELF_POWERED;
		status |= hsotg->remote_wakeup_allowed <<
			  USB_DEVICE_REMOTE_WAKEUP;
		reply = cpu_to_le16(status);
		break;

	case USB_RECIP_INTERFACE:
		 
		reply = cpu_to_le16(0);
		break;

	case USB_RECIP_ENDPOINT:
		ep = ep_from_windex(hsotg, le16_to_cpu(ctrl->wIndex));
		if (!ep)
			return -ENOENT;

		reply = cpu_to_le16(ep->halted ? 1 : 0);
		break;

	default:
		return 0;
	}

	if (le16_to_cpu(ctrl->wLength) != 2)
		return -EINVAL;

	ret = dwc2_hsotg_send_reply(hsotg, ep0, &reply, 2);
	if (ret) {
		dev_err(hsotg->dev, "%s: failed to send reply\n", __func__);
		return ret;
	}

	return 1;
}

static int dwc2_hsotg_ep_sethalt(struct usb_ep *ep, int value, bool now);

 
static struct dwc2_hsotg_req *get_ep_head(struct dwc2_hsotg_ep *hs_ep)
{
	return list_first_entry_or_null(&hs_ep->queue, struct dwc2_hsotg_req,
					queue);
}

 
static void dwc2_gadget_start_next_request(struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	int dir_in = hs_ep->dir_in;
	struct dwc2_hsotg_req *hs_req;

	if (!list_empty(&hs_ep->queue)) {
		hs_req = get_ep_head(hs_ep);
		dwc2_hsotg_start_req(hsotg, hs_ep, hs_req, false);
		return;
	}
	if (!hs_ep->isochronous)
		return;

	if (dir_in) {
		dev_dbg(hsotg->dev, "%s: No more ISOC-IN requests\n",
			__func__);
	} else {
		dev_dbg(hsotg->dev, "%s: No more ISOC-OUT requests\n",
			__func__);
	}
}

 
static int dwc2_hsotg_process_req_feature(struct dwc2_hsotg *hsotg,
					  struct usb_ctrlrequest *ctrl)
{
	struct dwc2_hsotg_ep *ep0 = hsotg->eps_out[0];
	struct dwc2_hsotg_req *hs_req;
	bool set = (ctrl->bRequest == USB_REQ_SET_FEATURE);
	struct dwc2_hsotg_ep *ep;
	int ret;
	bool halted;
	u32 recip;
	u32 wValue;
	u32 wIndex;

	dev_dbg(hsotg->dev, "%s: %s_FEATURE\n",
		__func__, set ? "SET" : "CLEAR");

	wValue = le16_to_cpu(ctrl->wValue);
	wIndex = le16_to_cpu(ctrl->wIndex);
	recip = ctrl->bRequestType & USB_RECIP_MASK;

	switch (recip) {
	case USB_RECIP_DEVICE:
		switch (wValue) {
		case USB_DEVICE_REMOTE_WAKEUP:
			if (set)
				hsotg->remote_wakeup_allowed = 1;
			else
				hsotg->remote_wakeup_allowed = 0;
			break;

		case USB_DEVICE_TEST_MODE:
			if ((wIndex & 0xff) != 0)
				return -EINVAL;
			if (!set)
				return -EINVAL;

			hsotg->test_mode = wIndex >> 8;
			break;
		default:
			return -ENOENT;
		}

		ret = dwc2_hsotg_send_reply(hsotg, ep0, NULL, 0);
		if (ret) {
			dev_err(hsotg->dev,
				"%s: failed to send reply\n", __func__);
			return ret;
		}
		break;

	case USB_RECIP_ENDPOINT:
		ep = ep_from_windex(hsotg, wIndex);
		if (!ep) {
			dev_dbg(hsotg->dev, "%s: no endpoint for 0x%04x\n",
				__func__, wIndex);
			return -ENOENT;
		}

		switch (wValue) {
		case USB_ENDPOINT_HALT:
			halted = ep->halted;

			if (!ep->wedged)
				dwc2_hsotg_ep_sethalt(&ep->ep, set, true);

			ret = dwc2_hsotg_send_reply(hsotg, ep0, NULL, 0);
			if (ret) {
				dev_err(hsotg->dev,
					"%s: failed to send reply\n", __func__);
				return ret;
			}

			 

			if (!set && halted) {
				 
				if (ep->req) {
					hs_req = ep->req;
					ep->req = NULL;
					list_del_init(&hs_req->queue);
					if (hs_req->req.complete) {
						spin_unlock(&hsotg->lock);
						usb_gadget_giveback_request(
							&ep->ep, &hs_req->req);
						spin_lock(&hsotg->lock);
					}
				}

				 
				if (!ep->req)
					dwc2_gadget_start_next_request(ep);
			}

			break;

		default:
			return -ENOENT;
		}
		break;
	default:
		return -ENOENT;
	}
	return 1;
}

static void dwc2_hsotg_enqueue_setup(struct dwc2_hsotg *hsotg);

 
static void dwc2_hsotg_stall_ep0(struct dwc2_hsotg *hsotg)
{
	struct dwc2_hsotg_ep *ep0 = hsotg->eps_out[0];
	u32 reg;
	u32 ctrl;

	dev_dbg(hsotg->dev, "ep0 stall (dir=%d)\n", ep0->dir_in);
	reg = (ep0->dir_in) ? DIEPCTL0 : DOEPCTL0;

	 

	ctrl = dwc2_readl(hsotg, reg);
	ctrl |= DXEPCTL_STALL;
	ctrl |= DXEPCTL_CNAK;
	dwc2_writel(hsotg, ctrl, reg);

	dev_dbg(hsotg->dev,
		"written DXEPCTL=0x%08x to %08x (DXEPCTL=0x%08x)\n",
		ctrl, reg, dwc2_readl(hsotg, reg));

	  
	 dwc2_hsotg_enqueue_setup(hsotg);
}

 
static void dwc2_hsotg_process_control(struct dwc2_hsotg *hsotg,
				       struct usb_ctrlrequest *ctrl)
{
	struct dwc2_hsotg_ep *ep0 = hsotg->eps_out[0];
	int ret = 0;
	u32 dcfg;

	dev_dbg(hsotg->dev,
		"ctrl Type=%02x, Req=%02x, V=%04x, I=%04x, L=%04x\n",
		ctrl->bRequestType, ctrl->bRequest, ctrl->wValue,
		ctrl->wIndex, ctrl->wLength);

	if (ctrl->wLength == 0) {
		ep0->dir_in = 1;
		hsotg->ep0_state = DWC2_EP0_STATUS_IN;
	} else if (ctrl->bRequestType & USB_DIR_IN) {
		ep0->dir_in = 1;
		hsotg->ep0_state = DWC2_EP0_DATA_IN;
	} else {
		ep0->dir_in = 0;
		hsotg->ep0_state = DWC2_EP0_DATA_OUT;
	}

	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
		switch (ctrl->bRequest) {
		case USB_REQ_SET_ADDRESS:
			hsotg->connected = 1;
			dcfg = dwc2_readl(hsotg, DCFG);
			dcfg &= ~DCFG_DEVADDR_MASK;
			dcfg |= (le16_to_cpu(ctrl->wValue) <<
				 DCFG_DEVADDR_SHIFT) & DCFG_DEVADDR_MASK;
			dwc2_writel(hsotg, dcfg, DCFG);

			dev_info(hsotg->dev, "new address %d\n", ctrl->wValue);

			ret = dwc2_hsotg_send_reply(hsotg, ep0, NULL, 0);
			return;

		case USB_REQ_GET_STATUS:
			ret = dwc2_hsotg_process_req_status(hsotg, ctrl);
			break;

		case USB_REQ_CLEAR_FEATURE:
		case USB_REQ_SET_FEATURE:
			ret = dwc2_hsotg_process_req_feature(hsotg, ctrl);
			break;
		}
	}

	 

	if (ret == 0 && hsotg->driver) {
		spin_unlock(&hsotg->lock);
		ret = hsotg->driver->setup(&hsotg->gadget, ctrl);
		spin_lock(&hsotg->lock);
		if (ret < 0)
			dev_dbg(hsotg->dev, "driver->setup() ret %d\n", ret);
	}

	hsotg->delayed_status = false;
	if (ret == USB_GADGET_DELAYED_STATUS)
		hsotg->delayed_status = true;

	 

	if (ret < 0)
		dwc2_hsotg_stall_ep0(hsotg);
}

 
static void dwc2_hsotg_complete_setup(struct usb_ep *ep,
				      struct usb_request *req)
{
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hsotg = hs_ep->parent;

	if (req->status < 0) {
		dev_dbg(hsotg->dev, "%s: failed %d\n", __func__, req->status);
		return;
	}

	spin_lock(&hsotg->lock);
	if (req->actual == 0)
		dwc2_hsotg_enqueue_setup(hsotg);
	else
		dwc2_hsotg_process_control(hsotg, req->buf);
	spin_unlock(&hsotg->lock);
}

 
static void dwc2_hsotg_enqueue_setup(struct dwc2_hsotg *hsotg)
{
	struct usb_request *req = hsotg->ctrl_req;
	struct dwc2_hsotg_req *hs_req = our_req(req);
	int ret;

	dev_dbg(hsotg->dev, "%s: queueing setup request\n", __func__);

	req->zero = 0;
	req->length = 8;
	req->buf = hsotg->ctrl_buff;
	req->complete = dwc2_hsotg_complete_setup;

	if (!list_empty(&hs_req->queue)) {
		dev_dbg(hsotg->dev, "%s already queued???\n", __func__);
		return;
	}

	hsotg->eps_out[0]->dir_in = 0;
	hsotg->eps_out[0]->send_zlp = 0;
	hsotg->ep0_state = DWC2_EP0_SETUP;

	ret = dwc2_hsotg_ep_queue(&hsotg->eps_out[0]->ep, req, GFP_ATOMIC);
	if (ret < 0) {
		dev_err(hsotg->dev, "%s: failed queue (%d)\n", __func__, ret);
		 
	}
}

static void dwc2_hsotg_program_zlp(struct dwc2_hsotg *hsotg,
				   struct dwc2_hsotg_ep *hs_ep)
{
	u32 ctrl;
	u8 index = hs_ep->index;
	u32 epctl_reg = hs_ep->dir_in ? DIEPCTL(index) : DOEPCTL(index);
	u32 epsiz_reg = hs_ep->dir_in ? DIEPTSIZ(index) : DOEPTSIZ(index);

	if (hs_ep->dir_in)
		dev_dbg(hsotg->dev, "Sending zero-length packet on ep%d\n",
			index);
	else
		dev_dbg(hsotg->dev, "Receiving zero-length packet on ep%d\n",
			index);
	if (using_desc_dma(hsotg)) {
		 
		dma_addr_t dma = hs_ep->desc_list_dma;

		if (!index)
			dwc2_gadget_set_ep0_desc_chain(hsotg, hs_ep);

		dwc2_gadget_config_nonisoc_xfer_ddma(hs_ep, dma, 0);
	} else {
		dwc2_writel(hsotg, DXEPTSIZ_MC(1) | DXEPTSIZ_PKTCNT(1) |
			    DXEPTSIZ_XFERSIZE(0),
			    epsiz_reg);
	}

	ctrl = dwc2_readl(hsotg, epctl_reg);
	ctrl |= DXEPCTL_CNAK;   
	ctrl |= DXEPCTL_EPENA;  
	ctrl |= DXEPCTL_USBACTEP;
	dwc2_writel(hsotg, ctrl, epctl_reg);
}

 
static void dwc2_hsotg_complete_request(struct dwc2_hsotg *hsotg,
					struct dwc2_hsotg_ep *hs_ep,
				       struct dwc2_hsotg_req *hs_req,
				       int result)
{
	if (!hs_req) {
		dev_dbg(hsotg->dev, "%s: nothing to complete?\n", __func__);
		return;
	}

	dev_dbg(hsotg->dev, "complete: ep %p %s, req %p, %d => %p\n",
		hs_ep, hs_ep->ep.name, hs_req, result, hs_req->req.complete);

	 

	if (hs_req->req.status == -EINPROGRESS)
		hs_req->req.status = result;

	if (using_dma(hsotg))
		dwc2_hsotg_unmap_dma(hsotg, hs_ep, hs_req);

	dwc2_hsotg_handle_unaligned_buf_complete(hsotg, hs_ep, hs_req);

	hs_ep->req = NULL;
	list_del_init(&hs_req->queue);

	 

	if (hs_req->req.complete) {
		spin_unlock(&hsotg->lock);
		usb_gadget_giveback_request(&hs_ep->ep, &hs_req->req);
		spin_lock(&hsotg->lock);
	}

	 
	if (using_desc_dma(hsotg) && hs_ep->isochronous)
		return;

	 

	if (!hs_ep->req && result >= 0)
		dwc2_gadget_start_next_request(hs_ep);
}

 
static void dwc2_gadget_complete_isoc_request_ddma(struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	struct dwc2_hsotg_req *hs_req;
	struct usb_request *ureq;
	u32 desc_sts;
	u32 mask;

	desc_sts = hs_ep->desc_list[hs_ep->compl_desc].status;

	 
	while ((desc_sts & DEV_DMA_BUFF_STS_MASK) >>
		DEV_DMA_BUFF_STS_SHIFT == DEV_DMA_BUFF_STS_DMADONE) {

		hs_req = get_ep_head(hs_ep);
		if (!hs_req) {
			dev_warn(hsotg->dev, "%s: ISOC EP queue empty\n", __func__);
			return;
		}
		ureq = &hs_req->req;

		 
		if ((desc_sts & DEV_DMA_STS_MASK) >> DEV_DMA_STS_SHIFT ==
			DEV_DMA_STS_SUCC) {
			mask = hs_ep->dir_in ? DEV_DMA_ISOC_TX_NBYTES_MASK :
				DEV_DMA_ISOC_RX_NBYTES_MASK;
			ureq->actual = ureq->length - ((desc_sts & mask) >>
				DEV_DMA_ISOC_NBYTES_SHIFT);

			 
			if (!hs_ep->dir_in && ureq->length & 0x3)
				ureq->actual += 4 - (ureq->length & 0x3);

			 
			ureq->frame_number =
				(desc_sts & DEV_DMA_ISOC_FRNUM_MASK) >>
				DEV_DMA_ISOC_FRNUM_SHIFT;
		}

		dwc2_hsotg_complete_request(hsotg, hs_ep, hs_req, 0);

		hs_ep->compl_desc++;
		if (hs_ep->compl_desc > (MAX_DMA_DESC_NUM_HS_ISOC - 1))
			hs_ep->compl_desc = 0;
		desc_sts = hs_ep->desc_list[hs_ep->compl_desc].status;
	}
}

 
static void dwc2_gadget_handle_isoc_bna(struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg *hsotg = hs_ep->parent;

	if (!hs_ep->dir_in)
		dwc2_flush_rx_fifo(hsotg);
	dwc2_hsotg_complete_request(hsotg, hs_ep, get_ep_head(hs_ep), 0);

	hs_ep->target_frame = TARGET_FRAME_INITIAL;
	hs_ep->next_desc = 0;
	hs_ep->compl_desc = 0;
}

 
static void dwc2_hsotg_rx_data(struct dwc2_hsotg *hsotg, int ep_idx, int size)
{
	struct dwc2_hsotg_ep *hs_ep = hsotg->eps_out[ep_idx];
	struct dwc2_hsotg_req *hs_req = hs_ep->req;
	int to_read;
	int max_req;
	int read_ptr;

	if (!hs_req) {
		u32 epctl = dwc2_readl(hsotg, DOEPCTL(ep_idx));
		int ptr;

		dev_dbg(hsotg->dev,
			"%s: FIFO %d bytes on ep%d but no req (DXEPCTl=0x%08x)\n",
			 __func__, size, ep_idx, epctl);

		 
		for (ptr = 0; ptr < size; ptr += 4)
			(void)dwc2_readl(hsotg, EPFIFO(ep_idx));

		return;
	}

	to_read = size;
	read_ptr = hs_req->req.actual;
	max_req = hs_req->req.length - read_ptr;

	dev_dbg(hsotg->dev, "%s: read %d/%d, done %d/%d\n",
		__func__, to_read, max_req, read_ptr, hs_req->req.length);

	if (to_read > max_req) {
		 

		 
		WARN_ON_ONCE(1);
	}

	hs_ep->total_data += to_read;
	hs_req->req.actual += to_read;
	to_read = DIV_ROUND_UP(to_read, 4);

	 
	dwc2_readl_rep(hsotg, EPFIFO(ep_idx),
		       hs_req->req.buf + read_ptr, to_read);
}

 
static void dwc2_hsotg_ep0_zlp(struct dwc2_hsotg *hsotg, bool dir_in)
{
	 
	hsotg->eps_out[0]->dir_in = dir_in;
	hsotg->ep0_state = dir_in ? DWC2_EP0_STATUS_IN : DWC2_EP0_STATUS_OUT;

	dwc2_hsotg_program_zlp(hsotg, hsotg->eps_out[0]);
}

 
static unsigned int dwc2_gadget_get_xfersize_ddma(struct dwc2_hsotg_ep *hs_ep)
{
	const struct usb_endpoint_descriptor *ep_desc = hs_ep->ep.desc;
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	unsigned int bytes_rem = 0;
	unsigned int bytes_rem_correction = 0;
	struct dwc2_dma_desc *desc = hs_ep->desc_list;
	int i;
	u32 status;
	u32 mps = hs_ep->ep.maxpacket;
	int dir_in = hs_ep->dir_in;

	if (!desc)
		return -EINVAL;

	 
	if (hs_ep->index)
		if (usb_endpoint_xfer_int(ep_desc) && !dir_in && (mps % 4))
			bytes_rem_correction = 4 - (mps % 4);

	for (i = 0; i < hs_ep->desc_count; ++i) {
		status = desc->status;
		bytes_rem += status & DEV_DMA_NBYTES_MASK;
		bytes_rem -= bytes_rem_correction;

		if (status & DEV_DMA_STS_MASK)
			dev_err(hsotg->dev, "descriptor %d closed with %x\n",
				i, status & DEV_DMA_STS_MASK);

		if (status & DEV_DMA_L)
			break;

		desc++;
	}

	return bytes_rem;
}

 
static void dwc2_hsotg_handle_outdone(struct dwc2_hsotg *hsotg, int epnum)
{
	u32 epsize = dwc2_readl(hsotg, DOEPTSIZ(epnum));
	struct dwc2_hsotg_ep *hs_ep = hsotg->eps_out[epnum];
	struct dwc2_hsotg_req *hs_req = hs_ep->req;
	struct usb_request *req = &hs_req->req;
	unsigned int size_left = DXEPTSIZ_XFERSIZE_GET(epsize);
	int result = 0;

	if (!hs_req) {
		dev_dbg(hsotg->dev, "%s: no request active\n", __func__);
		return;
	}

	if (epnum == 0 && hsotg->ep0_state == DWC2_EP0_STATUS_OUT) {
		dev_dbg(hsotg->dev, "zlp packet received\n");
		dwc2_hsotg_complete_request(hsotg, hs_ep, hs_req, 0);
		dwc2_hsotg_enqueue_setup(hsotg);
		return;
	}

	if (using_desc_dma(hsotg))
		size_left = dwc2_gadget_get_xfersize_ddma(hs_ep);

	if (using_dma(hsotg)) {
		unsigned int size_done;

		 

		size_done = hs_ep->size_loaded - size_left;
		size_done += hs_ep->last_load;

		req->actual = size_done;
	}

	 
	if (req->actual < req->length && size_left == 0) {
		dwc2_hsotg_start_req(hsotg, hs_ep, hs_req, true);
		return;
	}

	if (req->actual < req->length && req->short_not_ok) {
		dev_dbg(hsotg->dev, "%s: got %d/%d (short not ok) => error\n",
			__func__, req->actual, req->length);

		 
	}

	 
	if (!using_desc_dma(hsotg) && epnum == 0 &&
	    hsotg->ep0_state == DWC2_EP0_DATA_OUT) {
		 
		if (!hsotg->delayed_status)
			dwc2_hsotg_ep0_zlp(hsotg, true);
	}

	 
	if (!using_desc_dma(hsotg) && hs_ep->isochronous) {
		req->frame_number = hs_ep->target_frame;
		dwc2_gadget_incr_frame_num(hs_ep);
	}

	dwc2_hsotg_complete_request(hsotg, hs_ep, hs_req, result);
}

 
static void dwc2_hsotg_handle_rx(struct dwc2_hsotg *hsotg)
{
	u32 grxstsr = dwc2_readl(hsotg, GRXSTSP);
	u32 epnum, status, size;

	WARN_ON(using_dma(hsotg));

	epnum = grxstsr & GRXSTS_EPNUM_MASK;
	status = grxstsr & GRXSTS_PKTSTS_MASK;

	size = grxstsr & GRXSTS_BYTECNT_MASK;
	size >>= GRXSTS_BYTECNT_SHIFT;

	dev_dbg(hsotg->dev, "%s: GRXSTSP=0x%08x (%d@%d)\n",
		__func__, grxstsr, size, epnum);

	switch ((status & GRXSTS_PKTSTS_MASK) >> GRXSTS_PKTSTS_SHIFT) {
	case GRXSTS_PKTSTS_GLOBALOUTNAK:
		dev_dbg(hsotg->dev, "GLOBALOUTNAK\n");
		break;

	case GRXSTS_PKTSTS_OUTDONE:
		dev_dbg(hsotg->dev, "OutDone (Frame=0x%08x)\n",
			dwc2_hsotg_read_frameno(hsotg));

		if (!using_dma(hsotg))
			dwc2_hsotg_handle_outdone(hsotg, epnum);
		break;

	case GRXSTS_PKTSTS_SETUPDONE:
		dev_dbg(hsotg->dev,
			"SetupDone (Frame=0x%08x, DOPEPCTL=0x%08x)\n",
			dwc2_hsotg_read_frameno(hsotg),
			dwc2_readl(hsotg, DOEPCTL(0)));
		 
		if (hsotg->ep0_state == DWC2_EP0_SETUP)
			dwc2_hsotg_handle_outdone(hsotg, epnum);
		break;

	case GRXSTS_PKTSTS_OUTRX:
		dwc2_hsotg_rx_data(hsotg, epnum, size);
		break;

	case GRXSTS_PKTSTS_SETUPRX:
		dev_dbg(hsotg->dev,
			"SetupRX (Frame=0x%08x, DOPEPCTL=0x%08x)\n",
			dwc2_hsotg_read_frameno(hsotg),
			dwc2_readl(hsotg, DOEPCTL(0)));

		WARN_ON(hsotg->ep0_state != DWC2_EP0_SETUP);

		dwc2_hsotg_rx_data(hsotg, epnum, size);
		break;

	default:
		dev_warn(hsotg->dev, "%s: unknown status %08x\n",
			 __func__, grxstsr);

		dwc2_hsotg_dump(hsotg);
		break;
	}
}

 
static u32 dwc2_hsotg_ep0_mps(unsigned int mps)
{
	switch (mps) {
	case 64:
		return D0EPCTL_MPS_64;
	case 32:
		return D0EPCTL_MPS_32;
	case 16:
		return D0EPCTL_MPS_16;
	case 8:
		return D0EPCTL_MPS_8;
	}

	 
	WARN_ON(1);
	return (u32)-1;
}

 
static void dwc2_hsotg_set_ep_maxpacket(struct dwc2_hsotg *hsotg,
					unsigned int ep, unsigned int mps,
					unsigned int mc, unsigned int dir_in)
{
	struct dwc2_hsotg_ep *hs_ep;
	u32 reg;

	hs_ep = index_to_ep(hsotg, ep, dir_in);
	if (!hs_ep)
		return;

	if (ep == 0) {
		u32 mps_bytes = mps;

		 
		mps = dwc2_hsotg_ep0_mps(mps_bytes);
		if (mps > 3)
			goto bad_mps;
		hs_ep->ep.maxpacket = mps_bytes;
		hs_ep->mc = 1;
	} else {
		if (mps > 1024)
			goto bad_mps;
		hs_ep->mc = mc;
		if (mc > 3)
			goto bad_mps;
		hs_ep->ep.maxpacket = mps;
	}

	if (dir_in) {
		reg = dwc2_readl(hsotg, DIEPCTL(ep));
		reg &= ~DXEPCTL_MPS_MASK;
		reg |= mps;
		dwc2_writel(hsotg, reg, DIEPCTL(ep));
	} else {
		reg = dwc2_readl(hsotg, DOEPCTL(ep));
		reg &= ~DXEPCTL_MPS_MASK;
		reg |= mps;
		dwc2_writel(hsotg, reg, DOEPCTL(ep));
	}

	return;

bad_mps:
	dev_err(hsotg->dev, "ep%d: bad mps of %d\n", ep, mps);
}

 
static void dwc2_hsotg_txfifo_flush(struct dwc2_hsotg *hsotg, unsigned int idx)
{
	dwc2_writel(hsotg, GRSTCTL_TXFNUM(idx) | GRSTCTL_TXFFLSH,
		    GRSTCTL);

	 
	if (dwc2_hsotg_wait_bit_clear(hsotg, GRSTCTL, GRSTCTL_TXFFLSH, 100))
		dev_warn(hsotg->dev, "%s: timeout flushing fifo GRSTCTL_TXFFLSH\n",
			 __func__);
}

 
static int dwc2_hsotg_trytx(struct dwc2_hsotg *hsotg,
			    struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg_req *hs_req = hs_ep->req;

	if (!hs_ep->dir_in || !hs_req) {
		 
		if (hs_ep->index != 0)
			dwc2_hsotg_ctrl_epint(hsotg, hs_ep->index,
					      hs_ep->dir_in, 0);
		return 0;
	}

	if (hs_req->req.actual < hs_req->req.length) {
		dev_dbg(hsotg->dev, "trying to write more for ep%d\n",
			hs_ep->index);
		return dwc2_hsotg_write_fifo(hsotg, hs_ep, hs_req);
	}

	return 0;
}

 
static void dwc2_hsotg_complete_in(struct dwc2_hsotg *hsotg,
				   struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg_req *hs_req = hs_ep->req;
	u32 epsize = dwc2_readl(hsotg, DIEPTSIZ(hs_ep->index));
	int size_left, size_done;

	if (!hs_req) {
		dev_dbg(hsotg->dev, "XferCompl but no req\n");
		return;
	}

	 
	if (hs_ep->index == 0 && hsotg->ep0_state == DWC2_EP0_STATUS_IN) {
		dev_dbg(hsotg->dev, "zlp packet sent\n");

		 
		hs_ep->dir_in = 0;

		dwc2_hsotg_complete_request(hsotg, hs_ep, hs_req, 0);
		if (hsotg->test_mode) {
			int ret;

			ret = dwc2_hsotg_set_test_mode(hsotg, hsotg->test_mode);
			if (ret < 0) {
				dev_dbg(hsotg->dev, "Invalid Test #%d\n",
					hsotg->test_mode);
				dwc2_hsotg_stall_ep0(hsotg);
				return;
			}
		}
		dwc2_hsotg_enqueue_setup(hsotg);
		return;
	}

	 
	if (using_desc_dma(hsotg)) {
		size_left = dwc2_gadget_get_xfersize_ddma(hs_ep);
		if (size_left < 0)
			dev_err(hsotg->dev, "error parsing DDMA results %d\n",
				size_left);
	} else {
		size_left = DXEPTSIZ_XFERSIZE_GET(epsize);
	}

	size_done = hs_ep->size_loaded - size_left;
	size_done += hs_ep->last_load;

	if (hs_req->req.actual != size_done)
		dev_dbg(hsotg->dev, "%s: adjusting size done %d => %d\n",
			__func__, hs_req->req.actual, size_done);

	hs_req->req.actual = size_done;
	dev_dbg(hsotg->dev, "req->length:%d req->actual:%d req->zero:%d\n",
		hs_req->req.length, hs_req->req.actual, hs_req->req.zero);

	if (!size_left && hs_req->req.actual < hs_req->req.length) {
		dev_dbg(hsotg->dev, "%s trying more for req...\n", __func__);
		dwc2_hsotg_start_req(hsotg, hs_ep, hs_req, true);
		return;
	}

	 
	if (hs_ep->send_zlp) {
		hs_ep->send_zlp = 0;
		if (!using_desc_dma(hsotg)) {
			dwc2_hsotg_program_zlp(hsotg, hs_ep);
			 
			return;
		}
	}

	if (hs_ep->index == 0 && hsotg->ep0_state == DWC2_EP0_DATA_IN) {
		 
		dwc2_hsotg_ep0_zlp(hsotg, false);
		return;
	}

	 
	if (!using_desc_dma(hsotg) && hs_ep->isochronous) {
		hs_req->req.frame_number = hs_ep->target_frame;
		dwc2_gadget_incr_frame_num(hs_ep);
	}

	dwc2_hsotg_complete_request(hsotg, hs_ep, hs_req, 0);
}

 
static u32 dwc2_gadget_read_ep_interrupts(struct dwc2_hsotg *hsotg,
					  unsigned int idx, int dir_in)
{
	u32 epmsk_reg = dir_in ? DIEPMSK : DOEPMSK;
	u32 epint_reg = dir_in ? DIEPINT(idx) : DOEPINT(idx);
	u32 ints;
	u32 mask;
	u32 diepempmsk;

	mask = dwc2_readl(hsotg, epmsk_reg);
	diepempmsk = dwc2_readl(hsotg, DIEPEMPMSK);
	mask |= ((diepempmsk >> idx) & 0x1) ? DIEPMSK_TXFIFOEMPTY : 0;
	mask |= DXEPINT_SETUP_RCVD;

	ints = dwc2_readl(hsotg, epint_reg);
	ints &= mask;
	return ints;
}

 
static void dwc2_gadget_handle_ep_disabled(struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	struct dwc2_hsotg_req *hs_req;
	unsigned char idx = hs_ep->index;
	int dir_in = hs_ep->dir_in;
	u32 epctl_reg = dir_in ? DIEPCTL(idx) : DOEPCTL(idx);
	int dctl = dwc2_readl(hsotg, DCTL);

	dev_dbg(hsotg->dev, "%s: EPDisbld\n", __func__);

	if (dir_in) {
		int epctl = dwc2_readl(hsotg, epctl_reg);

		dwc2_hsotg_txfifo_flush(hsotg, hs_ep->fifo_index);

		if ((epctl & DXEPCTL_STALL) && (epctl & DXEPCTL_EPTYPE_BULK)) {
			int dctl = dwc2_readl(hsotg, DCTL);

			dctl |= DCTL_CGNPINNAK;
			dwc2_writel(hsotg, dctl, DCTL);
		}
	} else {

		if (dctl & DCTL_GOUTNAKSTS) {
			dctl |= DCTL_CGOUTNAK;
			dwc2_writel(hsotg, dctl, DCTL);
		}
	}

	if (!hs_ep->isochronous)
		return;

	if (list_empty(&hs_ep->queue)) {
		dev_dbg(hsotg->dev, "%s: complete_ep 0x%p, ep->queue empty!\n",
			__func__, hs_ep);
		return;
	}

	do {
		hs_req = get_ep_head(hs_ep);
		if (hs_req) {
			hs_req->req.frame_number = hs_ep->target_frame;
			hs_req->req.actual = 0;
			dwc2_hsotg_complete_request(hsotg, hs_ep, hs_req,
						    -ENODATA);
		}
		dwc2_gadget_incr_frame_num(hs_ep);
		 
		hsotg->frame_number = dwc2_hsotg_read_frameno(hsotg);
	} while (dwc2_gadget_target_frame_elapsed(hs_ep));
}

 
static void dwc2_gadget_handle_out_token_ep_disabled(struct dwc2_hsotg_ep *ep)
{
	struct dwc2_hsotg *hsotg = ep->parent;
	struct dwc2_hsotg_req *hs_req;
	int dir_in = ep->dir_in;

	if (dir_in || !ep->isochronous)
		return;

	if (using_desc_dma(hsotg)) {
		if (ep->target_frame == TARGET_FRAME_INITIAL) {
			 
			ep->target_frame = hsotg->frame_number;
			dwc2_gadget_start_isoc_ddma(ep);
		}
		return;
	}

	if (ep->target_frame == TARGET_FRAME_INITIAL) {
		u32 ctrl;

		ep->target_frame = hsotg->frame_number;
		if (ep->interval > 1) {
			ctrl = dwc2_readl(hsotg, DOEPCTL(ep->index));
			if (ep->target_frame & 0x1)
				ctrl |= DXEPCTL_SETODDFR;
			else
				ctrl |= DXEPCTL_SETEVENFR;

			dwc2_writel(hsotg, ctrl, DOEPCTL(ep->index));
		}
	}

	while (dwc2_gadget_target_frame_elapsed(ep)) {
		hs_req = get_ep_head(ep);
		if (hs_req) {
			hs_req->req.frame_number = ep->target_frame;
			hs_req->req.actual = 0;
			dwc2_hsotg_complete_request(hsotg, ep, hs_req, -ENODATA);
		}

		dwc2_gadget_incr_frame_num(ep);
		 
		hsotg->frame_number = dwc2_hsotg_read_frameno(hsotg);
	}

	if (!ep->req)
		dwc2_gadget_start_next_request(ep);

}

static void dwc2_hsotg_ep_stop_xfr(struct dwc2_hsotg *hsotg,
				   struct dwc2_hsotg_ep *hs_ep);

 
static void dwc2_gadget_handle_nak(struct dwc2_hsotg_ep *hs_ep)
{
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	struct dwc2_hsotg_req *hs_req;
	int dir_in = hs_ep->dir_in;
	u32 ctrl;

	if (!dir_in || !hs_ep->isochronous)
		return;

	if (hs_ep->target_frame == TARGET_FRAME_INITIAL) {

		if (using_desc_dma(hsotg)) {
			hs_ep->target_frame = hsotg->frame_number;
			dwc2_gadget_incr_frame_num(hs_ep);

			 
			if (hsotg->params.service_interval) {
				 
				hs_ep->target_frame &= ~hs_ep->interval + 1;

				 
				dwc2_gadget_incr_frame_num(hs_ep);
				dwc2_gadget_dec_frame_num_by_one(hs_ep);
			}

			dwc2_gadget_start_isoc_ddma(hs_ep);
			return;
		}

		hs_ep->target_frame = hsotg->frame_number;
		if (hs_ep->interval > 1) {
			u32 ctrl = dwc2_readl(hsotg,
					      DIEPCTL(hs_ep->index));
			if (hs_ep->target_frame & 0x1)
				ctrl |= DXEPCTL_SETODDFR;
			else
				ctrl |= DXEPCTL_SETEVENFR;

			dwc2_writel(hsotg, ctrl, DIEPCTL(hs_ep->index));
		}
	}

	if (using_desc_dma(hsotg))
		return;

	ctrl = dwc2_readl(hsotg, DIEPCTL(hs_ep->index));
	if (ctrl & DXEPCTL_EPENA)
		dwc2_hsotg_ep_stop_xfr(hsotg, hs_ep);
	else
		dwc2_hsotg_txfifo_flush(hsotg, hs_ep->fifo_index);

	while (dwc2_gadget_target_frame_elapsed(hs_ep)) {
		hs_req = get_ep_head(hs_ep);
		if (hs_req) {
			hs_req->req.frame_number = hs_ep->target_frame;
			hs_req->req.actual = 0;
			dwc2_hsotg_complete_request(hsotg, hs_ep, hs_req, -ENODATA);
		}

		dwc2_gadget_incr_frame_num(hs_ep);
		 
		hsotg->frame_number = dwc2_hsotg_read_frameno(hsotg);
	}

	if (!hs_ep->req)
		dwc2_gadget_start_next_request(hs_ep);
}

 
static void dwc2_hsotg_epint(struct dwc2_hsotg *hsotg, unsigned int idx,
			     int dir_in)
{
	struct dwc2_hsotg_ep *hs_ep = index_to_ep(hsotg, idx, dir_in);
	u32 epint_reg = dir_in ? DIEPINT(idx) : DOEPINT(idx);
	u32 epctl_reg = dir_in ? DIEPCTL(idx) : DOEPCTL(idx);
	u32 epsiz_reg = dir_in ? DIEPTSIZ(idx) : DOEPTSIZ(idx);
	u32 ints;

	ints = dwc2_gadget_read_ep_interrupts(hsotg, idx, dir_in);

	 
	dwc2_writel(hsotg, ints, epint_reg);

	if (!hs_ep) {
		dev_err(hsotg->dev, "%s:Interrupt for unconfigured ep%d(%s)\n",
			__func__, idx, dir_in ? "in" : "out");
		return;
	}

	dev_dbg(hsotg->dev, "%s: ep%d(%s) DxEPINT=0x%08x\n",
		__func__, idx, dir_in ? "in" : "out", ints);

	 
	if (idx == 0 && (ints & (DXEPINT_SETUP | DXEPINT_SETUP_RCVD)))
		ints &= ~DXEPINT_XFERCOMPL;

	 
	if (using_desc_dma(hsotg) && idx == 0 && !hs_ep->dir_in &&
	    hsotg->ep0_state == DWC2_EP0_SETUP && !(ints & DXEPINT_SETUP))
		ints &= ~DXEPINT_XFERCOMPL;

	if (ints & DXEPINT_XFERCOMPL) {
		dev_dbg(hsotg->dev,
			"%s: XferCompl: DxEPCTL=0x%08x, DXEPTSIZ=%08x\n",
			__func__, dwc2_readl(hsotg, epctl_reg),
			dwc2_readl(hsotg, epsiz_reg));

		 
		if (using_desc_dma(hsotg) && hs_ep->isochronous) {
			dwc2_gadget_complete_isoc_request_ddma(hs_ep);
		} else if (dir_in) {
			 
			if (!hs_ep->isochronous || !(ints & DXEPINT_NAKINTRPT))
				dwc2_hsotg_complete_in(hsotg, hs_ep);

			if (idx == 0 && !hs_ep->req)
				dwc2_hsotg_enqueue_setup(hsotg);
		} else if (using_dma(hsotg)) {
			 
			if (!hs_ep->isochronous || !(ints & DXEPINT_OUTTKNEPDIS))
				dwc2_hsotg_handle_outdone(hsotg, idx);
		}
	}

	if (ints & DXEPINT_EPDISBLD)
		dwc2_gadget_handle_ep_disabled(hs_ep);

	if (ints & DXEPINT_OUTTKNEPDIS)
		dwc2_gadget_handle_out_token_ep_disabled(hs_ep);

	if (ints & DXEPINT_NAKINTRPT)
		dwc2_gadget_handle_nak(hs_ep);

	if (ints & DXEPINT_AHBERR)
		dev_dbg(hsotg->dev, "%s: AHBErr\n", __func__);

	if (ints & DXEPINT_SETUP) {   
		dev_dbg(hsotg->dev, "%s: Setup/Timeout\n",  __func__);

		if (using_dma(hsotg) && idx == 0) {
			 

			if (dir_in)
				WARN_ON_ONCE(1);
			else
				dwc2_hsotg_handle_outdone(hsotg, 0);
		}
	}

	if (ints & DXEPINT_STSPHSERCVD) {
		dev_dbg(hsotg->dev, "%s: StsPhseRcvd\n", __func__);

		 
		if (hsotg->ep0_state == DWC2_EP0_DATA_OUT) {
			 
			if (using_desc_dma(hsotg)) {
				if (!hsotg->delayed_status)
					dwc2_hsotg_ep0_zlp(hsotg, true);
				else
				 
					dwc2_set_bit(hsotg, DIEPCTL(0),
						     DXEPCTL_CNAK);
			}
		}

	}

	if (ints & DXEPINT_BACK2BACKSETUP)
		dev_dbg(hsotg->dev, "%s: B2BSetup/INEPNakEff\n", __func__);

	if (ints & DXEPINT_BNAINTR) {
		dev_dbg(hsotg->dev, "%s: BNA interrupt\n", __func__);
		if (hs_ep->isochronous)
			dwc2_gadget_handle_isoc_bna(hs_ep);
	}

	if (dir_in && !hs_ep->isochronous) {
		 
		if (ints & DXEPINT_INTKNTXFEMP) {
			dev_dbg(hsotg->dev, "%s: ep%d: INTknTXFEmpMsk\n",
				__func__, idx);
		}

		 
		if (ints & DXEPINT_INTKNEPMIS) {
			dev_warn(hsotg->dev, "%s: ep%d: INTknEP\n",
				 __func__, idx);
		}

		 
		if (hsotg->dedicated_fifos &&
		    ints & DXEPINT_TXFEMP) {
			dev_dbg(hsotg->dev, "%s: ep%d: TxFIFOEmpty\n",
				__func__, idx);
			if (!using_dma(hsotg))
				dwc2_hsotg_trytx(hsotg, hs_ep);
		}
	}
}

 
static void dwc2_hsotg_irq_enumdone(struct dwc2_hsotg *hsotg)
{
	u32 dsts = dwc2_readl(hsotg, DSTS);
	int ep0_mps = 0, ep_mps = 8;

	 

	dev_dbg(hsotg->dev, "EnumDone (DSTS=0x%08x)\n", dsts);

	 

	 
	switch ((dsts & DSTS_ENUMSPD_MASK) >> DSTS_ENUMSPD_SHIFT) {
	case DSTS_ENUMSPD_FS:
	case DSTS_ENUMSPD_FS48:
		hsotg->gadget.speed = USB_SPEED_FULL;
		ep0_mps = EP0_MPS_LIMIT;
		ep_mps = 1023;
		break;

	case DSTS_ENUMSPD_HS:
		hsotg->gadget.speed = USB_SPEED_HIGH;
		ep0_mps = EP0_MPS_LIMIT;
		ep_mps = 1024;
		break;

	case DSTS_ENUMSPD_LS:
		hsotg->gadget.speed = USB_SPEED_LOW;
		ep0_mps = 8;
		ep_mps = 8;
		 
		break;
	}
	dev_info(hsotg->dev, "new device is %s\n",
		 usb_speed_string(hsotg->gadget.speed));

	 

	if (ep0_mps) {
		int i;
		 
		dwc2_hsotg_set_ep_maxpacket(hsotg, 0, ep0_mps, 0, 1);
		dwc2_hsotg_set_ep_maxpacket(hsotg, 0, ep0_mps, 0, 0);
		for (i = 1; i < hsotg->num_of_eps; i++) {
			if (hsotg->eps_in[i])
				dwc2_hsotg_set_ep_maxpacket(hsotg, i, ep_mps,
							    0, 1);
			if (hsotg->eps_out[i])
				dwc2_hsotg_set_ep_maxpacket(hsotg, i, ep_mps,
							    0, 0);
		}
	}

	 

	dwc2_hsotg_enqueue_setup(hsotg);

	dev_dbg(hsotg->dev, "EP0: DIEPCTL0=0x%08x, DOEPCTL0=0x%08x\n",
		dwc2_readl(hsotg, DIEPCTL0),
		dwc2_readl(hsotg, DOEPCTL0));
}

 
static void kill_all_requests(struct dwc2_hsotg *hsotg,
			      struct dwc2_hsotg_ep *ep,
			      int result)
{
	unsigned int size;

	ep->req = NULL;

	while (!list_empty(&ep->queue)) {
		struct dwc2_hsotg_req *req = get_ep_head(ep);

		dwc2_hsotg_complete_request(hsotg, ep, req, result);
	}

	if (!hsotg->dedicated_fifos)
		return;
	size = (dwc2_readl(hsotg, DTXFSTS(ep->fifo_index)) & 0xffff) * 4;
	if (size < ep->fifo_size)
		dwc2_hsotg_txfifo_flush(hsotg, ep->fifo_index);
}

 
void dwc2_hsotg_disconnect(struct dwc2_hsotg *hsotg)
{
	unsigned int ep;

	if (!hsotg->connected)
		return;

	hsotg->connected = 0;
	hsotg->test_mode = 0;

	 
	for (ep = 0; ep < hsotg->num_of_eps; ep++) {
		if (hsotg->eps_in[ep])
			kill_all_requests(hsotg, hsotg->eps_in[ep],
					  -ESHUTDOWN);
		if (hsotg->eps_out[ep])
			kill_all_requests(hsotg, hsotg->eps_out[ep],
					  -ESHUTDOWN);
	}

	call_gadget(hsotg, disconnect);
	hsotg->lx_state = DWC2_L3;

	usb_gadget_set_state(&hsotg->gadget, USB_STATE_NOTATTACHED);
}

 
static void dwc2_hsotg_irq_fifoempty(struct dwc2_hsotg *hsotg, bool periodic)
{
	struct dwc2_hsotg_ep *ep;
	int epno, ret;

	 
	for (epno = 0; epno < hsotg->num_of_eps; epno++) {
		ep = index_to_ep(hsotg, epno, 1);

		if (!ep)
			continue;

		if (!ep->dir_in)
			continue;

		if ((periodic && !ep->periodic) ||
		    (!periodic && ep->periodic))
			continue;

		ret = dwc2_hsotg_trytx(hsotg, ep);
		if (ret < 0)
			break;
	}
}

 
#define IRQ_RETRY_MASK (GINTSTS_NPTXFEMP | \
			GINTSTS_PTXFEMP |  \
			GINTSTS_RXFLVL)

static int dwc2_hsotg_ep_disable(struct usb_ep *ep);
 
void dwc2_hsotg_core_init_disconnected(struct dwc2_hsotg *hsotg,
				       bool is_usb_reset)
{
	u32 intmsk;
	u32 val;
	u32 usbcfg;
	u32 dcfg = 0;
	int ep;

	 
	kill_all_requests(hsotg, hsotg->eps_out[0], -ECONNRESET);

	if (!is_usb_reset) {
		if (dwc2_core_reset(hsotg, true))
			return;
	} else {
		 
		for (ep = 1; ep < hsotg->num_of_eps; ep++) {
			if (hsotg->eps_in[ep])
				dwc2_hsotg_ep_disable(&hsotg->eps_in[ep]->ep);
			if (hsotg->eps_out[ep])
				dwc2_hsotg_ep_disable(&hsotg->eps_out[ep]->ep);
		}
	}

	 

	 
	usbcfg = dwc2_readl(hsotg, GUSBCFG);
	usbcfg &= ~GUSBCFG_TOUTCAL_MASK;
	usbcfg |= GUSBCFG_TOUTCAL(7);

	 
	usbcfg &= ~(GUSBCFG_SRPCAP | GUSBCFG_HNPCAP);
        dwc2_writel(hsotg, usbcfg, GUSBCFG);

	dwc2_phy_init(hsotg, true);

	dwc2_hsotg_init_fifo(hsotg);

	if (!is_usb_reset)
		dwc2_set_bit(hsotg, DCTL, DCTL_SFTDISCON);

	dcfg |= DCFG_EPMISCNT(1);

	switch (hsotg->params.speed) {
	case DWC2_SPEED_PARAM_LOW:
		dcfg |= DCFG_DEVSPD_LS;
		break;
	case DWC2_SPEED_PARAM_FULL:
		if (hsotg->params.phy_type == DWC2_PHY_TYPE_PARAM_FS)
			dcfg |= DCFG_DEVSPD_FS48;
		else
			dcfg |= DCFG_DEVSPD_FS;
		break;
	default:
		dcfg |= DCFG_DEVSPD_HS;
	}

	if (hsotg->params.ipg_isoc_en)
		dcfg |= DCFG_IPG_ISOC_SUPPORDED;

	dwc2_writel(hsotg, dcfg,  DCFG);

	 
	dwc2_writel(hsotg, 0xffffffff, GOTGINT);

	 
	dwc2_writel(hsotg, 0xffffffff, GINTSTS);
	intmsk = GINTSTS_ERLYSUSP | GINTSTS_SESSREQINT |
		GINTSTS_GOUTNAKEFF | GINTSTS_GINNAKEFF |
		GINTSTS_USBRST | GINTSTS_RESETDET |
		GINTSTS_ENUMDONE | GINTSTS_OTGINT |
		GINTSTS_USBSUSP | GINTSTS_WKUPINT |
		GINTSTS_LPMTRANRCVD;

	if (!using_desc_dma(hsotg))
		intmsk |= GINTSTS_INCOMPL_SOIN | GINTSTS_INCOMPL_SOOUT;

	if (!hsotg->params.external_id_pin_ctl)
		intmsk |= GINTSTS_CONIDSTSCHNG;

	dwc2_writel(hsotg, intmsk, GINTMSK);

	if (using_dma(hsotg)) {
		dwc2_writel(hsotg, GAHBCFG_GLBL_INTR_EN | GAHBCFG_DMA_EN |
			    hsotg->params.ahbcfg,
			    GAHBCFG);

		 
		if (using_desc_dma(hsotg))
			dwc2_set_bit(hsotg, DCFG, DCFG_DESCDMA_EN);

	} else {
		dwc2_writel(hsotg, ((hsotg->dedicated_fifos) ?
						(GAHBCFG_NP_TXF_EMP_LVL |
						 GAHBCFG_P_TXF_EMP_LVL) : 0) |
			    GAHBCFG_GLBL_INTR_EN, GAHBCFG);
	}

	 

	dwc2_writel(hsotg, ((hsotg->dedicated_fifos && !using_dma(hsotg)) ?
		DIEPMSK_TXFIFOEMPTY | DIEPMSK_INTKNTXFEMPMSK : 0) |
		DIEPMSK_EPDISBLDMSK | DIEPMSK_XFERCOMPLMSK |
		DIEPMSK_TIMEOUTMSK | DIEPMSK_AHBERRMSK,
		DIEPMSK);

	 
	dwc2_writel(hsotg, (using_dma(hsotg) ? (DIEPMSK_XFERCOMPLMSK |
		DOEPMSK_STSPHSERCVDMSK) : 0) |
		DOEPMSK_EPDISBLDMSK | DOEPMSK_AHBERRMSK |
		DOEPMSK_SETUPMSK,
		DOEPMSK);

	 
	if (using_desc_dma(hsotg)) {
		dwc2_set_bit(hsotg, DOEPMSK, DOEPMSK_BNAMSK);
		dwc2_set_bit(hsotg, DIEPMSK, DIEPMSK_BNAININTRMSK);
	}

	 
	if (using_desc_dma(hsotg) && hsotg->params.service_interval)
		dwc2_set_bit(hsotg, DCTL, DCTL_SERVICE_INTERVAL_SUPPORTED);

	dwc2_writel(hsotg, 0, DAINTMSK);

	dev_dbg(hsotg->dev, "EP0: DIEPCTL0=0x%08x, DOEPCTL0=0x%08x\n",
		dwc2_readl(hsotg, DIEPCTL0),
		dwc2_readl(hsotg, DOEPCTL0));

	 
	dwc2_hsotg_en_gsint(hsotg, GINTSTS_OEPINT | GINTSTS_IEPINT);

	 
	if (!using_dma(hsotg))
		dwc2_hsotg_en_gsint(hsotg, GINTSTS_RXFLVL);

	 
	dwc2_hsotg_ctrl_epint(hsotg, 0, 0, 1);
	dwc2_hsotg_ctrl_epint(hsotg, 0, 1, 1);

	if (!is_usb_reset) {
		dwc2_set_bit(hsotg, DCTL, DCTL_PWRONPRGDONE);
		udelay(10);   
		dwc2_clear_bit(hsotg, DCTL, DCTL_PWRONPRGDONE);
	}

	dev_dbg(hsotg->dev, "DCTL=0x%08x\n", dwc2_readl(hsotg, DCTL));

	 

	 
	dwc2_writel(hsotg, DXEPTSIZ_MC(1) | DXEPTSIZ_PKTCNT(1) |
	       DXEPTSIZ_XFERSIZE(8), DOEPTSIZ0);

	dwc2_writel(hsotg, dwc2_hsotg_ep0_mps(hsotg->eps_out[0]->ep.maxpacket) |
	       DXEPCTL_CNAK | DXEPCTL_EPENA |
	       DXEPCTL_USBACTEP,
	       DOEPCTL0);

	 
	dwc2_writel(hsotg, dwc2_hsotg_ep0_mps(hsotg->eps_out[0]->ep.maxpacket) |
	       DXEPCTL_USBACTEP, DIEPCTL0);

	 
	val = DCTL_CGOUTNAK | DCTL_CGNPINNAK;
	if (!is_usb_reset)
		val |= DCTL_SFTDISCON;
	dwc2_set_bit(hsotg, DCTL, val);

	 
	dwc2_gadget_init_lpm(hsotg);

	 
	if (using_desc_dma(hsotg) && hsotg->params.service_interval)
		dwc2_gadget_program_ref_clk(hsotg);

	 
	mdelay(3);

	hsotg->lx_state = DWC2_L0;

	dwc2_hsotg_enqueue_setup(hsotg);

	dev_dbg(hsotg->dev, "EP0: DIEPCTL0=0x%08x, DOEPCTL0=0x%08x\n",
		dwc2_readl(hsotg, DIEPCTL0),
		dwc2_readl(hsotg, DOEPCTL0));
}

void dwc2_hsotg_core_disconnect(struct dwc2_hsotg *hsotg)
{
	 
	dwc2_set_bit(hsotg, DCTL, DCTL_SFTDISCON);
}

void dwc2_hsotg_core_connect(struct dwc2_hsotg *hsotg)
{
	 
	if (!hsotg->role_sw || (dwc2_readl(hsotg, GOTGCTL) & GOTGCTL_BSESVLD))
		dwc2_clear_bit(hsotg, DCTL, DCTL_SFTDISCON);
}

 
static void dwc2_gadget_handle_incomplete_isoc_in(struct dwc2_hsotg *hsotg)
{
	struct dwc2_hsotg_ep *hs_ep;
	u32 epctrl;
	u32 daintmsk;
	u32 idx;

	dev_dbg(hsotg->dev, "Incomplete isoc in interrupt received:\n");

	daintmsk = dwc2_readl(hsotg, DAINTMSK);

	for (idx = 1; idx < hsotg->num_of_eps; idx++) {
		hs_ep = hsotg->eps_in[idx];
		 
		if ((BIT(idx) & ~daintmsk) || !hs_ep->isochronous)
			continue;

		epctrl = dwc2_readl(hsotg, DIEPCTL(idx));
		if ((epctrl & DXEPCTL_EPENA) &&
		    dwc2_gadget_target_frame_elapsed(hs_ep)) {
			epctrl |= DXEPCTL_SNAK;
			epctrl |= DXEPCTL_EPDIS;
			dwc2_writel(hsotg, epctrl, DIEPCTL(idx));
		}
	}

	 
	dwc2_writel(hsotg, GINTSTS_INCOMPL_SOIN, GINTSTS);
}

 
static void dwc2_gadget_handle_incomplete_isoc_out(struct dwc2_hsotg *hsotg)
{
	u32 gintsts;
	u32 gintmsk;
	u32 daintmsk;
	u32 epctrl;
	struct dwc2_hsotg_ep *hs_ep;
	int idx;

	dev_dbg(hsotg->dev, "%s: GINTSTS_INCOMPL_SOOUT\n", __func__);

	daintmsk = dwc2_readl(hsotg, DAINTMSK);
	daintmsk >>= DAINT_OUTEP_SHIFT;

	for (idx = 1; idx < hsotg->num_of_eps; idx++) {
		hs_ep = hsotg->eps_out[idx];
		 
		if ((BIT(idx) & ~daintmsk) || !hs_ep->isochronous)
			continue;

		epctrl = dwc2_readl(hsotg, DOEPCTL(idx));
		if ((epctrl & DXEPCTL_EPENA) &&
		    dwc2_gadget_target_frame_elapsed(hs_ep)) {
			 
			gintmsk = dwc2_readl(hsotg, GINTMSK);
			gintmsk |= GINTSTS_GOUTNAKEFF;
			dwc2_writel(hsotg, gintmsk, GINTMSK);

			gintsts = dwc2_readl(hsotg, GINTSTS);
			if (!(gintsts & GINTSTS_GOUTNAKEFF)) {
				dwc2_set_bit(hsotg, DCTL, DCTL_SGOUTNAK);
				break;
			}
		}
	}

	 
	dwc2_writel(hsotg, GINTSTS_INCOMPL_SOOUT, GINTSTS);
}

 
static irqreturn_t dwc2_hsotg_irq(int irq, void *pw)
{
	struct dwc2_hsotg *hsotg = pw;
	int retry_count = 8;
	u32 gintsts;
	u32 gintmsk;

	if (!dwc2_is_device_mode(hsotg))
		return IRQ_NONE;

	spin_lock(&hsotg->lock);
irq_retry:
	gintsts = dwc2_readl(hsotg, GINTSTS);
	gintmsk = dwc2_readl(hsotg, GINTMSK);

	dev_dbg(hsotg->dev, "%s: %08x %08x (%08x) retry %d\n",
		__func__, gintsts, gintsts & gintmsk, gintmsk, retry_count);

	gintsts &= gintmsk;

	if (gintsts & GINTSTS_RESETDET) {
		dev_dbg(hsotg->dev, "%s: USBRstDet\n", __func__);

		dwc2_writel(hsotg, GINTSTS_RESETDET, GINTSTS);

		 
		if (hsotg->in_ppd && hsotg->lx_state == DWC2_L2)
			dwc2_exit_partial_power_down(hsotg, 0, true);

		hsotg->lx_state = DWC2_L0;
	}

	if (gintsts & (GINTSTS_USBRST | GINTSTS_RESETDET)) {
		u32 usb_status = dwc2_readl(hsotg, GOTGCTL);
		u32 connected = hsotg->connected;

		dev_dbg(hsotg->dev, "%s: USBRst\n", __func__);
		dev_dbg(hsotg->dev, "GNPTXSTS=%08x\n",
			dwc2_readl(hsotg, GNPTXSTS));

		dwc2_writel(hsotg, GINTSTS_USBRST, GINTSTS);

		 
		dwc2_hsotg_disconnect(hsotg);

		 
		dwc2_clear_bit(hsotg, DCFG, DCFG_DEVADDR_MASK);

		if (usb_status & GOTGCTL_BSESVLD && connected)
			dwc2_hsotg_core_init_disconnected(hsotg, true);
	}

	if (gintsts & GINTSTS_ENUMDONE) {
		dwc2_writel(hsotg, GINTSTS_ENUMDONE, GINTSTS);

		dwc2_hsotg_irq_enumdone(hsotg);
	}

	if (gintsts & (GINTSTS_OEPINT | GINTSTS_IEPINT)) {
		u32 daint = dwc2_readl(hsotg, DAINT);
		u32 daintmsk = dwc2_readl(hsotg, DAINTMSK);
		u32 daint_out, daint_in;
		int ep;

		daint &= daintmsk;
		daint_out = daint >> DAINT_OUTEP_SHIFT;
		daint_in = daint & ~(daint_out << DAINT_OUTEP_SHIFT);

		dev_dbg(hsotg->dev, "%s: daint=%08x\n", __func__, daint);

		for (ep = 0; ep < hsotg->num_of_eps && daint_out;
						ep++, daint_out >>= 1) {
			if (daint_out & 1)
				dwc2_hsotg_epint(hsotg, ep, 0);
		}

		for (ep = 0; ep < hsotg->num_of_eps  && daint_in;
						ep++, daint_in >>= 1) {
			if (daint_in & 1)
				dwc2_hsotg_epint(hsotg, ep, 1);
		}
	}

	 

	if (gintsts & GINTSTS_NPTXFEMP) {
		dev_dbg(hsotg->dev, "NPTxFEmp\n");

		 

		dwc2_hsotg_disable_gsint(hsotg, GINTSTS_NPTXFEMP);
		dwc2_hsotg_irq_fifoempty(hsotg, false);
	}

	if (gintsts & GINTSTS_PTXFEMP) {
		dev_dbg(hsotg->dev, "PTxFEmp\n");

		 

		dwc2_hsotg_disable_gsint(hsotg, GINTSTS_PTXFEMP);
		dwc2_hsotg_irq_fifoempty(hsotg, true);
	}

	if (gintsts & GINTSTS_RXFLVL) {
		 

		dwc2_hsotg_handle_rx(hsotg);
	}

	if (gintsts & GINTSTS_ERLYSUSP) {
		dev_dbg(hsotg->dev, "GINTSTS_ErlySusp\n");
		dwc2_writel(hsotg, GINTSTS_ERLYSUSP, GINTSTS);
	}

	 

	if (gintsts & GINTSTS_GOUTNAKEFF) {
		u8 idx;
		u32 epctrl;
		u32 gintmsk;
		u32 daintmsk;
		struct dwc2_hsotg_ep *hs_ep;

		daintmsk = dwc2_readl(hsotg, DAINTMSK);
		daintmsk >>= DAINT_OUTEP_SHIFT;
		 
		gintmsk = dwc2_readl(hsotg, GINTMSK);
		gintmsk &= ~GINTSTS_GOUTNAKEFF;
		dwc2_writel(hsotg, gintmsk, GINTMSK);

		dev_dbg(hsotg->dev, "GOUTNakEff triggered\n");
		for (idx = 1; idx < hsotg->num_of_eps; idx++) {
			hs_ep = hsotg->eps_out[idx];
			 
			if (BIT(idx) & ~daintmsk)
				continue;

			epctrl = dwc2_readl(hsotg, DOEPCTL(idx));

			
			if ((epctrl & DXEPCTL_EPENA) && hs_ep->isochronous) {
				epctrl |= DXEPCTL_SNAK;
				epctrl |= DXEPCTL_EPDIS;
				dwc2_writel(hsotg, epctrl, DOEPCTL(idx));
				continue;
			}

			
			if (hs_ep->halted) {
				if (!(epctrl & DXEPCTL_EPENA))
					epctrl |= DXEPCTL_EPENA;
				epctrl |= DXEPCTL_EPDIS;
				epctrl |= DXEPCTL_STALL;
				dwc2_writel(hsotg, epctrl, DOEPCTL(idx));
			}
		}

		 
	}

	if (gintsts & GINTSTS_GINNAKEFF) {
		dev_info(hsotg->dev, "GINNakEff triggered\n");

		dwc2_set_bit(hsotg, DCTL, DCTL_CGNPINNAK);

		dwc2_hsotg_dump(hsotg);
	}

	if (gintsts & GINTSTS_INCOMPL_SOIN)
		dwc2_gadget_handle_incomplete_isoc_in(hsotg);

	if (gintsts & GINTSTS_INCOMPL_SOOUT)
		dwc2_gadget_handle_incomplete_isoc_out(hsotg);

	 

	if (gintsts & IRQ_RETRY_MASK && --retry_count > 0)
		goto irq_retry;

	 
	if (hsotg->params.service_interval)
		dwc2_gadget_wkup_alert_handler(hsotg);

	spin_unlock(&hsotg->lock);

	return IRQ_HANDLED;
}

static void dwc2_hsotg_ep_stop_xfr(struct dwc2_hsotg *hsotg,
				   struct dwc2_hsotg_ep *hs_ep)
{
	u32 epctrl_reg;
	u32 epint_reg;

	epctrl_reg = hs_ep->dir_in ? DIEPCTL(hs_ep->index) :
		DOEPCTL(hs_ep->index);
	epint_reg = hs_ep->dir_in ? DIEPINT(hs_ep->index) :
		DOEPINT(hs_ep->index);

	dev_dbg(hsotg->dev, "%s: stopping transfer on %s\n", __func__,
		hs_ep->name);

	if (hs_ep->dir_in) {
		if (hsotg->dedicated_fifos || hs_ep->periodic) {
			dwc2_set_bit(hsotg, epctrl_reg, DXEPCTL_SNAK);
			 
			if (dwc2_hsotg_wait_bit_set(hsotg, epint_reg,
						    DXEPINT_INEPNAKEFF, 100))
				dev_warn(hsotg->dev,
					 "%s: timeout DIEPINT.NAKEFF\n",
					 __func__);
		} else {
			dwc2_set_bit(hsotg, DCTL, DCTL_SGNPINNAK);
			 
			if (dwc2_hsotg_wait_bit_set(hsotg, GINTSTS,
						    GINTSTS_GINNAKEFF, 100))
				dev_warn(hsotg->dev,
					 "%s: timeout GINTSTS.GINNAKEFF\n",
					 __func__);
		}
	} else {
		 
		dwc2_hsotg_disable_gsint(hsotg, GINTSTS_GOUTNAKEFF);

		if (!(dwc2_readl(hsotg, GINTSTS) & GINTSTS_GOUTNAKEFF))
			dwc2_set_bit(hsotg, DCTL, DCTL_SGOUTNAK);

		if (!using_dma(hsotg)) {
			 
			if (dwc2_hsotg_wait_bit_set(hsotg, GINTSTS,
						    GINTSTS_RXFLVL, 100)) {
				dev_warn(hsotg->dev, "%s: timeout GINTSTS.RXFLVL\n",
					 __func__);
			} else {
				 
				dwc2_readl(hsotg, GRXSTSP);
			}
		}

		 
		if (dwc2_hsotg_wait_bit_set(hsotg, GINTSTS,
					    GINTSTS_GOUTNAKEFF, 100))
			dev_warn(hsotg->dev, "%s: timeout GINTSTS.GOUTNAKEFF\n",
				 __func__);
	}

	 
	dwc2_set_bit(hsotg, epctrl_reg, DXEPCTL_EPDIS | DXEPCTL_SNAK);

	 
	if (dwc2_hsotg_wait_bit_set(hsotg, epint_reg, DXEPINT_EPDISBLD, 100))
		dev_warn(hsotg->dev,
			 "%s: timeout DOEPCTL.EPDisable\n", __func__);

	 
	dwc2_set_bit(hsotg, epint_reg, DXEPINT_EPDISBLD);

	if (hs_ep->dir_in) {
		unsigned short fifo_index;

		if (hsotg->dedicated_fifos || hs_ep->periodic)
			fifo_index = hs_ep->fifo_index;
		else
			fifo_index = 0;

		 
		dwc2_flush_tx_fifo(hsotg, fifo_index);

		 
		if (!hsotg->dedicated_fifos && !hs_ep->periodic)
			dwc2_set_bit(hsotg, DCTL, DCTL_CGNPINNAK);

	} else {
		 
		dwc2_set_bit(hsotg, DCTL, DCTL_CGOUTNAK);
	}
}

 
static int dwc2_hsotg_ep_enable(struct usb_ep *ep,
				const struct usb_endpoint_descriptor *desc)
{
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	unsigned long flags;
	unsigned int index = hs_ep->index;
	u32 epctrl_reg;
	u32 epctrl;
	u32 mps;
	u32 mc;
	u32 mask;
	unsigned int dir_in;
	unsigned int i, val, size;
	int ret = 0;
	unsigned char ep_type;
	int desc_num;

	dev_dbg(hsotg->dev,
		"%s: ep %s: a 0x%02x, attr 0x%02x, mps 0x%04x, intr %d\n",
		__func__, ep->name, desc->bEndpointAddress, desc->bmAttributes,
		desc->wMaxPacketSize, desc->bInterval);

	 
	if (index == 0) {
		dev_err(hsotg->dev, "%s: called for EP 0\n", __func__);
		return -EINVAL;
	}

	dir_in = (desc->bEndpointAddress & USB_ENDPOINT_DIR_MASK) ? 1 : 0;
	if (dir_in != hs_ep->dir_in) {
		dev_err(hsotg->dev, "%s: direction mismatch!\n", __func__);
		return -EINVAL;
	}

	ep_type = desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
	mps = usb_endpoint_maxp(desc);
	mc = usb_endpoint_maxp_mult(desc);

	 
	if (using_desc_dma(hsotg) && ep_type == USB_ENDPOINT_XFER_ISOC &&
	    dir_in && desc->bInterval > 10) {
		dev_err(hsotg->dev,
			"%s: ISOC IN, DDMA: bInterval>10 not supported!\n", __func__);
		return -EINVAL;
	}

	 
	if (using_desc_dma(hsotg) && ep_type == USB_ENDPOINT_XFER_ISOC &&
	    !dir_in && mc > 1) {
		dev_err(hsotg->dev,
			"%s: ISOC OUT, DDMA: HB not supported!\n", __func__);
		return -EINVAL;
	}

	 

	epctrl_reg = dir_in ? DIEPCTL(index) : DOEPCTL(index);
	epctrl = dwc2_readl(hsotg, epctrl_reg);

	dev_dbg(hsotg->dev, "%s: read DxEPCTL=0x%08x from 0x%08x\n",
		__func__, epctrl, epctrl_reg);

	if (using_desc_dma(hsotg) && ep_type == USB_ENDPOINT_XFER_ISOC)
		desc_num = MAX_DMA_DESC_NUM_HS_ISOC;
	else
		desc_num = MAX_DMA_DESC_NUM_GENERIC;

	 
	if (using_desc_dma(hsotg) && !hs_ep->desc_list) {
		hs_ep->desc_list = dmam_alloc_coherent(hsotg->dev,
			desc_num * sizeof(struct dwc2_dma_desc),
			&hs_ep->desc_list_dma, GFP_ATOMIC);
		if (!hs_ep->desc_list) {
			ret = -ENOMEM;
			goto error2;
		}
	}

	spin_lock_irqsave(&hsotg->lock, flags);

	epctrl &= ~(DXEPCTL_EPTYPE_MASK | DXEPCTL_MPS_MASK);
	epctrl |= DXEPCTL_MPS(mps);

	 
	epctrl |= DXEPCTL_USBACTEP;

	 
	dwc2_hsotg_set_ep_maxpacket(hsotg, hs_ep->index, mps, mc, dir_in);

	 
	hs_ep->isochronous = 0;
	hs_ep->periodic = 0;
	hs_ep->halted = 0;
	hs_ep->wedged = 0;
	hs_ep->interval = desc->bInterval;

	switch (ep_type) {
	case USB_ENDPOINT_XFER_ISOC:
		epctrl |= DXEPCTL_EPTYPE_ISO;
		epctrl |= DXEPCTL_SETEVENFR;
		hs_ep->isochronous = 1;
		hs_ep->interval = 1 << (desc->bInterval - 1);
		hs_ep->target_frame = TARGET_FRAME_INITIAL;
		hs_ep->next_desc = 0;
		hs_ep->compl_desc = 0;
		if (dir_in) {
			hs_ep->periodic = 1;
			mask = dwc2_readl(hsotg, DIEPMSK);
			mask |= DIEPMSK_NAKMSK;
			dwc2_writel(hsotg, mask, DIEPMSK);
		} else {
			epctrl |= DXEPCTL_SNAK;
			mask = dwc2_readl(hsotg, DOEPMSK);
			mask |= DOEPMSK_OUTTKNEPDISMSK;
			dwc2_writel(hsotg, mask, DOEPMSK);
		}
		break;

	case USB_ENDPOINT_XFER_BULK:
		epctrl |= DXEPCTL_EPTYPE_BULK;
		break;

	case USB_ENDPOINT_XFER_INT:
		if (dir_in)
			hs_ep->periodic = 1;

		if (hsotg->gadget.speed == USB_SPEED_HIGH)
			hs_ep->interval = 1 << (desc->bInterval - 1);

		epctrl |= DXEPCTL_EPTYPE_INTERRUPT;
		break;

	case USB_ENDPOINT_XFER_CONTROL:
		epctrl |= DXEPCTL_EPTYPE_CONTROL;
		break;
	}

	 
	if (dir_in && hsotg->dedicated_fifos) {
		unsigned fifo_count = dwc2_hsotg_tx_fifo_count(hsotg);
		u32 fifo_index = 0;
		u32 fifo_size = UINT_MAX;

		size = hs_ep->ep.maxpacket * hs_ep->mc;
		for (i = 1; i <= fifo_count; ++i) {
			if (hsotg->fifo_map & (1 << i))
				continue;
			val = dwc2_readl(hsotg, DPTXFSIZN(i));
			val = (val >> FIFOSIZE_DEPTH_SHIFT) * 4;
			if (val < size)
				continue;
			 
			if (val < fifo_size) {
				fifo_size = val;
				fifo_index = i;
			}
		}
		if (!fifo_index) {
			dev_err(hsotg->dev,
				"%s: No suitable fifo found\n", __func__);
			ret = -ENOMEM;
			goto error1;
		}
		epctrl &= ~(DXEPCTL_TXFNUM_LIMIT << DXEPCTL_TXFNUM_SHIFT);
		hsotg->fifo_map |= 1 << fifo_index;
		epctrl |= DXEPCTL_TXFNUM(fifo_index);
		hs_ep->fifo_index = fifo_index;
		hs_ep->fifo_size = fifo_size;
	}

	 
	if (index && !hs_ep->isochronous)
		epctrl |= DXEPCTL_SETD0PID;

	 

	if (hsotg->gadget.speed == USB_SPEED_FULL &&
	    hs_ep->isochronous && dir_in) {
		 
		u32 gsnpsid = dwc2_readl(hsotg, GSNPSID);

		if ((gsnpsid >= DWC2_CORE_REV_2_72a &&
		     gsnpsid <= DWC2_CORE_REV_4_00a) ||
		     gsnpsid == DWC2_FS_IOT_REV_1_00a ||
		     gsnpsid == DWC2_HS_IOT_REV_1_00a)
			epctrl |= DXEPCTL_CNAK;
	}

	dev_dbg(hsotg->dev, "%s: write DxEPCTL=0x%08x\n",
		__func__, epctrl);

	dwc2_writel(hsotg, epctrl, epctrl_reg);
	dev_dbg(hsotg->dev, "%s: read DxEPCTL=0x%08x\n",
		__func__, dwc2_readl(hsotg, epctrl_reg));

	 
	dwc2_hsotg_ctrl_epint(hsotg, index, dir_in, 1);

error1:
	spin_unlock_irqrestore(&hsotg->lock, flags);

error2:
	if (ret && using_desc_dma(hsotg) && hs_ep->desc_list) {
		dmam_free_coherent(hsotg->dev, desc_num *
			sizeof(struct dwc2_dma_desc),
			hs_ep->desc_list, hs_ep->desc_list_dma);
		hs_ep->desc_list = NULL;
	}

	return ret;
}

 
static int dwc2_hsotg_ep_disable(struct usb_ep *ep)
{
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	int dir_in = hs_ep->dir_in;
	int index = hs_ep->index;
	u32 epctrl_reg;
	u32 ctrl;

	dev_dbg(hsotg->dev, "%s(ep %p)\n", __func__, ep);

	if (ep == &hsotg->eps_out[0]->ep) {
		dev_err(hsotg->dev, "%s: called for ep0\n", __func__);
		return -EINVAL;
	}

	if (hsotg->op_state != OTG_STATE_B_PERIPHERAL) {
		dev_err(hsotg->dev, "%s: called in host mode?\n", __func__);
		return -EINVAL;
	}

	epctrl_reg = dir_in ? DIEPCTL(index) : DOEPCTL(index);

	ctrl = dwc2_readl(hsotg, epctrl_reg);

	if (ctrl & DXEPCTL_EPENA)
		dwc2_hsotg_ep_stop_xfr(hsotg, hs_ep);

	ctrl &= ~DXEPCTL_EPENA;
	ctrl &= ~DXEPCTL_USBACTEP;
	ctrl |= DXEPCTL_SNAK;

	dev_dbg(hsotg->dev, "%s: DxEPCTL=0x%08x\n", __func__, ctrl);
	dwc2_writel(hsotg, ctrl, epctrl_reg);

	 
	dwc2_hsotg_ctrl_epint(hsotg, hs_ep->index, hs_ep->dir_in, 0);

	 
	kill_all_requests(hsotg, hs_ep, -ESHUTDOWN);

	hsotg->fifo_map &= ~(1 << hs_ep->fifo_index);
	hs_ep->fifo_index = 0;
	hs_ep->fifo_size = 0;

	return 0;
}

static int dwc2_hsotg_ep_disable_lock(struct usb_ep *ep)
{
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hsotg = hs_ep->parent;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&hsotg->lock, flags);
	ret = dwc2_hsotg_ep_disable(ep);
	spin_unlock_irqrestore(&hsotg->lock, flags);
	return ret;
}

 
static bool on_list(struct dwc2_hsotg_ep *ep, struct dwc2_hsotg_req *test)
{
	struct dwc2_hsotg_req *req, *treq;

	list_for_each_entry_safe(req, treq, &ep->queue, queue) {
		if (req == test)
			return true;
	}

	return false;
}

 
static int dwc2_hsotg_ep_dequeue(struct usb_ep *ep, struct usb_request *req)
{
	struct dwc2_hsotg_req *hs_req = our_req(req);
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hs = hs_ep->parent;
	unsigned long flags;

	dev_dbg(hs->dev, "ep_dequeue(%p,%p)\n", ep, req);

	spin_lock_irqsave(&hs->lock, flags);

	if (!on_list(hs_ep, hs_req)) {
		spin_unlock_irqrestore(&hs->lock, flags);
		return -EINVAL;
	}

	 
	if (req == &hs_ep->req->req)
		dwc2_hsotg_ep_stop_xfr(hs, hs_ep);

	dwc2_hsotg_complete_request(hs, hs_ep, hs_req, -ECONNRESET);
	spin_unlock_irqrestore(&hs->lock, flags);

	return 0;
}

 
static int dwc2_gadget_ep_set_wedge(struct usb_ep *ep)
{
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hs = hs_ep->parent;

	unsigned long	flags;
	int		ret;

	spin_lock_irqsave(&hs->lock, flags);
	hs_ep->wedged = 1;
	ret = dwc2_hsotg_ep_sethalt(ep, 1, false);
	spin_unlock_irqrestore(&hs->lock, flags);

	return ret;
}

 
static int dwc2_hsotg_ep_sethalt(struct usb_ep *ep, int value, bool now)
{
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hs = hs_ep->parent;
	int index = hs_ep->index;
	u32 epreg;
	u32 epctl;
	u32 xfertype;

	dev_info(hs->dev, "%s(ep %p %s, %d)\n", __func__, ep, ep->name, value);

	if (index == 0) {
		if (value)
			dwc2_hsotg_stall_ep0(hs);
		else
			dev_warn(hs->dev,
				 "%s: can't clear halt on ep0\n", __func__);
		return 0;
	}

	if (hs_ep->isochronous) {
		dev_err(hs->dev, "%s is Isochronous Endpoint\n", ep->name);
		return -EINVAL;
	}

	if (!now && value && !list_empty(&hs_ep->queue)) {
		dev_dbg(hs->dev, "%s request is pending, cannot halt\n",
			ep->name);
		return -EAGAIN;
	}

	if (hs_ep->dir_in) {
		epreg = DIEPCTL(index);
		epctl = dwc2_readl(hs, epreg);

		if (value) {
			epctl |= DXEPCTL_STALL | DXEPCTL_SNAK;
			if (epctl & DXEPCTL_EPENA)
				epctl |= DXEPCTL_EPDIS;
		} else {
			epctl &= ~DXEPCTL_STALL;
			hs_ep->wedged = 0;
			xfertype = epctl & DXEPCTL_EPTYPE_MASK;
			if (xfertype == DXEPCTL_EPTYPE_BULK ||
			    xfertype == DXEPCTL_EPTYPE_INTERRUPT)
				epctl |= DXEPCTL_SETD0PID;
		}
		dwc2_writel(hs, epctl, epreg);
	} else {
		epreg = DOEPCTL(index);
		epctl = dwc2_readl(hs, epreg);

		if (value) {
			 
			dwc2_hsotg_en_gsint(hs, GINTSTS_GOUTNAKEFF);

			if (!(dwc2_readl(hs, GINTSTS) & GINTSTS_GOUTNAKEFF))
				dwc2_set_bit(hs, DCTL, DCTL_SGOUTNAK);
			 
		} else {
			epctl &= ~DXEPCTL_STALL;
			hs_ep->wedged = 0;
			xfertype = epctl & DXEPCTL_EPTYPE_MASK;
			if (xfertype == DXEPCTL_EPTYPE_BULK ||
			    xfertype == DXEPCTL_EPTYPE_INTERRUPT)
				epctl |= DXEPCTL_SETD0PID;
			dwc2_writel(hs, epctl, epreg);
		}
	}

	hs_ep->halted = value;
	return 0;
}

 
static int dwc2_hsotg_ep_sethalt_lock(struct usb_ep *ep, int value)
{
	struct dwc2_hsotg_ep *hs_ep = our_ep(ep);
	struct dwc2_hsotg *hs = hs_ep->parent;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&hs->lock, flags);
	ret = dwc2_hsotg_ep_sethalt(ep, value, false);
	spin_unlock_irqrestore(&hs->lock, flags);

	return ret;
}

static const struct usb_ep_ops dwc2_hsotg_ep_ops = {
	.enable		= dwc2_hsotg_ep_enable,
	.disable	= dwc2_hsotg_ep_disable_lock,
	.alloc_request	= dwc2_hsotg_ep_alloc_request,
	.free_request	= dwc2_hsotg_ep_free_request,
	.queue		= dwc2_hsotg_ep_queue_lock,
	.dequeue	= dwc2_hsotg_ep_dequeue,
	.set_halt	= dwc2_hsotg_ep_sethalt_lock,
	.set_wedge	= dwc2_gadget_ep_set_wedge,
	 
};

 
static void dwc2_hsotg_init(struct dwc2_hsotg *hsotg)
{
	 

	dwc2_writel(hsotg, DIEPMSK_TIMEOUTMSK | DIEPMSK_AHBERRMSK |
		    DIEPMSK_EPDISBLDMSK | DIEPMSK_XFERCOMPLMSK,
		    DIEPMSK);

	dwc2_writel(hsotg, DOEPMSK_SETUPMSK | DOEPMSK_AHBERRMSK |
		    DOEPMSK_EPDISBLDMSK | DOEPMSK_XFERCOMPLMSK,
		    DOEPMSK);

	dwc2_writel(hsotg, 0, DAINTMSK);

	 
	dwc2_set_bit(hsotg, DCTL, DCTL_SFTDISCON);

	 

	dev_dbg(hsotg->dev, "GRXFSIZ=0x%08x, GNPTXFSIZ=0x%08x\n",
		dwc2_readl(hsotg, GRXFSIZ),
		dwc2_readl(hsotg, GNPTXFSIZ));

	dwc2_hsotg_init_fifo(hsotg);

	if (using_dma(hsotg))
		dwc2_set_bit(hsotg, GAHBCFG, GAHBCFG_DMA_EN);
}

 
static int dwc2_hsotg_udc_start(struct usb_gadget *gadget,
				struct usb_gadget_driver *driver)
{
	struct dwc2_hsotg *hsotg = to_hsotg(gadget);
	unsigned long flags;
	int ret;

	if (!hsotg) {
		pr_err("%s: called with no device\n", __func__);
		return -ENODEV;
	}

	if (!driver) {
		dev_err(hsotg->dev, "%s: no driver\n", __func__);
		return -EINVAL;
	}

	if (driver->max_speed < USB_SPEED_FULL)
		dev_err(hsotg->dev, "%s: bad speed\n", __func__);

	if (!driver->setup) {
		dev_err(hsotg->dev, "%s: missing entry points\n", __func__);
		return -EINVAL;
	}

	WARN_ON(hsotg->driver);

	hsotg->driver = driver;
	hsotg->gadget.dev.of_node = hsotg->dev->of_node;
	hsotg->gadget.speed = USB_SPEED_UNKNOWN;

	if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL) {
		ret = dwc2_lowlevel_hw_enable(hsotg);
		if (ret)
			goto err;
	}

	if (!IS_ERR_OR_NULL(hsotg->uphy))
		otg_set_peripheral(hsotg->uphy->otg, &hsotg->gadget);

	spin_lock_irqsave(&hsotg->lock, flags);
	if (dwc2_hw_is_device(hsotg)) {
		dwc2_hsotg_init(hsotg);
		dwc2_hsotg_core_init_disconnected(hsotg, false);
	}

	hsotg->enabled = 0;
	spin_unlock_irqrestore(&hsotg->lock, flags);

	gadget->sg_supported = using_desc_dma(hsotg);
	dev_info(hsotg->dev, "bound driver %s\n", driver->driver.name);

	return 0;

err:
	hsotg->driver = NULL;
	return ret;
}

 
static int dwc2_hsotg_udc_stop(struct usb_gadget *gadget)
{
	struct dwc2_hsotg *hsotg = to_hsotg(gadget);
	unsigned long flags;
	int ep;

	if (!hsotg)
		return -ENODEV;

	 
	for (ep = 1; ep < hsotg->num_of_eps; ep++) {
		if (hsotg->eps_in[ep])
			dwc2_hsotg_ep_disable_lock(&hsotg->eps_in[ep]->ep);
		if (hsotg->eps_out[ep])
			dwc2_hsotg_ep_disable_lock(&hsotg->eps_out[ep]->ep);
	}

	spin_lock_irqsave(&hsotg->lock, flags);

	hsotg->driver = NULL;
	hsotg->gadget.speed = USB_SPEED_UNKNOWN;
	hsotg->enabled = 0;

	spin_unlock_irqrestore(&hsotg->lock, flags);

	if (!IS_ERR_OR_NULL(hsotg->uphy))
		otg_set_peripheral(hsotg->uphy->otg, NULL);

	if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL)
		dwc2_lowlevel_hw_disable(hsotg);

	return 0;
}

 
static int dwc2_hsotg_gadget_getframe(struct usb_gadget *gadget)
{
	return dwc2_hsotg_read_frameno(to_hsotg(gadget));
}

 
static int dwc2_hsotg_set_selfpowered(struct usb_gadget *gadget,
				      int is_selfpowered)
{
	struct dwc2_hsotg *hsotg = to_hsotg(gadget);
	unsigned long flags;

	spin_lock_irqsave(&hsotg->lock, flags);
	gadget->is_selfpowered = !!is_selfpowered;
	spin_unlock_irqrestore(&hsotg->lock, flags);

	return 0;
}

 
static int dwc2_hsotg_pullup(struct usb_gadget *gadget, int is_on)
{
	struct dwc2_hsotg *hsotg = to_hsotg(gadget);
	unsigned long flags;

	dev_dbg(hsotg->dev, "%s: is_on: %d op_state: %d\n", __func__, is_on,
		hsotg->op_state);

	 
	if (hsotg->op_state != OTG_STATE_B_PERIPHERAL) {
		hsotg->enabled = is_on;
		return 0;
	}

	spin_lock_irqsave(&hsotg->lock, flags);
	if (is_on) {
		hsotg->enabled = 1;
		dwc2_hsotg_core_init_disconnected(hsotg, false);
		 
		dwc2_enable_acg(hsotg);
		dwc2_hsotg_core_connect(hsotg);
	} else {
		dwc2_hsotg_core_disconnect(hsotg);
		dwc2_hsotg_disconnect(hsotg);
		hsotg->enabled = 0;
	}

	hsotg->gadget.speed = USB_SPEED_UNKNOWN;
	spin_unlock_irqrestore(&hsotg->lock, flags);

	return 0;
}

static int dwc2_hsotg_vbus_session(struct usb_gadget *gadget, int is_active)
{
	struct dwc2_hsotg *hsotg = to_hsotg(gadget);
	unsigned long flags;

	dev_dbg(hsotg->dev, "%s: is_active: %d\n", __func__, is_active);
	spin_lock_irqsave(&hsotg->lock, flags);

	 
	if (hsotg->lx_state == DWC2_L2 && hsotg->in_ppd)
		 
		dwc2_exit_partial_power_down(hsotg, 0, false);

	if (is_active) {
		hsotg->op_state = OTG_STATE_B_PERIPHERAL;

		dwc2_hsotg_core_init_disconnected(hsotg, false);
		if (hsotg->enabled) {
			 
			dwc2_enable_acg(hsotg);
			dwc2_hsotg_core_connect(hsotg);
		}
	} else {
		dwc2_hsotg_core_disconnect(hsotg);
		dwc2_hsotg_disconnect(hsotg);
	}

	spin_unlock_irqrestore(&hsotg->lock, flags);
	return 0;
}

 
static int dwc2_hsotg_vbus_draw(struct usb_gadget *gadget, unsigned int mA)
{
	struct dwc2_hsotg *hsotg = to_hsotg(gadget);

	if (IS_ERR_OR_NULL(hsotg->uphy))
		return -ENOTSUPP;
	return usb_phy_set_power(hsotg->uphy, mA);
}

static void dwc2_gadget_set_speed(struct usb_gadget *g, enum usb_device_speed speed)
{
	struct dwc2_hsotg *hsotg = to_hsotg(g);
	unsigned long		flags;

	spin_lock_irqsave(&hsotg->lock, flags);
	switch (speed) {
	case USB_SPEED_HIGH:
		hsotg->params.speed = DWC2_SPEED_PARAM_HIGH;
		break;
	case USB_SPEED_FULL:
		hsotg->params.speed = DWC2_SPEED_PARAM_FULL;
		break;
	case USB_SPEED_LOW:
		hsotg->params.speed = DWC2_SPEED_PARAM_LOW;
		break;
	default:
		dev_err(hsotg->dev, "invalid speed (%d)\n", speed);
	}
	spin_unlock_irqrestore(&hsotg->lock, flags);
}

static const struct usb_gadget_ops dwc2_hsotg_gadget_ops = {
	.get_frame	= dwc2_hsotg_gadget_getframe,
	.set_selfpowered	= dwc2_hsotg_set_selfpowered,
	.udc_start		= dwc2_hsotg_udc_start,
	.udc_stop		= dwc2_hsotg_udc_stop,
	.pullup                 = dwc2_hsotg_pullup,
	.udc_set_speed		= dwc2_gadget_set_speed,
	.vbus_session		= dwc2_hsotg_vbus_session,
	.vbus_draw		= dwc2_hsotg_vbus_draw,
};

 
static void dwc2_hsotg_initep(struct dwc2_hsotg *hsotg,
			      struct dwc2_hsotg_ep *hs_ep,
				       int epnum,
				       bool dir_in)
{
	char *dir;

	if (epnum == 0)
		dir = "";
	else if (dir_in)
		dir = "in";
	else
		dir = "out";

	hs_ep->dir_in = dir_in;
	hs_ep->index = epnum;

	snprintf(hs_ep->name, sizeof(hs_ep->name), "ep%d%s", epnum, dir);

	INIT_LIST_HEAD(&hs_ep->queue);
	INIT_LIST_HEAD(&hs_ep->ep.ep_list);

	 
	if (epnum)
		list_add_tail(&hs_ep->ep.ep_list, &hsotg->gadget.ep_list);

	hs_ep->parent = hsotg;
	hs_ep->ep.name = hs_ep->name;

	if (hsotg->params.speed == DWC2_SPEED_PARAM_LOW)
		usb_ep_set_maxpacket_limit(&hs_ep->ep, 8);
	else
		usb_ep_set_maxpacket_limit(&hs_ep->ep,
					   epnum ? 1024 : EP0_MPS_LIMIT);
	hs_ep->ep.ops = &dwc2_hsotg_ep_ops;

	if (epnum == 0) {
		hs_ep->ep.caps.type_control = true;
	} else {
		if (hsotg->params.speed != DWC2_SPEED_PARAM_LOW) {
			hs_ep->ep.caps.type_iso = true;
			hs_ep->ep.caps.type_bulk = true;
		}
		hs_ep->ep.caps.type_int = true;
	}

	if (dir_in)
		hs_ep->ep.caps.dir_in = true;
	else
		hs_ep->ep.caps.dir_out = true;

	 

	if (using_dma(hsotg)) {
		u32 next = DXEPCTL_NEXTEP((epnum + 1) % 15);

		if (dir_in)
			dwc2_writel(hsotg, next, DIEPCTL(epnum));
		else
			dwc2_writel(hsotg, next, DOEPCTL(epnum));
	}
}

 
static int dwc2_hsotg_hw_cfg(struct dwc2_hsotg *hsotg)
{
	u32 cfg;
	u32 ep_type;
	u32 i;

	 

	hsotg->num_of_eps = hsotg->hw_params.num_dev_ep;

	 
	hsotg->num_of_eps++;

	hsotg->eps_in[0] = devm_kzalloc(hsotg->dev,
					sizeof(struct dwc2_hsotg_ep),
					GFP_KERNEL);
	if (!hsotg->eps_in[0])
		return -ENOMEM;
	 
	hsotg->eps_out[0] = hsotg->eps_in[0];

	cfg = hsotg->hw_params.dev_ep_dirs;
	for (i = 1, cfg >>= 2; i < hsotg->num_of_eps; i++, cfg >>= 2) {
		ep_type = cfg & 3;
		 
		if (!(ep_type & 2)) {
			hsotg->eps_in[i] = devm_kzalloc(hsotg->dev,
				sizeof(struct dwc2_hsotg_ep), GFP_KERNEL);
			if (!hsotg->eps_in[i])
				return -ENOMEM;
		}
		 
		if (!(ep_type & 1)) {
			hsotg->eps_out[i] = devm_kzalloc(hsotg->dev,
				sizeof(struct dwc2_hsotg_ep), GFP_KERNEL);
			if (!hsotg->eps_out[i])
				return -ENOMEM;
		}
	}

	hsotg->fifo_mem = hsotg->hw_params.total_fifo_size;
	hsotg->dedicated_fifos = hsotg->hw_params.en_multiple_tx_fifo;

	dev_info(hsotg->dev, "EPs: %d, %s fifos, %d entries in SPRAM\n",
		 hsotg->num_of_eps,
		 hsotg->dedicated_fifos ? "dedicated" : "shared",
		 hsotg->fifo_mem);
	return 0;
}

 
static void dwc2_hsotg_dump(struct dwc2_hsotg *hsotg)
{
#ifdef DEBUG
	struct device *dev = hsotg->dev;
	u32 val;
	int idx;

	dev_info(dev, "DCFG=0x%08x, DCTL=0x%08x, DIEPMSK=%08x\n",
		 dwc2_readl(hsotg, DCFG), dwc2_readl(hsotg, DCTL),
		 dwc2_readl(hsotg, DIEPMSK));

	dev_info(dev, "GAHBCFG=0x%08x, GHWCFG1=0x%08x\n",
		 dwc2_readl(hsotg, GAHBCFG), dwc2_readl(hsotg, GHWCFG1));

	dev_info(dev, "GRXFSIZ=0x%08x, GNPTXFSIZ=0x%08x\n",
		 dwc2_readl(hsotg, GRXFSIZ), dwc2_readl(hsotg, GNPTXFSIZ));

	 

	for (idx = 1; idx < hsotg->num_of_eps; idx++) {
		val = dwc2_readl(hsotg, DPTXFSIZN(idx));
		dev_info(dev, "DPTx[%d] FSize=%d, StAddr=0x%08x\n", idx,
			 val >> FIFOSIZE_DEPTH_SHIFT,
			 val & FIFOSIZE_STARTADDR_MASK);
	}

	for (idx = 0; idx < hsotg->num_of_eps; idx++) {
		dev_info(dev,
			 "ep%d-in: EPCTL=0x%08x, SIZ=0x%08x, DMA=0x%08x\n", idx,
			 dwc2_readl(hsotg, DIEPCTL(idx)),
			 dwc2_readl(hsotg, DIEPTSIZ(idx)),
			 dwc2_readl(hsotg, DIEPDMA(idx)));

		val = dwc2_readl(hsotg, DOEPCTL(idx));
		dev_info(dev,
			 "ep%d-out: EPCTL=0x%08x, SIZ=0x%08x, DMA=0x%08x\n",
			 idx, dwc2_readl(hsotg, DOEPCTL(idx)),
			 dwc2_readl(hsotg, DOEPTSIZ(idx)),
			 dwc2_readl(hsotg, DOEPDMA(idx)));
	}

	dev_info(dev, "DVBUSDIS=0x%08x, DVBUSPULSE=%08x\n",
		 dwc2_readl(hsotg, DVBUSDIS), dwc2_readl(hsotg, DVBUSPULSE));
#endif
}

 
int dwc2_gadget_init(struct dwc2_hsotg *hsotg)
{
	struct device *dev = hsotg->dev;
	int epnum;
	int ret;

	 
	dev_dbg(dev, "NonPeriodic TXFIFO size: %d\n",
		hsotg->params.g_np_tx_fifo_size);
	dev_dbg(dev, "RXFIFO size: %d\n", hsotg->params.g_rx_fifo_size);

	switch (hsotg->params.speed) {
	case DWC2_SPEED_PARAM_LOW:
		hsotg->gadget.max_speed = USB_SPEED_LOW;
		break;
	case DWC2_SPEED_PARAM_FULL:
		hsotg->gadget.max_speed = USB_SPEED_FULL;
		break;
	default:
		hsotg->gadget.max_speed = USB_SPEED_HIGH;
		break;
	}

	hsotg->gadget.ops = &dwc2_hsotg_gadget_ops;
	hsotg->gadget.name = dev_name(dev);
	hsotg->gadget.otg_caps = &hsotg->params.otg_caps;
	hsotg->remote_wakeup_allowed = 0;

	if (hsotg->params.lpm)
		hsotg->gadget.lpm_capable = true;

	if (hsotg->dr_mode == USB_DR_MODE_OTG)
		hsotg->gadget.is_otg = 1;
	else if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL)
		hsotg->op_state = OTG_STATE_B_PERIPHERAL;

	ret = dwc2_hsotg_hw_cfg(hsotg);
	if (ret) {
		dev_err(hsotg->dev, "Hardware configuration failed: %d\n", ret);
		return ret;
	}

	hsotg->ctrl_buff = devm_kzalloc(hsotg->dev,
			DWC2_CTRL_BUFF_SIZE, GFP_KERNEL);
	if (!hsotg->ctrl_buff)
		return -ENOMEM;

	hsotg->ep0_buff = devm_kzalloc(hsotg->dev,
			DWC2_CTRL_BUFF_SIZE, GFP_KERNEL);
	if (!hsotg->ep0_buff)
		return -ENOMEM;

	if (using_desc_dma(hsotg)) {
		ret = dwc2_gadget_alloc_ctrl_desc_chains(hsotg);
		if (ret < 0)
			return ret;
	}

	ret = devm_request_irq(hsotg->dev, hsotg->irq, dwc2_hsotg_irq,
			       IRQF_SHARED, dev_name(hsotg->dev), hsotg);
	if (ret < 0) {
		dev_err(dev, "cannot claim IRQ for gadget\n");
		return ret;
	}

	 

	if (hsotg->num_of_eps == 0) {
		dev_err(dev, "wrong number of EPs (zero)\n");
		return -EINVAL;
	}

	 

	INIT_LIST_HEAD(&hsotg->gadget.ep_list);
	hsotg->gadget.ep0 = &hsotg->eps_out[0]->ep;

	 

	hsotg->ctrl_req = dwc2_hsotg_ep_alloc_request(&hsotg->eps_out[0]->ep,
						     GFP_KERNEL);
	if (!hsotg->ctrl_req) {
		dev_err(dev, "failed to allocate ctrl req\n");
		return -ENOMEM;
	}

	 
	for (epnum = 0; epnum < hsotg->num_of_eps; epnum++) {
		if (hsotg->eps_in[epnum])
			dwc2_hsotg_initep(hsotg, hsotg->eps_in[epnum],
					  epnum, 1);
		if (hsotg->eps_out[epnum])
			dwc2_hsotg_initep(hsotg, hsotg->eps_out[epnum],
					  epnum, 0);
	}

	dwc2_hsotg_dump(hsotg);

	return 0;
}

 
int dwc2_hsotg_remove(struct dwc2_hsotg *hsotg)
{
	usb_del_gadget_udc(&hsotg->gadget);
	dwc2_hsotg_ep_free_request(&hsotg->eps_out[0]->ep, hsotg->ctrl_req);

	return 0;
}

int dwc2_hsotg_suspend(struct dwc2_hsotg *hsotg)
{
	unsigned long flags;

	if (hsotg->lx_state != DWC2_L0)
		return 0;

	if (hsotg->driver) {
		int ep;

		dev_info(hsotg->dev, "suspending usb gadget %s\n",
			 hsotg->driver->driver.name);

		spin_lock_irqsave(&hsotg->lock, flags);
		if (hsotg->enabled)
			dwc2_hsotg_core_disconnect(hsotg);
		dwc2_hsotg_disconnect(hsotg);
		hsotg->gadget.speed = USB_SPEED_UNKNOWN;
		spin_unlock_irqrestore(&hsotg->lock, flags);

		for (ep = 1; ep < hsotg->num_of_eps; ep++) {
			if (hsotg->eps_in[ep])
				dwc2_hsotg_ep_disable_lock(&hsotg->eps_in[ep]->ep);
			if (hsotg->eps_out[ep])
				dwc2_hsotg_ep_disable_lock(&hsotg->eps_out[ep]->ep);
		}
	}

	return 0;
}

int dwc2_hsotg_resume(struct dwc2_hsotg *hsotg)
{
	unsigned long flags;

	if (hsotg->lx_state == DWC2_L2)
		return 0;

	if (hsotg->driver) {
		dev_info(hsotg->dev, "resuming usb gadget %s\n",
			 hsotg->driver->driver.name);

		spin_lock_irqsave(&hsotg->lock, flags);
		dwc2_hsotg_core_init_disconnected(hsotg, false);
		if (hsotg->enabled) {
			 
			dwc2_enable_acg(hsotg);
			dwc2_hsotg_core_connect(hsotg);
		}
		spin_unlock_irqrestore(&hsotg->lock, flags);
	}

	return 0;
}

 
int dwc2_backup_device_registers(struct dwc2_hsotg *hsotg)
{
	struct dwc2_dregs_backup *dr;
	int i;

	dev_dbg(hsotg->dev, "%s\n", __func__);

	 
	dr = &hsotg->dr_backup;

	dr->dcfg = dwc2_readl(hsotg, DCFG);
	dr->dctl = dwc2_readl(hsotg, DCTL);
	dr->daintmsk = dwc2_readl(hsotg, DAINTMSK);
	dr->diepmsk = dwc2_readl(hsotg, DIEPMSK);
	dr->doepmsk = dwc2_readl(hsotg, DOEPMSK);

	for (i = 0; i < hsotg->num_of_eps; i++) {
		 
		dr->diepctl[i] = dwc2_readl(hsotg, DIEPCTL(i));

		 
		if (dr->diepctl[i] & DXEPCTL_DPID)
			dr->diepctl[i] |= DXEPCTL_SETD1PID;
		else
			dr->diepctl[i] |= DXEPCTL_SETD0PID;

		dr->dieptsiz[i] = dwc2_readl(hsotg, DIEPTSIZ(i));
		dr->diepdma[i] = dwc2_readl(hsotg, DIEPDMA(i));

		 
		dr->doepctl[i] = dwc2_readl(hsotg, DOEPCTL(i));

		 
		if (dr->doepctl[i] & DXEPCTL_DPID)
			dr->doepctl[i] |= DXEPCTL_SETD1PID;
		else
			dr->doepctl[i] |= DXEPCTL_SETD0PID;

		dr->doeptsiz[i] = dwc2_readl(hsotg, DOEPTSIZ(i));
		dr->doepdma[i] = dwc2_readl(hsotg, DOEPDMA(i));
		dr->dtxfsiz[i] = dwc2_readl(hsotg, DPTXFSIZN(i));
	}
	dr->valid = true;
	return 0;
}

 
int dwc2_restore_device_registers(struct dwc2_hsotg *hsotg, int remote_wakeup)
{
	struct dwc2_dregs_backup *dr;
	int i;

	dev_dbg(hsotg->dev, "%s\n", __func__);

	 
	dr = &hsotg->dr_backup;
	if (!dr->valid) {
		dev_err(hsotg->dev, "%s: no device registers to restore\n",
			__func__);
		return -EINVAL;
	}
	dr->valid = false;

	if (!remote_wakeup)
		dwc2_writel(hsotg, dr->dctl, DCTL);

	dwc2_writel(hsotg, dr->daintmsk, DAINTMSK);
	dwc2_writel(hsotg, dr->diepmsk, DIEPMSK);
	dwc2_writel(hsotg, dr->doepmsk, DOEPMSK);

	for (i = 0; i < hsotg->num_of_eps; i++) {
		 
		dwc2_writel(hsotg, dr->dieptsiz[i], DIEPTSIZ(i));
		dwc2_writel(hsotg, dr->diepdma[i], DIEPDMA(i));
		dwc2_writel(hsotg, dr->doeptsiz[i], DOEPTSIZ(i));
		 
		if (using_desc_dma(hsotg) &&
		    (dr->diepctl[i] & DXEPCTL_EPENA))
			dr->diepdma[i] = hsotg->eps_in[i]->desc_list_dma;
		dwc2_writel(hsotg, dr->dtxfsiz[i], DPTXFSIZN(i));
		dwc2_writel(hsotg, dr->diepctl[i], DIEPCTL(i));
		 
		dwc2_writel(hsotg, dr->doeptsiz[i], DOEPTSIZ(i));
		 
		if (using_desc_dma(hsotg) &&
		    (dr->doepctl[i] & DXEPCTL_EPENA))
			dr->doepdma[i] = hsotg->eps_out[i]->desc_list_dma;
		dwc2_writel(hsotg, dr->doepdma[i], DOEPDMA(i));
		dwc2_writel(hsotg, dr->doepctl[i], DOEPCTL(i));
	}

	return 0;
}

 
void dwc2_gadget_init_lpm(struct dwc2_hsotg *hsotg)
{
	u32 val;

	if (!hsotg->params.lpm)
		return;

	val = GLPMCFG_LPMCAP | GLPMCFG_APPL1RES;
	val |= hsotg->params.hird_threshold_en ? GLPMCFG_HIRD_THRES_EN : 0;
	val |= hsotg->params.lpm_clock_gating ? GLPMCFG_ENBLSLPM : 0;
	val |= hsotg->params.hird_threshold << GLPMCFG_HIRD_THRES_SHIFT;
	val |= hsotg->params.besl ? GLPMCFG_ENBESL : 0;
	val |= GLPMCFG_LPM_REJECT_CTRL_CONTROL;
	val |= GLPMCFG_LPM_ACCEPT_CTRL_ISOC;
	dwc2_writel(hsotg, val, GLPMCFG);
	dev_dbg(hsotg->dev, "GLPMCFG=0x%08x\n", dwc2_readl(hsotg, GLPMCFG));

	 
	if (hsotg->params.service_interval)
		dwc2_set_bit(hsotg, GINTMSK2, GINTMSK2_WKUP_ALERT_INT_MSK);
}

 
void dwc2_gadget_program_ref_clk(struct dwc2_hsotg *hsotg)
{
	u32 val = 0;

	val |= GREFCLK_REF_CLK_MODE;
	val |= hsotg->params.ref_clk_per << GREFCLK_REFCLKPER_SHIFT;
	val |= hsotg->params.sof_cnt_wkup_alert <<
	       GREFCLK_SOF_CNT_WKUP_ALERT_SHIFT;

	dwc2_writel(hsotg, val, GREFCLK);
	dev_dbg(hsotg->dev, "GREFCLK=0x%08x\n", dwc2_readl(hsotg, GREFCLK));
}

 
int dwc2_gadget_enter_hibernation(struct dwc2_hsotg *hsotg)
{
	u32 gpwrdn;
	int ret = 0;

	 
	hsotg->lx_state = DWC2_L2;
	dev_dbg(hsotg->dev, "Start of hibernation completed\n");
	ret = dwc2_backup_global_registers(hsotg);
	if (ret) {
		dev_err(hsotg->dev, "%s: failed to backup global registers\n",
			__func__);
		return ret;
	}
	ret = dwc2_backup_device_registers(hsotg);
	if (ret) {
		dev_err(hsotg->dev, "%s: failed to backup device registers\n",
			__func__);
		return ret;
	}

	gpwrdn = GPWRDN_PWRDNRSTN;
	gpwrdn |= GPWRDN_PMUACTV;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	 
	hsotg->hibernated = 1;

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn |= GPWRDN_PMUINTSEL;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn |= GPWRDN_RST_DET_MSK;
	gpwrdn |= GPWRDN_LNSTSCHG_MSK;
	gpwrdn |= GPWRDN_STS_CHGINT_MSK;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn |= GPWRDN_PWRDNCLMP;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn |= GPWRDN_PWRDNSWTCH;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	 
	hsotg->gr_backup.gpwrdn = dwc2_readl(hsotg, GPWRDN);
	dev_dbg(hsotg->dev, "Hibernation completed\n");

	return ret;
}

 
int dwc2_gadget_exit_hibernation(struct dwc2_hsotg *hsotg,
				 int rem_wakeup, int reset)
{
	u32 pcgcctl;
	u32 gpwrdn;
	u32 dctl;
	int ret = 0;
	struct dwc2_gregs_backup *gr;
	struct dwc2_dregs_backup *dr;

	gr = &hsotg->gr_backup;
	dr = &hsotg->dr_backup;

	if (!hsotg->hibernated) {
		dev_dbg(hsotg->dev, "Already exited from Hibernation\n");
		return 1;
	}
	dev_dbg(hsotg->dev,
		"%s: called with rem_wakeup = %d reset = %d\n",
		__func__, rem_wakeup, reset);

	dwc2_hib_restore_common(hsotg, rem_wakeup, 0);

	if (!reset) {
		 
		dwc2_writel(hsotg, 0xffffffff, GINTSTS);
	}

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn &= ~GPWRDN_RESTORE;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);
	udelay(10);

	if (!rem_wakeup) {
		pcgcctl = dwc2_readl(hsotg, PCGCTL);
		pcgcctl &= ~PCGCTL_RSTPDWNMODULE;
		dwc2_writel(hsotg, pcgcctl, PCGCTL);
	}

	 
	dwc2_writel(hsotg, gr->gusbcfg, GUSBCFG);
	dwc2_writel(hsotg, dr->dcfg, DCFG);
	dwc2_writel(hsotg, dr->dctl, DCTL);

	 
	if (reset)
		dwc2_clear_bit(hsotg, DCFG, DCFG_DEVADDR_MASK);

	 
	gpwrdn = dwc2_readl(hsotg, GPWRDN);
	gpwrdn &= ~GPWRDN_PMUACTV;
	dwc2_writel(hsotg, gpwrdn, GPWRDN);

	if (rem_wakeup) {
		udelay(10);
		 
		dwc2_writel(hsotg, dr->dctl | DCTL_RMTWKUPSIG, DCTL);
	} else {
		udelay(50);
		 
		dctl = dwc2_readl(hsotg, DCTL);
		dctl |= DCTL_PWRONPRGDONE;
		dwc2_writel(hsotg, dctl, DCTL);
	}
	 
	mdelay(2);
	 
	dwc2_writel(hsotg, 0xffffffff, GINTSTS);

	 
	ret = dwc2_restore_global_registers(hsotg);
	if (ret) {
		dev_err(hsotg->dev, "%s: failed to restore registers\n",
			__func__);
		return ret;
	}

	 
	ret = dwc2_restore_device_registers(hsotg, rem_wakeup);
	if (ret) {
		dev_err(hsotg->dev, "%s: failed to restore device registers\n",
			__func__);
		return ret;
	}

	if (rem_wakeup) {
		mdelay(10);
		dctl = dwc2_readl(hsotg, DCTL);
		dctl &= ~DCTL_RMTWKUPSIG;
		dwc2_writel(hsotg, dctl, DCTL);
	}

	hsotg->hibernated = 0;
	hsotg->lx_state = DWC2_L0;
	dev_dbg(hsotg->dev, "Hibernation recovery completes here\n");

	return ret;
}

 
int dwc2_gadget_enter_partial_power_down(struct dwc2_hsotg *hsotg)
{
	u32 pcgcctl;
	int ret = 0;

	dev_dbg(hsotg->dev, "Entering device partial power down started.\n");

	 
	ret = dwc2_backup_global_registers(hsotg);
	if (ret) {
		dev_err(hsotg->dev, "%s: failed to backup global registers\n",
			__func__);
		return ret;
	}

	ret = dwc2_backup_device_registers(hsotg);
	if (ret) {
		dev_err(hsotg->dev, "%s: failed to backup device registers\n",
			__func__);
		return ret;
	}

	 
	dwc2_writel(hsotg, 0xffffffff, GINTSTS);

	 
	pcgcctl = dwc2_readl(hsotg, PCGCTL);

	pcgcctl |= PCGCTL_PWRCLMP;
	dwc2_writel(hsotg, pcgcctl, PCGCTL);
	udelay(5);

	pcgcctl |= PCGCTL_RSTPDWNMODULE;
	dwc2_writel(hsotg, pcgcctl, PCGCTL);
	udelay(5);

	pcgcctl |= PCGCTL_STOPPCLK;
	dwc2_writel(hsotg, pcgcctl, PCGCTL);

	 
	hsotg->in_ppd = 1;
	hsotg->lx_state = DWC2_L2;

	dev_dbg(hsotg->dev, "Entering device partial power down completed.\n");

	return ret;
}

 
int dwc2_gadget_exit_partial_power_down(struct dwc2_hsotg *hsotg,
					bool restore)
{
	u32 pcgcctl;
	u32 dctl;
	struct dwc2_dregs_backup *dr;
	int ret = 0;

	dr = &hsotg->dr_backup;

	dev_dbg(hsotg->dev, "Exiting device partial Power Down started.\n");

	pcgcctl = dwc2_readl(hsotg, PCGCTL);
	pcgcctl &= ~PCGCTL_STOPPCLK;
	dwc2_writel(hsotg, pcgcctl, PCGCTL);

	pcgcctl = dwc2_readl(hsotg, PCGCTL);
	pcgcctl &= ~PCGCTL_PWRCLMP;
	dwc2_writel(hsotg, pcgcctl, PCGCTL);

	pcgcctl = dwc2_readl(hsotg, PCGCTL);
	pcgcctl &= ~PCGCTL_RSTPDWNMODULE;
	dwc2_writel(hsotg, pcgcctl, PCGCTL);

	udelay(100);
	if (restore) {
		ret = dwc2_restore_global_registers(hsotg);
		if (ret) {
			dev_err(hsotg->dev, "%s: failed to restore registers\n",
				__func__);
			return ret;
		}
		 
		dwc2_writel(hsotg, dr->dcfg, DCFG);

		ret = dwc2_restore_device_registers(hsotg, 0);
		if (ret) {
			dev_err(hsotg->dev, "%s: failed to restore device registers\n",
				__func__);
			return ret;
		}
	}

	 
	dctl = dwc2_readl(hsotg, DCTL);
	dctl |= DCTL_PWRONPRGDONE;
	dwc2_writel(hsotg, dctl, DCTL);

	 
	hsotg->in_ppd = 0;
	hsotg->lx_state = DWC2_L0;

	dev_dbg(hsotg->dev, "Exiting device partial Power Down completed.\n");
	return ret;
}

 
void dwc2_gadget_enter_clock_gating(struct dwc2_hsotg *hsotg)
{
	u32 pcgctl;

	dev_dbg(hsotg->dev, "Entering device clock gating.\n");

	 
	pcgctl = dwc2_readl(hsotg, PCGCTL);
	pcgctl |= PCGCTL_STOPPCLK;
	dwc2_writel(hsotg, pcgctl, PCGCTL);
	udelay(5);

	 
	pcgctl = dwc2_readl(hsotg, PCGCTL);
	pcgctl |= PCGCTL_GATEHCLK;
	dwc2_writel(hsotg, pcgctl, PCGCTL);
	udelay(5);

	hsotg->lx_state = DWC2_L2;
	hsotg->bus_suspended = true;
}

 
void dwc2_gadget_exit_clock_gating(struct dwc2_hsotg *hsotg, int rem_wakeup)
{
	u32 pcgctl;
	u32 dctl;

	dev_dbg(hsotg->dev, "Exiting device clock gating.\n");

	 
	pcgctl = dwc2_readl(hsotg, PCGCTL);
	pcgctl &= ~PCGCTL_GATEHCLK;
	dwc2_writel(hsotg, pcgctl, PCGCTL);
	udelay(5);

	 
	pcgctl = dwc2_readl(hsotg, PCGCTL);
	pcgctl &= ~PCGCTL_STOPPCLK;
	dwc2_writel(hsotg, pcgctl, PCGCTL);
	udelay(5);

	if (rem_wakeup) {
		 
		dctl = dwc2_readl(hsotg, DCTL);
		dctl |= DCTL_RMTWKUPSIG;
		dwc2_writel(hsotg, dctl, DCTL);
	}

	 
	call_gadget(hsotg, resume);
	hsotg->lx_state = DWC2_L0;
	hsotg->bus_suspended = false;
}
