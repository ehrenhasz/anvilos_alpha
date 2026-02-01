 

#ifndef SMU13_DRIVER_IF_V13_0_0_H
#define SMU13_DRIVER_IF_V13_0_0_H

#define SMU13_0_0_DRIVER_IF_VERSION 0x3D


#define PPTABLE_VERSION 0x2B

#define NUM_GFXCLK_DPM_LEVELS    16
#define NUM_SOCCLK_DPM_LEVELS    8
#define NUM_MP0CLK_DPM_LEVELS    2
#define NUM_DCLK_DPM_LEVELS      8
#define NUM_VCLK_DPM_LEVELS      8
#define NUM_DISPCLK_DPM_LEVELS   8
#define NUM_DPPCLK_DPM_LEVELS    8
#define NUM_DPREFCLK_DPM_LEVELS  8
#define NUM_DCFCLK_DPM_LEVELS    8
#define NUM_DTBCLK_DPM_LEVELS    8
#define NUM_UCLK_DPM_LEVELS      4
#define NUM_LINK_LEVELS          3
#define NUM_FCLK_DPM_LEVELS      8
#define NUM_OD_FAN_MAX_POINTS    6


#define FEATURE_FW_DATA_READ_BIT              0
#define FEATURE_DPM_GFXCLK_BIT                1
#define FEATURE_DPM_GFX_POWER_OPTIMIZER_BIT   2
#define FEATURE_DPM_UCLK_BIT                  3
#define FEATURE_DPM_FCLK_BIT                  4
#define FEATURE_DPM_SOCCLK_BIT                5
#define FEATURE_DPM_MP0CLK_BIT                6
#define FEATURE_DPM_LINK_BIT                  7
#define FEATURE_DPM_DCN_BIT                   8
#define FEATURE_VMEMP_SCALING_BIT             9
#define FEATURE_VDDIO_MEM_SCALING_BIT         10
#define FEATURE_DS_GFXCLK_BIT                 11
#define FEATURE_DS_SOCCLK_BIT                 12
#define FEATURE_DS_FCLK_BIT                   13
#define FEATURE_DS_LCLK_BIT                   14
#define FEATURE_DS_DCFCLK_BIT                 15
#define FEATURE_DS_UCLK_BIT                   16
#define FEATURE_GFX_ULV_BIT                   17
#define FEATURE_FW_DSTATE_BIT                 18
#define FEATURE_GFXOFF_BIT                    19
#define FEATURE_BACO_BIT                      20
#define FEATURE_MM_DPM_BIT                    21
#define FEATURE_SOC_MPCLK_DS_BIT              22
#define FEATURE_BACO_MPCLK_DS_BIT             23
#define FEATURE_THROTTLERS_BIT                24
#define FEATURE_SMARTSHIFT_BIT                25
#define FEATURE_GTHR_BIT                      26
#define FEATURE_ACDC_BIT                      27
#define FEATURE_VR0HOT_BIT                    28
#define FEATURE_FW_CTF_BIT                    29
#define FEATURE_FAN_CONTROL_BIT               30
#define FEATURE_GFX_DCS_BIT                   31
#define FEATURE_GFX_READ_MARGIN_BIT           32
#define FEATURE_LED_DISPLAY_BIT               33
#define FEATURE_GFXCLK_SPREAD_SPECTRUM_BIT    34
#define FEATURE_OUT_OF_BAND_MONITOR_BIT       35
#define FEATURE_OPTIMIZED_VMIN_BIT            36
#define FEATURE_GFX_IMU_BIT                   37
#define FEATURE_BOOT_TIME_CAL_BIT             38
#define FEATURE_GFX_PCC_DFLL_BIT              39
#define FEATURE_SOC_CG_BIT                    40
#define FEATURE_DF_CSTATE_BIT                 41
#define FEATURE_GFX_EDC_BIT                   42
#define FEATURE_BOOT_POWER_OPT_BIT            43
#define FEATURE_CLOCK_POWER_DOWN_BYPASS_BIT   44
#define FEATURE_DS_VCN_BIT                    45
#define FEATURE_BACO_CG_BIT                   46
#define FEATURE_MEM_TEMP_READ_BIT             47
#define FEATURE_ATHUB_MMHUB_PG_BIT            48
#define FEATURE_SOC_PCC_BIT                   49
#define FEATURE_EDC_PWRBRK_BIT                50
#define FEATURE_BOMXCO_SVI3_PROG_BIT          51
#define FEATURE_SPARE_52_BIT                  52
#define FEATURE_SPARE_53_BIT                  53
#define FEATURE_SPARE_54_BIT                  54
#define FEATURE_SPARE_55_BIT                  55
#define FEATURE_SPARE_56_BIT                  56
#define FEATURE_SPARE_57_BIT                  57
#define FEATURE_SPARE_58_BIT                  58
#define FEATURE_SPARE_59_BIT                  59
#define FEATURE_SPARE_60_BIT                  60
#define FEATURE_SPARE_61_BIT                  61
#define FEATURE_SPARE_62_BIT                  62
#define FEATURE_SPARE_63_BIT                  63
#define NUM_FEATURES                          64

#define ALLOWED_FEATURE_CTRL_DEFAULT 0xFFFFFFFFFFFFFFFFULL
#define ALLOWED_FEATURE_CTRL_SCPM	((1 << FEATURE_DPM_GFXCLK_BIT) | \
									(1 << FEATURE_DPM_GFX_POWER_OPTIMIZER_BIT) | \
									(1 << FEATURE_DPM_UCLK_BIT) | \
									(1 << FEATURE_DPM_FCLK_BIT) | \
									(1 << FEATURE_DPM_SOCCLK_BIT) | \
									(1 << FEATURE_DPM_MP0CLK_BIT) | \
									(1 << FEATURE_DPM_LINK_BIT) | \
									(1 << FEATURE_DPM_DCN_BIT) | \
									(1 << FEATURE_DS_GFXCLK_BIT) | \
									(1 << FEATURE_DS_SOCCLK_BIT) | \
									(1 << FEATURE_DS_FCLK_BIT) | \
									(1 << FEATURE_DS_LCLK_BIT) | \
									(1 << FEATURE_DS_DCFCLK_BIT) | \
									(1 << FEATURE_DS_UCLK_BIT) | \
									(1ULL << FEATURE_DS_VCN_BIT))


typedef enum {
  FEATURE_PWR_ALL,
  FEATURE_PWR_S5,
  FEATURE_PWR_BACO,
  FEATURE_PWR_SOC,
  FEATURE_PWR_GFX,
  FEATURE_PWR_DOMAIN_COUNT,
} FEATURE_PWR_DOMAIN_e;



#define DEBUG_OVERRIDE_DISABLE_VOLT_LINK_VCN_FCLK      0x00000001
#define DEBUG_OVERRIDE_DISABLE_VOLT_LINK_DCN_FCLK      0x00000002
#define DEBUG_OVERRIDE_DISABLE_VOLT_LINK_MP0_FCLK      0x00000004
#define DEBUG_OVERRIDE_DISABLE_VOLT_LINK_VCN_DCFCLK    0x00000008
#define DEBUG_OVERRIDE_DISABLE_FAST_FCLK_TIMER         0x00000010
#define DEBUG_OVERRIDE_DISABLE_VCN_PG                  0x00000020
#define DEBUG_OVERRIDE_DISABLE_FMAX_VMAX               0x00000040
#define DEBUG_OVERRIDE_DISABLE_IMU_FW_CHECKS           0x00000080
#define DEBUG_OVERRIDE_DISABLE_D0i2_REENTRY_HSR_TIMER_CHECK 0x00000100
#define DEBUG_OVERRIDE_DISABLE_DFLL                    0x00000200
#define DEBUG_OVERRIDE_ENABLE_RLC_VF_BRINGUP_MODE      0x00000400
#define DEBUG_OVERRIDE_DFLL_MASTER_MODE                0x00000800
#define DEBUG_OVERRIDE_ENABLE_PROFILING_MODE           0x00001000


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

