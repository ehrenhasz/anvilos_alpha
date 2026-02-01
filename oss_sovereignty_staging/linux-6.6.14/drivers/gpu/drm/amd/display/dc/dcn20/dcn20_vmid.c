 

#include <linux/delay.h>

#include "dcn20_vmid.h"
#include "reg_helper.h"

#define REG(reg)\
	vmid->regs->reg

#define CTX \
	vmid->ctx

#undef FN
#define FN(reg_name, field_name) \
	vmid->shifts->field_name, vmid->masks->field_name

static void dcn20_wait_for_vmid_ready(struct dcn20_vmid *vmid)
{
	 

	int max_times = 10000;
	int delay_us  = 5;
	int i;

	for (i = 0; i < max_times; ++i) {
		uint32_t entry_lo32;

		REG_GET(PAGE_TABLE_BASE_ADDR_LO32,
			VM_CONTEXT0_PAGE_DIRECTORY_ENTRY_LO32,
			&entry_lo32);

		if (entry_lo32 & 0x1)
			return;

		udelay(delay_us);
	}

	 
	DC_LOG_WARNING("Timeout while waiting for GPUVM context update\n");
	ASSERT(0);
}

void dcn20_vmid_setup(struct dcn20_vmid *vmid, const struct dcn_vmid_page_table_config *config)
{
	REG_SET(PAGE_TABLE_START_ADDR_HI32, 0,
			VM_CONTEXT0_START_LOGICAL_PAGE_NUMBER_HI4, (config->page_table_start_addr >> 32) & 0xF);
	REG_SET(PAGE_TABLE_START_ADDR_LO32, 0,
			VM_CONTEXT0_START_LOGICAL_PAGE_NUMBER_LO32, config->page_table_start_addr & 0xFFFFFFFF);

	REG_SET(PAGE_TABLE_END_ADDR_HI32, 0,
			VM_CONTEXT0_END_LOGICAL_PAGE_NUMBER_HI4, (config->page_table_end_addr >> 32) & 0xF);
	REG_SET(PAGE_TABLE_END_ADDR_LO32, 0,
			VM_CONTEXT0_END_LOGICAL_PAGE_NUMBER_LO32, config->page_table_end_addr & 0xFFFFFFFF);

	REG_SET_2(CNTL, 0,
			VM_CONTEXT0_PAGE_TABLE_DEPTH, config->depth,
			VM_CONTEXT0_PAGE_TABLE_BLOCK_SIZE, config->block_size);

	REG_SET(PAGE_TABLE_BASE_ADDR_HI32, 0,
			VM_CONTEXT0_PAGE_DIRECTORY_ENTRY_HI32, (config->page_table_base_addr >> 32) & 0xFFFFFFFF);
	 
	REG_SET(PAGE_TABLE_BASE_ADDR_LO32, 0,
			VM_CONTEXT0_PAGE_DIRECTORY_ENTRY_LO32, config->page_table_base_addr & 0xFFFFFFFF);

	dcn20_wait_for_vmid_ready(vmid);
}
