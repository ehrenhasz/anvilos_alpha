 

#include "display/intel_dp_aux_regs.h"
#include "display/intel_gmbus_regs.h"
#include "gvt.h"
#include "i915_drv.h"
#include "i915_reg.h"

#define GMBUS1_TOTAL_BYTES_SHIFT 16
#define GMBUS1_TOTAL_BYTES_MASK 0x1ff
#define gmbus1_total_byte_count(v) (((v) >> \
	GMBUS1_TOTAL_BYTES_SHIFT) & GMBUS1_TOTAL_BYTES_MASK)
#define gmbus1_slave_addr(v) (((v) & 0xff) >> 1)
#define gmbus1_slave_index(v) (((v) >> 8) & 0xff)
#define gmbus1_bus_cycle(v) (((v) >> 25) & 0x7)

 
#define _GMBUS_PIN_SEL_MASK     (0x7)

static unsigned char edid_get_byte(struct intel_vgpu *vgpu)
{
	struct intel_vgpu_i2c_edid *edid = &vgpu->display.i2c_edid;
	unsigned char chr = 0;

	if (edid->state == I2C_NOT_SPECIFIED || !edid->slave_selected) {
		gvt_vgpu_err("Driver tries to read EDID without proper sequence!\n");
		return 0;
	}
	if (edid->current_edid_read >= EDID_SIZE) {
		gvt_vgpu_err("edid_get_byte() exceeds the size of EDID!\n");
		return 0;
	}

	if (!edid->edid_available) {
		gvt_vgpu_err("Reading EDID but EDID is not available!\n");
		return 0;
	}

	if (intel_vgpu_has_monitor_on_port(vgpu, edid->port)) {
		struct intel_vgpu_edid_data *edid_data =
			intel_vgpu_port(vgpu, edid->port)->edid;

		chr = edid_data->edid_block[edid->current_edid_read];
		edid->current_edid_read++;
	} else {
		gvt_vgpu_err("No EDID available during the reading?\n");
	}
	return chr;
}

static inline int cnp_get_port_from_gmbus0(u32 gmbus0)
{
	int port_select = gmbus0 & _GMBUS_PIN_SEL_MASK;
	int port = -EINVAL;

	if (port_select == GMBUS_PIN_1_BXT)
		port = PORT_B;
	else if (port_select == GMBUS_PIN_2_BXT)
		port = PORT_C;
	else if (port_select == GMBUS_PIN_3_BXT)
		port = PORT_D;
	else if (port_select == GMBUS_PIN_4_CNP)
		port = PORT_E;
	return port;
}

static inline int bxt_get_port_from_gmbus0(u32 gmbus0)
{
	int port_select = gmbus0 & _GMBUS_PIN_SEL_MASK;
	int port = -EINVAL;

	if (port_select == GMBUS_PIN_1_BXT)
		port = PORT_B;
	else if (port_select == GMBUS_PIN_2_BXT)
		port = PORT_C;
	else if (port_select == GMBUS_PIN_3_BXT)
		port = PORT_D;
	return port;
}

static inline int get_port_from_gmbus0(u32 gmbus0)
{
	int port_select = gmbus0 & _GMBUS_PIN_SEL_MASK;
	int port = -EINVAL;

	if (port_select == GMBUS_PIN_VGADDC)
		port = PORT_E;
	else if (port_select == GMBUS_PIN_DPC)
		port = PORT_C;
	else if (port_select == GMBUS_PIN_DPB)
		port = PORT_B;
	else if (port_select == GMBUS_PIN_DPD)
		port = PORT_D;
	return port;
}

