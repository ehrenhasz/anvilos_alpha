 

#include "reg_helper.h"
#include "clk_mgr_internal.h"
#include "rv1_clk_mgr_clk.h"

#include "ip/Discovery/hwid.h"
#include "ip/Discovery/v1/ip_offset_1.h"
#include "ip/CLK/clk_10_0_default.h"
#include "ip/CLK/clk_10_0_offset.h"
#include "ip/CLK/clk_10_0_reg.h"
#include "ip/CLK/clk_10_0_sh_mask.h"

#include "dce100/dce_clk_mgr.h"

#define CLK_BASE_INNER(inst) \
	CLK_BASE__INST ## inst ## _SEG0


#define CLK_REG(reg_name, block, inst)\
	CLK_BASE(mm ## block ## _ ## inst ## _ ## reg_name ## _BASE_IDX) + \
					mm ## block ## _ ## inst ## _ ## reg_name

#define REG(reg_name) \
	CLK_REG(reg_name, CLK0, 0)


 
void rv1_dump_clk_registers(struct clk_state_registers *regs, struct clk_bypass *bypass, struct clk_mgr *clk_mgr_base)
{
	struct clk_mgr_internal *clk_mgr = TO_CLK_MGR_INTERNAL(clk_mgr_base);

		regs->CLK0_CLK8_CURRENT_CNT = REG_READ(CLK0_CLK8_CURRENT_CNT) / 10; 

		bypass->dcfclk_bypass = REG_READ(CLK0_CLK8_BYPASS_CNTL) & 0x0007;
		if (bypass->dcfclk_bypass < 0 || bypass->dcfclk_bypass > 4)
			bypass->dcfclk_bypass = 0;


		regs->CLK0_CLK8_DS_CNTL = REG_READ(CLK0_CLK8_DS_CNTL) / 10;	

		regs->CLK0_CLK8_ALLOW_DS = REG_READ(CLK0_CLK8_ALLOW_DS); 

		regs->CLK0_CLK10_CURRENT_CNT = REG_READ(CLK0_CLK10_CURRENT_CNT) / 10; 

		bypass->dispclk_pypass = REG_READ(CLK0_CLK10_BYPASS_CNTL) & 0x0007;
		if (bypass->dispclk_pypass < 0 || bypass->dispclk_pypass > 4)
			bypass->dispclk_pypass = 0;

		regs->CLK0_CLK11_CURRENT_CNT = REG_READ(CLK0_CLK11_CURRENT_CNT) / 10; 

		bypass->dprefclk_bypass = REG_READ(CLK0_CLK11_BYPASS_CNTL) & 0x0007;
		if (bypass->dprefclk_bypass < 0 || bypass->dprefclk_bypass > 4)
			bypass->dprefclk_bypass = 0;

}
