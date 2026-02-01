 

#ifndef __DCN32_CLK_MGR_SMU_MSG_H_
#define __DCN32_CLK_MGR_SMU_MSG_H_

#include "core_types.h"
#include "dcn30/dcn30_clk_mgr_smu_msg.h"

#define FCLK_PSTATE_NOTSUPPORTED       0x00
#define FCLK_PSTATE_SUPPORTED          0x01

 
#define DALSMC_MSG_SetCabForUclkPstate	0x12
#define DALSMC_Result_OK				0x1

void
dcn32_smu_send_fclk_pstate_message(struct clk_mgr_internal *clk_mgr, bool enable);
void dcn32_smu_transfer_wm_table_dram_2_smu(struct clk_mgr_internal *clk_mgr);
void dcn32_smu_set_pme_workaround(struct clk_mgr_internal *clk_mgr);
void dcn32_smu_send_cab_for_uclk_message(struct clk_mgr_internal *clk_mgr, unsigned int num_ways);
void dcn32_smu_transfer_wm_table_dram_2_smu(struct clk_mgr_internal *clk_mgr);
unsigned int dcn32_smu_set_hard_min_by_freq(struct clk_mgr_internal *clk_mgr, uint32_t clk, uint16_t freq_mhz);
void dcn32_smu_wait_for_dmub_ack_mclk(struct clk_mgr_internal *clk_mgr, bool enable);

#endif  
