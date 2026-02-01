
 

#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/units.h>

#include "sb_regs.h"
#include "tb.h"

#define USB4_DATA_RETRIES		3
#define USB4_DATA_DWORDS		16

enum usb4_sb_target {
	USB4_SB_TARGET_ROUTER,
	USB4_SB_TARGET_PARTNER,
	USB4_SB_TARGET_RETIMER,
};

#define USB4_NVM_READ_OFFSET_MASK	GENMASK(23, 2)
#define USB4_NVM_READ_OFFSET_SHIFT	2
#define USB4_NVM_READ_LENGTH_MASK	GENMASK(27, 24)
#define USB4_NVM_READ_LENGTH_SHIFT	24

#define USB4_NVM_SET_OFFSET_MASK	USB4_NVM_READ_OFFSET_MASK
#define USB4_NVM_SET_OFFSET_SHIFT	USB4_NVM_READ_OFFSET_SHIFT

#define USB4_DROM_ADDRESS_MASK		GENMASK(14, 2)
#define USB4_DROM_ADDRESS_SHIFT		2
#define USB4_DROM_SIZE_MASK		GENMASK(19, 15)
#define USB4_DROM_SIZE_SHIFT		15

#define USB4_NVM_SECTOR_SIZE_MASK	GENMASK(23, 0)

#define USB4_BA_LENGTH_MASK		GENMASK(7, 0)
#define USB4_BA_INDEX_MASK		GENMASK(15, 0)

enum usb4_ba_index {
	USB4_BA_MAX_USB3 = 0x1,
	USB4_BA_MIN_DP_AUX = 0x2,
	USB4_BA_MIN_DP_MAIN = 0x3,
	USB4_BA_MAX_PCIE = 0x4,
	USB4_BA_MAX_HI = 0x5,
};

#define USB4_BA_VALUE_MASK		GENMASK(31, 16)
#define USB4_BA_VALUE_SHIFT		16

static int usb4_native_switch_op(struct tb_switch *sw, u16 opcode,
				 u32 *metadata, u8 *status,
				 const void *tx_data, size_t tx_dwords,
				 void *rx_data, size_t rx_dwords)
{
	u32 val;
	int ret;

	if (metadata) {
		ret = tb_sw_write(sw, metadata, TB_CFG_SWITCH, ROUTER_CS_25, 1);
		if (ret)
			return ret;
	}
	if (tx_dwords) {
		ret = tb_sw_write(sw, tx_data, TB_CFG_SWITCH, ROUTER_CS_9,
				  tx_dwords);
		if (ret)
			return ret;
	}

	val = opcode | ROUTER_CS_26_OV;
	ret = tb_sw_write(sw, &val, TB_CFG_SWITCH, ROUTER_CS_26, 1);
	if (ret)
		return ret;

	ret = tb_switch_wait_for_bit(sw, ROUTER_CS_26, ROUTER_CS_26_OV, 0, 500);
	if (ret)
		return ret;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, ROUTER_CS_26, 1);
	if (ret)
		return ret;

	if (val & ROUTER_CS_26_ONS)
		return -EOPNOTSUPP;

	if (status)
		*status = (val & ROUTER_CS_26_STATUS_MASK) >>
			ROUTER_CS_26_STATUS_SHIFT;

	if (metadata) {
		ret = tb_sw_read(sw, metadata, TB_CFG_SWITCH, ROUTER_CS_25, 1);
		if (ret)
			return ret;
	}
	if (rx_dwords) {
		ret = tb_sw_read(sw, rx_data, TB_CFG_SWITCH, ROUTER_CS_9,
				 rx_dwords);
		if (ret)
			return ret;
	}

	return 0;
}

static int __usb4_switch_op(struct tb_switch *sw, u16 opcode, u32 *metadata,
			    u8 *status, const void *tx_data, size_t tx_dwords,
			    void *rx_data, size_t rx_dwords)
{
	const struct tb_cm_ops *cm_ops = sw->tb->cm_ops;

	if (tx_dwords > USB4_DATA_DWORDS || rx_dwords > USB4_DATA_DWORDS)
		return -EINVAL;

	 
	if (cm_ops->usb4_switch_op) {
		int ret;

		ret = cm_ops->usb4_switch_op(sw, opcode, metadata, status,
					     tx_data, tx_dwords, rx_data,
					     rx_dwords);
		if (ret != -EOPNOTSUPP)
			return ret;

		 
	}

	return usb4_native_switch_op(sw, opcode, metadata, status, tx_data,
				     tx_dwords, rx_data, rx_dwords);
}

static inline int usb4_switch_op(struct tb_switch *sw, u16 opcode,
				 u32 *metadata, u8 *status)
{
	return __usb4_switch_op(sw, opcode, metadata, status, NULL, 0, NULL, 0);
}

static inline int usb4_switch_op_data(struct tb_switch *sw, u16 opcode,
				      u32 *metadata, u8 *status,
				      const void *tx_data, size_t tx_dwords,
				      void *rx_data, size_t rx_dwords)
{
	return __usb4_switch_op(sw, opcode, metadata, status, tx_data,
				tx_dwords, rx_data, rx_dwords);
}

static void usb4_switch_check_wakes(struct tb_switch *sw)
{
	bool wakeup_usb4 = false;
	struct usb4_port *usb4;
	struct tb_port *port;
	bool wakeup = false;
	u32 val;

	if (!device_may_wakeup(&sw->dev))
		return;

	if (tb_route(sw)) {
		if (tb_sw_read(sw, &val, TB_CFG_SWITCH, ROUTER_CS_6, 1))
			return;

		tb_sw_dbg(sw, "PCIe wake: %s, USB3 wake: %s\n",
			  (val & ROUTER_CS_6_WOPS) ? "yes" : "no",
			  (val & ROUTER_CS_6_WOUS) ? "yes" : "no");

		wakeup = val & (ROUTER_CS_6_WOPS | ROUTER_CS_6_WOUS);
	}

	 
	tb_switch_for_each_port(sw, port) {
		if (!port->cap_usb4)
			continue;

		if (tb_port_read(port, &val, TB_CFG_PORT,
				 port->cap_usb4 + PORT_CS_18, 1))
			break;

		tb_port_dbg(port, "USB4 wake: %s, connection wake: %s, disconnection wake: %s\n",
			    (val & PORT_CS_18_WOU4S) ? "yes" : "no",
			    (val & PORT_CS_18_WOCS) ? "yes" : "no",
			    (val & PORT_CS_18_WODS) ? "yes" : "no");

		wakeup_usb4 = val & (PORT_CS_18_WOU4S | PORT_CS_18_WOCS |
				     PORT_CS_18_WODS);

		usb4 = port->usb4;
		if (device_may_wakeup(&usb4->dev) && wakeup_usb4)
			pm_wakeup_event(&usb4->dev, 0);

		wakeup |= wakeup_usb4;
	}

	if (wakeup)
		pm_wakeup_event(&sw->dev, 0);
}

