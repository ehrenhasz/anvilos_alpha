
 


#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/bitfield.h>

#include "xhci.h"
#include "xhci-trace.h"

#define	PORT_WAKE_BITS	(PORT_WKOC_E | PORT_WKDISC_E | PORT_WKCONN_E)
#define	PORT_RWC_BITS	(PORT_CSC | PORT_PEC | PORT_WRC | PORT_OCC | \
			 PORT_RC | PORT_PLC | PORT_PE)

 
static u32 ssp_cap_default_ssa[] = {
	0x00050034,  
	0x000500b4,  
	0x000a4035,  
	0x000a40b5,  
	0x00054036,  
	0x000540b6,  
	0x000a4037,  
	0x000a40b7,  
};

static int xhci_create_usb3x_bos_desc(struct xhci_hcd *xhci, char *buf,
				      u16 wLength)
{
	struct usb_bos_descriptor	*bos;
	struct usb_ss_cap_descriptor	*ss_cap;
	struct usb_ssp_cap_descriptor	*ssp_cap;
	struct xhci_port_cap		*port_cap = NULL;
	u16				bcdUSB;
	u32				reg;
	u32				min_rate = 0;
	u8				min_ssid;
	u8				ssac;
	u8				ssic;
	int				offset;
	int				i;

	 
	bos = (struct usb_bos_descriptor *)buf;
	bos->bLength = USB_DT_BOS_SIZE;
	bos->bDescriptorType = USB_DT_BOS;
	bos->wTotalLength = cpu_to_le16(USB_DT_BOS_SIZE +
					USB_DT_USB_SS_CAP_SIZE);
	bos->bNumDeviceCaps = 1;

	 
	for (i = 0; i < xhci->num_port_caps; i++) {
		u8 major = xhci->port_caps[i].maj_rev;
		u8 minor = xhci->port_caps[i].min_rev;
		u16 rev = (major << 8) | minor;

		if (i == 0 || bcdUSB < rev) {
			bcdUSB = rev;
			port_cap = &xhci->port_caps[i];
		}
	}

	if (bcdUSB >= 0x0310) {
		if (port_cap->psi_count) {
			u8 num_sym_ssa = 0;

			for (i = 0; i < port_cap->psi_count; i++) {
				if ((port_cap->psi[i] & PLT_MASK) == PLT_SYM)
					num_sym_ssa++;
			}

			ssac = port_cap->psi_count + num_sym_ssa - 1;
			ssic = port_cap->psi_uid_count - 1;
		} else {
			if (bcdUSB >= 0x0320)
				ssac = 7;
			else
				ssac = 3;

			ssic = (ssac + 1) / 2 - 1;
		}

		bos->bNumDeviceCaps++;
		bos->wTotalLength = cpu_to_le16(USB_DT_BOS_SIZE +
						USB_DT_USB_SS_CAP_SIZE +
						USB_DT_USB_SSP_CAP_SIZE(ssac));
	}

	if (wLength < USB_DT_BOS_SIZE + USB_DT_USB_SS_CAP_SIZE)
		return wLength;

	 
	ss_cap = (struct usb_ss_cap_descriptor *)&buf[USB_DT_BOS_SIZE];
	ss_cap->bLength = USB_DT_USB_SS_CAP_SIZE;
	ss_cap->bDescriptorType = USB_DT_DEVICE_CAPABILITY;
	ss_cap->bDevCapabilityType = USB_SS_CAP_TYPE;
	ss_cap->bmAttributes = 0;  
	ss_cap->wSpeedSupported = cpu_to_le16(USB_5GBPS_OPERATION);
	ss_cap->bFunctionalitySupport = USB_LOW_SPEED_OPERATION;
	ss_cap->bU1devExitLat = 0;  
	ss_cap->bU2DevExitLat = 0;  

	reg = readl(&xhci->cap_regs->hcc_params);
	if (HCC_LTC(reg))
		ss_cap->bmAttributes |= USB_LTM_SUPPORT;

	if ((xhci->quirks & XHCI_LPM_SUPPORT)) {
		reg = readl(&xhci->cap_regs->hcs_params3);
		ss_cap->bU1devExitLat = HCS_U1_LATENCY(reg);
		ss_cap->bU2DevExitLat = cpu_to_le16(HCS_U2_LATENCY(reg));
	}

	if (wLength < le16_to_cpu(bos->wTotalLength))
		return wLength;

	if (bcdUSB < 0x0310)
		return le16_to_cpu(bos->wTotalLength);

	ssp_cap = (struct usb_ssp_cap_descriptor *)&buf[USB_DT_BOS_SIZE +
		USB_DT_USB_SS_CAP_SIZE];
	ssp_cap->bLength = USB_DT_USB_SSP_CAP_SIZE(ssac);
	ssp_cap->bDescriptorType = USB_DT_DEVICE_CAPABILITY;
	ssp_cap->bDevCapabilityType = USB_SSP_CAP_TYPE;
	ssp_cap->bReserved = 0;
	ssp_cap->wReserved = 0;
	ssp_cap->bmAttributes =
		cpu_to_le32(FIELD_PREP(USB_SSP_SUBLINK_SPEED_ATTRIBS, ssac) |
			    FIELD_PREP(USB_SSP_SUBLINK_SPEED_IDS, ssic));

	if (!port_cap->psi_count) {
		for (i = 0; i < ssac + 1; i++)
			ssp_cap->bmSublinkSpeedAttr[i] =
				cpu_to_le32(ssp_cap_default_ssa[i]);

		min_ssid = 4;
		goto out;
	}

	offset = 0;
	for (i = 0; i < port_cap->psi_count; i++) {
		u32 psi;
		u32 attr;
		u8 ssid;
		u8 lp;
		u8 lse;
		u8 psie;
		u16 lane_mantissa;
		u16 psim;
		u16 plt;

		psi = port_cap->psi[i];
		ssid = XHCI_EXT_PORT_PSIV(psi);
		lp = XHCI_EXT_PORT_LP(psi);
		psie = XHCI_EXT_PORT_PSIE(psi);
		psim = XHCI_EXT_PORT_PSIM(psi);
		plt = psi & PLT_MASK;

		lse = psie;
		lane_mantissa = psim;

		 
		for (; psie < USB_SSP_SUBLINK_SPEED_LSE_GBPS; psie++)
			psim /= 1000;

		if (!min_rate || psim < min_rate) {
			min_ssid = ssid;
			min_rate = psim;
		}

		 
		if (psim >= 10)
			lp = USB_SSP_SUBLINK_SPEED_LP_SSP;

		 
		if (bcdUSB == 0x0320 && plt == PLT_SYM) {
			 
			if (ssid == 6 && psie == 3 && psim == 10 && i) {
				u32 prev = port_cap->psi[i - 1];

				if ((prev & PLT_MASK) == PLT_SYM &&
				    XHCI_EXT_PORT_PSIV(prev) == 5 &&
				    XHCI_EXT_PORT_PSIE(prev) == 3 &&
				    XHCI_EXT_PORT_PSIM(prev) == 10) {
					lse = USB_SSP_SUBLINK_SPEED_LSE_GBPS;
					lane_mantissa = 5;
				}
			}

			if (psie == 3 && psim > 10) {
				lse = USB_SSP_SUBLINK_SPEED_LSE_GBPS;
				lane_mantissa = 10;
			}
		}

		attr = (FIELD_PREP(USB_SSP_SUBLINK_SPEED_SSID, ssid) |
			FIELD_PREP(USB_SSP_SUBLINK_SPEED_LP, lp) |
			FIELD_PREP(USB_SSP_SUBLINK_SPEED_LSE, lse) |
			FIELD_PREP(USB_SSP_SUBLINK_SPEED_LSM, lane_mantissa));

		switch (plt) {
		case PLT_SYM:
			attr |= FIELD_PREP(USB_SSP_SUBLINK_SPEED_ST,
					   USB_SSP_SUBLINK_SPEED_ST_SYM_RX);
			ssp_cap->bmSublinkSpeedAttr[offset++] = cpu_to_le32(attr);

			attr &= ~USB_SSP_SUBLINK_SPEED_ST;
			attr |= FIELD_PREP(USB_SSP_SUBLINK_SPEED_ST,
					   USB_SSP_SUBLINK_SPEED_ST_SYM_TX);
			ssp_cap->bmSublinkSpeedAttr[offset++] = cpu_to_le32(attr);
			break;
		case PLT_ASYM_RX:
			attr |= FIELD_PREP(USB_SSP_SUBLINK_SPEED_ST,
					   USB_SSP_SUBLINK_SPEED_ST_ASYM_RX);
			ssp_cap->bmSublinkSpeedAttr[offset++] = cpu_to_le32(attr);
			break;
		case PLT_ASYM_TX:
			attr |= FIELD_PREP(USB_SSP_SUBLINK_SPEED_ST,
					   USB_SSP_SUBLINK_SPEED_ST_ASYM_TX);
			ssp_cap->bmSublinkSpeedAttr[offset++] = cpu_to_le32(attr);
			break;
		}
	}
out:
	ssp_cap->wFunctionalitySupport =
		cpu_to_le16(FIELD_PREP(USB_SSP_MIN_SUBLINK_SPEED_ATTRIBUTE_ID,
				       min_ssid) |
			    FIELD_PREP(USB_SSP_MIN_RX_LANE_COUNT, 1) |
			    FIELD_PREP(USB_SSP_MIN_TX_LANE_COUNT, 1));

	return le16_to_cpu(bos->wTotalLength);
}

