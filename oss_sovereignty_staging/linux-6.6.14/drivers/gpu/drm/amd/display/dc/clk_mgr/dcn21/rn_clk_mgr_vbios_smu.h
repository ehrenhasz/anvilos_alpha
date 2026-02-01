 

#ifndef DAL_DC_RN_CLK_MGR_VBIOS_SMU_H_
#define DAL_DC_RN_CLK_MGR_VBIOS_SMU_H_

enum dcn_pwr_state;

int rn_vbios_smu_get_smu_version(struct clk_mgr_internal *clk_mgr);
int rn_vbios_smu_set_dispclk(struct clk_mgr_internal *clk_mgr, int requested_dispclk_khz);
int rn_vbios_smu_set_dprefclk(struct clk_mgr_internal *clk_mgr);
int rn_vbios_smu_set_hard_min_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_dcfclk_khz);
int rn_vbios_smu_set_min_deep_sleep_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_min_ds_dcfclk_khz);
void rn_vbios_smu_set_phyclk(struct clk_mgr_internal *clk_mgr, int requested_phyclk_khz);
int rn_vbios_smu_set_dppclk(struct clk_mgr_internal *clk_mgr, int requested_dpp_khz);
void rn_vbios_smu_set_dcn_low_power_state(struct clk_mgr_internal *clk_mgr, enum dcn_pwr_state);
void rn_vbios_smu_enable_48mhz_tmdp_refclk_pwrdwn(struct clk_mgr_internal *clk_mgr, bool enable);
void rn_vbios_smu_enable_pme_wa(struct clk_mgr_internal *clk_mgr);
int rn_vbios_smu_is_periodic_retraining_disabled(struct clk_mgr_internal *clk_mgr);

#endif  
