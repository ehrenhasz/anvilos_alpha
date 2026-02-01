
 

#include <linux/module.h>

#include "tb.h"

static bool clx_enabled = true;
module_param_named(clx, clx_enabled, bool, 0444);
MODULE_PARM_DESC(clx, "allow low power states on the high-speed lanes (default: true)");

static const char *clx_name(unsigned int clx)
{
	switch (clx) {
	case TB_CL0S | TB_CL1 | TB_CL2:
		return "CL0s/CL1/CL2";
	case TB_CL1 | TB_CL2:
		return "CL1/CL2";
	case TB_CL0S | TB_CL2:
		return "CL0s/CL2";
	case TB_CL0S | TB_CL1:
		return "CL0s/CL1";
	case TB_CL0S:
		return "CL0s";
	case 0:
		return "disabled";
	default:
		return "unknown";
	}
}

static int tb_port_pm_secondary_set(struct tb_port *port, bool secondary)
{
	u32 phy;
	int ret;

	ret = tb_port_read(port, &phy, TB_CFG_PORT,
			   port->cap_phy + LANE_ADP_CS_1, 1);
	if (ret)
		return ret;

	if (secondary)
		phy |= LANE_ADP_CS_1_PMS;
	else
		phy &= ~LANE_ADP_CS_1_PMS;

	return tb_port_write(port, &phy, TB_CFG_PORT,
			     port->cap_phy + LANE_ADP_CS_1, 1);
}

static int tb_port_pm_secondary_enable(struct tb_port *port)
{
	return tb_port_pm_secondary_set(port, true);
}

static int tb_port_pm_secondary_disable(struct tb_port *port)
{
	return tb_port_pm_secondary_set(port, false);
}

 
static bool tb_port_clx_supported(struct tb_port *port, unsigned int clx)
{
	u32 val, mask = 0;
	bool ret;

	 
	if (!port->bonded && port->dual_link_port)
		return false;

	 
	if (port->xdomain)
		return false;

	if (tb_switch_is_usb4(port->sw)) {
		if (!usb4_port_clx_supported(port))
			return false;
	} else if (!tb_lc_is_clx_supported(port)) {
		return false;
	}

	if (clx & TB_CL0S)
		mask |= LANE_ADP_CS_0_CL0S_SUPPORT;
	if (clx & TB_CL1)
		mask |= LANE_ADP_CS_0_CL1_SUPPORT;
	if (clx & TB_CL2)
		mask |= LANE_ADP_CS_0_CL2_SUPPORT;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_phy + LANE_ADP_CS_0, 1);
	if (ret)
		return false;

	return !!(val & mask);
}

static int tb_port_clx_set(struct tb_port *port, unsigned int clx, bool enable)
{
	u32 phy, mask = 0;
	int ret;

	if (clx & TB_CL0S)
		mask |= LANE_ADP_CS_1_CL0S_ENABLE;
	if (clx & TB_CL1)
		mask |= LANE_ADP_CS_1_CL1_ENABLE;
	if (clx & TB_CL2)
		mask |= LANE_ADP_CS_1_CL2_ENABLE;

	if (!mask)
		return -EOPNOTSUPP;

	ret = tb_port_read(port, &phy, TB_CFG_PORT,
			   port->cap_phy + LANE_ADP_CS_1, 1);
	if (ret)
		return ret;

	if (enable)
		phy |= mask;
	else
		phy &= ~mask;

	return tb_port_write(port, &phy, TB_CFG_PORT,
			     port->cap_phy + LANE_ADP_CS_1, 1);
}

static int tb_port_clx_disable(struct tb_port *port, unsigned int clx)
{
	return tb_port_clx_set(port, clx, false);
}

static int tb_port_clx_enable(struct tb_port *port, unsigned int clx)
{
	return tb_port_clx_set(port, clx, true);
}

static int tb_port_clx(struct tb_port *port)
{
	u32 val;
	int ret;

	if (!tb_port_clx_supported(port, TB_CL0S | TB_CL1 | TB_CL2))
		return 0;

	ret = tb_port_read(port, &val, TB_CFG_PORT,
			   port->cap_phy + LANE_ADP_CS_1, 1);
	if (ret)
		return ret;

	if (val & LANE_ADP_CS_1_CL0S_ENABLE)
		ret |= TB_CL0S;
	if (val & LANE_ADP_CS_1_CL1_ENABLE)
		ret |= TB_CL1;
	if (val & LANE_ADP_CS_1_CL2_ENABLE)
		ret |= TB_CL2;

	return ret;
}

 
bool tb_port_clx_is_enabled(struct tb_port *port, unsigned int clx)
{
	return !!(tb_port_clx(port) & clx);
}

 
int tb_switch_clx_init(struct tb_switch *sw)
{
	struct tb_port *up, *down;
	unsigned int clx, tmp;

	if (tb_switch_is_icm(sw))
		return 0;

	if (!tb_route(sw))
		return 0;

	if (!tb_switch_clx_is_supported(sw))
		return 0;

	up = tb_upstream_port(sw);
	down = tb_switch_downstream_port(sw);

	clx = tb_port_clx(up);
	tmp = tb_port_clx(down);
	if (clx != tmp)
		tb_sw_warn(sw, "CLx: inconsistent configuration %#x != %#x\n",
			   clx, tmp);

	tb_sw_dbg(sw, "CLx: current mode: %s\n", clx_name(clx));

	sw->clx = clx;
	return 0;
}

