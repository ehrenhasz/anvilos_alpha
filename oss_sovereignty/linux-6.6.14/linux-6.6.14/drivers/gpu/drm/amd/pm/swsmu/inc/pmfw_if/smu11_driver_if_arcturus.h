#ifndef SMU11_DRIVER_IF_ARCTURUS_H
#define SMU11_DRIVER_IF_ARCTURUS_H
#define PPTABLE_ARCTURUS_SMU_VERSION 4
#define NUM_GFXCLK_DPM_LEVELS  16
#define NUM_VCLK_DPM_LEVELS    8
#define NUM_DCLK_DPM_LEVELS    8
#define NUM_MP0CLK_DPM_LEVELS  2
#define NUM_SOCCLK_DPM_LEVELS  8
#define NUM_UCLK_DPM_LEVELS    4
#define NUM_FCLK_DPM_LEVELS    8
#define NUM_XGMI_LEVELS        2
#define NUM_XGMI_PSTATE_LEVELS 4
#define MAX_GFXCLK_DPM_LEVEL  (NUM_GFXCLK_DPM_LEVELS  - 1)
#define MAX_VCLK_DPM_LEVEL    (NUM_VCLK_DPM_LEVELS    - 1)
#define MAX_DCLK_DPM_LEVEL    (NUM_DCLK_DPM_LEVELS    - 1)
#define MAX_MP0CLK_DPM_LEVEL  (NUM_MP0CLK_DPM_LEVELS  - 1)
#define MAX_SOCCLK_DPM_LEVEL  (NUM_SOCCLK_DPM_LEVELS  - 1)
#define MAX_UCLK_DPM_LEVEL    (NUM_UCLK_DPM_LEVELS    - 1)
#define MAX_FCLK_DPM_LEVEL    (NUM_FCLK_DPM_LEVELS    - 1)
#define MAX_XGMI_LEVEL        (NUM_XGMI_LEVELS        - 1)
#define MAX_XGMI_PSTATE_LEVEL (NUM_XGMI_PSTATE_LEVELS - 1)
#define FEATURE_DPM_PREFETCHER_BIT      0
#define FEATURE_DPM_GFXCLK_BIT          1
#define FEATURE_DPM_UCLK_BIT            2
#define FEATURE_DPM_SOCCLK_BIT          3
#define FEATURE_DPM_FCLK_BIT            4
#define FEATURE_DPM_MP0CLK_BIT          5
#define FEATURE_DPM_XGMI_BIT            6
#define FEATURE_DS_GFXCLK_BIT           7
#define FEATURE_DS_SOCCLK_BIT           8
#define FEATURE_DS_LCLK_BIT             9
#define FEATURE_DS_FCLK_BIT             10
#define FEATURE_DS_UCLK_BIT             11
#define FEATURE_GFX_ULV_BIT             12
#define FEATURE_DPM_VCN_BIT             13
#define FEATURE_RSMU_SMN_CG_BIT         14
#define FEATURE_WAFL_CG_BIT             15
#define FEATURE_PPT_BIT                 16
#define FEATURE_TDC_BIT                 17
#define FEATURE_APCC_PLUS_BIT           18
#define FEATURE_VR0HOT_BIT              19
#define FEATURE_VR1HOT_BIT              20
#define FEATURE_FW_CTF_BIT              21
#define FEATURE_FAN_CONTROL_BIT         22
#define FEATURE_THERMAL_BIT             23
#define FEATURE_OUT_OF_BAND_MONITOR_BIT 24
#define FEATURE_TEMP_DEPENDENT_VMIN_BIT 25
#define FEATURE_PER_PART_VMIN_BIT       26
#define FEATURE_SPARE_27_BIT            27
#define FEATURE_SPARE_28_BIT            28
#define FEATURE_SPARE_29_BIT            29
#define FEATURE_SPARE_30_BIT            30
#define FEATURE_SPARE_31_BIT            31
#define FEATURE_SPARE_32_BIT            32
#define FEATURE_SPARE_33_BIT            33
#define FEATURE_SPARE_34_BIT            34
#define FEATURE_SPARE_35_BIT            35
#define FEATURE_SPARE_36_BIT            36
#define FEATURE_SPARE_37_BIT            37
#define FEATURE_SPARE_38_BIT            38
#define FEATURE_SPARE_39_BIT            39
#define FEATURE_SPARE_40_BIT            40
#define FEATURE_SPARE_41_BIT            41
#define FEATURE_SPARE_42_BIT            42
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
#define FEATURE_DPM_PREFETCHER_MASK       (1 << FEATURE_DPM_PREFETCHER_BIT       )
#define FEATURE_DPM_GFXCLK_MASK           (1 << FEATURE_DPM_GFXCLK_BIT           )
#define FEATURE_DPM_UCLK_MASK             (1 << FEATURE_DPM_UCLK_BIT             )
#define FEATURE_DPM_SOCCLK_MASK           (1 << FEATURE_DPM_SOCCLK_BIT           )
#define FEATURE_DPM_FCLK_MASK             (1 << FEATURE_DPM_FCLK_BIT             )
#define FEATURE_DPM_MP0CLK_MASK           (1 << FEATURE_DPM_MP0CLK_BIT           )
#define FEATURE_DPM_XGMI_MASK             (1 << FEATURE_DPM_XGMI_BIT             )
#define FEATURE_DS_GFXCLK_MASK            (1 << FEATURE_DS_GFXCLK_BIT            )
#define FEATURE_DS_SOCCLK_MASK            (1 << FEATURE_DS_SOCCLK_BIT            )
#define FEATURE_DS_LCLK_MASK              (1 << FEATURE_DS_LCLK_BIT              )
#define FEATURE_DS_FCLK_MASK              (1 << FEATURE_DS_FCLK_BIT              )
#define FEATURE_DS_UCLK_MASK              (1 << FEATURE_DS_UCLK_BIT              )
#define FEATURE_GFX_ULV_MASK              (1 << FEATURE_GFX_ULV_BIT              )
#define FEATURE_DPM_VCN_MASK              (1 << FEATURE_DPM_VCN_BIT              )
#define FEATURE_RSMU_SMN_CG_MASK          (1 << FEATURE_RSMU_SMN_CG_BIT          )
#define FEATURE_WAFL_CG_MASK              (1 << FEATURE_WAFL_CG_BIT              )
#define FEATURE_PPT_MASK                  (1 << FEATURE_PPT_BIT                  )
#define FEATURE_TDC_MASK                  (1 << FEATURE_TDC_BIT                  )
#define FEATURE_APCC_PLUS_MASK            (1 << FEATURE_APCC_PLUS_BIT            )
#define FEATURE_VR0HOT_MASK               (1 << FEATURE_VR0HOT_BIT               )
#define FEATURE_VR1HOT_MASK               (1 << FEATURE_VR1HOT_BIT               )
#define FEATURE_FW_CTF_MASK               (1 << FEATURE_FW_CTF_BIT               )
#define FEATURE_FAN_CONTROL_MASK          (1 << FEATURE_FAN_CONTROL_BIT          )
#define FEATURE_THERMAL_MASK              (1 << FEATURE_THERMAL_BIT              )
#define FEATURE_OUT_OF_BAND_MONITOR_MASK  (1 << FEATURE_OUT_OF_BAND_MONITOR_BIT   )
#define FEATURE_TEMP_DEPENDENT_VMIN_MASK  (1 << FEATURE_TEMP_DEPENDENT_VMIN_BIT )
#define FEATURE_PER_PART_VMIN_MASK        (1 << FEATURE_PER_PART_VMIN_BIT        )
#define DPM_OVERRIDE_DISABLE_UCLK_PID               0x00000001
#define DPM_OVERRIDE_DISABLE_VOLT_LINK_VCN_FCLK     0x00000002
#define I2C_CONTROLLER_ENABLED           1
#define I2C_CONTROLLER_DISABLED          0
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
#define THROTTLER_TEMP_VR_MEM_BIT  5
#define THROTTLER_TEMP_VR_SOC_BIT  6
#define THROTTLER_TDC_GFX_BIT      7
#define THROTTLER_TDC_SOC_BIT      8
#define THROTTLER_PPT0_BIT         9
#define THROTTLER_PPT1_BIT         10
#define THROTTLER_PPT2_BIT         11
#define THROTTLER_PPT3_BIT         12
#define THROTTLER_PPM_BIT          13
#define THROTTLER_FIT_BIT          14
#define THROTTLER_APCC_BIT         15
#define THROTTLER_VRHOT0_BIT       16
#define THROTTLER_VRHOT1_BIT       17
#define TABLE_TRANSFER_OK         0x0
#define TABLE_TRANSFER_FAILED     0xFF
#define TABLE_TRANSFER_PENDING    0xAB
#define WORKLOAD_PPLIB_DEFAULT_BIT        0
#define WORKLOAD_PPLIB_POWER_SAVING_BIT   1
#define WORKLOAD_PPLIB_VIDEO_BIT          2
#define WORKLOAD_PPLIB_COMPUTE_BIT        3
#define WORKLOAD_PPLIB_CUSTOM_BIT         4
#define WORKLOAD_PPLIB_COUNT              5
#define XGMI_STATE_D0 1
#define XGMI_STATE_D3 0
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
  I2C_CONTROLLER_NAME_VR_MEM,
  I2C_CONTROLLER_NAME_SPARE,
  I2C_CONTROLLER_NAME_COUNT,
} I2cControllerName_e;
typedef enum {
  I2C_CONTROLLER_THROTTLER_TYPE_NONE = 0,
  I2C_CONTROLLER_THROTTLER_VR_GFX,
  I2C_CONTROLLER_THROTTLER_VR_SOC,
  I2C_CONTROLLER_THROTTLER_VR_MEM,
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
  GFXCLK_SOURCE_AFLL,
  GFXCLK_SOURCE_COUNT,
} GfxclkSrc_e;
typedef enum {
  PPCLK_GFXCLK,
  PPCLK_VCLK,
  PPCLK_DCLK,
  PPCLK_SOCCLK,
  PPCLK_UCLK,
  PPCLK_FCLK,
  PPCLK_COUNT,
} PPCLK_e;
typedef enum {
  POWER_SOURCE_AC,
  POWER_SOURCE_DC,
  POWER_SOURCE_COUNT,
} POWER_SOURCE_e;
typedef enum {
  TEMP_EDGE,
  TEMP_HOTSPOT,
  TEMP_MEM,
  TEMP_VR_GFX,
  TEMP_VR_SOC,
  TEMP_VR_MEM,
  TEMP_COUNT
} TEMP_TYPE_e;
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
  XGMI_LINK_WIDTH_1 = 1,    
  XGMI_LINK_WIDTH_2 = 2,    
  XGMI_LINK_WIDTH_4 = 4,    
  XGMI_LINK_WIDTH_8 = 8,    
  XGMI_LINK_WIDTH_9 = 9,    
  XGMI_LINK_WIDTH_16 = 16,  
  XGMI_LINK_WIDTH_COUNT
} XGMI_LINK_WIDTH_e;
typedef struct {
  uint8_t        VoltageMode;          
  uint8_t        SnapToDiscrete;       
  uint8_t        NumDiscreteLevels;    
  uint8_t        padding;
  LinearInt_t    ConversionToAvfsClk;  
  QuadraticInt_t SsCurve;              
  uint16_t       SsFmin;               
  uint16_t       Padding16;
} DpmDescriptor_t;
#pragma pack(push, 1)
typedef struct {
  uint32_t Version;
  uint32_t FeaturesToRun[2];
  uint16_t SocketPowerLimitAc[PPT_THROTTLER_COUNT];
  uint16_t SocketPowerLimitAcTau[PPT_THROTTLER_COUNT];
  uint16_t TdcLimitSoc;              
  uint16_t TdcLimitSocTau;           
  uint16_t TdcLimitGfx;              
  uint16_t TdcLimitGfxTau;           
  uint16_t TedgeLimit;               
  uint16_t ThotspotLimit;            
  uint16_t TmemLimit;                
  uint16_t Tvr_gfxLimit;             
  uint16_t Tvr_memLimit;             
  uint16_t Tvr_socLimit;             
  uint32_t FitLimit;                 
  uint16_t PpmPowerLimit;            
  uint16_t PpmTemperatureThreshold;
  uint32_t ThrottlerControlMask;    
  uint16_t  UlvVoltageOffsetGfx;  
  uint16_t  UlvPadding;           
  uint8_t  UlvGfxclkBypass;   
  uint8_t  Padding234[3];
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
  uint16_t       FreqTableFclk     [NUM_FCLK_DPM_LEVELS    ];      
  uint32_t       Paddingclks[16];
  uint16_t       Mp0clkFreq        [NUM_MP0CLK_DPM_LEVELS];        
  uint16_t       Mp0DpmVoltage     [NUM_MP0CLK_DPM_LEVELS];        
  uint16_t        GfxclkFidle;           
  uint16_t        GfxclkSlewRate;        
  uint8_t         Padding567[4];
  uint16_t        GfxclkDsMaxFreq;       
  uint8_t         GfxclkSource;          
  uint8_t         Padding456;
  uint16_t     EnableTdpm;
  uint16_t     TdpmHighHystTemperature;
  uint16_t     TdpmLowHystTemperature;
  uint16_t     GfxclkFreqHighTempLimit;  
  uint16_t     FanStopTemp;           
  uint16_t     FanStartTemp;          
  uint16_t     FanGainEdge;
  uint16_t     FanGainHotspot;
  uint16_t     FanGainVrGfx;
  uint16_t     FanGainVrSoc;
  uint16_t     FanGainVrMem;
  uint16_t     FanGainHbm;
  uint16_t     FanPwmMin;
  uint16_t     FanAcousticLimitRpm;
  uint16_t     FanThrottlingRpm;
  uint16_t     FanMaximumRpm;
  uint16_t     FanTargetTemperature;
  uint16_t     FanTargetGfxclk;
  uint8_t      FanZeroRpmEnable;
  uint8_t      FanTachEdgePerRev;
  uint8_t      FanTempInputSelect;
  uint8_t      padding8_Fan;
  int16_t      FuzzyFan_ErrorSetDelta;
  int16_t      FuzzyFan_ErrorRateSetDelta;
  int16_t      FuzzyFan_PwmSetDelta;
  uint16_t     FuzzyFan_Reserved;
  uint8_t           OverrideAvfsGb[AVFS_VOLTAGE_COUNT];
  uint8_t           Padding8_Avfs[2];
  QuadraticInt_t    qAvfsGb[AVFS_VOLTAGE_COUNT];               
  DroopInt_t        dBtcGbGfxPll;        
  DroopInt_t        dBtcGbGfxAfll;         
  DroopInt_t        dBtcGbSoc;             
  LinearInt_t       qAgingGb[AVFS_VOLTAGE_COUNT];           
  QuadraticInt_t    qStaticVoltageOffset[AVFS_VOLTAGE_COUNT];  
  uint16_t          DcTol[AVFS_VOLTAGE_COUNT];             
  uint8_t           DcBtcEnabled[AVFS_VOLTAGE_COUNT];
  uint8_t           Padding8_GfxBtc[2];
  uint16_t          DcBtcMin[AVFS_VOLTAGE_COUNT];        
  uint16_t          DcBtcMax[AVFS_VOLTAGE_COUNT];        
  uint16_t          DcBtcGb[AVFS_VOLTAGE_COUNT];         
  uint8_t           XgmiDpmPstates[NUM_XGMI_LEVELS];  
  uint8_t           XgmiDpmSpare[2];
  uint16_t     VDDGFX_TVmin;        
  uint16_t     VDDSOC_TVmin;        
  uint16_t     VDDGFX_Vmin_HiTemp;  
  uint16_t     VDDGFX_Vmin_LoTemp;  
  uint16_t     VDDSOC_Vmin_HiTemp;  
  uint16_t     VDDSOC_Vmin_LoTemp;  
  uint16_t     VDDGFX_TVminHystersis;  
  uint16_t     VDDSOC_TVminHystersis;  
  uint32_t          DebugOverrides;
  QuadraticInt_t    ReservedEquation0;
  QuadraticInt_t    ReservedEquation1;
  QuadraticInt_t    ReservedEquation2;
  QuadraticInt_t    ReservedEquation3;
  uint16_t     MinVoltageUlvGfx;  
  uint16_t     PaddingUlv;        
  uint8_t      TotalPowerConfig;     
  uint8_t      TotalPowerSpare1;
  uint16_t     TotalPowerSpare2;
  uint16_t     PccThresholdLow;
  uint16_t     PccThresholdHigh;
  uint32_t     PaddingAPCC[6];   
  uint16_t BasePerformanceCardPower;
  uint16_t MaxPerformanceCardPower;
  uint16_t BasePerformanceFrequencyCap;    
  uint16_t MaxPerformanceFrequencyCap;     
  uint16_t VDDGFX_VminLow;         
  uint16_t VDDGFX_TVminLow;        
  uint16_t VDDGFX_VminLow_HiTemp;  
  uint16_t VDDGFX_VminLow_LoTemp;  
  uint32_t     Reserved[7];
  uint16_t     MaxVoltageStepGfx;  
  uint16_t     MaxVoltageStepSoc;  
  uint8_t      VddGfxVrMapping;      
  uint8_t      VddSocVrMapping;      
  uint8_t      VddMemVrMapping;      
  uint8_t      BoardVrMapping;       
  uint8_t      GfxUlvPhaseSheddingMask;  
  uint8_t      ExternalSensorPresent;  
  uint8_t      Padding8_V[2];
  uint16_t     GfxMaxCurrent;    
  int8_t       GfxOffset;        
  uint8_t      Padding_TelemetryGfx;
  uint16_t     SocMaxCurrent;    
  int8_t       SocOffset;        
  uint8_t      Padding_TelemetrySoc;
  uint16_t     MemMaxCurrent;    
  int8_t       MemOffset;        
  uint8_t      Padding_TelemetryMem;
  uint16_t     BoardMaxCurrent;    
  int8_t       BoardOffset;        
  uint8_t      Padding_TelemetryBoardInput;
  uint8_t      VR0HotGpio;       
  uint8_t      VR0HotPolarity;   
  uint8_t      VR1HotGpio;       
  uint8_t      VR1HotPolarity;   
  uint8_t      PllGfxclkSpreadEnabled;    
  uint8_t      PllGfxclkSpreadPercent;    
  uint16_t     PllGfxclkSpreadFreq;       
  uint8_t      UclkSpreadEnabled;    
  uint8_t      UclkSpreadPercent;    
  uint16_t     UclkSpreadFreq;       
  uint8_t      FclkSpreadEnabled;    
  uint8_t      FclkSpreadPercent;    
  uint16_t     FclkSpreadFreq;       
  uint8_t      FllGfxclkSpreadEnabled;    
  uint8_t      FllGfxclkSpreadPercent;    
  uint16_t     FllGfxclkSpreadFreq;       
  I2cControllerConfig_t  I2cControllers[NUM_I2C_CONTROLLERS];
  uint32_t     MemoryChannelEnabled;  
  uint8_t      DramBitWidth;  
  uint8_t      PaddingMem[3];
  uint16_t     TotalBoardPower;      
  uint16_t     BoardPadding;
  uint8_t           XgmiLinkSpeed   [NUM_XGMI_PSTATE_LEVELS];
  uint8_t           XgmiLinkWidth   [NUM_XGMI_PSTATE_LEVELS];
  uint16_t          XgmiFclkFreq    [NUM_XGMI_PSTATE_LEVELS];
  uint16_t          XgmiSocVoltage  [NUM_XGMI_PSTATE_LEVELS];
  uint8_t      GpioI2cScl;           
  uint8_t      GpioI2cSda;           
  uint16_t     GpioPadding;
  uint32_t     BoardVoltageCoeffA;     
  uint32_t     BoardVoltageCoeffB;     
  uint32_t     BoardReserved[7];
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
  uint16_t     VcnClkAverageLpfTau;
  uint16_t     padding16;
  uint32_t     MmHubPadding[8];  
} DriverSmuConfig_t;
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
  uint16_t TemperatureHBM        ;
  uint16_t TemperatureVrGfx      ;
  uint16_t TemperatureVrSoc      ;
  uint16_t TemperatureVrMem      ;
  uint32_t ThrottlerStatus       ;
  uint16_t CurrFanSpeed          ;
  uint16_t AverageVclkFrequency  ;
  uint16_t AverageDclkFrequency  ;
  uint16_t VcnActivityPercentage ;
  uint32_t EnergyAccumulator     ;
  uint32_t Padding[2];
  uint32_t     MmHubPadding[8];  
} SmuMetrics_t;
typedef struct {
  uint16_t avgPsmCount[75];
  uint16_t minPsmCount[75];
  float    avgPsmVoltage[75];
  float    minPsmVoltage[75];
  uint32_t MmHubPadding[8];  
} AvfsDebugTable_t;
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
  uint32_t MmHubPadding[8];  
} AvfsFuseOverride_t;
typedef struct {
  uint8_t   Gfx_ActiveHystLimit;
  uint8_t   Gfx_IdleHystLimit;
  uint8_t   Gfx_FPS;
  uint8_t   Gfx_MinActiveFreqType;
  uint8_t   Gfx_BoosterFreqType;
  uint8_t   Gfx_MinFreqStep;                 
  uint8_t   Gfx_UseRlcBusy;
  uint8_t   PaddingGfx[3];
  uint16_t  Gfx_MinActiveFreq;               
  uint16_t  Gfx_BoosterFreq;                 
  uint16_t  Gfx_PD_Data_time_constant;       
  uint32_t  Gfx_PD_Data_limit_a;             
  uint32_t  Gfx_PD_Data_limit_b;             
  uint32_t  Gfx_PD_Data_limit_c;             
  uint32_t  Gfx_PD_Data_error_coeff;         
  uint32_t  Gfx_PD_Data_error_rate_coeff;    
  uint8_t   Mem_ActiveHystLimit;
  uint8_t   Mem_IdleHystLimit;
  uint8_t   Mem_FPS;
  uint8_t   Mem_MinActiveFreqType;
  uint8_t   Mem_BoosterFreqType;
  uint8_t   Mem_MinFreqStep;                 
  uint8_t   Mem_UseRlcBusy;
  uint8_t   PaddingMem[3];
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
  uint32_t  BusyThreshold;                   
  uint32_t  BusyHyst;
  uint32_t  IdleHyst;
  uint32_t  MmHubPadding[8];  
} DpmActivityMonitorCoeffInt_t;
#define TABLE_PPTABLE                 0
#define TABLE_AVFS                    1
#define TABLE_AVFS_PSM_DEBUG          2
#define TABLE_AVFS_FUSE_OVERRIDE      3
#define TABLE_PMSTATUSLOG             4
#define TABLE_SMU_METRICS             5
#define TABLE_DRIVER_SMU_CONFIG       6
#define TABLE_OVERDRIVE               7
#define TABLE_WAFL_XGMI_TOPOLOGY      8
#define TABLE_I2C_COMMANDS            9
#define TABLE_ACTIVITY_MONITOR_COEFF  10
#define TABLE_COUNT                   11
typedef enum {
  DF_SWITCH_TYPE_FAST = 0,
  DF_SWITCH_TYPE_SLOW,
  DF_SWITCH_TYPE_COUNT,
} DF_SWITCH_TYPE_e;
typedef enum {
  DRAM_BIT_WIDTH_DISABLED = 0,
  DRAM_BIT_WIDTH_X_8,
  DRAM_BIT_WIDTH_X_16,
  DRAM_BIT_WIDTH_X_32,
  DRAM_BIT_WIDTH_X_64,  
  DRAM_BIT_WIDTH_X_128,
  DRAM_BIT_WIDTH_COUNT,
} DRAM_BIT_WIDTH_TYPE_e;
#define REMOVE_FMAX_MARGIN_BIT     0x0
#define REMOVE_DCTOL_MARGIN_BIT    0x1
#define REMOVE_PLATFORM_MARGIN_BIT 0x2
#endif
