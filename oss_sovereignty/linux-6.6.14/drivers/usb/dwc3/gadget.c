
 

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>

#include "debug.h"
#include "core.h"
#include "gadget.h"
#include "io.h"

#define DWC3_ALIGN_FRAME(d, n)	(((d)->frame_number + ((d)->interval * (n))) \
					& ~((d)->interval - 1))

 
int dwc3_gadget_set_test_mode(struct dwc3 *dwc, int mode)
{
	u32		reg;

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~DWC3_DCTL_TSTCTRL_MASK;

	switch (mode) {
	case USB_TEST_J:
	case USB_TEST_K:
	case USB_TEST_SE0_NAK:
	case USB_TEST_PACKET:
	case USB_TEST_FORCE_ENABLE:
		reg |= mode << 1;
		break;
	default:
		return -EINVAL;
	}

	dwc3_gadget_dctl_write_safe(dwc, reg);

	return 0;
}

 
int dwc3_gadget_get_link_state(struct dwc3 *dwc)
{
	u32		reg;

	reg = dwc3_readl(dwc->regs, DWC3_DSTS);

	return DWC3_DSTS_USBLNKST(reg);
}

 
int dwc3_gadget_set_link_state(struct dwc3 *dwc, enum dwc3_link_state state)
{
	int		retries = 10000;
	u32		reg;

	 
	if (!DWC3_VER_IS_PRIOR(DWC3, 194A)) {
		while (--retries) {
			reg = dwc3_readl(dwc->regs, DWC3_DSTS);
			if (reg & DWC3_DSTS_DCNRD)
				udelay(5);
			else
				break;
		}

		if (retries <= 0)
			return -ETIMEDOUT;
	}

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~DWC3_DCTL_ULSTCHNGREQ_MASK;

	 
	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	 
	reg |= DWC3_DCTL_ULSTCHNGREQ(state);
	dwc3_writel(dwc->regs, DWC3_DCTL, reg);

	 
	if (!DWC3_VER_IS_PRIOR(DWC3, 194A))
		return 0;

	 
	retries = 10000;
	while (--retries) {
		reg = dwc3_readl(dwc->regs, DWC3_DSTS);

		if (DWC3_DSTS_USBLNKST(reg) == state)
			return 0;

		udelay(5);
	}

	return -ETIMEDOUT;
}

static void dwc3_ep0_reset_state(struct dwc3 *dwc)
{
	unsigned int	dir;

	if (dwc->ep0state != EP0_SETUP_PHASE) {
		dir = !!dwc->ep0_expect_in;
		if (dwc->ep0state == EP0_DATA_PHASE)
			dwc3_ep0_end_control_data(dwc, dwc->eps[dir]);
		else
			dwc3_ep0_end_control_data(dwc, dwc->eps[!dir]);

		dwc->eps[0]->trb_enqueue = 0;
		dwc->eps[1]->trb_enqueue = 0;

		dwc3_ep0_stall_and_restart(dwc);
	}
}

 
static void dwc3_ep_inc_trb(u8 *index)
{
	(*index)++;
	if (*index == (DWC3_TRB_NUM - 1))
		*index = 0;
}

 
static void dwc3_ep_inc_enq(struct dwc3_ep *dep)
{
	dwc3_ep_inc_trb(&dep->trb_enqueue);
}

 
static void dwc3_ep_inc_deq(struct dwc3_ep *dep)
{
	dwc3_ep_inc_trb(&dep->trb_dequeue);
}

static void dwc3_gadget_del_and_unmap_request(struct dwc3_ep *dep,
		struct dwc3_request *req, int status)
{
	struct dwc3			*dwc = dep->dwc;

	list_del(&req->list);
	req->remaining = 0;
	req->needs_extra_trb = false;
	req->num_trbs = 0;

	if (req->request.status == -EINPROGRESS)
		req->request.status = status;

	if (req->trb)
		usb_gadget_unmap_request_by_dev(dwc->sysdev,
				&req->request, req->direction);

	req->trb = NULL;
	trace_dwc3_gadget_giveback(req);

	if (dep->number > 1)
		pm_runtime_put(dwc->dev);
}

 
void dwc3_gadget_giveback(struct dwc3_ep *dep, struct dwc3_request *req,
		int status)
{
	struct dwc3			*dwc = dep->dwc;

	dwc3_gadget_del_and_unmap_request(dep, req, status);
	req->status = DWC3_REQUEST_STATUS_COMPLETED;

	spin_unlock(&dwc->lock);
	usb_gadget_giveback_request(&dep->endpoint, &req->request);
	spin_lock(&dwc->lock);
}

 
int dwc3_send_gadget_generic_command(struct dwc3 *dwc, unsigned int cmd,
		u32 param)
{
	u32		timeout = 500;
	int		status = 0;
	int		ret = 0;
	u32		reg;

	dwc3_writel(dwc->regs, DWC3_DGCMDPAR, param);
	dwc3_writel(dwc->regs, DWC3_DGCMD, cmd | DWC3_DGCMD_CMDACT);

	do {
		reg = dwc3_readl(dwc->regs, DWC3_DGCMD);
		if (!(reg & DWC3_DGCMD_CMDACT)) {
			status = DWC3_DGCMD_STATUS(reg);
			if (status)
				ret = -EINVAL;
			break;
		}
	} while (--timeout);

	if (!timeout) {
		ret = -ETIMEDOUT;
		status = -ETIMEDOUT;
	}

	trace_dwc3_gadget_generic_cmd(cmd, param, status);

	return ret;
}

static int __dwc3_gadget_wakeup(struct dwc3 *dwc, bool async);

 
int dwc3_send_gadget_ep_cmd(struct dwc3_ep *dep, unsigned int cmd,
		struct dwc3_gadget_ep_cmd_params *params)
{
	const struct usb_endpoint_descriptor *desc = dep->endpoint.desc;
	struct dwc3		*dwc = dep->dwc;
	u32			timeout = 5000;
	u32			saved_config = 0;
	u32			reg;

	int			cmd_status = 0;
	int			ret = -EINVAL;

	 
	if (dwc->gadget->speed <= USB_SPEED_HIGH ||
	    DWC3_DEPCMD_CMD(cmd) == DWC3_DEPCMD_ENDTRANSFER) {
		reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
		if (unlikely(reg & DWC3_GUSB2PHYCFG_SUSPHY)) {
			saved_config |= DWC3_GUSB2PHYCFG_SUSPHY;
			reg &= ~DWC3_GUSB2PHYCFG_SUSPHY;
		}

		if (reg & DWC3_GUSB2PHYCFG_ENBLSLPM) {
			saved_config |= DWC3_GUSB2PHYCFG_ENBLSLPM;
			reg &= ~DWC3_GUSB2PHYCFG_ENBLSLPM;
		}

		if (saved_config)
			dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);
	}

	if (DWC3_DEPCMD_CMD(cmd) == DWC3_DEPCMD_STARTTRANSFER) {
		int link_state;

		 
		link_state = dwc3_gadget_get_link_state(dwc);
		switch (link_state) {
		case DWC3_LINK_STATE_U2:
			if (dwc->gadget->speed >= USB_SPEED_SUPER)
				break;

			fallthrough;
		case DWC3_LINK_STATE_U3:
			ret = __dwc3_gadget_wakeup(dwc, false);
			dev_WARN_ONCE(dwc->dev, ret, "wakeup failed --> %d\n",
					ret);
			break;
		}
	}

	 
	if (DWC3_DEPCMD_CMD(cmd) != DWC3_DEPCMD_UPDATETRANSFER) {
		dwc3_writel(dep->regs, DWC3_DEPCMDPAR0, params->param0);
		dwc3_writel(dep->regs, DWC3_DEPCMDPAR1, params->param1);
		dwc3_writel(dep->regs, DWC3_DEPCMDPAR2, params->param2);
	}

	 
	if (DWC3_DEPCMD_CMD(cmd) == DWC3_DEPCMD_UPDATETRANSFER &&
			!usb_endpoint_xfer_isoc(desc))
		cmd &= ~(DWC3_DEPCMD_CMDIOC | DWC3_DEPCMD_CMDACT);
	else
		cmd |= DWC3_DEPCMD_CMDACT;

	dwc3_writel(dep->regs, DWC3_DEPCMD, cmd);

	if (!(cmd & DWC3_DEPCMD_CMDACT) ||
		(DWC3_DEPCMD_CMD(cmd) == DWC3_DEPCMD_ENDTRANSFER &&
		!(cmd & DWC3_DEPCMD_CMDIOC))) {
		ret = 0;
		goto skip_status;
	}

	do {
		reg = dwc3_readl(dep->regs, DWC3_DEPCMD);
		if (!(reg & DWC3_DEPCMD_CMDACT)) {
			cmd_status = DWC3_DEPCMD_STATUS(reg);

			switch (cmd_status) {
			case 0:
				ret = 0;
				break;
			case DEPEVT_TRANSFER_NO_RESOURCE:
				dev_WARN(dwc->dev, "No resource for %s\n",
					 dep->name);
				ret = -EINVAL;
				break;
			case DEPEVT_TRANSFER_BUS_EXPIRY:
				 
				ret = -EAGAIN;
				break;
			default:
				dev_WARN(dwc->dev, "UNKNOWN cmd status\n");
			}

			break;
		}
	} while (--timeout);

	if (timeout == 0) {
		ret = -ETIMEDOUT;
		cmd_status = -ETIMEDOUT;
	}

skip_status:
	trace_dwc3_gadget_ep_cmd(dep, cmd, params, cmd_status);

	if (DWC3_DEPCMD_CMD(cmd) == DWC3_DEPCMD_STARTTRANSFER) {
		if (ret == 0)
			dep->flags |= DWC3_EP_TRANSFER_STARTED;

		if (ret != -ETIMEDOUT)
			dwc3_gadget_ep_get_transfer_index(dep);
	}

	if (saved_config) {
		reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
		reg |= saved_config;
		dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);
	}

	return ret;
}

static int dwc3_send_clear_stall_ep_cmd(struct dwc3_ep *dep)
{
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_gadget_ep_cmd_params params;
	u32 cmd = DWC3_DEPCMD_CLEARSTALL;

	 
	if (dep->direction &&
	    !DWC3_VER_IS_PRIOR(DWC3, 260A) &&
	    (dwc->gadget->speed >= USB_SPEED_SUPER))
		cmd |= DWC3_DEPCMD_CLEARPENDIN;

	memset(&params, 0, sizeof(params));

	return dwc3_send_gadget_ep_cmd(dep, cmd, &params);
}

static dma_addr_t dwc3_trb_dma_offset(struct dwc3_ep *dep,
		struct dwc3_trb *trb)
{
	u32		offset = (char *) trb - (char *) dep->trb_pool;

	return dep->trb_pool_dma + offset;
}

static int dwc3_alloc_trb_pool(struct dwc3_ep *dep)
{
	struct dwc3		*dwc = dep->dwc;

	if (dep->trb_pool)
		return 0;

	dep->trb_pool = dma_alloc_coherent(dwc->sysdev,
			sizeof(struct dwc3_trb) * DWC3_TRB_NUM,
			&dep->trb_pool_dma, GFP_KERNEL);
	if (!dep->trb_pool) {
		dev_err(dep->dwc->dev, "failed to allocate trb pool for %s\n",
				dep->name);
		return -ENOMEM;
	}

	return 0;
}

static void dwc3_free_trb_pool(struct dwc3_ep *dep)
{
	struct dwc3		*dwc = dep->dwc;

	dma_free_coherent(dwc->sysdev, sizeof(struct dwc3_trb) * DWC3_TRB_NUM,
			dep->trb_pool, dep->trb_pool_dma);

	dep->trb_pool = NULL;
	dep->trb_pool_dma = 0;
}

static int dwc3_gadget_set_xfer_resource(struct dwc3_ep *dep)
{
	struct dwc3_gadget_ep_cmd_params params;

	memset(&params, 0x00, sizeof(params));

	params.param0 = DWC3_DEPXFERCFG_NUM_XFER_RES(1);

	return dwc3_send_gadget_ep_cmd(dep, DWC3_DEPCMD_SETTRANSFRESOURCE,
			&params);
}

 
static int dwc3_gadget_start_config(struct dwc3_ep *dep)
{
	struct dwc3_gadget_ep_cmd_params params;
	struct dwc3		*dwc;
	u32			cmd;
	int			i;
	int			ret;

	if (dep->number)
		return 0;

	memset(&params, 0x00, sizeof(params));
	cmd = DWC3_DEPCMD_DEPSTARTCFG;
	dwc = dep->dwc;

	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
	if (ret)
		return ret;

	for (i = 0; i < DWC3_ENDPOINTS_NUM; i++) {
		struct dwc3_ep *dep = dwc->eps[i];

		if (!dep)
			continue;

		ret = dwc3_gadget_set_xfer_resource(dep);
		if (ret)
			return ret;
	}

	return 0;
}