static void reset_gmbus_controller(struct intel_vgpu *vgpu)
{
	vgpu_vreg_t(vgpu, PCH_GMBUS2) = GMBUS_HW_RDY;
	if (!vgpu->display.i2c_edid.edid_available)
		vgpu_vreg_t(vgpu, PCH_GMBUS2) |= GMBUS_SATOER;
	vgpu->display.i2c_edid.gmbus.phase = GMBUS_IDLE_PHASE;
}

 
static int gmbus0_mmio_write(struct intel_vgpu *vgpu,
			unsigned int offset, void *p_data, unsigned int bytes)
{
	struct drm_i915_private *i915 = vgpu->gvt->gt->i915;
	int port, pin_select;

	memcpy(&vgpu_vreg(vgpu, offset), p_data, bytes);

	pin_select = vgpu_vreg(vgpu, offset) & _GMBUS_PIN_SEL_MASK;

	intel_vgpu_init_i2c_edid(vgpu);

	if (pin_select == 0)
		return 0;

	if (IS_BROXTON(i915))
		port = bxt_get_port_from_gmbus0(pin_select);
	else if (IS_COFFEELAKE(i915) || IS_COMETLAKE(i915))
		port = cnp_get_port_from_gmbus0(pin_select);
	else
		port = get_port_from_gmbus0(pin_select);
	if (drm_WARN_ON(&i915->drm, port < 0))
		return 0;

	vgpu->display.i2c_edid.state = I2C_GMBUS;
	vgpu->display.i2c_edid.gmbus.phase = GMBUS_IDLE_PHASE;

	vgpu_vreg_t(vgpu, PCH_GMBUS2) &= ~GMBUS_ACTIVE;
	vgpu_vreg_t(vgpu, PCH_GMBUS2) |= GMBUS_HW_RDY | GMBUS_HW_WAIT_PHASE;

	if (intel_vgpu_has_monitor_on_port(vgpu, port) &&
			!intel_vgpu_port_is_dp(vgpu, port)) {
		vgpu->display.i2c_edid.port = port;
		vgpu->display.i2c_edid.edid_available = true;
		vgpu_vreg_t(vgpu, PCH_GMBUS2) &= ~GMBUS_SATOER;
	} else
		vgpu_vreg_t(vgpu, PCH_GMBUS2) |= GMBUS_SATOER;
	return 0;
}

static int gmbus1_mmio_write(struct intel_vgpu *vgpu, unsigned int offset,
		void *p_data, unsigned int bytes)
{
	struct intel_vgpu_i2c_edid *i2c_edid = &vgpu->display.i2c_edid;
	u32 slave_addr;
	u32 wvalue = *(u32 *)p_data;

	if (vgpu_vreg(vgpu, offset) & GMBUS_SW_CLR_INT) {
		if (!(wvalue & GMBUS_SW_CLR_INT)) {
			vgpu_vreg(vgpu, offset) &= ~GMBUS_SW_CLR_INT;
			reset_gmbus_controller(vgpu);
		}
		 
	} else {
		 
		if (wvalue & GMBUS_SW_CLR_INT) {
			vgpu_vreg_t(vgpu, PCH_GMBUS2) &= ~GMBUS_INT;
			vgpu_vreg_t(vgpu, PCH_GMBUS2) |= GMBUS_HW_RDY;
		}

		 
		if (wvalue & GMBUS_SW_RDY)
			wvalue &= ~GMBUS_SW_RDY;

		i2c_edid->gmbus.total_byte_count =
			gmbus1_total_byte_count(wvalue);
		slave_addr = gmbus1_slave_addr(wvalue);

		 
		if (slave_addr == EDID_ADDR) {
			i2c_edid->slave_selected = true;
		} else if (slave_addr != 0) {
			gvt_dbg_dpy(
				"vgpu%d: unsupported gmbus slave addr(0x%x)\n"
				"	gmbus operations will be ignored.\n",
					vgpu->id, slave_addr);
		}

		if (wvalue & GMBUS_CYCLE_INDEX)
			i2c_edid->current_edid_read =
				gmbus1_slave_index(wvalue);

		i2c_edid->gmbus.cycle_type = gmbus1_bus_cycle(wvalue);
		switch (gmbus1_bus_cycle(wvalue)) {
		case GMBUS_NOCYCLE:
			break;
		case GMBUS_STOP:
			 
			if (gmbus1_bus_cycle(vgpu_vreg(vgpu, offset))
				!= GMBUS_NOCYCLE) {
				intel_vgpu_init_i2c_edid(vgpu);
				 
				i2c_edid->gmbus.phase = GMBUS_IDLE_PHASE;
				vgpu_vreg_t(vgpu, PCH_GMBUS2) &= ~GMBUS_ACTIVE;
			}
			break;
		case NIDX_NS_W:
		case IDX_NS_W:
		case NIDX_STOP:
		case IDX_STOP:
			 
			i2c_edid->gmbus.phase = GMBUS_DATA_PHASE;
			vgpu_vreg_t(vgpu, PCH_GMBUS2) |= GMBUS_ACTIVE;
			break;
		default:
			gvt_vgpu_err("Unknown/reserved GMBUS cycle detected!\n");
			break;
		}
		 
		vgpu_vreg(vgpu, offset) = wvalue;
	}
	return 0;
}

static int gmbus3_mmio_write(struct intel_vgpu *vgpu, unsigned int offset,
	void *p_data, unsigned int bytes)
{
	struct drm_i915_private *i915 = vgpu->gvt->gt->i915;

	drm_WARN_ON(&i915->drm, 1);
	return 0;
}

