
 

#include "tb.h"

 
int tb_lc_read_uuid(struct tb_switch *sw, u32 *uuid)
{
	if (!sw->cap_lc)
		return -EINVAL;
	return tb_sw_read(sw, uuid, TB_CFG_SWITCH, sw->cap_lc + TB_LC_FUSE, 4);
}

static int read_lc_desc(struct tb_switch *sw, u32 *desc)
{
	if (!sw->cap_lc)
		return -EINVAL;
	return tb_sw_read(sw, desc, TB_CFG_SWITCH, sw->cap_lc + TB_LC_DESC, 1);
}

static int find_port_lc_cap(struct tb_port *port)
{
	struct tb_switch *sw = port->sw;
	int start, phys, ret, size;
	u32 desc;

	ret = read_lc_desc(sw, &desc);
	if (ret)
		return ret;

	 
	start = (desc & TB_LC_DESC_SIZE_MASK) >> TB_LC_DESC_SIZE_SHIFT;
	size = (desc & TB_LC_DESC_PORT_SIZE_MASK) >> TB_LC_DESC_PORT_SIZE_SHIFT;
	phys = tb_phy_port_from_link(port->port);

	return sw->cap_lc + start + phys * size;
}

static int tb_lc_set_port_configured(struct tb_port *port, bool configured)
{
	bool upstream = tb_is_upstream_port(port);
	struct tb_switch *sw = port->sw;
	u32 ctrl, lane;
	int cap, ret;

	if (sw->generation < 2)
		return 0;

	cap = find_port_lc_cap(port);
	if (cap < 0)
		return cap;

	ret = tb_sw_read(sw, &ctrl, TB_CFG_SWITCH, cap + TB_LC_SX_CTRL, 1);
	if (ret)
		return ret;

	 
	if (port->port % 2)
		lane = TB_LC_SX_CTRL_L1C;
	else
		lane = TB_LC_SX_CTRL_L2C;

	if (configured) {
		ctrl |= lane;
		if (upstream)
			ctrl |= TB_LC_SX_CTRL_UPSTREAM;
	} else {
		ctrl &= ~lane;
		if (upstream)
			ctrl &= ~TB_LC_SX_CTRL_UPSTREAM;
	}

	return tb_sw_write(sw, &ctrl, TB_CFG_SWITCH, cap + TB_LC_SX_CTRL, 1);
}

 
int tb_lc_configure_port(struct tb_port *port)
{
	return tb_lc_set_port_configured(port, true);
}

 
void tb_lc_unconfigure_port(struct tb_port *port)
{
	tb_lc_set_port_configured(port, false);
}

static int tb_lc_set_xdomain_configured(struct tb_port *port, bool configure)
{
	struct tb_switch *sw = port->sw;
	u32 ctrl, lane;
	int cap, ret;

	if (sw->generation < 2)
		return 0;

	cap = find_port_lc_cap(port);
	if (cap < 0)
		return cap;

	ret = tb_sw_read(sw, &ctrl, TB_CFG_SWITCH, cap + TB_LC_SX_CTRL, 1);
	if (ret)
		return ret;

	 
	if (port->port % 2)
		lane = TB_LC_SX_CTRL_L1D;
	else
		lane = TB_LC_SX_CTRL_L2D;

	if (configure)
		ctrl |= lane;
	else
		ctrl &= ~lane;

	return tb_sw_write(sw, &ctrl, TB_CFG_SWITCH, cap + TB_LC_SX_CTRL, 1);
}

 
int tb_lc_configure_xdomain(struct tb_port *port)
{
	return tb_lc_set_xdomain_configured(port, true);
}

 
void tb_lc_unconfigure_xdomain(struct tb_port *port)
{
	tb_lc_set_xdomain_configured(port, false);
}

 
int tb_lc_start_lane_initialization(struct tb_port *port)
{
	struct tb_switch *sw = port->sw;
	int ret, cap;
	u32 ctrl;

	if (!tb_route(sw))
		return 0;

	if (sw->generation < 2)
		return 0;

	cap = find_port_lc_cap(port);
	if (cap < 0)
		return cap;

	ret = tb_sw_read(sw, &ctrl, TB_CFG_SWITCH, cap + TB_LC_SX_CTRL, 1);
	if (ret)
		return ret;

	ctrl |= TB_LC_SX_CTRL_SLI;

	return tb_sw_write(sw, &ctrl, TB_CFG_SWITCH, cap + TB_LC_SX_CTRL, 1);
}

 
bool tb_lc_is_clx_supported(struct tb_port *port)
{
	struct tb_switch *sw = port->sw;
	int cap, ret;
	u32 val;

	cap = find_port_lc_cap(port);
	if (cap < 0)
		return false;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, cap + TB_LC_LINK_ATTR, 1);
	if (ret)
		return false;

	return !!(val & TB_LC_LINK_ATTR_CPS);
}

 
bool tb_lc_is_usb_plugged(struct tb_port *port)
{
	struct tb_switch *sw = port->sw;
	int cap, ret;
	u32 val;

	if (sw->generation != 3)
		return false;

	cap = find_port_lc_cap(port);
	if (cap < 0)
		return false;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, cap + TB_LC_CS_42, 1);
	if (ret)
		return false;

	return !!(val & TB_LC_CS_42_USB_PLUGGED);
}

 
bool tb_lc_is_xhci_connected(struct tb_port *port)
{
	struct tb_switch *sw = port->sw;
	int cap, ret;
	u32 val;

	if (sw->generation != 3)
		return false;

	cap = find_port_lc_cap(port);
	if (cap < 0)
		return false;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, cap + TB_LC_LINK_REQ, 1);
	if (ret)
		return false;

	return !!(val & TB_LC_LINK_REQ_XHCI_CONNECT);
}