static void xhci_common_hub_descriptor(struct xhci_hcd *xhci,
		struct usb_hub_descriptor *desc, int ports)
{
	u16 temp;

	desc->bHubContrCurrent = 0;

	desc->bNbrPorts = ports;
	temp = 0;
	 
	if (HCC_PPC(xhci->hcc_params))
		temp |= HUB_CHAR_INDV_PORT_LPSM;
	else
		temp |= HUB_CHAR_NO_LPSM;
	 
	 
	temp |= HUB_CHAR_INDV_PORT_OCPM;
	 
	 
	desc->wHubCharacteristics = cpu_to_le16(temp);
}

 
static void xhci_usb2_hub_descriptor(struct usb_hcd *hcd, struct xhci_hcd *xhci,
		struct usb_hub_descriptor *desc)
{
	int ports;
	u16 temp;
	__u8 port_removable[(USB_MAXCHILDREN + 1 + 7) / 8];
	u32 portsc;
	unsigned int i;
	struct xhci_hub *rhub;

	rhub = &xhci->usb2_rhub;
	ports = rhub->num_ports;
	xhci_common_hub_descriptor(xhci, desc, ports);
	desc->bDescriptorType = USB_DT_HUB;
	temp = 1 + (ports / 8);
	desc->bDescLength = USB_DT_HUB_NONVAR_SIZE + 2 * temp;
	desc->bPwrOn2PwrGood = 10;	 

	 
	memset(port_removable, 0, sizeof(port_removable));
	for (i = 0; i < ports; i++) {
		portsc = readl(rhub->ports[i]->addr);
		 
		if (portsc & PORT_DEV_REMOVE)
			 
			port_removable[(i + 1) / 8] |= 1 << ((i + 1) % 8);
	}

	 
	memset(desc->u.hs.DeviceRemovable, 0xff,
			sizeof(desc->u.hs.DeviceRemovable));
	memset(desc->u.hs.PortPwrCtrlMask, 0xff,
			sizeof(desc->u.hs.PortPwrCtrlMask));

	for (i = 0; i < (ports + 1 + 7) / 8; i++)
		memset(&desc->u.hs.DeviceRemovable[i], port_removable[i],
				sizeof(__u8));
}

 
static void xhci_usb3_hub_descriptor(struct usb_hcd *hcd, struct xhci_hcd *xhci,
		struct usb_hub_descriptor *desc)
{
	int ports;
	u16 port_removable;
	u32 portsc;
	unsigned int i;
	struct xhci_hub *rhub;

	rhub = &xhci->usb3_rhub;
	ports = rhub->num_ports;
	xhci_common_hub_descriptor(xhci, desc, ports);
	desc->bDescriptorType = USB_DT_SS_HUB;
	desc->bDescLength = USB_DT_SS_HUB_SIZE;
	desc->bPwrOn2PwrGood = 50;	 

	 
	desc->u.ss.bHubHdrDecLat = 0;
	desc->u.ss.wHubDelay = 0;

	port_removable = 0;
	 
	for (i = 0; i < ports; i++) {
		portsc = readl(rhub->ports[i]->addr);
		if (portsc & PORT_DEV_REMOVE)
			port_removable |= 1 << (i + 1);
	}

	desc->u.ss.DeviceRemovable = cpu_to_le16(port_removable);
}

static void xhci_hub_descriptor(struct usb_hcd *hcd, struct xhci_hcd *xhci,
		struct usb_hub_descriptor *desc)
{

	if (hcd->speed >= HCD_USB3)
		xhci_usb3_hub_descriptor(hcd, xhci, desc);
	else
		xhci_usb2_hub_descriptor(hcd, xhci, desc);

}

static unsigned int xhci_port_speed(unsigned int port_status)
{
	if (DEV_LOWSPEED(port_status))
		return USB_PORT_STAT_LOW_SPEED;
	if (DEV_HIGHSPEED(port_status))
		return USB_PORT_STAT_HIGH_SPEED;
	 
	return 0;
}

 
#define	XHCI_PORT_RO	((1<<0) | (1<<3) | (0xf<<10) | (1<<30))
 
#define XHCI_PORT_RWS	((0xf<<5) | (1<<9) | (0x3<<14) | (0x7<<25))
 
#define	XHCI_PORT_RW1S	((1<<4))
 
#define XHCI_PORT_RW1CS	((1<<1) | (0x7f<<17))
 
#define	XHCI_PORT_RW	((1<<16))
 
#define	XHCI_PORT_RZ	((1<<2) | (1<<24) | (0xf<<28))

 

u32 xhci_port_state_to_neutral(u32 state)
{
	 
	return (state & XHCI_PORT_RO) | (state & XHCI_PORT_RWS);
}
EXPORT_SYMBOL_GPL(xhci_port_state_to_neutral);

 

