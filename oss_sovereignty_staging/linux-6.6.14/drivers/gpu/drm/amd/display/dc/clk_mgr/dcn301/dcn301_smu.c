 

#include "core_types.h"
#include "clk_mgr_internal.h"
#include "reg_helper.h"
#include <linux/delay.h>

#include "dcn301_smu.h"

#include "vangogh_ip_offset.h"

#include "mp/mp_11_5_0_offset.h"
#include "mp/mp_11_5_0_sh_mask.h"

#define REG(reg_name) \
	(MP0_BASE.instance[0].segment[mm ## reg_name ## _BASE_IDX] + mm ## reg_name)

#define FN(reg_name, field) \
	FD(reg_name##__##field)

#include "logger_types.h"
#undef DC_LOGGER
#define DC_LOGGER \
	CTX->logger
#define smu_print(str, ...) {DC_LOG_SMU(str, ##__VA_ARGS__); }

#define VBIOSSMC_MSG_GetSmuVersion                0x2
#define VBIOSSMC_MSG_SetDispclkFreq               0x4
#define VBIOSSMC_MSG_SetDprefclkFreq              0x5
#define VBIOSSMC_MSG_SetDppclkFreq                0x6
#define VBIOSSMC_MSG_SetHardMinDcfclkByFreq       0x7
#define VBIOSSMC_MSG_SetMinDeepSleepDcfclk        0x8

#define VBIOSSMC_MSG_GetFclkFrequency             0xA


#define VBIOSSMC_MSG_UpdatePmeRestore			  0xD
#define VBIOSSMC_MSG_SetVbiosDramAddrHigh         0xE   
#define VBIOSSMC_MSG_SetVbiosDramAddrLow          0xF
#define VBIOSSMC_MSG_TransferTableSmu2Dram        0x10
#define VBIOSSMC_MSG_TransferTableDram2Smu        0x11
#define VBIOSSMC_MSG_SetDisplayIdleOptimizations  0x12

#define VBIOSSMC_Status_BUSY                      0x0
#define VBIOSSMC_Result_OK                        0x1
#define VBIOSSMC_Result_Failed                    0xFF
#define VBIOSSMC_Result_UnknownCmd                0xFE
#define VBIOSSMC_Result_CmdRejectedPrereq         0xFD
#define VBIOSSMC_Result_CmdRejectedBusy           0xFC

 
static uint32_t dcn301_smu_wait_for_response(struct clk_mgr_internal *clk_mgr, unsigned int delay_us, unsigned int max_retries)
{
	uint32_t res_val = VBIOSSMC_Status_BUSY;

	do {
		res_val = REG_READ(MP1_SMN_C2PMSG_91);
		if (res_val != VBIOSSMC_Status_BUSY)
			break;

		if (delay_us >= 1000)
			msleep(delay_us/1000);
		else if (delay_us > 0)
			udelay(delay_us);
	} while (max_retries--);

	return res_val;
}

static int dcn301_smu_send_msg_with_param(struct clk_mgr_internal *clk_mgr,
					  unsigned int msg_id,
					  unsigned int param)
{
	uint32_t result;

	result = dcn301_smu_wait_for_response(clk_mgr, 10, 200000);

	if (result != VBIOSSMC_Result_OK)
		smu_print("SMU Response was not OK. SMU response after wait received is: %d\n", result);

	if (result == VBIOSSMC_Status_BUSY) {
		return -1;
	}

	 
	REG_WRITE(MP1_SMN_C2PMSG_91, VBIOSSMC_Status_BUSY);

	 
	REG_WRITE(MP1_SMN_C2PMSG_83, param);

	 
	REG_WRITE(MP1_SMN_C2PMSG_67, msg_id);

	result = dcn301_smu_wait_for_response(clk_mgr, 10, 200000);

	ASSERT(result == VBIOSSMC_Result_OK);

	 
	return REG_READ(MP1_SMN_C2PMSG_83);
}

int dcn301_smu_get_smu_version(struct clk_mgr_internal *clk_mgr)
{
	int smu_version = dcn301_smu_send_msg_with_param(clk_mgr,
							 VBIOSSMC_MSG_GetSmuVersion,
							 0);

	DC_LOG_DEBUG("%s %x\n", __func__, smu_version);

	return smu_version;
}


int dcn301_smu_set_dispclk(struct clk_mgr_internal *clk_mgr, int requested_dispclk_khz)
{
	int actual_dispclk_set_mhz = -1;

	DC_LOG_DEBUG("%s(%d)\n", __func__, requested_dispclk_khz);

	 
	actual_dispclk_set_mhz = dcn301_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDispclkFreq,
			khz_to_mhz_ceil(requested_dispclk_khz));

	return actual_dispclk_set_mhz * 1000;
}

int dcn301_smu_set_dprefclk(struct clk_mgr_internal *clk_mgr)
{
	int actual_dprefclk_set_mhz = -1;

	DC_LOG_DEBUG("%s %d\n", __func__, clk_mgr->base.dprefclk_khz / 1000);

	actual_dprefclk_set_mhz = dcn301_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDprefclkFreq,
			khz_to_mhz_ceil(clk_mgr->base.dprefclk_khz));

	 

	return actual_dprefclk_set_mhz * 1000;
}

int dcn301_smu_set_hard_min_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_dcfclk_khz)
{
	int actual_dcfclk_set_mhz = -1;

	DC_LOG_DEBUG("%s(%d)\n", __func__, requested_dcfclk_khz);

	actual_dcfclk_set_mhz = dcn301_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetHardMinDcfclkByFreq,
			khz_to_mhz_ceil(requested_dcfclk_khz));