static int gmbus3_mmio_read(struct intel_vgpu *vgpu, unsigned int offset,
		void *p_data, unsigned int bytes)
{
	int i;
	unsigned char byte_data;
	struct intel_vgpu_i2c_edid *i2c_edid = &vgpu->display.i2c_edid;
	int byte_left = i2c_edid->gmbus.total_byte_count -
				i2c_edid->current_edid_read;
	int byte_count = byte_left;
	u32 reg_data = 0;

	 
	if (vgpu_vreg_t(vgpu, PCH_GMBUS1) & GMBUS_SLAVE_READ) {
		if (byte_left <= 0) {
			memcpy(p_data, &vgpu_vreg(vgpu, offset), bytes);
			return 0;
		}

		if (byte_count > 4)
			byte_count = 4;
		for (i = 0; i < byte_count; i++) {
			byte_data = edid_get_byte(vgpu);
			reg_data |= (byte_data << (i << 3));
		}

		memcpy(&vgpu_vreg(vgpu, offset), &reg_data, byte_count);
		memcpy(p_data, &vgpu_vreg(vgpu, offset), bytes);

		if (byte_left <= 4) {
			switch (i2c_edid->gmbus.cycle_type) {
			case NIDX_STOP:
			case IDX_STOP:
				i2c_edid->gmbus.phase = GMBUS_IDLE_PHASE;
				break;
			case NIDX_NS_W:
			case IDX_NS_W:
			default:
				i2c_edid->gmbus.phase = GMBUS_WAIT_PHASE;
				break;
			}
			intel_vgpu_init_i2c_edid(vgpu);
		}
		 
	} else {
		memcpy(p_data, &vgpu_vreg(vgpu, offset), bytes);
		gvt_vgpu_err("warning: gmbus3 read with nothing returned\n");
	}
	return 0;
}

static int gmbus2_mmio_read(struct intel_vgpu *vgpu, unsigned int offset,
		void *p_data, unsigned int bytes)
{
	u32 value = vgpu_vreg(vgpu, offset);

	if (!(vgpu_vreg(vgpu, offset) & GMBUS_INUSE))
		vgpu_vreg(vgpu, offset) |= GMBUS_INUSE;
	memcpy(p_data, (void *)&value, bytes);
	return 0;
}

static int gmbus2_mmio_write(struct intel_vgpu *vgpu, unsigned int offset,
		void *p_data, unsigned int bytes)
{
	u32 wvalue = *(u32 *)p_data;

	if (wvalue & GMBUS_INUSE)
		vgpu_vreg(vgpu, offset) &= ~GMBUS_INUSE;
	 
	return 0;
}

 
int intel_gvt_i2c_handle_gmbus_read(struct intel_vgpu *vgpu,
	unsigned int offset, void *p_data, unsigned int bytes)
{
	struct drm_i915_private *i915 = vgpu->gvt->gt->i915;

	if (drm_WARN_ON(&i915->drm, bytes > 8 && (offset & (bytes - 1))))
		return -EINVAL;

	if (offset == i915_mmio_reg_offset(PCH_GMBUS2))
		return gmbus2_mmio_read(vgpu, offset, p_data, bytes);
	else if (offset == i915_mmio_reg_offset(PCH_GMBUS3))
		return gmbus3_mmio_read(vgpu, offset, p_data, bytes);

	memcpy(p_data, &vgpu_vreg(vgpu, offset), bytes);
	return 0;
}

 
int intel_gvt_i2c_handle_gmbus_write(struct intel_vgpu *vgpu,
		unsigned int offset, void *p_data, unsigned int bytes)
{
	struct drm_i915_private *i915 = vgpu->gvt->gt->i915;

	if (drm_WARN_ON(&i915->drm, bytes > 8 && (offset & (bytes - 1))))
		return -EINVAL;

	if (offset == i915_mmio_reg_offset(PCH_GMBUS0))
		return gmbus0_mmio_write(vgpu, offset, p_data, bytes);
	else if (offset == i915_mmio_reg_offset(PCH_GMBUS1))
		return gmbus1_mmio_write(vgpu, offset, p_data, bytes);
	else if (offset == i915_mmio_reg_offset(PCH_GMBUS2))
		return gmbus2_mmio_write(vgpu, offset, p_data, bytes);
	else if (offset == i915_mmio_reg_offset(PCH_GMBUS3))
		return gmbus3_mmio_write(vgpu, offset, p_data, bytes);

	memcpy(&vgpu_vreg(vgpu, offset), p_data, bytes);
	return 0;
}