int xhci_find_slot_id_by_port(struct usb_hcd *hcd, struct xhci_hcd *xhci,
		u16 port)
{
	int slot_id;
	int i;
	enum usb_device_speed speed;

	slot_id = 0;
	for (i = 0; i < MAX_HC_SLOTS; i++) {
		if (!xhci->devs[i] || !xhci->devs[i]->udev)
			continue;
		speed = xhci->devs[i]->udev->speed;
		if (((speed >= USB_SPEED_SUPER) == (hcd->speed >= HCD_USB3))
				&& xhci->devs[i]->fake_port == port) {
			slot_id = i;
			break;
		}
	}

	return slot_id;
}
EXPORT_SYMBOL_GPL(xhci_find_slot_id_by_port);

 
static int xhci_stop_device(struct xhci_hcd *xhci, int slot_id, int suspend)
{
	struct xhci_virt_device *virt_dev;
	struct xhci_command *cmd;
	unsigned long flags;
	int ret;
	int i;

	ret = 0;
	virt_dev = xhci->devs[slot_id];
	if (!virt_dev)
		return -ENODEV;

	trace_xhci_stop_device(virt_dev);

	cmd = xhci_alloc_command(xhci, true, GFP_NOIO);
	if (!cmd)
		return -ENOMEM;

	spin_lock_irqsave(&xhci->lock, flags);
	for (i = LAST_EP_INDEX; i > 0; i--) {
		if (virt_dev->eps[i].ring && virt_dev->eps[i].ring->dequeue) {
			struct xhci_ep_ctx *ep_ctx;
			struct xhci_command *command;

			ep_ctx = xhci_get_ep_ctx(xhci, virt_dev->out_ctx, i);

			 
			if (GET_EP_CTX_STATE(ep_ctx) != EP_STATE_RUNNING)
				continue;

			command = xhci_alloc_command(xhci, false, GFP_NOWAIT);
			if (!command) {
				spin_unlock_irqrestore(&xhci->lock, flags);
				ret = -ENOMEM;
				goto cmd_cleanup;
			}

			ret = xhci_queue_stop_endpoint(xhci, command, slot_id,
						       i, suspend);
			if (ret) {
				spin_unlock_irqrestore(&xhci->lock, flags);
				xhci_free_command(xhci, command);
				goto cmd_cleanup;
			}
		}
	}
	ret = xhci_queue_stop_endpoint(xhci, cmd, slot_id, 0, suspend);
	if (ret) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		goto cmd_cleanup;
	}

	xhci_ring_cmd_db(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);

	 
	wait_for_completion(cmd->completion);

	if (cmd->status == COMP_COMMAND_ABORTED ||
	    cmd->status == COMP_COMMAND_RING_STOPPED) {
		xhci_warn(xhci, "Timeout while waiting for stop endpoint command\n");
		ret = -ETIME;
	}

cmd_cleanup:
	xhci_free_command(xhci, cmd);
	return ret;
}

 
void xhci_ring_device(struct xhci_hcd *xhci, int slot_id)
{
	int i, s;
	struct xhci_virt_ep *ep;

	for (i = 0; i < LAST_EP_INDEX + 1; i++) {
		ep = &xhci->devs[slot_id]->eps[i];

		if (ep->ep_state & EP_HAS_STREAMS) {
			for (s = 1; s < ep->stream_info->num_streams; s++)
				xhci_ring_ep_doorbell(xhci, slot_id, i, s);
		} else if (ep->ring && ep->ring->dequeue) {
			xhci_ring_ep_doorbell(xhci, slot_id, i, 0);
		}
	}

	return;
}

static void xhci_disable_port(struct xhci_hcd *xhci, struct xhci_port *port)
{
	struct usb_hcd *hcd;
	u32 portsc;

	hcd = port->rhub->hcd;

	 
	if (hcd->speed >= HCD_USB3) {
		xhci_dbg(xhci, "Ignoring request to disable SuperSpeed port.\n");
		return;
	}

	if (xhci->quirks & XHCI_BROKEN_PORT_PED) {
		xhci_dbg(xhci,
			 "Broken Port Enabled/Disabled, ignoring port disable request.\n");
		return;
	}

	portsc = readl(port->addr);
	portsc = xhci_port_state_to_neutral(portsc);

	 
	writel(portsc | PORT_PE, port->addr);

	portsc = readl(port->addr);
	xhci_dbg(xhci, "disable port %d-%d, portsc: 0x%x\n",
		 hcd->self.busnum, port->hcd_portnum + 1, portsc);
}

static void xhci_clear_port_change_bit(struct xhci_hcd *xhci, u16 wValue,
		u16 wIndex, __le32 __iomem *addr, u32 port_status)
{
	char *port_change_bit;
	u32 status;

	switch (wValue) {
	case USB_PORT_FEAT_C_RESET:
		status = PORT_RC;
		port_change_bit = "reset";
		break;
	case USB_PORT_FEAT_C_BH_PORT_RESET:
		status = PORT_WRC;
		port_change_bit = "warm(BH) reset";
		break;
	case USB_PORT_FEAT_C_CONNECTION:
		status = PORT_CSC;
		port_change_bit = "connect";
		break;
	case USB_PORT_FEAT_C_OVER_CURRENT:
		status = PORT_OCC;
		port_change_bit = "over-current";
		break;
	case USB_PORT_FEAT_C_ENABLE:
		status = PORT_PEC;
		port_change_bit = "enable/disable";
		break;
	case USB_PORT_FEAT_C_SUSPEND:
		status = PORT_PLC;
		port_change_bit = "suspend/resume";
		break;
	case USB_PORT_FEAT_C_PORT_LINK_STATE:
		status = PORT_PLC;
		port_change_bit = "link state";
		break;
	case USB_PORT_FEAT_C_PORT_CONFIG_ERROR:
		status = PORT_CEC;
		port_change_bit = "config error";
		break;
	default:
		 
		return;
	}
	 
	writel(port_status | status, addr);
	port_status = readl(addr);

	xhci_dbg(xhci, "clear port%d %s change, portsc: 0x%x\n",
		 wIndex + 1, port_change_bit, port_status);
}

struct xhci_hub *xhci_get_rhub(struct usb_hcd *hcd)
{
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);

	if (hcd->speed >= HCD_USB3)
		return &xhci->usb3_rhub;
	return &xhci->usb2_rhub;
}

 
static void xhci_set_port_power(struct xhci_hcd *xhci, struct xhci_port *port,
				bool on, unsigned long *flags)
	__must_hold(&xhci->lock)
{
	struct usb_hcd *hcd;
	u32 temp;

	hcd = port->rhub->hcd;
	temp = readl(port->addr);

	xhci_dbg(xhci, "set port power %d-%d %s, portsc: 0x%x\n",
		 hcd->self.busnum, port->hcd_portnum + 1, on ? "ON" : "OFF", temp);

	temp = xhci_port_state_to_neutral(temp);

	if (on) {
		 
		writel(temp | PORT_POWER, port->addr);
		readl(port->addr);
	} else {
		 
		writel(temp & ~PORT_POWER, port->addr);
	}

	spin_unlock_irqrestore(&xhci->lock, *flags);
	temp = usb_acpi_power_manageable(hcd->self.root_hub,
					 port->hcd_portnum);
	if (temp)
		usb_acpi_set_power_state(hcd->self.root_hub,
					 port->hcd_portnum, on);
	spin_lock_irqsave(&xhci->lock, *flags);
}

static void xhci_port_set_test_mode(struct xhci_hcd *xhci,
	u16 test_mode, u16 wIndex)
{
	u32 temp;
	struct xhci_port *port;

	 
	port = xhci->usb2_rhub.ports[wIndex];
	temp = readl(port->addr + PORTPMSC);
	temp |= test_mode << PORT_TEST_MODE_SHIFT;
	writel(temp, port->addr + PORTPMSC);
	xhci->test_mode = test_mode;
	if (test_mode == USB_TEST_FORCE_ENABLE)
		xhci_start(xhci);
}

static int xhci_enter_test_mode(struct xhci_hcd *xhci,
				u16 test_mode, u16 wIndex, unsigned long *flags)
	__must_hold(&xhci->lock)
{
	int i, retval;

	 
	xhci_dbg(xhci, "Disable all slots\n");
	spin_unlock_irqrestore(&xhci->lock, *flags);
	for (i = 1; i <= HCS_MAX_SLOTS(xhci->hcs_params1); i++) {
		if (!xhci->devs[i])
			continue;

		retval = xhci_disable_slot(xhci, i);
		xhci_free_virt_device(xhci, i);
		if (retval)
			xhci_err(xhci, "Failed to disable slot %d, %d. Enter test mode anyway\n",
				 i, retval);
	}
	spin_lock_irqsave(&xhci->lock, *flags);
	 
	xhci_dbg(xhci, "Disable all port (PP = 0)\n");
	 
	for (i = 0; i < xhci->usb3_rhub.num_ports; i++)
		xhci_set_port_power(xhci, xhci->usb3_rhub.ports[i], false, flags);
	 
	for (i = 0; i < xhci->usb2_rhub.num_ports; i++)
		xhci_set_port_power(xhci, xhci->usb2_rhub.ports[i], false, flags);
	 
	xhci_dbg(xhci, "Stop controller\n");
	retval = xhci_halt(xhci);
	if (retval)
		return retval;
	 
	pm_runtime_forbid(xhci_to_hcd(xhci)->self.controller);
	 
	 
	xhci_dbg(xhci, "Enter Test Mode: %d, Port_id=%d\n",
					test_mode, wIndex + 1);
	xhci_port_set_test_mode(xhci, test_mode, wIndex);
	return retval;
}

