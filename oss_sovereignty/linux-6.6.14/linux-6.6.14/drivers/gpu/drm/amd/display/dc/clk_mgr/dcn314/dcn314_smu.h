#ifndef DAL_DC_314_SMU_H_
#define DAL_DC_314_SMU_H_
#include "smu13_driver_if_v13_0_4.h"
typedef enum {
	WCK_RATIO_1_1 = 0,   
	WCK_RATIO_1_2,
	WCK_RATIO_1_4,
	WCK_RATIO_MAX
} WCK_RATIO_e;
typedef struct {
  uint32_t FClk;
  uint32_t MemClk;
  uint32_t Voltage;
  uint8_t  WckRatio;
  uint8_t  Spare[3];
} DfPstateTable314_t;
typedef struct {
  uint32_t DcfClocks[NUM_DCFCLK_DPM_LEVELS];
  uint32_t DispClocks[NUM_DISPCLK_DPM_LEVELS];
  uint32_t DppClocks[NUM_DPPCLK_DPM_LEVELS];
  uint32_t SocClocks[NUM_SOCCLK_DPM_LEVELS];
  uint32_t VClocks[NUM_VCN_DPM_LEVELS];
  uint32_t DClocks[NUM_VCN_DPM_LEVELS];
  uint32_t SocVoltage[NUM_SOC_VOLTAGE_LEVELS];
  DfPstateTable314_t DfPstateTable[NUM_DF_PSTATE_LEVELS];
  uint8_t  NumDcfClkLevelsEnabled;
  uint8_t  NumDispClkLevelsEnabled;  
  uint8_t  NumSocClkLevelsEnabled;
  uint8_t  VcnClkLevelsEnabled;      
  uint8_t  NumDfPstatesEnabled;
  uint8_t  spare[3];
  uint32_t MinGfxClk;
  uint32_t MaxGfxClk;
} DpmClocks314_t;
struct dcn314_watermarks {
	WatermarkRowGeneric_t WatermarkRow[WM_COUNT][NUM_WM_RANGES];
	uint32_t MmHubPadding[7];  
};
struct dcn314_smu_dpm_clks {
	DpmClocks314_t *dpm_clks;
	union large_integer mc_address;
};
struct display_idle_optimization {
	unsigned int df_request_disabled : 1;
	unsigned int phy_ref_clk_off     : 1;
	unsigned int s0i2_rdy            : 1;
	unsigned int reserved            : 29;
};
union display_idle_optimization_u {
	struct display_idle_optimization idle_info;
	uint32_t data;
};
int dcn314_smu_get_smu_version(struct clk_mgr_internal *clk_mgr);
int dcn314_smu_set_dispclk(struct clk_mgr_internal *clk_mgr, int requested_dispclk_khz);
int dcn314_smu_set_dprefclk(struct clk_mgr_internal *clk_mgr);
int dcn314_smu_set_hard_min_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_dcfclk_khz);
int dcn314_smu_set_min_deep_sleep_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_min_ds_dcfclk_khz);
int dcn314_smu_set_dppclk(struct clk_mgr_internal *clk_mgr, int requested_dpp_khz);
void dcn314_smu_set_display_idle_optimization(struct clk_mgr_internal *clk_mgr, uint32_t idle_info);
void dcn314_smu_enable_phy_refclk_pwrdwn(struct clk_mgr_internal *clk_mgr, bool enable);
void dcn314_smu_enable_pme_wa(struct clk_mgr_internal *clk_mgr);
void dcn314_smu_set_dram_addr_high(struct clk_mgr_internal *clk_mgr, uint32_t addr_high);
void dcn314_smu_set_dram_addr_low(struct clk_mgr_internal *clk_mgr, uint32_t addr_low);
void dcn314_smu_transfer_dpm_table_smu_2_dram(struct clk_mgr_internal *clk_mgr);
void dcn314_smu_transfer_wm_table_dram_2_smu(struct clk_mgr_internal *clk_mgr);
void dcn314_smu_set_zstate_support(struct clk_mgr_internal *clk_mgr, enum dcn_zstate_support_state support);
void dcn314_smu_set_dtbclk(struct clk_mgr_internal *clk_mgr, bool enable);
#endif  