enum {
	AUX_CH_CTL = 0,
	AUX_CH_DATA1,
	AUX_CH_DATA2,
	AUX_CH_DATA3,
	AUX_CH_DATA4,
	AUX_CH_DATA5
};

static inline int get_aux_ch_reg(unsigned int offset)
{
	int reg;

	switch (offset & 0xff) {
	case 0x10:
		reg = AUX_CH_CTL;
		break;
	case 0x14:
		reg = AUX_CH_DATA1;
		break;
	case 0x18:
		reg = AUX_CH_DATA2;
		break;
	case 0x1c:
		reg = AUX_CH_DATA3;
		break;
	case 0x20:
		reg = AUX_CH_DATA4;
		break;
	case 0x24:
		reg = AUX_CH_DATA5;
		break;
	default:
		reg = -1;
		break;
	}
	return reg;
}

 
void intel_gvt_i2c_handle_aux_ch_write(struct intel_vgpu *vgpu,
				int port_idx,
				unsigned int offset,
				void *p_data)
{
	struct drm_i915_private *i915 = vgpu->gvt->gt->i915;
	struct intel_vgpu_i2c_edid *i2c_edid = &vgpu->display.i2c_edid;
	int msg_length, ret_msg_size;
	int msg, addr, ctrl, op;
	u32 value = *(u32 *)p_data;
	int aux_data_for_write = 0;
	int reg = get_aux_ch_reg(offset);

	if (reg != AUX_CH_CTL) {
		vgpu_vreg(vgpu, offset) = value;
		return;
	}

	msg_length = REG_FIELD_GET(DP_AUX_CH_CTL_MESSAGE_SIZE_MASK, value);

	
	msg = vgpu_vreg(vgpu, offset + 4);
	addr = (msg >> 8) & 0xffff;
	ctrl = (msg >> 24) & 0xff;
	op = ctrl >> 4;
	if (!(value & DP_AUX_CH_CTL_SEND_BUSY)) {
		 
		return;
	}

	 
	ret_msg_size = (((op & 0x1) == GVT_AUX_I2C_READ) ? 2 : 1);
	vgpu_vreg(vgpu, offset) =
		DP_AUX_CH_CTL_DONE |
		DP_AUX_CH_CTL_MESSAGE_SIZE(ret_msg_size);

	if (msg_length == 3) {
		if (!(op & GVT_AUX_I2C_MOT)) {
			 
			intel_vgpu_init_i2c_edid(vgpu);
		} else {
			 
			i2c_edid->aux_ch.i2c_over_aux_ch = true;
			i2c_edid->aux_ch.aux_ch_mot = true;
			if (addr == 0) {
				 
				intel_vgpu_init_i2c_edid(vgpu);
			} else if (addr == EDID_ADDR) {
				i2c_edid->state = I2C_AUX_CH;
				i2c_edid->port = port_idx;
				i2c_edid->slave_selected = true;
				if (intel_vgpu_has_monitor_on_port(vgpu,
					port_idx) &&
					intel_vgpu_port_is_dp(vgpu, port_idx))
					i2c_edid->edid_available = true;
			}
		}
	} else if ((op & 0x1) == GVT_AUX_I2C_WRITE) {
		 
	} else {
		if (drm_WARN_ON(&i915->drm, (op & 0x1) != GVT_AUX_I2C_READ))
			return;
		if (drm_WARN_ON(&i915->drm, msg_length != 4))
			return;
		if (i2c_edid->edid_available && i2c_edid->slave_selected) {
			unsigned char val = edid_get_byte(vgpu);

			aux_data_for_write = (val << 16);
		} else
			aux_data_for_write = (0xff << 16);
	}
	 
	aux_data_for_write |= GVT_AUX_I2C_REPLY_ACK << 24;
	vgpu_vreg(vgpu, offset + 4) = aux_data_for_write;
}

 
void intel_vgpu_init_i2c_edid(struct intel_vgpu *vgpu)
{
	struct intel_vgpu_i2c_edid *edid = &vgpu->display.i2c_edid;

	edid->state = I2C_NOT_SPECIFIED;

	edid->port = -1;
	edid->slave_selected = false;
	edid->edid_available = false;
	edid->current_edid_read = 0;

	memset(&edid->gmbus, 0, sizeof(struct intel_vgpu_i2c_gmbus));

	edid->aux_ch.i2c_over_aux_ch = false;
	edid->aux_ch.aux_ch_mot = false;
}