static int xhci_exit_test_mode(struct xhci_hcd *xhci)
{
	int retval;

	if (!xhci->test_mode) {
		xhci_err(xhci, "Not in test mode, do nothing.\n");
		return 0;
	}
	if (xhci->test_mode == USB_TEST_FORCE_ENABLE &&
		!(xhci->xhc_state & XHCI_STATE_HALTED)) {
		retval = xhci_halt(xhci);
		if (retval)
			return retval;
	}
	pm_runtime_allow(xhci_to_hcd(xhci)->self.controller);
	xhci->test_mode = 0;
	return xhci_reset(xhci, XHCI_RESET_SHORT_USEC);
}

void xhci_set_link_state(struct xhci_hcd *xhci, struct xhci_port *port,
			 u32 link_state)
{
	u32 temp;
	u32 portsc;

	portsc = readl(port->addr);
	temp = xhci_port_state_to_neutral(portsc);
	temp &= ~PORT_PLS_MASK;
	temp |= PORT_LINK_STROBE | link_state;
	writel(temp, port->addr);

	xhci_dbg(xhci, "Set port %d-%d link state, portsc: 0x%x, write 0x%x",
		 port->rhub->hcd->self.busnum, port->hcd_portnum + 1,
		 portsc, temp);
}

static void xhci_set_remote_wake_mask(struct xhci_hcd *xhci,
				      struct xhci_port *port, u16 wake_mask)
{
	u32 temp;

	temp = readl(port->addr);
	temp = xhci_port_state_to_neutral(temp);

	if (wake_mask & USB_PORT_FEAT_REMOTE_WAKE_CONNECT)
		temp |= PORT_WKCONN_E;
	else
		temp &= ~PORT_WKCONN_E;

	if (wake_mask & USB_PORT_FEAT_REMOTE_WAKE_DISCONNECT)
		temp |= PORT_WKDISC_E;
	else
		temp &= ~PORT_WKDISC_E;

	if (wake_mask & USB_PORT_FEAT_REMOTE_WAKE_OVER_CURRENT)
		temp |= PORT_WKOC_E;
	else
		temp &= ~PORT_WKOC_E;

	writel(temp, port->addr);
}

 
void xhci_test_and_clear_bit(struct xhci_hcd *xhci, struct xhci_port *port,
			     u32 port_bit)
{
	u32 temp;

	temp = readl(port->addr);
	if (temp & port_bit) {
		temp = xhci_port_state_to_neutral(temp);
		temp |= port_bit;
		writel(temp, port->addr);
	}
}

 
static void xhci_hub_report_usb3_link_state(struct xhci_hcd *xhci,
		u32 *status, u32 status_reg)
{
	u32 pls = status_reg & PORT_PLS_MASK;

	 
	if (status_reg & PORT_CAS) {
		 
		if (pls != USB_SS_PORT_LS_COMP_MOD &&
		    pls != USB_SS_PORT_LS_SS_INACTIVE) {
			pls = USB_SS_PORT_LS_COMP_MOD;
		}
		 
		pls |= USB_PORT_STAT_CONNECTION;
	} else {
		 
		if (pls == XDEV_RESUME) {
			*status |= USB_SS_PORT_LS_U3;
			return;
		}

		 
		if ((xhci->quirks & XHCI_COMP_MODE_QUIRK) &&
				(pls == USB_SS_PORT_LS_COMP_MOD))
			pls |= USB_PORT_STAT_CONNECTION;
	}

	 
	*status |= pls;
}

 
static void xhci_del_comp_mod_timer(struct xhci_hcd *xhci, u32 status,
				    u16 wIndex)
{
	u32 all_ports_seen_u0 = ((1 << xhci->usb3_rhub.num_ports) - 1);
	bool port_in_u0 = ((status & PORT_PLS_MASK) == XDEV_U0);

	if (!(xhci->quirks & XHCI_COMP_MODE_QUIRK))
		return;

	if ((xhci->port_status_u0 != all_ports_seen_u0) && port_in_u0) {
		xhci->port_status_u0 |= 1 << wIndex;
		if (xhci->port_status_u0 == all_ports_seen_u0) {
			del_timer_sync(&xhci->comp_mode_recovery_timer);
			xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"All USB3 ports have entered U0 already!");
			xhci_dbg_trace(xhci, trace_xhci_dbg_quirks,
				"Compliance Mode Recovery Timer Deleted.");
		}
	}
}

static int xhci_handle_usb2_port_link_resume(struct xhci_port *port,
					     u32 portsc,
					     unsigned long *flags)
{
	struct xhci_bus_state *bus_state;
	struct xhci_hcd	*xhci;
	struct usb_hcd *hcd;
	int slot_id;
	u32 wIndex;

	hcd = port->rhub->hcd;
	bus_state = &port->rhub->bus_state;
	xhci = hcd_to_xhci(hcd);
	wIndex = port->hcd_portnum;

	if ((portsc & PORT_RESET) || !(portsc & PORT_PE)) {
		return -EINVAL;
	}
	 
	if (!port->resume_timestamp) {
		 
		if (test_bit(wIndex, &bus_state->resuming_ports)) {
			 
		} else {
			 
			unsigned long timeout = jiffies +
				msecs_to_jiffies(USB_RESUME_TIMEOUT);

			set_bit(wIndex, &bus_state->resuming_ports);
			port->resume_timestamp = timeout;
			mod_timer(&hcd->rh_timer, timeout);
			usb_hcd_start_port_resume(&hcd->self, wIndex);
		}
	 
	} else if (time_after_eq(jiffies, port->resume_timestamp)) {
		int time_left;

		xhci_dbg(xhci, "resume USB2 port %d-%d\n",
			 hcd->self.busnum, wIndex + 1);

		port->resume_timestamp = 0;
		clear_bit(wIndex, &bus_state->resuming_ports);

		reinit_completion(&port->rexit_done);
		port->rexit_active = true;

		xhci_test_and_clear_bit(xhci, port, PORT_PLC);
		xhci_set_link_state(xhci, port, XDEV_U0);

		spin_unlock_irqrestore(&xhci->lock, *flags);
		time_left = wait_for_completion_timeout(
			&port->rexit_done,
			msecs_to_jiffies(XHCI_MAX_REXIT_TIMEOUT_MS));
		spin_lock_irqsave(&xhci->lock, *flags);

		if (time_left) {
			slot_id = xhci_find_slot_id_by_port(hcd, xhci,
							    wIndex + 1);
			if (!slot_id) {
				xhci_dbg(xhci, "slot_id is zero\n");
				return -ENODEV;
			}
			xhci_ring_device(xhci, slot_id);
		} else {
			int port_status = readl(port->addr);

			xhci_warn(xhci, "Port resume timed out, port %d-%d: 0x%x\n",
				  hcd->self.busnum, wIndex + 1, port_status);
			 
		}

		usb_hcd_end_port_resume(&hcd->self, wIndex);
		bus_state->port_c_suspend |= 1 << wIndex;
		bus_state->suspended_ports &= ~(1 << wIndex);
	}

	return 0;
}