static int dwc3_gadget_set_ep_config(struct dwc3_ep *dep, unsigned int action)
{
	const struct usb_ss_ep_comp_descriptor *comp_desc;
	const struct usb_endpoint_descriptor *desc;
	struct dwc3_gadget_ep_cmd_params params;
	struct dwc3 *dwc = dep->dwc;

	comp_desc = dep->endpoint.comp_desc;
	desc = dep->endpoint.desc;

	memset(&params, 0x00, sizeof(params));

	params.param0 = DWC3_DEPCFG_EP_TYPE(usb_endpoint_type(desc))
		| DWC3_DEPCFG_MAX_PACKET_SIZE(usb_endpoint_maxp(desc));

	 
	if (dwc->gadget->speed >= USB_SPEED_SUPER) {
		u32 burst = dep->endpoint.maxburst;

		params.param0 |= DWC3_DEPCFG_BURST_SIZE(burst - 1);
	}

	params.param0 |= action;
	if (action == DWC3_DEPCFG_ACTION_RESTORE)
		params.param2 |= dep->saved_state;

	if (usb_endpoint_xfer_control(desc))
		params.param1 = DWC3_DEPCFG_XFER_COMPLETE_EN;

	if (dep->number <= 1 || usb_endpoint_xfer_isoc(desc))
		params.param1 |= DWC3_DEPCFG_XFER_NOT_READY_EN;

	if (usb_ss_max_streams(comp_desc) && usb_endpoint_xfer_bulk(desc)) {
		params.param1 |= DWC3_DEPCFG_STREAM_CAPABLE
			| DWC3_DEPCFG_XFER_COMPLETE_EN
			| DWC3_DEPCFG_STREAM_EVENT_EN;
		dep->stream_capable = true;
	}

	if (!usb_endpoint_xfer_control(desc))
		params.param1 |= DWC3_DEPCFG_XFER_IN_PROGRESS_EN;

	 
	params.param1 |= DWC3_DEPCFG_EP_NUMBER(dep->number);

	 
	if (dep->direction)
		params.param0 |= DWC3_DEPCFG_FIFO_NUMBER(dep->number >> 1);

	if (desc->bInterval) {
		u8 bInterval_m1;

		 
		bInterval_m1 = min_t(u8, desc->bInterval - 1, 13);

		if (usb_endpoint_type(desc) == USB_ENDPOINT_XFER_INT &&
		    dwc->gadget->speed == USB_SPEED_FULL)
			dep->interval = desc->bInterval;
		else
			dep->interval = 1 << (desc->bInterval - 1);

		params.param1 |= DWC3_DEPCFG_BINTERVAL_M1(bInterval_m1);
	}

	return dwc3_send_gadget_ep_cmd(dep, DWC3_DEPCMD_SETEPCONFIG, &params);
}

 
static int dwc3_gadget_calc_tx_fifo_size(struct dwc3 *dwc, int mult)
{
	int max_packet = 1024;
	int fifo_size;
	int mdwidth;

	mdwidth = dwc3_mdwidth(dwc);

	 
	mdwidth >>= 3;

	if (DWC3_VER_IS_PRIOR(DWC3, 290A))
		fifo_size = mult * (max_packet / mdwidth) + 1;
	else
		fifo_size = mult * ((max_packet + mdwidth) / mdwidth) + 1;
	return fifo_size;
}

 
void dwc3_gadget_clear_tx_fifos(struct dwc3 *dwc)
{
	struct dwc3_ep *dep;
	int fifo_depth;
	int size;
	int num;

	if (!dwc->do_fifo_resize)
		return;

	 
	dep = dwc->eps[1];
	size = dwc3_readl(dwc->regs, DWC3_GTXFIFOSIZ(0));
	if (DWC3_IP_IS(DWC3))
		fifo_depth = DWC3_GTXFIFOSIZ_TXFDEP(size);
	else
		fifo_depth = DWC31_GTXFIFOSIZ_TXFDEP(size);

	dwc->last_fifo_depth = fifo_depth;
	 
	for (num = 3; num < min_t(int, dwc->num_eps, DWC3_ENDPOINTS_NUM);
	     num += 2) {
		dep = dwc->eps[num];
		 
		size = DWC3_IP_IS(DWC3) ? 0 :
			dwc3_readl(dwc->regs, DWC3_GTXFIFOSIZ(num >> 1)) &
				   DWC31_GTXFIFOSIZ_TXFRAMNUM;

		dwc3_writel(dwc->regs, DWC3_GTXFIFOSIZ(num >> 1), size);
		dep->flags &= ~DWC3_EP_TXFIFO_RESIZED;
	}
	dwc->num_ep_resized = 0;
}

 
static int dwc3_gadget_resize_tx_fifos(struct dwc3_ep *dep)
{
	struct dwc3 *dwc = dep->dwc;
	int fifo_0_start;
	int ram1_depth;
	int fifo_size;
	int min_depth;
	int num_in_ep;
	int remaining;
	int num_fifos = 1;
	int fifo;
	int tmp;

	if (!dwc->do_fifo_resize)
		return 0;

	 
	if (!usb_endpoint_dir_in(dep->endpoint.desc) || dep->number <= 1)
		return 0;

	 
	if (dep->flags & DWC3_EP_TXFIFO_RESIZED)
		return 0;

	ram1_depth = DWC3_RAM1_DEPTH(dwc->hwparams.hwparams7);

	if ((dep->endpoint.maxburst > 1 &&
	     usb_endpoint_xfer_bulk(dep->endpoint.desc)) ||
	    usb_endpoint_xfer_isoc(dep->endpoint.desc))
		num_fifos = 3;

	if (dep->endpoint.maxburst > 6 &&
	    (usb_endpoint_xfer_bulk(dep->endpoint.desc) ||
	     usb_endpoint_xfer_isoc(dep->endpoint.desc)) && DWC3_IP_IS(DWC31))
		num_fifos = dwc->tx_fifo_resize_max_num;

	 
	fifo = dwc3_gadget_calc_tx_fifo_size(dwc, 1);

	 
	num_in_ep = dwc->max_cfg_eps;
	num_in_ep -= dwc->num_ep_resized;

	 
	min_depth = num_in_ep * (fifo + 1);
	remaining = ram1_depth - min_depth - dwc->last_fifo_depth;
	remaining = max_t(int, 0, remaining);
	 
	fifo_size = (num_fifos - 1) * fifo;
	if (remaining < fifo_size)
		fifo_size = remaining;

	fifo_size += fifo;
	 
	fifo_size++;

	 
	tmp = dwc3_readl(dwc->regs, DWC3_GTXFIFOSIZ(0));
	fifo_0_start = DWC3_GTXFIFOSIZ_TXFSTADDR(tmp);

	fifo_size |= (fifo_0_start + (dwc->last_fifo_depth << 16));
	if (DWC3_IP_IS(DWC3))
		dwc->last_fifo_depth += DWC3_GTXFIFOSIZ_TXFDEP(fifo_size);
	else
		dwc->last_fifo_depth += DWC31_GTXFIFOSIZ_TXFDEP(fifo_size);

	 
	if (dwc->last_fifo_depth >= ram1_depth) {
		dev_err(dwc->dev, "Fifosize(%d) > RAM size(%d) %s depth:%d\n",
			dwc->last_fifo_depth, ram1_depth,
			dep->endpoint.name, fifo_size);
		if (DWC3_IP_IS(DWC3))
			fifo_size = DWC3_GTXFIFOSIZ_TXFDEP(fifo_size);
		else
			fifo_size = DWC31_GTXFIFOSIZ_TXFDEP(fifo_size);

		dwc->last_fifo_depth -= fifo_size;
		return -ENOMEM;
	}

	dwc3_writel(dwc->regs, DWC3_GTXFIFOSIZ(dep->number >> 1), fifo_size);
	dep->flags |= DWC3_EP_TXFIFO_RESIZED;
	dwc->num_ep_resized++;

	return 0;
}

 
static int __dwc3_gadget_ep_enable(struct dwc3_ep *dep, unsigned int action)
{
	const struct usb_endpoint_descriptor *desc = dep->endpoint.desc;
	struct dwc3		*dwc = dep->dwc;

	u32			reg;
	int			ret;

	if (!(dep->flags & DWC3_EP_ENABLED)) {
		ret = dwc3_gadget_resize_tx_fifos(dep);
		if (ret)
			return ret;

		ret = dwc3_gadget_start_config(dep);
		if (ret)
			return ret;
	}

	ret = dwc3_gadget_set_ep_config(dep, action);
	if (ret)
		return ret;

	if (!(dep->flags & DWC3_EP_ENABLED)) {
		struct dwc3_trb	*trb_st_hw;
		struct dwc3_trb	*trb_link;

		dep->type = usb_endpoint_type(desc);
		dep->flags |= DWC3_EP_ENABLED;

		reg = dwc3_readl(dwc->regs, DWC3_DALEPENA);
		reg |= DWC3_DALEPENA_EP(dep->number);
		dwc3_writel(dwc->regs, DWC3_DALEPENA, reg);

		dep->trb_dequeue = 0;
		dep->trb_enqueue = 0;

		if (usb_endpoint_xfer_control(desc))
			goto out;

		 
		memset(dep->trb_pool, 0,
		       sizeof(struct dwc3_trb) * DWC3_TRB_NUM);

		 
		trb_st_hw = &dep->trb_pool[0];

		trb_link = &dep->trb_pool[DWC3_TRB_NUM - 1];
		trb_link->bpl = lower_32_bits(dwc3_trb_dma_offset(dep, trb_st_hw));
		trb_link->bph = upper_32_bits(dwc3_trb_dma_offset(dep, trb_st_hw));
		trb_link->ctrl |= DWC3_TRBCTL_LINK_TRB;
		trb_link->ctrl |= DWC3_TRB_CTRL_HWO;
	}

	 
	if (usb_endpoint_xfer_bulk(desc) ||
			usb_endpoint_xfer_int(desc)) {
		struct dwc3_gadget_ep_cmd_params params;
		struct dwc3_trb	*trb;
		dma_addr_t trb_dma;
		u32 cmd;

		memset(&params, 0, sizeof(params));
		trb = &dep->trb_pool[0];
		trb_dma = dwc3_trb_dma_offset(dep, trb);

		params.param0 = upper_32_bits(trb_dma);
		params.param1 = lower_32_bits(trb_dma);

		cmd = DWC3_DEPCMD_STARTTRANSFER;

		ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
		if (ret < 0)
			return ret;

		if (dep->stream_capable) {
			 
			dwc3_stop_active_transfer(dep, true, true);

			 
			if (!dep->direction ||
			    !(dwc->hwparams.hwparams9 &
			      DWC3_GHWPARAMS9_DEV_TXF_FLUSH_BYPASS))
				dep->flags |= DWC3_EP_FORCE_RESTART_STREAM;
		}
	}

out:
	trace_dwc3_gadget_ep_enable(dep);

	return 0;
}

void dwc3_remove_requests(struct dwc3 *dwc, struct dwc3_ep *dep, int status)
{
	struct dwc3_request		*req;

	dwc3_stop_active_transfer(dep, true, false);

	 
	if (dep->flags & DWC3_EP_DELAY_STOP)
		return;

	 
	while (!list_empty(&dep->started_list)) {
		req = next_request(&dep->started_list);

		dwc3_gadget_giveback(dep, req, status);
	}

	while (!list_empty(&dep->pending_list)) {
		req = next_request(&dep->pending_list);

		dwc3_gadget_giveback(dep, req, status);
	}

	while (!list_empty(&dep->cancelled_list)) {
		req = next_request(&dep->cancelled_list);

		dwc3_gadget_giveback(dep, req, status);
	}
}

 
static int __dwc3_gadget_ep_disable(struct dwc3_ep *dep)
{
	struct dwc3		*dwc = dep->dwc;
	u32			reg;
	u32			mask;

	trace_dwc3_gadget_ep_disable(dep);

	 
	if (dep->flags & DWC3_EP_STALL)
		__dwc3_gadget_ep_set_halt(dep, 0, false);

	reg = dwc3_readl(dwc->regs, DWC3_DALEPENA);
	reg &= ~DWC3_DALEPENA_EP(dep->number);
	dwc3_writel(dwc->regs, DWC3_DALEPENA, reg);

	dwc3_remove_requests(dwc, dep, -ESHUTDOWN);

	dep->stream_capable = false;
	dep->type = 0;
	mask = DWC3_EP_TXFIFO_RESIZED;
	 
	if (dep->flags & DWC3_EP_DELAY_STOP)
		mask |= (DWC3_EP_DELAY_STOP | DWC3_EP_TRANSFER_STARTED);
	dep->flags &= mask;

	 
	if (dep->number > 1) {
		dep->endpoint.comp_desc = NULL;
		dep->endpoint.desc = NULL;
	}

	return 0;
}

 

static int dwc3_gadget_ep0_enable(struct usb_ep *ep,
		const struct usb_endpoint_descriptor *desc)
{
	return -EINVAL;
}

static int dwc3_gadget_ep0_disable(struct usb_ep *ep)
{
	return -EINVAL;
}

 

