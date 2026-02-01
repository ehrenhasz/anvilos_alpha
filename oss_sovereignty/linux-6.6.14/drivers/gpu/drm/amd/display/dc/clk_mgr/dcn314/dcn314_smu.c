
 



#include "core_types.h"
#include "clk_mgr_internal.h"
#include "reg_helper.h"
#include "dm_helpers.h"
#include "dcn314_smu.h"

#include "mp/mp_13_0_5_offset.h"

 
#define MP1_BASE__INST0_SEG0                       0x00016000
#define MP1_BASE__INST0_SEG1                       0x0243FC00
#define MP1_BASE__INST0_SEG2                       0x00DC0000
#define MP1_BASE__INST0_SEG3                       0x00E00000
#define MP1_BASE__INST0_SEG4                       0x00E40000
#define MP1_BASE__INST0_SEG5                       0

#ifdef BASE_INNER
#undef BASE_INNER
#endif

#define BASE_INNER(seg) MP1_BASE__INST0_SEG ## seg

#define BASE(seg) BASE_INNER(seg)

#define REG(reg_name) (BASE(reg##reg_name##_BASE_IDX) + reg##reg_name)

#define FN(reg_name, field) \
	FD(reg_name##__##field)

#include "logger_types.h"
#undef DC_LOGGER
#define DC_LOGGER \
	CTX->logger
#define smu_print(str, ...) {DC_LOG_SMU(str, ##__VA_ARGS__); }

#define VBIOSSMC_MSG_TestMessage                  0x1
#define VBIOSSMC_MSG_GetSmuVersion                0x2
#define VBIOSSMC_MSG_PowerUpGfx                   0x3
#define VBIOSSMC_MSG_SetDispclkFreq               0x4
#define VBIOSSMC_MSG_SetDprefclkFreq              0x5   
#define VBIOSSMC_MSG_SetDppclkFreq                0x6
#define VBIOSSMC_MSG_SetHardMinDcfclkByFreq       0x7
#define VBIOSSMC_MSG_SetMinDeepSleepDcfclk        0x8
#define VBIOSSMC_MSG_SetPhyclkVoltageByFreq       0x9	
#define VBIOSSMC_MSG_GetFclkFrequency             0xA
#define VBIOSSMC_MSG_SetDisplayCount              0xB   
#define VBIOSSMC_MSG_EnableTmdp48MHzRefclkPwrDown 0xC   
#define VBIOSSMC_MSG_UpdatePmeRestore             0xD
#define VBIOSSMC_MSG_SetVbiosDramAddrHigh         0xE   
#define VBIOSSMC_MSG_SetVbiosDramAddrLow          0xF
#define VBIOSSMC_MSG_TransferTableSmu2Dram        0x10
#define VBIOSSMC_MSG_TransferTableDram2Smu        0x11
#define VBIOSSMC_MSG_SetDisplayIdleOptimizations  0x12
#define VBIOSSMC_MSG_GetDprefclkFreq              0x13
#define VBIOSSMC_MSG_GetDtbclkFreq                0x14
#define VBIOSSMC_MSG_AllowZstatesEntry            0x15
#define VBIOSSMC_MSG_DisallowZstatesEntry	  0x16
#define VBIOSSMC_MSG_SetDtbClk                    0x17
#define VBIOSSMC_Message_Count                    0x18

#define VBIOSSMC_Status_BUSY                      0x0
#define VBIOSSMC_Result_OK                        0x1
#define VBIOSSMC_Result_Failed                    0xFF
#define VBIOSSMC_Result_UnknownCmd                0xFE
#define VBIOSSMC_Result_CmdRejectedPrereq         0xFD
#define VBIOSSMC_Result_CmdRejectedBusy           0xFC

 
static uint32_t dcn314_smu_wait_for_response(struct clk_mgr_internal *clk_mgr, unsigned int delay_us, unsigned int max_retries)
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

static int dcn314_smu_send_msg_with_param(struct clk_mgr_internal *clk_mgr,
					 unsigned int msg_id,
					 unsigned int param)
{
	uint32_t result;

	result = dcn314_smu_wait_for_response(clk_mgr, 10, 200000);

	if (result != VBIOSSMC_Result_OK)
		smu_print("SMU Response was not OK. SMU response after wait received is: %d\n",
				result);

	if (result == VBIOSSMC_Status_BUSY)
		return -1;

	 
	REG_WRITE(MP1_SMN_C2PMSG_91, VBIOSSMC_Status_BUSY);

	 
	REG_WRITE(MP1_SMN_C2PMSG_83, param);

	 
	REG_WRITE(MP1_SMN_C2PMSG_67, msg_id);

	result = dcn314_smu_wait_for_response(clk_mgr, 10, 200000);

	if (result == VBIOSSMC_Result_Failed) {
		if (msg_id == VBIOSSMC_MSG_TransferTableDram2Smu &&
		    param == TABLE_WATERMARKS)
			DC_LOG_DEBUG("Watermarks table not configured properly by SMU");
		else if (msg_id == VBIOSSMC_MSG_SetHardMinDcfclkByFreq ||
			 msg_id == VBIOSSMC_MSG_SetMinDeepSleepDcfclk)
			DC_LOG_WARNING("DCFCLK_DPM is not enabled by BIOS");
		else
			ASSERT(0);
		REG_WRITE(MP1_SMN_C2PMSG_91, VBIOSSMC_Result_OK);
		return -1;
	}

	if (IS_SMU_TIMEOUT(result)) {
		ASSERT(0);
		dm_helpers_smu_timeout(CTX, msg_id, param, 10 * 200000);
	}

	return REG_READ(MP1_SMN_C2PMSG_83);
}

int dcn314_smu_get_smu_version(struct clk_mgr_internal *clk_mgr)
{
	return dcn314_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_GetSmuVersion,
			0);
}


