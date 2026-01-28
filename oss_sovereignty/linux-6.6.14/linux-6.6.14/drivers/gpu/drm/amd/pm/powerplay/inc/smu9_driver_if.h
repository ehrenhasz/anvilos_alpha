#ifndef SMU9_DRIVER_IF_H
#define SMU9_DRIVER_IF_H
#include "smu9.h"
#define SMU9_DRIVER_IF_VERSION 0xE
#define PPTABLE_V10_SMU_VERSION 1
#define NUM_GFXCLK_DPM_LEVELS  8
#define NUM_UVD_DPM_LEVELS     8
#define NUM_VCE_DPM_LEVELS     8
#define NUM_MP0CLK_DPM_LEVELS  8
#define NUM_UCLK_DPM_LEVELS    4
#define NUM_SOCCLK_DPM_LEVELS  8
#define NUM_DCEFCLK_DPM_LEVELS 8
#define NUM_LINK_LEVELS        2
#define MAX_GFXCLK_DPM_LEVEL  (NUM_GFXCLK_DPM_LEVELS  - 1)
#define MAX_UVD_DPM_LEVEL     (NUM_UVD_DPM_LEVELS     - 1)
#define MAX_VCE_DPM_LEVEL     (NUM_VCE_DPM_LEVELS     - 1)
#define MAX_MP0CLK_DPM_LEVEL  (NUM_MP0CLK_DPM_LEVELS  - 1)
#define MAX_UCLK_DPM_LEVEL    (NUM_UCLK_DPM_LEVELS    - 1)
#define MAX_SOCCLK_DPM_LEVEL  (NUM_SOCCLK_DPM_LEVELS  - 1)
#define MAX_DCEFCLK_DPM_LEVEL (NUM_DCEFCLK_DPM_LEVELS - 1)
#define MAX_LINK_DPM_LEVEL    (NUM_LINK_LEVELS        - 1)
#define MIN_GFXCLK_DPM_LEVEL  0
#define MIN_UVD_DPM_LEVEL     0
#define MIN_VCE_DPM_LEVEL     0
#define MIN_MP0CLK_DPM_LEVEL  0
#define MIN_UCLK_DPM_LEVEL    0
#define MIN_SOCCLK_DPM_LEVEL  0
#define MIN_DCEFCLK_DPM_LEVEL 0
#define MIN_LINK_DPM_LEVEL    0
#define NUM_EVV_VOLTAGE_LEVELS 8
#define MAX_EVV_VOLTAGE_LEVEL (NUM_EVV_VOLTAGE_LEVELS - 1)
#define MIN_EVV_VOLTAGE_LEVEL 0
#define NUM_PSP_LEVEL_MAP 4
#define PPSMC_GeminiModeNone   0   
#define PPSMC_GeminiModeMaster 1   
#define PPSMC_GeminiModeSlave  2   
#define VOLTAGE_MODE_AVFS_INTERPOLATE 0
#define VOLTAGE_MODE_AVFS_WORST_CASE  1
#define VOLTAGE_MODE_STATIC           2
typedef struct {
  uint32_t FbMult;  
  uint32_t SsFbMult;  
  uint16_t SsSlewFrac;
  uint8_t  SsOn;
  uint8_t  Did;       
} PllSetting_t;
typedef struct {
  int32_t a0;
  int32_t a1;
  int32_t a2;
  uint8_t a0_shift;
  uint8_t a1_shift;
  uint8_t a2_shift;
  uint8_t padding;
} GbVdroopTable_t;
typedef struct {
  int32_t m1;
  int32_t m2;
  int32_t b;
  uint8_t m1_shift;
  uint8_t m2_shift;
  uint8_t b_shift;
  uint8_t padding;
} QuadraticInt_t;
#define NUM_DSPCLK_LEVELS 8
typedef enum {
  DSPCLK_DCEFCLK = 0,
  DSPCLK_DISPCLK,
  DSPCLK_PIXCLK,
  DSPCLK_PHYCLK,
  DSPCLK_COUNT,
} DSPCLK_e;
typedef struct {
  uint16_t Freq;  
  uint16_t Vid;   
} DisplayClockTable_t;
#pragma pack(push, 1)
typedef struct {
  uint16_t SocketPowerLimit;  
  uint16_t TdcLimit;          
  uint16_t EdcLimit;          
  uint16_t TedgeLimit;        
  uint16_t ThotspotLimit;     
  uint16_t ThbmLimit;         
  uint16_t Tvr_socLimit;      
  uint16_t Tvr_memLimit;      
  uint16_t Tliquid1Limit;     
  uint16_t Tliquid2Limit;     
  uint16_t TplxLimit;         
  uint16_t LoadLineResistance;  
  uint32_t FitLimit;          
  uint8_t  Liquid1_I2C_address;
  uint8_t  Liquid2_I2C_address;
  uint8_t  Vr_I2C_address;
  uint8_t  Plx_I2C_address;
  uint8_t  GeminiMode;
  uint8_t  spare17[3];
  uint32_t GeminiApertureHigh;
  uint32_t GeminiApertureLow;
  uint8_t  Liquid_I2C_LineSCL;
  uint8_t  Liquid_I2C_LineSDA;
  uint8_t  Vr_I2C_LineSCL;
  uint8_t  Vr_I2C_LineSDA;
  uint8_t  Plx_I2C_LineSCL;
  uint8_t  Plx_I2C_LineSDA;
  uint8_t  paddingx[2];
  uint8_t  UlvOffsetVid;      
  uint8_t  UlvSmnclkDid;      
  uint8_t  UlvMp1clkDid;      
  uint8_t  UlvGfxclkBypass;   
  uint8_t      SocVid[NUM_EVV_VOLTAGE_LEVELS];
  uint8_t      MinVoltageVid;  
  uint8_t      MaxVoltageVid;  
  uint8_t      MaxVidStep;  
  uint8_t      padding8;
  uint8_t      UlvPhaseSheddingPsi0;  
  uint8_t      UlvPhaseSheddingPsi1;  
  uint8_t      padding8_2[2];
  PllSetting_t GfxclkLevel[NUM_GFXCLK_DPM_LEVELS];
  uint8_t      SocclkDid[NUM_SOCCLK_DPM_LEVELS];           
  uint8_t      SocDpmVoltageIndex[NUM_SOCCLK_DPM_LEVELS];
  uint8_t      VclkDid[NUM_UVD_DPM_LEVELS];             
  uint8_t      DclkDid[NUM_UVD_DPM_LEVELS];             
  uint8_t      UvdDpmVoltageIndex[NUM_UVD_DPM_LEVELS];
  uint8_t      EclkDid[NUM_VCE_DPM_LEVELS];             
  uint8_t      VceDpmVoltageIndex[NUM_VCE_DPM_LEVELS];
  uint8_t      Mp0clkDid[NUM_MP0CLK_DPM_LEVELS];           
  uint8_t      Mp0DpmVoltageIndex[NUM_MP0CLK_DPM_LEVELS];
  DisplayClockTable_t DisplayClockTable[DSPCLK_COUNT][NUM_DSPCLK_LEVELS];
  QuadraticInt_t      DisplayClock2Gfxclk[DSPCLK_COUNT];
  uint8_t      GfxDpmVoltageMode;
  uint8_t      SocDpmVoltageMode;
  uint8_t      UclkDpmVoltageMode;
  uint8_t      UvdDpmVoltageMode;
  uint8_t      VceDpmVoltageMode;
  uint8_t      Mp0DpmVoltageMode;
  uint8_t      DisplayDpmVoltageMode;
  uint8_t      padding8_3;
  uint16_t     GfxclkSlewRate;
  uint16_t     padding;
  uint32_t     LowGfxclkInterruptThreshold;   
  uint8_t      GfxclkAverageAlpha;
  uint8_t      SocclkAverageAlpha;
  uint8_t      UclkAverageAlpha;
  uint8_t      GfxActivityAverageAlpha;
  uint8_t      MemVid[NUM_UCLK_DPM_LEVELS];     
  PllSetting_t UclkLevel[NUM_UCLK_DPM_LEVELS];    
  uint8_t      MemSocVoltageIndex[NUM_UCLK_DPM_LEVELS];
  uint8_t      LowestUclkReservedForUlv;  
  uint8_t      paddingUclk[3];
  uint16_t     NumMemoryChannels;   
  uint16_t     MemoryChannelWidth;  
  uint8_t      CksEnable[NUM_GFXCLK_DPM_LEVELS];
  uint8_t      CksVidOffset[NUM_GFXCLK_DPM_LEVELS];
  uint8_t      PspLevelMap[NUM_PSP_LEVEL_MAP];
  uint8_t     PcieGenSpeed[NUM_LINK_LEVELS];            
  uint8_t     PcieLaneCount[NUM_LINK_LEVELS];           
  uint8_t     LclkDid[NUM_LINK_LEVELS];                 
  uint8_t     paddingLinkDpm[2];
  uint16_t     FanStopTemp;           
  uint16_t     FanStartTemp;          
  uint16_t     FanGainEdge;
  uint16_t     FanGainHotspot;
  uint16_t     FanGainLiquid;
  uint16_t     FanGainVrVddc;
  uint16_t     FanGainVrMvdd;
  uint16_t     FanGainPlx;
  uint16_t     FanGainHbm;
  uint16_t     FanPwmMin;
  uint16_t     FanAcousticLimitRpm;
  uint16_t     FanThrottlingRpm;
  uint16_t     FanMaximumRpm;
  uint16_t     FanTargetTemperature;
  uint16_t     FanTargetGfxclk;
  uint8_t      FanZeroRpmEnable;
  uint8_t      FanSpare;
  int16_t      FuzzyFan_ErrorSetDelta;
  int16_t      FuzzyFan_ErrorRateSetDelta;
  int16_t      FuzzyFan_PwmSetDelta;
  uint16_t     FuzzyFan_Reserved;
  uint8_t      AcDcGpio;         
  uint8_t      AcDcPolarity;     
  uint8_t      VR0HotGpio;       
  uint8_t      VR0HotPolarity;   
  uint8_t      VR1HotGpio;       
  uint8_t      VR1HotPolarity;   
  uint8_t      Padding1;        
  uint8_t      Padding2;        
  uint8_t      LedPin0;          
  uint8_t      LedPin1;          
  uint8_t      LedPin2;          
  uint8_t      padding8_4;
  uint8_t      OverrideBtcGbCksOn;
  uint8_t      OverrideAvfsGbCksOn;
  uint8_t      PaddingAvfs8[2];
  GbVdroopTable_t BtcGbVdroopTableCksOn;
  GbVdroopTable_t BtcGbVdroopTableCksOff;
  QuadraticInt_t  AvfsGbCksOn;   
  QuadraticInt_t  AvfsGbCksOff;  
  uint8_t      StaticVoltageOffsetVid[NUM_GFXCLK_DPM_LEVELS];  
  uint32_t     AConstant[3];
  uint16_t     DC_tol_sigma;
  uint16_t     Platform_mean;
  uint16_t     Platform_sigma;
  uint16_t     PSM_Age_CompFactor;
  uint32_t     DpmLevelPowerDelta;
  uint8_t      EnableBoostState;
  uint8_t      AConstant_Shift;
  uint8_t      DC_tol_sigma_Shift;
  uint8_t      PSM_Age_CompFactor_Shift;
  uint16_t     BoostStartTemperature;
  uint16_t     BoostStopTemperature;
  PllSetting_t GfxBoostState;
  uint8_t      AcgEnable[NUM_GFXCLK_DPM_LEVELS];
  GbVdroopTable_t AcgBtcGbVdroopTable;
  QuadraticInt_t  AcgAvfsGb;
  uint32_t     AcgFreqTable[NUM_GFXCLK_DPM_LEVELS];
  uint32_t     MmHubPadding[3];  
} PPTable_t;
#pragma pack(pop)
typedef struct {
  uint16_t MinClock;  
  uint16_t MaxClock;  
  uint16_t MinUclk;
  uint16_t MaxUclk;
  uint8_t  WmSetting;
  uint8_t  Padding[3];
} WatermarkRowGeneric_t;
#define NUM_WM_RANGES 4
typedef enum {
  WM_SOCCLK = 0,
  WM_DCEFCLK,
  WM_COUNT,
} WM_CLOCK_e;
typedef struct {
  WatermarkRowGeneric_t WatermarkRow[WM_COUNT][NUM_WM_RANGES];
  uint32_t     MmHubPadding[7];  
} Watermarks_t;
#ifdef PPTABLE_V10_SMU_VERSION
typedef struct {
  float        AvfsGbCksOn[NUM_GFXCLK_DPM_LEVELS];
  float        AcBtcGbCksOn[NUM_GFXCLK_DPM_LEVELS];
  float        AvfsGbCksOff[NUM_GFXCLK_DPM_LEVELS];
  float        AcBtcGbCksOff[NUM_GFXCLK_DPM_LEVELS];
  float        DcBtcGb;
  uint32_t     MmHubPadding[7];  
} AvfsTable_t;
#else
typedef struct {
  uint32_t     AvfsGbCksOn[NUM_GFXCLK_DPM_LEVELS];
  uint32_t     AcBtcGbCksOn[NUM_GFXCLK_DPM_LEVELS];
  uint32_t     AvfsGbCksOff[NUM_GFXCLK_DPM_LEVELS];
  uint32_t     AcBtcGbCksOff[NUM_GFXCLK_DPM_LEVELS];
  uint32_t     DcBtcGb;
  uint32_t     MmHubPadding[7];  
} AvfsTable_t;
#endif
typedef struct {
  uint16_t avgPsmCount[30];
  uint16_t minPsmCount[30];
  float    avgPsmVoltage[30];
  float    minPsmVoltage[30];
  uint32_t MmHubPadding[7];  
} AvfsDebugTable_t;
typedef struct {
  uint8_t  AvfsEn;
  uint8_t  AvfsVersion;
  uint8_t  Padding[2];
  int32_t VFT0_m1;  
  int32_t VFT0_m2;  
  int32_t VFT0_b;   
  int32_t VFT1_m1;  
  int32_t VFT1_m2;  
  int32_t VFT1_b;   
  int32_t VFT2_m1;  
  int32_t VFT2_m2;  
  int32_t VFT2_b;   
  int32_t AvfsGb0_m1;  
  int32_t AvfsGb0_m2;  
  int32_t AvfsGb0_b;   
  int32_t AcBtcGb_m1;  
  int32_t AcBtcGb_m2;  
  int32_t AcBtcGb_b;   
  uint32_t AvfsTempCold;
  uint32_t AvfsTempMid;
  uint32_t AvfsTempHot;
  uint32_t InversionVoltage;  
  int32_t P2V_m1;  
  int32_t P2V_m2;  
  int32_t P2V_b;   
  uint32_t P2VCharzFreq;  
  uint32_t EnabledAvfsModules;
  uint32_t MmHubPadding[7];  
} AvfsFuseOverride_t;
#define TABLE_PPTABLE            0
#define TABLE_WATERMARKS         1
#define TABLE_AVFS               2
#define TABLE_AVFS_PSM_DEBUG     3
#define TABLE_AVFS_FUSE_OVERRIDE 4
#define TABLE_PMSTATUSLOG        5
#define TABLE_COUNT              6
#define UCLK_SWITCH_SLOW 0
#define UCLK_SWITCH_FAST 1
#define SQ_Enable_MASK 0x1
#define SQ_IR_MASK 0x2
#define SQ_PCC_MASK 0x4
#define SQ_EDC_MASK 0x8
#define TCP_Enable_MASK 0x100
#define TCP_IR_MASK 0x200
#define TCP_PCC_MASK 0x400
#define TCP_EDC_MASK 0x800
#define TD_Enable_MASK 0x10000
#define TD_IR_MASK 0x20000
#define TD_PCC_MASK 0x40000
#define TD_EDC_MASK 0x80000
#define DB_Enable_MASK 0x1000000
#define DB_IR_MASK 0x2000000
#define DB_PCC_MASK 0x4000000
#define DB_EDC_MASK 0x8000000
#define SQ_Enable_SHIFT 0
#define SQ_IR_SHIFT 1
#define SQ_PCC_SHIFT 2
#define SQ_EDC_SHIFT 3
#define TCP_Enable_SHIFT 8
#define TCP_IR_SHIFT 9
#define TCP_PCC_SHIFT 10
#define TCP_EDC_SHIFT 11
#define TD_Enable_SHIFT 16
#define TD_IR_SHIFT 17
#define TD_PCC_SHIFT 18
#define TD_EDC_SHIFT 19
#define DB_Enable_SHIFT 24
#define DB_IR_SHIFT 25
#define DB_PCC_SHIFT 26
#define DB_EDC_SHIFT 27
#define REMOVE_FMAX_MARGIN_BIT     0x0
#define REMOVE_DCTOL_MARGIN_BIT    0x1
#define REMOVE_PLATFORM_MARGIN_BIT 0x2
#endif
