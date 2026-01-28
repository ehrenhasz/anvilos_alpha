#ifndef __SMU13_DRIVER_IF_V13_0_5_H__
#define __SMU13_DRIVER_IF_V13_0_5_H__
#define SMU13_0_5_DRIVER_IF_VERSION 5
#define THROTTLER_STATUS_BIT_SPL            0
#define THROTTLER_STATUS_BIT_FPPT           1
#define THROTTLER_STATUS_BIT_SPPT           2
#define THROTTLER_STATUS_BIT_SPPT_APU       3
#define THROTTLER_STATUS_BIT_THM_CORE       4
#define THROTTLER_STATUS_BIT_THM_GFX        5
#define THROTTLER_STATUS_BIT_THM_SOC        6
#define THROTTLER_STATUS_BIT_TDC_VDD        7
#define THROTTLER_STATUS_BIT_TDC_SOC        8
#define THROTTLER_STATUS_BIT_PROCHOT_CPU    9
#define THROTTLER_STATUS_BIT_PROCHOT_GFX   10
#define THROTTLER_STATUS_BIT_EDC_CPU       11
#define THROTTLER_STATUS_BIT_EDC_GFX       12
#define NUM_DCFCLK_DPM_LEVELS   4
#define NUM_DISPCLK_DPM_LEVELS  4
#define NUM_DPPCLK_DPM_LEVELS   4
#define NUM_SOCCLK_DPM_LEVELS   4
#define NUM_VCN_DPM_LEVELS      4
#define NUM_SOC_VOLTAGE_LEVELS  4
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
typedef struct {
  WatermarkRowGeneric_t WatermarkRow[WM_COUNT][NUM_WM_RANGES];
  uint32_t MmHubPadding[7];  
} Watermarks_t;
typedef struct {
  uint32_t FClk;
  uint32_t MemClk;
  uint32_t Voltage;
} DfPstateTable_t;
typedef struct {
  uint16_t GfxclkFrequency;              
  uint16_t SocclkFrequency;              
  uint16_t VclkFrequency;                
  uint16_t DclkFrequency;                
  uint16_t MemclkFrequency;              
  uint16_t spare;
  uint16_t GfxActivity;                  
  uint16_t UvdActivity;                  
  uint16_t Voltage[2];                   
  uint16_t Current[2];                   
  uint16_t Power[2];                     
  uint16_t GfxTemperature;               
  uint16_t SocTemperature;               
  uint16_t ThrottlerStatus;
  uint16_t CurrentSocketPower;           
} SmuMetrics_t;
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
} DpmClocks_t;
#define TABLE_BIOS_IF            0  
#define TABLE_WATERMARKS         1  
#define TABLE_CUSTOM_DPM         2  
#define TABLE_SPARE1             3
#define TABLE_DPMCLOCKS          4  
#define TABLE_MOMENTARY_PM       5  
#define TABLE_MODERN_STDBY       6  
#define TABLE_SMU_METRICS        7  
#define TABLE_COUNT              8
#endif