static bool link_is_usb4(struct tb_port *port)
{
	u32 val;

	if (!port->cap_usb4)
		return false;

	if (tb_port_read(port, &val, TB_CFG_PORT,
			 port->cap_usb4 + PORT_CS_18, 1))
		return false;

	return !(val & PORT_CS_18_TCM);
}

 
int usb4_switch_setup(struct tb_switch *sw)
{
	struct tb_switch *parent = tb_switch_parent(sw);
	struct tb_port *down;
	bool tbt3, xhci;
	u32 val = 0;
	int ret;

	usb4_switch_check_wakes(sw);

	if (!tb_route(sw))
		return 0;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, ROUTER_CS_6, 1);
	if (ret)
		return ret;

	down = tb_switch_downstream_port(sw);
	sw->link_usb4 = link_is_usb4(down);
	tb_sw_dbg(sw, "link: %s\n", sw->link_usb4 ? "USB4" : "TBT");

	xhci = val & ROUTER_CS_6_HCI;
	tbt3 = !(val & ROUTER_CS_6_TNS);

	tb_sw_dbg(sw, "TBT3 support: %s, xHCI: %s\n",
		  tbt3 ? "yes" : "no", xhci ? "yes" : "no");

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, ROUTER_CS_5, 1);
	if (ret)
		return ret;

	if (tb_acpi_may_tunnel_usb3() && sw->link_usb4 &&
	    tb_switch_find_port(parent, TB_TYPE_USB3_DOWN)) {
		val |= ROUTER_CS_5_UTO;
		xhci = false;
	}

	 
	if (tb_acpi_may_tunnel_pcie() &&
	    tb_switch_find_port(parent, TB_TYPE_PCIE_DOWN)) {
		val |= ROUTER_CS_5_PTO;
		 
		if (xhci)
			val |= ROUTER_CS_5_HCO;
	}

	 
	val |= ROUTER_CS_5_C3S;

	return tb_sw_write(sw, &val, TB_CFG_SWITCH, ROUTER_CS_5, 1);
}

 
int usb4_switch_configuration_valid(struct tb_switch *sw)
{
	u32 val;
	int ret;

	if (!tb_route(sw))
		return 0;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, ROUTER_CS_5, 1);
	if (ret)
		return ret;

	val |= ROUTER_CS_5_CV;

	ret = tb_sw_write(sw, &val, TB_CFG_SWITCH, ROUTER_CS_5, 1);
	if (ret)
		return ret;

	return tb_switch_wait_for_bit(sw, ROUTER_CS_6, ROUTER_CS_6_CR,
				      ROUTER_CS_6_CR, 50);
}

 
int usb4_switch_read_uid(struct tb_switch *sw, u64 *uid)
{
	return tb_sw_read(sw, uid, TB_CFG_SWITCH, ROUTER_CS_7, 2);
}

static int usb4_switch_drom_read_block(void *data,
				       unsigned int dwaddress, void *buf,
				       size_t dwords)
{
	struct tb_switch *sw = data;
	u8 status = 0;
	u32 metadata;
	int ret;

	metadata = (dwords << USB4_DROM_SIZE_SHIFT) & USB4_DROM_SIZE_MASK;
	metadata |= (dwaddress << USB4_DROM_ADDRESS_SHIFT) &
		USB4_DROM_ADDRESS_MASK;

	ret = usb4_switch_op_data(sw, USB4_SWITCH_OP_DROM_READ, &metadata,
				  &status, NULL, 0, buf, dwords);
	if (ret)
		return ret;

	return status ? -EIO : 0;
}

 
int usb4_switch_drom_read(struct tb_switch *sw, unsigned int address, void *buf,
			  size_t size)
{
	return tb_nvm_read_data(address, buf, size, USB4_DATA_RETRIES,
				usb4_switch_drom_read_block, sw);
}

 
bool usb4_switch_lane_bonding_possible(struct tb_switch *sw)
{
	struct tb_port *up;
	int ret;
	u32 val;

	up = tb_upstream_port(sw);
	ret = tb_port_read(up, &val, TB_CFG_PORT, up->cap_usb4 + PORT_CS_18, 1);
	if (ret)
		return false;

	return !!(val & PORT_CS_18_BE);
}

 
int usb4_switch_set_wake(struct tb_switch *sw, unsigned int flags)
{
	struct usb4_port *usb4;
	struct tb_port *port;
	u64 route = tb_route(sw);
	u32 val;
	int ret;

	 
	tb_switch_for_each_port(sw, port) {
		if (!tb_port_is_null(port))
			continue;
		if (!route && tb_is_upstream_port(port))
			continue;
		if (!port->cap_usb4)
			continue;

		ret = tb_port_read(port, &val, TB_CFG_PORT,
				   port->cap_usb4 + PORT_CS_19, 1);
		if (ret)
			return ret;

		val &= ~(PORT_CS_19_WOC | PORT_CS_19_WOD | PORT_CS_19_WOU4);

		if (tb_is_upstream_port(port)) {
			val |= PORT_CS_19_WOU4;
		} else {
			bool configured = val & PORT_CS_19_PC;
			usb4 = port->usb4;

			if (((flags & TB_WAKE_ON_CONNECT) |
			      device_may_wakeup(&usb4->dev)) && !configured)
				val |= PORT_CS_19_WOC;
			if (((flags & TB_WAKE_ON_DISCONNECT) |
			      device_may_wakeup(&usb4->dev)) && configured)
				val |= PORT_CS_19_WOD;
			if ((flags & TB_WAKE_ON_USB4) && configured)
				val |= PORT_CS_19_WOU4;
		}

		ret = tb_port_write(port, &val, TB_CFG_PORT,
				    port->cap_usb4 + PORT_CS_19, 1);
		if (ret)
			return ret;
	}

	 
	if (route) {
		ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, ROUTER_CS_5, 1);
		if (ret)
			return ret;

		val &= ~(ROUTER_CS_5_WOP | ROUTER_CS_5_WOU | ROUTER_CS_5_WOD);
		if (flags & TB_WAKE_ON_USB3)
			val |= ROUTER_CS_5_WOU;
		if (flags & TB_WAKE_ON_PCIE)
			val |= ROUTER_CS_5_WOP;
		if (flags & TB_WAKE_ON_DP)
			val |= ROUTER_CS_5_WOD;

		ret = tb_sw_write(sw, &val, TB_CFG_SWITCH, ROUTER_CS_5, 1);
		if (ret)
			return ret;
	}

	return 0;
}

 
int usb4_switch_set_sleep(struct tb_switch *sw)
{
	int ret;
	u32 val;

	 
	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, ROUTER_CS_5, 1);
	if (ret)
		return ret;

	val |= ROUTER_CS_5_SLP;

	ret = tb_sw_write(sw, &val, TB_CFG_SWITCH, ROUTER_CS_5, 1);
	if (ret)
		return ret;

	return tb_switch_wait_for_bit(sw, ROUTER_CS_6, ROUTER_CS_6_SLPR,
				      ROUTER_CS_6_SLPR, 500);
}

 
int usb4_switch_nvm_sector_size(struct tb_switch *sw)
{
	u32 metadata;
	u8 status;
	int ret;

	ret = usb4_switch_op(sw, USB4_SWITCH_OP_NVM_SECTOR_SIZE, &metadata,
			     &status);
	if (ret)
		return ret;

	if (status)
		return status == 0x2 ? -EOPNOTSUPP : -EIO;

	return metadata & USB4_NVM_SECTOR_SIZE_MASK;
}

static int usb4_switch_nvm_read_block(void *data,
	unsigned int dwaddress, void *buf, size_t dwords)
{
	struct tb_switch *sw = data;
	u8 status = 0;
	u32 metadata;
	int ret;

	metadata = (dwords << USB4_NVM_READ_LENGTH_SHIFT) &
		   USB4_NVM_READ_LENGTH_MASK;
	metadata |= (dwaddress << USB4_NVM_READ_OFFSET_SHIFT) &
		   USB4_NVM_READ_OFFSET_MASK;

	ret = usb4_switch_op_data(sw, USB4_SWITCH_OP_NVM_READ, &metadata,
				  &status, NULL, 0, buf, dwords);
	if (ret)
		return ret;

	return status ? -EIO : 0;
}

 
int usb4_switch_nvm_read(struct tb_switch *sw, unsigned int address, void *buf,
			 size_t size)
{
	return tb_nvm_read_data(address, buf, size, USB4_DATA_RETRIES,
				usb4_switch_nvm_read_block, sw);
}

 
int usb4_switch_nvm_set_offset(struct tb_switch *sw, unsigned int address)
{
	u32 metadata, dwaddress;
	u8 status = 0;
	int ret;

	dwaddress = address / 4;
	metadata = (dwaddress << USB4_NVM_SET_OFFSET_SHIFT) &
		   USB4_NVM_SET_OFFSET_MASK;

	ret = usb4_switch_op(sw, USB4_SWITCH_OP_NVM_SET_OFFSET, &metadata,
			     &status);
	if (ret)
		return ret;

	return status ? -EIO : 0;
}