static int __tb_lc_xhci_connect(struct tb_port *port, bool connect)
{
	struct tb_switch *sw = port->sw;
	int cap, ret;
	u32 val;

	if (sw->generation != 3)
		return -EINVAL;

	cap = find_port_lc_cap(port);
	if (cap < 0)
		return cap;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, cap + TB_LC_LINK_REQ, 1);
	if (ret)
		return ret;

	if (connect)
		val |= TB_LC_LINK_REQ_XHCI_CONNECT;
	else
		val &= ~TB_LC_LINK_REQ_XHCI_CONNECT;

	return tb_sw_write(sw, &val, TB_CFG_SWITCH, cap + TB_LC_LINK_REQ, 1);
}

 
int tb_lc_xhci_connect(struct tb_port *port)
{
	int ret;

	ret = __tb_lc_xhci_connect(port, true);
	if (ret)
		return ret;

	tb_port_dbg(port, "xHCI connected\n");
	return 0;
}

 
void tb_lc_xhci_disconnect(struct tb_port *port)
{
	__tb_lc_xhci_connect(port, false);
	tb_port_dbg(port, "xHCI disconnected\n");
}

static int tb_lc_set_wake_one(struct tb_switch *sw, unsigned int offset,
			      unsigned int flags)
{
	u32 ctrl;
	int ret;

	 
	ret = tb_sw_read(sw, &ctrl, TB_CFG_SWITCH,
			 offset + TB_LC_SX_CTRL, 1);
	if (ret)
		return ret;

	ctrl &= ~(TB_LC_SX_CTRL_WOC | TB_LC_SX_CTRL_WOD | TB_LC_SX_CTRL_WODPC |
		  TB_LC_SX_CTRL_WODPD | TB_LC_SX_CTRL_WOP | TB_LC_SX_CTRL_WOU4);

	if (flags & TB_WAKE_ON_CONNECT)
		ctrl |= TB_LC_SX_CTRL_WOC | TB_LC_SX_CTRL_WOD;
	if (flags & TB_WAKE_ON_USB4)
		ctrl |= TB_LC_SX_CTRL_WOU4;
	if (flags & TB_WAKE_ON_PCIE)
		ctrl |= TB_LC_SX_CTRL_WOP;
	if (flags & TB_WAKE_ON_DP)
		ctrl |= TB_LC_SX_CTRL_WODPC | TB_LC_SX_CTRL_WODPD;

	return tb_sw_write(sw, &ctrl, TB_CFG_SWITCH, offset + TB_LC_SX_CTRL, 1);
}

 
int tb_lc_set_wake(struct tb_switch *sw, unsigned int flags)
{
	int start, size, nlc, ret, i;
	u32 desc;

	if (sw->generation < 2)
		return 0;

	if (!tb_route(sw))
		return 0;

	ret = read_lc_desc(sw, &desc);
	if (ret)
		return ret;

	 
	nlc = desc & TB_LC_DESC_NLC_MASK;
	start = (desc & TB_LC_DESC_SIZE_MASK) >> TB_LC_DESC_SIZE_SHIFT;
	size = (desc & TB_LC_DESC_PORT_SIZE_MASK) >> TB_LC_DESC_PORT_SIZE_SHIFT;

	 
	for (i = 0; i < nlc; i++) {
		unsigned int offset = sw->cap_lc + start + i * size;

		ret = tb_lc_set_wake_one(sw, offset, flags);
		if (ret)
			return ret;
	}

	return 0;
}

 
int tb_lc_set_sleep(struct tb_switch *sw)
{
	int start, size, nlc, ret, i;
	u32 desc;

	if (sw->generation < 2)
		return 0;

	ret = read_lc_desc(sw, &desc);
	if (ret)
		return ret;

	 
	nlc = desc & TB_LC_DESC_NLC_MASK;
	start = (desc & TB_LC_DESC_SIZE_MASK) >> TB_LC_DESC_SIZE_SHIFT;
	size = (desc & TB_LC_DESC_PORT_SIZE_MASK) >> TB_LC_DESC_PORT_SIZE_SHIFT;

	 
	for (i = 0; i < nlc; i++) {
		unsigned int offset = sw->cap_lc + start + i * size;
		u32 ctrl;

		ret = tb_sw_read(sw, &ctrl, TB_CFG_SWITCH,
				 offset + TB_LC_SX_CTRL, 1);
		if (ret)
			return ret;

		ctrl |= TB_LC_SX_CTRL_SLP;
		ret = tb_sw_write(sw, &ctrl, TB_CFG_SWITCH,
				  offset + TB_LC_SX_CTRL, 1);
		if (ret)
			return ret;
	}

	return 0;
}

 
bool tb_lc_lane_bonding_possible(struct tb_switch *sw)
{
	struct tb_port *up;
	int cap, ret;
	u32 val;

	if (sw->generation < 2)
		return false;

	up = tb_upstream_port(sw);
	cap = find_port_lc_cap(up);
	if (cap < 0)
		return false;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH, cap + TB_LC_PORT_ATTR, 1);
	if (ret)
		return false;

	return !!(val & TB_LC_PORT_ATTR_BE);
}

