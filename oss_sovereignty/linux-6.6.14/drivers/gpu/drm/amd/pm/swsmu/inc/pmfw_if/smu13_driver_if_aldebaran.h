 

#ifndef SMU13_DRIVER_IF_ALDEBARAN_H
#define SMU13_DRIVER_IF_ALDEBARAN_H

#define SMU13_DRIVER_IF_VERSION_ALDE 0x08

#define NUM_VCLK_DPM_LEVELS   8
#define NUM_DCLK_DPM_LEVELS   8
#define NUM_SOCCLK_DPM_LEVELS 8
#define NUM_LCLK_DPM_LEVELS   8
#define NUM_UCLK_DPM_LEVELS   4
#define NUM_FCLK_DPM_LEVELS   8
#define NUM_XGMI_DPM_LEVELS   4


#define FEATURE_DATA_CALCULATIONS       0
#define FEATURE_DPM_GFXCLK_BIT          1
#define FEATURE_DPM_UCLK_BIT            2
#define FEATURE_DPM_SOCCLK_BIT          3
#define FEATURE_DPM_FCLK_BIT            4
#define FEATURE_DPM_LCLK_BIT            5
#define FEATURE_DPM_XGMI_BIT            6
#define FEATURE_DS_GFXCLK_BIT           7
#define FEATURE_DS_SOCCLK_BIT           8
#define FEATURE_DS_LCLK_BIT             9
#define FEATURE_DS_FCLK_BIT             10
#define FEATURE_DS_UCLK_BIT             11
#define FEATURE_GFX_SS_BIT              12
#define FEATURE_DPM_VCN_BIT             13
#define FEATURE_RSMU_SMN_CG_BIT         14
#define FEATURE_WAFL_CG_BIT             15
#define FEATURE_PPT_BIT                 16
#define FEATURE_TDC_BIT                 17
#define FEATURE_APCC_PLUS_BIT           18
#define FEATURE_APCC_DFLL_BIT           19
#define FEATURE_FW_CTF_BIT              20
#define FEATURE_THERMAL_BIT             21
#define FEATURE_OUT_OF_BAND_MONITOR_BIT 22
#define FEATURE_SPARE_23_BIT            23
#define FEATURE_XGMI_PER_LINK_PWR_DWN   24
#define FEATURE_DF_CSTATE               25
#define FEATURE_FUSE_CG_BIT             26
#define FEATURE_MP1_CG_BIT              27
#define FEATURE_SMUIO_CG_BIT            28
#define FEATURE_THM_CG_BIT              29
#define FEATURE_CLK_CG_BIT              30
#define FEATURE_EDC_BIT                 31
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


#define I2C_CONTROLLER_ENABLED  1
#define I2C_CONTROLLER_DISABLED 0



#define THROTTLER_PPT0_BIT         0
#define THROTTLER_PPT1_BIT         1
#define THROTTLER_TDC_GFX_BIT      2
#define THROTTLER_TDC_SOC_BIT      3
#define THROTTLER_TDC_HBM_BIT      4
#define THROTTLER_SPARE_5          5
#define THROTTLER_TEMP_GPU_BIT     6
#define THROTTLER_TEMP_MEM_BIT     7
#define THORTTLER_SPARE_8          8
#define THORTTLER_SPARE_9          9
#define THORTTLER_SPARE_10         10
#define THROTTLER_TEMP_VR_GFX_BIT  11
#define THROTTLER_TEMP_VR_SOC_BIT  12
#define THROTTLER_TEMP_VR_MEM_BIT  13
#define THORTTLER_SPARE_14         14
#define THORTTLER_SPARE_15         15
#define THORTTLER_SPARE_16         16
#define THORTTLER_SPARE_17         17
#define THORTTLER_SPARE_18         18
#define THROTTLER_APCC_BIT         19


#define TABLE_TRANSFER_OK         0x0
#define TABLE_TRANSFER_FAILED     0xFF
#define TABLE_TRANSFER_PENDING    0xAB


#define NUM_I2C_CONTROLLERS                8

#define I2C_CONTROLLER_ENABLED             1
#define I2C_CONTROLLER_DISABLED            0

#define MAX_SW_I2C_COMMANDS                24

#define ALDEBARAN_UMC_CHANNEL_NUM    32

typedef enum {
  I2C_CONTROLLER_PORT_0, 
  I2C_CONTROLLER_PORT_1, 
  I2C_CONTROLLER_PORT_COUNT,
} I2cControllerPort_e;