static int usb4_switch_nvm_write_next_block(void *data, unsigned int dwaddress,
					    const void *buf, size_t dwords)
{
	struct tb_switch *sw = data;
	u8 status;
	int ret;

	ret = usb4_switch_op_data(sw, USB4_SWITCH_OP_NVM_WRITE, NULL, &status,
				  buf, dwords, NULL, 0);
	if (ret)
		return ret;

	return status ? -EIO : 0;
}

 
int usb4_switch_nvm_write(struct tb_switch *sw, unsigned int address,
			  const void *buf, size_t size)
{
	int ret;

	ret = usb4_switch_nvm_set_offset(sw, address);
	if (ret)
		return ret;

	return tb_nvm_write_data(address, buf, size, USB4_DATA_RETRIES,
				 usb4_switch_nvm_write_next_block, sw);
}

 
int usb4_switch_nvm_authenticate(struct tb_switch *sw)
{
	int ret;

	ret = usb4_switch_op(sw, USB4_SWITCH_OP_NVM_AUTH, NULL, NULL);
	switch (ret) {
	 
	case -EACCES:
	case -ENOTCONN:
	case -ETIMEDOUT:
		return 0;

	default:
		return ret;
	}
}

 
int usb4_switch_nvm_authenticate_status(struct tb_switch *sw, u32 *status)
{
	const struct tb_cm_ops *cm_ops = sw->tb->cm_ops;
	u16 opcode;
	u32 val;
	int ret;

	if (cm_ops->usb4_switch_nvm_authenticate_status) {
		ret = cm_ops->usb4_switch_nvm_authenticate_status(sw, status);
		if (ret != -EOPNOTSUPP)
			return ret;
	}

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, ROUTER_CS_26, 1);
	if (ret)
		return ret;

	 
	opcode = val & ROUTER_CS_26_OPCODE_MASK;
	if (opcode == USB4_SWITCH_OP_NVM_AUTH) {
		if (val & ROUTER_CS_26_OV)
			return -EBUSY;
		if (val & ROUTER_CS_26_ONS)
			return -EOPNOTSUPP;

		*status = (val & ROUTER_CS_26_STATUS_MASK) >>
			ROUTER_CS_26_STATUS_SHIFT;
	} else {
		*status = 0;
	}

	return 0;
}

 
int usb4_switch_credits_init(struct tb_switch *sw)
{
	int max_usb3, min_dp_aux, min_dp_main, max_pcie, max_dma;
	int ret, length, i, nports;
	const struct tb_port *port;
	u32 data[USB4_DATA_DWORDS];
	u32 metadata = 0;
	u8 status = 0;

	memset(data, 0, sizeof(data));
	ret = usb4_switch_op_data(sw, USB4_SWITCH_OP_BUFFER_ALLOC, &metadata,
				  &status, NULL, 0, data, ARRAY_SIZE(data));
	if (ret)
		return ret;
	if (status)
		return -EIO;

	length = metadata & USB4_BA_LENGTH_MASK;
	if (WARN_ON(length > ARRAY_SIZE(data)))
		return -EMSGSIZE;

	max_usb3 = -1;
	min_dp_aux = -1;
	min_dp_main = -1;
	max_pcie = -1;
	max_dma = -1;

	tb_sw_dbg(sw, "credit allocation parameters:\n");

	for (i = 0; i < length; i++) {
		u16 index, value;

		index = data[i] & USB4_BA_INDEX_MASK;
		value = (data[i] & USB4_BA_VALUE_MASK) >> USB4_BA_VALUE_SHIFT;

		switch (index) {
		case USB4_BA_MAX_USB3:
			tb_sw_dbg(sw, " USB3: %u\n", value);
			max_usb3 = value;
			break;
		case USB4_BA_MIN_DP_AUX:
			tb_sw_dbg(sw, " DP AUX: %u\n", value);
			min_dp_aux = value;
			break;
		case USB4_BA_MIN_DP_MAIN:
			tb_sw_dbg(sw, " DP main: %u\n", value);
			min_dp_main = value;
			break;
		case USB4_BA_MAX_PCIE:
			tb_sw_dbg(sw, " PCIe: %u\n", value);
			max_pcie = value;
			break;
		case USB4_BA_MAX_HI:
			tb_sw_dbg(sw, " DMA: %u\n", value);
			max_dma = value;
			break;
		default:
			tb_sw_dbg(sw, " unknown credit allocation index %#x, skipping\n",
				  index);
			break;
		}
	}

	 

	 
	if (!tb_route(sw) && max_dma < 0) {
		tb_sw_warn(sw, "host router is missing baMaxHI\n");
		goto err_invalid;
	}

	nports = 0;
	tb_switch_for_each_port(sw, port) {
		if (tb_port_is_null(port))
			nports++;
	}

	 
	if (nports > 2 && (min_dp_aux < 0 || min_dp_main < 0)) {
		tb_sw_warn(sw, "multiple USB4 ports require baMinDPaux/baMinDPmain\n");
		goto err_invalid;
	}

	tb_switch_for_each_port(sw, port) {
		if (tb_port_is_dpout(port) && min_dp_main < 0) {
			tb_sw_warn(sw, "missing baMinDPmain");
			goto err_invalid;
		}
		if ((tb_port_is_dpin(port) || tb_port_is_dpout(port)) &&
		    min_dp_aux < 0) {
			tb_sw_warn(sw, "missing baMinDPaux");
			goto err_invalid;
		}
		if ((tb_port_is_usb3_down(port) || tb_port_is_usb3_up(port)) &&
		    max_usb3 < 0) {
			tb_sw_warn(sw, "missing baMaxUSB3");
			goto err_invalid;
		}
		if ((tb_port_is_pcie_down(port) || tb_port_is_pcie_up(port)) &&
		    max_pcie < 0) {
			tb_sw_warn(sw, "missing baMaxPCIe");
			goto err_invalid;
		}
	}

	 
	sw->credit_allocation = true;
	if (max_usb3 > 0)
		sw->max_usb3_credits = max_usb3;
	if (min_dp_aux > 0)
		sw->min_dp_aux_credits = min_dp_aux;
	if (min_dp_main > 0)
		sw->min_dp_main_credits = min_dp_main;
	if (max_pcie > 0)
		sw->max_pcie_credits = max_pcie;
	if (max_dma > 0)
		sw->max_dma_credits = max_dma;

	return 0;

err_invalid:
	return -EINVAL;
}

 
bool usb4_switch_query_dp_resource(struct tb_switch *sw, struct tb_port *in)
{
	u32 metadata = in->port;
	u8 status;
	int ret;

	ret = usb4_switch_op(sw, USB4_SWITCH_OP_QUERY_DP_RESOURCE, &metadata,
			     &status);
	 
	if (ret == -EOPNOTSUPP)
		return true;
	if (ret)
		return false;

	return !status;
}

 
int usb4_switch_alloc_dp_resource(struct tb_switch *sw, struct tb_port *in)
{
	u32 metadata = in->port;
	u8 status;
	int ret;

	ret = usb4_switch_op(sw, USB4_SWITCH_OP_ALLOC_DP_RESOURCE, &metadata,
			     &status);
	if (ret == -EOPNOTSUPP)
		return 0;
	if (ret)
		return ret;

	return status ? -EBUSY : 0;
}

 
int usb4_switch_dealloc_dp_resource(struct tb_switch *sw, struct tb_port *in)
{
	u32 metadata = in->port;
	u8 status;
	int ret;

	ret = usb4_switch_op(sw, USB4_SWITCH_OP_DEALLOC_DP_RESOURCE, &metadata,
			     &status);
	if (ret == -EOPNOTSUPP)
		return 0;
	if (ret)
		return ret;

	return status ? -EIO : 0;
}

