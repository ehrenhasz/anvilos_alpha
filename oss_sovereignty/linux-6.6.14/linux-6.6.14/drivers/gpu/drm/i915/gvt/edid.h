#ifndef _GVT_EDID_H_
#define _GVT_EDID_H_
#include <linux/types.h>
struct intel_vgpu;
#define EDID_SIZE		128
#define EDID_ADDR		0x50  
#define GVT_AUX_NATIVE_WRITE			0x8
#define GVT_AUX_NATIVE_READ			0x9
#define GVT_AUX_I2C_WRITE			0x0
#define GVT_AUX_I2C_READ			0x1
#define GVT_AUX_I2C_STATUS			0x2
#define GVT_AUX_I2C_MOT				0x4
#define GVT_AUX_I2C_REPLY_ACK			0x0
struct intel_vgpu_edid_data {
	bool data_valid;
	unsigned char edid_block[EDID_SIZE];
};
enum gmbus_cycle_type {
	GMBUS_NOCYCLE	= 0x0,
	NIDX_NS_W	= 0x1,
	IDX_NS_W	= 0x3,
	GMBUS_STOP	= 0x4,
	NIDX_STOP	= 0x5,
	IDX_STOP	= 0x7
};
enum gvt_gmbus_phase {
	GMBUS_IDLE_PHASE = 0,
	GMBUS_DATA_PHASE,
	GMBUS_WAIT_PHASE,
	GMBUS_MAX_PHASE
};
struct intel_vgpu_i2c_gmbus {
	unsigned int total_byte_count;  
	enum gmbus_cycle_type cycle_type;
	enum gvt_gmbus_phase phase;
};
struct intel_vgpu_i2c_aux_ch {
	bool i2c_over_aux_ch;
	bool aux_ch_mot;
};
enum i2c_state {
	I2C_NOT_SPECIFIED = 0,
	I2C_GMBUS = 1,
	I2C_AUX_CH = 2
};
struct intel_vgpu_i2c_edid {
	enum i2c_state state;
	unsigned int port;
	bool slave_selected;
	bool edid_available;
	unsigned int current_edid_read;
	struct intel_vgpu_i2c_gmbus gmbus;
	struct intel_vgpu_i2c_aux_ch aux_ch;
};
void intel_vgpu_init_i2c_edid(struct intel_vgpu *vgpu);
int intel_gvt_i2c_handle_gmbus_read(struct intel_vgpu *vgpu,
		unsigned int offset, void *p_data, unsigned int bytes);
int intel_gvt_i2c_handle_gmbus_write(struct intel_vgpu *vgpu,
		unsigned int offset, void *p_data, unsigned int bytes);
void intel_gvt_i2c_handle_aux_ch_write(struct intel_vgpu *vgpu,
		int port_idx,
		unsigned int offset,
		void *p_data);
#endif  
