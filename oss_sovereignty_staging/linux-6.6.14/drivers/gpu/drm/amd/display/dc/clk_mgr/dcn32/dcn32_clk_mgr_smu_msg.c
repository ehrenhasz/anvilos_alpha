 

#include "dcn32_clk_mgr_smu_msg.h"

#include "clk_mgr_internal.h"
#include "reg_helper.h"
#include "dalsmc.h"
#include "smu13_driver_if.h"

#define mmDAL_MSG_REG  0x1628A
#define mmDAL_ARG_REG  0x16273
#define mmDAL_RESP_REG 0x16274

#define REG(reg_name) \
	mm ## reg_name

#include "logger_types.h"

#define smu_print(str, ...) {DC_LOG_SMU(str, ##__VA_ARGS__); }


 
static uint32_t dcn32_smu_wait_for_response(struct clk_mgr_internal *clk_mgr, unsigned int delay_us, unsigned int max_retries)
{
	uint32_t reg = 0;

	do {
		reg = REG_READ(DAL_RESP_REG);
		if (reg)
			break;

		if (delay_us >= 1000)
			msleep(delay_us/1000);
		else if (delay_us > 0)
			udelay(delay_us);
	} while (max_retries--);

	return reg;
}

static bool dcn32_smu_send_msg_with_param(struct clk_mgr_internal *clk_mgr, uint32_t msg_id, uint32_t param_in, uint32_t *param_out)
{
	 
	dcn32_smu_wait_for_response(clk_mgr, 10, 200000);

	 
	REG_WRITE(DAL_RESP_REG, 0);

	 
	REG_WRITE(DAL_ARG_REG, param_in);

	 
	REG_WRITE(DAL_MSG_REG, msg_id);

	 
	if (dcn32_smu_wait_for_response(clk_mgr, 10, 200000) == DALSMC_Result_OK) {
		if (param_out)
			*param_out = REG_READ(DAL_ARG_REG);

		return true;
	}

	return false;
}

void dcn32_smu_send_fclk_pstate_message(struct clk_mgr_internal *clk_mgr, bool enable)
{
	smu_print("FCLK P-state support value is : %d\n", enable);

	dcn32_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_SetFclkSwitchAllow, enable ? FCLK_PSTATE_SUPPORTED : FCLK_PSTATE_NOTSUPPORTED, NULL);
}

void dcn32_smu_send_cab_for_uclk_message(struct clk_mgr_internal *clk_mgr, unsigned int num_ways)
{
	uint32_t param = (num_ways << 1) | (num_ways > 0);

	dcn32_smu_send_msg_with_param(clk_mgr, DALSMC_MSG_SetCabForUclkPstate, param, NULL);
	smu_print("Numways for SubVP : %d\n", num_ways);
}

void dcn32_smu_transfer_wm_table_dram_2_smu(struct clk_mgr_internal *clk_mgr)
{
	smu_print("SMU Transfer WM table DRAM 2 SMU\n");

	dcn32_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_TransferTableDram2Smu, TABLE_WATERMARKS, NULL);
}

void dcn32_smu_set_pme_workaround(struct clk_mgr_internal *clk_mgr)
{
	smu_print("SMU Set PME workaround\n");

	dcn32_smu_send_msg_with_param(clk_mgr,
		DALSMC_MSG_BacoAudioD3PME, 0, NULL);
}

 
unsigned int dcn32_smu_set_hard_min_by_freq(struct clk_mgr_internal *clk_mgr, uint32_t clk, uint16_t freq_mhz)
{
	uint32_t response = 0;

	 
	uint32_t param = (clk << 16) | freq_mhz;

	smu_print("SMU Set hard min by freq: clk = %d, freq_mhz = %d MHz\n", clk, freq_mhz);

	dcn32_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_SetHardMinByFreq, param, &response);

	smu_print("SMU Frequency set = %d KHz\n", response);

	return response;
}

void dcn32_smu_wait_for_dmub_ack_mclk(struct clk_mgr_internal *clk_mgr, bool enable)
{
	smu_print("PMFW to wait for DMCUB ack for MCLK : %d\n", enable);

	dcn32_smu_send_msg_with_param(clk_mgr, 0x14, enable ? 1 : 0, NULL);
}