static int usb4_port_idx(const struct tb_switch *sw, const struct tb_port *port)
{
	struct tb_port *p;
	int usb4_idx = 0;

	 
	tb_switch_for_each_port(sw, p) {
		if (!tb_port_is_null(p))
			continue;
		if (tb_is_upstream_port(p))
			continue;
		if (!p->link_nr) {
			if (p == port)
				break;
			usb4_idx++;
		}
	}

	return usb4_idx;
}

 
struct tb_port *usb4_switch_map_pcie_down(struct tb_switch *sw,
					  const struct tb_port *port)
{
	int usb4_idx = usb4_port_idx(sw, port);
	struct tb_port *p;
	int pcie_idx = 0;

	 
	tb_switch_for_each_port(sw, p) {
		if (!tb_port_is_pcie_down(p))
			continue;

		if (pcie_idx == usb4_idx)
			return p;

		pcie_idx++;
	}

	return NULL;
}

 
struct tb_port *usb4_switch_map_usb3_down(struct tb_switch *sw,
					  const struct tb_port *port)
{
	int usb4_idx = usb4_port_idx(sw, port);
	struct tb_port *p;
	int usb_idx = 0;

	 
	tb_switch_for_each_port(sw, p) {
		if (!tb_port_is_usb3_down(p))
			continue;

		if (usb_idx == usb4_idx)
			return p;

		usb_idx++;
	}

	return NULL;
}

 
int usb4_switch_add_ports(struct tb_switch *sw)
{
	struct tb_port *port;

	if (tb_switch_is_icm(sw) || !tb_switch_is_usb4(sw))
		return 0;

	tb_switch_for_each_port(sw, port) {
		struct usb4_port *usb4;

		if (!tb_port_is_null(port))
			continue;
		if (!port->cap_usb4)
			continue;

		usb4 = usb4_port_device_add(port);
		if (IS_ERR(usb4)) {
			usb4_switch_remove_ports(sw);
			return PTR_ERR(usb4);
		}

		port->usb4 = usb4;
	}

	return 0;
}

 
void usb4_switch_remove_ports(struct tb_switch *sw)
{
	struct tb_port *port;

	tb_switch_for_each_port(sw, port) {
		if (port->usb4) {
			usb4_port_device_remove(port->usb4);
			port->usb4 = NULL;
		}
	}
}

 
int usb4_port_unlock(struct tb_port *port)
{
	int ret;
	u32 val;

	ret = tb_port_read(port, &val, TB_CFG_PORT, ADP_CS_4, 1);
	if (ret)
		return ret;

	val &= ~ADP_CS_4_LCK;
	return tb_port_write(port, &val, TB_CFG_PORT, ADP_CS_4, 1);
}

 
int usb4_port_hotplug_enable(struct tb_port *port)
{
	int ret;
	u32 val;

	ret = tb_port_read(port, &val, TB_CFG_PORT, ADP_CS_5, 1);
	if (ret)
		return ret;

	val &= ~ADP_CS_5_DHP;
	return tb_port_write(port, &val, TB_CFG_PORT, ADP_CS_5, 1);
}

static int usb4_port_set_configured(struct tb_port *port, bool configured)
{
	int ret;
	u32 val;

	if (!port->cap_usb4)
		return -EINVAL;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_usb4 + PORT_CS_19, 1);
	if (ret)
		return ret;

	if (configured)
		val |= PORT_CS_19_PC;
	else
		val &= ~PORT_CS_19_PC;

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_usb4 + PORT_CS_19, 1);
}

 
int usb4_port_configure(struct tb_port *port)
{
	return usb4_port_set_configured(port, true);
}

 
void usb4_port_unconfigure(struct tb_port *port)
{
	usb4_port_set_configured(port, false);
}

static int usb4_set_xdomain_configured(struct tb_port *port, bool configured)
{
	int ret;
	u32 val;

	if (!port->cap_usb4)
		return -EINVAL;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_usb4 + PORT_CS_19, 1);
	if (ret)
		return ret;

	if (configured)
		val |= PORT_CS_19_PID;
	else
		val &= ~PORT_CS_19_PID;

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_usb4 + PORT_CS_19, 1);
}

 
int usb4_port_configure_xdomain(struct tb_port *port, struct tb_xdomain *xd)
{
	xd->link_usb4 = link_is_usb4(port);
	return usb4_set_xdomain_configured(port, true);
}

 
void usb4_port_unconfigure_xdomain(struct tb_port *port)
{
	usb4_set_xdomain_configured(port, false);
}

static int usb4_port_wait_for_bit(struct tb_port *port, u32 offset, u32 bit,
				  u32 value, int timeout_msec)
{
	ktime_t timeout = ktime_add_ms(ktime_get(), timeout_msec);

	do {
		u32 val;
		int ret;

		ret = tb_port_read(port, &val, TB_CFG_PORT, offset, 1);
		if (ret)
			return ret;

		if ((val & bit) == value)
			return 0;

		usleep_range(50, 100);
	} while (ktime_before(ktime_get(), timeout));

	return -ETIMEDOUT;
}

static int usb4_port_read_data(struct tb_port *port, void *data, size_t dwords)
{
	if (dwords > USB4_DATA_DWORDS)
		return -EINVAL;

	return tb_port_read(port, data, TB_CFG_PORT, port->cap_usb4 + PORT_CS_2,
			    dwords);
}

static int usb4_port_write_data(struct tb_port *port, const void *data,
				size_t dwords)
{
	if (dwords > USB4_DATA_DWORDS)
		return -EINVAL;

	return tb_port_write(port, data, TB_CFG_PORT, port->cap_usb4 + PORT_CS_2,
			     dwords);
}

static int usb4_port_sb_read(struct tb_port *port, enum usb4_sb_target target,
			     u8 index, u8 reg, void *buf, u8 size)
{
	size_t dwords = DIV_ROUND_UP(size, 4);
	int ret;
	u32 val;

	if (!port->cap_usb4)
		return -EINVAL;

	val = reg;
	val |= size << PORT_CS_1_LENGTH_SHIFT;
	val |= (target << PORT_CS_1_TARGET_SHIFT) & PORT_CS_1_TARGET_MASK;
	if (target == USB4_SB_TARGET_RETIMER)
		val |= (index << PORT_CS_1_RETIMER_INDEX_SHIFT);
	val |= PORT_CS_1_PND;

	ret = tb_port_write(port, &val, TB_CFG_PORT,
			    port->cap_usb4 + PORT_CS_1, 1);
	if (ret)
		return ret;

	ret = usb4_port_wait_for_bit(port, port->cap_usb4 + PORT_CS_1,
				     PORT_CS_1_PND, 0, 500);
	if (ret)
		return ret;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			    port->cap_usb4 + PORT_CS_1, 1);
	if (ret)
		return ret;

	if (val & PORT_CS_1_NR)
		return -ENODEV;
	if (val & PORT_CS_1_RC)
		return -EIO;

	return buf ? usb4_port_read_data(port, buf, dwords) : 0;
}

