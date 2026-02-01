 

#include "core_types.h"
#include "clk_mgr_internal.h"
#include "reg_helper.h"
#include "dm_helpers.h"
#include "dcn315_smu.h"
#include "mp/mp_13_0_5_offset.h"

#define MAX_INSTANCE                                        6
#define MAX_SEGMENT                                         6
#define SMU_REGISTER_WRITE_RETRY_COUNT                      5

struct IP_BASE_INSTANCE {
    unsigned int segment[MAX_SEGMENT];
};

struct IP_BASE {
    struct IP_BASE_INSTANCE instance[MAX_INSTANCE];
};

static const struct IP_BASE MP0_BASE = { { { { 0x00016000, 0x00DC0000, 0x00E00000, 0x00E40000, 0x0243FC00, 0 } },
					{ { 0, 0, 0, 0, 0, 0 } },
					{ { 0, 0, 0, 0, 0, 0 } },
					{ { 0, 0, 0, 0, 0, 0 } },
					{ { 0, 0, 0, 0, 0, 0 } },
					{ { 0, 0, 0, 0, 0, 0 } } } };
static const struct IP_BASE NBIO_BASE = { { { { 0x00000000, 0x00000014, 0x00000D20, 0x00010400, 0x0241B000, 0x04040000 } },
					{ { 0, 0, 0, 0, 0, 0 } },
					{ { 0, 0, 0, 0, 0, 0 } },
					{ { 0, 0, 0, 0, 0, 0 } },
					{ { 0, 0, 0, 0, 0, 0 } },
					{ { 0, 0, 0, 0, 0, 0 } } } };

#define regBIF_BX_PF2_RSMU_INDEX                                                                        0x0000
#define regBIF_BX_PF2_RSMU_INDEX_BASE_IDX                                                               1
#define regBIF_BX_PF2_RSMU_DATA                                                                         0x0001
#define regBIF_BX_PF2_RSMU_DATA_BASE_IDX                                                                1

