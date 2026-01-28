#ifndef SMU_13_0_6_PMFW_H
#define SMU_13_0_6_PMFW_H
#define NUM_VCLK_DPM_LEVELS   4
#define NUM_DCLK_DPM_LEVELS   4
#define NUM_SOCCLK_DPM_LEVELS 4
#define NUM_LCLK_DPM_LEVELS   4
#define NUM_UCLK_DPM_LEVELS   4
#define NUM_FCLK_DPM_LEVELS   4
#define NUM_XGMI_DPM_LEVELS   2
#define NUM_CXL_BITRATES      4
#define NUM_PCIE_BITRATES     4
#define NUM_XGMI_BITRATES     4
#define NUM_XGMI_WIDTHS       3
typedef enum {
    FEATURE_DATA_CALCULATION            = 0,
    FEATURE_DPM_CCLK                    = 1,
    FEATURE_DPM_FCLK                    = 2,
    FEATURE_DPM_GFXCLK                  = 3,
    FEATURE_DPM_LCLK                    = 4,
    FEATURE_DPM_SOCCLK                  = 5,
    FEATURE_DPM_UCLK                    = 6,
    FEATURE_DPM_VCN                     = 7,
    FEATURE_DPM_XGMI                    = 8,
    FEATURE_DS_FCLK                     = 9,
   FEATURE_DS_GFXCLK                   = 10,
   FEATURE_DS_LCLK                     = 11,
   FEATURE_DS_MP0CLK                   = 12,
   FEATURE_DS_MP1CLK                   = 13,
   FEATURE_DS_MPIOCLK                  = 14,
   FEATURE_DS_SOCCLK                   = 15,
   FEATURE_DS_VCN                      = 16,
   FEATURE_APCC_DFLL                   = 17,
   FEATURE_APCC_PLUS                   = 18,
   FEATURE_DF_CSTATE                   = 19,
   FEATURE_CC6                         = 20,
   FEATURE_PC6                         = 21,
   FEATURE_CPPC                        = 22,
   FEATURE_PPT                         = 23,
   FEATURE_TDC                         = 24,
   FEATURE_THERMAL                     = 25,
   FEATURE_SOC_PCC                     = 26,
   FEATURE_CCD_PCC                     = 27,
   FEATURE_CCD_EDC                     = 28,
   FEATURE_PROCHOT                     = 29,
   FEATURE_DVO_CCLK                    = 30,
   FEATURE_FDD_AID_HBM                 = 31,
   FEATURE_FDD_AID_SOC                 = 32,
   FEATURE_FDD_XCD_EDC                 = 33,
   FEATURE_FDD_XCD_XVMIN               = 34,
   FEATURE_FW_CTF                      = 35,
   FEATURE_GFXOFF                      = 36,
   FEATURE_SMU_CG                      = 37,
   FEATURE_PSI7                        = 38,
   FEATURE_CSTATE_BOOST                = 39,
   FEATURE_XGMI_PER_LINK_PWR_DOWN      = 40,
   FEATURE_CXL_QOS                     = 41,
   FEATURE_SOC_DC_RTC                  = 42,
   FEATURE_GFX_DC_RTC                  = 43,
   NUM_FEATURES                        = 44
} FEATURE_LIST_e;
typedef enum {
  PCIE_LINK_SPEED_INDEX_TABLE_GEN1,
  PCIE_LINK_SPEED_INDEX_TABLE_GEN2,
  PCIE_LINK_SPEED_INDEX_TABLE_GEN3,
  PCIE_LINK_SPEED_INDEX_TABLE_GEN4,
  PCIE_LINK_SPEED_INDEX_TABLE_GEN4_ESM,
  PCIE_LINK_SPEED_INDEX_TABLE_GEN5,
  PCIE_LINK_SPEED_INDEX_TABLE_COUNT
} PCIE_LINK_SPEED_INDEX_TABLE_e;
typedef enum {
  VOLTAGE_COLD_0,
  VOLTAGE_COLD_1,
  VOLTAGE_COLD_2,
  VOLTAGE_COLD_3,
  VOLTAGE_COLD_4,
  VOLTAGE_COLD_5,
  VOLTAGE_COLD_6,
  VOLTAGE_COLD_7,
  VOLTAGE_MID_0,
  VOLTAGE_MID_1,
  VOLTAGE_MID_2,
  VOLTAGE_MID_3,
  VOLTAGE_MID_4,
  VOLTAGE_MID_5,
  VOLTAGE_MID_6,
  VOLTAGE_MID_7,
  VOLTAGE_HOT_0,
  VOLTAGE_HOT_1,
  VOLTAGE_HOT_2,
  VOLTAGE_HOT_3,
  VOLTAGE_HOT_4,
  VOLTAGE_HOT_5,
  VOLTAGE_HOT_6,
  VOLTAGE_HOT_7,
  VOLTAGE_GUARDBAND_COUNT
} GFX_GUARDBAND_e;
#define SMU_METRICS_TABLE_VERSION 0x7
typedef struct __attribute__((packed, aligned(4))) {
  uint32_t AccumulationCounter;
  uint32_t MaxSocketTemperature;
  uint32_t MaxVrTemperature;
  uint32_t MaxHbmTemperature;
  uint64_t MaxSocketTemperatureAcc;
  uint64_t MaxVrTemperatureAcc;
  uint64_t MaxHbmTemperatureAcc;
  uint32_t SocketPowerLimit;
  uint32_t MaxSocketPowerLimit;
  uint32_t SocketPower;
  uint64_t Timestamp;
  uint64_t SocketEnergyAcc;
  uint64_t CcdEnergyAcc;
  uint64_t XcdEnergyAcc;
  uint64_t AidEnergyAcc;
  uint64_t HbmEnergyAcc;
  uint32_t CclkFrequencyLimit;
  uint32_t GfxclkFrequencyLimit;
  uint32_t FclkFrequency;
  uint32_t UclkFrequency;
  uint32_t SocclkFrequency[4];
  uint32_t VclkFrequency[4];
  uint32_t DclkFrequency[4];
  uint32_t LclkFrequency[4];
  uint64_t GfxclkFrequencyAcc[8];
  uint64_t CclkFrequencyAcc[96];
  uint32_t MaxCclkFrequency;
  uint32_t MinCclkFrequency;
  uint32_t MaxGfxclkFrequency;
  uint32_t MinGfxclkFrequency;
  uint32_t FclkFrequencyTable[4];
  uint32_t UclkFrequencyTable[4];
  uint32_t SocclkFrequencyTable[4];
  uint32_t VclkFrequencyTable[4];
  uint32_t DclkFrequencyTable[4];
  uint32_t LclkFrequencyTable[4];
  uint32_t MaxLclkDpmRange;
  uint32_t MinLclkDpmRange;
  uint32_t XgmiWidth;
  uint32_t XgmiBitrate;
  uint64_t XgmiReadBandwidthAcc[8];
  uint64_t XgmiWriteBandwidthAcc[8];
  uint32_t SocketC0Residency;
  uint32_t SocketGfxBusy;
  uint32_t DramBandwidthUtilization;
  uint64_t SocketC0ResidencyAcc;
  uint64_t SocketGfxBusyAcc;
  uint64_t DramBandwidthAcc;
  uint32_t MaxDramBandwidth;
  uint64_t DramBandwidthUtilizationAcc;
  uint64_t PcieBandwidthAcc[4];
  uint32_t ProchotResidencyAcc;
  uint32_t PptResidencyAcc;
  uint32_t SocketThmResidencyAcc;
  uint32_t VrThmResidencyAcc;
  uint32_t HbmThmResidencyAcc;
  uint32_t GfxLockXCDMak;
  uint32_t GfxclkFrequency[8];
  uint64_t PublicSerialNumber_AID[4];
  uint64_t PublicSerialNumber_XCD[8];
  uint64_t PublicSerialNumber_CCD[12];
} MetricsTable_t;
#define SMU_VF_METRICS_TABLE_VERSION 0x3
typedef struct __attribute__((packed, aligned(4))) {
  uint32_t AccumulationCounter;
  uint32_t InstGfxclk_TargFreq;
  uint64_t AccGfxclk_TargFreq;
  uint64_t AccGfxRsmuDpm_Busy;
} VfMetricsTable_t;
#endif