static int usb4_port_sb_write(struct tb_port *port, enum usb4_sb_target target,
			      u8 index, u8 reg, const void *buf, u8 size)
{
	size_t dwords = DIV_ROUND_UP(size, 4);
	int ret;
	u32 val;

	if (!port->cap_usb4)
		return -EINVAL;

	if (buf) {
		ret = usb4_port_write_data(port, buf, dwords);
		if (ret)
			return ret;
	}

	val = reg;
	val |= size << PORT_CS_1_LENGTH_SHIFT;
	val |= PORT_CS_1_WNR_WRITE;
	val |= (target << PORT_CS_1_TARGET_SHIFT) & PORT_CS_1_TARGET_MASK;
	if (target == USB4_SB_TARGET_RETIMER)
		val |= (index << PORT_CS_1_RETIMER_INDEX_SHIFT);
	val |= PORT_CS_1_PND;

	ret = tb_port_write(port, &val, TB_CFG_PORT,
			    port->cap_usb4 + PORT_CS_1, 1);
	if (ret)
		return ret;

	ret = usb4_port_wait_for_bit(port, port->cap_usb4 + PORT_CS_1,
				     PORT_CS_1_PND, 0, 500);
	if (ret)
		return ret;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			    port->cap_usb4 + PORT_CS_1, 1);
	if (ret)
		return ret;

	if (val & PORT_CS_1_NR)
		return -ENODEV;
	if (val & PORT_CS_1_RC)
		return -EIO;

	return 0;
}

static int usb4_port_sb_opcode_err_to_errno(u32 val)
{
	switch (val) {
	case 0:
		return 0;
	case USB4_SB_OPCODE_ERR:
		return -EAGAIN;
	case USB4_SB_OPCODE_ONS:
		return -EOPNOTSUPP;
	default:
		return -EIO;
	}
}

static int usb4_port_sb_op(struct tb_port *port, enum usb4_sb_target target,
			   u8 index, enum usb4_sb_opcode opcode, int timeout_msec)
{
	ktime_t timeout;
	u32 val;
	int ret;

	val = opcode;
	ret = usb4_port_sb_write(port, target, index, USB4_SB_OPCODE, &val,
				 sizeof(val));
	if (ret)
		return ret;

	timeout = ktime_add_ms(ktime_get(), timeout_msec);

	do {
		 
		ret = usb4_port_sb_read(port, target, index, USB4_SB_OPCODE,
					&val, sizeof(val));
		if (ret)
			return ret;

		if (val != opcode)
			return usb4_port_sb_opcode_err_to_errno(val);
	} while (ktime_before(ktime_get(), timeout));

	return -ETIMEDOUT;
}

static int usb4_port_set_router_offline(struct tb_port *port, bool offline)
{
	u32 val = !offline;
	int ret;

	ret = usb4_port_sb_write(port, USB4_SB_TARGET_ROUTER, 0,
				  USB4_SB_METADATA, &val, sizeof(val));
	if (ret)
		return ret;

	val = USB4_SB_OPCODE_ROUTER_OFFLINE;
	return usb4_port_sb_write(port, USB4_SB_TARGET_ROUTER, 0,
				  USB4_SB_OPCODE, &val, sizeof(val));
}

 
int usb4_port_router_offline(struct tb_port *port)
{
	return usb4_port_set_router_offline(port, true);
}

 
int usb4_port_router_online(struct tb_port *port)
{
	return usb4_port_set_router_offline(port, false);
}

 
int usb4_port_enumerate_retimers(struct tb_port *port)
{
	u32 val;

	val = USB4_SB_OPCODE_ENUMERATE_RETIMERS;
	return usb4_port_sb_write(port, USB4_SB_TARGET_ROUTER, 0,
				  USB4_SB_OPCODE, &val, sizeof(val));
}

 
bool usb4_port_clx_supported(struct tb_port *port)
{
	int ret;
	u32 val;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_usb4 + PORT_CS_18, 1);
	if (ret)
		return false;

	return !!(val & PORT_CS_18_CPS);
}

 
int usb4_port_margining_caps(struct tb_port *port, u32 *caps)
{
	int ret;

	ret = usb4_port_sb_op(port, USB4_SB_TARGET_ROUTER, 0,
			      USB4_SB_OPCODE_READ_LANE_MARGINING_CAP, 500);
	if (ret)
		return ret;

	return usb4_port_sb_read(port, USB4_SB_TARGET_ROUTER, 0,
				 USB4_SB_DATA, caps, sizeof(*caps) * 2);
}

 
int usb4_port_hw_margin(struct tb_port *port, unsigned int lanes,
			unsigned int ber_level, bool timing, bool right_high,
			u32 *results)
{
	u32 val;
	int ret;

	val = lanes;
	if (timing)
		val |= USB4_MARGIN_HW_TIME;
	if (right_high)
		val |= USB4_MARGIN_HW_RH;
	if (ber_level)
		val |= (ber_level << USB4_MARGIN_HW_BER_SHIFT) &
			USB4_MARGIN_HW_BER_MASK;

	ret = usb4_port_sb_write(port, USB4_SB_TARGET_ROUTER, 0,
				 USB4_SB_METADATA, &val, sizeof(val));
	if (ret)
		return ret;

	ret = usb4_port_sb_op(port, USB4_SB_TARGET_ROUTER, 0,
			      USB4_SB_OPCODE_RUN_HW_LANE_MARGINING, 2500);
	if (ret)
		return ret;

	return usb4_port_sb_read(port, USB4_SB_TARGET_ROUTER, 0,
				 USB4_SB_DATA, results, sizeof(*results) * 2);
}

 
int usb4_port_sw_margin(struct tb_port *port, unsigned int lanes, bool timing,
			bool right_high, u32 counter)
{
	u32 val;
	int ret;

	val = lanes;
	if (timing)
		val |= USB4_MARGIN_SW_TIME;
	if (right_high)
		val |= USB4_MARGIN_SW_RH;
	val |= (counter << USB4_MARGIN_SW_COUNTER_SHIFT) &
		USB4_MARGIN_SW_COUNTER_MASK;

	ret = usb4_port_sb_write(port, USB4_SB_TARGET_ROUTER, 0,
				 USB4_SB_METADATA, &val, sizeof(val));
	if (ret)
		return ret;

	return usb4_port_sb_op(port, USB4_SB_TARGET_ROUTER, 0,
			       USB4_SB_OPCODE_RUN_SW_LANE_MARGINING, 2500);
}

 
int usb4_port_sw_margin_errors(struct tb_port *port, u32 *errors)
{
	int ret;

	ret = usb4_port_sb_op(port, USB4_SB_TARGET_ROUTER, 0,
			      USB4_SB_OPCODE_READ_SW_MARGIN_ERR, 150);
	if (ret)
		return ret;

	return usb4_port_sb_read(port, USB4_SB_TARGET_ROUTER, 0,
				 USB4_SB_METADATA, errors, sizeof(*errors));
}