static int tb_lc_dp_sink_from_port(const struct tb_switch *sw,
				   struct tb_port *in)
{
	struct tb_port *port;

	 
	tb_switch_for_each_port(sw, port) {
		if (tb_port_is_dpin(port))
			return in != port;
	}

	return -EINVAL;
}

static int tb_lc_dp_sink_available(struct tb_switch *sw, int sink)
{
	u32 val, alloc;
	int ret;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH,
			 sw->cap_lc + TB_LC_SNK_ALLOCATION, 1);
	if (ret)
		return ret;

	 
	if (!sink) {
		alloc = val & TB_LC_SNK_ALLOCATION_SNK0_MASK;
		if (!alloc || alloc == TB_LC_SNK_ALLOCATION_SNK0_CM)
			return 0;
	} else {
		alloc = (val & TB_LC_SNK_ALLOCATION_SNK1_MASK) >>
			TB_LC_SNK_ALLOCATION_SNK1_SHIFT;
		if (!alloc || alloc == TB_LC_SNK_ALLOCATION_SNK1_CM)
			return 0;
	}

	return -EBUSY;
}

 
bool tb_lc_dp_sink_query(struct tb_switch *sw, struct tb_port *in)
{
	int sink;

	 
	if (sw->generation < 3)
		return true;

	sink = tb_lc_dp_sink_from_port(sw, in);
	if (sink < 0)
		return false;

	return !tb_lc_dp_sink_available(sw, sink);
}

 
int tb_lc_dp_sink_alloc(struct tb_switch *sw, struct tb_port *in)
{
	int ret, sink;
	u32 val;

	if (sw->generation < 3)
		return 0;

	sink = tb_lc_dp_sink_from_port(sw, in);
	if (sink < 0)
		return sink;

	ret = tb_lc_dp_sink_available(sw, sink);
	if (ret)
		return ret;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH,
			 sw->cap_lc + TB_LC_SNK_ALLOCATION, 1);
	if (ret)
		return ret;

	if (!sink) {
		val &= ~TB_LC_SNK_ALLOCATION_SNK0_MASK;
		val |= TB_LC_SNK_ALLOCATION_SNK0_CM;
	} else {
		val &= ~TB_LC_SNK_ALLOCATION_SNK1_MASK;
		val |= TB_LC_SNK_ALLOCATION_SNK1_CM <<
			TB_LC_SNK_ALLOCATION_SNK1_SHIFT;
	}

	ret = tb_sw_write(sw, &val, TB_CFG_SWITCH,
			  sw->cap_lc + TB_LC_SNK_ALLOCATION, 1);

	if (ret)
		return ret;

	tb_port_dbg(in, "sink %d allocated\n", sink);
	return 0;
}

 
int tb_lc_dp_sink_dealloc(struct tb_switch *sw, struct tb_port *in)
{
	int ret, sink;
	u32 val;

	if (sw->generation < 3)
		return 0;

	sink = tb_lc_dp_sink_from_port(sw, in);
	if (sink < 0)
		return sink;

	 
	ret = tb_lc_dp_sink_available(sw, sink);
	if (ret)
		return ret;

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH,
			 sw->cap_lc + TB_LC_SNK_ALLOCATION, 1);
	if (ret)
		return ret;

	if (!sink)
		val &= ~TB_LC_SNK_ALLOCATION_SNK0_MASK;
	else
		val &= ~TB_LC_SNK_ALLOCATION_SNK1_MASK;

	ret = tb_sw_write(sw, &val, TB_CFG_SWITCH,
			  sw->cap_lc + TB_LC_SNK_ALLOCATION, 1);
	if (ret)
		return ret;

	tb_port_dbg(in, "sink %d de-allocated\n", sink);
	return 0;
}

 
int tb_lc_force_power(struct tb_switch *sw)
{
	u32 in = 0xffff;

	return tb_sw_write(sw, &in, TB_CFG_SWITCH, TB_LC_POWER, 1);
}