static u32 xhci_get_ext_port_status(u32 raw_port_status, u32 port_li)
{
	u32 ext_stat = 0;
	int speed_id;

	 
	speed_id = DEV_PORT_SPEED(raw_port_status);
	ext_stat |= speed_id;		 
	ext_stat |= speed_id << 4;	 

	ext_stat |= PORT_RX_LANES(port_li) << 8;   
	ext_stat |= PORT_TX_LANES(port_li) << 12;  

	return ext_stat;
}

static void xhci_get_usb3_port_status(struct xhci_port *port, u32 *status,
				      u32 portsc)
{
	struct xhci_bus_state *bus_state;
	struct xhci_hcd	*xhci;
	struct usb_hcd *hcd;
	u32 link_state;
	u32 portnum;

	bus_state = &port->rhub->bus_state;
	xhci = hcd_to_xhci(port->rhub->hcd);
	hcd = port->rhub->hcd;
	link_state = portsc & PORT_PLS_MASK;
	portnum = port->hcd_portnum;

	 

	if (portsc & PORT_PLC && (link_state != XDEV_RESUME))
		*status |= USB_PORT_STAT_C_LINK_STATE << 16;
	if (portsc & PORT_WRC)
		*status |= USB_PORT_STAT_C_BH_RESET << 16;
	if (portsc & PORT_CEC)
		*status |= USB_PORT_STAT_C_CONFIG_ERROR << 16;

	 
	if (portsc & PORT_POWER)
		*status |= USB_SS_PORT_STAT_POWER;

	 
	if (link_state != XDEV_U3 &&
	    link_state != XDEV_RESUME &&
	    link_state != XDEV_RECOVERY) {
		 
		if (bus_state->port_remote_wakeup & (1 << portnum)) {
			bus_state->port_remote_wakeup &= ~(1 << portnum);
			usb_hcd_end_port_resume(&hcd->self, portnum);
		}
		bus_state->suspended_ports &= ~(1 << portnum);
	}

	xhci_hub_report_usb3_link_state(xhci, status, portsc);
	xhci_del_comp_mod_timer(xhci, portsc, portnum);
}

static void xhci_get_usb2_port_status(struct xhci_port *port, u32 *status,
				      u32 portsc, unsigned long *flags)
{
	struct xhci_bus_state *bus_state;
	u32 link_state;
	u32 portnum;
	int err;

	bus_state = &port->rhub->bus_state;
	link_state = portsc & PORT_PLS_MASK;
	portnum = port->hcd_portnum;

	 
	if (portsc & PORT_POWER) {
		*status |= USB_PORT_STAT_POWER;

		 
		if (link_state == XDEV_U3)
			*status |= USB_PORT_STAT_SUSPEND;
		if (link_state == XDEV_U2)
			*status |= USB_PORT_STAT_L1;
		if (link_state == XDEV_U0) {
			if (bus_state->suspended_ports & (1 << portnum)) {
				bus_state->suspended_ports &= ~(1 << portnum);
				bus_state->port_c_suspend |= 1 << portnum;
			}
		}
		if (link_state == XDEV_RESUME) {
			err = xhci_handle_usb2_port_link_resume(port, portsc,
								flags);
			if (err < 0)
				*status = 0xffffffff;
			else if (port->resume_timestamp || port->rexit_active)
				*status |= USB_PORT_STAT_SUSPEND;
		}
	}

	 
	if (link_state != XDEV_U3 && link_state != XDEV_RESUME) {
		if (port->resume_timestamp ||
		    test_bit(portnum, &bus_state->resuming_ports)) {
			port->resume_timestamp = 0;
			clear_bit(portnum, &bus_state->resuming_ports);
			usb_hcd_end_port_resume(&port->rhub->hcd->self, portnum);
		}
		port->rexit_active = 0;
		bus_state->suspended_ports &= ~(1 << portnum);
	}
}

 
static u32 xhci_get_port_status(struct usb_hcd *hcd,
		struct xhci_bus_state *bus_state,
	u16 wIndex, u32 raw_port_status,
		unsigned long *flags)
	__releases(&xhci->lock)
	__acquires(&xhci->lock)
{
	u32 status = 0;
	struct xhci_hub *rhub;
	struct xhci_port *port;

	rhub = xhci_get_rhub(hcd);
	port = rhub->ports[wIndex];

	 
	if (raw_port_status & PORT_CSC)
		status |= USB_PORT_STAT_C_CONNECTION << 16;
	if (raw_port_status & PORT_PEC)
		status |= USB_PORT_STAT_C_ENABLE << 16;
	if ((raw_port_status & PORT_OCC))
		status |= USB_PORT_STAT_C_OVERCURRENT << 16;
	if ((raw_port_status & PORT_RC))
		status |= USB_PORT_STAT_C_RESET << 16;

	 
	if (raw_port_status & PORT_CONNECT) {
		status |= USB_PORT_STAT_CONNECTION;
		status |= xhci_port_speed(raw_port_status);
	}
	if (raw_port_status & PORT_PE)
		status |= USB_PORT_STAT_ENABLE;
	if (raw_port_status & PORT_OC)
		status |= USB_PORT_STAT_OVERCURRENT;
	if (raw_port_status & PORT_RESET)
		status |= USB_PORT_STAT_RESET;

	 
	if (hcd->speed >= HCD_USB3)
		xhci_get_usb3_port_status(port, &status, raw_port_status);
	else
		xhci_get_usb2_port_status(port, &status, raw_port_status,
					  flags);

	if (bus_state->port_c_suspend & (1 << wIndex))
		status |= USB_PORT_STAT_C_SUSPEND << 16;

	return status;
}