static inline int usb4_port_retimer_op(struct tb_port *port, u8 index,
				       enum usb4_sb_opcode opcode,
				       int timeout_msec)
{
	return usb4_port_sb_op(port, USB4_SB_TARGET_RETIMER, index, opcode,
			       timeout_msec);
}

 
int usb4_port_retimer_set_inbound_sbtx(struct tb_port *port, u8 index)
{
	int ret;

	ret = usb4_port_retimer_op(port, index, USB4_SB_OPCODE_SET_INBOUND_SBTX,
				   500);

	if (ret != -ENODEV)
		return ret;

	 
	return usb4_port_retimer_op(port, index, USB4_SB_OPCODE_SET_INBOUND_SBTX,
				    500);
}

 
int usb4_port_retimer_unset_inbound_sbtx(struct tb_port *port, u8 index)
{
	return usb4_port_retimer_op(port, index,
				    USB4_SB_OPCODE_UNSET_INBOUND_SBTX, 500);
}

 
int usb4_port_retimer_read(struct tb_port *port, u8 index, u8 reg, void *buf,
			   u8 size)
{
	return usb4_port_sb_read(port, USB4_SB_TARGET_RETIMER, index, reg, buf,
				 size);
}

 
int usb4_port_retimer_write(struct tb_port *port, u8 index, u8 reg,
			    const void *buf, u8 size)
{
	return usb4_port_sb_write(port, USB4_SB_TARGET_RETIMER, index, reg, buf,
				  size);
}

 
int usb4_port_retimer_is_last(struct tb_port *port, u8 index)
{
	u32 metadata;
	int ret;

	ret = usb4_port_retimer_op(port, index, USB4_SB_OPCODE_QUERY_LAST_RETIMER,
				   500);
	if (ret)
		return ret;

	ret = usb4_port_retimer_read(port, index, USB4_SB_METADATA, &metadata,
				     sizeof(metadata));
	return ret ? ret : metadata & 1;
}

 
int usb4_port_retimer_nvm_sector_size(struct tb_port *port, u8 index)
{
	u32 metadata;
	int ret;

	ret = usb4_port_retimer_op(port, index, USB4_SB_OPCODE_GET_NVM_SECTOR_SIZE,
				   500);
	if (ret)
		return ret;

	ret = usb4_port_retimer_read(port, index, USB4_SB_METADATA, &metadata,
				     sizeof(metadata));
	return ret ? ret : metadata & USB4_NVM_SECTOR_SIZE_MASK;
}

 
int usb4_port_retimer_nvm_set_offset(struct tb_port *port, u8 index,
				     unsigned int address)
{
	u32 metadata, dwaddress;
	int ret;

	dwaddress = address / 4;
	metadata = (dwaddress << USB4_NVM_SET_OFFSET_SHIFT) &
		  USB4_NVM_SET_OFFSET_MASK;

	ret = usb4_port_retimer_write(port, index, USB4_SB_METADATA, &metadata,
				      sizeof(metadata));
	if (ret)
		return ret;

	return usb4_port_retimer_op(port, index, USB4_SB_OPCODE_NVM_SET_OFFSET,
				    500);
}

struct retimer_info {
	struct tb_port *port;
	u8 index;
};

static int usb4_port_retimer_nvm_write_next_block(void *data,
	unsigned int dwaddress, const void *buf, size_t dwords)

{
	const struct retimer_info *info = data;
	struct tb_port *port = info->port;
	u8 index = info->index;
	int ret;

	ret = usb4_port_retimer_write(port, index, USB4_SB_DATA,
				      buf, dwords * 4);
	if (ret)
		return ret;

	return usb4_port_retimer_op(port, index,
			USB4_SB_OPCODE_NVM_BLOCK_WRITE, 1000);
}

 
int usb4_port_retimer_nvm_write(struct tb_port *port, u8 index, unsigned int address,
				const void *buf, size_t size)
{
	struct retimer_info info = { .port = port, .index = index };
	int ret;

	ret = usb4_port_retimer_nvm_set_offset(port, index, address);
	if (ret)
		return ret;

	return tb_nvm_write_data(address, buf, size, USB4_DATA_RETRIES,
				 usb4_port_retimer_nvm_write_next_block, &info);
}

 
int usb4_port_retimer_nvm_authenticate(struct tb_port *port, u8 index)
{
	u32 val;

	 
	val = USB4_SB_OPCODE_NVM_AUTH_WRITE;
	return usb4_port_sb_write(port, USB4_SB_TARGET_RETIMER, index,
				  USB4_SB_OPCODE, &val, sizeof(val));
}

 
int usb4_port_retimer_nvm_authenticate_status(struct tb_port *port, u8 index,
					      u32 *status)
{
	u32 metadata, val;
	int ret;

	ret = usb4_port_retimer_read(port, index, USB4_SB_OPCODE, &val,
				     sizeof(val));
	if (ret)
		return ret;

	ret = usb4_port_sb_opcode_err_to_errno(val);
	switch (ret) {
	case 0:
		*status = 0;
		return 0;

	case -EAGAIN:
		ret = usb4_port_retimer_read(port, index, USB4_SB_METADATA,
					     &metadata, sizeof(metadata));
		if (ret)
			return ret;

		*status = metadata & USB4_SB_METADATA_NVM_AUTH_WRITE_MASK;
		return 0;

	default:
		return ret;
	}
}

static int usb4_port_retimer_nvm_read_block(void *data, unsigned int dwaddress,
					    void *buf, size_t dwords)
{
	const struct retimer_info *info = data;
	struct tb_port *port = info->port;
	u8 index = info->index;
	u32 metadata;
	int ret;

	metadata = dwaddress << USB4_NVM_READ_OFFSET_SHIFT;
	if (dwords < USB4_DATA_DWORDS)
		metadata |= dwords << USB4_NVM_READ_LENGTH_SHIFT;

	ret = usb4_port_retimer_write(port, index, USB4_SB_METADATA, &metadata,
				      sizeof(metadata));
	if (ret)
		return ret;

	ret = usb4_port_retimer_op(port, index, USB4_SB_OPCODE_NVM_READ, 500);
	if (ret)
		return ret;

	return usb4_port_retimer_read(port, index, USB4_SB_DATA, buf,
				      dwords * 4);
}

 
int usb4_port_retimer_nvm_read(struct tb_port *port, u8 index,
			       unsigned int address, void *buf, size_t size)
{
	struct retimer_info info = { .port = port, .index = index };

	return tb_nvm_read_data(address, buf, size, USB4_DATA_RETRIES,
				usb4_port_retimer_nvm_read_block, &info);
}

static inline unsigned int
usb4_usb3_port_max_bandwidth(const struct tb_port *port, unsigned int bw)
{
	 
	if (port->max_bw)
		return min(bw, port->max_bw);
	return bw;
}

 
int usb4_usb3_port_max_link_rate(struct tb_port *port)
{
	int ret, lr;
	u32 val;

	if (!tb_port_is_usb3_down(port) && !tb_port_is_usb3_up(port))
		return -EINVAL;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_USB3_CS_4, 1);
	if (ret)
		return ret;

	lr = (val & ADP_USB3_CS_4_MSLR_MASK) >> ADP_USB3_CS_4_MSLR_SHIFT;
	ret = lr == ADP_USB3_CS_4_MSLR_20G ? 20000 : 10000;

	return usb4_usb3_port_max_bandwidth(port, ret);
}

 
int usb4_usb3_port_actual_link_rate(struct tb_port *port)
{
	int ret, lr;
	u32 val;

	if (!tb_port_is_usb3_down(port) && !tb_port_is_usb3_up(port))
		return -EINVAL;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_USB3_CS_4, 1);
	if (ret)
		return ret;

	if (!(val & ADP_USB3_CS_4_ULV))
		return 0;

	lr = val & ADP_USB3_CS_4_ALR_MASK;
	ret = lr == ADP_USB3_CS_4_ALR_20G ? 20000 : 10000;

	return usb4_usb3_port_max_bandwidth(port, ret);
}

static int usb4_usb3_port_cm_request(struct tb_port *port, bool request)
{
	int ret;
	u32 val;

	if (!tb_port_is_usb3_down(port))
		return -EINVAL;
	if (tb_route(port->sw))
		return -EINVAL;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_USB3_CS_2, 1);
	if (ret)
		return ret;

	if (request)
		val |= ADP_USB3_CS_2_CMR;
	else
		val &= ~ADP_USB3_CS_2_CMR;

	ret = tb_port_write(port, &val, TB_CFG_PORT,
			    port->cap_adap + ADP_USB3_CS_2, 1);
	if (ret)
		return ret;

	 
	val &= ADP_USB3_CS_2_CMR;
	return usb4_port_wait_for_bit(port, port->cap_adap + ADP_USB3_CS_1,
				      ADP_USB3_CS_1_HCA, val, 1500);
}

static inline int usb4_usb3_port_set_cm_request(struct tb_port *port)
{
	return usb4_usb3_port_cm_request(port, true);
}