static int dwc3_gadget_ep_enable(struct usb_ep *ep,
		const struct usb_endpoint_descriptor *desc)
{
	struct dwc3_ep			*dep;
	struct dwc3			*dwc;
	unsigned long			flags;
	int				ret;

	if (!ep || !desc || desc->bDescriptorType != USB_DT_ENDPOINT) {
		pr_debug("dwc3: invalid parameters\n");
		return -EINVAL;
	}

	if (!desc->wMaxPacketSize) {
		pr_debug("dwc3: missing wMaxPacketSize\n");
		return -EINVAL;
	}

	dep = to_dwc3_ep(ep);
	dwc = dep->dwc;

	if (dev_WARN_ONCE(dwc->dev, dep->flags & DWC3_EP_ENABLED,
					"%s is already enabled\n",
					dep->name))
		return 0;

	spin_lock_irqsave(&dwc->lock, flags);
	ret = __dwc3_gadget_ep_enable(dep, DWC3_DEPCFG_ACTION_INIT);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static int dwc3_gadget_ep_disable(struct usb_ep *ep)
{
	struct dwc3_ep			*dep;
	struct dwc3			*dwc;
	unsigned long			flags;
	int				ret;

	if (!ep) {
		pr_debug("dwc3: invalid parameters\n");
		return -EINVAL;
	}

	dep = to_dwc3_ep(ep);
	dwc = dep->dwc;

	if (dev_WARN_ONCE(dwc->dev, !(dep->flags & DWC3_EP_ENABLED),
					"%s is already disabled\n",
					dep->name))
		return 0;

	spin_lock_irqsave(&dwc->lock, flags);
	ret = __dwc3_gadget_ep_disable(dep);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static struct usb_request *dwc3_gadget_ep_alloc_request(struct usb_ep *ep,
		gfp_t gfp_flags)
{
	struct dwc3_request		*req;
	struct dwc3_ep			*dep = to_dwc3_ep(ep);

	req = kzalloc(sizeof(*req), gfp_flags);
	if (!req)
		return NULL;

	req->direction	= dep->direction;
	req->epnum	= dep->number;
	req->dep	= dep;
	req->status	= DWC3_REQUEST_STATUS_UNKNOWN;

	trace_dwc3_alloc_request(req);

	return &req->request;
}

static void dwc3_gadget_ep_free_request(struct usb_ep *ep,
		struct usb_request *request)
{
	struct dwc3_request		*req = to_dwc3_request(request);

	trace_dwc3_free_request(req);
	kfree(req);
}

 
static struct dwc3_trb *dwc3_ep_prev_trb(struct dwc3_ep *dep, u8 index)
{
	u8 tmp = index;

	if (!tmp)
		tmp = DWC3_TRB_NUM - 1;

	return &dep->trb_pool[tmp - 1];
}

static u32 dwc3_calc_trbs_left(struct dwc3_ep *dep)
{
	u8			trbs_left;

	 
	if (dep->trb_enqueue == dep->trb_dequeue) {
		 
		if (!list_empty(&dep->started_list))
			return 0;

		return DWC3_TRB_NUM - 1;
	}

	trbs_left = dep->trb_dequeue - dep->trb_enqueue;
	trbs_left &= (DWC3_TRB_NUM - 1);

	if (dep->trb_dequeue < dep->trb_enqueue)
		trbs_left--;

	return trbs_left;
}

 
static void dwc3_prepare_one_trb(struct dwc3_ep *dep,
		struct dwc3_request *req, unsigned int trb_length,
		unsigned int chain, unsigned int node, bool use_bounce_buffer,
		bool must_interrupt)
{
	struct dwc3_trb		*trb;
	dma_addr_t		dma;
	unsigned int		stream_id = req->request.stream_id;
	unsigned int		short_not_ok = req->request.short_not_ok;
	unsigned int		no_interrupt = req->request.no_interrupt;
	unsigned int		is_last = req->request.is_last;
	struct dwc3		*dwc = dep->dwc;
	struct usb_gadget	*gadget = dwc->gadget;
	enum usb_device_speed	speed = gadget->speed;

	if (use_bounce_buffer)
		dma = dep->dwc->bounce_addr;
	else if (req->request.num_sgs > 0)
		dma = sg_dma_address(req->start_sg);
	else
		dma = req->request.dma;

	trb = &dep->trb_pool[dep->trb_enqueue];

	if (!req->trb) {
		dwc3_gadget_move_started_request(req);
		req->trb = trb;
		req->trb_dma = dwc3_trb_dma_offset(dep, trb);
	}

	req->num_trbs++;

	trb->size = DWC3_TRB_SIZE_LENGTH(trb_length);
	trb->bpl = lower_32_bits(dma);
	trb->bph = upper_32_bits(dma);

	switch (usb_endpoint_type(dep->endpoint.desc)) {
	case USB_ENDPOINT_XFER_CONTROL:
		trb->ctrl = DWC3_TRBCTL_CONTROL_SETUP;
		break;

	case USB_ENDPOINT_XFER_ISOC:
		if (!node) {
			trb->ctrl = DWC3_TRBCTL_ISOCHRONOUS_FIRST;

			 
			if (speed == USB_SPEED_HIGH) {
				struct usb_ep *ep = &dep->endpoint;
				unsigned int mult = 2;
				unsigned int maxp = usb_endpoint_maxp(ep->desc);

				if (req->request.length <= (2 * maxp))
					mult--;

				if (req->request.length <= maxp)
					mult--;

				trb->size |= DWC3_TRB_SIZE_PCM1(mult);
			}
		} else {
			trb->ctrl = DWC3_TRBCTL_ISOCHRONOUS;
		}

		if (!no_interrupt && !chain)
			trb->ctrl |= DWC3_TRB_CTRL_ISP_IMI;
		break;

	case USB_ENDPOINT_XFER_BULK:
	case USB_ENDPOINT_XFER_INT:
		trb->ctrl = DWC3_TRBCTL_NORMAL;
		break;
	default:
		 
		dev_WARN(dwc->dev, "Unknown endpoint type %d\n",
				usb_endpoint_type(dep->endpoint.desc));
	}

	 
	if (usb_endpoint_dir_out(dep->endpoint.desc)) {
		if (!dep->stream_capable)
			trb->ctrl |= DWC3_TRB_CTRL_CSP;

		if (short_not_ok)
			trb->ctrl |= DWC3_TRB_CTRL_ISP_IMI;
	}

	 
	if (dep->stream_capable && DWC3_MST_CAPABLE(&dwc->hwparams))
		trb->ctrl |= DWC3_TRB_CTRL_CSP;

	if ((!no_interrupt && !chain) || must_interrupt)
		trb->ctrl |= DWC3_TRB_CTRL_IOC;

	if (chain)
		trb->ctrl |= DWC3_TRB_CTRL_CHN;
	else if (dep->stream_capable && is_last &&
		 !DWC3_MST_CAPABLE(&dwc->hwparams))
		trb->ctrl |= DWC3_TRB_CTRL_LST;

	if (usb_endpoint_xfer_bulk(dep->endpoint.desc) && dep->stream_capable)
		trb->ctrl |= DWC3_TRB_CTRL_SID_SOFN(stream_id);

	 
	wmb();
	trb->ctrl |= DWC3_TRB_CTRL_HWO;

	dwc3_ep_inc_enq(dep);

	trace_dwc3_prepare_trb(dep, trb);
}

static bool dwc3_needs_extra_trb(struct dwc3_ep *dep, struct dwc3_request *req)
{
	unsigned int maxp = usb_endpoint_maxp(dep->endpoint.desc);
	unsigned int rem = req->request.length % maxp;

	if ((req->request.length && req->request.zero && !rem &&
			!usb_endpoint_xfer_isoc(dep->endpoint.desc)) ||
			(!req->direction && rem))
		return true;

	return false;
}

 
static int dwc3_prepare_last_sg(struct dwc3_ep *dep,
		struct dwc3_request *req, unsigned int entry_length,
		unsigned int node)
{
	unsigned int maxp = usb_endpoint_maxp(dep->endpoint.desc);
	unsigned int rem = req->request.length % maxp;
	unsigned int num_trbs = 1;

	if (dwc3_needs_extra_trb(dep, req))
		num_trbs++;

	if (dwc3_calc_trbs_left(dep) < num_trbs)
		return 0;

	req->needs_extra_trb = num_trbs > 1;

	 
	if (req->direction || req->request.length)
		dwc3_prepare_one_trb(dep, req, entry_length,
				req->needs_extra_trb, node, false, false);

	 
	if ((!req->direction && !req->request.length) || req->needs_extra_trb)
		dwc3_prepare_one_trb(dep, req,
				req->direction ? 0 : maxp - rem,
				false, 1, true, false);

	return num_trbs;
}

static int dwc3_prepare_trbs_sg(struct dwc3_ep *dep,
		struct dwc3_request *req)
{
	struct scatterlist *sg = req->start_sg;
	struct scatterlist *s;
	int		i;
	unsigned int length = req->request.length;
	unsigned int remaining = req->request.num_mapped_sgs
		- req->num_queued_sgs;
	unsigned int num_trbs = req->num_trbs;
	bool needs_extra_trb = dwc3_needs_extra_trb(dep, req);

	 
	for_each_sg(req->request.sg, s, req->num_queued_sgs, i)
		length -= sg_dma_len(s);

	for_each_sg(sg, s, remaining, i) {
		unsigned int num_trbs_left = dwc3_calc_trbs_left(dep);
		unsigned int trb_length;
		bool must_interrupt = false;
		bool last_sg = false;

		trb_length = min_t(unsigned int, length, sg_dma_len(s));

		length -= trb_length;

		 
		if ((i == remaining - 1) || !length)
			last_sg = true;

		if (!num_trbs_left)
			break;

		if (last_sg) {
			if (!dwc3_prepare_last_sg(dep, req, trb_length, i))
				break;
		} else {
			 
			if (num_trbs_left == 1 || (needs_extra_trb &&
					num_trbs_left <= 2 &&
					sg_dma_len(sg_next(s)) >= length)) {
				struct dwc3_request *r;

				 
				list_for_each_entry(r, &dep->started_list, list) {
					if (r != req && !r->request.no_interrupt)
						break;

					if (r == req)
						must_interrupt = true;
				}
			}

			dwc3_prepare_one_trb(dep, req, trb_length, 1, i, false,
					must_interrupt);
		}

		 
		if (!last_sg)
			req->start_sg = sg_next(s);

		req->num_queued_sgs++;
		req->num_pending_sgs--;

		 
		if (length == 0) {
			req->num_pending_sgs = 0;
			break;
		}

		if (must_interrupt)
			break;
	}

	return req->num_trbs - num_trbs;
}

static int dwc3_prepare_trbs_linear(struct dwc3_ep *dep,
		struct dwc3_request *req)
{
	return dwc3_prepare_last_sg(dep, req, req->request.length, 0);
}

 
static int dwc3_prepare_trbs(struct dwc3_ep *dep)
{
	struct dwc3_request	*req, *n;
	int			ret = 0;

	BUILD_BUG_ON_NOT_POWER_OF_2(DWC3_TRB_NUM);

	 
	list_for_each_entry(req, &dep->started_list, list) {
		if (req->num_pending_sgs > 0) {
			ret = dwc3_prepare_trbs_sg(dep, req);
			if (!ret || req->num_pending_sgs)
				return ret;
		}

		if (!dwc3_calc_trbs_left(dep))
			return ret;

		 
		if (dep->stream_capable && req->request.is_last &&
		    !DWC3_MST_CAPABLE(&dep->dwc->hwparams))
			return ret;
	}

	list_for_each_entry_safe(req, n, &dep->pending_list, list) {
		struct dwc3	*dwc = dep->dwc;

		ret = usb_gadget_map_request_by_dev(dwc->sysdev, &req->request,
						    dep->direction);
		if (ret)
			return ret;

		req->sg			= req->request.sg;
		req->start_sg		= req->sg;
		req->num_queued_sgs	= 0;
		req->num_pending_sgs	= req->request.num_mapped_sgs;

		if (req->num_pending_sgs > 0) {
			ret = dwc3_prepare_trbs_sg(dep, req);
			if (req->num_pending_sgs)
				return ret;
		} else {
			ret = dwc3_prepare_trbs_linear(dep, req);
		}

		if (!ret || !dwc3_calc_trbs_left(dep))
			return ret;

		 
		if (dep->stream_capable && req->request.is_last &&
		    !DWC3_MST_CAPABLE(&dwc->hwparams))
			return ret;
	}

	return ret;
}

static void dwc3_gadget_ep_cleanup_cancelled_requests(struct dwc3_ep *dep);

static int __dwc3_gadget_kick_transfer(struct dwc3_ep *dep)
{
	struct dwc3_gadget_ep_cmd_params params;
	struct dwc3_request		*req;
	int				starting;
	int				ret;
	u32				cmd;

	 
	ret = dwc3_prepare_trbs(dep);
	if (ret < 0)
		return ret;

	starting = !(dep->flags & DWC3_EP_TRANSFER_STARTED);

	 
	if (!ret && !starting)
		return ret;

	req = next_request(&dep->started_list);
	if (!req) {
		dep->flags |= DWC3_EP_PENDING_REQUEST;
		return 0;
	}

	memset(&params, 0, sizeof(params));

	if (starting) {
		params.param0 = upper_32_bits(req->trb_dma);
		params.param1 = lower_32_bits(req->trb_dma);
		cmd = DWC3_DEPCMD_STARTTRANSFER;

		if (dep->stream_capable)
			cmd |= DWC3_DEPCMD_PARAM(req->request.stream_id);

		if (usb_endpoint_xfer_isoc(dep->endpoint.desc))
			cmd |= DWC3_DEPCMD_PARAM(dep->frame_number);
	} else {
		cmd = DWC3_DEPCMD_UPDATETRANSFER |
			DWC3_DEPCMD_PARAM(dep->resource_index);
	}

	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
	if (ret < 0) {
		struct dwc3_request *tmp;

		if (ret == -EAGAIN)
			return ret;

		dwc3_stop_active_transfer(dep, true, true);

		list_for_each_entry_safe(req, tmp, &dep->started_list, list)
			dwc3_gadget_move_cancelled_request(req, DWC3_REQUEST_STATUS_DEQUEUED);

		 
		if (!(dep->flags & DWC3_EP_END_TRANSFER_PENDING))
			dwc3_gadget_ep_cleanup_cancelled_requests(dep);

		return ret;
	}

	if (dep->stream_capable && req->request.is_last &&
	    !DWC3_MST_CAPABLE(&dep->dwc->hwparams))
		dep->flags |= DWC3_EP_WAIT_TRANSFER_COMPLETE;

	return 0;
}

static int __dwc3_gadget_get_frame(struct dwc3 *dwc)
{
	u32			reg;

	reg = dwc3_readl(dwc->regs, DWC3_DSTS);
	return DWC3_DSTS_SOFFN(reg);
}

 
static int __dwc3_stop_active_transfer(struct dwc3_ep *dep, bool force, bool interrupt)
{
	struct dwc3 *dwc = dep->dwc;
	struct dwc3_gadget_ep_cmd_params params;
	u32 cmd;
	int ret;

	cmd = DWC3_DEPCMD_ENDTRANSFER;
	cmd |= force ? DWC3_DEPCMD_HIPRI_FORCERM : 0;
	cmd |= interrupt ? DWC3_DEPCMD_CMDIOC : 0;
	cmd |= DWC3_DEPCMD_PARAM(dep->resource_index);
	memset(&params, 0, sizeof(params));
	ret = dwc3_send_gadget_ep_cmd(dep, cmd, &params);
	 
	if (ret == -ETIMEDOUT && dep->dwc->ep0state != EP0_SETUP_PHASE) {
		dep->flags |= DWC3_EP_DELAY_STOP;
		return 0;
	}
	WARN_ON_ONCE(ret);
	dep->resource_index = 0;

	if (!interrupt) {
		if (!DWC3_IP_IS(DWC3) || DWC3_VER_IS_PRIOR(DWC3, 310A))
			mdelay(1);
		dep->flags &= ~DWC3_EP_TRANSFER_STARTED;
	} else if (!ret) {
		dep->flags |= DWC3_EP_END_TRANSFER_PENDING;
	}

	dep->flags &= ~DWC3_EP_DELAY_STOP;
	return ret;
}

 
static int dwc3_gadget_start_isoc_quirk(struct dwc3_ep *dep)
{
	int cmd_status = 0;
	bool test0;
	bool test1;

	while (dep->combo_num < 2) {
		struct dwc3_gadget_ep_cmd_params params;
		u32 test_frame_number;
		u32 cmd;

		 
		test_frame_number = dep->frame_number & DWC3_FRNUMBER_MASK;
		test_frame_number |= dep->combo_num << 14;
		test_frame_number += max_t(u32, 4, dep->interval);

		params.param0 = upper_32_bits(dep->dwc->bounce_addr);
		params.param1 = lower_32_bits(dep->dwc->bounce_addr);

		cmd = DWC3_DEPCMD_STARTTRANSFER;
		cmd |= DWC3_DEPCMD_PARAM(test_frame_number);
		cmd_status = dwc3_send_gadget_ep_cmd(dep, cmd, &params);

		 
		if (cmd_status && cmd_status != -EAGAIN) {
			dep->start_cmd_status = 0;
			dep->combo_num = 0;
			return 0;
		}

		 
		if (dep->combo_num == 0)
			dep->start_cmd_status = cmd_status;

		dep->combo_num++;

		 
		if (cmd_status == 0) {
			dwc3_stop_active_transfer(dep, true, true);
			return 0;
		}
	}

	 
	test0 = (dep->start_cmd_status == 0);
	test1 = (cmd_status == 0);

	if (!test0 && test1)
		dep->combo_num = 1;
	else if (!test0 && !test1)
		dep->combo_num = 2;
	else if (test0 && !test1)
		dep->combo_num = 3;
	else if (test0 && test1)
		dep->combo_num = 0;

	dep->frame_number &= DWC3_FRNUMBER_MASK;
	dep->frame_number |= dep->combo_num << 14;
	dep->frame_number += max_t(u32, 4, dep->interval);

	 
	dep->start_cmd_status = 0;
	dep->combo_num = 0;

	return __dwc3_gadget_kick_transfer(dep);
}

static int __dwc3_gadget_start_isoc(struct dwc3_ep *dep)
{
	const struct usb_endpoint_descriptor *desc = dep->endpoint.desc;
	struct dwc3 *dwc = dep->dwc;
	int ret;
	int i;

	if (list_empty(&dep->pending_list) &&
	    list_empty(&dep->started_list)) {
		dep->flags |= DWC3_EP_PENDING_REQUEST;
		return -EAGAIN;
	}

	if (!dwc->dis_start_transfer_quirk &&
	    (DWC3_VER_IS_PRIOR(DWC31, 170A) ||
	     DWC3_VER_TYPE_IS_WITHIN(DWC31, 170A, EA01, EA06))) {
		if (dwc->gadget->speed <= USB_SPEED_HIGH && dep->direction)
			return dwc3_gadget_start_isoc_quirk(dep);
	}

	if (desc->bInterval <= 14 &&
	    dwc->gadget->speed >= USB_SPEED_HIGH) {
		u32 frame = __dwc3_gadget_get_frame(dwc);
		bool rollover = frame <
				(dep->frame_number & DWC3_FRNUMBER_MASK);

		 

		dep->frame_number = (dep->frame_number & ~DWC3_FRNUMBER_MASK) |
				     frame;
		if (rollover)
			dep->frame_number += BIT(14);
	}

	for (i = 0; i < DWC3_ISOC_MAX_RETRIES; i++) {
		int future_interval = i + 1;

		 
		if (desc->bInterval < 3)
			future_interval += 3 - desc->bInterval;

		dep->frame_number = DWC3_ALIGN_FRAME(dep, future_interval);

		ret = __dwc3_gadget_kick_transfer(dep);
		if (ret != -EAGAIN)
			break;
	}

	 
	if (ret == -EAGAIN)
		ret = __dwc3_stop_active_transfer(dep, false, true);

	return ret;
}

static int __dwc3_gadget_ep_queue(struct dwc3_ep *dep, struct dwc3_request *req)
{
	struct dwc3		*dwc = dep->dwc;

	if (!dep->endpoint.desc || !dwc->pullups_connected || !dwc->connected) {
		dev_dbg(dwc->dev, "%s: can't queue to disabled endpoint\n",
				dep->name);
		return -ESHUTDOWN;
	}

	if (WARN(req->dep != dep, "request %pK belongs to '%s'\n",
				&req->request, req->dep->name))
		return -EINVAL;

	if (WARN(req->status < DWC3_REQUEST_STATUS_COMPLETED,
				"%s: request %pK already in flight\n",
				dep->name, &req->request))
		return -EINVAL;

	pm_runtime_get(dwc->dev);

	req->request.actual	= 0;
	req->request.status	= -EINPROGRESS;

	trace_dwc3_ep_queue(req);

	list_add_tail(&req->list, &dep->pending_list);
	req->status = DWC3_REQUEST_STATUS_QUEUED;

	if (dep->flags & DWC3_EP_WAIT_TRANSFER_COMPLETE)
		return 0;

	 
	if ((dep->flags & DWC3_EP_END_TRANSFER_PENDING) ||
	    (dep->flags & DWC3_EP_WEDGE) ||
	    (dep->flags & DWC3_EP_DELAY_STOP) ||
	    (dep->flags & DWC3_EP_STALL)) {
		dep->flags |= DWC3_EP_DELAY_START;
		return 0;
	}

	 
	if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		if (!(dep->flags & DWC3_EP_TRANSFER_STARTED)) {
			if ((dep->flags & DWC3_EP_PENDING_REQUEST))
				return __dwc3_gadget_start_isoc(dep);

			return 0;
		}
	}

	__dwc3_gadget_kick_transfer(dep);

	return 0;
}

static int dwc3_gadget_ep_queue(struct usb_ep *ep, struct usb_request *request,
	gfp_t gfp_flags)
{
	struct dwc3_request		*req = to_dwc3_request(request);
	struct dwc3_ep			*dep = to_dwc3_ep(ep);
	struct dwc3			*dwc = dep->dwc;

	unsigned long			flags;

	int				ret;

	spin_lock_irqsave(&dwc->lock, flags);
	ret = __dwc3_gadget_ep_queue(dep, req);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static void dwc3_gadget_ep_skip_trbs(struct dwc3_ep *dep, struct dwc3_request *req)
{
	int i;

	 
	if (!req->trb)
		return;

	 
	for (i = 0; i < req->num_trbs; i++) {
		struct dwc3_trb *trb;

		trb = &dep->trb_pool[dep->trb_dequeue];
		trb->ctrl &= ~DWC3_TRB_CTRL_HWO;
		dwc3_ep_inc_deq(dep);
	}

	req->num_trbs = 0;
}

static void dwc3_gadget_ep_cleanup_cancelled_requests(struct dwc3_ep *dep)
{
	struct dwc3_request		*req;
	struct dwc3			*dwc = dep->dwc;

	while (!list_empty(&dep->cancelled_list)) {
		req = next_request(&dep->cancelled_list);
		dwc3_gadget_ep_skip_trbs(dep, req);
		switch (req->status) {
		case DWC3_REQUEST_STATUS_DISCONNECTED:
			dwc3_gadget_giveback(dep, req, -ESHUTDOWN);
			break;
		case DWC3_REQUEST_STATUS_DEQUEUED:
			dwc3_gadget_giveback(dep, req, -ECONNRESET);
			break;
		case DWC3_REQUEST_STATUS_STALLED:
			dwc3_gadget_giveback(dep, req, -EPIPE);
			break;
		default:
			dev_err(dwc->dev, "request cancelled with wrong reason:%d\n", req->status);
			dwc3_gadget_giveback(dep, req, -ECONNRESET);
			break;
		}
		 
		if (!dep->endpoint.desc)
			break;
	}
}

static int dwc3_gadget_ep_dequeue(struct usb_ep *ep,
		struct usb_request *request)
{
	struct dwc3_request		*req = to_dwc3_request(request);
	struct dwc3_request		*r = NULL;

	struct dwc3_ep			*dep = to_dwc3_ep(ep);
	struct dwc3			*dwc = dep->dwc;

	unsigned long			flags;
	int				ret = 0;

	trace_dwc3_ep_dequeue(req);

	spin_lock_irqsave(&dwc->lock, flags);

	list_for_each_entry(r, &dep->cancelled_list, list) {
		if (r == req)
			goto out;
	}

	list_for_each_entry(r, &dep->pending_list, list) {
		if (r == req) {
			 
			if (dep->number > 1)
				dwc3_gadget_giveback(dep, req, -ECONNRESET);
			else
				dwc3_ep0_reset_state(dwc);
			goto out;
		}
	}

	list_for_each_entry(r, &dep->started_list, list) {
		if (r == req) {
			struct dwc3_request *t;

			 
			dwc3_stop_active_transfer(dep, true, true);

			 
			list_for_each_entry_safe(r, t, &dep->started_list, list)
				dwc3_gadget_move_cancelled_request(r,
						DWC3_REQUEST_STATUS_DEQUEUED);

			dep->flags &= ~DWC3_EP_WAIT_TRANSFER_COMPLETE;

			goto out;
		}
	}

	dev_err(dwc->dev, "request %pK was not queued to %s\n",
		request, ep->name);
	ret = -EINVAL;
out:
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

int __dwc3_gadget_ep_set_halt(struct dwc3_ep *dep, int value, int protocol)
{
	struct dwc3_gadget_ep_cmd_params	params;
	struct dwc3				*dwc = dep->dwc;
	struct dwc3_request			*req;
	struct dwc3_request			*tmp;
	int					ret;

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc)) {
		dev_err(dwc->dev, "%s is of Isochronous type\n", dep->name);
		return -EINVAL;
	}

	memset(&params, 0x00, sizeof(params));

	if (value) {
		struct dwc3_trb *trb;

		unsigned int transfer_in_flight;
		unsigned int started;

		if (dep->number > 1)
			trb = dwc3_ep_prev_trb(dep, dep->trb_enqueue);
		else
			trb = &dwc->ep0_trb[dep->trb_enqueue];

		transfer_in_flight = trb->ctrl & DWC3_TRB_CTRL_HWO;
		started = !list_empty(&dep->started_list);

		if (!protocol && ((dep->direction && transfer_in_flight) ||
				(!dep->direction && started))) {
			return -EAGAIN;
		}

		ret = dwc3_send_gadget_ep_cmd(dep, DWC3_DEPCMD_SETSTALL,
				&params);
		if (ret)
			dev_err(dwc->dev, "failed to set STALL on %s\n",
					dep->name);
		else
			dep->flags |= DWC3_EP_STALL;
	} else {
		 
		if (dep->number <= 1) {
			dep->flags &= ~(DWC3_EP_STALL | DWC3_EP_WEDGE);
			return 0;
		}

		dwc3_stop_active_transfer(dep, true, true);

		list_for_each_entry_safe(req, tmp, &dep->started_list, list)
			dwc3_gadget_move_cancelled_request(req, DWC3_REQUEST_STATUS_STALLED);

		if (dep->flags & DWC3_EP_END_TRANSFER_PENDING ||
		    (dep->flags & DWC3_EP_DELAY_STOP)) {
			dep->flags |= DWC3_EP_PENDING_CLEAR_STALL;
			if (protocol)
				dwc->clear_stall_protocol = dep->number;

			return 0;
		}

		dwc3_gadget_ep_cleanup_cancelled_requests(dep);

		ret = dwc3_send_clear_stall_ep_cmd(dep);
		if (ret) {
			dev_err(dwc->dev, "failed to clear STALL on %s\n",
					dep->name);
			return ret;
		}

		dep->flags &= ~(DWC3_EP_STALL | DWC3_EP_WEDGE);

		if ((dep->flags & DWC3_EP_DELAY_START) &&
		    !usb_endpoint_xfer_isoc(dep->endpoint.desc))
			__dwc3_gadget_kick_transfer(dep);

		dep->flags &= ~DWC3_EP_DELAY_START;
	}

	return ret;
}

static int dwc3_gadget_ep_set_halt(struct usb_ep *ep, int value)
{
	struct dwc3_ep			*dep = to_dwc3_ep(ep);
	struct dwc3			*dwc = dep->dwc;

	unsigned long			flags;

	int				ret;

	spin_lock_irqsave(&dwc->lock, flags);
	ret = __dwc3_gadget_ep_set_halt(dep, value, false);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static int dwc3_gadget_ep_set_wedge(struct usb_ep *ep)
{
	struct dwc3_ep			*dep = to_dwc3_ep(ep);
	struct dwc3			*dwc = dep->dwc;
	unsigned long			flags;
	int				ret;

	spin_lock_irqsave(&dwc->lock, flags);
	dep->flags |= DWC3_EP_WEDGE;

	if (dep->number == 0 || dep->number == 1)
		ret = __dwc3_gadget_ep0_set_halt(ep, 1);
	else
		ret = __dwc3_gadget_ep_set_halt(dep, 1, false);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

 

static struct usb_endpoint_descriptor dwc3_gadget_ep0_desc = {
	.bLength	= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bmAttributes	= USB_ENDPOINT_XFER_CONTROL,
};

static const struct usb_ep_ops dwc3_gadget_ep0_ops = {
	.enable		= dwc3_gadget_ep0_enable,
	.disable	= dwc3_gadget_ep0_disable,
	.alloc_request	= dwc3_gadget_ep_alloc_request,
	.free_request	= dwc3_gadget_ep_free_request,
	.queue		= dwc3_gadget_ep0_queue,
	.dequeue	= dwc3_gadget_ep_dequeue,
	.set_halt	= dwc3_gadget_ep0_set_halt,
	.set_wedge	= dwc3_gadget_ep_set_wedge,
};

static const struct usb_ep_ops dwc3_gadget_ep_ops = {
	.enable		= dwc3_gadget_ep_enable,
	.disable	= dwc3_gadget_ep_disable,
	.alloc_request	= dwc3_gadget_ep_alloc_request,
	.free_request	= dwc3_gadget_ep_free_request,
	.queue		= dwc3_gadget_ep_queue,
	.dequeue	= dwc3_gadget_ep_dequeue,
	.set_halt	= dwc3_gadget_ep_set_halt,
	.set_wedge	= dwc3_gadget_ep_set_wedge,
};

 

static void dwc3_gadget_enable_linksts_evts(struct dwc3 *dwc, bool set)
{
	u32 reg;

	if (DWC3_VER_IS_PRIOR(DWC3, 250A))
		return;

	reg = dwc3_readl(dwc->regs, DWC3_DEVTEN);
	if (set)
		reg |= DWC3_DEVTEN_ULSTCNGEN;
	else
		reg &= ~DWC3_DEVTEN_ULSTCNGEN;

	dwc3_writel(dwc->regs, DWC3_DEVTEN, reg);
}

static int dwc3_gadget_get_frame(struct usb_gadget *g)
{
	struct dwc3		*dwc = gadget_to_dwc(g);

	return __dwc3_gadget_get_frame(dwc);
}

static int __dwc3_gadget_wakeup(struct dwc3 *dwc, bool async)
{
	int			retries;

	int			ret;
	u32			reg;

	u8			link_state;

	 
	reg = dwc3_readl(dwc->regs, DWC3_DSTS);

	link_state = DWC3_DSTS_USBLNKST(reg);

	switch (link_state) {
	case DWC3_LINK_STATE_RESET:
	case DWC3_LINK_STATE_RX_DET:	 
	case DWC3_LINK_STATE_U3:	 
	case DWC3_LINK_STATE_U2:	 
	case DWC3_LINK_STATE_U1:
	case DWC3_LINK_STATE_RESUME:
		break;
	default:
		return -EINVAL;
	}

	if (async)
		dwc3_gadget_enable_linksts_evts(dwc, true);

	ret = dwc3_gadget_set_link_state(dwc, DWC3_LINK_STATE_RECOV);
	if (ret < 0) {
		dev_err(dwc->dev, "failed to put link in Recovery\n");
		dwc3_gadget_enable_linksts_evts(dwc, false);
		return ret;
	}

	 
	if (DWC3_VER_IS_PRIOR(DWC3, 194A)) {
		 
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg &= ~DWC3_DCTL_ULSTCHNGREQ_MASK;
		dwc3_writel(dwc->regs, DWC3_DCTL, reg);
	}

	 
	if (async)
		return 0;

	 
	retries = 20000;

	while (retries--) {
		reg = dwc3_readl(dwc->regs, DWC3_DSTS);

		 
		if (DWC3_DSTS_USBLNKST(reg) == DWC3_LINK_STATE_U0)
			break;
	}

	if (DWC3_DSTS_USBLNKST(reg) != DWC3_LINK_STATE_U0) {
		dev_err(dwc->dev, "failed to send remote wakeup\n");
		return -EINVAL;
	}

	return 0;
}

static int dwc3_gadget_wakeup(struct usb_gadget *g)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;
	int			ret;

	if (!dwc->wakeup_configured) {
		dev_err(dwc->dev, "remote wakeup not configured\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&dwc->lock, flags);
	if (!dwc->gadget->wakeup_armed) {
		dev_err(dwc->dev, "not armed for remote wakeup\n");
		spin_unlock_irqrestore(&dwc->lock, flags);
		return -EINVAL;
	}
	ret = __dwc3_gadget_wakeup(dwc, true);

	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static void dwc3_resume_gadget(struct dwc3 *dwc);

static int dwc3_gadget_func_wakeup(struct usb_gadget *g, int intf_id)
{
	struct  dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;
	int			ret;
	int			link_state;

	if (!dwc->wakeup_configured) {
		dev_err(dwc->dev, "remote wakeup not configured\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&dwc->lock, flags);
	 
	link_state = dwc3_gadget_get_link_state(dwc);
	if (link_state == DWC3_LINK_STATE_U3) {
		ret = __dwc3_gadget_wakeup(dwc, false);
		if (ret) {
			spin_unlock_irqrestore(&dwc->lock, flags);
			return -EINVAL;
		}
		dwc3_resume_gadget(dwc);
		dwc->suspended = false;
		dwc->link_state = DWC3_LINK_STATE_U0;
	}

	ret = dwc3_send_gadget_generic_command(dwc, DWC3_DGCMD_DEV_NOTIFICATION,
					       DWC3_DGCMDPAR_DN_FUNC_WAKE |
					       DWC3_DGCMDPAR_INTF_SEL(intf_id));
	if (ret)
		dev_err(dwc->dev, "function remote wakeup failed, ret:%d\n", ret);

	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static int dwc3_gadget_set_remote_wakeup(struct usb_gadget *g, int set)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->wakeup_configured = !!set;
	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}

static int dwc3_gadget_set_selfpowered(struct usb_gadget *g,
		int is_selfpowered)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;

	spin_lock_irqsave(&dwc->lock, flags);
	g->is_selfpowered = !!is_selfpowered;
	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}

static void dwc3_stop_active_transfers(struct dwc3 *dwc)
{
	u32 epnum;

	for (epnum = 2; epnum < dwc->num_eps; epnum++) {
		struct dwc3_ep *dep;

		dep = dwc->eps[epnum];
		if (!dep)
			continue;

		dwc3_remove_requests(dwc, dep, -ESHUTDOWN);
	}
}

static void __dwc3_gadget_set_ssp_rate(struct dwc3 *dwc)
{
	enum usb_ssp_rate	ssp_rate = dwc->gadget_ssp_rate;
	u32			reg;

	if (ssp_rate == USB_SSP_GEN_UNKNOWN)
		ssp_rate = dwc->max_ssp_rate;

	reg = dwc3_readl(dwc->regs, DWC3_DCFG);
	reg &= ~DWC3_DCFG_SPEED_MASK;
	reg &= ~DWC3_DCFG_NUMLANES(~0);

	if (ssp_rate == USB_SSP_GEN_1x2)
		reg |= DWC3_DCFG_SUPERSPEED;
	else if (dwc->max_ssp_rate != USB_SSP_GEN_1x2)
		reg |= DWC3_DCFG_SUPERSPEED_PLUS;

	if (ssp_rate != USB_SSP_GEN_2x1 &&
	    dwc->max_ssp_rate != USB_SSP_GEN_2x1)
		reg |= DWC3_DCFG_NUMLANES(1);

	dwc3_writel(dwc->regs, DWC3_DCFG, reg);
}

static void __dwc3_gadget_set_speed(struct dwc3 *dwc)
{
	enum usb_device_speed	speed;
	u32			reg;

	speed = dwc->gadget_max_speed;
	if (speed == USB_SPEED_UNKNOWN || speed > dwc->maximum_speed)
		speed = dwc->maximum_speed;

	if (speed == USB_SPEED_SUPER_PLUS &&
	    DWC3_IP_IS(DWC32)) {
		__dwc3_gadget_set_ssp_rate(dwc);
		return;
	}

	reg = dwc3_readl(dwc->regs, DWC3_DCFG);
	reg &= ~(DWC3_DCFG_SPEED_MASK);

	 
	if (DWC3_VER_IS_PRIOR(DWC3, 220A) &&
	    !dwc->dis_metastability_quirk) {
		reg |= DWC3_DCFG_SUPERSPEED;
	} else {
		switch (speed) {
		case USB_SPEED_FULL:
			reg |= DWC3_DCFG_FULLSPEED;
			break;
		case USB_SPEED_HIGH:
			reg |= DWC3_DCFG_HIGHSPEED;
			break;
		case USB_SPEED_SUPER:
			reg |= DWC3_DCFG_SUPERSPEED;
			break;
		case USB_SPEED_SUPER_PLUS:
			if (DWC3_IP_IS(DWC3))
				reg |= DWC3_DCFG_SUPERSPEED;
			else
				reg |= DWC3_DCFG_SUPERSPEED_PLUS;
			break;
		default:
			dev_err(dwc->dev, "invalid speed (%d)\n", speed);

			if (DWC3_IP_IS(DWC3))
				reg |= DWC3_DCFG_SUPERSPEED;
			else
				reg |= DWC3_DCFG_SUPERSPEED_PLUS;
		}
	}

	if (DWC3_IP_IS(DWC32) &&
	    speed > USB_SPEED_UNKNOWN &&
	    speed < USB_SPEED_SUPER_PLUS)
		reg &= ~DWC3_DCFG_NUMLANES(~0);

	dwc3_writel(dwc->regs, DWC3_DCFG, reg);
}

static int dwc3_gadget_run_stop(struct dwc3 *dwc, int is_on)
{
	u32			reg;
	u32			timeout = 2000;

	if (pm_runtime_suspended(dwc->dev))
		return 0;

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	if (is_on) {
		if (DWC3_VER_IS_WITHIN(DWC3, ANY, 187A)) {
			reg &= ~DWC3_DCTL_TRGTULST_MASK;
			reg |= DWC3_DCTL_TRGTULST_RX_DET;
		}

		if (!DWC3_VER_IS_PRIOR(DWC3, 194A))
			reg &= ~DWC3_DCTL_KEEP_CONNECT;
		reg |= DWC3_DCTL_RUN_STOP;

		__dwc3_gadget_set_speed(dwc);
		dwc->pullups_connected = true;
	} else {
		reg &= ~DWC3_DCTL_RUN_STOP;

		dwc->pullups_connected = false;
	}

	dwc3_gadget_dctl_write_safe(dwc, reg);

	do {
		usleep_range(1000, 2000);
		reg = dwc3_readl(dwc->regs, DWC3_DSTS);
		reg &= DWC3_DSTS_DEVCTRLHLT;
	} while (--timeout && !(!is_on ^ !reg));

	if (!timeout)
		return -ETIMEDOUT;

	return 0;
}

static void dwc3_gadget_disable_irq(struct dwc3 *dwc);
static void __dwc3_gadget_stop(struct dwc3 *dwc);
static int __dwc3_gadget_start(struct dwc3 *dwc);

static int dwc3_gadget_soft_disconnect(struct dwc3 *dwc)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->connected = false;

	 
	if (dwc->delayed_status)
		dwc3_ep0_send_delayed_status(dwc);

	 
	dwc3_stop_active_transfers(dwc);
	spin_unlock_irqrestore(&dwc->lock, flags);

	 
	if (dwc->ep0state != EP0_SETUP_PHASE) {
		reinit_completion(&dwc->ep0_in_setup);

		ret = wait_for_completion_timeout(&dwc->ep0_in_setup,
				msecs_to_jiffies(DWC3_PULL_UP_TIMEOUT));
		if (ret == 0) {
			dev_warn(dwc->dev, "wait for SETUP phase timed out\n");
			spin_lock_irqsave(&dwc->lock, flags);
			dwc3_ep0_reset_state(dwc);
			spin_unlock_irqrestore(&dwc->lock, flags);
		}
	}

	 
	ret = dwc3_gadget_run_stop(dwc, false);

	 
	spin_lock_irqsave(&dwc->lock, flags);
	__dwc3_gadget_stop(dwc);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static int dwc3_gadget_soft_connect(struct dwc3 *dwc)
{
	int ret;

	 
	ret = dwc3_core_soft_reset(dwc);
	if (ret)
		return ret;

	dwc3_event_buffers_setup(dwc);
	__dwc3_gadget_start(dwc);
	return dwc3_gadget_run_stop(dwc, true);
}

static int dwc3_gadget_pullup(struct usb_gadget *g, int is_on)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	int			ret;

	is_on = !!is_on;

	dwc->softconnect = is_on;

	 
	if (!is_on) {
		pm_runtime_barrier(dwc->dev);
		if (pm_runtime_suspended(dwc->dev))
			return 0;
	}

	 
	ret = pm_runtime_get_sync(dwc->dev);
	if (!ret || ret < 0) {
		pm_runtime_put(dwc->dev);
		if (ret < 0)
			pm_runtime_set_suspended(dwc->dev);
		return ret;
	}

	if (dwc->pullups_connected == is_on) {
		pm_runtime_put(dwc->dev);
		return 0;
	}

	synchronize_irq(dwc->irq_gadget);

	if (!is_on)
		ret = dwc3_gadget_soft_disconnect(dwc);
	else
		ret = dwc3_gadget_soft_connect(dwc);

	pm_runtime_put(dwc->dev);

	return ret;
}

static void dwc3_gadget_enable_irq(struct dwc3 *dwc)
{
	u32			reg;

	 
	reg = (DWC3_DEVTEN_EVNTOVERFLOWEN |
			DWC3_DEVTEN_CMDCMPLTEN |
			DWC3_DEVTEN_ERRTICERREN |
			DWC3_DEVTEN_WKUPEVTEN |
			DWC3_DEVTEN_CONNECTDONEEN |
			DWC3_DEVTEN_USBRSTEN |
			DWC3_DEVTEN_DISCONNEVTEN);

	if (DWC3_VER_IS_PRIOR(DWC3, 250A))
		reg |= DWC3_DEVTEN_ULSTCNGEN;

	 
	if (!DWC3_VER_IS_PRIOR(DWC3, 230A))
		reg |= DWC3_DEVTEN_U3L2L1SUSPEN;

	dwc3_writel(dwc->regs, DWC3_DEVTEN, reg);
}

static void dwc3_gadget_disable_irq(struct dwc3 *dwc)
{
	 
	dwc3_writel(dwc->regs, DWC3_DEVTEN, 0x00);
}

static irqreturn_t dwc3_interrupt(int irq, void *_dwc);
static irqreturn_t dwc3_thread_interrupt(int irq, void *_dwc);

 
static void dwc3_gadget_setup_nump(struct dwc3 *dwc)
{
	u32 ram2_depth;
	u32 mdwidth;
	u32 nump;
	u32 reg;

	ram2_depth = DWC3_GHWPARAMS7_RAM2_DEPTH(dwc->hwparams.hwparams7);
	mdwidth = dwc3_mdwidth(dwc);

	nump = ((ram2_depth * mdwidth / 8) - 24 - 16) / 1024;
	nump = min_t(u32, nump, 16);

	 
	reg = dwc3_readl(dwc->regs, DWC3_DCFG);
	reg &= ~DWC3_DCFG_NUMP_MASK;
	reg |= nump << DWC3_DCFG_NUMP_SHIFT;
	dwc3_writel(dwc->regs, DWC3_DCFG, reg);
}

static int __dwc3_gadget_start(struct dwc3 *dwc)
{
	struct dwc3_ep		*dep;
	int			ret = 0;
	u32			reg;

	 
	if (dwc->imod_interval) {
		dwc3_writel(dwc->regs, DWC3_DEV_IMOD(0), dwc->imod_interval);
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(0), DWC3_GEVNTCOUNT_EHB);
	} else if (dwc3_has_imod(dwc)) {
		dwc3_writel(dwc->regs, DWC3_DEV_IMOD(0), 0);
	}

	 
	reg = dwc3_readl(dwc->regs, DWC3_GRXTHRCFG);
	if (DWC3_IP_IS(DWC3))
		reg &= ~DWC3_GRXTHRCFG_PKTCNTSEL;
	else
		reg &= ~DWC31_GRXTHRCFG_PKTCNTSEL;

	dwc3_writel(dwc->regs, DWC3_GRXTHRCFG, reg);

	dwc3_gadget_setup_nump(dwc);

	 
	reg = dwc3_readl(dwc->regs, DWC3_DCFG);
	reg |= DWC3_DCFG_IGNSTRMPP;
	dwc3_writel(dwc->regs, DWC3_DCFG, reg);

	 
	if (DWC3_MST_CAPABLE(&dwc->hwparams)) {
		reg = dwc3_readl(dwc->regs, DWC3_DCFG1);
		reg &= ~DWC3_DCFG1_DIS_MST_ENH;
		dwc3_writel(dwc->regs, DWC3_DCFG1, reg);
	}

	 
	dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(512);

	dep = dwc->eps[0];
	dep->flags = 0;
	ret = __dwc3_gadget_ep_enable(dep, DWC3_DEPCFG_ACTION_INIT);
	if (ret) {
		dev_err(dwc->dev, "failed to enable %s\n", dep->name);
		goto err0;
	}

	dep = dwc->eps[1];
	dep->flags = 0;
	ret = __dwc3_gadget_ep_enable(dep, DWC3_DEPCFG_ACTION_INIT);
	if (ret) {
		dev_err(dwc->dev, "failed to enable %s\n", dep->name);
		goto err1;
	}

	 
	dwc->ep0state = EP0_SETUP_PHASE;
	dwc->ep0_bounced = false;
	dwc->link_state = DWC3_LINK_STATE_SS_DIS;
	dwc->delayed_status = false;
	dwc3_ep0_out_start(dwc);

	dwc3_gadget_enable_irq(dwc);

	return 0;

err1:
	__dwc3_gadget_ep_disable(dwc->eps[0]);

err0:
	return ret;
}

static int dwc3_gadget_start(struct usb_gadget *g,
		struct usb_gadget_driver *driver)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;
	int			ret;
	int			irq;

	irq = dwc->irq_gadget;
	ret = request_threaded_irq(irq, dwc3_interrupt, dwc3_thread_interrupt,
			IRQF_SHARED, "dwc3", dwc->ev_buf);
	if (ret) {
		dev_err(dwc->dev, "failed to request irq #%d --> %d\n",
				irq, ret);
		return ret;
	}

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->gadget_driver	= driver;
	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}

static void __dwc3_gadget_stop(struct dwc3 *dwc)
{
	dwc3_gadget_disable_irq(dwc);
	__dwc3_gadget_ep_disable(dwc->eps[0]);
	__dwc3_gadget_ep_disable(dwc->eps[1]);
}

static int dwc3_gadget_stop(struct usb_gadget *g)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->gadget_driver	= NULL;
	dwc->max_cfg_eps = 0;
	spin_unlock_irqrestore(&dwc->lock, flags);

	free_irq(dwc->irq_gadget, dwc->ev_buf);

	return 0;
}

static void dwc3_gadget_config_params(struct usb_gadget *g,
				      struct usb_dcd_config_params *params)
{
	struct dwc3		*dwc = gadget_to_dwc(g);

	params->besl_baseline = USB_DEFAULT_BESL_UNSPECIFIED;
	params->besl_deep = USB_DEFAULT_BESL_UNSPECIFIED;

	 
	if (!dwc->dis_enblslpm_quirk) {
		 
		params->besl_baseline = 1;
		if (dwc->is_utmi_l1_suspend)
			params->besl_deep =
				clamp_t(u8, dwc->hird_threshold, 2, 15);
	}

	 
	if (dwc->dis_u1_entry_quirk)
		params->bU1devExitLat = 0;
	else
		params->bU1devExitLat = DWC3_DEFAULT_U1_DEV_EXIT_LAT;

	 
	if (dwc->dis_u2_entry_quirk)
		params->bU2DevExitLat = 0;
	else
		params->bU2DevExitLat =
				cpu_to_le16(DWC3_DEFAULT_U2_DEV_EXIT_LAT);
}

static void dwc3_gadget_set_speed(struct usb_gadget *g,
				  enum usb_device_speed speed)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->gadget_max_speed = speed;
	spin_unlock_irqrestore(&dwc->lock, flags);
}

static void dwc3_gadget_set_ssp_rate(struct usb_gadget *g,
				     enum usb_ssp_rate rate)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->gadget_max_speed = USB_SPEED_SUPER_PLUS;
	dwc->gadget_ssp_rate = rate;
	spin_unlock_irqrestore(&dwc->lock, flags);
}