int xhci_hub_control(struct usb_hcd *hcd, u16 typeReq, u16 wValue,
		u16 wIndex, char *buf, u16 wLength)
{
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	int max_ports;
	unsigned long flags;
	u32 temp, status;
	int retval = 0;
	int slot_id;
	struct xhci_bus_state *bus_state;
	u16 link_state = 0;
	u16 wake_mask = 0;
	u16 timeout = 0;
	u16 test_mode = 0;
	struct xhci_hub *rhub;
	struct xhci_port **ports;
	struct xhci_port *port;
	int portnum1;

	rhub = xhci_get_rhub(hcd);
	ports = rhub->ports;
	max_ports = rhub->num_ports;
	bus_state = &rhub->bus_state;
	portnum1 = wIndex & 0xff;

	spin_lock_irqsave(&xhci->lock, flags);
	switch (typeReq) {
	case GetHubStatus:
		 
		memset(buf, 0, 4);
		break;
	case GetHubDescriptor:
		 
		if (hcd->speed >= HCD_USB3 &&
				(wLength < USB_DT_SS_HUB_SIZE ||
				 wValue != (USB_DT_SS_HUB << 8))) {
			xhci_dbg(xhci, "Wrong hub descriptor type for "
					"USB 3.0 roothub.\n");
			goto error;
		}
		xhci_hub_descriptor(hcd, xhci,
				(struct usb_hub_descriptor *) buf);
		break;
	case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
		if ((wValue & 0xff00) != (USB_DT_BOS << 8))
			goto error;

		if (hcd->speed < HCD_USB3)
			goto error;

		retval = xhci_create_usb3x_bos_desc(xhci, buf, wLength);
		spin_unlock_irqrestore(&xhci->lock, flags);
		return retval;
	case GetPortStatus:
		if (!portnum1 || portnum1 > max_ports)
			goto error;

		wIndex--;
		port = ports[portnum1 - 1];
		temp = readl(port->addr);
		if (temp == ~(u32)0) {
			xhci_hc_died(xhci);
			retval = -ENODEV;
			break;
		}
		trace_xhci_get_port_status(wIndex, temp);
		status = xhci_get_port_status(hcd, bus_state, wIndex, temp,
					      &flags);
		if (status == 0xffffffff)
			goto error;

		xhci_dbg(xhci, "Get port status %d-%d read: 0x%x, return 0x%x",
			 hcd->self.busnum, portnum1, temp, status);

		put_unaligned(cpu_to_le32(status), (__le32 *) buf);
		 
		if (wValue == 0x02) {
			u32 port_li;

			if (hcd->speed < HCD_USB31 || wLength != 8) {
				xhci_err(xhci, "get ext port status invalid parameter\n");
				retval = -EINVAL;
				break;
			}
			port_li = readl(port->addr + PORTLI);
			status = xhci_get_ext_port_status(temp, port_li);
			put_unaligned_le32(status, &buf[4]);
		}
		break;
	case SetPortFeature:
		if (wValue == USB_PORT_FEAT_LINK_STATE)
			link_state = (wIndex & 0xff00) >> 3;
		if (wValue == USB_PORT_FEAT_REMOTE_WAKE_MASK)
			wake_mask = wIndex & 0xff00;
		if (wValue == USB_PORT_FEAT_TEST)
			test_mode = (wIndex & 0xff00) >> 8;
		 
		timeout = (wIndex & 0xff00) >> 8;

		wIndex &= 0xff;
		if (!portnum1 || portnum1 > max_ports)
			goto error;

		port = ports[portnum1 - 1];
		wIndex--;
		temp = readl(port->addr);
		if (temp == ~(u32)0) {
			xhci_hc_died(xhci);
			retval = -ENODEV;
			break;
		}
		temp = xhci_port_state_to_neutral(temp);
		 
		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			temp = readl(port->addr);
			if ((temp & PORT_PLS_MASK) != XDEV_U0) {
				 
				xhci_set_link_state(xhci, port, XDEV_U0);
				spin_unlock_irqrestore(&xhci->lock, flags);
				msleep(10);
				spin_lock_irqsave(&xhci->lock, flags);
			}
			 
			temp = readl(port->addr);
			if ((temp & PORT_PE) == 0 || (temp & PORT_RESET)
				|| (temp & PORT_PLS_MASK) >= XDEV_U3) {
				xhci_warn(xhci, "USB core suspending port %d-%d not in U0/U1/U2\n",
					  hcd->self.busnum, portnum1);
				goto error;
			}

			slot_id = xhci_find_slot_id_by_port(hcd, xhci,
							    portnum1);
			if (!slot_id) {
				xhci_warn(xhci, "slot_id is zero\n");
				goto error;
			}
			 
			spin_unlock_irqrestore(&xhci->lock, flags);
			xhci_stop_device(xhci, slot_id, 1);
			spin_lock_irqsave(&xhci->lock, flags);

			xhci_set_link_state(xhci, port, XDEV_U3);

			spin_unlock_irqrestore(&xhci->lock, flags);
			msleep(10);  
			spin_lock_irqsave(&xhci->lock, flags);

			temp = readl(port->addr);
			bus_state->suspended_ports |= 1 << wIndex;
			break;
		case USB_PORT_FEAT_LINK_STATE:
			temp = readl(port->addr);
			 
			if (link_state == USB_SS_PORT_LS_SS_DISABLED) {
				xhci_dbg(xhci, "Disable port %d-%d\n",
					 hcd->self.busnum, portnum1);
				temp = xhci_port_state_to_neutral(temp);
				 
				temp |= PORT_CSC | PORT_PEC | PORT_WRC |
					PORT_OCC | PORT_RC | PORT_PLC |
					PORT_CEC;
				writel(temp | PORT_PE, port->addr);
				temp = readl(port->addr);
				break;
			}

			 
			if (link_state == USB_SS_PORT_LS_RX_DETECT) {
				xhci_dbg(xhci, "Enable port %d-%d\n",
					 hcd->self.busnum, portnum1);
				xhci_set_link_state(xhci, port,	link_state);
				temp = readl(port->addr);
				break;
			}

			 
			if (link_state == USB_SS_PORT_LS_COMP_MOD) {
				if (!HCC2_CTC(xhci->hcc_params2)) {
					xhci_dbg(xhci, "CTC flag is 0, port already supports entering compliance mode\n");
					break;
				}

				if ((temp & PORT_CONNECT)) {
					xhci_warn(xhci, "Can't set compliance mode when port is connected\n");
					goto error;
				}

				xhci_dbg(xhci, "Enable compliance mode transition for port %d-%d\n",
					 hcd->self.busnum, portnum1);
				xhci_set_link_state(xhci, port, link_state);

				temp = readl(port->addr);
				break;
			}
			 
			if (!(temp & PORT_PE)) {
				retval = -ENODEV;
				break;
			}
			 
			if (link_state > USB_SS_PORT_LS_U3) {
				xhci_warn(xhci, "Cannot set port %d-%d link state %d\n",
					  hcd->self.busnum, portnum1, link_state);
				goto error;
			}

			 
			if (link_state == USB_SS_PORT_LS_U0) {
				u32 pls = temp & PORT_PLS_MASK;
				bool wait_u0 = false;

				 
				if (pls == XDEV_U0)
					break;
				if (pls == XDEV_U3 ||
				    pls == XDEV_RESUME ||
				    pls == XDEV_RECOVERY) {
					wait_u0 = true;
					reinit_completion(&port->u3exit_done);
				}
				if (pls <= XDEV_U3)  
					xhci_set_link_state(xhci, port, USB_SS_PORT_LS_U0);
				if (!wait_u0) {
					if (pls > XDEV_U3)
						goto error;
					break;
				}
				spin_unlock_irqrestore(&xhci->lock, flags);
				if (!wait_for_completion_timeout(&port->u3exit_done,
								 msecs_to_jiffies(500)))
					xhci_dbg(xhci, "missing U0 port change event for port %d-%d\n",
						 hcd->self.busnum, portnum1);
				spin_lock_irqsave(&xhci->lock, flags);
				temp = readl(port->addr);
				break;
			}

			if (link_state == USB_SS_PORT_LS_U3) {
				int retries = 16;
				slot_id = xhci_find_slot_id_by_port(hcd, xhci,
								    portnum1);
				if (slot_id) {
					 
					spin_unlock_irqrestore(&xhci->lock,
								flags);
					xhci_stop_device(xhci, slot_id, 1);
					spin_lock_irqsave(&xhci->lock, flags);
				}
				xhci_set_link_state(xhci, port, USB_SS_PORT_LS_U3);
				spin_unlock_irqrestore(&xhci->lock, flags);
				while (retries--) {
					usleep_range(4000, 8000);
					temp = readl(port->addr);
					if ((temp & PORT_PLS_MASK) == XDEV_U3)
						break;
				}
				spin_lock_irqsave(&xhci->lock, flags);
				temp = readl(port->addr);
				bus_state->suspended_ports |= 1 << wIndex;
			}
			break;
		case USB_PORT_FEAT_POWER:
			 
			xhci_set_port_power(xhci, port, true, &flags);
			break;
		case USB_PORT_FEAT_RESET:
			temp = (temp | PORT_RESET);
			writel(temp, port->addr);

			temp = readl(port->addr);
			xhci_dbg(xhci, "set port reset, actual port %d-%d status  = 0x%x\n",
				 hcd->self.busnum, portnum1, temp);
			break;
		case USB_PORT_FEAT_REMOTE_WAKE_MASK:
			xhci_set_remote_wake_mask(xhci, port, wake_mask);
			temp = readl(port->addr);
			xhci_dbg(xhci, "set port remote wake mask, actual port %d-%d status  = 0x%x\n",
				 hcd->self.busnum, portnum1, temp);
			break;
		case USB_PORT_FEAT_BH_PORT_RESET:
			temp |= PORT_WR;
			writel(temp, port->addr);
			temp = readl(port->addr);
			break;
		case USB_PORT_FEAT_U1_TIMEOUT:
			if (hcd->speed < HCD_USB3)
				goto error;
			temp = readl(port->addr + PORTPMSC);
			temp &= ~PORT_U1_TIMEOUT_MASK;
			temp |= PORT_U1_TIMEOUT(timeout);
			writel(temp, port->addr + PORTPMSC);
			break;
		case USB_PORT_FEAT_U2_TIMEOUT:
			if (hcd->speed < HCD_USB3)
				goto error;
			temp = readl(port->addr + PORTPMSC);
			temp &= ~PORT_U2_TIMEOUT_MASK;
			temp |= PORT_U2_TIMEOUT(timeout);
			writel(temp, port->addr + PORTPMSC);
			break;
		case USB_PORT_FEAT_TEST:
			 
			if (hcd->speed != HCD_USB2)
				goto error;
			if (test_mode > USB_TEST_FORCE_ENABLE ||
			    test_mode < USB_TEST_J)
				goto error;
			retval = xhci_enter_test_mode(xhci, test_mode, wIndex,
						      &flags);
			break;
		default:
			goto error;
		}
		 
		temp = readl(port->addr);
		break;
	case ClearPortFeature:
		if (!portnum1 || portnum1 > max_ports)
			goto error;

		port = ports[portnum1 - 1];

		wIndex--;
		temp = readl(port->addr);
		if (temp == ~(u32)0) {
			xhci_hc_died(xhci);
			retval = -ENODEV;
			break;
		}
		 
		temp = xhci_port_state_to_neutral(temp);
		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			temp = readl(port->addr);
			xhci_dbg(xhci, "clear USB_PORT_FEAT_SUSPEND\n");
			xhci_dbg(xhci, "PORTSC %04x\n", temp);
			if (temp & PORT_RESET)
				goto error;
			if ((temp & PORT_PLS_MASK) == XDEV_U3) {
				if ((temp & PORT_PE) == 0)
					goto error;

				set_bit(wIndex, &bus_state->resuming_ports);
				usb_hcd_start_port_resume(&hcd->self, wIndex);
				xhci_set_link_state(xhci, port, XDEV_RESUME);
				spin_unlock_irqrestore(&xhci->lock, flags);
				msleep(USB_RESUME_TIMEOUT);
				spin_lock_irqsave(&xhci->lock, flags);
				xhci_set_link_state(xhci, port, XDEV_U0);
				clear_bit(wIndex, &bus_state->resuming_ports);
				usb_hcd_end_port_resume(&hcd->self, wIndex);
			}
			bus_state->port_c_suspend |= 1 << wIndex;

			slot_id = xhci_find_slot_id_by_port(hcd, xhci,
					portnum1);
			if (!slot_id) {
				xhci_dbg(xhci, "slot_id is zero\n");
				goto error;
			}
			xhci_ring_device(xhci, slot_id);
			break;
		case USB_PORT_FEAT_C_SUSPEND:
			bus_state->port_c_suspend &= ~(1 << wIndex);
			fallthrough;
		case USB_PORT_FEAT_C_RESET:
		case USB_PORT_FEAT_C_BH_PORT_RESET:
		case USB_PORT_FEAT_C_CONNECTION:
		case USB_PORT_FEAT_C_OVER_CURRENT:
		case USB_PORT_FEAT_C_ENABLE:
		case USB_PORT_FEAT_C_PORT_LINK_STATE:
		case USB_PORT_FEAT_C_PORT_CONFIG_ERROR:
			xhci_clear_port_change_bit(xhci, wValue, wIndex,
					port->addr, temp);
			break;
		case USB_PORT_FEAT_ENABLE:
			xhci_disable_port(xhci, port);
			break;
		case USB_PORT_FEAT_POWER:
			xhci_set_port_power(xhci, port, false, &flags);
			break;
		case USB_PORT_FEAT_TEST:
			retval = xhci_exit_test_mode(xhci);
			break;
		default:
			goto error;
		}
		break;
	default:
error:
		 
		retval = -EPIPE;
	}
	spin_unlock_irqrestore(&xhci->lock, flags);
	return retval;
}
EXPORT_SYMBOL_GPL(xhci_hub_control);

 
int xhci_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	unsigned long flags;
	u32 temp, status;
	u32 mask;
	int i, retval;
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	int max_ports;
	struct xhci_bus_state *bus_state;
	bool reset_change = false;
	struct xhci_hub *rhub;
	struct xhci_port **ports;

	rhub = xhci_get_rhub(hcd);
	ports = rhub->ports;
	max_ports = rhub->num_ports;
	bus_state = &rhub->bus_state;

	 
	retval = (max_ports + 8) / 8;
	memset(buf, 0, retval);

	 
	spin_lock_irqsave(&xhci->lock, flags);

	status = bus_state->resuming_ports;

	 
	if (xhci->run_graceperiod) {
		if (time_before(jiffies, xhci->run_graceperiod))
			status = 1;
		else
			xhci->run_graceperiod = 0;
	}

	mask = PORT_CSC | PORT_PEC | PORT_OCC | PORT_PLC | PORT_WRC | PORT_CEC;

	 
	for (i = 0; i < max_ports; i++) {
		temp = readl(ports[i]->addr);
		if (temp == ~(u32)0) {
			xhci_hc_died(xhci);
			retval = -ENODEV;
			break;
		}
		trace_xhci_hub_status_data(i, temp);

		if ((temp & mask) != 0 ||
			(bus_state->port_c_suspend & 1 << i) ||
			(ports[i]->resume_timestamp && time_after_eq(
			    jiffies, ports[i]->resume_timestamp))) {
			buf[(i + 1) / 8] |= 1 << (i + 1) % 8;
			status = 1;
		}
		if ((temp & PORT_RC))
			reset_change = true;
		if (temp & PORT_OC)
			status = 1;
	}
	if (!status && !reset_change) {
		xhci_dbg(xhci, "%s: stopping usb%d port polling\n",
			 __func__, hcd->self.busnum);
		clear_bit(HCD_FLAG_POLL_RH, &hcd->flags);
	}
	spin_unlock_irqrestore(&xhci->lock, flags);
	return status ? retval : 0;
}

#ifdef CONFIG_PM

