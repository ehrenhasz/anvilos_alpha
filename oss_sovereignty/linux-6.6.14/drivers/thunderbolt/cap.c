
 

#include <linux/slab.h>
#include <linux/errno.h>

#include "tb.h"

#define CAP_OFFSET_MAX		0xff
#define VSE_CAP_OFFSET_MAX	0xffff
#define TMU_ACCESS_EN		BIT(20)

static int tb_port_enable_tmu(struct tb_port *port, bool enable)
{
	struct tb_switch *sw = port->sw;
	u32 value, offset;
	int ret;

	 
	if (tb_switch_is_light_ridge(sw))
		offset = 0x26;
	else if (tb_switch_is_eagle_ridge(sw))
		offset = 0x2a;
	else
		return 0;

	ret = tb_sw_read(sw, &value, TB_CFG_SWITCH, offset, 1);
	if (ret)
		return ret;

	if (enable)
		value |= TMU_ACCESS_EN;
	else
		value &= ~TMU_ACCESS_EN;

	return tb_sw_write(sw, &value, TB_CFG_SWITCH, offset, 1);
}

static void tb_port_dummy_read(struct tb_port *port)
{
	 
	if (tb_switch_is_light_ridge(port->sw)) {
		u32 dummy;

		tb_port_read(port, &dummy, TB_CFG_PORT, 0, 1);
	}
}

 
int tb_port_next_cap(struct tb_port *port, unsigned int offset)
{
	struct tb_cap_any header;
	int ret;

	if (!offset)
		return port->config.first_cap_offset;

	ret = tb_port_read(port, &header, TB_CFG_PORT, offset, 1);
	if (ret)
		return ret;

	return header.basic.next;
}

static int __tb_port_find_cap(struct tb_port *port, enum tb_port_cap cap)
{
	int offset = 0;

	do {
		struct tb_cap_any header;
		int ret;

		offset = tb_port_next_cap(port, offset);
		if (offset < 0)
			return offset;

		ret = tb_port_read(port, &header, TB_CFG_PORT, offset, 1);
		if (ret)
			return ret;

		if (header.basic.cap == cap)
			return offset;
	} while (offset > 0);

	return -ENOENT;
}

 
int tb_port_find_cap(struct tb_port *port, enum tb_port_cap cap)
{
	int ret;

	ret = tb_port_enable_tmu(port, true);
	if (ret)
		return ret;

	ret = __tb_port_find_cap(port, cap);

	tb_port_dummy_read(port);
	tb_port_enable_tmu(port, false);

	return ret;
}

 
int tb_switch_next_cap(struct tb_switch *sw, unsigned int offset)
{
	struct tb_cap_any header;
	int ret;

	if (!offset)
		return sw->config.first_cap_offset;

	ret = tb_sw_read(sw, &header, TB_CFG_SWITCH, offset, 2);
	if (ret)
		return ret;

	switch (header.basic.cap) {
	case TB_SWITCH_CAP_TMU:
		ret = header.basic.next;
		break;

	case TB_SWITCH_CAP_VSE:
		if (!header.extended_short.length)
			ret = header.extended_long.next;
		else
			ret = header.extended_short.next;
		break;

	default:
		tb_sw_dbg(sw, "unknown capability %#x at %#x\n",
			  header.basic.cap, offset);
		ret = -EINVAL;
		break;
	}

	return ret >= VSE_CAP_OFFSET_MAX ? 0 : ret;
}

 
int tb_switch_find_cap(struct tb_switch *sw, enum tb_switch_cap cap)
{
	int offset = 0;

	do {
		struct tb_cap_any header;
		int ret;

		offset = tb_switch_next_cap(sw, offset);
		if (offset < 0)
			return offset;

		ret = tb_sw_read(sw, &header, TB_CFG_SWITCH, offset, 1);
		if (ret)
			return ret;

		if (header.basic.cap == cap)
			return offset;
	} while (offset);

	return -ENOENT;
}

 
int tb_switch_find_vse_cap(struct tb_switch *sw, enum tb_switch_vse_cap vsec)
{
	int offset = 0;

	do {
		struct tb_cap_any header;
		int ret;

		offset = tb_switch_next_cap(sw, offset);
		if (offset < 0)
			return offset;

		ret = tb_sw_read(sw, &header, TB_CFG_SWITCH, offset, 1);
		if (ret)
			return ret;

		if (header.extended_short.cap == TB_SWITCH_CAP_VSE &&
		    header.extended_short.vsec_id == vsec)
			return offset;
	} while (offset);

	return -ENOENT;
}