static inline int usb4_usb3_port_clear_cm_request(struct tb_port *port)
{
	return usb4_usb3_port_cm_request(port, false);
}

static unsigned int usb3_bw_to_mbps(u32 bw, u8 scale)
{
	unsigned long uframes;

	uframes = bw * 512UL << scale;
	return DIV_ROUND_CLOSEST(uframes * 8000, MEGA);
}

static u32 mbps_to_usb3_bw(unsigned int mbps, u8 scale)
{
	unsigned long uframes;

	 
	uframes = ((unsigned long)mbps * MEGA) / 8000;
	return DIV_ROUND_UP(uframes, 512UL << scale);
}

static int usb4_usb3_port_read_allocated_bandwidth(struct tb_port *port,
						   int *upstream_bw,
						   int *downstream_bw)
{
	u32 val, bw, scale;
	int ret;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_USB3_CS_2, 1);
	if (ret)
		return ret;

	ret = tb_port_read(port, &scale, TB_CFG_PORT,
			   port->cap_adap + ADP_USB3_CS_3, 1);
	if (ret)
		return ret;

	scale &= ADP_USB3_CS_3_SCALE_MASK;

	bw = val & ADP_USB3_CS_2_AUBW_MASK;
	*upstream_bw = usb3_bw_to_mbps(bw, scale);

	bw = (val & ADP_USB3_CS_2_ADBW_MASK) >> ADP_USB3_CS_2_ADBW_SHIFT;
	*downstream_bw = usb3_bw_to_mbps(bw, scale);

	return 0;
}

 
int usb4_usb3_port_allocated_bandwidth(struct tb_port *port, int *upstream_bw,
				       int *downstream_bw)
{
	int ret;

	ret = usb4_usb3_port_set_cm_request(port);
	if (ret)
		return ret;

	ret = usb4_usb3_port_read_allocated_bandwidth(port, upstream_bw,
						      downstream_bw);
	usb4_usb3_port_clear_cm_request(port);

	return ret;
}

static int usb4_usb3_port_read_consumed_bandwidth(struct tb_port *port,
						  int *upstream_bw,
						  int *downstream_bw)
{
	u32 val, bw, scale;
	int ret;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_USB3_CS_1, 1);
	if (ret)
		return ret;

	ret = tb_port_read(port, &scale, TB_CFG_PORT,
			   port->cap_adap + ADP_USB3_CS_3, 1);
	if (ret)
		return ret;

	scale &= ADP_USB3_CS_3_SCALE_MASK;

	bw = val & ADP_USB3_CS_1_CUBW_MASK;
	*upstream_bw = usb3_bw_to_mbps(bw, scale);

	bw = (val & ADP_USB3_CS_1_CDBW_MASK) >> ADP_USB3_CS_1_CDBW_SHIFT;
	*downstream_bw = usb3_bw_to_mbps(bw, scale);

	return 0;
}

static int usb4_usb3_port_write_allocated_bandwidth(struct tb_port *port,
						    int upstream_bw,
						    int downstream_bw)
{
	u32 val, ubw, dbw, scale;
	int ret, max_bw;

	 
	scale = 0;
	max_bw = max(upstream_bw, downstream_bw);
	while (scale < 64) {
		if (mbps_to_usb3_bw(max_bw, scale) < 4096)
			break;
		scale++;
	}

	if (WARN_ON(scale >= 64))
		return -EINVAL;

	ret = tb_port_write(port, &scale, TB_CFG_PORT,
			    port->cap_adap + ADP_USB3_CS_3, 1);
	if (ret)
		return ret;

	ubw = mbps_to_usb3_bw(upstream_bw, scale);
	dbw = mbps_to_usb3_bw(downstream_bw, scale);

	tb_port_dbg(port, "scaled bandwidth %u/%u, scale %u\n", ubw, dbw, scale);

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_USB3_CS_2, 1);
	if (ret)
		return ret;

	val &= ~(ADP_USB3_CS_2_AUBW_MASK | ADP_USB3_CS_2_ADBW_MASK);
	val |= dbw << ADP_USB3_CS_2_ADBW_SHIFT;
	val |= ubw;

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_adap + ADP_USB3_CS_2, 1);
}

 
int usb4_usb3_port_allocate_bandwidth(struct tb_port *port, int *upstream_bw,
				      int *downstream_bw)
{
	int ret, consumed_up, consumed_down, allocate_up, allocate_down;

	ret = usb4_usb3_port_set_cm_request(port);
	if (ret)
		return ret;

	ret = usb4_usb3_port_read_consumed_bandwidth(port, &consumed_up,
						     &consumed_down);
	if (ret)
		goto err_request;

	 
	allocate_up = max(*upstream_bw, consumed_up);
	allocate_down = max(*downstream_bw, consumed_down);

	ret = usb4_usb3_port_write_allocated_bandwidth(port, allocate_up,
						       allocate_down);
	if (ret)
		goto err_request;

	*upstream_bw = allocate_up;
	*downstream_bw = allocate_down;

err_request:
	usb4_usb3_port_clear_cm_request(port);
	return ret;
}

 
int usb4_usb3_port_release_bandwidth(struct tb_port *port, int *upstream_bw,
				     int *downstream_bw)
{
	int ret, consumed_up, consumed_down;

	ret = usb4_usb3_port_set_cm_request(port);
	if (ret)
		return ret;

	ret = usb4_usb3_port_read_consumed_bandwidth(port, &consumed_up,
						     &consumed_down);
	if (ret)
		goto err_request;

	 
	if (consumed_up < 1000)
		consumed_up = 1000;
	if (consumed_down < 1000)
		consumed_down = 1000;

	ret = usb4_usb3_port_write_allocated_bandwidth(port, consumed_up,
						       consumed_down);
	if (ret)
		goto err_request;

	*upstream_bw = consumed_up;
	*downstream_bw = consumed_down;

err_request:
	usb4_usb3_port_clear_cm_request(port);
	return ret;
}

