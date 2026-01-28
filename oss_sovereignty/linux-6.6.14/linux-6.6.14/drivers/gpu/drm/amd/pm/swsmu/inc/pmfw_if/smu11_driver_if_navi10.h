#ifndef __SMU11_DRIVER_IF_NAVI10_H__
#define __SMU11_DRIVER_IF_NAVI10_H__
#define PPTABLE_NV10_SMU_VERSION 8
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
#define NUM_UCLK_DPM_LEVELS    4 
#define NUM_MP1CLK_DPM_LEVELS  2
#define NUM_LINK_LEVELS        2
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
#define MAX_UCLK_DPM_LEVEL    (NUM_UCLK_DPM_LEVELS    - 1)
#define MAX_MP1CLK_DPM_LEVEL  (NUM_MP1CLK_DPM_LEVELS  - 1)
#define MAX_LINK_LEVEL        (NUM_LINK_LEVELS        - 1)
#define PPSMC_GeminiModeNone   0   
#define PPSMC_GeminiModeMaster 1   
#define PPSMC_GeminiModeSlave  2   
#define FEATURE_DPM_PREFETCHER_BIT      0
#define FEATURE_DPM_GFXCLK_BIT          1
#define FEATURE_DPM_GFX_PACE_BIT        2
#define FEATURE_DPM_UCLK_BIT            3
#define FEATURE_DPM_SOCCLK_BIT          4
#define FEATURE_DPM_MP0CLK_BIT          5
#define FEATURE_DPM_LINK_BIT            6
#define FEATURE_DPM_DCEFCLK_BIT         7
#define FEATURE_MEM_VDDCI_SCALING_BIT   8 
#define FEATURE_MEM_MVDD_SCALING_BIT    9
#define FEATURE_DS_GFXCLK_BIT           10
#define FEATURE_DS_SOCCLK_BIT           11
#define FEATURE_DS_LCLK_BIT             12
#define FEATURE_DS_DCEFCLK_BIT          13
#define FEATURE_DS_UCLK_BIT             14
#define FEATURE_GFX_ULV_BIT             15  
#define FEATURE_FW_DSTATE_BIT           16 
#define FEATURE_GFXOFF_BIT              17
#define FEATURE_BACO_BIT                18
#define FEATURE_VCN_PG_BIT              19  
#define FEATURE_JPEG_PG_BIT             20
#define FEATURE_USB_PG_BIT              21
#define FEATURE_RSMU_SMN_CG_BIT         22
#define FEATURE_PPT_BIT                 23
#define FEATURE_TDC_BIT                 24
#define FEATURE_GFX_EDC_BIT             25
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
#define FEATURE_SPARE_43_BIT            43
#define FEATURE_SPARE_44_BIT            44
#define FEATURE_SPARE_45_BIT            45
#define FEATURE_SPARE_46_BIT            46
#define FEATURE_SPARE_47_BIT            47
#define FEATURE_SPARE_48_BIT            48
#define FEATURE_SPARE_49_BIT            49
#define FEATURE_SPARE_50_BIT            50
#define FEATURE_SPARE_51_BIT            51
#define FEATURE_SPARE_52_BIT            52
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
#define DPM_OVERRIDE_DISABLE_SOCCLK_PID             0x00000001
#define DPM_OVERRIDE_DISABLE_UCLK_PID               0x00000002
#define DPM_OVERRIDE_DISABLE_VOLT_LINK_VCN_SOCCLK   0x00000004
#define DPM_OVERRIDE_ENABLE_FREQ_LINK_VCLK_SOCCLK   0x00000008
#define DPM_OVERRIDE_ENABLE_FREQ_LINK_DCLK_SOCCLK   0x00000010
#define DPM_OVERRIDE_ENABLE_FREQ_LINK_GFXCLK_SOCCLK 0x00000020
#define DPM_OVERRIDE_ENABLE_FREQ_LINK_GFXCLK_UCLK   0x00000040
#define DPM_OVERRIDE_DISABLE_VOLT_LINK_DCE_SOCCLK   0x00000080
#define DPM_OVERRIDE_DISABLE_VOLT_LINK_MP0_SOCCLK   0x00000100
#define DPM_OVERRIDE_DISABLE_DFLL_PLL_SHUTDOWN      0x00000200
#define DPM_OVERRIDE_DISABLE_MEMORY_TEMPERATURE_READ 0x00000400
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
#define THROTTLER_TEMP_SKIN_BIT    11
#define THROTTLER_TDC_GFX_BIT      12
#define THROTTLER_TDC_SOC_BIT      13
#define THROTTLER_PPT0_BIT         14
#define THROTTLER_PPT1_BIT         15
#define THROTTLER_PPT2_BIT         16
#define THROTTLER_PPT3_BIT         17
#define THROTTLER_FIT_BIT          18
#define THROTTLER_PPM_BIT          19
#define THROTTLER_APCC_BIT         20
#define FW_DSTATE_SOC_ULV_BIT              0
#define FW_DSTATE_G6_HSR_BIT               1
#define FW_DSTATE_G6_PHY_VDDCI_OFF_BIT     2
#define FW_DSTATE_MP0_DS_BIT               3
#define FW_DSTATE_SMN_DS_BIT               4
#define FW_DSTATE_MP1_DS_BIT               5
#define FW_DSTATE_MP1_WHISPER_MODE_BIT     6
#define FW_DSTATE_LIV_MIN_BIT              7
#define FW_DSTATE_SOC_PLL_PWRDN_BIT        8   
#define FW_DSTATE_SOC_ULV_MASK             (1 << FW_DSTATE_SOC_ULV_BIT          )
#define FW_DSTATE_G6_HSR_MASK              (1 << FW_DSTATE_G6_HSR_BIT           )
#define FW_DSTATE_G6_PHY_VDDCI_OFF_MASK    (1 << FW_DSTATE_G6_PHY_VDDCI_OFF_BIT )
#define FW_DSTATE_MP1_DS_MASK              (1 << FW_DSTATE_MP1_DS_BIT           )  
#define FW_DSTATE_MP0_DS_MASK              (1 << FW_DSTATE_MP0_DS_BIT           )   
#define FW_DSTATE_SMN_DS_MASK              (1 << FW_DSTATE_SMN_DS_BIT           )
#define FW_DSTATE_MP1_WHISPER_MODE_MASK    (1 << FW_DSTATE_MP1_WHISPER_MODE_BIT )
#define FW_DSTATE_LIV_MIN_MASK             (1 << FW_DSTATE_LIV_MIN_BIT          )
#define FW_DSTATE_SOC_PLL_PWRDN_MASK       (1 << FW_DSTATE_SOC_PLL_PWRDN_BIT    )
#define NUM_I2C_CONTROLLERS                8
#define I2C_CONTROLLER_ENABLED             1
#define I2C_CONTROLLER_DISABLED            0
#define MAX_SW_I2C_COMMANDS                8
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
  I2C_CONTROLLER_NAME_SPARE,
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
  I2C_CONTROLLER_THROTTLER_COUNT,  
} I2cControllerThrottler_e;
typedef enum {
  I2C_CONTROLLER_PROTOCOL_VR_0,
  I2C_CONTROLLER_PROTOCOL_VR_1,
  I2C_CONTROLLER_PROTOCOL_TMP_0,
  I2C_CONTROLLER_PROTOCOL_TMP_1,
  I2C_CONTROLLER_PROTOCOL_SPARE_0,
  I2C_CONTROLLER_PROTOCOL_SPARE_1,
  I2C_CONTROLLER_PROTOCOL_COUNT,  
} I2cControllerProtocol_e;
typedef struct {
  uint8_t   Enabled;
  uint8_t   Speed;
  uint8_t   Padding[2];
  uint32_t  SlaveAddress;
  uint8_t   ControllerPort;
  uint8_t   ControllerName;
  uint8_t   ThermalThrotter;
  uint8_t   I2cProtocol;
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
#define CMDCONFIG_STOP_BIT      0
#define CMDCONFIG_RESTART_BIT   1
#define CMDCONFIG_STOP_MASK     (1 << CMDCONFIG_STOP_BIT)
#define CMDCONFIG_RESTART_MASK  (1 << CMDCONFIG_RESTART_BIT)
typedef struct {
  uint8_t RegisterAddr;  
  uint8_t Cmd;   
  uint8_t Data;   
  uint8_t CmdConfig;  
} SwI2cCmd_t;  
typedef struct {
  uint8_t     I2CcontrollerPort;  
  uint8_t     I2CSpeed;           
  uint16_t    SlaveAddress;
  uint8_t     NumCmds;            
  uint8_t     Padding[3];
  SwI2cCmd_t  SwI2cCmds[MAX_SW_I2C_COMMANDS];
  uint32_t     MmHubPadding[8];  
} SwI2cRequest_t;  
typedef enum {
  BACO_SEQUENCE,
  MSR_SEQUENCE,
  BAMACO_SEQUENCE,
  ULPS_SEQUENCE,
  D3HOT_SEQUENCE_COUNT,
}D3HOTSequence_e;
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
  uint32_t m;   
  uint32_t b;   
} LinearInt_t;
typedef struct {            
  uint32_t a;   
  uint32_t b;   
  uint32_t c;   
} DroopInt_t;
typedef enum {
  GFXCLK_SOURCE_PLL = 0, 
  GFXCLK_SOURCE_DFLL, 
  GFXCLK_SOURCE_COUNT, 
} GfxclkSrc_e; 
typedef enum {
  PPCLK_GFXCLK = 0,
  PPCLK_SOCCLK,
  PPCLK_UCLK,
  PPCLK_DCLK,
  PPCLK_VCLK,
  PPCLK_DCEFCLK,
  PPCLK_DISPCLK,
  PPCLK_PIXCLK,
  PPCLK_PHYCLK,
  PPCLK_COUNT,
} PPCLK_e;
typedef enum {
  POWER_SOURCE_AC,
  POWER_SOURCE_DC,
  POWER_SOURCE_COUNT,
} POWER_SOURCE_e;
typedef enum  {
  PPT_THROTTLER_PPT0,
  PPT_THROTTLER_PPT1,
  PPT_THROTTLER_PPT2,
  PPT_THROTTLER_PPT3,       
  PPT_THROTTLER_COUNT
} PPT_THROTTLER_e;
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
  MEMORY_TYPE_GDDR6 = 0,
  MEMORY_TYPE_HBM,
} MemoryType_e;
typedef enum {
  PWR_CONFIG_TDP = 0,
  PWR_CONFIG_TGP,
  PWR_CONFIG_TCP_ESTIMATED,
  PWR_CONFIG_TCP_MEASURED,
} PwrConfig_e;
typedef struct {
  uint8_t        VoltageMode;          
  uint8_t        SnapToDiscrete;       
  uint8_t        NumDiscreteLevels;    
  uint8_t        Padding;         
  LinearInt_t    ConversionToAvfsClk;  
  QuadraticInt_t SsCurve;              
} DpmDescriptor_t;
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
  TEMP_COUNT
} TEMP_e;
#define POWER_MANAGER_CONTROLLER_NOT_RUNNING 0
#define POWER_MANAGER_CONTROLLER_RUNNING     1
#define POWER_MANAGER_CONTROLLER_BIT                             0
#define MAXIMUM_DPM_STATE_GFX_ENGINE_RESTRICTED_BIT              8
#define GPU_DIE_TEMPERATURE_THROTTLING_BIT                       9
#define HBM_DIE_TEMPERATURE_THROTTLING_BIT                       10
#define TGP_THROTTLING_BIT                                       11
#define PCC_THROTTLING_BIT                                       12
#define HBM_TEMPERATURE_EXCEEDING_TEMPERATURE_LIMIT_BIT          13
#define HBM_TEMPERATURE_EXCEEDING_MAX_MEMORY_TEMPERATURE_BIT     14
#define POWER_MANAGER_CONTROLLER_MASK                            (1 << POWER_MANAGER_CONTROLLER_BIT                        ) 
#define MAXIMUM_DPM_STATE_GFX_ENGINE_RESTRICTED_MASK             (1 << MAXIMUM_DPM_STATE_GFX_ENGINE_RESTRICTED_BIT         )
#define GPU_DIE_TEMPERATURE_THROTTLING_MASK                      (1 << GPU_DIE_TEMPERATURE_THROTTLING_BIT                  ) 
#define HBM_DIE_TEMPERATURE_THROTTLING_MASK                      (1 << HBM_DIE_TEMPERATURE_THROTTLING_BIT                  )
#define TGP_THROTTLING_MASK                                      (1 << TGP_THROTTLING_BIT                                  )
#define PCC_THROTTLING_MASK                                      (1 << PCC_THROTTLING_BIT                                  )
#define HBM_TEMPERATURE_EXCEEDING_TEMPERATURE_LIMIT_MASK         (1 << HBM_TEMPERATURE_EXCEEDING_TEMPERATURE_LIMIT_BIT     )
#define HBM_TEMPERATURE_EXCEEDING_MAX_MEMORY_TEMPERATURE_MASK    (1 << HBM_TEMPERATURE_EXCEEDING_MAX_MEMORY_TEMPERATURE_BIT) 
typedef struct {
  uint8_t  MinorInfoVersion;
  uint8_t  MajorInfoVersion;
  uint8_t  TableSize;
  uint8_t  Reserved;
  uint8_t  Reserved1;
  uint8_t  RevID;
  uint16_t DeviceID;
  uint16_t DieTemperatureLimit;
  uint16_t FanTargetTemperature;
  uint16_t MemoryTemperatureLimit;
  uint16_t MemoryTemperatureLimit1;
  uint16_t TGP;
  uint16_t CardPower;
  uint32_t DieTemperatureRegisterOffset;
  uint32_t Reserved2;
  uint32_t Reserved3;
  uint32_t Status;
  uint16_t DieTemperature;
  uint16_t CurrentMemoryTemperature;
  uint16_t MemoryTemperature;
  uint8_t MemoryHotspotPosition;
  uint8_t Reserved4;
  uint32_t BoardLevelEnergyAccumulator;  
} OutOfBandMonitor_t;
#pragma pack(push, 1)
typedef struct {
  uint32_t Version;
  uint32_t FeaturesToRun[2];
  uint16_t SocketPowerLimitAc[PPT_THROTTLER_COUNT];
  uint16_t SocketPowerLimitAcTau[PPT_THROTTLER_COUNT];
  uint16_t SocketPowerLimitDc[PPT_THROTTLER_COUNT];
  uint16_t SocketPowerLimitDcTau[PPT_THROTTLER_COUNT];  
  uint16_t TdcLimitSoc;              
  uint16_t TdcLimitSocTau;           
  uint16_t TdcLimitGfx;              
  uint16_t TdcLimitGfxTau;           
  uint16_t TedgeLimit;               
  uint16_t ThotspotLimit;            
  uint16_t TmemLimit;                
  uint16_t Tvr_gfxLimit;             
  uint16_t Tvr_mem0Limit;            
  uint16_t Tvr_mem1Limit;            
  uint16_t Tvr_socLimit;             
  uint16_t Tliquid0Limit;            
  uint16_t Tliquid1Limit;            
  uint16_t TplxLimit;                
  uint32_t FitLimit;                 
  uint16_t PpmPowerLimit;            
  uint16_t PpmTemperatureThreshold;
  uint32_t ThrottlerControlMask;    
  uint32_t FwDStateMask;            
  uint16_t  UlvVoltageOffsetSoc;  
  uint16_t  UlvVoltageOffsetGfx;  
  uint8_t   GceaLinkMgrIdleThreshold;         
  uint8_t   paddingRlcUlvParams[3];
  uint8_t  UlvSmnclkDid;      
  uint8_t  UlvMp1clkDid;      
  uint8_t  UlvGfxclkBypass;   
  uint8_t  Padding234;
  uint16_t     MinVoltageUlvGfx;  
  uint16_t     MinVoltageUlvSoc;  
  uint16_t     MinVoltageGfx;      
  uint16_t     MinVoltageSoc;      
  uint16_t     MaxVoltageGfx;      
  uint16_t     MaxVoltageSoc;      
  uint16_t     LoadLineResistanceGfx;    
  uint16_t     LoadLineResistanceSoc;    
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
  uint32_t       Paddingclks[16];
  uint16_t       DcModeMaxFreq     [PPCLK_COUNT            ];      
  uint16_t       Padding8_Clks;
  uint8_t        FreqTableUclkDiv  [NUM_UCLK_DPM_LEVELS    ];      
  uint16_t       Mp0clkFreq        [NUM_MP0CLK_DPM_LEVELS];        
  uint16_t       Mp0DpmVoltage     [NUM_MP0CLK_DPM_LEVELS];        
  uint16_t       MemVddciVoltage   [NUM_UCLK_DPM_LEVELS];          
  uint16_t       MemMvddVoltage    [NUM_UCLK_DPM_LEVELS];          
  uint16_t        GfxclkFgfxoffEntry;    
  uint16_t        GfxclkFinit;           
  uint16_t        GfxclkFidle;           
  uint16_t        GfxclkSlewRate;        
  uint16_t        GfxclkFopt;            
  uint8_t         Padding567[2]; 
  uint16_t        GfxclkDsMaxFreq;       
  uint8_t         GfxclkSource;          
  uint8_t         Padding456;
  uint8_t      LowestUclkReservedForUlv;  
  uint8_t      paddingUclk[3];
  uint8_t      MemoryType;           
  uint8_t      MemoryChannels;
  uint8_t      PaddingMem[2];
  uint8_t      PcieGenSpeed[NUM_LINK_LEVELS];            
  uint8_t      PcieLaneCount[NUM_LINK_LEVELS];           
  uint16_t     LclkFreq[NUM_LINK_LEVELS];              
  uint16_t     EnableTdpm;      
  uint16_t     TdpmHighHystTemperature;
  uint16_t     TdpmLowHystTemperature;
  uint16_t     GfxclkFreqHighTempLimit;  
  uint16_t     FanStopTemp;           
  uint16_t     FanStartTemp;          
  uint16_t     FanGainEdge;
  uint16_t     FanGainHotspot;
  uint16_t     FanGainLiquid0;
  uint16_t     FanGainLiquid1;  
  uint16_t     FanGainVrGfx;
  uint16_t     FanGainVrSoc;
  uint16_t     FanGainVrMem0;
  uint16_t     FanGainVrMem1;  
  uint16_t     FanGainPlx;
  uint16_t     FanGainMem;
  uint16_t     FanPwmMin;
  uint16_t     FanAcousticLimitRpm;
  uint16_t     FanThrottlingRpm;
  uint16_t     FanMaximumRpm;
  uint16_t     FanTargetTemperature;
  uint16_t     FanTargetGfxclk;
  uint8_t      FanTempInputSelect;
  uint8_t      FanPadding;
  uint8_t      FanZeroRpmEnable; 
  uint8_t      FanTachEdgePerRev;
  int16_t      FuzzyFan_ErrorSetDelta;
  int16_t      FuzzyFan_ErrorRateSetDelta;
  int16_t      FuzzyFan_PwmSetDelta;
  uint16_t     FuzzyFan_Reserved;
  uint8_t           OverrideAvfsGb[AVFS_VOLTAGE_COUNT];
  uint8_t           Padding8_Avfs[2];
  QuadraticInt_t    qAvfsGb[AVFS_VOLTAGE_COUNT];               
  DroopInt_t        dBtcGbGfxPll;          
  DroopInt_t        dBtcGbGfxDfll;         
  DroopInt_t        dBtcGbSoc;             
  LinearInt_t       qAgingGb[AVFS_VOLTAGE_COUNT];           
  QuadraticInt_t    qStaticVoltageOffset[AVFS_VOLTAGE_COUNT];  
  uint16_t          DcTol[AVFS_VOLTAGE_COUNT];             
  uint8_t           DcBtcEnabled[AVFS_VOLTAGE_COUNT];
  uint8_t           Padding8_GfxBtc[2];
  uint16_t          DcBtcMin[AVFS_VOLTAGE_COUNT];        
  uint16_t          DcBtcMax[AVFS_VOLTAGE_COUNT];        
  uint32_t          DebugOverrides;
  QuadraticInt_t    ReservedEquation0; 
  QuadraticInt_t    ReservedEquation1; 
  QuadraticInt_t    ReservedEquation2; 
  QuadraticInt_t    ReservedEquation3; 
  uint8_t      TotalPowerConfig;     
  uint8_t      TotalPowerSpare1;  
  uint16_t     TotalPowerSpare2;
  uint16_t     PccThresholdLow;
  uint16_t     PccThresholdHigh;
  uint32_t     MGpuFanBoostLimitRpm;
  uint32_t     PaddingAPCC[5];
  uint16_t     VDDGFX_TVmin;        
  uint16_t     VDDSOC_TVmin;        
  uint16_t     VDDGFX_Vmin_HiTemp;  
  uint16_t     VDDGFX_Vmin_LoTemp;  
  uint16_t     VDDSOC_Vmin_HiTemp;  
  uint16_t     VDDSOC_Vmin_LoTemp;  
  uint16_t     VDDGFX_TVminHystersis;  
  uint16_t     VDDSOC_TVminHystersis;  
  uint32_t     BtcConfig;
  uint16_t     SsFmin[10];  
  uint16_t     DcBtcGb[AVFS_VOLTAGE_COUNT];
  uint32_t     Reserved[8];
  I2cControllerConfig_t  I2cControllers[NUM_I2C_CONTROLLERS];     
  uint16_t     MaxVoltageStepGfx;  
  uint16_t     MaxVoltageStepSoc;  
  uint8_t      VddGfxVrMapping;    
  uint8_t      VddSocVrMapping;    
  uint8_t      VddMem0VrMapping;   
  uint8_t      VddMem1VrMapping;   
  uint8_t      GfxUlvPhaseSheddingMask;  
  uint8_t      SocUlvPhaseSheddingMask;  
  uint8_t      ExternalSensorPresent;  
  uint8_t      Padding8_V; 
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
  uint8_t      padding8_4;
  uint8_t      PllGfxclkSpreadEnabled;    
  uint8_t      PllGfxclkSpreadPercent;    
  uint16_t     PllGfxclkSpreadFreq;       
  uint8_t      DfllGfxclkSpreadEnabled;    
  uint8_t      DfllGfxclkSpreadPercent;    
  uint16_t     DfllGfxclkSpreadFreq;       
  uint8_t      UclkSpreadEnabled;    
  uint8_t      UclkSpreadPercent;    
  uint16_t     UclkSpreadFreq;       
  uint8_t      SoclkSpreadEnabled;    
  uint8_t      SocclkSpreadPercent;    
  uint16_t     SocclkSpreadFreq;       
  uint16_t     TotalBoardPower;      
  uint16_t     BoardPadding; 
  uint32_t     MvddRatio;  
  uint8_t      RenesesLoadLineEnabled;
  uint8_t      GfxLoadlineResistance;
  uint8_t      SocLoadlineResistance;
  uint8_t      Padding8_Loadline;
  uint32_t     BoardReserved[8];
  uint32_t     MmHubPadding[8];  
} PPTable_t;
#pragma pack(pop)
typedef struct {
  uint16_t     GfxclkAverageLpfTau;
  uint16_t     SocclkAverageLpfTau;
  uint16_t     UclkAverageLpfTau;
  uint16_t     GfxActivityLpfTau;
  uint16_t     UclkActivityLpfTau;
  uint16_t     SocketPowerLpfTau;
  uint32_t     MmHubPadding[8];  
} DriverSmuConfig_t;
typedef struct {
  uint16_t      GfxclkFmin;            
  uint16_t      GfxclkFmax;            
  uint16_t      GfxclkFreq1;           
  uint16_t      GfxclkVolt1;           
  uint16_t      GfxclkFreq2;           
  uint16_t      GfxclkVolt2;           
  uint16_t      GfxclkFreq3;           
  uint16_t      GfxclkVolt3;           
  uint16_t      UclkFmax;              
  int16_t       OverDrivePct;          
  uint16_t      FanMaximumRpm;
  uint16_t      FanMinimumPwm;
  uint16_t      FanTargetTemperature;  
  uint16_t      FanMode;
  uint16_t      FanMaxPwm;
  uint16_t      FanMinPwm;
  uint16_t      FanMaxTemp;  
  uint16_t      FanMinTemp;  
  uint16_t      MaxOpTemp;             
  uint16_t      FanZeroRpmEnable;
  uint32_t     MmHubPadding[6];  
} OverDriveTable_t; 
typedef struct {
  uint16_t CurrClock[PPCLK_COUNT];
  uint16_t AverageGfxclkFrequency;
  uint16_t AverageSocclkFrequency;
  uint16_t AverageUclkFrequency  ;
  uint16_t AverageGfxActivity    ;
  uint16_t AverageUclkActivity   ;
  uint8_t  CurrSocVoltageOffset  ;
  uint8_t  CurrGfxVoltageOffset  ;
  uint8_t  CurrMemVidOffset      ;
  uint8_t  Padding8              ;
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
  uint8_t  Padding8_2;
  uint16_t CurrFanSpeed;
  uint32_t     MmHubPadding[8];  
} SmuMetrics_legacy_t;
typedef struct {
  uint16_t CurrClock[PPCLK_COUNT];
  uint16_t AverageGfxclkFrequencyPostDs;
  uint16_t AverageSocclkFrequency;
  uint16_t AverageUclkFrequencyPostDs;
  uint16_t AverageGfxActivity    ;
  uint16_t AverageUclkActivity   ;
  uint8_t  CurrSocVoltageOffset  ;
  uint8_t  CurrGfxVoltageOffset  ;
  uint8_t  CurrMemVidOffset      ;
  uint8_t  Padding8              ;
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
  uint8_t  Padding8_2;
  uint16_t CurrFanSpeed;
  uint16_t AverageGfxclkFrequencyPreDs;
  uint16_t AverageUclkFrequencyPreDs;
  uint8_t  PcieRate;
  uint8_t  PcieWidth;
  uint8_t  Padding8_3[2];
  uint32_t     MmHubPadding[8];  
} SmuMetrics_t;
typedef struct {
  uint16_t CurrClock[PPCLK_COUNT];
  uint16_t AverageGfxclkFrequency;
  uint16_t AverageSocclkFrequency;
  uint16_t AverageUclkFrequency  ;
  uint16_t AverageGfxActivity    ;
  uint16_t AverageUclkActivity   ;
  uint8_t  CurrSocVoltageOffset  ;
  uint8_t  CurrGfxVoltageOffset  ;
  uint8_t  CurrMemVidOffset      ;
  uint8_t  Padding8              ;
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
  uint8_t  Padding8_2;
  uint16_t CurrFanSpeed;
  uint32_t EnergyAccumulator;
  uint16_t AverageVclkFrequency  ;
  uint16_t AverageDclkFrequency  ;
  uint16_t VcnActivityPercentage ;
  uint16_t padding16_2;
  uint32_t     MmHubPadding[8];  
} SmuMetrics_NV12_legacy_t;
typedef struct {
  uint16_t CurrClock[PPCLK_COUNT];
  uint16_t AverageGfxclkFrequencyPostDs;
  uint16_t AverageSocclkFrequency;
  uint16_t AverageUclkFrequencyPostDs;
  uint16_t AverageGfxActivity    ;
  uint16_t AverageUclkActivity   ;
  uint8_t  CurrSocVoltageOffset  ;
  uint8_t  CurrGfxVoltageOffset  ;
  uint8_t  CurrMemVidOffset      ;
  uint8_t  Padding8              ;
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
  uint8_t  Padding8_2;
  uint16_t CurrFanSpeed;
  uint16_t AverageVclkFrequency  ;
  uint16_t AverageDclkFrequency  ;
  uint16_t VcnActivityPercentage ;
  uint16_t AverageGfxclkFrequencyPreDs;
  uint16_t AverageUclkFrequencyPreDs;
  uint8_t  PcieRate;
  uint8_t  PcieWidth;
  uint32_t Padding32_1;
  uint64_t EnergyAccumulator;
  uint32_t     MmHubPadding[8];  
} SmuMetrics_NV12_t;
typedef union SmuMetrics {
	SmuMetrics_legacy_t		nv10_legacy_metrics;
	SmuMetrics_t			nv10_metrics;
	SmuMetrics_NV12_legacy_t	nv12_legacy_metrics;
	SmuMetrics_NV12_t		nv12_metrics;
} SmuMetrics_NV1X_t;
typedef struct {
  uint16_t MinClock;  
  uint16_t MaxClock;  
  uint16_t MinUclk;
  uint16_t MaxUclk;
  uint8_t  WmSetting;
  uint8_t  Padding[3];
  uint32_t     MmHubPadding[8];  
} WatermarkRowGeneric_t;
#define NUM_WM_RANGES 4
typedef enum {
  WM_SOCCLK = 0,
  WM_DCEFCLK,
  WM_COUNT,
} WM_CLOCK_e;
typedef struct {
  WatermarkRowGeneric_t WatermarkRow[WM_COUNT][NUM_WM_RANGES];
  uint32_t     MmHubPadding[8];  
} Watermarks_t;
typedef struct {
  uint16_t avgPsmCount[28];
  uint16_t minPsmCount[28];
  float    avgPsmVoltage[28];
  float    minPsmVoltage[28];
  uint32_t     MmHubPadding[32];  
} AvfsDebugTable_t_NV14;
typedef struct {
  uint16_t avgPsmCount[36];
  uint16_t minPsmCount[36];
  float    avgPsmVoltage[36]; 
  float    minPsmVoltage[36];
  uint32_t     MmHubPadding[8];  
} AvfsDebugTable_t_NV10;
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
  uint32_t EnabledAvfsModules[2];  
  uint32_t     MmHubPadding[8];  
} AvfsFuseOverride_t;
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
  uint8_t   Soc_ActiveHystLimit;
  uint8_t   Soc_IdleHystLimit;
  uint8_t   Soc_FPS;
  uint8_t   Soc_MinActiveFreqType;
  uint8_t   Soc_BoosterFreqType; 
  uint8_t   Soc_MinFreqStep;                 
  uint16_t  Soc_MinActiveFreq;               
  uint16_t  Soc_BoosterFreq;                 
  uint16_t  Soc_PD_Data_time_constant;       
  uint32_t  Soc_PD_Data_limit_a;             
  uint32_t  Soc_PD_Data_limit_b;             
  uint32_t  Soc_PD_Data_limit_c;             
  uint32_t  Soc_PD_Data_error_coeff;         
  uint32_t  Soc_PD_Data_error_rate_coeff;    
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
  uint32_t     MmHubPadding[8];  
} DpmActivityMonitorCoeffInt_t;
#define WORKLOAD_PPLIB_DEFAULT_BIT        0 
#define WORKLOAD_PPLIB_FULL_SCREEN_3D_BIT 1 
#define WORKLOAD_PPLIB_POWER_SAVING_BIT   2 
#define WORKLOAD_PPLIB_VIDEO_BIT          3 
#define WORKLOAD_PPLIB_VR_BIT             4 
#define WORKLOAD_PPLIB_COMPUTE_BIT        5 
#define WORKLOAD_PPLIB_CUSTOM_BIT         6 
#define WORKLOAD_PPLIB_COUNT              7 
#define TABLE_TRANSFER_OK         0x0
#define TABLE_TRANSFER_FAILED     0xFF
#define TABLE_PPTABLE                 0
#define TABLE_WATERMARKS              1
#define TABLE_AVFS                    2
#define TABLE_AVFS_PSM_DEBUG          3
#define TABLE_AVFS_FUSE_OVERRIDE      4
#define TABLE_PMSTATUSLOG             5
#define TABLE_SMU_METRICS             6
#define TABLE_DRIVER_SMU_CONFIG       7
#define TABLE_ACTIVITY_MONITOR_COEFF  8
#define TABLE_OVERDRIVE               9
#define TABLE_I2C_COMMANDS           10
#define TABLE_PACE                   11
#define TABLE_COUNT                  12
#define RLC_PACE_TABLE_NUM_LEVELS 16
typedef struct {
  float FlopsPerByteTable[RLC_PACE_TABLE_NUM_LEVELS];
  uint32_t     MmHubPadding[8];  
} RlcPaceFlopsPerByteOverride_t;
#define UCLK_SWITCH_SLOW 0
#define UCLK_SWITCH_FAST 1
#endif
