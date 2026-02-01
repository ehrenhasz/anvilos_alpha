 
 

#ifndef _VDEC_IPI_MSG_H_
#define _VDEC_IPI_MSG_H_

 
enum vdec_ipi_msgid {
	AP_IPIMSG_DEC_INIT = 0xA000,
	AP_IPIMSG_DEC_START = 0xA001,
	AP_IPIMSG_DEC_END = 0xA002,
	AP_IPIMSG_DEC_DEINIT = 0xA003,
	AP_IPIMSG_DEC_RESET = 0xA004,
	AP_IPIMSG_DEC_CORE = 0xA005,
	AP_IPIMSG_DEC_CORE_END = 0xA006,
	AP_IPIMSG_DEC_GET_PARAM = 0xA007,

	VPU_IPIMSG_DEC_INIT_ACK = 0xB000,
	VPU_IPIMSG_DEC_START_ACK = 0xB001,
	VPU_IPIMSG_DEC_END_ACK = 0xB002,
	VPU_IPIMSG_DEC_DEINIT_ACK = 0xB003,
	VPU_IPIMSG_DEC_RESET_ACK = 0xB004,
	VPU_IPIMSG_DEC_CORE_ACK = 0xB005,
	VPU_IPIMSG_DEC_CORE_END_ACK = 0xB006,
	VPU_IPIMSG_DEC_GET_PARAM_ACK = 0xB007,
};

 
struct vdec_ap_ipi_cmd {
	uint32_t msg_id;
	union {
		uint32_t vpu_inst_addr;
		uint32_t inst_id;
	};
	u32 codec_type;
	u32 reserved;
};

 
struct vdec_vpu_ipi_ack {
	uint32_t msg_id;
	int32_t status;
	uint64_t ap_inst_addr;
};

 
struct vdec_ap_ipi_init {
	uint32_t msg_id;
	u32 codec_type;
	uint64_t ap_inst_addr;
};

 
struct vdec_ap_ipi_dec_start {
	uint32_t msg_id;
	union {
		uint32_t vpu_inst_addr;
		uint32_t inst_id;
	};
	uint32_t data[3];
	u32 codec_type;
};

 
struct vdec_vpu_ipi_init_ack {
	uint32_t msg_id;
	int32_t status;
	uint64_t ap_inst_addr;
	uint32_t vpu_inst_addr;
	uint32_t vdec_abi_version;
	uint32_t inst_id;
};

 
struct vdec_ap_ipi_get_param {
	u32 msg_id;
	u32 inst_id;
	u32 data[4];
	u32 param_type;
	u32 codec_type;
};

 
struct vdec_vpu_ipi_get_param_ack {
	u32 msg_id;
	s32 status;
	u64 ap_inst_addr;
	u32 data[4];
	u32 param_type;
	u32 reserved;
};

#endif
