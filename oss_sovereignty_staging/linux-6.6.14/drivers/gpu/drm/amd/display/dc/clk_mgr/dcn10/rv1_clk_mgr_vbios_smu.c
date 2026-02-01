 

#include "core_types.h"
#include "clk_mgr_internal.h"
#include "reg_helper.h"
#include <linux/delay.h>

#include "rv1_clk_mgr_vbios_smu.h"

#define MAX_INSTANCE	5
#define MAX_SEGMENT		5

struct IP_BASE_INSTANCE {
	unsigned int segment[MAX_SEGMENT];
};

struct IP_BASE {
	struct IP_BASE_INSTANCE instance[MAX_INSTANCE];
};


static const struct IP_BASE MP1_BASE  = { { { { 0x00016000, 0, 0, 0, 0 } },
											 { { 0, 0, 0, 0, 0 } },
											 { { 0, 0, 0, 0, 0 } },
											 { { 0, 0, 0, 0, 0 } },
											 { { 0, 0, 0, 0, 0 } } } };

#define mmMP1_SMN_C2PMSG_91            0x29B
#define mmMP1_SMN_C2PMSG_83            0x293
#define mmMP1_SMN_C2PMSG_67            0x283
#define mmMP1_SMN_C2PMSG_91_BASE_IDX   0
#define mmMP1_SMN_C2PMSG_83_BASE_IDX   0
#define mmMP1_SMN_C2PMSG_67_BASE_IDX   0

#define MP1_SMN_C2PMSG_91__CONTENT_MASK                    0xffffffffL
#define MP1_SMN_C2PMSG_83__CONTENT_MASK                    0xffffffffL
#define MP1_SMN_C2PMSG_67__CONTENT_MASK                    0xffffffffL
#define MP1_SMN_C2PMSG_91__CONTENT__SHIFT                  0x00000000
#define MP1_SMN_C2PMSG_83__CONTENT__SHIFT                  0x00000000
#define MP1_SMN_C2PMSG_67__CONTENT__SHIFT                  0x00000000

#define REG(reg_name) \
	(MP1_BASE.instance[0].segment[mm ## reg_name ## _BASE_IDX] + mm ## reg_name)

#define FN(reg_name, field) \
	FD(reg_name##__##field)

#define VBIOSSMC_MSG_SetDispclkFreq           0x4
#define VBIOSSMC_MSG_SetDprefclkFreq          0x5

#define VBIOSSMC_Status_BUSY                      0x0
#define VBIOSSMC_Result_OK                        0x1
#define VBIOSSMC_Result_Failed                    0xFF
#define VBIOSSMC_Result_UnknownCmd                0xFE
#define VBIOSSMC_Result_CmdRejectedPrereq         0xFD
#define VBIOSSMC_Result_CmdRejectedBusy           0xFC

 
static uint32_t rv1_smu_wait_for_response(struct clk_mgr_internal *clk_mgr, unsigned int delay_us, unsigned int max_retries)
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

static int rv1_vbios_smu_send_msg_with_param(struct clk_mgr_internal *clk_mgr,
		unsigned int msg_id, unsigned int param)
{
	uint32_t result;

	 
	REG_WRITE(MP1_SMN_C2PMSG_91, VBIOSSMC_Status_BUSY);

	 
	REG_WRITE(MP1_SMN_C2PMSG_83, param);

	 
	REG_WRITE(MP1_SMN_C2PMSG_67, msg_id);

	result = rv1_smu_wait_for_response(clk_mgr, 10, 1000);

	ASSERT(result == VBIOSSMC_Result_OK);

	 
	return REG_READ(MP1_SMN_C2PMSG_83);
}

int rv1_vbios_smu_set_dispclk(struct clk_mgr_internal *clk_mgr, int requested_dispclk_khz)
{
	int actual_dispclk_set_mhz = -1;
	struct dc *dc = clk_mgr->base.ctx->dc;
	struct dmcu *dmcu = dc->res_pool->dmcu;

	 
	actual_dispclk_set_mhz = rv1_vbios_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDispclkFreq,
			khz_to_mhz_ceil(requested_dispclk_khz));

	if (dmcu && dmcu->funcs->is_dmcu_initialized(dmcu)) {
		if (clk_mgr->dfs_bypass_disp_clk != actual_dispclk_set_mhz)
			dmcu->funcs->set_psr_wait_loop(dmcu,
					actual_dispclk_set_mhz / 7);
	}

	return actual_dispclk_set_mhz * 1000;
}

int rv1_vbios_smu_set_dprefclk(struct clk_mgr_internal *clk_mgr)
{
	int actual_dprefclk_set_mhz = -1;

	actual_dprefclk_set_mhz = rv1_vbios_smu_send_msg_with_param(
			clk_mgr,
			VBIOSSMC_MSG_SetDprefclkFreq,
			khz_to_mhz_ceil(clk_mgr->base.dprefclk_khz));

	 

	return actual_dprefclk_set_mhz * 1000;
}