typedef enum {
  SVI_PSI_0, 
  SVI_PSI_1, 
  SVI_PSI_2, 
  SVI_PSI_3, 
  SVI_PSI_4, 
  SVI_PSI_5, 
  SVI_PSI_6, 
  SVI_PSI_7, 
} SVI_PSI_e;


#define THROTTLER_TEMP_EDGE_BIT        0
#define THROTTLER_TEMP_HOTSPOT_BIT     1
#define THROTTLER_TEMP_HOTSPOT_G_BIT   2
#define THROTTLER_TEMP_HOTSPOT_M_BIT   3
#define THROTTLER_TEMP_MEM_BIT         4
#define THROTTLER_TEMP_VR_GFX_BIT      5
#define THROTTLER_TEMP_VR_MEM0_BIT     6
#define THROTTLER_TEMP_VR_MEM1_BIT     7
#define THROTTLER_TEMP_VR_SOC_BIT      8
#define THROTTLER_TEMP_VR_U_BIT        9
#define THROTTLER_TEMP_LIQUID0_BIT     10
#define THROTTLER_TEMP_LIQUID1_BIT     11
#define THROTTLER_TEMP_PLX_BIT         12
#define THROTTLER_TDC_GFX_BIT          13
#define THROTTLER_TDC_SOC_BIT          14
#define THROTTLER_TDC_U_BIT            15
#define THROTTLER_PPT0_BIT             16
#define THROTTLER_PPT1_BIT             17
#define THROTTLER_PPT2_BIT             18
#define THROTTLER_PPT3_BIT             19
#define THROTTLER_FIT_BIT              20
#define THROTTLER_GFX_APCC_PLUS_BIT    21
#define THROTTLER_COUNT                22


#define FW_DSTATE_SOC_ULV_BIT               0
#define FW_DSTATE_G6_HSR_BIT                1
#define FW_DSTATE_G6_PHY_VMEMP_OFF_BIT      2
#define FW_DSTATE_SMN_DS_BIT                3
#define FW_DSTATE_MP1_WHISPER_MODE_BIT      4
#define FW_DSTATE_SOC_LIV_MIN_BIT           5
#define FW_DSTATE_SOC_PLL_PWRDN_BIT         6
#define FW_DSTATE_MEM_PLL_PWRDN_BIT         7
#define FW_DSTATE_MALL_ALLOC_BIT            8
#define FW_DSTATE_MEM_PSI_BIT               9
#define FW_DSTATE_HSR_NON_STROBE_BIT        10
#define FW_DSTATE_MP0_ENTER_WFI_BIT         11
#define FW_DSTATE_U_ULV_BIT                 12
#define FW_DSTATE_MALL_FLUSH_BIT            13
#define FW_DSTATE_SOC_PSI_BIT               14
#define FW_DSTATE_U_PSI_BIT                 15
#define FW_DSTATE_UCP_DS_BIT                16
#define FW_DSTATE_CSRCLK_DS_BIT             17
#define FW_DSTATE_MMHUB_INTERLOCK_BIT       18
#define FW_DSTATE_D0i3_2_QUIET_FW_BIT       19
#define FW_DSTATE_CLDO_PRG_BIT              20
#define FW_DSTATE_DF_PLL_PWRDN_BIT          21
#define FW_DSTATE_U_LOW_PWR_MODE_EN_BIT     22
#define FW_DSTATE_GFX_PSI6_BIT              23
#define FW_DSTATE_GFX_VR_PWR_STAGE_BIT      24


#define LED_DISPLAY_GFX_DPM_BIT            0
#define LED_DISPLAY_PCIE_BIT               1
#define LED_DISPLAY_ERROR_BIT              2


#define MEM_TEMP_READ_OUT_OF_BAND_BIT          0
#define MEM_TEMP_READ_IN_BAND_REFRESH_BIT      1
#define MEM_TEMP_READ_IN_BAND_DUMMY_PSTATE_BIT 2

typedef enum {
  SMARTSHIFT_VERSION_1,
  SMARTSHIFT_VERSION_2,
  SMARTSHIFT_VERSION_3,
} SMARTSHIFT_VERSION_e;

typedef enum {
  FOPT_CALC_AC_CALC_DC,
  FOPT_PPTABLE_AC_CALC_DC,
  FOPT_CALC_AC_PPTABLE_DC,
  FOPT_PPTABLE_AC_PPTABLE_DC,
} FOPT_CALC_e;

typedef enum {
  DRAM_BIT_WIDTH_DISABLED = 0,
  DRAM_BIT_WIDTH_X_8 = 8,
  DRAM_BIT_WIDTH_X_16 = 16,
  DRAM_BIT_WIDTH_X_32 = 32,
  DRAM_BIT_WIDTH_X_64 = 64,
  DRAM_BIT_WIDTH_X_128 = 128,
  DRAM_BIT_WIDTH_COUNT,
} DRAM_BIT_WIDTH_TYPE_e;


#define NUM_I2C_CONTROLLERS                8

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
	I2C_CONTROLLER_NAME_VR_VMEMP,
	I2C_CONTROLLER_NAME_VR_VDDIO,
	I2C_CONTROLLER_NAME_LIQUID0,
	I2C_CONTROLLER_NAME_LIQUID1,
	I2C_CONTROLLER_NAME_PLX,
	I2C_CONTROLLER_NAME_FAN_INTAKE,
	I2C_CONTROLLER_NAME_COUNT,
} I2cControllerName_e;

typedef enum {
  I2C_CONTROLLER_THROTTLER_TYPE_NONE = 0,
  I2C_CONTROLLER_THROTTLER_VR_GFX,
  I2C_CONTROLLER_THROTTLER_VR_SOC,
  I2C_CONTROLLER_THROTTLER_VR_VMEMP,
  I2C_CONTROLLER_THROTTLER_VR_VDDIO,
  I2C_CONTROLLER_THROTTLER_LIQUID0,
  I2C_CONTROLLER_THROTTLER_LIQUID1,
  I2C_CONTROLLER_THROTTLER_PLX,
  I2C_CONTROLLER_THROTTLER_FAN_INTAKE,
  I2C_CONTROLLER_THROTTLER_INA3221,
  I2C_CONTROLLER_THROTTLER_COUNT,
} I2cControllerThrottler_e;

typedef enum {
	I2C_CONTROLLER_PROTOCOL_VR_XPDE132G5,
	I2C_CONTROLLER_PROTOCOL_VR_IR35217,
	I2C_CONTROLLER_PROTOCOL_TMP_MAX31875,
	I2C_CONTROLLER_PROTOCOL_INA3221,
	I2C_CONTROLLER_PROTOCOL_TMP_MAX6604,
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

typedef struct {
  uint64_t mca_umc_status;
  uint64_t mca_umc_addr;

  uint16_t ce_count_lo_chip;
  uint16_t ce_count_hi_chip;

  uint32_t eccPadding;
} EccInfo_t;

typedef struct {
  EccInfo_t  EccInfo[24];
} EccInfoTable_t;


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
  uint32_t m;  
  uint32_t b;  
} LinearInt_t;

typedef struct {
  uint32_t a;  
  uint32_t b;  
  uint32_t c;  
} DroopInt_t;