#define REG(reg_name) \
	(MP0_BASE.instance[0].segment[reg ## reg_name ## _BASE_IDX] + reg ## reg_name)

#define FN(reg_name, field) \
	FD(reg_name##__##field)

#define REG_NBIO(reg_name) \
	(NBIO_BASE.instance[0].segment[regBIF_BX_PF2_ ## reg_name ## _BASE_IDX] + regBIF_BX_PF2_ ## reg_name)

#include "logger_types.h"
#undef DC_LOGGER
#define DC_LOGGER \
	CTX->logger
#define smu_print(str, ...) {DC_LOG_SMU(str, ##__VA_ARGS__); }

#define mmMP1_C2PMSG_3                            0x3B1050C

#define VBIOSSMC_MSG_TestMessage                  0x01 
#define VBIOSSMC_MSG_GetPmfwVersion               0x02 
#define VBIOSSMC_MSG_Spare0                       0x03 
#define VBIOSSMC_MSG_SetDispclkFreq               0x04 
#define VBIOSSMC_MSG_Spare1                       0x05 
#define VBIOSSMC_MSG_SetDppclkFreq                0x06 
#define VBIOSSMC_MSG_SetHardMinDcfclkByFreq       0x07 
#define VBIOSSMC_MSG_SetMinDeepSleepDcfclk        0x08 
#define VBIOSSMC_MSG_GetDtbclkFreq                0x09 
#define VBIOSSMC_MSG_SetDtbClk                    0x0A 
#define VBIOSSMC_MSG_SetDisplayCount              0x0B 
#define VBIOSSMC_MSG_EnableTmdp48MHzRefclkPwrDown 0x0C 
#define VBIOSSMC_MSG_UpdatePmeRestore             0x0D 
#define VBIOSSMC_MSG_SetVbiosDramAddrHigh         0x0E 
#define VBIOSSMC_MSG_SetVbiosDramAddrLow          0x0F 
#define VBIOSSMC_MSG_TransferTableSmu2Dram        0x10 
#define VBIOSSMC_MSG_TransferTableDram2Smu        0x11 
#define VBIOSSMC_MSG_SetDisplayIdleOptimizations  0x12 
#define VBIOSSMC_MSG_GetDprefclkFreq              0x13 
#define VBIOSSMC_Message_Count                    0x14 

#define VBIOSSMC_Status_BUSY                      0x0
#define VBIOSSMC_Result_OK                        0x01 
#define VBIOSSMC_Result_Failed                    0xFF 
#define VBIOSSMC_Result_UnknownCmd                0xFE 
#define VBIOSSMC_Result_CmdRejectedPrereq         0xFD 
#define VBIOSSMC_Result_CmdRejectedBusy           0xFC 

 
static uint32_t dcn315_smu_wait_for_response(struct clk_mgr_internal *clk_mgr, unsigned int delay_us, unsigned int max_retries)
{
	uint32_t res_val = VBIOSSMC_Status_BUSY;

	do {
		res_val = REG_READ(MP1_SMN_C2PMSG_38);
		if (res_val != VBIOSSMC_Status_BUSY)
			break;

		if (delay_us >= 1000)
			msleep(delay_us/1000);
		else if (delay_us > 0)
			udelay(delay_us);
	} while (max_retries--);

	return res_val;
}

static int dcn315_smu_send_msg_with_param(
		struct clk_mgr_internal *clk_mgr,
		unsigned int msg_id, unsigned int param)
{
	uint32_t result;
	uint32_t i = 0;
	uint32_t read_back_data;

	result = dcn315_smu_wait_for_response(clk_mgr, 10, 200000);

	if (result != VBIOSSMC_Result_OK)
		smu_print("SMU Response was not OK. SMU response after wait received is: %d\n", result);

	if (result == VBIOSSMC_Status_BUSY) {
		return -1;
	}

	 
	REG_WRITE(MP1_SMN_C2PMSG_38, VBIOSSMC_Status_BUSY);

	 
	REG_WRITE(MP1_SMN_C2PMSG_37, param);

	for (i = 0; i < SMU_REGISTER_WRITE_RETRY_COUNT; i++) {
		 
		generic_write_indirect_reg(CTX,
			REG_NBIO(RSMU_INDEX), REG_NBIO(RSMU_DATA),
			mmMP1_C2PMSG_3, msg_id);
		read_back_data = generic_read_indirect_reg(CTX,
			REG_NBIO(RSMU_INDEX), REG_NBIO(RSMU_DATA),
			mmMP1_C2PMSG_3);
		if (read_back_data == msg_id)
			break;
		udelay(2);
		smu_print("SMU msg id write fail %x times. \n", i + 1);
	}

	result = dcn315_smu_wait_for_response(clk_mgr, 10, 200000);

	if (result == VBIOSSMC_Status_BUSY) {
		ASSERT(0);
		dm_helpers_smu_timeout(CTX, msg_id, param, 10 * 200000);
	}

	return REG_READ(MP1_SMN_C2PMSG_37);
}

int dcn315_smu_get_smu_version(struct clk_mgr_internal *clk_mgr)
{
	return dcn315_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_GetPmfwVersion,
			0);
}


int dcn315_smu_set_dispclk(struct clk_mgr_internal *clk_mgr, int requested_dispclk_khz)
{
	int actual_dispclk_set_mhz = -1;

	if (!clk_mgr->smu_present)
		return requested_dispclk_khz;

	 
	actual_dispclk_set_mhz = dcn315_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDispclkFreq,
			khz_to_mhz_ceil(requested_dispclk_khz));

	return actual_dispclk_set_mhz * 1000;
}

int dcn315_smu_set_hard_min_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_dcfclk_khz)
{
	int actual_dcfclk_set_mhz = -1;

	if (!clk_mgr->base.ctx->dc->debug.pstate_enabled)
		return -1;

	if (!clk_mgr->smu_present)
		return requested_dcfclk_khz;

	actual_dcfclk_set_mhz = dcn315_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetHardMinDcfclkByFreq,
			khz_to_mhz_ceil(requested_dcfclk_khz));

#ifdef DBG
	smu_print("actual_dcfclk_set_mhz %d is set to : %d\n", actual_dcfclk_set_mhz, actual_dcfclk_set_mhz * 1000);
#endif

	return actual_dcfclk_set_mhz * 1000;
}

int dcn315_smu_set_min_deep_sleep_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_min_ds_dcfclk_khz)
{
	int actual_min_ds_dcfclk_mhz = -1;

	if (!clk_mgr->base.ctx->dc->debug.pstate_enabled)
		return -1;

	if (!clk_mgr->smu_present)
		return requested_min_ds_dcfclk_khz;

	actual_min_ds_dcfclk_mhz = dcn315_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetMinDeepSleepDcfclk,
			khz_to_mhz_ceil(requested_min_ds_dcfclk_khz));

	return actual_min_ds_dcfclk_mhz * 1000;
}

