 

#ifndef DAL_DC_DCN10_RV1_CLK_MGR_VBIOS_SMU_H_
#define DAL_DC_DCN10_RV1_CLK_MGR_VBIOS_SMU_H_

int rv1_vbios_smu_set_dispclk(struct clk_mgr_internal *clk_mgr, int requested_dispclk_khz);
int rv1_vbios_smu_set_dprefclk(struct clk_mgr_internal *clk_mgr);

#endif  