typedef enum {
  DCS_ARCH_DISABLED,
  DCS_ARCH_FADCS,
  DCS_ARCH_ASYNC,
} DCS_ARCH_e;


typedef enum {
  PPCLK_GFXCLK = 0,
  PPCLK_SOCCLK,
  PPCLK_UCLK,
  PPCLK_FCLK,
  PPCLK_DCLK_0,
  PPCLK_VCLK_0,
  PPCLK_DCLK_1,
  PPCLK_VCLK_1,
  PPCLK_DISPCLK,
  PPCLK_DPPCLK,
  PPCLK_DPREFCLK,
  PPCLK_DCFCLK,
  PPCLK_DTBCLK,
  PPCLK_COUNT,
} PPCLK_e;

typedef enum {
  VOLTAGE_MODE_PPTABLE = 0,
  VOLTAGE_MODE_FUSES,
  VOLTAGE_MODE_COUNT,
} VOLTAGE_MODE_e;


typedef enum {
  AVFS_VOLTAGE_GFX = 0,
  AVFS_VOLTAGE_SOC,
  AVFS_VOLTAGE_COUNT,
} AVFS_VOLTAGE_TYPE_e;

typedef enum {
  AVFS_TEMP_COLD = 0,
  AVFS_TEMP_HOT,
  AVFS_TEMP_COUNT,
} AVFS_TEMP_e;

typedef enum {
  AVFS_D_G,
  AVFS_D_M_B,
  AVFS_D_M_S,
  AVFS_D_COUNT,
} AVFS_D_e;

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

typedef struct {
  uint8_t        Padding;
  uint8_t        SnapToDiscrete;      
  uint8_t        NumDiscreteLevels;   
  uint8_t        CalculateFopt;       
  LinearInt_t    ConversionToAvfsClk; 
  uint32_t       Padding3[3];
  uint16_t       Padding4;
  uint16_t       FoptimalDc;          
  uint16_t       FoptimalAc;          
  uint16_t       Padding2;
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
  TEMP_HOTSPOT_G,
  TEMP_HOTSPOT_M,
  TEMP_MEM,
  TEMP_VR_GFX,
  TEMP_VR_MEM0,
  TEMP_VR_MEM1,
  TEMP_VR_SOC,
  TEMP_VR_U,
  TEMP_LIQUID0,
  TEMP_LIQUID1,
  TEMP_PLX,
  TEMP_COUNT,
} TEMP_e;

typedef enum {
  TDC_THROTTLER_GFX,
  TDC_THROTTLER_SOC,
  TDC_THROTTLER_U,
  TDC_THROTTLER_COUNT
} TDC_THROTTLER_e;

typedef enum {
  SVI_PLANE_GFX,
  SVI_PLANE_SOC,
  SVI_PLANE_VMEMP,
  SVI_PLANE_VDDIO_MEM,
  SVI_PLANE_U,
  SVI_PLANE_COUNT,
} SVI_PLANE_e;

typedef enum {
  PMFW_VOLT_PLANE_GFX,
  PMFW_VOLT_PLANE_SOC,
  PMFW_VOLT_PLANE_COUNT
} PMFW_VOLT_PLANE_e;

typedef enum {
  CUSTOMER_VARIANT_ROW,
  CUSTOMER_VARIANT_FALCON,
  CUSTOMER_VARIANT_COUNT,
} CUSTOMER_VARIANT_e;

typedef enum {
  POWER_SOURCE_AC,
  POWER_SOURCE_DC,
  POWER_SOURCE_COUNT,
} POWER_SOURCE_e;

typedef enum {
  MEM_VENDOR_PLACEHOLDER0,
  MEM_VENDOR_SAMSUNG,
  MEM_VENDOR_INFINEON,
  MEM_VENDOR_ELPIDA,
  MEM_VENDOR_ETRON,
  MEM_VENDOR_NANYA,
  MEM_VENDOR_HYNIX,
  MEM_VENDOR_MOSEL,
  MEM_VENDOR_WINBOND,
  MEM_VENDOR_ESMT,
  MEM_VENDOR_PLACEHOLDER1,
  MEM_VENDOR_PLACEHOLDER2,
  MEM_VENDOR_PLACEHOLDER3,
  MEM_VENDOR_PLACEHOLDER4,
  MEM_VENDOR_PLACEHOLDER5,
  MEM_VENDOR_MICRON,
  MEM_VENDOR_COUNT,
} MEM_VENDOR_e;

typedef enum {
  PP_GRTAVFS_HW_CPO_CTL_ZONE0,
  PP_GRTAVFS_HW_CPO_CTL_ZONE1,
  PP_GRTAVFS_HW_CPO_CTL_ZONE2,
  PP_GRTAVFS_HW_CPO_CTL_ZONE3,
  PP_GRTAVFS_HW_CPO_CTL_ZONE4,
  PP_GRTAVFS_HW_CPO_EN_0_31_ZONE0,
  PP_GRTAVFS_HW_CPO_EN_32_63_ZONE0,
  PP_GRTAVFS_HW_CPO_EN_0_31_ZONE1,
  PP_GRTAVFS_HW_CPO_EN_32_63_ZONE1,
  PP_GRTAVFS_HW_CPO_EN_0_31_ZONE2,
  PP_GRTAVFS_HW_CPO_EN_32_63_ZONE2,
  PP_GRTAVFS_HW_CPO_EN_0_31_ZONE3,
  PP_GRTAVFS_HW_CPO_EN_32_63_ZONE3,
  PP_GRTAVFS_HW_CPO_EN_0_31_ZONE4,
  PP_GRTAVFS_HW_CPO_EN_32_63_ZONE4,
  PP_GRTAVFS_HW_ZONE0_VF,
  PP_GRTAVFS_HW_ZONE1_VF1,
  PP_GRTAVFS_HW_ZONE2_VF2,
  PP_GRTAVFS_HW_ZONE3_VF3,
  PP_GRTAVFS_HW_VOLTAGE_GB,
  PP_GRTAVFS_HW_CPOSCALINGCTRL_ZONE0,
  PP_GRTAVFS_HW_CPOSCALINGCTRL_ZONE1,
  PP_GRTAVFS_HW_CPOSCALINGCTRL_ZONE2,
  PP_GRTAVFS_HW_CPOSCALINGCTRL_ZONE3,
  PP_GRTAVFS_HW_CPOSCALINGCTRL_ZONE4,
  PP_GRTAVFS_HW_RESERVED_0,
  PP_GRTAVFS_HW_RESERVED_1,
  PP_GRTAVFS_HW_RESERVED_2,
  PP_GRTAVFS_HW_RESERVED_3,
  PP_GRTAVFS_HW_RESERVED_4,
  PP_GRTAVFS_HW_RESERVED_5,
  PP_GRTAVFS_HW_RESERVED_6,
  PP_GRTAVFS_HW_FUSE_COUNT,
} PP_GRTAVFS_HW_FUSE_e;

typedef enum {
  PP_GRTAVFS_FW_COMMON_PPVMIN_Z1_HOT_T0,
  PP_GRTAVFS_FW_COMMON_PPVMIN_Z1_COLD_T0,
  PP_GRTAVFS_FW_COMMON_PPVMIN_Z2_HOT_T0,
  PP_GRTAVFS_FW_COMMON_PPVMIN_Z2_COLD_T0,
  PP_GRTAVFS_FW_COMMON_PPVMIN_Z3_HOT_T0,
  PP_GRTAVFS_FW_COMMON_PPVMIN_Z3_COLD_T0,
  PP_GRTAVFS_FW_COMMON_PPVMIN_Z4_HOT_T0,
  PP_GRTAVFS_FW_COMMON_PPVMIN_Z4_COLD_T0,
  PP_GRTAVFS_FW_COMMON_SRAM_RM_Z0,
  PP_GRTAVFS_FW_COMMON_SRAM_RM_Z1,
  PP_GRTAVFS_FW_COMMON_SRAM_RM_Z2,
  PP_GRTAVFS_FW_COMMON_SRAM_RM_Z3,
  PP_GRTAVFS_FW_COMMON_SRAM_RM_Z4,
  PP_GRTAVFS_FW_COMMON_FUSE_COUNT,
} PP_GRTAVFS_FW_COMMON_FUSE_e;