typedef enum {
  I2C_CONTROLLER_THROTTLER_TYPE_NONE,
  I2C_CONTROLLER_THROTTLER_VR_GFX0,
  I2C_CONTROLLER_THROTTLER_VR_GFX1,
  I2C_CONTROLLER_THROTTLER_VR_SOC,
  I2C_CONTROLLER_THROTTLER_VR_MEM,
  I2C_CONTROLLER_THROTTLER_COUNT,
} I2cControllerThrottler_e;

typedef enum {
  I2C_CONTROLLER_PROTOCOL_VR_MP2855,
  I2C_CONTROLLER_PROTOCOL_COUNT,
} I2cControllerProtocol_e;

typedef struct {
  uint8_t   Enabled;
  uint8_t   Speed;
  uint8_t   SlaveAddress;
  uint8_t   ControllerPort;
  uint8_t   ThermalThrotter;
  uint8_t   I2cProtocol;
  uint8_t   PaddingConfig[2];
} I2cControllerConfig_t;

typedef enum {
  I2C_PORT_SVD_SCL,
  I2C_PORT_GPIO,
} I2cPort_e;

typedef enum {
  I2C_SPEED_FAST_50K,     
  I2C_SPEED_FAST_100K,    
  I2C_SPEED_FAST_400K,    
  I2C_SPEED_FAST_PLUS_1M, 
  I2C_SPEED_HIGH_1M,      
  I2C_SPEED_HIGH_2M,      
  I2C_SPEED_COUNT,
} I2cSpeed_e;

typedef enum {
  I2C_CMD_READ,
  I2C_CMD_WRITE,
  I2C_CMD_COUNT,
} I2cCmdType_e;

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
  uint8_t    I2CcontrollerPort; 
  uint8_t    I2CSpeed;          
  uint8_t    SlaveAddress;      
  uint8_t    NumCmds;           
  SwI2cCmd_t SwI2cCmds[MAX_SW_I2C_COMMANDS];
} SwI2cRequest_t; 

typedef struct {
  SwI2cRequest_t SwI2cRequest;
  uint32_t       Spare[8];
  uint32_t       MmHubPadding[8]; 
} SwI2cRequestExternal_t;

typedef struct {
  uint32_t a;  
  uint32_t b;  
  uint32_t c;  
} QuadraticInt_t;

typedef struct {
  uint32_t m;  
  uint32_t b;  
} LinearInt_t;

typedef enum {
  GFXCLK_SOURCE_PLL,
  GFXCLK_SOURCE_DFLL,
  GFXCLK_SOURCE_COUNT,
} GfxclkSrc_e;

typedef enum {
  PPCLK_GFXCLK,
  PPCLK_VCLK,
  PPCLK_DCLK,
  PPCLK_SOCCLK,
  PPCLK_UCLK,
  PPCLK_FCLK,
  PPCLK_LCLK,
  PPCLK_COUNT,
} PPCLK_e;

typedef enum {
  GPIO_INT_POLARITY_ACTIVE_LOW,
  GPIO_INT_POLARITY_ACTIVE_HIGH,
} GpioIntPolarity_e;


typedef enum {
  UCLK_DPM_MODE_BANDWIDTH,
  UCLK_DPM_MODE_LATENCY,
} UCLK_DPM_MODE_e;

typedef struct {
  uint8_t        StartupLevel;
  uint8_t        NumDiscreteLevels;   
  uint16_t       SsFmin;              
  LinearInt_t    ConversionToAvfsClk; 
  QuadraticInt_t SsCurve;             
} DpmDescriptor_t;