int dcn314_smu_set_dispclk(struct clk_mgr_internal *clk_mgr, int requested_dispclk_khz)
{
	int actual_dispclk_set_mhz = -1;

	if (!clk_mgr->smu_present)
		return requested_dispclk_khz;

	 
	actual_dispclk_set_mhz = dcn314_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDispclkFreq,
			khz_to_mhz_ceil(requested_dispclk_khz));

	return actual_dispclk_set_mhz * 1000;
}

int dcn314_smu_set_dprefclk(struct clk_mgr_internal *clk_mgr)
{
	int actual_dprefclk_set_mhz = -1;

	if (!clk_mgr->smu_present)
		return clk_mgr->base.dprefclk_khz;

	actual_dprefclk_set_mhz = dcn314_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDprefclkFreq,
			khz_to_mhz_ceil(clk_mgr->base.dprefclk_khz));

	 

	return actual_dprefclk_set_mhz * 1000;
}

int dcn314_smu_set_hard_min_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_dcfclk_khz)
{
	int actual_dcfclk_set_mhz = -1;

	if (!clk_mgr->base.ctx->dc->debug.pstate_enabled)
		return -1;

	if (!clk_mgr->smu_present)
		return requested_dcfclk_khz;

	actual_dcfclk_set_mhz = dcn314_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetHardMinDcfclkByFreq,
			khz_to_mhz_ceil(requested_dcfclk_khz));

#ifdef DBG
	smu_print("actual_dcfclk_set_mhz %d is set to : %d\n",
			actual_dcfclk_set_mhz,
			actual_dcfclk_set_mhz * 1000);
#endif

	return actual_dcfclk_set_mhz * 1000;
}

int dcn314_smu_set_min_deep_sleep_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_min_ds_dcfclk_khz)
{
	int actual_min_ds_dcfclk_mhz = -1;

	if (!clk_mgr->base.ctx->dc->debug.pstate_enabled)
		return -1;

	if (!clk_mgr->smu_present)
		return requested_min_ds_dcfclk_khz;

	actual_min_ds_dcfclk_mhz = dcn314_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetMinDeepSleepDcfclk,
			khz_to_mhz_ceil(requested_min_ds_dcfclk_khz));

	return actual_min_ds_dcfclk_mhz * 1000;
}