typedef enum {
  PP_GRTAVFS_FW_SEP_FUSE_GB1_PWL_VOLTAGE_NEG_1,
  PP_GRTAVFS_FW_SEP_FUSE_GB1_PWL_VOLTAGE_0,
  PP_GRTAVFS_FW_SEP_FUSE_GB1_PWL_VOLTAGE_1,
  PP_GRTAVFS_FW_SEP_FUSE_GB1_PWL_VOLTAGE_2,
  PP_GRTAVFS_FW_SEP_FUSE_GB1_PWL_VOLTAGE_3,
  PP_GRTAVFS_FW_SEP_FUSE_GB1_PWL_VOLTAGE_4,
  PP_GRTAVFS_FW_SEP_FUSE_GB2_PWL_VOLTAGE_NEG_1,
  PP_GRTAVFS_FW_SEP_FUSE_GB2_PWL_VOLTAGE_0,
  PP_GRTAVFS_FW_SEP_FUSE_GB2_PWL_VOLTAGE_1,
  PP_GRTAVFS_FW_SEP_FUSE_GB2_PWL_VOLTAGE_2,
  PP_GRTAVFS_FW_SEP_FUSE_GB2_PWL_VOLTAGE_3,
  PP_GRTAVFS_FW_SEP_FUSE_GB2_PWL_VOLTAGE_4,
  PP_GRTAVFS_FW_SEP_FUSE_VF_NEG_1_FREQUENCY,
  PP_GRTAVFS_FW_SEP_FUSE_VF4_FREQUENCY,
  PP_GRTAVFS_FW_SEP_FUSE_FREQUENCY_TO_COUNT_SCALER_0,
  PP_GRTAVFS_FW_SEP_FUSE_FREQUENCY_TO_COUNT_SCALER_1,
  PP_GRTAVFS_FW_SEP_FUSE_FREQUENCY_TO_COUNT_SCALER_2,
  PP_GRTAVFS_FW_SEP_FUSE_FREQUENCY_TO_COUNT_SCALER_3,
  PP_GRTAVFS_FW_SEP_FUSE_FREQUENCY_TO_COUNT_SCALER_4,
  PP_GRTAVFS_FW_SEP_FUSE_COUNT,
} PP_GRTAVFS_FW_SEP_FUSE_e;

#define PP_NUM_RTAVFS_PWL_ZONES 5

#define PP_OD_FEATURE_GFX_VF_CURVE_BIT  0
#define PP_OD_FEATURE_PPT_BIT       2
#define PP_OD_FEATURE_FAN_CURVE_BIT 3
#define PP_OD_FEATURE_GFXCLK_BIT      7
#define PP_OD_FEATURE_UCLK_BIT      8
#define PP_OD_FEATURE_ZERO_FAN_BIT      9
#define PP_OD_FEATURE_TEMPERATURE_BIT 10
#define PP_OD_FEATURE_COUNT 13



typedef struct {
  int8_t   Offset; 
  uint8_t  Padding;
  uint16_t MaxCurrent; 
} SviTelemetryScale_t;

#define PP_NUM_OD_VF_CURVE_POINTS PP_NUM_RTAVFS_PWL_ZONES + 1

typedef enum {
	FAN_MODE_AUTO = 0,
	FAN_MODE_MANUAL_LINEAR,
} FanMode_e;

typedef struct {
  uint32_t FeatureCtrlMask;

  
  int16_t                VoltageOffsetPerZoneBoundary[PP_NUM_OD_VF_CURVE_POINTS];

  uint32_t               Reserved;

  
  int16_t                GfxclkFmin;           
  int16_t                GfxclkFmax;           
  uint16_t               UclkFmin;             
  uint16_t               UclkFmax;             

  
  int16_t                Ppt;         
  int16_t                Tdc;

  
  uint8_t                FanLinearPwmPoints[NUM_OD_FAN_MAX_POINTS];
  uint8_t                FanLinearTempPoints[NUM_OD_FAN_MAX_POINTS];
  uint16_t               FanMinimumPwm;
  uint16_t               AcousticTargetRpmThreshold;
  uint16_t               AcousticLimitRpmThreshold;
  uint16_t               FanTargetTemperature; 
  uint8_t                FanZeroRpmEnable;
  uint8_t                FanZeroRpmStopTemp;
  uint8_t                FanMode;
  uint8_t                MaxOpTemp;

  uint32_t               Spare[13];
  uint32_t               MmHubPadding[8]; 
} OverDriveTable_t;

typedef struct {
  OverDriveTable_t OverDriveTable;

} OverDriveTableExternal_t;

typedef struct {
  uint32_t FeatureCtrlMask;

  int16_t VoltageOffsetPerZoneBoundary;
  uint16_t               Reserved1;

  uint16_t               Reserved2;

  int16_t               GfxclkFmin;           
  int16_t               GfxclkFmax;           
  uint16_t               UclkFmin;             
  uint16_t               UclkFmax;             

  
  int16_t                Ppt;         
  int16_t                Tdc;

  uint8_t                FanLinearPwmPoints;
  uint8_t                FanLinearTempPoints;
  uint16_t               FanMinimumPwm;
  uint16_t               AcousticTargetRpmThreshold;
  uint16_t               AcousticLimitRpmThreshold;
  uint16_t               FanTargetTemperature; 
  uint8_t                FanZeroRpmEnable;
  uint8_t                FanZeroRpmStopTemp;
  uint8_t                FanMode;
  uint8_t                MaxOpTemp;

  uint32_t               Spare[13];

} OverDriveLimits_t;


typedef enum {
  BOARD_GPIO_SMUIO_0,
  BOARD_GPIO_SMUIO_1,
  BOARD_GPIO_SMUIO_2,
  BOARD_GPIO_SMUIO_3,
  BOARD_GPIO_SMUIO_4,
  BOARD_GPIO_SMUIO_5,
  BOARD_GPIO_SMUIO_6,
  BOARD_GPIO_SMUIO_7,
  BOARD_GPIO_SMUIO_8,
  BOARD_GPIO_SMUIO_9,
  BOARD_GPIO_SMUIO_10,
  BOARD_GPIO_SMUIO_11,
  BOARD_GPIO_SMUIO_12,
  BOARD_GPIO_SMUIO_13,
  BOARD_GPIO_SMUIO_14,
  BOARD_GPIO_SMUIO_15,
  BOARD_GPIO_SMUIO_16,
  BOARD_GPIO_SMUIO_17,
  BOARD_GPIO_SMUIO_18,
  BOARD_GPIO_SMUIO_19,
  BOARD_GPIO_SMUIO_20,
  BOARD_GPIO_SMUIO_21,
  BOARD_GPIO_SMUIO_22,
  BOARD_GPIO_SMUIO_23,
  BOARD_GPIO_SMUIO_24,
  BOARD_GPIO_SMUIO_25,
  BOARD_GPIO_SMUIO_26,
  BOARD_GPIO_SMUIO_27,
  BOARD_GPIO_SMUIO_28,
  BOARD_GPIO_SMUIO_29,
  BOARD_GPIO_SMUIO_30,
  BOARD_GPIO_SMUIO_31,
  MAX_BOARD_GPIO_SMUIO_NUM,
  BOARD_GPIO_DC_GEN_A,
  BOARD_GPIO_DC_GEN_B,
  BOARD_GPIO_DC_GEN_C,
  BOARD_GPIO_DC_GEN_D,
  BOARD_GPIO_DC_GEN_E,
  BOARD_GPIO_DC_GEN_F,
  BOARD_GPIO_DC_GEN_G,
  BOARD_GPIO_DC_GENLK_CLK,
  BOARD_GPIO_DC_GENLK_VSYNC,
  BOARD_GPIO_DC_SWAPLOCK_A,
  BOARD_GPIO_DC_SWAPLOCK_B,
} BOARD_GPIO_TYPE_e;