#pragma pack(push, 1)
typedef struct {
  uint32_t Version;

  
  uint32_t FeaturesToRun[2];

  
  uint16_t PptLimit;      
  uint16_t TdcLimitGfx;   
  uint16_t TdcLimitSoc;   
  uint16_t TdcLimitHbm;   
  uint16_t ThotspotLimit; 
  uint16_t TmemLimit;     
  uint16_t Tvr_gfxLimit;  
  uint16_t Tvr_memLimit;  
  uint16_t Tvr_socLimit;  
  uint16_t PaddingLimit;

  
  uint16_t MaxVoltageGfx; 
  uint16_t MaxVoltageSoc; 

  
  DpmDescriptor_t DpmDescriptor[PPCLK_COUNT];

  uint8_t  DidTableVclk[NUM_VCLK_DPM_LEVELS];     
  uint8_t  DidTableDclk[NUM_DCLK_DPM_LEVELS];     
  uint8_t  DidTableSocclk[NUM_SOCCLK_DPM_LEVELS]; 
  uint8_t  DidTableLclk[NUM_LCLK_DPM_LEVELS];     
  uint32_t FidTableFclk[NUM_FCLK_DPM_LEVELS];     
  uint8_t  DidTableFclk[NUM_FCLK_DPM_LEVELS];     
  uint32_t FidTableUclk[NUM_UCLK_DPM_LEVELS];     
  uint8_t  DidTableUclk[NUM_UCLK_DPM_LEVELS];     

  uint32_t StartupFidPll0; 
  uint32_t StartupFidPll4; 
  uint32_t StartupFidPll5; 

  uint8_t  StartupSmnclkDid;
  uint8_t  StartupMp0clkDid;
  uint8_t  StartupMp1clkDid;
  uint8_t  StartupWaflclkDid;
  uint8_t  StartupGfxavfsclkDid;
  uint8_t  StartupMpioclkDid;
  uint8_t  StartupDxioclkDid;
  uint8_t  spare123;

  uint8_t  StartupVidGpu0Svi0Plane0; 
  uint8_t  StartupVidGpu0Svi0Plane1; 
  uint8_t  StartupVidGpu0Svi1Plane0; 
  uint8_t  StartupVidGpu0Svi1Plane1; 

  uint8_t  StartupVidGpu1Svi0Plane0; 
  uint8_t  StartupVidGpu1Svi0Plane1; 
  uint8_t  StartupVidGpu1Svi1Plane0; 
  uint8_t  StartupVidGpu1Svi1Plane1; 

  
  uint16_t GfxclkFmax;   
  uint16_t GfxclkFmin;   
  uint16_t GfxclkFidle;  
  uint16_t GfxclkFinit;  
  uint8_t  GfxclkSource; 
  uint8_t  spare1[2];
  uint8_t  StartupGfxclkDid;
  uint32_t StartupGfxclkFid;

  
  uint16_t GFX_Guardband_Freq[8];         
  int16_t  GFX_Guardband_Voltage_Cold[8]; 
  int16_t  GFX_Guardband_Voltage_Mid[8];  
  int16_t  GFX_Guardband_Voltage_Hot[8];  

  uint16_t SOC_Guardband_Freq[8];         
  int16_t  SOC_Guardband_Voltage_Cold[8]; 
  int16_t  SOC_Guardband_Voltage_Mid[8];  
  int16_t  SOC_Guardband_Voltage_Hot[8];  

  
  uint16_t DcBtcEnabled;
  int16_t  DcBtcMin;       
  int16_t  DcBtcMax;       
  int16_t  DcBtcGb;        

  
  uint8_t  XgmiLinkSpeed[NUM_XGMI_DPM_LEVELS]; 
  uint8_t  XgmiLinkWidth[NUM_XGMI_DPM_LEVELS]; 
  uint8_t  XgmiStartupLevel;
  uint8_t  spare12[3];

  
  uint16_t GFX_PPVmin_Enabled;
  uint16_t GFX_Vmin_Plat_Offset_Hot;  
  uint16_t GFX_Vmin_Plat_Offset_Cold; 
  uint16_t GFX_Vmin_Hot_T0;           
  uint16_t GFX_Vmin_Cold_T0;          
  uint16_t GFX_Vmin_Hot_Eol;          
  uint16_t GFX_Vmin_Cold_Eol;         
  uint16_t GFX_Vmin_Aging_Offset;     
  uint16_t GFX_Vmin_Temperature_Hot;  
  uint16_t GFX_Vmin_Temperature_Cold; 

  
  uint16_t SOC_PPVmin_Enabled;
  uint16_t SOC_Vmin_Plat_Offset_Hot;  
  uint16_t SOC_Vmin_Plat_Offset_Cold; 
  uint16_t SOC_Vmin_Hot_T0;           
  uint16_t SOC_Vmin_Cold_T0;          
  uint16_t SOC_Vmin_Hot_Eol;          
  uint16_t SOC_Vmin_Cold_Eol;         
  uint16_t SOC_Vmin_Aging_Offset;     
  uint16_t SOC_Vmin_Temperature_Hot;  
  uint16_t SOC_Vmin_Temperature_Cold; 

  
  uint32_t ApccPlusResidencyLimit; 

  
  uint16_t DeterminismVoltageOffset; 
  uint16_t spare22;

  
  uint32_t spare3[14];

  
  
  uint16_t GfxMaxCurrent; 
  int8_t   GfxOffset;     
  uint8_t  Padding_TelemetryGfx;

  uint16_t SocMaxCurrent; 
  int8_t   SocOffset;     
  uint8_t  Padding_TelemetrySoc;

  uint16_t MemMaxCurrent; 
  int8_t   MemOffset;     
  uint8_t  Padding_TelemetryMem;

  uint16_t BoardMaxCurrent; 
  int8_t   BoardOffset;     
  uint8_t  Padding_TelemetryBoardInput;

  
  uint32_t BoardVoltageCoeffA; 
  uint32_t BoardVoltageCoeffB; 

  
  uint8_t  VR0HotGpio;     
  uint8_t  VR0HotPolarity; 
  uint8_t  VR1HotGpio;     
  uint8_t  VR1HotPolarity; 

  
  uint8_t  UclkSpreadEnabled; 
  uint8_t  UclkSpreadPercent; 
  uint16_t UclkSpreadFreq;    

  
  uint8_t  FclkSpreadEnabled; 
  uint8_t  FclkSpreadPercent; 
  uint16_t FclkSpreadFreq;    

  
  I2cControllerConfig_t  I2cControllers[NUM_I2C_CONTROLLERS];

  
  uint8_t  GpioI2cScl; 
  uint8_t  GpioI2cSda; 
  uint16_t spare5;

  uint16_t XgmiMaxCurrent; 
  int8_t   XgmiOffset;     
  uint8_t  Padding_TelemetryXgmi;

  uint16_t  EdcPowerLimit;
  uint16_t  spare6;

  
  uint32_t reserved[14];

} PPTable_t;
#pragma pack(pop)