int dcn314_smu_set_dppclk(struct clk_mgr_internal *clk_mgr, int requested_dpp_khz)
{
	int actual_dppclk_set_mhz = -1;

	if (!clk_mgr->smu_present)
		return requested_dpp_khz;

	actual_dppclk_set_mhz = dcn314_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDppclkFreq,
			khz_to_mhz_ceil(requested_dpp_khz));

	return actual_dppclk_set_mhz * 1000;
}

void dcn314_smu_set_display_idle_optimization(struct clk_mgr_internal *clk_mgr, uint32_t idle_info)
{
	if (!clk_mgr->base.ctx->dc->debug.pstate_enabled)
		return;

	if (!clk_mgr->smu_present)
		return;

	
	dcn314_smu_send_msg_with_param(
		clk_mgr,
		VBIOSSMC_MSG_SetDisplayIdleOptimizations,
		idle_info);
}

void dcn314_smu_enable_phy_refclk_pwrdwn(struct clk_mgr_internal *clk_mgr, bool enable)
{
	union display_idle_optimization_u idle_info = { 0 };

	if (!clk_mgr->smu_present)
		return;

	if (enable) {
		idle_info.idle_info.df_request_disabled = 1;
		idle_info.idle_info.phy_ref_clk_off = 1;
	}

	dcn314_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDisplayIdleOptimizations,
			idle_info.data);
}

void dcn314_smu_enable_pme_wa(struct clk_mgr_internal *clk_mgr)
{
	if (!clk_mgr->smu_present)
		return;

	dcn314_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_UpdatePmeRestore,
			0);
}

void dcn314_smu_set_dram_addr_high(struct clk_mgr_internal *clk_mgr, uint32_t addr_high)
{
	if (!clk_mgr->smu_present)
		return;

	dcn314_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_SetVbiosDramAddrHigh, addr_high);
}

void dcn314_smu_set_dram_addr_low(struct clk_mgr_internal *clk_mgr, uint32_t addr_low)
{
	if (!clk_mgr->smu_present)
		return;

	dcn314_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_SetVbiosDramAddrLow, addr_low);
}

void dcn314_smu_transfer_dpm_table_smu_2_dram(struct clk_mgr_internal *clk_mgr)
{
	if (!clk_mgr->smu_present)
		return;

	dcn314_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_TransferTableSmu2Dram, TABLE_DPMCLOCKS);
}

void dcn314_smu_transfer_wm_table_dram_2_smu(struct clk_mgr_internal *clk_mgr)
{
	if (!clk_mgr->smu_present)
		return;

	dcn314_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_TransferTableDram2Smu, TABLE_WATERMARKS);
}

void dcn314_smu_set_zstate_support(struct clk_mgr_internal *clk_mgr, enum dcn_zstate_support_state support)
{
	unsigned int msg_id, param;

	if (!clk_mgr->smu_present)
		return;

	switch (support) {

	case DCN_ZSTATE_SUPPORT_ALLOW:
		msg_id = VBIOSSMC_MSG_AllowZstatesEntry;
		param = (1 << 10) | (1 << 9) | (1 << 8);
		break;

	case DCN_ZSTATE_SUPPORT_DISALLOW:
		msg_id = VBIOSSMC_MSG_AllowZstatesEntry;
		param = 0;
		break;


	case DCN_ZSTATE_SUPPORT_ALLOW_Z10_ONLY:
		msg_id = VBIOSSMC_MSG_AllowZstatesEntry;
		param = (1 << 10);
		break;

	case DCN_ZSTATE_SUPPORT_ALLOW_Z8_Z10_ONLY:
		msg_id = VBIOSSMC_MSG_AllowZstatesEntry;
		param = (1 << 10) | (1 << 8);
		break;

	case DCN_ZSTATE_SUPPORT_ALLOW_Z8_ONLY:
		msg_id = VBIOSSMC_MSG_AllowZstatesEntry;
		param = (1 << 8);
		break;

	default: 
		msg_id = VBIOSSMC_MSG_AllowZstatesEntry;
		param = 0;
		break;
	}


	dcn314_smu_send_msg_with_param(
		clk_mgr,
		msg_id,
		param);

}

 
void dcn314_smu_set_dtbclk(struct clk_mgr_internal *clk_mgr, bool enable)
{
	if (!clk_mgr->smu_present)
		return;

	dcn314_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDtbClk,
			enable);
}