#define INVALID_BOARD_GPIO 0xFF

#define MARKETING_BASE_CLOCKS         0
#define MARKETING_GAME_CLOCKS         1
#define MARKETING_BOOST_CLOCKS        2

typedef struct {
  
  uint16_t InitGfxclk_bypass;
  uint16_t InitSocclk;
  uint16_t InitMp0clk;
  uint16_t InitMpioclk;
  uint16_t InitSmnclk;
  uint16_t InitUcpclk;
  uint16_t InitCsrclk;
  

  uint16_t InitDprefclk;
  uint16_t InitDcfclk;
  uint16_t InitDtbclk;
  
  uint16_t InitDclk; 
  uint16_t InitVclk;
  
  uint16_t InitUsbdfsclk;
  uint16_t InitMp1clk;
  uint16_t InitLclk;
  uint16_t InitBaco400clk_bypass;
  uint16_t InitBaco1200clk_bypass;
  uint16_t InitBaco700clk_bypass;
  
  uint16_t InitFclk;
  
  uint16_t InitGfxclk_clkb;

  
  uint8_t InitUclkDPMState;    

  uint8_t Padding[3];

  uint32_t InitVcoFreqPll0;
  uint32_t InitVcoFreqPll1;
  uint32_t InitVcoFreqPll2;
  uint32_t InitVcoFreqPll3;
  uint32_t InitVcoFreqPll4;
  uint32_t InitVcoFreqPll5;
  uint32_t InitVcoFreqPll6;

  
  uint16_t InitGfx;     
  uint16_t InitSoc;     
  uint16_t InitU; 

  uint16_t Padding2;

  uint32_t Spare[8];

} BootValues_t;


typedef struct {
   uint16_t Power[PPT_THROTTLER_COUNT][POWER_SOURCE_COUNT]; 
  uint16_t Tdc[TDC_THROTTLER_COUNT];             

  uint16_t Temperature[TEMP_COUNT]; 

  uint8_t  PwmLimitMin;
  uint8_t  PwmLimitMax;
  uint8_t  FanTargetTemperature;
  uint8_t  Spare1[1];

  uint16_t AcousticTargetRpmThresholdMin;
  uint16_t AcousticTargetRpmThresholdMax;

  uint16_t AcousticLimitRpmThresholdMin;
  uint16_t AcousticLimitRpmThresholdMax;

  uint16_t  PccLimitMin;
  uint16_t  PccLimitMax;

  uint16_t  FanStopTempMin;
  uint16_t  FanStopTempMax;
  uint16_t  FanStartTempMin;
  uint16_t  FanStartTempMax;

  uint16_t  PowerMinPpt0[POWER_SOURCE_COUNT];
  uint32_t Spare[11];

} MsgLimits_t;

typedef struct {
  uint16_t BaseClockAc;
  uint16_t GameClockAc;
  uint16_t BoostClockAc;
  uint16_t BaseClockDc;
  uint16_t GameClockDc;
  uint16_t BoostClockDc;

  uint32_t Reserved[4];
} DriverReportedClocks_t;

typedef struct {
  uint8_t           DcBtcEnabled;
  uint8_t           Padding[3];

  uint16_t          DcTol;            
  uint16_t          DcBtcGb;       

  uint16_t          DcBtcMin;       
  uint16_t          DcBtcMax;       

  LinearInt_t       DcBtcGbScalar;

} AvfsDcBtcParams_t;

typedef struct {
  uint16_t       AvfsTemp[AVFS_TEMP_COUNT]; 
  uint16_t      VftFMin;  
  uint16_t      VInversion; 
  QuadraticInt_t qVft[AVFS_TEMP_COUNT];
  QuadraticInt_t qAvfsGb;
  QuadraticInt_t qAvfsGb2;
} AvfsFuseOverride_t;

