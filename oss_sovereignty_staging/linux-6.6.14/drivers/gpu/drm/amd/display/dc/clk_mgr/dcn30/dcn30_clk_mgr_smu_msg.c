 

#include <linux/delay.h>
#include "dcn30_clk_mgr_smu_msg.h"

#include "clk_mgr_internal.h"
#include "reg_helper.h"
#include "dm_helpers.h"

#include "dalsmc.h"
#include "dcn30_smu11_driver_if.h"

#define mmDAL_MSG_REG  0x1628A
#define mmDAL_ARG_REG  0x16273
#define mmDAL_RESP_REG 0x16274

#define REG(reg_name) \
	mm ## reg_name

#include "logger_types.h"
#undef DC_LOGGER
#define DC_LOGGER \
	CTX->logger
#define smu_print(str, ...) {DC_LOG_SMU(str, ##__VA_ARGS__); }


 
static uint32_t dcn30_smu_wait_for_response(struct clk_mgr_internal *clk_mgr, unsigned int delay_us, unsigned int max_retries)
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

static bool dcn30_smu_send_msg_with_param(struct clk_mgr_internal *clk_mgr, uint32_t msg_id, uint32_t param_in, uint32_t *param_out)
{
	uint32_t result;
	 
	dcn30_smu_wait_for_response(clk_mgr, 10, 200000);

	 
	REG_WRITE(DAL_RESP_REG, 0);

	 
	REG_WRITE(DAL_ARG_REG, param_in);

	 
	REG_WRITE(DAL_MSG_REG, msg_id);

	result = dcn30_smu_wait_for_response(clk_mgr, 10, 200000);

	if (IS_SMU_TIMEOUT(result)) {
		dm_helpers_smu_timeout(CTX, msg_id, param_in, 10 * 200000);
	}

	 
	if (result == DALSMC_Result_OK) {
		if (param_out)
			*param_out = REG_READ(DAL_ARG_REG);

		return true;
	}

	return false;
}

 
bool dcn30_smu_test_message(struct clk_mgr_internal *clk_mgr, uint32_t input)
{
	uint32_t response = 0;

	smu_print("SMU Test message: %d\n", input);

	if (dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_TestMessage, input, &response))
		if (response == input + 1)
			return true;

	return false;
}

bool dcn30_smu_get_smu_version(struct clk_mgr_internal *clk_mgr, unsigned int *version)
{
	smu_print("SMU Get SMU version\n");

	if (dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_GetSmuVersion, 0, version)) {

		smu_print("SMU version: %d\n", *version);

		return true;
	}

	return false;
}

 
bool dcn30_smu_check_driver_if_version(struct clk_mgr_internal *clk_mgr)
{
	uint32_t response = 0;

	smu_print("SMU Check driver if version\n");

	if (dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_GetDriverIfVersion, 0, &response)) {

		smu_print("SMU driver if version: %d\n", response);

		if (response == SMU11_DRIVER_IF_VERSION)
			return true;
	}

	return false;
}

 
bool dcn30_smu_check_msg_header_version(struct clk_mgr_internal *clk_mgr)
{
	uint32_t response = 0;

	smu_print("SMU Check msg header version\n");

	if (dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_GetMsgHeaderVersion, 0, &response)) {

		smu_print("SMU msg header version: %d\n", response);

		if (response == DALSMC_VERSION)
			return true;
	}

	return false;
}

void dcn30_smu_set_dram_addr_high(struct clk_mgr_internal *clk_mgr, uint32_t addr_high)
{
	smu_print("SMU Set DRAM addr high: %d\n", addr_high);

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_SetDalDramAddrHigh, addr_high, NULL);
}

void dcn30_smu_set_dram_addr_low(struct clk_mgr_internal *clk_mgr, uint32_t addr_low)
{
	smu_print("SMU Set DRAM addr low: %d\n", addr_low);

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_SetDalDramAddrLow, addr_low, NULL);
}

void dcn30_smu_transfer_wm_table_smu_2_dram(struct clk_mgr_internal *clk_mgr)
{
	smu_print("SMU Transfer WM table SMU 2 DRAM\n");

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_TransferTableSmu2Dram, TABLE_WATERMARKS, NULL);
}