static int tb_switch_pm_secondary_resolve(struct tb_switch *sw)
{
	struct tb_port *up, *down;
	int ret;

	if (!tb_route(sw))
		return 0;

	up = tb_upstream_port(sw);
	down = tb_switch_downstream_port(sw);
	ret = tb_port_pm_secondary_enable(up);
	if (ret)
		return ret;

	return tb_port_pm_secondary_disable(down);
}

static int tb_switch_mask_clx_objections(struct tb_switch *sw)
{
	int up_port = sw->config.upstream_port_number;
	u32 offset, val[2], mask_obj, unmask_obj;
	int ret, i;

	 
	if (!tb_switch_is_titan_ridge(sw))
		return 0;

	if (!tb_route(sw))
		return 0;

	 
	if (up_port == 1) {
		mask_obj = TB_LOW_PWR_C0_PORT_B_MASK;
		unmask_obj = TB_LOW_PWR_C1_PORT_A_MASK;
		offset = TB_LOW_PWR_C1_CL1;
	} else {
		mask_obj = TB_LOW_PWR_C1_PORT_A_MASK;
		unmask_obj = TB_LOW_PWR_C0_PORT_B_MASK;
		offset = TB_LOW_PWR_C3_CL1;
	}

	ret = tb_sw_read(sw, &val, TB_CFG_SWITCH,
			 sw->cap_lp + offset, ARRAY_SIZE(val));
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(val); i++) {
		val[i] |= mask_obj;
		val[i] &= ~unmask_obj;
	}

	return tb_sw_write(sw, &val, TB_CFG_SWITCH,
			   sw->cap_lp + offset, ARRAY_SIZE(val));
}

 
bool tb_switch_clx_is_supported(const struct tb_switch *sw)
{
	if (!clx_enabled)
		return false;

	if (sw->quirks & QUIRK_NO_CLX)
		return false;

	 
	if (tb_switch_is_tiger_lake(sw))
		return false;

	return tb_switch_is_usb4(sw) || tb_switch_is_titan_ridge(sw);
}

static bool validate_mask(unsigned int clx)
{
	 
	if (clx & TB_CL1)
		return (clx & TB_CL0S) == TB_CL0S;
	return true;
}

 
int tb_switch_clx_enable(struct tb_switch *sw, unsigned int clx)
{
	bool up_clx_support, down_clx_support;
	struct tb_switch *parent_sw;
	struct tb_port *up, *down;
	int ret;

	if (!clx || sw->clx == clx)
		return 0;

	if (!validate_mask(clx))
		return -EINVAL;

	parent_sw = tb_switch_parent(sw);
	if (!parent_sw)
		return 0;

	if (!tb_switch_clx_is_supported(parent_sw) ||
	    !tb_switch_clx_is_supported(sw))
		return 0;

	 
	if ((clx & TB_CL2) &&
	    (usb4_switch_version(parent_sw) < 2 ||
	     usb4_switch_version(sw) < 2))
		return -EOPNOTSUPP;

	ret = tb_switch_pm_secondary_resolve(sw);
	if (ret)
		return ret;

	up = tb_upstream_port(sw);
	down = tb_switch_downstream_port(sw);

	up_clx_support = tb_port_clx_supported(up, clx);
	down_clx_support = tb_port_clx_supported(down, clx);

	tb_port_dbg(up, "CLx: %s %ssupported\n", clx_name(clx),
		    up_clx_support ? "" : "not ");
	tb_port_dbg(down, "CLx: %s %ssupported\n", clx_name(clx),
		    down_clx_support ? "" : "not ");

	if (!up_clx_support || !down_clx_support)
		return -EOPNOTSUPP;

	ret = tb_port_clx_enable(up, clx);
	if (ret)
		return ret;

	ret = tb_port_clx_enable(down, clx);
	if (ret) {
		tb_port_clx_disable(up, clx);
		return ret;
	}

	ret = tb_switch_mask_clx_objections(sw);
	if (ret) {
		tb_port_clx_disable(up, clx);
		tb_port_clx_disable(down, clx);
		return ret;
	}

	sw->clx |= clx;

	tb_sw_dbg(sw, "CLx: %s enabled\n", clx_name(clx));
	return 0;
}

 
int tb_switch_clx_disable(struct tb_switch *sw)
{
	unsigned int clx = sw->clx;
	struct tb_port *up, *down;
	int ret;

	if (!tb_switch_clx_is_supported(sw))
		return 0;

	if (!clx)
		return 0;

	up = tb_upstream_port(sw);
	down = tb_switch_downstream_port(sw);

	ret = tb_port_clx_disable(up, clx);
	if (ret)
		return ret;

	ret = tb_port_clx_disable(down, clx);
	if (ret)
		return ret;

	sw->clx = 0;

	tb_sw_dbg(sw, "CLx: %s disabled\n", clx_name(clx));
	return clx;
}