static int dwc3_gadget_vbus_draw(struct usb_gadget *g, unsigned int mA)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	union power_supply_propval	val = {0};
	int				ret;

	if (dwc->usb2_phy)
		return usb_phy_set_power(dwc->usb2_phy, mA);

	if (!dwc->usb_psy)
		return -EOPNOTSUPP;

	val.intval = 1000 * mA;
	ret = power_supply_set_property(dwc->usb_psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);

	return ret;
}

 
static int dwc3_gadget_check_config(struct usb_gadget *g)
{
	struct dwc3 *dwc = gadget_to_dwc(g);
	struct usb_ep *ep;
	int fifo_size = 0;
	int ram1_depth;
	int ep_num = 0;

	if (!dwc->do_fifo_resize)
		return 0;

	list_for_each_entry(ep, &g->ep_list, ep_list) {
		 
		if (ep->claimed && (ep->address & USB_DIR_IN))
			ep_num++;
	}

	if (ep_num <= dwc->max_cfg_eps)
		return 0;

	 
	dwc->max_cfg_eps = ep_num;

	fifo_size = dwc3_gadget_calc_tx_fifo_size(dwc, dwc->max_cfg_eps);
	 
	fifo_size += dwc->max_cfg_eps;

	 
	ram1_depth = DWC3_RAM1_DEPTH(dwc->hwparams.hwparams7);
	if (fifo_size > ram1_depth)
		return -ENOMEM;

	return 0;
}