#ifdef DBG
	smu_print("actual_dcfclk_set_mhz %d is set to : %d\n", actual_dcfclk_set_mhz, actual_dcfclk_set_mhz * 1000);
#endif

	return actual_dcfclk_set_mhz * 1000;
}

int dcn301_smu_set_min_deep_sleep_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_min_ds_dcfclk_khz)
{
	int actual_min_ds_dcfclk_mhz = -1;

	DC_LOG_DEBUG("%s(%d)\n", __func__, requested_min_ds_dcfclk_khz);

	actual_min_ds_dcfclk_mhz = dcn301_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetMinDeepSleepDcfclk,
			khz_to_mhz_ceil(requested_min_ds_dcfclk_khz));

	return actual_min_ds_dcfclk_mhz * 1000;
}

int dcn301_smu_set_dppclk(struct clk_mgr_internal *clk_mgr, int requested_dpp_khz)
{
	int actual_dppclk_set_mhz = -1;

	DC_LOG_DEBUG("%s(%d)\n", __func__, requested_dpp_khz);

	actual_dppclk_set_mhz = dcn301_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDppclkFreq,
			khz_to_mhz_ceil(requested_dpp_khz));

	return actual_dppclk_set_mhz * 1000;
}

void dcn301_smu_set_display_idle_optimization(struct clk_mgr_internal *clk_mgr, uint32_t idle_info)
{
	

	DC_LOG_DEBUG("%s(%x)\n", __func__, idle_info);

	dcn301_smu_send_msg_with_param(
		clk_mgr,
		VBIOSSMC_MSG_SetDisplayIdleOptimizations,
		idle_info);
}

void dcn301_smu_enable_phy_refclk_pwrdwn(struct clk_mgr_internal *clk_mgr, bool enable)
{
	union display_idle_optimization_u idle_info = { 0 };

	if (enable) {
		idle_info.idle_info.df_request_disabled = 1;
		idle_info.idle_info.phy_ref_clk_off = 1;
	}

	DC_LOG_DEBUG("%s(%d)\n", __func__, enable);

	dcn301_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDisplayIdleOptimizations,
			idle_info.data);
}

void dcn301_smu_enable_pme_wa(struct clk_mgr_internal *clk_mgr)
{
	dcn301_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_UpdatePmeRestore,
			0);
}

void dcn301_smu_set_dram_addr_high(struct clk_mgr_internal *clk_mgr, uint32_t addr_high)
{
	DC_LOG_DEBUG("%s(%x)\n", __func__, addr_high);

	dcn301_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_SetVbiosDramAddrHigh, addr_high);
}

void dcn301_smu_set_dram_addr_low(struct clk_mgr_internal *clk_mgr, uint32_t addr_low)
{
	DC_LOG_DEBUG("%s(%x)\n", __func__, addr_low);

	dcn301_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_SetVbiosDramAddrLow, addr_low);
}

void dcn301_smu_transfer_dpm_table_smu_2_dram(struct clk_mgr_internal *clk_mgr)
{
	dcn301_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_TransferTableSmu2Dram, TABLE_DPMCLOCKS);
}

void dcn301_smu_transfer_wm_table_dram_2_smu(struct clk_mgr_internal *clk_mgr)
{
	dcn301_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_TransferTableDram2Smu, TABLE_WATERMARKS);
}
