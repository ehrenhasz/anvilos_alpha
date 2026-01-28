#ifndef DAL_DC_INC_HW_VMID_H_
#define DAL_DC_INC_HW_VMID_H_
#include "core_types.h"
#include "dchubbub.h"
struct dcn_vmid_registers {
	uint32_t CNTL;
	uint32_t PAGE_TABLE_BASE_ADDR_HI32;
	uint32_t PAGE_TABLE_BASE_ADDR_LO32;
	uint32_t PAGE_TABLE_START_ADDR_HI32;
	uint32_t PAGE_TABLE_START_ADDR_LO32;
	uint32_t PAGE_TABLE_END_ADDR_HI32;
	uint32_t PAGE_TABLE_END_ADDR_LO32;
};
struct dcn_vmid_page_table_config {
	uint64_t	page_table_start_addr;
	uint64_t	page_table_end_addr;
	enum dcn_hubbub_page_table_depth	depth;
	enum dcn_hubbub_page_table_block_size	block_size;
	uint64_t	page_table_base_addr;
};
#endif  