typedef struct {
  

  uint32_t Version; 

  
  uint32_t FeaturesToRun[NUM_FEATURES / 32]; 

  
  uint8_t      TotalPowerConfig;    
  uint8_t      CustomerVariant; 
  uint8_t      MemoryTemperatureTypeMask; 
  uint8_t      SmartShiftVersion; 

  
  uint16_t SocketPowerLimitAc[PPT_THROTTLER_COUNT]; 
  uint16_t SocketPowerLimitDc[PPT_THROTTLER_COUNT];  

  uint16_t SocketPowerLimitSmartShift2; 

  
  
  uint8_t  EnableLegacyPptLimit;
  uint8_t  UseInputTelemetry; 
  uint8_t  SmartShiftMinReportedPptinDcs; 

  uint8_t  PaddingPpt[1];

  uint16_t VrTdcLimit[TDC_THROTTLER_COUNT];             

  uint16_t PlatformTdcLimit[TDC_THROTTLER_COUNT];             

  uint16_t TemperatureLimit[TEMP_COUNT]; 

  uint16_t HwCtfTempLimit; 

  uint16_t PaddingInfra;

  
  uint32_t FitControllerFailureRateLimit; 
  
  uint32_t FitControllerGfxDutyCycle; 
  
  uint32_t FitControllerSocDutyCycle; 

  
  uint32_t FitControllerSocOffset;  

  uint32_t     GfxApccPlusResidencyLimit; 

  
  uint32_t ThrottlerControlMask;   

  
  uint32_t FwDStateMask;           

  
  uint16_t  UlvVoltageOffset[PMFW_VOLT_PLANE_COUNT]; 

  uint16_t     UlvVoltageOffsetU; 
  uint16_t     DeepUlvVoltageOffsetSoc;        

  
  uint16_t     DefaultMaxVoltage[PMFW_VOLT_PLANE_COUNT]; 
  uint16_t     BoostMaxVoltage[PMFW_VOLT_PLANE_COUNT]; 

  
  int16_t         VminTempHystersis[PMFW_VOLT_PLANE_COUNT]; 
  int16_t         VminTempThreshold[PMFW_VOLT_PLANE_COUNT]; 
  uint16_t        Vmin_Hot_T0[PMFW_VOLT_PLANE_COUNT];            
  uint16_t        Vmin_Cold_T0[PMFW_VOLT_PLANE_COUNT];           
  uint16_t        Vmin_Hot_Eol[PMFW_VOLT_PLANE_COUNT];           
  uint16_t        Vmin_Cold_Eol[PMFW_VOLT_PLANE_COUNT];          
  uint16_t        Vmin_Aging_Offset[PMFW_VOLT_PLANE_COUNT];      
  uint16_t        Spare_Vmin_Plat_Offset_Hot[PMFW_VOLT_PLANE_COUNT];   
  uint16_t        Spare_Vmin_Plat_Offset_Cold[PMFW_VOLT_PLANE_COUNT];  

  
  uint16_t        VcBtcFixedVminAgingOffset[PMFW_VOLT_PLANE_COUNT];
  
  uint16_t        VcBtcVmin2PsmDegrationGb[PMFW_VOLT_PLANE_COUNT];
  
  uint32_t        VcBtcPsmA[PMFW_VOLT_PLANE_COUNT];                   
  
  uint32_t        VcBtcPsmB[PMFW_VOLT_PLANE_COUNT];                   
  
  uint32_t        VcBtcVminA[PMFW_VOLT_PLANE_COUNT];                  
  
  uint32_t        VcBtcVminB[PMFW_VOLT_PLANE_COUNT];                  

  uint8_t        PerPartVminEnabled[PMFW_VOLT_PLANE_COUNT];
  uint8_t        VcBtcEnabled[PMFW_VOLT_PLANE_COUNT];

  uint16_t SocketPowerLimitAcTau[PPT_THROTTLER_COUNT]; 
  uint16_t SocketPowerLimitDcTau[PPT_THROTTLER_COUNT]; 

  QuadraticInt_t Vmin_droop;
  uint32_t       SpareVmin[9];


  
  DpmDescriptor_t DpmDescriptor[PPCLK_COUNT];

  uint16_t       FreqTableGfx      [NUM_GFXCLK_DPM_LEVELS  ];     
  uint16_t       FreqTableVclk     [NUM_VCLK_DPM_LEVELS    ];     
  uint16_t       FreqTableDclk     [NUM_DCLK_DPM_LEVELS    ];     
  uint16_t       FreqTableSocclk   [NUM_SOCCLK_DPM_LEVELS  ];     
  uint16_t       FreqTableUclk     [NUM_UCLK_DPM_LEVELS    ];     
  uint16_t       FreqTableDispclk  [NUM_DISPCLK_DPM_LEVELS ];     
  uint16_t       FreqTableDppClk   [NUM_DPPCLK_DPM_LEVELS  ];     
  uint16_t       FreqTableDprefclk [NUM_DPREFCLK_DPM_LEVELS];     
  uint16_t       FreqTableDcfclk   [NUM_DCFCLK_DPM_LEVELS  ];     
  uint16_t       FreqTableDtbclk   [NUM_DTBCLK_DPM_LEVELS  ];     
  uint16_t       FreqTableFclk     [NUM_FCLK_DPM_LEVELS    ];     

  uint32_t       DcModeMaxFreq     [PPCLK_COUNT            ];     

  
  uint16_t       Mp0clkFreq        [NUM_MP0CLK_DPM_LEVELS];       
  uint16_t       Mp0DpmVoltage     [NUM_MP0CLK_DPM_LEVELS];       

  uint8_t         GfxclkSpare[2];
  uint16_t        GfxclkFreqCap;

  
  uint16_t        GfxclkFgfxoffEntry;   
  uint16_t        GfxclkFgfxoffExitImu; 
  uint16_t        GfxclkFgfxoffExitRlc; 
  uint16_t        GfxclkThrottleClock;  
  uint8_t         EnableGfxPowerStagesGpio; 
  uint8_t         GfxIdlePadding;

  uint8_t          SmsRepairWRCKClkDivEn;
  uint8_t          SmsRepairWRCKClkDivVal;
  uint8_t          GfxOffEntryEarlyMGCGEn;
  uint8_t          GfxOffEntryForceCGCGEn;
  uint8_t          GfxOffEntryForceCGCGDelayEn;
  uint8_t          GfxOffEntryForceCGCGDelayVal; 

  uint16_t        GfxclkFreqGfxUlv; 
  uint8_t         GfxIdlePadding2[2];

  uint32_t        GfxOffEntryHysteresis;
  uint32_t        GfxoffSpare[15];

  
  uint32_t        DfllBtcMasterScalerM;
  int32_t         DfllBtcMasterScalerB;
  uint32_t        DfllBtcSlaveScalerM;
  int32_t         DfllBtcSlaveScalerB;

  uint32_t        DfllPccAsWaitCtrl; 
  uint32_t        DfllPccAsStepCtrl; 

  uint32_t        DfllL2FrequencyBoostM; 
  uint32_t        DfllL2FrequencyBoostB; 
  uint32_t        GfxGpoSpare[8];

  

  uint16_t        DcsGfxOffVoltage;     
  uint16_t        PaddingDcs;

  uint16_t        DcsMinGfxOffTime;     
  uint16_t        DcsMaxGfxOffTime;      

  uint32_t        DcsMinCreditAccum;    

  uint16_t        DcsExitHysteresis;    
  uint16_t        DcsTimeout;           

  uint8_t         FoptEnabled;
  uint8_t         DcsSpare2[3];
  uint32_t        DcsFoptM;             
  uint32_t        DcsFoptB;             

  uint32_t        DcsSpare[11];

  
  uint16_t     ShadowFreqTableUclk[NUM_UCLK_DPM_LEVELS];     
  uint8_t      UseStrobeModeOptimizations; 
  uint8_t      PaddingMem[3];

  uint8_t      UclkDpmPstates     [NUM_UCLK_DPM_LEVELS];     
  uint8_t      FreqTableUclkDiv  [NUM_UCLK_DPM_LEVELS    ];     

  uint16_t     MemVmempVoltage   [NUM_UCLK_DPM_LEVELS];         
  uint16_t     MemVddioVoltage    [NUM_UCLK_DPM_LEVELS];         

  

  uint8_t      FclkDpmUPstates[NUM_FCLK_DPM_LEVELS]; 
  uint16_t     FclkDpmVddU[NUM_FCLK_DPM_LEVELS]; 
  uint16_t     FclkDpmUSpeed[NUM_FCLK_DPM_LEVELS]; 
  uint16_t     FclkDpmDisallowPstateFreq;  
  uint16_t     PaddingFclk;

  
  uint8_t      PcieGenSpeed[NUM_LINK_LEVELS];           
  uint8_t      PcieLaneCount[NUM_LINK_LEVELS];          
  uint16_t     LclkFreq[NUM_LINK_LEVELS];

  
  uint16_t     FanStopTemp[TEMP_COUNT];          
  uint16_t     FanStartTemp[TEMP_COUNT];         

  uint16_t     FanGain[TEMP_COUNT];
  uint16_t     FanGainPadding;

  uint16_t     FanPwmMin;
  uint16_t     AcousticTargetRpmThreshold;
  uint16_t     AcousticLimitRpmThreshold;
  uint16_t     FanMaximumRpm;
  uint16_t     MGpuAcousticLimitRpmThreshold;
  uint16_t     FanTargetGfxclk;
  uint32_t     TempInputSelectMask;
  uint8_t      FanZeroRpmEnable;
  uint8_t      FanTachEdgePerRev;
  uint16_t     FanTargetTemperature[TEMP_COUNT];

  
  int16_t      FuzzyFan_ErrorSetDelta;
  int16_t      FuzzyFan_ErrorRateSetDelta;
  int16_t      FuzzyFan_PwmSetDelta;
  uint16_t     FuzzyFan_Reserved;

  uint16_t     FwCtfLimit[TEMP_COUNT];

  uint16_t IntakeTempEnableRPM;
  int16_t IntakeTempOffsetTemp;
  uint16_t IntakeTempReleaseTemp;
  uint16_t IntakeTempHighIntakeAcousticLimit;
  uint16_t IntakeTempAcouticLimitReleaseRate;

  int16_t FanAbnormalTempLimitOffset;
  uint16_t FanStalledTriggerRpm;
  uint16_t FanAbnormalTriggerRpmCoeff;
  uint16_t FanAbnormalDetectionEnable;

  uint8_t      FanIntakeSensorSupport;
  uint8_t      FanIntakePadding[3];
  uint32_t     FanSpare[13];

  

  uint8_t      OverrideGfxAvfsFuses;
  uint8_t      GfxAvfsPadding[3];

  uint32_t     L2HwRtAvfsFuses[PP_GRTAVFS_HW_FUSE_COUNT]; 
  uint32_t     SeHwRtAvfsFuses[PP_GRTAVFS_HW_FUSE_COUNT];

  uint32_t     CommonRtAvfs[PP_GRTAVFS_FW_COMMON_FUSE_COUNT];

  uint32_t     L2FwRtAvfsFuses[PP_GRTAVFS_FW_SEP_FUSE_COUNT];
  uint32_t     SeFwRtAvfsFuses[PP_GRTAVFS_FW_SEP_FUSE_COUNT];

  uint32_t    Droop_PWL_F[PP_NUM_RTAVFS_PWL_ZONES];
  uint32_t    Droop_PWL_a[PP_NUM_RTAVFS_PWL_ZONES];
  uint32_t    Droop_PWL_b[PP_NUM_RTAVFS_PWL_ZONES];
  uint32_t    Droop_PWL_c[PP_NUM_RTAVFS_PWL_ZONES];

  uint32_t   Static_PWL_Offset[PP_NUM_RTAVFS_PWL_ZONES];

  uint32_t   dGbV_dT_vmin;
  uint32_t   dGbV_dT_vmax;

  
  uint32_t   V2F_vmin_range_low;
  uint32_t   V2F_vmin_range_high;
  uint32_t   V2F_vmax_range_low;
  uint32_t   V2F_vmax_range_high;

  AvfsDcBtcParams_t DcBtcGfxParams;

  uint32_t   GfxAvfsSpare[32];

  

  uint8_t      OverrideSocAvfsFuses;
  uint8_t      MinSocAvfsRevision;
  uint8_t      SocAvfsPadding[2];

  AvfsFuseOverride_t SocAvfsFuseOverride[AVFS_D_COUNT];

  DroopInt_t        dBtcGbSoc[AVFS_D_COUNT];            

  LinearInt_t       qAgingGb[AVFS_D_COUNT];          

  QuadraticInt_t    qStaticVoltageOffset[AVFS_D_COUNT]; 

  AvfsDcBtcParams_t DcBtcSocParams[AVFS_D_COUNT];

  uint32_t   SocAvfsSpare[32];

  
  BootValues_t BootValues;

  
  DriverReportedClocks_t DriverReportedClocks;

  
  MsgLimits_t MsgLimits;

  
  OverDriveLimits_t OverDriveLimitsMin;
  OverDriveLimits_t OverDriveLimitsBasicMax;
  uint32_t reserved[22];

  
  uint32_t          DebugOverrides;

  
  uint8_t     TotalBoardPowerSupport;
  uint8_t     TotalBoardPowerPadding[3];

  int16_t     TotalIdleBoardPowerM;
  int16_t     TotalIdleBoardPowerB;
  int16_t     TotalBoardPowerM;
  int16_t     TotalBoardPowerB;

  
  QuadraticInt_t qFeffCoeffGameClock[POWER_SOURCE_COUNT];
  QuadraticInt_t qFeffCoeffBaseClock[POWER_SOURCE_COUNT];
  QuadraticInt_t qFeffCoeffBoostClock[POWER_SOURCE_COUNT];

  uint16_t TemperatureLimit_Hynix; 
  uint16_t TemperatureLimit_Micron; 
  uint16_t TemperatureFwCtfLimit_Hynix;
  uint16_t TemperatureFwCtfLimit_Micron;

  
  uint32_t         Spare[41];

  
  uint32_t     MmHubPadding[8];

} SkuTable_t;