static void dwc3_gadget_async_callbacks(struct usb_gadget *g, bool enable)
{
	struct dwc3		*dwc = gadget_to_dwc(g);
	unsigned long		flags;

	spin_lock_irqsave(&dwc->lock, flags);
	dwc->async_callbacks = enable;
	spin_unlock_irqrestore(&dwc->lock, flags);
}

static const struct usb_gadget_ops dwc3_gadget_ops = {
	.get_frame		= dwc3_gadget_get_frame,
	.wakeup			= dwc3_gadget_wakeup,
	.func_wakeup		= dwc3_gadget_func_wakeup,
	.set_remote_wakeup	= dwc3_gadget_set_remote_wakeup,
	.set_selfpowered	= dwc3_gadget_set_selfpowered,
	.pullup			= dwc3_gadget_pullup,
	.udc_start		= dwc3_gadget_start,
	.udc_stop		= dwc3_gadget_stop,
	.udc_set_speed		= dwc3_gadget_set_speed,
	.udc_set_ssp_rate	= dwc3_gadget_set_ssp_rate,
	.get_config_params	= dwc3_gadget_config_params,
	.vbus_draw		= dwc3_gadget_vbus_draw,
	.check_config		= dwc3_gadget_check_config,
	.udc_async_callbacks	= dwc3_gadget_async_callbacks,
};

 