typedef struct {
  
  uint16_t     GfxclkAverageLpfTau;
  uint16_t     SocclkAverageLpfTau;
  uint16_t     UclkAverageLpfTau;
  uint16_t     GfxActivityLpfTau;
  uint16_t     UclkActivityLpfTau;

  uint16_t     SocketPowerLpfTau;

  uint32_t     Spare[8];
  
  uint32_t     MmHubPadding[8]; 
} DriverSmuConfig_t;

typedef struct {
  uint16_t CurrClock[PPCLK_COUNT];
  uint16_t Padding1              ;
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

  uint32_t PublicSerialNumLower32;
  uint32_t PublicSerialNumUpper32;
  uint16_t TemperatureAllHBM[4]  ;
  uint32_t GfxBusyAcc            ;
  uint32_t DramBusyAcc           ;
  uint32_t EnergyAcc64bitLow     ; 
  uint32_t EnergyAcc64bitHigh    ;
  uint32_t TimeStampLow          ; 
  uint32_t TimeStampHigh         ;

  
  uint32_t     MmHubPadding[8]; 
} SmuMetrics_t;


typedef struct {
  uint16_t avgPsmCount[76];
  uint16_t minPsmCount[76];
  float    avgPsmVoltage[76];
  float    minPsmVoltage[76];

  uint32_t MmHubPadding[8]; 
} AvfsDebugTable_t;

typedef struct {
	uint64_t mca_umc_status;
	uint64_t mca_umc_addr;
	uint16_t ce_count_lo_chip;
	uint16_t ce_count_hi_chip;

	uint32_t eccPadding;
} EccInfo_t;

typedef struct {
	uint64_t mca_umc_status;
	uint64_t mca_umc_addr;

	uint16_t ce_count_lo_chip;
	uint16_t ce_count_hi_chip;

	uint32_t eccPadding;

	uint64_t mca_ceumc_addr;
} EccInfo_V2_t;

typedef struct {
	union {
		EccInfo_t  EccInfo[ALDEBARAN_UMC_CHANNEL_NUM];
		EccInfo_V2_t EccInfo_V2[ALDEBARAN_UMC_CHANNEL_NUM];
	};
} EccInfoTable_t;




#define TABLE_PPTABLE                 0
#define TABLE_AVFS_PSM_DEBUG          1
#define TABLE_AVFS_FUSE_OVERRIDE      2
#define TABLE_PMSTATUSLOG             3
#define TABLE_SMU_METRICS             4
#define TABLE_DRIVER_SMU_CONFIG       5
#define TABLE_I2C_COMMANDS            6
#define TABLE_ECCINFO                 7
#define TABLE_COUNT                   8

#endif