typedef struct {
  
  uint32_t    Version; 


  
  I2cControllerConfig_t  I2cControllers[NUM_I2C_CONTROLLERS];

  
  uint8_t      VddGfxVrMapping;   
  uint8_t      VddSocVrMapping;   
  uint8_t      VddMem0VrMapping;  
  uint8_t      VddMem1VrMapping;  

  uint8_t      GfxUlvPhaseSheddingMask; 
  uint8_t      SocUlvPhaseSheddingMask; 
  uint8_t      VmempUlvPhaseSheddingMask; 
  uint8_t      VddioUlvPhaseSheddingMask; 

  
  uint8_t      SlaveAddrMapping[SVI_PLANE_COUNT];
  uint8_t      VrPsiSupport[SVI_PLANE_COUNT];

  uint8_t      PaddingPsi[SVI_PLANE_COUNT];
  uint8_t      EnablePsi6[SVI_PLANE_COUNT];       

  
  SviTelemetryScale_t SviTelemetryScale[SVI_PLANE_COUNT];
  uint32_t     VoltageTelemetryRatio[SVI_PLANE_COUNT]; 

  uint8_t      DownSlewRateVr[SVI_PLANE_COUNT];

  

  uint8_t      LedOffGpio;
  uint8_t      FanOffGpio;
  uint8_t      GfxVrPowerStageOffGpio;

  uint8_t      AcDcGpio;        
  uint8_t      AcDcPolarity;    
  uint8_t      VR0HotGpio;      
  uint8_t      VR0HotPolarity;  

  uint8_t      GthrGpio;        
  uint8_t      GthrPolarity;    

  
  uint8_t      LedPin0;         
  uint8_t      LedPin1;         
  uint8_t      LedPin2;         
  uint8_t      LedEnableMask;

  uint8_t      LedPcie;        
  uint8_t      LedError;       

  

  
  uint8_t      UclkTrainingModeSpreadPercent;
  uint8_t      UclkSpreadPadding;
  uint16_t     UclkSpreadFreq;      

  
  uint8_t      UclkSpreadPercent[MEM_VENDOR_COUNT];

  uint8_t      GfxclkSpreadEnable;

  
  uint8_t      FclkSpreadPercent;   
  uint16_t     FclkSpreadFreq;      

  
  uint8_t      DramWidth; 
  uint8_t      PaddingMem1[7];

  
  uint8_t      HsrEnabled;
  uint8_t      VddqOffEnabled;
  uint8_t      PaddingUmcFlags[2];

  uint32_t    PostVoltageSetBacoDelay; 
  uint32_t    BacoEntryDelay; 

  uint8_t     FuseWritePowerMuxPresent;
  uint8_t     FuseWritePadding[3];

  
  uint32_t     BoardSpare[63];

  

  
  uint32_t     MmHubPadding[8];
} BoardTable_t;

#pragma pack(push, 1)
typedef struct {
  SkuTable_t SkuTable;
  BoardTable_t BoardTable;
} PPTable_t;
#pragma pack(pop)

typedef struct {
  
  uint16_t     GfxclkAverageLpfTau;
  uint16_t     FclkAverageLpfTau;
  uint16_t     UclkAverageLpfTau;
  uint16_t     GfxActivityLpfTau;
  uint16_t     UclkActivityLpfTau;
  uint16_t     SocketPowerLpfTau;
  uint16_t     VcnClkAverageLpfTau;
  uint16_t     VcnUsageAverageLpfTau;
} DriverSmuConfig_t;

typedef struct {
  DriverSmuConfig_t DriverSmuConfig;

  uint32_t     Spare[8];
  
  uint32_t     MmHubPadding[8]; 
} DriverSmuConfigExternal_t;