static int dwc3_gadget_init_control_endpoint(struct dwc3_ep *dep)
{
	struct dwc3 *dwc = dep->dwc;

	usb_ep_set_maxpacket_limit(&dep->endpoint, 512);
	dep->endpoint.maxburst = 1;
	dep->endpoint.ops = &dwc3_gadget_ep0_ops;
	if (!dep->direction)
		dwc->gadget->ep0 = &dep->endpoint;

	dep->endpoint.caps.type_control = true;

	return 0;
}

static int dwc3_gadget_init_in_endpoint(struct dwc3_ep *dep)
{
	struct dwc3 *dwc = dep->dwc;
	u32 mdwidth;
	int size;
	int maxpacket;

	mdwidth = dwc3_mdwidth(dwc);

	 
	mdwidth /= 8;

	size = dwc3_readl(dwc->regs, DWC3_GTXFIFOSIZ(dep->number >> 1));
	if (DWC3_IP_IS(DWC3))
		size = DWC3_GTXFIFOSIZ_TXFDEP(size);
	else
		size = DWC31_GTXFIFOSIZ_TXFDEP(size);

	 
	if (DWC3_VER_IS_PRIOR(DWC3, 290A))
		maxpacket = mdwidth * (size - 1);
	else
		maxpacket = mdwidth * ((size - 1) - 1) - mdwidth;

	 
	size = min_t(int, maxpacket, 1024);
	usb_ep_set_maxpacket_limit(&dep->endpoint, size);

	dep->endpoint.max_streams = 16;
	dep->endpoint.ops = &dwc3_gadget_ep_ops;
	list_add_tail(&dep->endpoint.ep_list,
			&dwc->gadget->ep_list);
	dep->endpoint.caps.type_iso = true;
	dep->endpoint.caps.type_bulk = true;
	dep->endpoint.caps.type_int = true;

	return dwc3_alloc_trb_pool(dep);
}

static int dwc3_gadget_init_out_endpoint(struct dwc3_ep *dep)
{
	struct dwc3 *dwc = dep->dwc;
	u32 mdwidth;
	int size;

	mdwidth = dwc3_mdwidth(dwc);

	 
	mdwidth /= 8;

	 
	size = dwc3_readl(dwc->regs, DWC3_GRXFIFOSIZ(0));
	if (DWC3_IP_IS(DWC3))
		size = DWC3_GRXFIFOSIZ_RXFDEP(size);
	else
		size = DWC31_GRXFIFOSIZ_RXFDEP(size);

	 
	size *= mdwidth;

	 
	size -= (3 * 8) + 16;
	if (size < 0)
		size = 0;
	else
		size /= 3;

	usb_ep_set_maxpacket_limit(&dep->endpoint, size);
	dep->endpoint.max_streams = 16;
	dep->endpoint.ops = &dwc3_gadget_ep_ops;
	list_add_tail(&dep->endpoint.ep_list,
			&dwc->gadget->ep_list);
	dep->endpoint.caps.type_iso = true;
	dep->endpoint.caps.type_bulk = true;
	dep->endpoint.caps.type_int = true;

	return dwc3_alloc_trb_pool(dep);
}

static int dwc3_gadget_init_endpoint(struct dwc3 *dwc, u8 epnum)
{
	struct dwc3_ep			*dep;
	bool				direction = epnum & 1;
	int				ret;
	u8				num = epnum >> 1;

	dep = kzalloc(sizeof(*dep), GFP_KERNEL);
	if (!dep)
		return -ENOMEM;

	dep->dwc = dwc;
	dep->number = epnum;
	dep->direction = direction;
	dep->regs = dwc->regs + DWC3_DEP_BASE(epnum);
	dwc->eps[epnum] = dep;
	dep->combo_num = 0;
	dep->start_cmd_status = 0;

	snprintf(dep->name, sizeof(dep->name), "ep%u%s", num,
			direction ? "in" : "out");

	dep->endpoint.name = dep->name;

	if (!(dep->number > 1)) {
		dep->endpoint.desc = &dwc3_gadget_ep0_desc;
		dep->endpoint.comp_desc = NULL;
	}

	if (num == 0)
		ret = dwc3_gadget_init_control_endpoint(dep);
	else if (direction)
		ret = dwc3_gadget_init_in_endpoint(dep);
	else
		ret = dwc3_gadget_init_out_endpoint(dep);

	if (ret)
		return ret;

	dep->endpoint.caps.dir_in = direction;
	dep->endpoint.caps.dir_out = !direction;

	INIT_LIST_HEAD(&dep->pending_list);
	INIT_LIST_HEAD(&dep->started_list);
	INIT_LIST_HEAD(&dep->cancelled_list);

	dwc3_debugfs_create_endpoint_dir(dep);

	return 0;
}

static int dwc3_gadget_init_endpoints(struct dwc3 *dwc, u8 total)
{
	u8				epnum;

	INIT_LIST_HEAD(&dwc->gadget->ep_list);

	for (epnum = 0; epnum < total; epnum++) {
		int			ret;

		ret = dwc3_gadget_init_endpoint(dwc, epnum);
		if (ret)
			return ret;
	}

	return 0;
}

static void dwc3_gadget_free_endpoints(struct dwc3 *dwc)
{
	struct dwc3_ep			*dep;
	u8				epnum;

	for (epnum = 0; epnum < DWC3_ENDPOINTS_NUM; epnum++) {
		dep = dwc->eps[epnum];
		if (!dep)
			continue;
		 
		if (epnum != 0 && epnum != 1) {
			dwc3_free_trb_pool(dep);
			list_del(&dep->endpoint.ep_list);
		}

		dwc3_debugfs_remove_endpoint_dir(dep);
		kfree(dep);
	}
}

 

static int dwc3_gadget_ep_reclaim_completed_trb(struct dwc3_ep *dep,
		struct dwc3_request *req, struct dwc3_trb *trb,
		const struct dwc3_event_depevt *event, int status, int chain)
{
	unsigned int		count;

	dwc3_ep_inc_deq(dep);

	trace_dwc3_complete_trb(dep, trb);
	req->num_trbs--;

	 
	if (chain && (trb->ctrl & DWC3_TRB_CTRL_HWO))
		trb->ctrl &= ~DWC3_TRB_CTRL_HWO;

	 
	if (usb_endpoint_xfer_isoc(dep->endpoint.desc) &&
	    (trb->ctrl & DWC3_TRBCTL_ISOCHRONOUS_FIRST)) {
		unsigned int frame_number;

		frame_number = DWC3_TRB_CTRL_GET_SID_SOFN(trb->ctrl);
		frame_number &= ~(dep->interval - 1);
		req->request.frame_number = frame_number;
	}

	 
	if (trb->bpl == lower_32_bits(dep->dwc->bounce_addr) &&
	    trb->bph == upper_32_bits(dep->dwc->bounce_addr)) {
		trb->ctrl &= ~DWC3_TRB_CTRL_HWO;
		return 1;
	}

	count = trb->size & DWC3_TRB_SIZE_MASK;
	req->remaining += count;

	if ((trb->ctrl & DWC3_TRB_CTRL_HWO) && status != -ESHUTDOWN)
		return 1;

	if (event->status & DEPEVT_STATUS_SHORT && !chain)
		return 1;

	if ((trb->ctrl & DWC3_TRB_CTRL_ISP_IMI) &&
	    DWC3_TRB_SIZE_TRBSTS(trb->size) == DWC3_TRBSTS_MISSED_ISOC)
		return 1;

	if ((trb->ctrl & DWC3_TRB_CTRL_IOC) ||
	    (trb->ctrl & DWC3_TRB_CTRL_LST))
		return 1;

	return 0;
}

static int dwc3_gadget_ep_reclaim_trb_sg(struct dwc3_ep *dep,
		struct dwc3_request *req, const struct dwc3_event_depevt *event,
		int status)
{
	struct dwc3_trb *trb = &dep->trb_pool[dep->trb_dequeue];
	struct scatterlist *sg = req->sg;
	struct scatterlist *s;
	unsigned int num_queued = req->num_queued_sgs;
	unsigned int i;
	int ret = 0;

	for_each_sg(sg, s, num_queued, i) {
		trb = &dep->trb_pool[dep->trb_dequeue];

		req->sg = sg_next(s);
		req->num_queued_sgs--;

		ret = dwc3_gadget_ep_reclaim_completed_trb(dep, req,
				trb, event, status, true);
		if (ret)
			break;
	}

	return ret;
}

static int dwc3_gadget_ep_reclaim_trb_linear(struct dwc3_ep *dep,
		struct dwc3_request *req, const struct dwc3_event_depevt *event,
		int status)
{
	struct dwc3_trb *trb = &dep->trb_pool[dep->trb_dequeue];

	return dwc3_gadget_ep_reclaim_completed_trb(dep, req, trb,
			event, status, false);
}

static bool dwc3_gadget_ep_request_completed(struct dwc3_request *req)
{
	return req->num_pending_sgs == 0 && req->num_queued_sgs == 0;
}

static int dwc3_gadget_ep_cleanup_completed_request(struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event,
		struct dwc3_request *req, int status)
{
	int request_status;
	int ret;

	if (req->request.num_mapped_sgs)
		ret = dwc3_gadget_ep_reclaim_trb_sg(dep, req, event,
				status);
	else
		ret = dwc3_gadget_ep_reclaim_trb_linear(dep, req, event,
				status);

	req->request.actual = req->request.length - req->remaining;

	if (!dwc3_gadget_ep_request_completed(req))
		goto out;

	if (req->needs_extra_trb) {
		ret = dwc3_gadget_ep_reclaim_trb_linear(dep, req, event,
				status);
		req->needs_extra_trb = false;
	}

	 
	if (req->request.no_interrupt) {
		struct dwc3_trb *trb;

		trb = dwc3_ep_prev_trb(dep, dep->trb_dequeue);
		switch (DWC3_TRB_SIZE_TRBSTS(trb->size)) {
		case DWC3_TRBSTS_MISSED_ISOC:
			 
			request_status = -EXDEV;
			break;
		case DWC3_TRB_STS_XFER_IN_PROG:
			 
		case DWC3_TRBSTS_SETUP_PENDING:
			 
		case DWC3_TRBSTS_OK:
		default:
			request_status = 0;
			break;
		}
	} else {
		request_status = status;
	}

	dwc3_gadget_giveback(dep, req, request_status);

out:
	return ret;
}

static void dwc3_gadget_ep_cleanup_completed_requests(struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event, int status)
{
	struct dwc3_request	*req;

	while (!list_empty(&dep->started_list)) {
		int ret;

		req = next_request(&dep->started_list);
		ret = dwc3_gadget_ep_cleanup_completed_request(dep, event,
				req, status);
		if (ret)
			break;
		 
		if (!dep->endpoint.desc)
			break;
	}
}

static bool dwc3_gadget_ep_should_continue(struct dwc3_ep *dep)
{
	struct dwc3_request	*req;
	struct dwc3		*dwc = dep->dwc;

	if (!dep->endpoint.desc || !dwc->pullups_connected ||
	    !dwc->connected)
		return false;

	if (!list_empty(&dep->pending_list))
		return true;

	 
	req = next_request(&dep->started_list);
	if (!req)
		return false;

	return !dwc3_gadget_ep_request_completed(req);
}

static void dwc3_gadget_endpoint_frame_from_event(struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event)
{
	dep->frame_number = event->parameters;
}

static bool dwc3_gadget_endpoint_trbs_complete(struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event, int status)
{
	struct dwc3		*dwc = dep->dwc;
	bool			no_started_trb = true;

	dwc3_gadget_ep_cleanup_completed_requests(dep, event, status);

	if (dep->flags & DWC3_EP_END_TRANSFER_PENDING)
		goto out;

	if (!dep->endpoint.desc)
		return no_started_trb;

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc) &&
		list_empty(&dep->started_list) &&
		(list_empty(&dep->pending_list) || status == -EXDEV))
		dwc3_stop_active_transfer(dep, true, true);
	else if (dwc3_gadget_ep_should_continue(dep))
		if (__dwc3_gadget_kick_transfer(dep) == 0)
			no_started_trb = false;

out:
	 
	if (DWC3_VER_IS_PRIOR(DWC3, 183A)) {
		u32		reg;
		int		i;

		for (i = 0; i < DWC3_ENDPOINTS_NUM; i++) {
			dep = dwc->eps[i];

			if (!(dep->flags & DWC3_EP_ENABLED))
				continue;

			if (!list_empty(&dep->started_list))
				return no_started_trb;
		}

		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg |= dwc->u1u2;
		dwc3_writel(dwc->regs, DWC3_DCTL, reg);

		dwc->u1u2 = 0;
	}

	return no_started_trb;
}

