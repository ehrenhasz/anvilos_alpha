#ifndef __MMSCH_V3_0_H__
#define __MMSCH_V3_0_H__
#include "amdgpu_vcn.h"
#define MMSCH_VERSION_MAJOR	3
#define MMSCH_VERSION_MINOR	0
#define MMSCH_VERSION	(MMSCH_VERSION_MAJOR << 16 | MMSCH_VERSION_MINOR)
#define MMSCH_V3_0_VCN_INSTANCES 0x2
enum mmsch_v3_0_command_type {
	MMSCH_COMMAND__DIRECT_REG_WRITE = 0,
	MMSCH_COMMAND__DIRECT_REG_POLLING = 2,
	MMSCH_COMMAND__DIRECT_REG_READ_MODIFY_WRITE = 3,
	MMSCH_COMMAND__INDIRECT_REG_WRITE = 8,
	MMSCH_COMMAND__END = 0xf
};
struct mmsch_v3_0_table_info {
	uint32_t init_status;
	uint32_t table_offset;
	uint32_t table_size;
};
struct mmsch_v3_0_init_header {
	uint32_t version;
	uint32_t total_size;
	struct mmsch_v3_0_table_info inst[MMSCH_V3_0_VCN_INSTANCES];
};
struct mmsch_v3_0_cmd_direct_reg_header {
	uint32_t reg_offset   : 28;
	uint32_t command_type : 4;
};
struct mmsch_v3_0_cmd_indirect_reg_header {
	uint32_t reg_offset    : 20;
	uint32_t reg_idx_space : 8;
	uint32_t command_type  : 4;
};
struct mmsch_v3_0_cmd_direct_write {
	struct mmsch_v3_0_cmd_direct_reg_header cmd_header;
	uint32_t reg_value;
};
struct mmsch_v3_0_cmd_direct_read_modify_write {
	struct mmsch_v3_0_cmd_direct_reg_header cmd_header;
	uint32_t write_data;
	uint32_t mask_value;
};
struct mmsch_v3_0_cmd_direct_polling {
	struct mmsch_v3_0_cmd_direct_reg_header cmd_header;
	uint32_t mask_value;
	uint32_t wait_value;
};
struct mmsch_v3_0_cmd_end {
	struct mmsch_v3_0_cmd_direct_reg_header cmd_header;
};
struct mmsch_v3_0_cmd_indirect_write {
	struct mmsch_v3_0_cmd_indirect_reg_header cmd_header;
	uint32_t reg_value;
};
#define MMSCH_V3_0_INSERT_DIRECT_RD_MOD_WT(reg, mask, data) { \
	size = sizeof(struct mmsch_v3_0_cmd_direct_read_modify_write); \
	size_dw = size / 4; \
	direct_rd_mod_wt.cmd_header.reg_offset = reg; \
	direct_rd_mod_wt.mask_value = mask; \
	direct_rd_mod_wt.write_data = data; \
	memcpy((void *)table_loc, &direct_rd_mod_wt, size); \
	table_loc += size_dw; \
	table_size += size_dw; \
}
#define MMSCH_V3_0_INSERT_DIRECT_WT(reg, value) { \
	size = sizeof(struct mmsch_v3_0_cmd_direct_write); \
	size_dw = size / 4; \
	direct_wt.cmd_header.reg_offset = reg; \
	direct_wt.reg_value = value; \
	memcpy((void *)table_loc, &direct_wt, size); \
	table_loc += size_dw; \
	table_size += size_dw; \
}
#define MMSCH_V3_0_INSERT_DIRECT_POLL(reg, mask, wait) { \
	size = sizeof(struct mmsch_v3_0_cmd_direct_polling); \
	size_dw = size / 4; \
	direct_poll.cmd_header.reg_offset = reg; \
	direct_poll.mask_value = mask; \
	direct_poll.wait_value = wait; \
	memcpy((void *)table_loc, &direct_poll, size); \
	table_loc += size_dw; \
	table_size += size_dw; \
}
#define MMSCH_V3_0_INSERT_END() { \
	size = sizeof(struct mmsch_v3_0_cmd_end); \
	size_dw = size / 4; \
	memcpy((void *)table_loc, &end, size); \
	table_loc += size_dw; \
	table_size += size_dw; \
}
#endif