static bool is_usb4_dpin(const struct tb_port *port)
{
	if (!tb_port_is_dpin(port))
		return false;
	if (!tb_switch_is_usb4(port->sw))
		return false;
	return true;
}

 
int usb4_dp_port_set_cm_id(struct tb_port *port, int cm_id)
{
	u32 val;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	val &= ~ADP_DP_CS_2_CM_ID_MASK;
	val |= cm_id << ADP_DP_CS_2_CM_ID_SHIFT;

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_2, 1);
}

 
bool usb4_dp_port_bandwidth_mode_supported(struct tb_port *port)
{
	int ret;
	u32 val;

	if (!is_usb4_dpin(port))
		return false;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + DP_LOCAL_CAP, 1);
	if (ret)
		return false;

	return !!(val & DP_COMMON_CAP_BW_MODE);
}

 
bool usb4_dp_port_bandwidth_mode_enabled(struct tb_port *port)
{
	int ret;
	u32 val;

	if (!is_usb4_dpin(port))
		return false;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_8, 1);
	if (ret)
		return false;

	return !!(val & ADP_DP_CS_8_DPME);
}

 
int usb4_dp_port_set_cm_bandwidth_mode_supported(struct tb_port *port,
						 bool supported)
{
	u32 val;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	if (supported)
		val |= ADP_DP_CS_2_CMMS;
	else
		val &= ~ADP_DP_CS_2_CMMS;

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_2, 1);
}

 
int usb4_dp_port_group_id(struct tb_port *port)
{
	u32 val;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	return (val & ADP_DP_CS_2_GROUP_ID_MASK) >> ADP_DP_CS_2_GROUP_ID_SHIFT;
}

 
int usb4_dp_port_set_group_id(struct tb_port *port, int group_id)
{
	u32 val;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	val &= ~ADP_DP_CS_2_GROUP_ID_MASK;
	val |= group_id << ADP_DP_CS_2_GROUP_ID_SHIFT;

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_2, 1);
}

 
int usb4_dp_port_nrd(struct tb_port *port, int *rate, int *lanes)
{
	u32 val, tmp;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	tmp = (val & ADP_DP_CS_2_NRD_MLR_MASK) >> ADP_DP_CS_2_NRD_MLR_SHIFT;
	switch (tmp) {
	case DP_COMMON_CAP_RATE_RBR:
		*rate = 1620;
		break;
	case DP_COMMON_CAP_RATE_HBR:
		*rate = 2700;
		break;
	case DP_COMMON_CAP_RATE_HBR2:
		*rate = 5400;
		break;
	case DP_COMMON_CAP_RATE_HBR3:
		*rate = 8100;
		break;
	}

	tmp = val & ADP_DP_CS_2_NRD_MLC_MASK;
	switch (tmp) {
	case DP_COMMON_CAP_1_LANE:
		*lanes = 1;
		break;
	case DP_COMMON_CAP_2_LANES:
		*lanes = 2;
		break;
	case DP_COMMON_CAP_4_LANES:
		*lanes = 4;
		break;
	}

	return 0;
}

 
int usb4_dp_port_set_nrd(struct tb_port *port, int rate, int lanes)
{
	u32 val;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	val &= ~ADP_DP_CS_2_NRD_MLR_MASK;

	switch (rate) {
	case 1620:
		break;
	case 2700:
		val |= (DP_COMMON_CAP_RATE_HBR << ADP_DP_CS_2_NRD_MLR_SHIFT)
			& ADP_DP_CS_2_NRD_MLR_MASK;
		break;
	case 5400:
		val |= (DP_COMMON_CAP_RATE_HBR2 << ADP_DP_CS_2_NRD_MLR_SHIFT)
			& ADP_DP_CS_2_NRD_MLR_MASK;
		break;
	case 8100:
		val |= (DP_COMMON_CAP_RATE_HBR3 << ADP_DP_CS_2_NRD_MLR_SHIFT)
			& ADP_DP_CS_2_NRD_MLR_MASK;
		break;
	default:
		return -EINVAL;
	}

	val &= ~ADP_DP_CS_2_NRD_MLC_MASK;

	switch (lanes) {
	case 1:
		break;
	case 2:
		val |= DP_COMMON_CAP_2_LANES;
		break;
	case 4:
		val |= DP_COMMON_CAP_4_LANES;
		break;
	default:
		return -EINVAL;
	}

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_2, 1);
}

 
int usb4_dp_port_granularity(struct tb_port *port)
{
	u32 val;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	val &= ADP_DP_CS_2_GR_MASK;
	val >>= ADP_DP_CS_2_GR_SHIFT;

	switch (val) {
	case ADP_DP_CS_2_GR_0_25G:
		return 250;
	case ADP_DP_CS_2_GR_0_5G:
		return 500;
	case ADP_DP_CS_2_GR_1G:
		return 1000;
	}

	return -EINVAL;
}

 
int usb4_dp_port_set_granularity(struct tb_port *port, int granularity)
{
	u32 val;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	val &= ~ADP_DP_CS_2_GR_MASK;

	switch (granularity) {
	case 250:
		val |= ADP_DP_CS_2_GR_0_25G << ADP_DP_CS_2_GR_SHIFT;
		break;
	case 500:
		val |= ADP_DP_CS_2_GR_0_5G << ADP_DP_CS_2_GR_SHIFT;
		break;
	case 1000:
		val |= ADP_DP_CS_2_GR_1G << ADP_DP_CS_2_GR_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_2, 1);
}

 
int usb4_dp_port_set_estimated_bandwidth(struct tb_port *port, int bw)
{
	u32 val, granularity;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = usb4_dp_port_granularity(port);
	if (ret < 0)
		return ret;
	granularity = ret;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	val &= ~ADP_DP_CS_2_ESTIMATED_BW_MASK;
	val |= (bw / granularity) << ADP_DP_CS_2_ESTIMATED_BW_SHIFT;

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_2, 1);
}

 
int usb4_dp_port_allocated_bandwidth(struct tb_port *port)
{
	u32 val, granularity;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = usb4_dp_port_granularity(port);
	if (ret < 0)
		return ret;
	granularity = ret;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + DP_STATUS, 1);
	if (ret)
		return ret;

	val &= DP_STATUS_ALLOCATED_BW_MASK;
	val >>= DP_STATUS_ALLOCATED_BW_SHIFT;

	return val * granularity;
}

static int __usb4_dp_port_set_cm_ack(struct tb_port *port, bool ack)
{
	u32 val;
	int ret;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	if (ack)
		val |= ADP_DP_CS_2_CA;
	else
		val &= ~ADP_DP_CS_2_CA;

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_2, 1);
}

static inline int usb4_dp_port_set_cm_ack(struct tb_port *port)
{
	return __usb4_dp_port_set_cm_ack(port, true);
}

static int usb4_dp_port_wait_and_clear_cm_ack(struct tb_port *port,
					      int timeout_msec)
{
	ktime_t end;
	u32 val;
	int ret;

	ret = __usb4_dp_port_set_cm_ack(port, false);
	if (ret)
		return ret;

	end = ktime_add_ms(ktime_get(), timeout_msec);
	do {
		ret = tb_port_read(port, &val, TB_CFG_PORT,
				   port->cap_adap + ADP_DP_CS_8, 1);
		if (ret)
			return ret;

		if (!(val & ADP_DP_CS_8_DR))
			break;

		usleep_range(50, 100);
	} while (ktime_before(ktime_get(), end));

	if (val & ADP_DP_CS_8_DR)
		return -ETIMEDOUT;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_2, 1);
	if (ret)
		return ret;

	val &= ~ADP_DP_CS_2_CA;
	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_adap + ADP_DP_CS_2, 1);
}

 
int usb4_dp_port_allocate_bandwidth(struct tb_port *port, int bw)
{
	u32 val, granularity;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = usb4_dp_port_granularity(port);
	if (ret < 0)
		return ret;
	granularity = ret;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + DP_STATUS, 1);
	if (ret)
		return ret;

	val &= ~DP_STATUS_ALLOCATED_BW_MASK;
	val |= (bw / granularity) << DP_STATUS_ALLOCATED_BW_SHIFT;

	ret = tb_port_write(port, &val, TB_CFG_PORT,
			    port->cap_adap + DP_STATUS, 1);
	if (ret)
		return ret;

	ret = usb4_dp_port_set_cm_ack(port);
	if (ret)
		return ret;

	return usb4_dp_port_wait_and_clear_cm_ack(port, 500);
}

 
int usb4_dp_port_requested_bandwidth(struct tb_port *port)
{
	u32 val, granularity;
	int ret;

	if (!is_usb4_dpin(port))
		return -EOPNOTSUPP;

	ret = usb4_dp_port_granularity(port);
	if (ret < 0)
		return ret;
	granularity = ret;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_DP_CS_8, 1);
	if (ret)
		return ret;

	if (!(val & ADP_DP_CS_8_DR))
		return -ENODATA;

	return (val & ADP_DP_CS_8_REQUESTED_BW_MASK) * granularity;
}

 
int usb4_pci_port_set_ext_encapsulation(struct tb_port *port, bool enable)
{
	u32 val;
	int ret;

	if (!tb_port_is_pcie_up(port) && !tb_port_is_pcie_down(port))
		return -EINVAL;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_adap + ADP_PCIE_CS_1, 1);
	if (ret)
		return ret;

	if (enable)
		val |= ADP_PCIE_CS_1_EE;
	else
		val &= ~ADP_PCIE_CS_1_EE;

	return tb_port_write(port, &val, TB_CFG_PORT,
			     port->cap_adap + ADP_PCIE_CS_1, 1);
}