static void dwc3_gadget_endpoint_transfer_in_progress(struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event)
{
	int status = 0;

	if (!dep->endpoint.desc)
		return;

	if (usb_endpoint_xfer_isoc(dep->endpoint.desc))
		dwc3_gadget_endpoint_frame_from_event(dep, event);

	if (event->status & DEPEVT_STATUS_BUSERR)
		status = -ECONNRESET;

	if (event->status & DEPEVT_STATUS_MISSED_ISOC)
		status = -EXDEV;

	dwc3_gadget_endpoint_trbs_complete(dep, event, status);
}

static void dwc3_gadget_endpoint_transfer_complete(struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event)
{
	int status = 0;

	dep->flags &= ~DWC3_EP_TRANSFER_STARTED;

	if (event->status & DEPEVT_STATUS_BUSERR)
		status = -ECONNRESET;

	if (dwc3_gadget_endpoint_trbs_complete(dep, event, status))
		dep->flags &= ~DWC3_EP_WAIT_TRANSFER_COMPLETE;
}

static void dwc3_gadget_endpoint_transfer_not_ready(struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event)
{
	dwc3_gadget_endpoint_frame_from_event(dep, event);

	 
	if (dep->flags & DWC3_EP_END_TRANSFER_PENDING)
		return;

	(void) __dwc3_gadget_start_isoc(dep);
}

static void dwc3_gadget_endpoint_command_complete(struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event)
{
	u8 cmd = DEPEVT_PARAMETER_CMD(event->parameters);

	if (cmd != DWC3_DEPCMD_ENDTRANSFER)
		return;

	 
	if (dep->stream_capable)
		dep->flags |= DWC3_EP_IGNORE_NEXT_NOSTREAM;

	dep->flags &= ~DWC3_EP_END_TRANSFER_PENDING;
	dep->flags &= ~DWC3_EP_TRANSFER_STARTED;
	dwc3_gadget_ep_cleanup_cancelled_requests(dep);

	if (dep->flags & DWC3_EP_PENDING_CLEAR_STALL) {
		struct dwc3 *dwc = dep->dwc;

		dep->flags &= ~DWC3_EP_PENDING_CLEAR_STALL;
		if (dwc3_send_clear_stall_ep_cmd(dep)) {
			struct usb_ep *ep0 = &dwc->eps[0]->endpoint;

			dev_err(dwc->dev, "failed to clear STALL on %s\n", dep->name);
			if (dwc->delayed_status)
				__dwc3_gadget_ep0_set_halt(ep0, 1);
			return;
		}

		dep->flags &= ~(DWC3_EP_STALL | DWC3_EP_WEDGE);
		if (dwc->clear_stall_protocol == dep->number)
			dwc3_ep0_send_delayed_status(dwc);
	}

	if ((dep->flags & DWC3_EP_DELAY_START) &&
	    !usb_endpoint_xfer_isoc(dep->endpoint.desc))
		__dwc3_gadget_kick_transfer(dep);

	dep->flags &= ~DWC3_EP_DELAY_START;
}

static void dwc3_gadget_endpoint_stream_event(struct dwc3_ep *dep,
		const struct dwc3_event_depevt *event)
{
	struct dwc3 *dwc = dep->dwc;

	if (event->status == DEPEVT_STREAMEVT_FOUND) {
		dep->flags |= DWC3_EP_FIRST_STREAM_PRIMED;
		goto out;
	}

	 
	switch (event->parameters) {
	case DEPEVT_STREAM_PRIME:
		 
		if (dep->flags & DWC3_EP_FORCE_RESTART_STREAM) {
			if (dep->flags & DWC3_EP_FIRST_STREAM_PRIMED)
				dep->flags &= ~DWC3_EP_FORCE_RESTART_STREAM;
			else
				dep->flags |= DWC3_EP_FIRST_STREAM_PRIMED;
		}

		break;
	case DEPEVT_STREAM_NOSTREAM:
		if ((dep->flags & DWC3_EP_IGNORE_NEXT_NOSTREAM) ||
		    !(dep->flags & DWC3_EP_FORCE_RESTART_STREAM) ||
		    (!DWC3_MST_CAPABLE(&dwc->hwparams) &&
		     !(dep->flags & DWC3_EP_WAIT_TRANSFER_COMPLETE)))
			break;

		 
		if (DWC3_VER_IS_WITHIN(DWC32, 100A, ANY)) {
			unsigned int cmd = DWC3_DGCMD_SET_ENDPOINT_PRIME;

			dwc3_send_gadget_generic_command(dwc, cmd, dep->number);
		} else {
			dep->flags |= DWC3_EP_DELAY_START;
			dwc3_stop_active_transfer(dep, true, true);
			return;
		}
		break;
	}

out:
	dep->flags &= ~DWC3_EP_IGNORE_NEXT_NOSTREAM;
}

static void dwc3_endpoint_interrupt(struct dwc3 *dwc,
		const struct dwc3_event_depevt *event)
{
	struct dwc3_ep		*dep;
	u8			epnum = event->endpoint_number;

	dep = dwc->eps[epnum];

	if (!(dep->flags & DWC3_EP_ENABLED)) {
		if ((epnum > 1) && !(dep->flags & DWC3_EP_TRANSFER_STARTED))
			return;

		 
		if ((event->endpoint_event != DWC3_DEPEVT_EPCMDCMPLT) &&
			!(epnum <= 1 && event->endpoint_event == DWC3_DEPEVT_XFERCOMPLETE))
			return;
	}

	if (epnum == 0 || epnum == 1) {
		dwc3_ep0_interrupt(dwc, event);
		return;
	}

	switch (event->endpoint_event) {
	case DWC3_DEPEVT_XFERINPROGRESS:
		dwc3_gadget_endpoint_transfer_in_progress(dep, event);
		break;
	case DWC3_DEPEVT_XFERNOTREADY:
		dwc3_gadget_endpoint_transfer_not_ready(dep, event);
		break;
	case DWC3_DEPEVT_EPCMDCMPLT:
		dwc3_gadget_endpoint_command_complete(dep, event);
		break;
	case DWC3_DEPEVT_XFERCOMPLETE:
		dwc3_gadget_endpoint_transfer_complete(dep, event);
		break;
	case DWC3_DEPEVT_STREAMEVT:
		dwc3_gadget_endpoint_stream_event(dep, event);
		break;
	case DWC3_DEPEVT_RXTXFIFOEVT:
		break;
	default:
		dev_err(dwc->dev, "unknown endpoint event %d\n", event->endpoint_event);
		break;
	}
}

static void dwc3_disconnect_gadget(struct dwc3 *dwc)
{
	if (dwc->async_callbacks && dwc->gadget_driver->disconnect) {
		spin_unlock(&dwc->lock);
		dwc->gadget_driver->disconnect(dwc->gadget);
		spin_lock(&dwc->lock);
	}
}

static void dwc3_suspend_gadget(struct dwc3 *dwc)
{
	if (dwc->async_callbacks && dwc->gadget_driver->suspend) {
		spin_unlock(&dwc->lock);
		dwc->gadget_driver->suspend(dwc->gadget);
		spin_lock(&dwc->lock);
	}
}

static void dwc3_resume_gadget(struct dwc3 *dwc)
{
	if (dwc->async_callbacks && dwc->gadget_driver->resume) {
		spin_unlock(&dwc->lock);
		dwc->gadget_driver->resume(dwc->gadget);
		spin_lock(&dwc->lock);
	}
}

static void dwc3_reset_gadget(struct dwc3 *dwc)
{
	if (!dwc->gadget_driver)
		return;

	if (dwc->async_callbacks && dwc->gadget->speed != USB_SPEED_UNKNOWN) {
		spin_unlock(&dwc->lock);
		usb_gadget_udc_reset(dwc->gadget, dwc->gadget_driver);
		spin_lock(&dwc->lock);
	}
}

void dwc3_stop_active_transfer(struct dwc3_ep *dep, bool force,
	bool interrupt)
{
	struct dwc3 *dwc = dep->dwc;

	 
	if (dep->number <= 1 && dwc->ep0state != EP0_DATA_PHASE)
		return;

	if (interrupt && (dep->flags & DWC3_EP_DELAY_STOP))
		return;

	if (!(dep->flags & DWC3_EP_TRANSFER_STARTED) ||
	    (dep->flags & DWC3_EP_END_TRANSFER_PENDING))
		return;

	 
	if (dwc->ep0state != EP0_SETUP_PHASE && !dwc->delayed_status) {
		dep->flags |= DWC3_EP_DELAY_STOP;
		return;
	}

	 

	__dwc3_stop_active_transfer(dep, force, interrupt);
}

static void dwc3_clear_stall_all_ep(struct dwc3 *dwc)
{
	u32 epnum;

	for (epnum = 1; epnum < DWC3_ENDPOINTS_NUM; epnum++) {
		struct dwc3_ep *dep;
		int ret;

		dep = dwc->eps[epnum];
		if (!dep)
			continue;

		if (!(dep->flags & DWC3_EP_STALL))
			continue;

		dep->flags &= ~DWC3_EP_STALL;

		ret = dwc3_send_clear_stall_ep_cmd(dep);
		WARN_ON_ONCE(ret);
	}
}

static void dwc3_gadget_disconnect_interrupt(struct dwc3 *dwc)
{
	int			reg;

	dwc->suspended = false;

	dwc3_gadget_set_link_state(dwc, DWC3_LINK_STATE_RX_DET);

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~DWC3_DCTL_INITU1ENA;
	reg &= ~DWC3_DCTL_INITU2ENA;
	dwc3_gadget_dctl_write_safe(dwc, reg);

	dwc->connected = false;

	dwc3_disconnect_gadget(dwc);

	dwc->gadget->speed = USB_SPEED_UNKNOWN;
	dwc->setup_packet_pending = false;
	dwc->gadget->wakeup_armed = false;
	dwc3_gadget_enable_linksts_evts(dwc, false);
	usb_gadget_set_state(dwc->gadget, USB_STATE_NOTATTACHED);

	dwc3_ep0_reset_state(dwc);

	 
	pm_request_idle(dwc->dev);
}

static void dwc3_gadget_reset_interrupt(struct dwc3 *dwc)
{
	u32			reg;

	dwc->suspended = false;

	 
	dwc->connected = false;

	 
	if (DWC3_VER_IS_PRIOR(DWC3, 188A)) {
		if (dwc->setup_packet_pending)
			dwc3_gadget_disconnect_interrupt(dwc);
	}

	dwc3_reset_gadget(dwc);

	 
	dwc3_ep0_reset_state(dwc);

	 
	dwc3_stop_active_transfers(dwc);
	dwc->connected = true;

	reg = dwc3_readl(dwc->regs, DWC3_DCTL);
	reg &= ~DWC3_DCTL_TSTCTRL_MASK;
	dwc3_gadget_dctl_write_safe(dwc, reg);
	dwc->test_mode = false;
	dwc->gadget->wakeup_armed = false;
	dwc3_gadget_enable_linksts_evts(dwc, false);
	dwc3_clear_stall_all_ep(dwc);

	 
	reg = dwc3_readl(dwc->regs, DWC3_DCFG);
	reg &= ~(DWC3_DCFG_DEVADDR_MASK);
	dwc3_writel(dwc->regs, DWC3_DCFG, reg);
}

static void dwc3_gadget_conndone_interrupt(struct dwc3 *dwc)
{
	struct dwc3_ep		*dep;
	int			ret;
	u32			reg;
	u8			lanes = 1;
	u8			speed;

	if (!dwc->softconnect)
		return;

	reg = dwc3_readl(dwc->regs, DWC3_DSTS);
	speed = reg & DWC3_DSTS_CONNECTSPD;
	dwc->speed = speed;

	if (DWC3_IP_IS(DWC32))
		lanes = DWC3_DSTS_CONNLANES(reg) + 1;

	dwc->gadget->ssp_rate = USB_SSP_GEN_UNKNOWN;

	 

	switch (speed) {
	case DWC3_DSTS_SUPERSPEED_PLUS:
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(512);
		dwc->gadget->ep0->maxpacket = 512;
		dwc->gadget->speed = USB_SPEED_SUPER_PLUS;

		if (lanes > 1)
			dwc->gadget->ssp_rate = USB_SSP_GEN_2x2;
		else
			dwc->gadget->ssp_rate = USB_SSP_GEN_2x1;
		break;
	case DWC3_DSTS_SUPERSPEED:
		 
		if (DWC3_VER_IS_PRIOR(DWC3, 190A))
			dwc3_gadget_reset_interrupt(dwc);

		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(512);
		dwc->gadget->ep0->maxpacket = 512;
		dwc->gadget->speed = USB_SPEED_SUPER;

		if (lanes > 1) {
			dwc->gadget->speed = USB_SPEED_SUPER_PLUS;
			dwc->gadget->ssp_rate = USB_SSP_GEN_1x2;
		}
		break;
	case DWC3_DSTS_HIGHSPEED:
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(64);
		dwc->gadget->ep0->maxpacket = 64;
		dwc->gadget->speed = USB_SPEED_HIGH;
		break;
	case DWC3_DSTS_FULLSPEED:
		dwc3_gadget_ep0_desc.wMaxPacketSize = cpu_to_le16(64);
		dwc->gadget->ep0->maxpacket = 64;
		dwc->gadget->speed = USB_SPEED_FULL;
		break;
	}

	dwc->eps[1]->endpoint.maxpacket = dwc->gadget->ep0->maxpacket;

	 

	if (!DWC3_VER_IS_WITHIN(DWC3, ANY, 194A) &&
	    !dwc->usb2_gadget_lpm_disable &&
	    (speed != DWC3_DSTS_SUPERSPEED) &&
	    (speed != DWC3_DSTS_SUPERSPEED_PLUS)) {
		reg = dwc3_readl(dwc->regs, DWC3_DCFG);
		reg |= DWC3_DCFG_LPM_CAP;
		dwc3_writel(dwc->regs, DWC3_DCFG, reg);

		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg &= ~(DWC3_DCTL_HIRD_THRES_MASK | DWC3_DCTL_L1_HIBER_EN);

		reg |= DWC3_DCTL_HIRD_THRES(dwc->hird_threshold |
					    (dwc->is_utmi_l1_suspend << 4));

		 
		WARN_ONCE(DWC3_VER_IS_PRIOR(DWC3, 240A) && dwc->has_lpm_erratum,
				"LPM Erratum not available on dwc3 revisions < 2.40a\n");

		if (dwc->has_lpm_erratum && !DWC3_VER_IS_PRIOR(DWC3, 240A))
			reg |= DWC3_DCTL_NYET_THRES(dwc->lpm_nyet_threshold);

		dwc3_gadget_dctl_write_safe(dwc, reg);
	} else {
		if (dwc->usb2_gadget_lpm_disable) {
			reg = dwc3_readl(dwc->regs, DWC3_DCFG);
			reg &= ~DWC3_DCFG_LPM_CAP;
			dwc3_writel(dwc->regs, DWC3_DCFG, reg);
		}

		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		reg &= ~DWC3_DCTL_HIRD_THRES_MASK;
		dwc3_gadget_dctl_write_safe(dwc, reg);
	}

	dep = dwc->eps[0];
	ret = __dwc3_gadget_ep_enable(dep, DWC3_DEPCFG_ACTION_MODIFY);
	if (ret) {
		dev_err(dwc->dev, "failed to enable %s\n", dep->name);
		return;
	}

	dep = dwc->eps[1];
	ret = __dwc3_gadget_ep_enable(dep, DWC3_DEPCFG_ACTION_MODIFY);
	if (ret) {
		dev_err(dwc->dev, "failed to enable %s\n", dep->name);
		return;
	}

	 
}