void dcn30_smu_transfer_wm_table_dram_2_smu(struct clk_mgr_internal *clk_mgr)
{
	smu_print("SMU Transfer WM table DRAM 2 SMU\n");

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_TransferTableDram2Smu, TABLE_WATERMARKS, NULL);
}

 
unsigned int dcn30_smu_set_hard_min_by_freq(struct clk_mgr_internal *clk_mgr, uint32_t clk, uint16_t freq_mhz)
{
	uint32_t response = 0;

	 
	uint32_t param = (clk << 16) | freq_mhz;

	smu_print("SMU Set hard min by freq: clk = %d, freq_mhz = %d MHz\n", clk, freq_mhz);

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_SetHardMinByFreq, param, &response);

	smu_print("SMU Frequency set = %d MHz\n", response);

	return response;
}

 
unsigned int dcn30_smu_set_hard_max_by_freq(struct clk_mgr_internal *clk_mgr, uint32_t clk, uint16_t freq_mhz)
{
	uint32_t response = 0;

	 
	uint32_t param = (clk << 16) | freq_mhz;

	smu_print("SMU Set hard max by freq: clk = %d, freq_mhz = %d MHz\n", clk, freq_mhz);

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_SetHardMaxByFreq, param, &response);

	smu_print("SMU Frequency set = %d MHz\n", response);

	return response;
}

 
unsigned int dcn30_smu_get_dpm_freq_by_index(struct clk_mgr_internal *clk_mgr, uint32_t clk, uint8_t dpm_level)
{
	uint32_t response = 0;

	 
	uint32_t param = (clk << 16) | dpm_level;

	smu_print("SMU Get dpm freq by index: clk = %d, dpm_level = %d\n", clk, dpm_level);

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_GetDpmFreqByIndex, param, &response);

	smu_print("SMU dpm freq: %d MHz\n", response);

	return response;
}

 
unsigned int dcn30_smu_get_dc_mode_max_dpm_freq(struct clk_mgr_internal *clk_mgr, uint32_t clk)
{
	uint32_t response = 0;

	 
	uint32_t param = clk << 16;

	smu_print("SMU Get DC mode max DPM freq: clk = %d\n", clk);

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_GetDcModeMaxDpmFreq, param, &response);

	smu_print("SMU DC mode max DMP freq: %d MHz\n", response);

	return response;
}

void dcn30_smu_set_min_deep_sleep_dcef_clk(struct clk_mgr_internal *clk_mgr, uint32_t freq_mhz)
{
	smu_print("SMU Set min deep sleep dcef clk: freq_mhz = %d MHz\n", freq_mhz);

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_SetMinDeepSleepDcefclk, freq_mhz, NULL);
}

void dcn30_smu_set_num_of_displays(struct clk_mgr_internal *clk_mgr, uint32_t num_displays)
{
	smu_print("SMU Set num of displays: num_displays = %d\n", num_displays);

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_NumOfDisplays, num_displays, NULL);
}

void dcn30_smu_set_display_refresh_from_mall(struct clk_mgr_internal *clk_mgr, bool enable, uint8_t cache_timer_delay, uint8_t cache_timer_scale)
{
	 
	uint32_t param = (cache_timer_scale << 7) | (cache_timer_delay << 1) | (enable ? 1 : 0);

	smu_print("SMU Set display refresh from mall: enable = %d, cache_timer_delay = %d, cache_timer_scale = %d\n",
		enable, cache_timer_delay, cache_timer_scale);

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_SetDisplayRefreshFromMall, param, NULL);
}

void dcn30_smu_set_external_client_df_cstate_allow(struct clk_mgr_internal *clk_mgr, bool enable)
{
	smu_print("SMU Set external client df cstate allow: enable = %d\n", enable);

	dcn30_smu_send_msg_with_param(clk_mgr,
			DALSMC_MSG_SetExternalClientDfCstateAllow, enable ? 1 : 0, NULL);
}

void dcn30_smu_set_pme_workaround(struct clk_mgr_internal *clk_mgr)
{
	smu_print("SMU Set PME workaround\n");

	dcn30_smu_send_msg_with_param(clk_mgr,
	DALSMC_MSG_BacoAudioD3PME, 0, NULL);
}