typedef struct {

  uint16_t       FreqTableGfx      [NUM_GFXCLK_DPM_LEVELS  ];     
  uint16_t       FreqTableVclk     [NUM_VCLK_DPM_LEVELS    ];     
  uint16_t       FreqTableDclk     [NUM_DCLK_DPM_LEVELS    ];     
  uint16_t       FreqTableSocclk   [NUM_SOCCLK_DPM_LEVELS  ];     
  uint16_t       FreqTableUclk     [NUM_UCLK_DPM_LEVELS    ];     
  uint16_t       FreqTableDispclk  [NUM_DISPCLK_DPM_LEVELS ];     
  uint16_t       FreqTableDppClk   [NUM_DPPCLK_DPM_LEVELS  ];     
  uint16_t       FreqTableDprefclk [NUM_DPREFCLK_DPM_LEVELS];     
  uint16_t       FreqTableDcfclk   [NUM_DCFCLK_DPM_LEVELS  ];     
  uint16_t       FreqTableDtbclk   [NUM_DTBCLK_DPM_LEVELS  ];     
  uint16_t       FreqTableFclk     [NUM_FCLK_DPM_LEVELS    ];     

  uint16_t       DcModeMaxFreq     [PPCLK_COUNT            ];     

  uint16_t       Padding;

  uint32_t Spare[32];

  
  uint32_t     MmHubPadding[8]; 

} DriverInfoTable_t;

typedef struct {
  uint32_t CurrClock[PPCLK_COUNT];

  uint16_t AverageGfxclkFrequencyTarget;
  uint16_t AverageGfxclkFrequencyPreDs;
  uint16_t AverageGfxclkFrequencyPostDs;
  uint16_t AverageFclkFrequencyPreDs;
  uint16_t AverageFclkFrequencyPostDs;
  uint16_t AverageMemclkFrequencyPreDs  ; 
  uint16_t AverageMemclkFrequencyPostDs  ; 
  uint16_t AverageVclk0Frequency  ;
  uint16_t AverageDclk0Frequency  ;
  uint16_t AverageVclk1Frequency  ;
  uint16_t AverageDclk1Frequency  ;
  uint16_t PCIeBusy;
  uint16_t dGPU_W_MAX;
  uint16_t padding;

  uint32_t MetricsCounter;

  uint16_t AvgVoltage[SVI_PLANE_COUNT];
  uint16_t AvgCurrent[SVI_PLANE_COUNT];

  uint16_t AverageGfxActivity    ;
  uint16_t AverageUclkActivity   ;
  uint16_t Vcn0ActivityPercentage  ;
  uint16_t Vcn1ActivityPercentage  ;

  uint32_t EnergyAccumulator;
  uint16_t AverageSocketPower;
  uint16_t AverageTotalBoardPower;

  uint16_t AvgTemperature[TEMP_COUNT];
  uint16_t AvgTemperatureFanIntake;

  uint8_t  PcieRate               ;
  uint8_t  PcieWidth              ;

  uint8_t  AvgFanPwm;
  uint8_t  Padding[1];
  uint16_t AvgFanRpm;


  uint8_t ThrottlingPercentage[THROTTLER_COUNT];
  uint8_t VmaxThrottlingPercentage;
  uint8_t Padding1[3];

  
  uint32_t D3HotEntryCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint32_t D3HotExitCountPerMode[D3HOT_SEQUENCE_COUNT];
  uint32_t ArmMsgReceivedCountPerMode[D3HOT_SEQUENCE_COUNT];

  uint16_t ApuSTAPMSmartShiftLimit;
  uint16_t ApuSTAPMLimit;
  uint16_t AvgApuSocketPower;

  uint16_t AverageUclkActivity_MAX;

  uint32_t PublicSerialNumberLower;
  uint32_t PublicSerialNumberUpper;

} SmuMetrics_t;

typedef struct {
  SmuMetrics_t SmuMetrics;
  uint32_t Spare[29];

  
  uint32_t     MmHubPadding[8]; 
} SmuMetricsExternal_t;

typedef struct {
  uint8_t  WmSetting;
  uint8_t  Flags;
  uint8_t  Padding[2];

} WatermarkRowGeneric_t;

#define NUM_WM_RANGES 4

typedef enum {
  WATERMARKS_CLOCK_RANGE = 0,
  WATERMARKS_DUMMY_PSTATE,
  WATERMARKS_MALL,
  WATERMARKS_COUNT,
} WATERMARKS_FLAGS_e;

typedef struct {
  
  WatermarkRowGeneric_t WatermarkRow[NUM_WM_RANGES];
} Watermarks_t;

typedef struct {
  Watermarks_t Watermarks;
  uint32_t  Spare[16];

  uint32_t     MmHubPadding[8]; 
} WatermarksExternal_t;

typedef struct {
  uint16_t avgPsmCount[214];
  uint16_t minPsmCount[214];
  float    avgPsmVoltage[214];
  float    minPsmVoltage[214];
} AvfsDebugTable_t;

typedef struct {
  AvfsDebugTable_t AvfsDebugTable;

  uint32_t     MmHubPadding[8]; 
} AvfsDebugTableExternal_t;


typedef struct {
  uint8_t   Gfx_ActiveHystLimit;
  uint8_t   Gfx_IdleHystLimit;
  uint8_t   Gfx_FPS;
  uint8_t   Gfx_MinActiveFreqType;
  uint8_t   Gfx_BoosterFreqType;
  uint8_t   PaddingGfx;
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
  uint8_t   PaddingFclk;
  uint16_t  Fclk_MinActiveFreq;              
  uint16_t  Fclk_BoosterFreq;                
  uint16_t  Fclk_PD_Data_time_constant;      
  uint32_t  Fclk_PD_Data_limit_a;            
  uint32_t  Fclk_PD_Data_limit_b;            
  uint32_t  Fclk_PD_Data_limit_c;            
  uint32_t  Fclk_PD_Data_error_coeff;        
  uint32_t  Fclk_PD_Data_error_rate_coeff;   

  uint32_t  Mem_UpThreshold_Limit[NUM_UCLK_DPM_LEVELS];          
  uint8_t   Mem_UpHystLimit[NUM_UCLK_DPM_LEVELS];
  uint8_t   Mem_DownHystLimit[NUM_UCLK_DPM_LEVELS];
  uint16_t  Mem_Fps;
  uint8_t   padding[2];

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
#define WORKLOAD_PPLIB_WINDOW_3D_BIT      7
#define WORKLOAD_PPLIB_COUNT              8







#define TABLE_TRANSFER_OK         0x0
#define TABLE_TRANSFER_FAILED     0xFF
#define TABLE_TRANSFER_PENDING    0xAB


#define TABLE_PPTABLE                 0
#define TABLE_COMBO_PPTABLE           1
#define TABLE_WATERMARKS              2
#define TABLE_AVFS_PSM_DEBUG          3
#define TABLE_PMSTATUSLOG             4
#define TABLE_SMU_METRICS             5
#define TABLE_DRIVER_SMU_CONFIG       6
#define TABLE_ACTIVITY_MONITOR_COEFF  7
#define TABLE_OVERDRIVE               8
#define TABLE_I2C_COMMANDS            9
#define TABLE_DRIVER_INFO             10
#define TABLE_ECCINFO                 11
#define TABLE_COUNT                   12


#define IH_INTERRUPT_ID_TO_DRIVER                   0xFE
#define IH_INTERRUPT_CONTEXT_ID_BACO                0x2
#define IH_INTERRUPT_CONTEXT_ID_AC                  0x3
#define IH_INTERRUPT_CONTEXT_ID_DC                  0x4
#define IH_INTERRUPT_CONTEXT_ID_AUDIO_D0            0x5
#define IH_INTERRUPT_CONTEXT_ID_AUDIO_D3            0x6
#define IH_INTERRUPT_CONTEXT_ID_THERMAL_THROTTLING  0x7
#define IH_INTERRUPT_CONTEXT_ID_FAN_ABNORMAL        0x8
#define IH_INTERRUPT_CONTEXT_ID_FAN_RECOVERY        0x9

#endif