static void dwc3_gadget_wakeup_interrupt(struct dwc3 *dwc, unsigned int evtinfo)
{
	dwc->suspended = false;

	 

	if (dwc->async_callbacks && dwc->gadget_driver->resume) {
		spin_unlock(&dwc->lock);
		dwc->gadget_driver->resume(dwc->gadget);
		spin_lock(&dwc->lock);
	}

	dwc->link_state = evtinfo & DWC3_LINK_STATE_MASK;
}

static void dwc3_gadget_linksts_change_interrupt(struct dwc3 *dwc,
		unsigned int evtinfo)
{
	enum dwc3_link_state	next = evtinfo & DWC3_LINK_STATE_MASK;
	unsigned int		pwropt;

	 
	pwropt = DWC3_GHWPARAMS1_EN_PWROPT(dwc->hwparams.hwparams1);
	if (DWC3_VER_IS_PRIOR(DWC3, 250A) &&
			(pwropt != DWC3_GHWPARAMS1_EN_PWROPT_HIB)) {
		if ((dwc->link_state == DWC3_LINK_STATE_U3) &&
				(next == DWC3_LINK_STATE_RESUME)) {
			return;
		}
	}

	 
	if (DWC3_VER_IS_PRIOR(DWC3, 183A)) {
		if (next == DWC3_LINK_STATE_U0) {
			u32	u1u2;
			u32	reg;

			switch (dwc->link_state) {
			case DWC3_LINK_STATE_U1:
			case DWC3_LINK_STATE_U2:
				reg = dwc3_readl(dwc->regs, DWC3_DCTL);
				u1u2 = reg & (DWC3_DCTL_INITU2ENA
						| DWC3_DCTL_ACCEPTU2ENA
						| DWC3_DCTL_INITU1ENA
						| DWC3_DCTL_ACCEPTU1ENA);

				if (!dwc->u1u2)
					dwc->u1u2 = reg & u1u2;

				reg &= ~u1u2;

				dwc3_gadget_dctl_write_safe(dwc, reg);
				break;
			default:
				 
				break;
			}
		}
	}

	switch (next) {
	case DWC3_LINK_STATE_U0:
		if (dwc->gadget->wakeup_armed) {
			dwc3_gadget_enable_linksts_evts(dwc, false);
			dwc3_resume_gadget(dwc);
			dwc->suspended = false;
		}
		break;
	case DWC3_LINK_STATE_U1:
		if (dwc->speed == USB_SPEED_SUPER)
			dwc3_suspend_gadget(dwc);
		break;
	case DWC3_LINK_STATE_U2:
	case DWC3_LINK_STATE_U3:
		dwc3_suspend_gadget(dwc);
		break;
	case DWC3_LINK_STATE_RESUME:
		dwc3_resume_gadget(dwc);
		break;
	default:
		 
		break;
	}

	dwc->link_state = next;
}

static void dwc3_gadget_suspend_interrupt(struct dwc3 *dwc,
					  unsigned int evtinfo)
{
	enum dwc3_link_state next = evtinfo & DWC3_LINK_STATE_MASK;

	if (!dwc->suspended && next == DWC3_LINK_STATE_U3) {
		dwc->suspended = true;
		dwc3_suspend_gadget(dwc);
	}

	dwc->link_state = next;
}

static void dwc3_gadget_interrupt(struct dwc3 *dwc,
		const struct dwc3_event_devt *event)
{
	switch (event->type) {
	case DWC3_DEVICE_EVENT_DISCONNECT:
		dwc3_gadget_disconnect_interrupt(dwc);
		break;
	case DWC3_DEVICE_EVENT_RESET:
		dwc3_gadget_reset_interrupt(dwc);
		break;
	case DWC3_DEVICE_EVENT_CONNECT_DONE:
		dwc3_gadget_conndone_interrupt(dwc);
		break;
	case DWC3_DEVICE_EVENT_WAKEUP:
		dwc3_gadget_wakeup_interrupt(dwc, event->event_info);
		break;
	case DWC3_DEVICE_EVENT_HIBER_REQ:
		dev_WARN_ONCE(dwc->dev, true, "unexpected hibernation event\n");
		break;
	case DWC3_DEVICE_EVENT_LINK_STATUS_CHANGE:
		dwc3_gadget_linksts_change_interrupt(dwc, event->event_info);
		break;
	case DWC3_DEVICE_EVENT_SUSPEND:
		 
		if (!DWC3_VER_IS_PRIOR(DWC3, 230A))
			dwc3_gadget_suspend_interrupt(dwc, event->event_info);
		break;
	case DWC3_DEVICE_EVENT_SOF:
	case DWC3_DEVICE_EVENT_ERRATIC_ERROR:
	case DWC3_DEVICE_EVENT_CMD_CMPL:
	case DWC3_DEVICE_EVENT_OVERFLOW:
		break;
	default:
		dev_WARN(dwc->dev, "UNKNOWN IRQ %d\n", event->type);
	}
}

static void dwc3_process_event_entry(struct dwc3 *dwc,
		const union dwc3_event *event)
{
	trace_dwc3_event(event->raw, dwc);

	if (!event->type.is_devspec)
		dwc3_endpoint_interrupt(dwc, &event->depevt);
	else if (event->type.type == DWC3_EVENT_TYPE_DEV)
		dwc3_gadget_interrupt(dwc, &event->devt);
	else
		dev_err(dwc->dev, "UNKNOWN IRQ type %d\n", event->raw);
}

static irqreturn_t dwc3_process_event_buf(struct dwc3_event_buffer *evt)
{
	struct dwc3 *dwc = evt->dwc;
	irqreturn_t ret = IRQ_NONE;
	int left;

	left = evt->count;

	if (!(evt->flags & DWC3_EVENT_PENDING))
		return IRQ_NONE;

	while (left > 0) {
		union dwc3_event event;

		event.raw = *(u32 *) (evt->cache + evt->lpos);

		dwc3_process_event_entry(dwc, &event);

		 
		evt->lpos = (evt->lpos + 4) % evt->length;
		left -= 4;
	}

	evt->count = 0;
	ret = IRQ_HANDLED;

	 
	dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(0),
		    DWC3_GEVNTSIZ_SIZE(evt->length));

	if (dwc->imod_interval) {
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(0), DWC3_GEVNTCOUNT_EHB);
		dwc3_writel(dwc->regs, DWC3_DEV_IMOD(0), dwc->imod_interval);
	}

	 
	evt->flags &= ~DWC3_EVENT_PENDING;

	return ret;
}

static irqreturn_t dwc3_thread_interrupt(int irq, void *_evt)
{
	struct dwc3_event_buffer *evt = _evt;
	struct dwc3 *dwc = evt->dwc;
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;

	local_bh_disable();
	spin_lock_irqsave(&dwc->lock, flags);
	ret = dwc3_process_event_buf(evt);
	spin_unlock_irqrestore(&dwc->lock, flags);
	local_bh_enable();

	return ret;
}

static irqreturn_t dwc3_check_event_buf(struct dwc3_event_buffer *evt)
{
	struct dwc3 *dwc = evt->dwc;
	u32 amount;
	u32 count;

	if (pm_runtime_suspended(dwc->dev)) {
		dwc->pending_events = true;
		 
		pm_runtime_get(dwc->dev);
		disable_irq_nosync(dwc->irq_gadget);
		return IRQ_HANDLED;
	}

	 
	if (evt->flags & DWC3_EVENT_PENDING)
		return IRQ_HANDLED;

	count = dwc3_readl(dwc->regs, DWC3_GEVNTCOUNT(0));
	count &= DWC3_GEVNTCOUNT_MASK;
	if (!count)
		return IRQ_NONE;

	evt->count = count;
	evt->flags |= DWC3_EVENT_PENDING;

	 
	dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(0),
		    DWC3_GEVNTSIZ_INTMASK | DWC3_GEVNTSIZ_SIZE(evt->length));

	amount = min(count, evt->length - evt->lpos);
	memcpy(evt->cache + evt->lpos, evt->buf + evt->lpos, amount);

	if (amount < count)
		memcpy(evt->cache, evt->buf, count - amount);

	dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(0), count);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t dwc3_interrupt(int irq, void *_evt)
{
	struct dwc3_event_buffer	*evt = _evt;

	return dwc3_check_event_buf(evt);
}

static int dwc3_gadget_get_irq(struct dwc3 *dwc)
{
	struct platform_device *dwc3_pdev = to_platform_device(dwc->dev);
	int irq;

	irq = platform_get_irq_byname_optional(dwc3_pdev, "peripheral");
	if (irq > 0)
		goto out;

	if (irq == -EPROBE_DEFER)
		goto out;

	irq = platform_get_irq_byname_optional(dwc3_pdev, "dwc_usb3");
	if (irq > 0)
		goto out;

	if (irq == -EPROBE_DEFER)
		goto out;

	irq = platform_get_irq(dwc3_pdev, 0);

out:
	return irq;
}

static void dwc_gadget_release(struct device *dev)
{
	struct usb_gadget *gadget = container_of(dev, struct usb_gadget, dev);

	kfree(gadget);
}

 
int dwc3_gadget_init(struct dwc3 *dwc)
{
	int ret;
	int irq;
	struct device *dev;

	irq = dwc3_gadget_get_irq(dwc);
	if (irq < 0) {
		ret = irq;
		goto err0;
	}

	dwc->irq_gadget = irq;

	dwc->ep0_trb = dma_alloc_coherent(dwc->sysdev,
					  sizeof(*dwc->ep0_trb) * 2,
					  &dwc->ep0_trb_addr, GFP_KERNEL);
	if (!dwc->ep0_trb) {
		dev_err(dwc->dev, "failed to allocate ep0 trb\n");
		ret = -ENOMEM;
		goto err0;
	}

	dwc->setup_buf = kzalloc(DWC3_EP0_SETUP_SIZE, GFP_KERNEL);
	if (!dwc->setup_buf) {
		ret = -ENOMEM;
		goto err1;
	}

	dwc->bounce = dma_alloc_coherent(dwc->sysdev, DWC3_BOUNCE_SIZE,
			&dwc->bounce_addr, GFP_KERNEL);
	if (!dwc->bounce) {
		ret = -ENOMEM;
		goto err2;
	}

	init_completion(&dwc->ep0_in_setup);
	dwc->gadget = kzalloc(sizeof(struct usb_gadget), GFP_KERNEL);
	if (!dwc->gadget) {
		ret = -ENOMEM;
		goto err3;
	}


	usb_initialize_gadget(dwc->dev, dwc->gadget, dwc_gadget_release);
	dev				= &dwc->gadget->dev;
	dev->platform_data		= dwc;
	dwc->gadget->ops		= &dwc3_gadget_ops;
	dwc->gadget->speed		= USB_SPEED_UNKNOWN;
	dwc->gadget->ssp_rate		= USB_SSP_GEN_UNKNOWN;
	dwc->gadget->sg_supported	= true;
	dwc->gadget->name		= "dwc3-gadget";
	dwc->gadget->lpm_capable	= !dwc->usb2_gadget_lpm_disable;
	dwc->gadget->wakeup_capable	= true;

	 
	if (DWC3_VER_IS_PRIOR(DWC3, 220A) &&
	    !dwc->dis_metastability_quirk)
		dev_info(dwc->dev, "changing max_speed on rev %08x\n",
				dwc->revision);

	dwc->gadget->max_speed		= dwc->maximum_speed;
	dwc->gadget->max_ssp_rate	= dwc->max_ssp_rate;

	 

	ret = dwc3_gadget_init_endpoints(dwc, dwc->num_eps);
	if (ret)
		goto err4;

	ret = usb_add_gadget(dwc->gadget);
	if (ret) {
		dev_err(dwc->dev, "failed to add gadget\n");
		goto err5;
	}

	if (DWC3_IP_IS(DWC32) && dwc->maximum_speed == USB_SPEED_SUPER_PLUS)
		dwc3_gadget_set_ssp_rate(dwc->gadget, dwc->max_ssp_rate);
	else
		dwc3_gadget_set_speed(dwc->gadget, dwc->maximum_speed);

	return 0;

err5:
	dwc3_gadget_free_endpoints(dwc);
err4:
	usb_put_gadget(dwc->gadget);
	dwc->gadget = NULL;
err3:
	dma_free_coherent(dwc->sysdev, DWC3_BOUNCE_SIZE, dwc->bounce,
			dwc->bounce_addr);

err2:
	kfree(dwc->setup_buf);

err1:
	dma_free_coherent(dwc->sysdev, sizeof(*dwc->ep0_trb) * 2,
			dwc->ep0_trb, dwc->ep0_trb_addr);

err0:
	return ret;
}

 

void dwc3_gadget_exit(struct dwc3 *dwc)
{
	if (!dwc->gadget)
		return;

	usb_del_gadget(dwc->gadget);
	dwc3_gadget_free_endpoints(dwc);
	usb_put_gadget(dwc->gadget);
	dma_free_coherent(dwc->sysdev, DWC3_BOUNCE_SIZE, dwc->bounce,
			  dwc->bounce_addr);
	kfree(dwc->setup_buf);
	dma_free_coherent(dwc->sysdev, sizeof(*dwc->ep0_trb) * 2,
			  dwc->ep0_trb, dwc->ep0_trb_addr);
}

int dwc3_gadget_suspend(struct dwc3 *dwc)
{
	unsigned long flags;
	int ret;

	if (!dwc->gadget_driver)
		return 0;

	ret = dwc3_gadget_soft_disconnect(dwc);
	if (ret)
		goto err;

	spin_lock_irqsave(&dwc->lock, flags);
	dwc3_disconnect_gadget(dwc);
	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;

err:
	 
	if (dwc->softconnect)
		dwc3_gadget_soft_connect(dwc);

	return ret;
}

int dwc3_gadget_resume(struct dwc3 *dwc)
{
	if (!dwc->gadget_driver || !dwc->softconnect)
		return 0;

	return dwc3_gadget_soft_connect(dwc);
}

void dwc3_gadget_process_pending_events(struct dwc3 *dwc)
{
	if (dwc->pending_events) {
		dwc3_interrupt(dwc->irq_gadget, dwc->ev_buf);
		dwc3_thread_interrupt(dwc->irq_gadget, dwc->ev_buf);
		pm_runtime_put(dwc->dev);
		dwc->pending_events = false;
		enable_irq(dwc->irq_gadget);
	}
}
