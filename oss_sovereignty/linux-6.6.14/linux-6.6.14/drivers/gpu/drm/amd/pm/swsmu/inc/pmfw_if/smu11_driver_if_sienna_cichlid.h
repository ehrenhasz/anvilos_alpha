#ifndef __SMU11_DRIVER_IF_SIENNA_CICHLID_H__
#define __SMU11_DRIVER_IF_SIENNA_CICHLID_H__
#define SMU11_DRIVER_IF_VERSION 0x40
#define PPTABLE_Sienna_Cichlid_SMU_VERSION 7
#define NUM_GFXCLK_DPM_LEVELS  16
#define NUM_SMNCLK_DPM_LEVELS  2
#define NUM_SOCCLK_DPM_LEVELS  8
#define NUM_MP0CLK_DPM_LEVELS  2
#define NUM_DCLK_DPM_LEVELS    8
#define NUM_VCLK_DPM_LEVELS    8
#define NUM_DCEFCLK_DPM_LEVELS 8
#define NUM_PHYCLK_DPM_LEVELS  8
#define NUM_DISPCLK_DPM_LEVELS 8
#define NUM_PIXCLK_DPM_LEVELS  8
#define NUM_DTBCLK_DPM_LEVELS  8
#define NUM_UCLK_DPM_LEVELS    4 
#define NUM_MP1CLK_DPM_LEVELS  2
#define NUM_LINK_LEVELS        2
#define NUM_FCLK_DPM_LEVELS    8 
#define NUM_XGMI_LEVELS        2
#define NUM_XGMI_PSTATE_LEVELS 4
#define NUM_OD_FAN_MAX_POINTS  6
#define MAX_GFXCLK_DPM_LEVEL  (NUM_GFXCLK_DPM_LEVELS  - 1)
#define MAX_SMNCLK_DPM_LEVEL  (NUM_SMNCLK_DPM_LEVELS  - 1)
#define MAX_SOCCLK_DPM_LEVEL  (NUM_SOCCLK_DPM_LEVELS  - 1)
#define MAX_MP0CLK_DPM_LEVEL  (NUM_MP0CLK_DPM_LEVELS  - 1)
#define MAX_DCLK_DPM_LEVEL    (NUM_DCLK_DPM_LEVELS    - 1)
#define MAX_VCLK_DPM_LEVEL    (NUM_VCLK_DPM_LEVELS    - 1)
#define MAX_DCEFCLK_DPM_LEVEL (NUM_DCEFCLK_DPM_LEVELS - 1)
#define MAX_DISPCLK_DPM_LEVEL (NUM_DISPCLK_DPM_LEVELS - 1)
#define MAX_PIXCLK_DPM_LEVEL  (NUM_PIXCLK_DPM_LEVELS  - 1)
#define MAX_PHYCLK_DPM_LEVEL  (NUM_PHYCLK_DPM_LEVELS  - 1)
#define MAX_DTBCLK_DPM_LEVEL  (NUM_DTBCLK_DPM_LEVELS  - 1)
#define MAX_UCLK_DPM_LEVEL    (NUM_UCLK_DPM_LEVELS    - 1)
#define MAX_MP1CLK_DPM_LEVEL  (NUM_MP1CLK_DPM_LEVELS  - 1)
#define MAX_LINK_LEVEL        (NUM_LINK_LEVELS        - 1)
#define MAX_FCLK_DPM_LEVEL    (NUM_FCLK_DPM_LEVELS    - 1)
#define PPSMC_GeminiModeNone   0   
#define PPSMC_GeminiModeMaster 1   
#define PPSMC_GeminiModeSlave  2   
#define FEATURE_DPM_PREFETCHER_BIT      0
#define FEATURE_DPM_GFXCLK_BIT          1
#define FEATURE_DPM_GFX_GPO_BIT         2
#define FEATURE_DPM_UCLK_BIT            3
#define FEATURE_DPM_FCLK_BIT            4
#define FEATURE_DPM_SOCCLK_BIT          5
#define FEATURE_DPM_MP0CLK_BIT          6
#define FEATURE_DPM_LINK_BIT            7
#define FEATURE_DPM_DCEFCLK_BIT         8
#define FEATURE_DPM_XGMI_BIT            9
#define FEATURE_MEM_VDDCI_SCALING_BIT   10 
#define FEATURE_MEM_MVDD_SCALING_BIT    11
#define FEATURE_DS_GFXCLK_BIT           12
#define FEATURE_DS_SOCCLK_BIT           13
#define FEATURE_DS_FCLK_BIT             14
#define FEATURE_DS_LCLK_BIT             15
#define FEATURE_DS_DCEFCLK_BIT          16
#define FEATURE_DS_UCLK_BIT             17
#define FEATURE_GFX_ULV_BIT             18  
#define FEATURE_FW_DSTATE_BIT           19 
#define FEATURE_GFXOFF_BIT              20
#define FEATURE_BACO_BIT                21
#define FEATURE_MM_DPM_PG_BIT           22  
#define FEATURE_SPARE_23_BIT            23
#define FEATURE_PPT_BIT                 24
#define FEATURE_TDC_BIT                 25
#define FEATURE_APCC_PLUS_BIT           26
#define FEATURE_GTHR_BIT                27
#define FEATURE_ACDC_BIT                28
#define FEATURE_VR0HOT_BIT              29
#define FEATURE_VR1HOT_BIT              30  
#define FEATURE_FW_CTF_BIT              31
#define FEATURE_FAN_CONTROL_BIT         32
#define FEATURE_THERMAL_BIT             33
#define FEATURE_GFX_DCS_BIT             34
#define FEATURE_RM_BIT                  35
#define FEATURE_LED_DISPLAY_BIT         36
#define FEATURE_GFX_SS_BIT              37
#define FEATURE_OUT_OF_BAND_MONITOR_BIT 38
#define FEATURE_TEMP_DEPENDENT_VMIN_BIT 39
#define FEATURE_MMHUB_PG_BIT            40 
#define FEATURE_ATHUB_PG_BIT            41
#define FEATURE_APCC_DFLL_BIT           42
#define FEATURE_DF_SUPERV_BIT           43
#define FEATURE_RSMU_SMN_CG_BIT         44
#define FEATURE_DF_CSTATE_BIT           45
#define FEATURE_2_STEP_PSTATE_BIT       46
#define FEATURE_SMNCLK_DPM_BIT          47
#define FEATURE_PERLINK_GMIDOWN_BIT     48
#define FEATURE_GFX_EDC_BIT             49
#define FEATURE_GFX_PER_PART_VMIN_BIT   50
#define FEATURE_SMART_SHIFT_BIT         51
#define FEATURE_APT_BIT                 52
#define FEATURE_SPARE_53_BIT            53
#define FEATURE_SPARE_54_BIT            54
#define FEATURE_SPARE_55_BIT            55
#define FEATURE_SPARE_56_BIT            56
#define FEATURE_SPARE_57_BIT            57
#define FEATURE_SPARE_58_BIT            58
#define FEATURE_SPARE_59_BIT            59
#define FEATURE_SPARE_60_BIT            60
#define FEATURE_SPARE_61_BIT            61
#define FEATURE_SPARE_62_BIT            62
#define FEATURE_SPARE_63_BIT            63
#define NUM_FEATURES                    64
typedef enum {
  FEATURE_PWR_ALL,
  FEATURE_PWR_S5,
  FEATURE_PWR_BACO,
  FEATURE_PWR_SOC,
  FEATURE_PWR_GFX,
  FEATURE_PWR_DOMAIN_COUNT,
} FEATURE_PWR_DOMAIN_e;
#define DPM_OVERRIDE_DISABLE_FCLK_PID                0x00000001
#define DPM_OVERRIDE_DISABLE_UCLK_PID                0x00000002
#define DPM_OVERRIDE_DISABLE_VOLT_LINK_VCN_FCLK      0x00000004
#define DPM_OVERRIDE_ENABLE_FREQ_LINK_VCLK_FCLK      0x00000008
#define DPM_OVERRIDE_ENABLE_FREQ_LINK_DCLK_FCLK      0x00000010
#define DPM_OVERRIDE_ENABLE_FREQ_LINK_GFXCLK_SOCCLK  0x00000020
#define DPM_OVERRIDE_ENABLE_FREQ_LINK_GFXCLK_UCLK    0x00000040
#define DPM_OVERRIDE_DISABLE_VOLT_LINK_DCE_FCLK      0x00000080
#define DPM_OVERRIDE_DISABLE_VOLT_LINK_MP0_SOCCLK    0x00000100
#define DPM_OVERRIDE_DISABLE_DFLL_PLL_SHUTDOWN       0x00000200
#define DPM_OVERRIDE_DISABLE_MEMORY_TEMPERATURE_READ 0x00000400
#define DPM_OVERRIDE_DISABLE_VOLT_LINK_VCN_DCEFCLK   0x00000800
#define DPM_OVERRIDE_DISABLE_FAST_FCLK_TIMER         0x00001000
#define DPM_OVERRIDE_DISABLE_VCN_PG                  0x00002000
#define DPM_OVERRIDE_DISABLE_FMAX_VMAX               0x00004000
#define DPM_OVERRIDE_ENABLE_eGPU_USB_WA              0x00008000
#define VR_MAPPING_VR_SELECT_MASK  0x01
#define VR_MAPPING_VR_SELECT_SHIFT 0x00
#define VR_MAPPING_PLANE_SELECT_MASK  0x02
#define VR_MAPPING_PLANE_SELECT_SHIFT 0x01
#define PSI_SEL_VR0_PLANE0_PSI0  0x01
#define PSI_SEL_VR0_PLANE0_PSI1  0x02
#define PSI_SEL_VR0_PLANE1_PSI0  0x04
#define PSI_SEL_VR0_PLANE1_PSI1  0x08
#define PSI_SEL_VR1_PLANE0_PSI0  0x10
#define PSI_SEL_VR1_PLANE0_PSI1  0x20
#define PSI_SEL_VR1_PLANE1_PSI0  0x40
#define PSI_SEL_VR1_PLANE1_PSI1  0x80
#define THROTTLER_PADDING_BIT      0
#define THROTTLER_TEMP_EDGE_BIT    1
#define THROTTLER_TEMP_HOTSPOT_BIT 2
#define THROTTLER_TEMP_MEM_BIT     3
#define THROTTLER_TEMP_VR_GFX_BIT  4
#define THROTTLER_TEMP_VR_MEM0_BIT 5
#define THROTTLER_TEMP_VR_MEM1_BIT 6
#define THROTTLER_TEMP_VR_SOC_BIT  7
#define THROTTLER_TEMP_LIQUID0_BIT 8
#define THROTTLER_TEMP_LIQUID1_BIT 9
#define THROTTLER_TEMP_PLX_BIT     10
#define THROTTLER_TDC_GFX_BIT      11
#define THROTTLER_TDC_SOC_BIT      12
#define THROTTLER_PPT0_BIT         13
#define THROTTLER_PPT1_BIT         14
#define THROTTLER_PPT2_BIT         15
#define THROTTLER_PPT3_BIT         16
#define THROTTLER_FIT_BIT          17
#define THROTTLER_PPM_BIT          18
#define THROTTLER_APCC_BIT         19
#define THROTTLER_COUNT            20
#define FW_DSTATE_SOC_ULV_BIT               0
#define FW_DSTATE_G6_HSR_BIT                1
#define FW_DSTATE_G6_PHY_VDDCI_OFF_BIT      2
#define FW_DSTATE_MP0_DS_BIT                3
#define FW_DSTATE_SMN_DS_BIT                4
#define FW_DSTATE_MP1_DS_BIT                5
#define FW_DSTATE_MP1_WHISPER_MODE_BIT      6
#define FW_DSTATE_SOC_LIV_MIN_BIT           7
#define FW_DSTATE_SOC_PLL_PWRDN_BIT         8
#define FW_DSTATE_MEM_PLL_PWRDN_BIT         9   
#define FW_DSTATE_OPTIMIZE_MALL_REFRESH_BIT 10
#define FW_DSTATE_MEM_PSI_BIT               11
#define FW_DSTATE_HSR_NON_STROBE_BIT        12
#define FW_DSTATE_MP0_ENTER_WFI_BIT         13
#define FW_DSTATE_SOC_ULV_MASK                    (1 << FW_DSTATE_SOC_ULV_BIT          )
#define FW_DSTATE_G6_HSR_MASK                     (1 << FW_DSTATE_G6_HSR_BIT           )
#define FW_DSTATE_G6_PHY_VDDCI_OFF_MASK           (1 << FW_DSTATE_G6_PHY_VDDCI_OFF_BIT )
#define FW_DSTATE_MP1_DS_MASK                     (1 << FW_DSTATE_MP1_DS_BIT           )  
#define FW_DSTATE_MP0_DS_MASK                     (1 << FW_DSTATE_MP0_DS_BIT           )   
#define FW_DSTATE_SMN_DS_MASK                     (1 << FW_DSTATE_SMN_DS_BIT           )
#define FW_DSTATE_MP1_WHISPER_MODE_MASK           (1 << FW_DSTATE_MP1_WHISPER_MODE_BIT )
#define FW_DSTATE_SOC_LIV_MIN_MASK                (1 << FW_DSTATE_SOC_LIV_MIN_BIT      )
#define FW_DSTATE_SOC_PLL_PWRDN_MASK              (1 << FW_DSTATE_SOC_PLL_PWRDN_BIT    )
#define FW_DSTATE_MEM_PLL_PWRDN_MASK              (1 << FW_DSTATE_MEM_PLL_PWRDN_BIT    )
#define FW_DSTATE_OPTIMIZE_MALL_REFRESH_MASK      (1 << FW_DSTATE_OPTIMIZE_MALL_REFRESH_BIT    )
#define FW_DSTATE_MEM_PSI_MASK                    (1 << FW_DSTATE_MEM_PSI_BIT    )
#define FW_DSTATE_HSR_NON_STROBE_MASK             (1 << FW_DSTATE_HSR_NON_STROBE_BIT    )
#define FW_DSTATE_MP0_ENTER_WFI_MASK              (1 << FW_DSTATE_MP0_ENTER_WFI_BIT    )
#define GFX_GPO_PACE_BIT                   0
#define GFX_GPO_DEM_BIT                    1
#define GFX_GPO_PACE_MASK                  (1 << GFX_GPO_PACE_BIT)
#define GFX_GPO_DEM_MASK                   (1 << GFX_GPO_DEM_BIT )
#define GPO_UPDATE_REQ_UCLKDPM_MASK  0x1
#define GPO_UPDATE_REQ_FCLKDPM_MASK  0x2
#define GPO_UPDATE_REQ_MALLHIT_MASK  0x4
#define LED_DISPLAY_GFX_DPM_BIT            0
#define LED_DISPLAY_PCIE_BIT               1
#define LED_DISPLAY_ERROR_BIT              2
#define RLC_PACE_TABLE_NUM_LEVELS          16
#define SIENNA_CICHLID_UMC_CHANNEL_NUM     16
typedef struct {
  uint64_t mca_umc_status;
  uint64_t mca_umc_addr;
  uint16_t ce_count_lo_chip;
  uint16_t ce_count_hi_chip;
  uint32_t eccPadding;
} EccInfo_t;
typedef struct {
  EccInfo_t  EccInfo[SIENNA_CICHLID_UMC_CHANNEL_NUM];
} EccInfoTable_t;
typedef enum {
  DRAM_BIT_WIDTH_DISABLED = 0,
  DRAM_BIT_WIDTH_X_8,
  DRAM_BIT_WIDTH_X_16,
  DRAM_BIT_WIDTH_X_32,
  DRAM_BIT_WIDTH_X_64,  
  DRAM_BIT_WIDTH_X_128,
  DRAM_BIT_WIDTH_COUNT,
} DRAM_BIT_WIDTH_TYPE_e;
#define NUM_I2C_CONTROLLERS                16
#define I2C_CONTROLLER_ENABLED             1
#define I2C_CONTROLLER_DISABLED            0
#define MAX_SW_I2C_COMMANDS                24
typedef enum {
  I2C_CONTROLLER_PORT_0 = 0,   
  I2C_CONTROLLER_PORT_1 = 1,   
  I2C_CONTROLLER_PORT_COUNT,
} I2cControllerPort_e;
typedef enum {
  I2C_CONTROLLER_NAME_VR_GFX = 0,
  I2C_CONTROLLER_NAME_VR_SOC,
  I2C_CONTROLLER_NAME_VR_VDDCI,
  I2C_CONTROLLER_NAME_VR_MVDD,
  I2C_CONTROLLER_NAME_LIQUID0,
  I2C_CONTROLLER_NAME_LIQUID1,  
  I2C_CONTROLLER_NAME_PLX,
  I2C_CONTROLLER_NAME_OTHER,
  I2C_CONTROLLER_NAME_COUNT,  
} I2cControllerName_e;
typedef enum {
  I2C_CONTROLLER_THROTTLER_TYPE_NONE = 0,
  I2C_CONTROLLER_THROTTLER_VR_GFX,
  I2C_CONTROLLER_THROTTLER_VR_SOC,
  I2C_CONTROLLER_THROTTLER_VR_VDDCI,
  I2C_CONTROLLER_THROTTLER_VR_MVDD,
  I2C_CONTROLLER_THROTTLER_LIQUID0,
  I2C_CONTROLLER_THROTTLER_LIQUID1,  
  I2C_CONTROLLER_THROTTLER_PLX,
  I2C_CONTROLLER_THROTTLER_INA3221,
  I2C_CONTROLLER_THROTTLER_COUNT,  
} I2cControllerThrottler_e;
typedef enum {
  I2C_CONTROLLER_PROTOCOL_VR_XPDE132G5,
  I2C_CONTROLLER_PROTOCOL_VR_IR35217,
  I2C_CONTROLLER_PROTOCOL_TMP_TMP102A,
  I2C_CONTROLLER_PROTOCOL_INA3221,
  I2C_CONTROLLER_PROTOCOL_COUNT,  
} I2cControllerProtocol_e;
typedef struct {
  uint8_t   Enabled;
  uint8_t   Speed;
  uint8_t   SlaveAddress;  
  uint8_t   ControllerPort;
  uint8_t   ControllerName;
  uint8_t   ThermalThrotter;
  uint8_t   I2cProtocol;
  uint8_t   PaddingConfig;  
} I2cControllerConfig_t;
typedef enum {
  I2C_PORT_SVD_SCL = 0,  
  I2C_PORT_GPIO,      
} I2cPort_e; 
typedef enum {
  I2C_SPEED_FAST_50K = 0,       
  I2C_SPEED_FAST_100K,          
  I2C_SPEED_FAST_400K,          
  I2C_SPEED_FAST_PLUS_1M,       
  I2C_SPEED_HIGH_1M,            
  I2C_SPEED_HIGH_2M,            
  I2C_SPEED_COUNT,  
} I2cSpeed_e;
typedef enum {
  I2C_CMD_READ = 0,
  I2C_CMD_WRITE,
  I2C_CMD_COUNT,  
} I2cCmdType_e;
typedef enum {
  FAN_MODE_AUTO = 0,
  FAN_MODE_MANUAL_LINEAR,
} FanMode_e;
#define CMDCONFIG_STOP_BIT             0
#define CMDCONFIG_RESTART_BIT          1
#define CMDCONFIG_READWRITE_BIT        2  
#define CMDCONFIG_STOP_MASK           (1 << CMDCONFIG_STOP_BIT)
#define CMDCONFIG_RESTART_MASK        (1 << CMDCONFIG_RESTART_BIT)
#define CMDCONFIG_READWRITE_MASK      (1 << CMDCONFIG_READWRITE_BIT)
typedef struct {
  uint8_t ReadWriteData;   
  uint8_t CmdConfig;  
} SwI2cCmd_t;  
typedef struct {
  uint8_t     I2CcontrollerPort;  
  uint8_t     I2CSpeed;           
  uint8_t     SlaveAddress;       
  uint8_t     NumCmds;            
  SwI2cCmd_t  SwI2cCmds[MAX_SW_I2C_COMMANDS];
} SwI2cRequest_t;  
typedef struct {
  SwI2cRequest_t SwI2cRequest;
  uint32_t Spare[8];
  uint32_t MmHubPadding[8];  
} SwI2cRequestExternal_t;
typedef enum {
  BACO_SEQUENCE,
  MSR_SEQUENCE,
  BAMACO_SEQUENCE,
  ULPS_SEQUENCE,
  D3HOT_SEQUENCE_COUNT,
} D3HOTSequence_e;
typedef enum {
  PG_DYNAMIC_MODE = 0,
  PG_STATIC_MODE,
} PowerGatingMode_e;
typedef enum {
  PG_POWER_DOWN = 0,
  PG_POWER_UP,
} PowerGatingSettings_e;
typedef struct {            
  uint32_t a;   
  uint32_t b;   
  uint32_t c;   
} QuadraticInt_t;
typedef struct {            
  uint32_t a;   
  uint32_t b;   
  uint32_t c;   
} QuadraticFixedPoint_t;
typedef struct {            
  uint32_t m;   
  uint32_t b;   
} LinearInt_t;
typedef struct {            
  uint32_t a;   
  uint32_t b;   
  uint32_t c;   
} DroopInt_t;
#define NUM_PIECE_WISE_LINEAR_DROOP_MODEL_VF_POINTS 5
typedef enum {
  PIECEWISE_LINEAR_FUSED_MODEL = 0,
  PIECEWISE_LINEAR_PP_MODEL,
  QUADRATIC_PP_MODEL,
  PERPART_PIECEWISE_LINEAR_PP_MODEL,  
} DfllDroopModelSelect_e;
typedef struct {
  uint32_t Fset[NUM_PIECE_WISE_LINEAR_DROOP_MODEL_VF_POINTS];     
  uint32_t Vdroop[NUM_PIECE_WISE_LINEAR_DROOP_MODEL_VF_POINTS];   
}PiecewiseLinearDroopInt_t;
typedef enum {
  GFXCLK_SOURCE_PLL = 0, 
  GFXCLK_SOURCE_DFLL, 
  GFXCLK_SOURCE_COUNT, 
} GFXCLK_SOURCE_e; 
typedef enum {
  PPCLK_GFXCLK = 0,
  PPCLK_SOCCLK,
  PPCLK_UCLK,
  PPCLK_FCLK,  
  PPCLK_DCLK_0,
  PPCLK_VCLK_0,
  PPCLK_DCLK_1,
  PPCLK_VCLK_1,
  PPCLK_DCEFCLK,
  PPCLK_DISPCLK,
  PPCLK_PIXCLK,
  PPCLK_PHYCLK,
  PPCLK_DTBCLK,
  PPCLK_COUNT,
} PPCLK_e;
typedef enum {
  VOLTAGE_MODE_AVFS = 0,
  VOLTAGE_MODE_AVFS_SS,
  VOLTAGE_MODE_SS,
  VOLTAGE_MODE_COUNT,
} VOLTAGE_MODE_e;
typedef enum {
  AVFS_VOLTAGE_GFX = 0,
  AVFS_VOLTAGE_SOC,
  AVFS_VOLTAGE_COUNT,
} AVFS_VOLTAGE_TYPE_e;
typedef enum {
  UCLK_DIV_BY_1 = 0,
  UCLK_DIV_BY_2,
  UCLK_DIV_BY_4,
  UCLK_DIV_BY_8,
} UCLK_DIV_e;
typedef enum {
  GPIO_INT_POLARITY_ACTIVE_LOW = 0,
  GPIO_INT_POLARITY_ACTIVE_HIGH,
} GpioIntPolarity_e;
typedef enum {
  PWR_CONFIG_TDP = 0,
  PWR_CONFIG_TGP,
  PWR_CONFIG_TCP_ESTIMATED,
  PWR_CONFIG_TCP_MEASURED,
} PwrConfig_e;
typedef enum {
  XGMI_LINK_RATE_2 = 2,     
  XGMI_LINK_RATE_4 = 4,     
  XGMI_LINK_RATE_8 = 8,     
  XGMI_LINK_RATE_12 = 12,   
  XGMI_LINK_RATE_16 = 16,   
  XGMI_LINK_RATE_17 = 17,   
  XGMI_LINK_RATE_18 = 18,   
  XGMI_LINK_RATE_19 = 19,   
  XGMI_LINK_RATE_20 = 20,   
  XGMI_LINK_RATE_21 = 21,   
  XGMI_LINK_RATE_22 = 22,   
  XGMI_LINK_RATE_23 = 23,   
  XGMI_LINK_RATE_24 = 24,   
  XGMI_LINK_RATE_25 = 25,   
  XGMI_LINK_RATE_COUNT
} XGMI_LINK_RATE_e;
typedef enum {
  XGMI_LINK_WIDTH_1 = 0,   
  XGMI_LINK_WIDTH_2,   
  XGMI_LINK_WIDTH_4,   
  XGMI_LINK_WIDTH_8,   
  XGMI_LINK_WIDTH_9,   
  XGMI_LINK_WIDTH_16,  
  XGMI_LINK_WIDTH_COUNT
} XGMI_LINK_WIDTH_e;
typedef struct {
  uint8_t        VoltageMode;          
  uint8_t        SnapToDiscrete;       
  uint8_t        NumDiscreteLevels;    
  uint8_t        Padding;         
  LinearInt_t    ConversionToAvfsClk;  
  QuadraticInt_t SsCurve;              
  uint16_t       SsFmin;               
  uint16_t       Padding16;    
} DpmDescriptor_t;
typedef enum  {
  PPT_THROTTLER_PPT0,
  PPT_THROTTLER_PPT1,
  PPT_THROTTLER_PPT2,
  PPT_THROTTLER_PPT3,       
  PPT_THROTTLER_COUNT
} PPT_THROTTLER_e;
typedef enum  {
  TEMP_EDGE,
  TEMP_HOTSPOT,
  TEMP_MEM,
  TEMP_VR_GFX,
  TEMP_VR_MEM0,
  TEMP_VR_MEM1,
  TEMP_VR_SOC,  
  TEMP_LIQUID0,
  TEMP_LIQUID1,  
  TEMP_PLX,
  TEMP_COUNT,
} TEMP_e;
typedef enum {
  TDC_THROTTLER_GFX,
  TDC_THROTTLER_SOC,
  TDC_THROTTLER_COUNT
} TDC_THROTTLER_e;
typedef enum {
  CUSTOMER_VARIANT_ROW,
  CUSTOMER_VARIANT_FALCON,
  CUSTOMER_VARIANT_COUNT,
} CUSTOMER_VARIANT_e;
typedef struct {
  uint16_t Fmin;
  uint16_t Fmax;
} UclkDpmChangeRange_t;
#pragma pack(push, 1)
typedef struct {
  uint32_t Version;
  uint32_t FeaturesToRun[NUM_FEATURES / 32];
  uint16_t SocketPowerLimitAc[PPT_THROTTLER_COUNT];  
  uint16_t SocketPowerLimitAcTau[PPT_THROTTLER_COUNT];  
  uint16_t SocketPowerLimitDc[PPT_THROTTLER_COUNT];   
  uint16_t SocketPowerLimitDcTau[PPT_THROTTLER_COUNT];   
  uint16_t TdcLimit[TDC_THROTTLER_COUNT];              
  uint16_t TdcLimitTau[TDC_THROTTLER_COUNT];           
  uint16_t TemperatureLimit[TEMP_COUNT];  
  uint32_t FitLimit;                 
  uint8_t      TotalPowerConfig;     
  uint8_t      TotalPowerPadding[3];  
  uint32_t     ApccPlusResidencyLimit;
  uint16_t       SmnclkDpmFreq        [NUM_SMNCLK_DPM_LEVELS];        
  uint16_t       SmnclkDpmVoltage     [NUM_SMNCLK_DPM_LEVELS];        
  uint32_t       PaddingAPCC;
  uint16_t       PerPartDroopVsetGfxDfll[NUM_PIECE_WISE_LINEAR_DROOP_MODEL_VF_POINTS];   
  uint16_t       PaddingPerPartDroop;
  uint32_t ThrottlerControlMask;    
  uint32_t FwDStateMask;            
  uint16_t  UlvVoltageOffsetSoc;  
  uint16_t  UlvVoltageOffsetGfx;  
  uint16_t     MinVoltageUlvGfx;  
  uint16_t     MinVoltageUlvSoc;  
  uint16_t     SocLIVmin;         
  uint16_t     PaddingLIVmin;
  uint8_t   GceaLinkMgrIdleThreshold;         
  uint8_t   paddingRlcUlvParams[3];
  uint16_t     MinVoltageGfx;      
  uint16_t     MinVoltageSoc;      
  uint16_t     MaxVoltageGfx;      
  uint16_t     MaxVoltageSoc;      
  uint16_t     LoadLineResistanceGfx;    
  uint16_t     LoadLineResistanceSoc;    
  uint16_t     VDDGFX_TVmin;        
  uint16_t     VDDSOC_TVmin;        
  uint16_t     VDDGFX_Vmin_HiTemp;  
  uint16_t     VDDGFX_Vmin_LoTemp;  
  uint16_t     VDDSOC_Vmin_HiTemp;  
  uint16_t     VDDSOC_Vmin_LoTemp;  
  uint16_t     VDDGFX_TVminHystersis;  
  uint16_t     VDDSOC_TVminHystersis;  
  DpmDescriptor_t DpmDescriptor[PPCLK_COUNT];
  uint16_t       FreqTableGfx      [NUM_GFXCLK_DPM_LEVELS  ];      
  uint16_t       FreqTableVclk     [NUM_VCLK_DPM_LEVELS    ];      
  uint16_t       FreqTableDclk     [NUM_DCLK_DPM_LEVELS    ];      
  uint16_t       FreqTableSocclk   [NUM_SOCCLK_DPM_LEVELS  ];      
  uint16_t       FreqTableUclk     [NUM_UCLK_DPM_LEVELS    ];      
  uint16_t       FreqTableDcefclk  [NUM_DCEFCLK_DPM_LEVELS ];      
  uint16_t       FreqTableDispclk  [NUM_DISPCLK_DPM_LEVELS ];      
  uint16_t       FreqTablePixclk   [NUM_PIXCLK_DPM_LEVELS  ];      
  uint16_t       FreqTablePhyclk   [NUM_PHYCLK_DPM_LEVELS  ];      
  uint16_t       FreqTableDtbclk   [NUM_DTBCLK_DPM_LEVELS  ];      
  uint16_t       FreqTableFclk     [NUM_FCLK_DPM_LEVELS    ];      
  uint32_t       Paddingclks;
  DroopInt_t     PerPartDroopModelGfxDfll[NUM_PIECE_WISE_LINEAR_DROOP_MODEL_VF_POINTS];  
  uint32_t       DcModeMaxFreq     [PPCLK_COUNT            ];      
  uint8_t        FreqTableUclkDiv  [NUM_UCLK_DPM_LEVELS    ];      
  uint16_t       FclkBoostFreq;                                    
  uint16_t       FclkParamPadding;
  uint16_t       Mp0clkFreq        [NUM_MP0CLK_DPM_LEVELS];        
  uint16_t       Mp0DpmVoltage     [NUM_MP0CLK_DPM_LEVELS];        
  uint16_t       MemVddciVoltage   [NUM_UCLK_DPM_LEVELS];          
  uint16_t       MemMvddVoltage    [NUM_UCLK_DPM_LEVELS];          
  uint16_t        GfxclkFgfxoffEntry;    
  uint16_t        GfxclkFinit;           
  uint16_t        GfxclkFidle;           
  uint8_t         GfxclkSource;          
  uint8_t         GfxclkPadding;
  uint8_t         GfxGpoSubFeatureMask;  
  uint8_t         GfxGpoEnabledWorkPolicyMask;  
  uint8_t         GfxGpoDisabledWorkPolicyMask;  
  uint8_t         GfxGpoPadding[1];
  uint32_t        GfxGpoVotingAllow;     
  uint32_t        GfxGpoPadding32[4];  
  uint16_t        GfxDcsFopt;            
  uint16_t        GfxDcsFclkFopt;        
  uint16_t        GfxDcsUclkFopt;        
  uint16_t        DcsGfxOffVoltage;      
  uint16_t        DcsMinGfxOffTime;      
  uint16_t        DcsMaxGfxOffTime;       
  uint32_t        DcsMinCreditAccum;     
  uint16_t        DcsExitHysteresis;     
  uint16_t        DcsTimeout;            
  uint32_t        DcsParamPadding[5];
  uint16_t        FlopsPerByteTable[RLC_PACE_TABLE_NUM_LEVELS];  
  uint8_t      LowestUclkReservedForUlv;  
  uint8_t      PaddingMem[3];
  uint8_t      UclkDpmPstates     [NUM_UCLK_DPM_LEVELS];      
  UclkDpmChangeRange_t UclkDpmSrcFreqRange;   
  UclkDpmChangeRange_t UclkDpmTargFreqRange;  
  uint16_t UclkDpmMidstepFreq;                
  uint16_t UclkMidstepPadding;
  uint8_t      PcieGenSpeed[NUM_LINK_LEVELS];            
  uint8_t      PcieLaneCount[NUM_LINK_LEVELS];           
  uint16_t     LclkFreq[NUM_LINK_LEVELS];              
  uint16_t     FanStopTemp;           
  uint16_t     FanStartTemp;          
  uint16_t     FanGain[TEMP_COUNT];
  uint16_t     FanPwmMin;
  uint16_t     FanAcousticLimitRpm;
  uint16_t     FanThrottlingRpm;
  uint16_t     FanMaximumRpm;
  uint16_t     MGpuFanBoostLimitRpm;  
  uint16_t     FanTargetTemperature;
  uint16_t     FanTargetGfxclk;
  uint16_t     FanPadding16;
  uint8_t      FanTempInputSelect;
  uint8_t      FanPadding;
  uint8_t      FanZeroRpmEnable; 
  uint8_t      FanTachEdgePerRev;
  int16_t      FuzzyFan_ErrorSetDelta;
  int16_t      FuzzyFan_ErrorRateSetDelta;
  int16_t      FuzzyFan_PwmSetDelta;
  uint16_t     FuzzyFan_Reserved;
  uint8_t           OverrideAvfsGb[AVFS_VOLTAGE_COUNT];
  uint8_t           dBtcGbGfxDfllModelSelect;   
  uint8_t           Padding8_Avfs;
  QuadraticInt_t    qAvfsGb[AVFS_VOLTAGE_COUNT];               
  DroopInt_t        dBtcGbGfxPll;          
  DroopInt_t        dBtcGbGfxDfll;         
  DroopInt_t        dBtcGbSoc;             
  LinearInt_t       qAgingGb[AVFS_VOLTAGE_COUNT];           
  PiecewiseLinearDroopInt_t   PiecewiseLinearDroopIntGfxDfll;  
  QuadraticInt_t    qStaticVoltageOffset[AVFS_VOLTAGE_COUNT];  
  uint16_t          DcTol[AVFS_VOLTAGE_COUNT];             
  uint8_t           DcBtcEnabled[AVFS_VOLTAGE_COUNT];
  uint8_t           Padding8_GfxBtc[2];
  uint16_t          DcBtcMin[AVFS_VOLTAGE_COUNT];        
  uint16_t          DcBtcMax[AVFS_VOLTAGE_COUNT];        
  uint16_t          DcBtcGb[AVFS_VOLTAGE_COUNT];        
  uint8_t           XgmiDpmPstates[NUM_XGMI_LEVELS];  
  uint8_t           XgmiDpmSpare[2];
  uint32_t          DebugOverrides;
  QuadraticInt_t    ReservedEquation0; 
  QuadraticInt_t    ReservedEquation1; 
  QuadraticInt_t    ReservedEquation2; 
  QuadraticInt_t    ReservedEquation3; 
  uint8_t          CustomerVariant;
  uint8_t          VcBtcEnabled;
  uint16_t         VcBtcVminT0;                  
  uint16_t         VcBtcFixedVminAgingOffset;    
  uint16_t         VcBtcVmin2PsmDegrationGb;     
  uint32_t         VcBtcPsmA;                    
  uint32_t         VcBtcPsmB;                    
  uint32_t         VcBtcVminA;                   
  uint32_t         VcBtcVminB;                   
  uint16_t         LedGpio;             
  uint16_t         GfxPowerStagesGpio;  
  uint32_t         SkuReserved[8];
  uint32_t     GamingClk[6];
  I2cControllerConfig_t  I2cControllers[NUM_I2C_CONTROLLERS];     
  uint8_t      GpioScl;   
  uint8_t      GpioSda;   
  uint8_t      FchUsbPdSlaveAddr;  
  uint8_t      I2cSpare[1];
  uint8_t      VddGfxVrMapping;    
  uint8_t      VddSocVrMapping;    
  uint8_t      VddMem0VrMapping;   
  uint8_t      VddMem1VrMapping;   
  uint8_t      GfxUlvPhaseSheddingMask;  
  uint8_t      SocUlvPhaseSheddingMask;  
  uint8_t      VddciUlvPhaseSheddingMask;  
  uint8_t      MvddUlvPhaseSheddingMask;  
  uint16_t     GfxMaxCurrent;    
  int8_t       GfxOffset;        
  uint8_t      Padding_TelemetryGfx;
  uint16_t     SocMaxCurrent;    
  int8_t       SocOffset;        
  uint8_t      Padding_TelemetrySoc;
  uint16_t     Mem0MaxCurrent;    
  int8_t       Mem0Offset;        
  uint8_t      Padding_TelemetryMem0;
  uint16_t     Mem1MaxCurrent;    
  int8_t       Mem1Offset;        
  uint8_t      Padding_TelemetryMem1;
  uint32_t     MvddRatio;  
  uint8_t      AcDcGpio;         
  uint8_t      AcDcPolarity;     
  uint8_t      VR0HotGpio;       
  uint8_t      VR0HotPolarity;   
  uint8_t      VR1HotGpio;       
  uint8_t      VR1HotPolarity;   
  uint8_t      GthrGpio;         
  uint8_t      GthrPolarity;     
  uint8_t      LedPin0;          
  uint8_t      LedPin1;          
  uint8_t      LedPin2;          
  uint8_t      LedEnableMask;
  uint8_t      LedPcie;         
  uint8_t      LedError;        
  uint8_t      LedSpare1[2];
  uint8_t      PllGfxclkSpreadEnabled;    
  uint8_t      PllGfxclkSpreadPercent;    
  uint16_t     PllGfxclkSpreadFreq;       
  uint8_t      DfllGfxclkSpreadEnabled;    
  uint8_t      DfllGfxclkSpreadPercent;    
  uint16_t     DfllGfxclkSpreadFreq;       
  uint16_t     UclkSpreadPadding;
  uint16_t     UclkSpreadFreq;       
  uint8_t      FclkSpreadEnabled;    
  uint8_t      FclkSpreadPercent;    
  uint16_t     FclkSpreadFreq;       
  uint32_t     MemoryChannelEnabled;  
  uint8_t      DramBitWidth;  
  uint8_t      PaddingMem1[3];
  uint16_t     TotalBoardPower;      
  uint16_t     BoardPowerPadding; 
  uint8_t      XgmiLinkSpeed   [NUM_XGMI_PSTATE_LEVELS];
  uint8_t      XgmiLinkWidth   [NUM_XGMI_PSTATE_LEVELS];
  uint16_t     XgmiFclkFreq    [NUM_XGMI_PSTATE_LEVELS];
  uint16_t     XgmiSocVoltage  [NUM_XGMI_PSTATE_LEVELS];
  uint8_t      HsrEnabled;
  uint8_t      VddqOffEnabled;
  uint8_t      PaddingUmcFlags[2];
  uint8_t      UclkSpreadPercent[16];   
  uint32_t     BoardReserved[11];
  uint32_t     MmHubPadding[8];  
} PPTable_t;
#pragma pack(pop)
typedef struct {
  uint32_t Version;
  uint32_t FeaturesToRun[NUM_FEATURES / 32];
  uint16_t SocketPowerLimitAc[PPT_THROTTLER_COUNT];  
  uint16_t SocketPowerLimitAcTau[PPT_THROTTLER_COUNT];  
  uint16_t SocketPowerLimitDc[PPT_THROTTLER_COUNT];   
  uint16_t SocketPowerLimitDcTau[PPT_THROTTLER_COUNT];   
  uint16_t TdcLimit[TDC_THROTTLER_COUNT];              
  uint16_t TdcLimitTau[TDC_THROTTLER_COUNT];           
  uint16_t TemperatureLimit[TEMP_COUNT];  
  uint32_t FitLimit;                 
  uint8_t      TotalPowerConfig;     
  uint8_t      TotalPowerPadding[3];  
  uint32_t     ApccPlusResidencyLimit;
  uint16_t       SmnclkDpmFreq        [NUM_SMNCLK_DPM_LEVELS];        
  uint16_t       SmnclkDpmVoltage     [NUM_SMNCLK_DPM_LEVELS];        
  uint32_t       PaddingAPCC;
  uint16_t       PerPartDroopVsetGfxDfll[NUM_PIECE_WISE_LINEAR_DROOP_MODEL_VF_POINTS];   
  uint16_t       PaddingPerPartDroop;
  uint32_t ThrottlerControlMask;    
  uint32_t FwDStateMask;            
  uint16_t  UlvVoltageOffsetSoc;  
  uint16_t  UlvVoltageOffsetGfx;  
  uint16_t     MinVoltageUlvGfx;  
  uint16_t     MinVoltageUlvSoc;  
  uint16_t     SocLIVmin;
  uint16_t     SocLIVminoffset;
  uint8_t   GceaLinkMgrIdleThreshold;         
  uint8_t   paddingRlcUlvParams[3];
  uint16_t     MinVoltageGfx;      
  uint16_t     MinVoltageSoc;      
  uint16_t     MaxVoltageGfx;      
  uint16_t     MaxVoltageSoc;      
  uint16_t     LoadLineResistanceGfx;    
  uint16_t     LoadLineResistanceSoc;    
  uint16_t     VDDGFX_TVmin;        
  uint16_t     VDDSOC_TVmin;        
  uint16_t     VDDGFX_Vmin_HiTemp;  
  uint16_t     VDDGFX_Vmin_LoTemp;  
  uint16_t     VDDSOC_Vmin_HiTemp;  
  uint16_t     VDDSOC_Vmin_LoTemp;  
  uint16_t     VDDGFX_TVminHystersis;  
  uint16_t     VDDSOC_TVminHystersis;  
  DpmDescriptor_t DpmDescriptor[PPCLK_COUNT];
  uint16_t       FreqTableGfx      [NUM_GFXCLK_DPM_LEVELS  ];      
  uint16_t       FreqTableVclk     [NUM_VCLK_DPM_LEVELS    ];      
  uint16_t       FreqTableDclk     [NUM_DCLK_DPM_LEVELS    ];      
  uint16_t       FreqTableSocclk   [NUM_SOCCLK_DPM_LEVELS  ];      
  uint16_t       FreqTableUclk     [NUM_UCLK_DPM_LEVELS    ];      
  uint16_t       FreqTableDcefclk  [NUM_DCEFCLK_DPM_LEVELS ];      
  uint16_t       FreqTableDispclk  [NUM_DISPCLK_DPM_LEVELS ];      
  uint16_t       FreqTablePixclk   [NUM_PIXCLK_DPM_LEVELS  ];      
  uint16_t       FreqTablePhyclk   [NUM_PHYCLK_DPM_LEVELS  ];      
  uint16_t       FreqTableDtbclk   [NUM_DTBCLK_DPM_LEVELS  ];      
  uint16_t       FreqTableFclk     [NUM_FCLK_DPM_LEVELS    ];      
  uint32_t       Paddingclks;
  DroopInt_t     PerPartDroopModelGfxDfll[NUM_PIECE_WISE_LINEAR_DROOP_MODEL_VF_POINTS];  
  uint32_t       DcModeMaxFreq     [PPCLK_COUNT            ];      
  uint8_t        FreqTableUclkDiv  [NUM_UCLK_DPM_LEVELS    ];      
  uint16_t       FclkBoostFreq;                                    
  uint16_t       FclkParamPadding;
  uint16_t       Mp0clkFreq        [NUM_MP0CLK_DPM_LEVELS];        
  uint16_t       Mp0DpmVoltage     [NUM_MP0CLK_DPM_LEVELS];        
  uint16_t       MemVddciVoltage   [NUM_UCLK_DPM_LEVELS];          
  uint16_t       MemMvddVoltage    [NUM_UCLK_DPM_LEVELS];          
  uint16_t        GfxclkFgfxoffEntry;    
  uint16_t        GfxclkFinit;           
  uint16_t        GfxclkFidle;           
  uint8_t         GfxclkSource;          
  uint8_t         GfxclkPadding;
  uint8_t         GfxGpoSubFeatureMask;  
  uint8_t         GfxGpoEnabledWorkPolicyMask;  
  uint8_t         GfxGpoDisabledWorkPolicyMask;  
  uint8_t         GfxGpoPadding[1];
  uint32_t        GfxGpoVotingAllow;     
  uint32_t        GfxGpoPadding32[4];
  uint16_t        GfxDcsFopt;            
  uint16_t        GfxDcsFclkFopt;        
  uint16_t        GfxDcsUclkFopt;        
  uint16_t        DcsGfxOffVoltage;      
  uint16_t        DcsMinGfxOffTime;      
  uint16_t        DcsMaxGfxOffTime;       
  uint32_t        DcsMinCreditAccum;     
  uint16_t        DcsExitHysteresis;     
  uint16_t        DcsTimeout;            
  uint32_t        DcsParamPadding[5];
  uint16_t        FlopsPerByteTable[RLC_PACE_TABLE_NUM_LEVELS];  
  uint8_t      LowestUclkReservedForUlv;  
  uint8_t      PaddingMem[3];
  uint8_t      UclkDpmPstates     [NUM_UCLK_DPM_LEVELS];      
  UclkDpmChangeRange_t UclkDpmSrcFreqRange;   
  UclkDpmChangeRange_t UclkDpmTargFreqRange;  
  uint16_t UclkDpmMidstepFreq;                
  uint16_t UclkMidstepPadding;
  uint8_t      PcieGenSpeed[NUM_LINK_LEVELS];            
  uint8_t      PcieLaneCount[NUM_LINK_LEVELS];           
  uint16_t     LclkFreq[NUM_LINK_LEVELS];              
  uint16_t     FanStopTemp;           
  uint16_t     FanStartTemp;          
  uint16_t     FanGain[TEMP_COUNT];
  uint16_t     FanPwmMin;
  uint16_t     FanAcousticLimitRpm;
  uint16_t     FanThrottlingRpm;
  uint16_t     FanMaximumRpm;
  uint16_t     MGpuFanBoostLimitRpm;  
  uint16_t     FanTargetTemperature;
  uint16_t     FanTargetGfxclk;
  uint16_t     FanPadding16;
  uint8_t      FanTempInputSelect;
  uint8_t      FanPadding;
  uint8_t      FanZeroRpmEnable; 
  uint8_t      FanTachEdgePerRev;
  int16_t      FuzzyFan_ErrorSetDelta;
  int16_t      FuzzyFan_ErrorRateSetDelta;
  int16_t      FuzzyFan_PwmSetDelta;
  uint16_t     FuzzyFan_Reserved;
  uint8_t           OverrideAvfsGb[AVFS_VOLTAGE_COUNT];
  uint8_t           dBtcGbGfxDfllModelSelect;   
  uint8_t           Padding8_Avfs;
  QuadraticInt_t    qAvfsGb[AVFS_VOLTAGE_COUNT];               
  DroopInt_t        dBtcGbGfxPll;          
  DroopInt_t        dBtcGbGfxDfll;         
  DroopInt_t        dBtcGbSoc;             
  LinearInt_t       qAgingGb[AVFS_VOLTAGE_COUNT];           
  PiecewiseLinearDroopInt_t   PiecewiseLinearDroopIntGfxDfll;  
  QuadraticInt_t    qStaticVoltageOffset[AVFS_VOLTAGE_COUNT];  
  uint16_t          DcTol[AVFS_VOLTAGE_COUNT];             
  uint8_t           DcBtcEnabled[AVFS_VOLTAGE_COUNT];
  uint8_t           Padding8_GfxBtc[2];
  uint16_t          DcBtcMin[AVFS_VOLTAGE_COUNT];        
  uint16_t          DcBtcMax[AVFS_VOLTAGE_COUNT];        
  uint16_t          DcBtcGb[AVFS_VOLTAGE_COUNT];        
  uint8_t           XgmiDpmPstates[NUM_XGMI_LEVELS];  
  uint8_t           XgmiDpmSpare[2];
  uint32_t          DebugOverrides;
  QuadraticInt_t    ReservedEquation0;
  QuadraticInt_t    ReservedEquation1;
  QuadraticInt_t    ReservedEquation2;
  QuadraticInt_t    ReservedEquation3;
  uint8_t          CustomerVariant;
  uint8_t          VcBtcEnabled;
  uint16_t         VcBtcVminT0;                  
  uint16_t         VcBtcFixedVminAgingOffset;    
  uint16_t         VcBtcVmin2PsmDegrationGb;     
  uint32_t         VcBtcPsmA;                    
  uint32_t         VcBtcPsmB;                    
  uint32_t         VcBtcVminA;                   
  uint32_t         VcBtcVminB;                   
  uint16_t         LedGpio;             
  uint16_t         GfxPowerStagesGpio;  
  uint32_t         SkuReserved[63];
  uint32_t     GamingClk[6];
  I2cControllerConfig_t  I2cControllers[NUM_I2C_CONTROLLERS];     
  uint8_t      GpioScl;   
  uint8_t      GpioSda;   
  uint8_t      FchUsbPdSlaveAddr;  
  uint8_t      I2cSpare[1];
  uint8_t      VddGfxVrMapping;    
  uint8_t      VddSocVrMapping;    
  uint8_t      VddMem0VrMapping;   
  uint8_t      VddMem1VrMapping;   
  uint8_t      GfxUlvPhaseSheddingMask;  
  uint8_t      SocUlvPhaseSheddingMask;  
  uint8_t      VddciUlvPhaseSheddingMask;  
  uint8_t      MvddUlvPhaseSheddingMask;  
  uint16_t     GfxMaxCurrent;    
  int8_t       GfxOffset;        
  uint8_t      Padding_TelemetryGfx;
  uint16_t     SocMaxCurrent;    
  int8_t       SocOffset;        
  uint8_t      Padding_TelemetrySoc;
  uint16_t     Mem0MaxCurrent;    
  int8_t       Mem0Offset;        
  uint8_t      Padding_TelemetryMem0;
  uint16_t     Mem1MaxCurrent;    
  int8_t       Mem1Offset;        
  uint8_t      Padding_TelemetryMem1;
  uint32_t     MvddRatio;  
  uint8_t      AcDcGpio;         
  uint8_t      AcDcPolarity;     
  uint8_t      VR0HotGpio;       
  uint8_t      VR0HotPolarity;   
  uint8_t      VR1HotGpio;       
  uint8_t      VR1HotPolarity;   
  uint8_t      GthrGpio;         
  uint8_t      GthrPolarity;     
  uint8_t      LedPin0;          
  uint8_t      LedPin1;          
  uint8_t      LedPin2;          
  uint8_t      LedEnableMask;
  uint8_t      LedPcie;         
  uint8_t      LedError;        
  uint8_t      LedSpare1[2];
  uint8_t      PllGfxclkSpreadEnabled;    
  uint8_t      PllGfxclkSpreadPercent;    
  uint16_t     PllGfxclkSpreadFreq;       
  uint8_t      DfllGfxclkSpreadEnabled;    
  uint8_t      DfllGfxclkSpreadPercent;    
  uint16_t     DfllGfxclkSpreadFreq;       
  uint16_t     UclkSpreadPadding;
  uint16_t     UclkSpreadFreq;       
  uint8_t      FclkSpreadEnabled;    
  uint8_t      FclkSpreadPercent;    
  uint16_t     FclkSpreadFreq;       
  uint32_t     MemoryChannelEnabled;  
  uint8_t      DramBitWidth;  
  uint8_t      PaddingMem1[3];
  uint16_t     TotalBoardPower;      
  uint16_t     BoardPowerPadding; 
  uint8_t      XgmiLinkSpeed   [NUM_XGMI_PSTATE_LEVELS];
  uint8_t      XgmiLinkWidth   [NUM_XGMI_PSTATE_LEVELS];
  uint16_t     XgmiFclkFreq    [NUM_XGMI_PSTATE_LEVELS];
  uint16_t     XgmiSocVoltage  [NUM_XGMI_PSTATE_LEVELS];
  uint8_t      HsrEnabled;
  uint8_t      VddqOffEnabled;
  uint8_t      PaddingUmcFlags[2];
  uint8_t      UclkSpreadPercent[16];   
  uint32_t     BoardReserved[11];
  uint32_t     MmHubPadding[8];  
} PPTable_beige_goby_t;
typedef struct {
  uint16_t     GfxclkAverageLpfTau;
  uint16_t     FclkAverageLpfTau;
  uint16_t     UclkAverageLpfTau;
  uint16_t     GfxActivityLpfTau;
  uint16_t     UclkActivityLpfTau;
  uint16_t     SocketPowerLpfTau;  
  uint16_t     VcnClkAverageLpfTau;
  uint16_t     padding16; 
} DriverSmuConfig_t;
typedef struct {
  DriverSmuConfig_t DriverSmuConfig;
  uint32_t     Spare[7];  
  uint32_t     MmHubPadding[8];  
} DriverSmuConfigExternal_t;
typedef struct {
  uint16_t               GfxclkFmin;            
  uint16_t               GfxclkFmax;            
  QuadraticInt_t         CustomGfxVfCurve;      
  uint16_t               CustomCurveFmin;       
  uint16_t               UclkFmin;              
  uint16_t               UclkFmax;              
  int16_t                OverDrivePct;          
  uint16_t               FanMaximumRpm;
  uint16_t               FanMinimumPwm;
  uint16_t               FanAcousticLimitRpm;
  uint16_t               FanTargetTemperature;  
  uint8_t                FanLinearPwmPoints[NUM_OD_FAN_MAX_POINTS];
  uint8_t                FanLinearTempPoints[NUM_OD_FAN_MAX_POINTS];
  uint16_t               MaxOpTemp;             
  int16_t                VddGfxOffset;          
  uint8_t                FanZeroRpmEnable;
  uint8_t                FanZeroRpmStopTemp;
  uint8_t                FanMode;
  uint8_t                Padding[1];
} OverDriveTable_t; 
typedef struct {
  OverDriveTable_t OverDriveTable;
  uint32_t      Spare[8];  
  uint32_t     MmHubPadding[8];  
} OverDriveTableExternal_t;
typedef struct {
  uint32_t CurrClock[PPCLK_COUNT];
  uint16_t AverageGfxclkFrequencyPreDs;
  uint16_t AverageGfxclkFrequencyPostDs;
  uint16_t AverageFclkFrequencyPreDs;
  uint16_t AverageFclkFrequencyPostDs;
  uint16_t AverageUclkFrequencyPreDs  ;
  uint16_t AverageUclkFrequencyPostDs  ;
  uint16_t AverageGfxActivity    ;
  uint16_t AverageUclkActivity   ;
  uint8_t  CurrSocVoltageOffset  ;
  uint8_t  CurrGfxVoltageOffset  ;
  uint8_t  CurrMemVidOffset      ;
  uint8_t  Padding8        ;
  uint16_t AverageSocketPower    ;
  uint16_t TemperatureEdge       ;
  uint16_t TemperatureHotspot    ;
  uint16_t TemperatureMem        ;
  uint16_t TemperatureVrGfx      ;
  uint16_t TemperatureVrMem0     ;
  uint16_t TemperatureVrMem1     ;  
  uint16_t TemperatureVrSoc      ;  
  uint16_t TemperatureLiquid0    ;
  uint16_t TemperatureLiquid1    ;  
  uint16_t TemperaturePlx        ;
  uint16_t Padding16             ;
  uint32_t ThrottlerStatus       ; 
  uint8_t  LinkDpmLevel;
  uint8_t  CurrFanPwm;
  uint16_t CurrFanSpeed;
  uint8_t D3HotEntryCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint8_t D3HotExitCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint8_t ArmMsgReceivedCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint32_t EnergyAccumulator;
  uint16_t AverageVclk0Frequency  ;
  uint16_t AverageDclk0Frequency  ;  
  uint16_t AverageVclk1Frequency  ;
  uint16_t AverageDclk1Frequency  ;  
  uint16_t VcnActivityPercentage  ;  
  uint8_t  PcieRate               ;
  uint8_t  PcieWidth              ;
  uint16_t AverageGfxclkFrequencyTarget;
  uint16_t Padding16_2;
} SmuMetrics_t;
typedef struct {
  uint32_t CurrClock[PPCLK_COUNT];
  uint16_t AverageGfxclkFrequencyPreDs;
  uint16_t AverageGfxclkFrequencyPostDs;
  uint16_t AverageFclkFrequencyPreDs;
  uint16_t AverageFclkFrequencyPostDs;
  uint16_t AverageUclkFrequencyPreDs  ;
  uint16_t AverageUclkFrequencyPostDs  ;
  uint16_t AverageGfxActivity    ;
  uint16_t AverageUclkActivity   ;
  uint8_t  CurrSocVoltageOffset  ;
  uint8_t  CurrGfxVoltageOffset  ;
  uint8_t  CurrMemVidOffset      ;
  uint8_t  Padding8        ;
  uint16_t AverageSocketPower    ;
  uint16_t TemperatureEdge       ;
  uint16_t TemperatureHotspot    ;
  uint16_t TemperatureMem        ;
  uint16_t TemperatureVrGfx      ;
  uint16_t TemperatureVrMem0     ;
  uint16_t TemperatureVrMem1     ;
  uint16_t TemperatureVrSoc      ;
  uint16_t TemperatureLiquid0    ;
  uint16_t TemperatureLiquid1    ;
  uint16_t TemperaturePlx        ;
  uint16_t Padding16             ;
  uint32_t AccCnt                ;
  uint8_t  ThrottlingPercentage[THROTTLER_COUNT];
  uint8_t  LinkDpmLevel;
  uint8_t  CurrFanPwm;
  uint16_t CurrFanSpeed;
  uint8_t D3HotEntryCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint8_t D3HotExitCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint8_t ArmMsgReceivedCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint32_t EnergyAccumulator;
  uint16_t AverageVclk0Frequency  ;
  uint16_t AverageDclk0Frequency  ;
  uint16_t AverageVclk1Frequency  ;
  uint16_t AverageDclk1Frequency  ;
  uint16_t VcnActivityPercentage  ;  
  uint8_t  PcieRate               ;
  uint8_t  PcieWidth              ;
  uint16_t AverageGfxclkFrequencyTarget;
  uint16_t Padding16_2;
} SmuMetrics_V2_t;
typedef struct {
  uint32_t CurrClock[PPCLK_COUNT];
  uint16_t AverageGfxclkFrequencyPreDs;
  uint16_t AverageGfxclkFrequencyPostDs;
  uint16_t AverageFclkFrequencyPreDs;
  uint16_t AverageFclkFrequencyPostDs;
  uint16_t AverageUclkFrequencyPreDs;
  uint16_t AverageUclkFrequencyPostDs;
  uint16_t AverageGfxActivity;
  uint16_t AverageUclkActivity;
  uint8_t  CurrSocVoltageOffset;
  uint8_t  CurrGfxVoltageOffset;
  uint8_t  CurrMemVidOffset;
  uint8_t  Padding8;
  uint16_t AverageSocketPower;
  uint16_t TemperatureEdge;
  uint16_t TemperatureHotspot;
  uint16_t TemperatureMem;
  uint16_t TemperatureVrGfx;
  uint16_t TemperatureVrMem0;
  uint16_t TemperatureVrMem1;
  uint16_t TemperatureVrSoc;
  uint16_t TemperatureLiquid0;
  uint16_t TemperatureLiquid1;
  uint16_t TemperaturePlx;
  uint16_t Padding16;
  uint32_t AccCnt;
  uint8_t  ThrottlingPercentage[THROTTLER_COUNT];
  uint8_t  LinkDpmLevel;
  uint8_t  CurrFanPwm;
  uint16_t CurrFanSpeed;
  uint8_t D3HotEntryCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint8_t D3HotExitCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint8_t ArmMsgReceivedCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint32_t EnergyAccumulator;
  uint16_t AverageVclk0Frequency;
  uint16_t AverageDclk0Frequency;
  uint16_t AverageVclk1Frequency;
  uint16_t AverageDclk1Frequency;
  uint16_t VcnUsagePercentage0;
  uint16_t VcnUsagePercentage1;
  uint8_t  PcieRate;
  uint8_t  PcieWidth;
  uint16_t AverageGfxclkFrequencyTarget;
  uint32_t PublicSerialNumLower32;
  uint32_t PublicSerialNumUpper32;
} SmuMetrics_V3_t;
typedef struct {
	uint32_t CurrClock[PPCLK_COUNT];
	uint16_t AverageGfxclkFrequencyPreDs;
	uint16_t AverageGfxclkFrequencyPostDs;
	uint16_t AverageFclkFrequencyPreDs;
	uint16_t AverageFclkFrequencyPostDs;
	uint16_t AverageUclkFrequencyPreDs;
	uint16_t AverageUclkFrequencyPostDs;
	uint16_t AverageGfxActivity;
	uint16_t AverageUclkActivity;
	uint8_t  CurrSocVoltageOffset;
	uint8_t  CurrGfxVoltageOffset;
	uint8_t  CurrMemVidOffset;
	uint8_t  Padding8;
	uint16_t AverageSocketPower;
	uint16_t TemperatureEdge;
	uint16_t TemperatureHotspot;
	uint16_t TemperatureMem;
	uint16_t TemperatureVrGfx;
	uint16_t TemperatureVrMem0;
	uint16_t TemperatureVrMem1;
	uint16_t TemperatureVrSoc;
	uint16_t TemperatureLiquid0;
	uint16_t TemperatureLiquid1;
	uint16_t TemperaturePlx;
	uint16_t Padding16;
	uint32_t AccCnt;
	uint8_t  ThrottlingPercentage[THROTTLER_COUNT];
	uint8_t  LinkDpmLevel;
	uint8_t  CurrFanPwm;
	uint16_t CurrFanSpeed;
	uint8_t D3HotEntryCountPerMode[D3HOT_SEQUENCE_COUNT];
	uint8_t D3HotExitCountPerMode[D3HOT_SEQUENCE_COUNT];
	uint8_t ArmMsgReceivedCountPerMode[D3HOT_SEQUENCE_COUNT];
	uint32_t EnergyAccumulator;
	uint16_t AverageVclk0Frequency;
	uint16_t AverageDclk0Frequency;
	uint16_t AverageVclk1Frequency;
	uint16_t AverageDclk1Frequency;
	uint16_t VcnUsagePercentage0;
	uint16_t VcnUsagePercentage1;
	uint8_t  PcieRate;
	uint8_t  PcieWidth;
	uint16_t AverageGfxclkFrequencyTarget;
	uint8_t  ApuSTAPMSmartShiftLimit;
	uint8_t  AverageApuSocketPower;
	uint8_t  ApuSTAPMLimit;
	uint8_t  Padding8_2;
} SmuMetrics_V4_t;
typedef struct {
  union {
    SmuMetrics_t SmuMetrics;
    SmuMetrics_V2_t SmuMetrics_V2;
    SmuMetrics_V3_t SmuMetrics_V3;
    SmuMetrics_V4_t SmuMetrics_V4;
  };
  uint32_t Spare[1];
  uint32_t     MmHubPadding[8];  
} SmuMetricsExternal_t;
typedef struct {
  uint16_t MinClock;  
  uint16_t MaxClock;  
  uint16_t MinUclk;
  uint16_t MaxUclk;
  uint8_t  WmSetting;
  uint8_t  Flags;
  uint8_t  Padding[2];
} WatermarkRowGeneric_t;
#define NUM_WM_RANGES 4
typedef enum {
  WM_SOCCLK = 0,
  WM_DCEFCLK,
  WM_COUNT,
} WM_CLOCK_e;
typedef enum {
  WATERMARKS_CLOCK_RANGE = 0,
  WATERMARKS_DUMMY_PSTATE,
  WATERMARKS_MALL,
  WATERMARKS_COUNT,
} WATERMARKS_FLAGS_e;
typedef struct {
  WatermarkRowGeneric_t WatermarkRow[WM_COUNT][NUM_WM_RANGES];
} Watermarks_t;
typedef struct {
  Watermarks_t Watermarks;
  uint32_t     MmHubPadding[8];  
} WatermarksExternal_t;
typedef struct {
  uint16_t avgPsmCount[67];
  uint16_t minPsmCount[67];
  float    avgPsmVoltage[67]; 
  float    minPsmVoltage[67];
} AvfsDebugTable_t;
typedef struct {
  AvfsDebugTable_t AvfsDebugTable;
  uint32_t     MmHubPadding[8];  
} AvfsDebugTableExternal_t;
typedef struct {
  uint8_t  AvfsVersion;
  uint8_t  Padding;
  uint8_t  AvfsEn[AVFS_VOLTAGE_COUNT];
  uint8_t  OverrideVFT[AVFS_VOLTAGE_COUNT];
  uint8_t  OverrideAvfsGb[AVFS_VOLTAGE_COUNT];
  uint8_t  OverrideTemperatures[AVFS_VOLTAGE_COUNT];
  uint8_t  OverrideVInversion[AVFS_VOLTAGE_COUNT];
  uint8_t  OverrideP2V[AVFS_VOLTAGE_COUNT];
  uint8_t  OverrideP2VCharzFreq[AVFS_VOLTAGE_COUNT];
  int32_t VFT0_m1[AVFS_VOLTAGE_COUNT];  
  int32_t VFT0_m2[AVFS_VOLTAGE_COUNT];  
  int32_t VFT0_b[AVFS_VOLTAGE_COUNT];   
  int32_t VFT1_m1[AVFS_VOLTAGE_COUNT];  
  int32_t VFT1_m2[AVFS_VOLTAGE_COUNT];  
  int32_t VFT1_b[AVFS_VOLTAGE_COUNT];   
  int32_t VFT2_m1[AVFS_VOLTAGE_COUNT];  
  int32_t VFT2_m2[AVFS_VOLTAGE_COUNT];  
  int32_t VFT2_b[AVFS_VOLTAGE_COUNT];   
  int32_t AvfsGb0_m1[AVFS_VOLTAGE_COUNT];  
  int32_t AvfsGb0_m2[AVFS_VOLTAGE_COUNT];  
  int32_t AvfsGb0_b[AVFS_VOLTAGE_COUNT];   
  int32_t AcBtcGb_m1[AVFS_VOLTAGE_COUNT];  
  int32_t AcBtcGb_m2[AVFS_VOLTAGE_COUNT];  
  int32_t AcBtcGb_b[AVFS_VOLTAGE_COUNT];   
  uint32_t AvfsTempCold[AVFS_VOLTAGE_COUNT];
  uint32_t AvfsTempMid[AVFS_VOLTAGE_COUNT];
  uint32_t AvfsTempHot[AVFS_VOLTAGE_COUNT];
  uint32_t VInversion[AVFS_VOLTAGE_COUNT];  
  int32_t P2V_m1[AVFS_VOLTAGE_COUNT];  
  int32_t P2V_m2[AVFS_VOLTAGE_COUNT];  
  int32_t P2V_b[AVFS_VOLTAGE_COUNT];   
  uint32_t P2VCharzFreq[AVFS_VOLTAGE_COUNT];  
  uint32_t EnabledAvfsModules[3];  
} AvfsFuseOverride_t;
typedef struct {
  AvfsFuseOverride_t AvfsFuseOverride;
  uint32_t     MmHubPadding[8];  
} AvfsFuseOverrideExternal_t;
typedef struct {
  uint8_t   Gfx_ActiveHystLimit;
  uint8_t   Gfx_IdleHystLimit;
  uint8_t   Gfx_FPS;
  uint8_t   Gfx_MinActiveFreqType;
  uint8_t   Gfx_BoosterFreqType; 
  uint8_t   Gfx_MinFreqStep;                 
  uint16_t  Gfx_MinActiveFreq;               
  uint16_t  Gfx_BoosterFreq;                 
  uint16_t  Gfx_PD_Data_time_constant;       
  uint32_t  Gfx_PD_Data_limit_a;             
  uint32_t  Gfx_PD_Data_limit_b;             
  uint32_t  Gfx_PD_Data_limit_c;             
  uint32_t  Gfx_PD_Data_error_coeff;         
  uint32_t  Gfx_PD_Data_error_rate_coeff;    
  uint8_t   Fclk_ActiveHystLimit;
  uint8_t   Fclk_IdleHystLimit;
  uint8_t   Fclk_FPS;
  uint8_t   Fclk_MinActiveFreqType;
  uint8_t   Fclk_BoosterFreqType; 
  uint8_t   Fclk_MinFreqStep;                 
  uint16_t  Fclk_MinActiveFreq;               
  uint16_t  Fclk_BoosterFreq;                 
  uint16_t  Fclk_PD_Data_time_constant;       
  uint32_t  Fclk_PD_Data_limit_a;             
  uint32_t  Fclk_PD_Data_limit_b;             
  uint32_t  Fclk_PD_Data_limit_c;             
  uint32_t  Fclk_PD_Data_error_coeff;         
  uint32_t  Fclk_PD_Data_error_rate_coeff;    
  uint8_t   Mem_ActiveHystLimit;
  uint8_t   Mem_IdleHystLimit;
  uint8_t   Mem_FPS;
  uint8_t   Mem_MinActiveFreqType;
  uint8_t   Mem_BoosterFreqType;
  uint8_t   Mem_MinFreqStep;                 
  uint16_t  Mem_MinActiveFreq;               
  uint16_t  Mem_BoosterFreq;                 
  uint16_t  Mem_PD_Data_time_constant;       
  uint32_t  Mem_PD_Data_limit_a;             
  uint32_t  Mem_PD_Data_limit_b;             
  uint32_t  Mem_PD_Data_limit_c;             
  uint32_t  Mem_PD_Data_error_coeff;         
  uint32_t  Mem_PD_Data_error_rate_coeff;    
  uint32_t  Mem_UpThreshold_Limit;           
  uint8_t   Mem_UpHystLimit;
  uint8_t   Mem_DownHystLimit;
  uint16_t  Mem_Fps;
} DpmActivityMonitorCoeffInt_t;
typedef struct {
  DpmActivityMonitorCoeffInt_t DpmActivityMonitorCoeffInt;
  uint32_t     MmHubPadding[8];  
} DpmActivityMonitorCoeffIntExternal_t;
#define WORKLOAD_PPLIB_DEFAULT_BIT        0 
#define WORKLOAD_PPLIB_FULL_SCREEN_3D_BIT 1 
#define WORKLOAD_PPLIB_POWER_SAVING_BIT   2 
#define WORKLOAD_PPLIB_VIDEO_BIT          3 
#define WORKLOAD_PPLIB_VR_BIT             4 
#define WORKLOAD_PPLIB_COMPUTE_BIT        5 
#define WORKLOAD_PPLIB_CUSTOM_BIT         6 
#define WORKLOAD_PPLIB_W3D_BIT            7 
#define WORKLOAD_PPLIB_COUNT              8 
#define TABLE_TRANSFER_OK         0x0
#define TABLE_TRANSFER_FAILED     0xFF
#define TABLE_PPTABLE                 0
#define TABLE_WATERMARKS              1
#define TABLE_AVFS_PSM_DEBUG          2
#define TABLE_AVFS_FUSE_OVERRIDE      3
#define TABLE_PMSTATUSLOG             4
#define TABLE_SMU_METRICS             5
#define TABLE_DRIVER_SMU_CONFIG       6
#define TABLE_ACTIVITY_MONITOR_COEFF  7
#define TABLE_OVERDRIVE               8
#define TABLE_I2C_COMMANDS            9
#define TABLE_PACE                   10
#define TABLE_ECCINFO                11
#define TABLE_COUNT                  12
typedef struct {
  float FlopsPerByteTable[RLC_PACE_TABLE_NUM_LEVELS];
} RlcPaceFlopsPerByteOverride_t;
typedef struct {
  RlcPaceFlopsPerByteOverride_t RlcPaceFlopsPerByteOverride;
  uint32_t     MmHubPadding[8];  
} RlcPaceFlopsPerByteOverrideExternal_t;
#define UCLK_SWITCH_SLOW 0
#define UCLK_SWITCH_FAST 1
#define UCLK_SWITCH_DUMMY 2
#endif
