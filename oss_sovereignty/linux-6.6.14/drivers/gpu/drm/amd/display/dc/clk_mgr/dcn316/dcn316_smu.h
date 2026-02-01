 

#ifndef DAL_DC_316_SMU_H_
#define DAL_DC_316_SMU_H_
#include "os_types.h"

#define PMFW_DRIVER_IF_VERSION 4

#define NUM_DCFCLK_DPM_LEVELS   8
#define NUM_DISPCLK_DPM_LEVELS  8
#define NUM_DPPCLK_DPM_LEVELS   8
#define NUM_SOCCLK_DPM_LEVELS   8
#define NUM_VCN_DPM_LEVELS      8
#define NUM_SOC_VOLTAGE_LEVELS  8
#define NUM_DF_PSTATE_LEVELS    4

typedef struct {
  uint16_t MinClock; 
  uint16_t MaxClock; 
  uint16_t MinMclk;
  uint16_t MaxMclk;
  uint8_t  WmSetting;
  uint8_t  WmType;  
  uint8_t  Padding[2];
} WatermarkRowGeneric_t;

#define NUM_WM_RANGES 4
#define WM_PSTATE_CHG 0
#define WM_RETRAINING 1

typedef enum {
  WM_SOCCLK = 0,
  WM_DCFCLK,
  WM_COUNT,
} WM_CLOCK_e;

typedef enum{
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
} DfPstateTable_t;



typedef struct {
  uint32_t DcfClocks[NUM_DCFCLK_DPM_LEVELS];
  uint32_t DispClocks[NUM_DISPCLK_DPM_LEVELS];
  uint32_t DppClocks[NUM_DPPCLK_DPM_LEVELS];
  uint32_t SocClocks[NUM_SOCCLK_DPM_LEVELS];
  uint32_t VClocks[NUM_VCN_DPM_LEVELS];
  uint32_t DClocks[NUM_VCN_DPM_LEVELS];
  uint32_t SocVoltage[NUM_SOC_VOLTAGE_LEVELS];
  DfPstateTable_t DfPstateTable[NUM_DF_PSTATE_LEVELS];
  uint8_t  NumDcfClkLevelsEnabled;
  uint8_t  NumDispClkLevelsEnabled; 
  uint8_t  NumSocClkLevelsEnabled;
  uint8_t  VcnClkLevelsEnabled;     
  uint8_t  NumDfPstatesEnabled;
  uint8_t  spare[3];
  uint32_t MinGfxClk;
  uint32_t MaxGfxClk;
} DpmClocks_316_t;

struct dcn316_watermarks {
  
  WatermarkRowGeneric_t WatermarkRow[WM_COUNT][NUM_WM_RANGES];
  uint32_t MmHubPadding[7]; 
};

struct dcn316_smu_dpm_clks {
	DpmClocks_316_t *dpm_clks;
	union large_integer mc_address;
};

#define TABLE_WATERMARKS         1 
#define TABLE_DPMCLOCKS          4 

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

int dcn316_smu_get_smu_version(struct clk_mgr_internal *clk_mgr);
int dcn316_smu_set_dispclk(struct clk_mgr_internal *clk_mgr, int requested_dispclk_khz);
int dcn316_smu_set_hard_min_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_dcfclk_khz);
int dcn316_smu_set_min_deep_sleep_dcfclk(struct clk_mgr_internal *clk_mgr, int requested_min_ds_dcfclk_khz);
int dcn316_smu_set_dppclk(struct clk_mgr_internal *clk_mgr, int requested_dpp_khz);
void dcn316_smu_set_display_idle_optimization(struct clk_mgr_internal *clk_mgr, uint32_t idle_info);
void dcn316_smu_enable_phy_refclk_pwrdwn(struct clk_mgr_internal *clk_mgr, bool enable);
void dcn316_smu_set_dram_addr_high(struct clk_mgr_internal *clk_mgr, uint32_t addr_high);
void dcn316_smu_set_dram_addr_low(struct clk_mgr_internal *clk_mgr, uint32_t addr_low);
void dcn316_smu_transfer_dpm_table_smu_2_dram(struct clk_mgr_internal *clk_mgr);
void dcn316_smu_transfer_wm_table_dram_2_smu(struct clk_mgr_internal *clk_mgr);
void dcn316_smu_request_voltage_via_phyclk(struct clk_mgr_internal *clk_mgr, int requested_phyclk_khz);
void dcn316_smu_enable_pme_wa(struct clk_mgr_internal *clk_mgr);
void dcn316_smu_set_dtbclk(struct clk_mgr_internal *clk_mgr, bool enable);
int dcn316_smu_get_dpref_clk(struct clk_mgr_internal *clk_mgr);
int dcn316_smu_get_smu_fclk(struct clk_mgr_internal *clk_mgr);

#endif  