int xhci_bus_suspend(struct usb_hcd *hcd)
{
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	int max_ports, port_index;
	struct xhci_bus_state *bus_state;
	unsigned long flags;
	struct xhci_hub *rhub;
	struct xhci_port **ports;
	u32 portsc_buf[USB_MAXCHILDREN];
	bool wake_enabled;

	rhub = xhci_get_rhub(hcd);
	ports = rhub->ports;
	max_ports = rhub->num_ports;
	bus_state = &rhub->bus_state;
	wake_enabled = hcd->self.root_hub->do_remote_wakeup;

	spin_lock_irqsave(&xhci->lock, flags);

	if (wake_enabled) {
		if (bus_state->resuming_ports ||	 
		    bus_state->port_remote_wakeup) {	 
			spin_unlock_irqrestore(&xhci->lock, flags);
			xhci_dbg(xhci, "usb%d bus suspend to fail because a port is resuming\n",
				 hcd->self.busnum);
			return -EBUSY;
		}
	}
	 
	bus_state->bus_suspended = 0;
	port_index = max_ports;
	while (port_index--) {
		u32 t1, t2;
		int retries = 10;
retry:
		t1 = readl(ports[port_index]->addr);
		t2 = xhci_port_state_to_neutral(t1);
		portsc_buf[port_index] = 0;

		 
		if ((hcd->speed >= HCD_USB3) && retries-- &&
		    (t1 & PORT_PLS_MASK) == XDEV_POLLING) {
			spin_unlock_irqrestore(&xhci->lock, flags);
			msleep(XHCI_PORT_POLLING_LFPS_TIME);
			spin_lock_irqsave(&xhci->lock, flags);
			xhci_dbg(xhci, "port %d-%d polling in bus suspend, waiting\n",
				 hcd->self.busnum, port_index + 1);
			goto retry;
		}
		 
		if (t1 & PORT_OC) {
			bus_state->bus_suspended = 0;
			spin_unlock_irqrestore(&xhci->lock, flags);
			xhci_dbg(xhci, "Bus suspend bailout, port over-current detected\n");
			return -EBUSY;
		}
		 
		if ((t1 & PORT_PE) && (t1 & PORT_PLS_MASK) == XDEV_U0) {
			if ((t1 & PORT_CSC) && wake_enabled) {
				bus_state->bus_suspended = 0;
				spin_unlock_irqrestore(&xhci->lock, flags);
				xhci_dbg(xhci, "Bus suspend bailout, port connect change\n");
				return -EBUSY;
			}
			xhci_dbg(xhci, "port %d-%d not suspended\n",
				 hcd->self.busnum, port_index + 1);
			t2 &= ~PORT_PLS_MASK;
			t2 |= PORT_LINK_STROBE | XDEV_U3;
			set_bit(port_index, &bus_state->bus_suspended);
		}
		 
		if (wake_enabled) {
			if (t1 & PORT_CONNECT) {
				t2 |= PORT_WKOC_E | PORT_WKDISC_E;
				t2 &= ~PORT_WKCONN_E;
			} else {
				t2 |= PORT_WKOC_E | PORT_WKCONN_E;
				t2 &= ~PORT_WKDISC_E;
			}

			if ((xhci->quirks & XHCI_U2_DISABLE_WAKE) &&
			    (hcd->speed < HCD_USB3)) {
				if (usb_amd_pt_check_port(hcd->self.controller,
							  port_index))
					t2 &= ~PORT_WAKE_BITS;
			}
		} else
			t2 &= ~PORT_WAKE_BITS;

		t1 = xhci_port_state_to_neutral(t1);
		if (t1 != t2)
			portsc_buf[port_index] = t2;
	}

	 
	port_index = max_ports;
	while (port_index--) {
		if (!portsc_buf[port_index])
			continue;
		if (test_bit(port_index, &bus_state->bus_suspended)) {
			int slot_id;

			slot_id = xhci_find_slot_id_by_port(hcd, xhci,
							    port_index + 1);
			if (slot_id) {
				spin_unlock_irqrestore(&xhci->lock, flags);
				xhci_stop_device(xhci, slot_id, 1);
				spin_lock_irqsave(&xhci->lock, flags);
			}
		}
		writel(portsc_buf[port_index], ports[port_index]->addr);
	}
	hcd->state = HC_STATE_SUSPENDED;
	bus_state->next_statechange = jiffies + msecs_to_jiffies(10);
	spin_unlock_irqrestore(&xhci->lock, flags);

	if (bus_state->bus_suspended)
		usleep_range(5000, 10000);

	return 0;
}

 
static bool xhci_port_missing_cas_quirk(struct xhci_port *port)
{
	u32 portsc;

	portsc = readl(port->addr);

	 
	if (portsc & (PORT_CONNECT | PORT_CAS))
		return false;

	if (((portsc & PORT_PLS_MASK) != XDEV_POLLING) &&
	    ((portsc & PORT_PLS_MASK) != XDEV_COMP_MODE))
		return false;

	 
	portsc &= ~(PORT_RWC_BITS | PORT_CEC | PORT_WAKE_BITS);
	portsc |= PORT_WR;
	writel(portsc, port->addr);
	 
	readl(port->addr);
	return true;
}

int xhci_bus_resume(struct usb_hcd *hcd)
{
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct xhci_bus_state *bus_state;
	unsigned long flags;
	int max_ports, port_index;
	int slot_id;
	int sret;
	u32 next_state;
	u32 temp, portsc;
	struct xhci_hub *rhub;
	struct xhci_port **ports;

	rhub = xhci_get_rhub(hcd);
	ports = rhub->ports;
	max_ports = rhub->num_ports;
	bus_state = &rhub->bus_state;

	if (time_before(jiffies, bus_state->next_statechange))
		msleep(5);

	spin_lock_irqsave(&xhci->lock, flags);
	if (!HCD_HW_ACCESSIBLE(hcd)) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		return -ESHUTDOWN;
	}

	 
	temp = readl(&xhci->op_regs->command);
	temp &= ~CMD_EIE;
	writel(temp, &xhci->op_regs->command);

	 
	if (hcd->speed >= HCD_USB3)
		next_state = XDEV_U0;
	else
		next_state = XDEV_RESUME;

	port_index = max_ports;
	while (port_index--) {
		portsc = readl(ports[port_index]->addr);

		 
		if ((xhci->quirks & XHCI_MISSING_CAS) &&
		    (hcd->speed >= HCD_USB3) &&
		    xhci_port_missing_cas_quirk(ports[port_index])) {
			xhci_dbg(xhci, "reset stuck port %d-%d\n",
				 hcd->self.busnum, port_index + 1);
			clear_bit(port_index, &bus_state->bus_suspended);
			continue;
		}
		 
		if (test_bit(port_index, &bus_state->bus_suspended))
			switch (portsc & PORT_PLS_MASK) {
			case XDEV_U3:
				portsc = xhci_port_state_to_neutral(portsc);
				portsc &= ~PORT_PLS_MASK;
				portsc |= PORT_LINK_STROBE | next_state;
				break;
			case XDEV_RESUME:
				 
				break;
			default:
				 
				clear_bit(port_index,
					  &bus_state->bus_suspended);
				break;
			}
		 
		portsc &= ~(PORT_RWC_BITS | PORT_CEC | PORT_WAKE_BITS);
		writel(portsc, ports[port_index]->addr);
	}

	 
	if (hcd->speed < HCD_USB3) {
		if (bus_state->bus_suspended) {
			spin_unlock_irqrestore(&xhci->lock, flags);
			msleep(USB_RESUME_TIMEOUT);
			spin_lock_irqsave(&xhci->lock, flags);
		}
		for_each_set_bit(port_index, &bus_state->bus_suspended,
				 BITS_PER_LONG) {
			 
			xhci_test_and_clear_bit(xhci, ports[port_index],
						PORT_PLC);
			xhci_set_link_state(xhci, ports[port_index], XDEV_U0);
		}
	}

	 
	for_each_set_bit(port_index, &bus_state->bus_suspended, BITS_PER_LONG) {
		sret = xhci_handshake(ports[port_index]->addr, PORT_PLC,
				      PORT_PLC, 10 * 1000);
		if (sret) {
			xhci_warn(xhci, "port %d-%d resume PLC timeout\n",
				  hcd->self.busnum, port_index + 1);
			continue;
		}
		xhci_test_and_clear_bit(xhci, ports[port_index], PORT_PLC);
		slot_id = xhci_find_slot_id_by_port(hcd, xhci, port_index + 1);
		if (slot_id)
			xhci_ring_device(xhci, slot_id);
	}
	(void) readl(&xhci->op_regs->command);

	bus_state->next_statechange = jiffies + msecs_to_jiffies(5);
	 
	temp = readl(&xhci->op_regs->command);
	temp |= CMD_EIE;
	writel(temp, &xhci->op_regs->command);
	temp = readl(&xhci->op_regs->command);

	spin_unlock_irqrestore(&xhci->lock, flags);
	return 0;
}

unsigned long xhci_get_resuming_ports(struct usb_hcd *hcd)
{
	struct xhci_hub *rhub = xhci_get_rhub(hcd);

	 
	return rhub->bus_state.resuming_ports;	 
}

#endif	 
