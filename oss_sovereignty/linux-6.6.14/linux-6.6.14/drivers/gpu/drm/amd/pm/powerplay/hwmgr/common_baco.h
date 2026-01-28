#ifndef __COMMON_BOCO_H__
#define __COMMON_BOCO_H__
#include "hwmgr.h"
enum baco_cmd_type {
	CMD_WRITE = 0,
	CMD_READMODIFYWRITE,
	CMD_WAITFOR,
	CMD_DELAY_MS,
	CMD_DELAY_US,
};
struct baco_cmd_entry {
	enum baco_cmd_type cmd;
	uint32_t 	reg_offset;
	uint32_t     	mask;
	uint32_t     	shift;
	uint32_t     	timeout;
	uint32_t     	val;
};
struct soc15_baco_cmd_entry {
	enum baco_cmd_type cmd;
	uint32_t 	hwip;
	uint32_t 	inst;
	uint32_t 	seg;
	uint32_t 	reg_offset;
	uint32_t     	mask;
	uint32_t     	shift;
	uint32_t     	timeout;
	uint32_t     	val;
};
extern bool baco_program_registers(struct pp_hwmgr *hwmgr,
				   const struct baco_cmd_entry *entry,
				   const u32 array_size);
extern bool soc15_baco_program_registers(struct pp_hwmgr *hwmgr,
					const struct soc15_baco_cmd_entry *entry,
					const u32 array_size);
#endif