int dcn315_smu_set_dppclk(struct clk_mgr_internal *clk_mgr, int requested_dpp_khz)
{
	int actual_dppclk_set_mhz = -1;

	if (!clk_mgr->smu_present)
		return requested_dpp_khz;

	actual_dppclk_set_mhz = dcn315_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDppclkFreq,
			khz_to_mhz_ceil(requested_dpp_khz));

	return actual_dppclk_set_mhz * 1000;
}

void dcn315_smu_set_display_idle_optimization(struct clk_mgr_internal *clk_mgr, uint32_t idle_info)
{
	if (!clk_mgr->base.ctx->dc->debug.pstate_enabled)
		return;

	if (!clk_mgr->smu_present)
		return;

	
	dcn315_smu_send_msg_with_param(
		clk_mgr,
		VBIOSSMC_MSG_SetDisplayIdleOptimizations,
		idle_info);
}

void dcn315_smu_enable_phy_refclk_pwrdwn(struct clk_mgr_internal *clk_mgr, bool enable)
{
	union display_idle_optimization_u idle_info = { 0 };

	if (!clk_mgr->smu_present)
		return;

	if (enable) {
		idle_info.idle_info.df_request_disabled = 1;
		idle_info.idle_info.phy_ref_clk_off = 1;
	}

	dcn315_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDisplayIdleOptimizations,
			idle_info.data);
}

void dcn315_smu_enable_pme_wa(struct clk_mgr_internal *clk_mgr)
{
	if (!clk_mgr->smu_present)
		return;

	dcn315_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_UpdatePmeRestore,
			0);
}
void dcn315_smu_set_dram_addr_high(struct clk_mgr_internal *clk_mgr, uint32_t addr_high)
{
	if (!clk_mgr->smu_present)
		return;

	dcn315_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_SetVbiosDramAddrHigh, addr_high);
}

void dcn315_smu_set_dram_addr_low(struct clk_mgr_internal *clk_mgr, uint32_t addr_low)
{
	if (!clk_mgr->smu_present)
		return;

	dcn315_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_SetVbiosDramAddrLow, addr_low);
}

void dcn315_smu_transfer_dpm_table_smu_2_dram(struct clk_mgr_internal *clk_mgr)
{
	if (!clk_mgr->smu_present)
		return;

	dcn315_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_TransferTableSmu2Dram, TABLE_DPMCLOCKS);
}

void dcn315_smu_transfer_wm_table_dram_2_smu(struct clk_mgr_internal *clk_mgr)
{
	if (!clk_mgr->smu_present)
		return;

	dcn315_smu_send_msg_with_param(clk_mgr,
			VBIOSSMC_MSG_TransferTableDram2Smu, TABLE_WATERMARKS);
}

int dcn315_smu_get_dpref_clk(struct clk_mgr_internal *clk_mgr)
{
	int dprefclk_get_mhz = -1;
	if (clk_mgr->smu_present) {
		dprefclk_get_mhz = dcn315_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_GetDprefclkFreq,
			0);
	}
	return (dprefclk_get_mhz * 1000);
}

int dcn315_smu_get_dtbclk(struct clk_mgr_internal *clk_mgr)
{
	int fclk_get_mhz = -1;

	if (clk_mgr->smu_present) {
		fclk_get_mhz = dcn315_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_GetDtbclkFreq,
			0);
	}
	return (fclk_get_mhz * 1000);
}

void dcn315_smu_set_dtbclk(struct clk_mgr_internal *clk_mgr, bool enable)
{
	if (!clk_mgr->smu_present)
		return;

	dcn315_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDtbClk,
			enable);
}
