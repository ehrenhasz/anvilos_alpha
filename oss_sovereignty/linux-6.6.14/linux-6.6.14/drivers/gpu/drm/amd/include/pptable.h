#ifndef _PPTABLE_H
#define _PPTABLE_H
#pragma pack(1)
typedef struct _ATOM_PPLIB_THERMALCONTROLLER
{
    UCHAR ucType;            
    UCHAR ucI2cLine;         
    UCHAR ucI2cAddress;
    UCHAR ucFanParameters;   
    UCHAR ucFanMinRPM;       
    UCHAR ucFanMaxRPM;       
    UCHAR ucReserved;        
    UCHAR ucFlags;           
} ATOM_PPLIB_THERMALCONTROLLER;
#define ATOM_PP_FANPARAMETERS_TACHOMETER_PULSES_PER_REVOLUTION_MASK 0x0f
#define ATOM_PP_FANPARAMETERS_NOFAN                                 0x80     
#define ATOM_PP_THERMALCONTROLLER_NONE      0
#define ATOM_PP_THERMALCONTROLLER_LM63      1   
#define ATOM_PP_THERMALCONTROLLER_ADM1032   2   
#define ATOM_PP_THERMALCONTROLLER_ADM1030   3   
#define ATOM_PP_THERMALCONTROLLER_MUA6649   4   
#define ATOM_PP_THERMALCONTROLLER_LM64      5
#define ATOM_PP_THERMALCONTROLLER_F75375    6   
#define ATOM_PP_THERMALCONTROLLER_RV6xx     7
#define ATOM_PP_THERMALCONTROLLER_RV770     8
#define ATOM_PP_THERMALCONTROLLER_ADT7473   9
#define ATOM_PP_THERMALCONTROLLER_KONG      10
#define ATOM_PP_THERMALCONTROLLER_EXTERNAL_GPIO     11
#define ATOM_PP_THERMALCONTROLLER_EVERGREEN 12
#define ATOM_PP_THERMALCONTROLLER_EMC2103   13     
#define ATOM_PP_THERMALCONTROLLER_SUMO      14     
#define ATOM_PP_THERMALCONTROLLER_NISLANDS  15
#define ATOM_PP_THERMALCONTROLLER_SISLANDS  16
#define ATOM_PP_THERMALCONTROLLER_LM96163   17
#define ATOM_PP_THERMALCONTROLLER_CISLANDS  18
#define ATOM_PP_THERMALCONTROLLER_KAVERI    19
#define ATOM_PP_THERMALCONTROLLER_ICELAND   20
#define ATOM_PP_THERMALCONTROLLER_TONGA     21
#define ATOM_PP_THERMALCONTROLLER_FIJI      22
#define ATOM_PP_THERMALCONTROLLER_POLARIS10 23
#define ATOM_PP_THERMALCONTROLLER_VEGA10    24
#define ATOM_PP_THERMALCONTROLLER_ADT7473_WITH_INTERNAL   0x89     
#define ATOM_PP_THERMALCONTROLLER_EMC2103_WITH_INTERNAL   0x8D     
typedef struct _ATOM_PPLIB_STATE
{
    UCHAR ucNonClockStateIndex;
    UCHAR ucClockStateIndices[];  
} ATOM_PPLIB_STATE;
typedef struct _ATOM_PPLIB_FANTABLE
{
    UCHAR   ucFanTableFormat;                 
    UCHAR   ucTHyst;                          
    USHORT  usTMin;                           
    USHORT  usTMed;                           
    USHORT  usTHigh;                          
    USHORT  usPWMMin;                         
    USHORT  usPWMMed;                         
    USHORT  usPWMHigh;                        
} ATOM_PPLIB_FANTABLE;
typedef struct _ATOM_PPLIB_FANTABLE2
{
    ATOM_PPLIB_FANTABLE basicTable;
    USHORT  usTMax;                           
} ATOM_PPLIB_FANTABLE2;
typedef struct _ATOM_PPLIB_FANTABLE3
{
	ATOM_PPLIB_FANTABLE2 basicTable2;
	UCHAR ucFanControlMode;
	USHORT usFanPWMMax;
	USHORT usFanOutputSensitivity;
} ATOM_PPLIB_FANTABLE3;
typedef struct _ATOM_PPLIB_FANTABLE4
{
    ATOM_PPLIB_FANTABLE3 basicTable3;
    USHORT  usFanRPMMax;
} ATOM_PPLIB_FANTABLE4;
typedef struct _ATOM_PPLIB_FANTABLE5
{
    ATOM_PPLIB_FANTABLE4 basicTable4;
    USHORT  usFanCurrentLow;
    USHORT  usFanCurrentHigh;
    USHORT  usFanRPMLow;
    USHORT  usFanRPMHigh;
} ATOM_PPLIB_FANTABLE5;
typedef struct _ATOM_PPLIB_EXTENDEDHEADER
{
    USHORT  usSize;
    ULONG   ulMaxEngineClock;    
    ULONG   ulMaxMemoryClock;    
    USHORT  usVCETableOffset;  
    USHORT  usUVDTableOffset;    
    USHORT  usSAMUTableOffset;   
    USHORT  usPPMTableOffset;    
    USHORT  usACPTableOffset;   
    USHORT  usPowerTuneTableOffset;
    USHORT  usSclkVddgfxTableOffset;
    USHORT  usVQBudgetingTableOffset;  
} ATOM_PPLIB_EXTENDEDHEADER;
#define ATOM_PP_PLATFORM_CAP_BACKBIAS 1
#define ATOM_PP_PLATFORM_CAP_POWERPLAY 2
#define ATOM_PP_PLATFORM_CAP_SBIOSPOWERSOURCE 4
#define ATOM_PP_PLATFORM_CAP_ASPM_L0s 8
#define ATOM_PP_PLATFORM_CAP_ASPM_L1 16
#define ATOM_PP_PLATFORM_CAP_HARDWAREDC 32
#define ATOM_PP_PLATFORM_CAP_GEMINIPRIMARY 64
#define ATOM_PP_PLATFORM_CAP_STEPVDDC 128
#define ATOM_PP_PLATFORM_CAP_VOLTAGECONTROL 256
#define ATOM_PP_PLATFORM_CAP_SIDEPORTCONTROL 512
#define ATOM_PP_PLATFORM_CAP_TURNOFFPLL_ASPML1 1024
#define ATOM_PP_PLATFORM_CAP_HTLINKCONTROL 2048
#define ATOM_PP_PLATFORM_CAP_MVDDCONTROL 4096
#define ATOM_PP_PLATFORM_CAP_GOTO_BOOT_ON_ALERT 0x2000               
#define ATOM_PP_PLATFORM_CAP_DONT_WAIT_FOR_VBLANK_ON_ALERT 0x4000    
#define ATOM_PP_PLATFORM_CAP_VDDCI_CONTROL 0x8000                    
#define ATOM_PP_PLATFORM_CAP_REGULATOR_HOT 0x00010000                
#define ATOM_PP_PLATFORM_CAP_BACO          0x00020000                
#define ATOM_PP_PLATFORM_CAP_NEW_CAC_VOLTAGE   0x00040000            
#define ATOM_PP_PLATFORM_CAP_REVERT_GPIO5_POLARITY   0x00080000      
#define ATOM_PP_PLATFORM_CAP_OUTPUT_THERMAL2GPIO17   0x00100000      
#define ATOM_PP_PLATFORM_CAP_VRHOT_GPIO_CONFIGURABLE   0x00200000    
#define ATOM_PP_PLATFORM_CAP_TEMP_INVERSION   0x00400000             
#define ATOM_PP_PLATFORM_CAP_EVV    0x00800000
#define ATOM_PP_PLATFORM_COMBINE_PCC_WITH_THERMAL_SIGNAL    0x01000000
#define ATOM_PP_PLATFORM_LOAD_POST_PRODUCTION_FIRMWARE    0x02000000
#define ATOM_PP_PLATFORM_CAP_DISABLE_USING_ACTUAL_TEMPERATURE_FOR_POWER_CALC   0x04000000
#define ATOM_PP_PLATFORM_CAP_VRHOT_POLARITY_HIGH   0x08000000
typedef struct _ATOM_PPLIB_POWERPLAYTABLE
{
      ATOM_COMMON_TABLE_HEADER sHeader;
      UCHAR ucDataRevision;
      UCHAR ucNumStates;
      UCHAR ucStateEntrySize;
      UCHAR ucClockInfoSize;
      UCHAR ucNonClockSize;
      USHORT usStateArrayOffset;
      USHORT usClockInfoArrayOffset;
      USHORT usNonClockInfoArrayOffset;
      USHORT usBackbiasTime;     
      USHORT usVoltageTime;      
      USHORT usTableSize;        
      ULONG ulPlatformCaps;             
      ATOM_PPLIB_THERMALCONTROLLER    sThermalController;
      USHORT usBootClockInfoOffset;
      USHORT usBootNonClockInfoOffset;
} ATOM_PPLIB_POWERPLAYTABLE;
typedef struct _ATOM_PPLIB_POWERPLAYTABLE2
{
    ATOM_PPLIB_POWERPLAYTABLE basicTable;
    UCHAR   ucNumCustomThermalPolicy;
    USHORT  usCustomThermalPolicyArrayOffset;
}ATOM_PPLIB_POWERPLAYTABLE2, *LPATOM_PPLIB_POWERPLAYTABLE2;
typedef struct _ATOM_PPLIB_POWERPLAYTABLE3
{
    ATOM_PPLIB_POWERPLAYTABLE2 basicTable2;
    USHORT                     usFormatID;                       
    USHORT                     usFanTableOffset;
    USHORT                     usExtendendedHeaderOffset;
} ATOM_PPLIB_POWERPLAYTABLE3, *LPATOM_PPLIB_POWERPLAYTABLE3;
typedef struct _ATOM_PPLIB_POWERPLAYTABLE4
{
    ATOM_PPLIB_POWERPLAYTABLE3 basicTable3;
    ULONG                      ulGoldenPPID;                     
    ULONG                      ulGoldenRevision;                 
    USHORT                     usVddcDependencyOnSCLKOffset;
    USHORT                     usVddciDependencyOnMCLKOffset;
    USHORT                     usVddcDependencyOnMCLKOffset;
    USHORT                     usMaxClockVoltageOnDCOffset;
    USHORT                     usVddcPhaseShedLimitsTableOffset;     
    USHORT                     usMvddDependencyOnMCLKOffset;  
} ATOM_PPLIB_POWERPLAYTABLE4, *LPATOM_PPLIB_POWERPLAYTABLE4;
typedef struct _ATOM_PPLIB_POWERPLAYTABLE5
{
    ATOM_PPLIB_POWERPLAYTABLE4 basicTable4;
    ULONG                      ulTDPLimit;
    ULONG                      ulNearTDPLimit;
    ULONG                      ulSQRampingThreshold;
    USHORT                     usCACLeakageTableOffset;          
    ULONG                      ulCACLeakage;                     
    USHORT                     usTDPODLimit;
    USHORT                     usLoadLineSlope;                  
} ATOM_PPLIB_POWERPLAYTABLE5, *LPATOM_PPLIB_POWERPLAYTABLE5;
#define ATOM_PPLIB_CLASSIFICATION_UI_MASK          0x0007
#define ATOM_PPLIB_CLASSIFICATION_UI_SHIFT         0
#define ATOM_PPLIB_CLASSIFICATION_UI_NONE          0
#define ATOM_PPLIB_CLASSIFICATION_UI_BATTERY       1
#define ATOM_PPLIB_CLASSIFICATION_UI_BALANCED      3
#define ATOM_PPLIB_CLASSIFICATION_UI_PERFORMANCE   5
#define ATOM_PPLIB_CLASSIFICATION_BOOT                   0x0008
#define ATOM_PPLIB_CLASSIFICATION_THERMAL                0x0010
#define ATOM_PPLIB_CLASSIFICATION_LIMITEDPOWERSOURCE     0x0020
#define ATOM_PPLIB_CLASSIFICATION_REST                   0x0040
#define ATOM_PPLIB_CLASSIFICATION_FORCED                 0x0080
#define ATOM_PPLIB_CLASSIFICATION_3DPERFORMANCE          0x0100
#define ATOM_PPLIB_CLASSIFICATION_OVERDRIVETEMPLATE      0x0200
#define ATOM_PPLIB_CLASSIFICATION_UVDSTATE               0x0400
#define ATOM_PPLIB_CLASSIFICATION_3DLOW                  0x0800
#define ATOM_PPLIB_CLASSIFICATION_ACPI                   0x1000
#define ATOM_PPLIB_CLASSIFICATION_HD2STATE               0x2000
#define ATOM_PPLIB_CLASSIFICATION_HDSTATE                0x4000
#define ATOM_PPLIB_CLASSIFICATION_SDSTATE                0x8000
#define ATOM_PPLIB_CLASSIFICATION2_LIMITEDPOWERSOURCE_2     0x0001
#define ATOM_PPLIB_CLASSIFICATION2_ULV                      0x0002
#define ATOM_PPLIB_CLASSIFICATION2_MVC                      0x0004    
#define ATOM_PPLIB_SINGLE_DISPLAY_ONLY           0x00000001
#define ATOM_PPLIB_SUPPORTS_VIDEO_PLAYBACK         0x00000002
#define ATOM_PPLIB_PCIE_LINK_SPEED_MASK            0x00000004
#define ATOM_PPLIB_PCIE_LINK_SPEED_SHIFT           2
#define ATOM_PPLIB_PCIE_LINK_WIDTH_MASK            0x000000F8
#define ATOM_PPLIB_PCIE_LINK_WIDTH_SHIFT           3
#define ATOM_PPLIB_LIMITED_REFRESHRATE_VALUE_MASK  0x00000F00
#define ATOM_PPLIB_LIMITED_REFRESHRATE_VALUE_SHIFT 8
#define ATOM_PPLIB_LIMITED_REFRESHRATE_UNLIMITED    0
#define ATOM_PPLIB_LIMITED_REFRESHRATE_50HZ         1
#define ATOM_PPLIB_SOFTWARE_DISABLE_LOADBALANCING        0x00001000
#define ATOM_PPLIB_SOFTWARE_ENABLE_SLEEP_FOR_TIMESTAMPS  0x00002000
#define ATOM_PPLIB_DISALLOW_ON_DC                       0x00004000
#define ATOM_PPLIB_ENABLE_VARIBRIGHT                     0x00008000
#define ATOM_PPLIB_SWSTATE_MEMORY_DLL_OFF               0x000010000
#define ATOM_PPLIB_M3ARB_MASK                       0x00060000
#define ATOM_PPLIB_M3ARB_SHIFT                      17
#define ATOM_PPLIB_ENABLE_DRR                       0x00080000
typedef struct _ATOM_PPLIB_THERMAL_STATE
{
    UCHAR   ucMinTemperature;
    UCHAR   ucMaxTemperature;
    UCHAR   ucThermalAction;
}ATOM_PPLIB_THERMAL_STATE, *LPATOM_PPLIB_THERMAL_STATE;
#define ATOM_PPLIB_NONCLOCKINFO_VER1      12
#define ATOM_PPLIB_NONCLOCKINFO_VER2      24
typedef struct _ATOM_PPLIB_NONCLOCK_INFO
{
      USHORT usClassification;
      UCHAR  ucMinTemperature;
      UCHAR  ucMaxTemperature;
      ULONG  ulCapsAndSettings;
      UCHAR  ucRequiredPower;
      USHORT usClassification2;
      ULONG  ulVCLK;
      ULONG  ulDCLK;
      UCHAR  ucUnused[5];
} ATOM_PPLIB_NONCLOCK_INFO;
typedef struct _ATOM_PPLIB_R600_CLOCK_INFO
{
      USHORT usEngineClockLow;
      UCHAR ucEngineClockHigh;
      USHORT usMemoryClockLow;
      UCHAR ucMemoryClockHigh;
      USHORT usVDDC;
      USHORT usUnused1;
      USHORT usUnused2;
      ULONG ulFlags;  
} ATOM_PPLIB_R600_CLOCK_INFO;
#define ATOM_PPLIB_R600_FLAGS_PCIEGEN2          1
#define ATOM_PPLIB_R600_FLAGS_UVDSAFE           2
#define ATOM_PPLIB_R600_FLAGS_BACKBIASENABLE    4
#define ATOM_PPLIB_R600_FLAGS_MEMORY_ODT_OFF    8
#define ATOM_PPLIB_R600_FLAGS_MEMORY_DLL_OFF   16
#define ATOM_PPLIB_R600_FLAGS_LOWPOWER         32    
typedef struct _ATOM_PPLIB_RS780_CLOCK_INFO
{
      USHORT usLowEngineClockLow;          
      UCHAR  ucLowEngineClockHigh;
      USHORT usHighEngineClockLow;         
      UCHAR  ucHighEngineClockHigh;
      USHORT usMemoryClockLow;             
      UCHAR  ucMemoryClockHigh;            
      UCHAR  ucPadding;                    
      USHORT usVDDC;                       
      UCHAR  ucMaxHTLinkWidth;             
      UCHAR  ucMinHTLinkWidth;             
      USHORT usHTLinkFreq;                 
      ULONG  ulFlags; 
} ATOM_PPLIB_RS780_CLOCK_INFO;
#define ATOM_PPLIB_RS780_VOLTAGE_NONE       0 
#define ATOM_PPLIB_RS780_VOLTAGE_LOW        1 
#define ATOM_PPLIB_RS780_VOLTAGE_HIGH       2 
#define ATOM_PPLIB_RS780_VOLTAGE_VARIABLE   3 
#define ATOM_PPLIB_RS780_SPMCLK_NONE        0    
#define ATOM_PPLIB_RS780_SPMCLK_LOW         1
#define ATOM_PPLIB_RS780_SPMCLK_HIGH        2
#define ATOM_PPLIB_RS780_HTLINKFREQ_NONE       0 
#define ATOM_PPLIB_RS780_HTLINKFREQ_LOW        1 
#define ATOM_PPLIB_RS780_HTLINKFREQ_HIGH       2 
typedef struct _ATOM_PPLIB_EVERGREEN_CLOCK_INFO
{
      USHORT usEngineClockLow;
      UCHAR  ucEngineClockHigh;
      USHORT usMemoryClockLow;
      UCHAR  ucMemoryClockHigh;
      USHORT usVDDC;
      USHORT usVDDCI;
      USHORT usUnused;
      ULONG ulFlags;  
} ATOM_PPLIB_EVERGREEN_CLOCK_INFO;
typedef struct _ATOM_PPLIB_SI_CLOCK_INFO
{
      USHORT usEngineClockLow;
      UCHAR  ucEngineClockHigh;
      USHORT usMemoryClockLow;
      UCHAR  ucMemoryClockHigh;
      USHORT usVDDC;
      USHORT usVDDCI;
      UCHAR  ucPCIEGen;
      UCHAR  ucUnused1;
      ULONG ulFlags;  
} ATOM_PPLIB_SI_CLOCK_INFO;
typedef struct _ATOM_PPLIB_CI_CLOCK_INFO
{
      USHORT usEngineClockLow;
      UCHAR  ucEngineClockHigh;
      USHORT usMemoryClockLow;
      UCHAR  ucMemoryClockHigh;
      UCHAR  ucPCIEGen;
      USHORT usPCIELane;
} ATOM_PPLIB_CI_CLOCK_INFO;
typedef struct _ATOM_PPLIB_SUMO_CLOCK_INFO{
      USHORT usEngineClockLow;   
      UCHAR  ucEngineClockHigh;  
      UCHAR  vddcIndex;          
      USHORT tdpLimit;
      USHORT rsv1;
      ULONG rsv2[2];
}ATOM_PPLIB_SUMO_CLOCK_INFO;
typedef struct _ATOM_PPLIB_KV_CLOCK_INFO {
      USHORT usEngineClockLow;
      UCHAR  ucEngineClockHigh;
      UCHAR  vddcIndex;
      USHORT tdpLimit;
      USHORT rsv1;
      ULONG rsv2[2];
} ATOM_PPLIB_KV_CLOCK_INFO;
typedef struct _ATOM_PPLIB_CZ_CLOCK_INFO {
      UCHAR index;
      UCHAR rsv[3];
} ATOM_PPLIB_CZ_CLOCK_INFO;
typedef struct _ATOM_PPLIB_STATE_V2
{
      UCHAR ucNumDPMLevels;
      UCHAR nonClockInfoIndex;
      UCHAR clockInfoIndex[];
} ATOM_PPLIB_STATE_V2;
typedef struct _StateArray{
    UCHAR ucNumEntries;
    ATOM_PPLIB_STATE_V2 states[1];
}StateArray;
typedef struct _ClockInfoArray{
    UCHAR ucNumEntries;
    UCHAR ucEntrySize;
    UCHAR clockInfo[1];
}ClockInfoArray;
typedef struct _NonClockInfoArray{
    UCHAR ucNumEntries;
    UCHAR ucEntrySize;
    ATOM_PPLIB_NONCLOCK_INFO nonClockInfo[1];
}NonClockInfoArray;
typedef struct _ATOM_PPLIB_Clock_Voltage_Dependency_Record
{
    USHORT usClockLow;
    UCHAR  ucClockHigh;
    USHORT usVoltage;
}ATOM_PPLIB_Clock_Voltage_Dependency_Record;
typedef struct _ATOM_PPLIB_Clock_Voltage_Dependency_Table
{
    UCHAR ucNumEntries;                                                 
    ATOM_PPLIB_Clock_Voltage_Dependency_Record entries[1];              
}ATOM_PPLIB_Clock_Voltage_Dependency_Table;
typedef struct _ATOM_PPLIB_Clock_Voltage_Limit_Record
{
    USHORT usSclkLow;
    UCHAR  ucSclkHigh;
    USHORT usMclkLow;
    UCHAR  ucMclkHigh;
    USHORT usVddc;
    USHORT usVddci;
}ATOM_PPLIB_Clock_Voltage_Limit_Record;
typedef struct _ATOM_PPLIB_Clock_Voltage_Limit_Table
{
    UCHAR ucNumEntries;                                                 
    ATOM_PPLIB_Clock_Voltage_Limit_Record entries[1];                   
}ATOM_PPLIB_Clock_Voltage_Limit_Table;
union _ATOM_PPLIB_CAC_Leakage_Record
{
    struct
    {
        USHORT usVddc;           
        ULONG  ulLeakageValue;   
    };
    struct
     {
        USHORT usVddc1;
        USHORT usVddc2;
        USHORT usVddc3;
     };
};
typedef union _ATOM_PPLIB_CAC_Leakage_Record ATOM_PPLIB_CAC_Leakage_Record;
typedef struct _ATOM_PPLIB_CAC_Leakage_Table
{
    UCHAR ucNumEntries;                                                  
    ATOM_PPLIB_CAC_Leakage_Record entries[1];                            
}ATOM_PPLIB_CAC_Leakage_Table;
typedef struct _ATOM_PPLIB_PhaseSheddingLimits_Record
{
    USHORT usVoltage;
    USHORT usSclkLow;
    UCHAR  ucSclkHigh;
    USHORT usMclkLow;
    UCHAR  ucMclkHigh;
}ATOM_PPLIB_PhaseSheddingLimits_Record;
typedef struct _ATOM_PPLIB_PhaseSheddingLimits_Table
{
    UCHAR ucNumEntries;                                                  
    ATOM_PPLIB_PhaseSheddingLimits_Record entries[1];                    
}ATOM_PPLIB_PhaseSheddingLimits_Table;
typedef struct _VCEClockInfo{
    USHORT usEVClkLow;
    UCHAR  ucEVClkHigh;
    USHORT usECClkLow;
    UCHAR  ucECClkHigh;
}VCEClockInfo;
typedef struct _VCEClockInfoArray{
    UCHAR ucNumEntries;
    VCEClockInfo entries[1];
}VCEClockInfoArray;
typedef struct _ATOM_PPLIB_VCE_Clock_Voltage_Limit_Record
{
    USHORT usVoltage;
    UCHAR  ucVCEClockInfoIndex;
}ATOM_PPLIB_VCE_Clock_Voltage_Limit_Record;
typedef struct _ATOM_PPLIB_VCE_Clock_Voltage_Limit_Table
{
    UCHAR numEntries;
    ATOM_PPLIB_VCE_Clock_Voltage_Limit_Record entries[1];
}ATOM_PPLIB_VCE_Clock_Voltage_Limit_Table;
typedef struct _ATOM_PPLIB_VCE_State_Record
{
    UCHAR  ucVCEClockInfoIndex;
    UCHAR  ucClockInfoIndex;  
}ATOM_PPLIB_VCE_State_Record;
typedef struct _ATOM_PPLIB_VCE_State_Table
{
    UCHAR numEntries;
    ATOM_PPLIB_VCE_State_Record entries[1];
}ATOM_PPLIB_VCE_State_Table;
typedef struct _ATOM_PPLIB_VCE_Table
{
      UCHAR revid;
}ATOM_PPLIB_VCE_Table;
typedef struct _UVDClockInfo{
    USHORT usVClkLow;
    UCHAR  ucVClkHigh;
    USHORT usDClkLow;
    UCHAR  ucDClkHigh;
}UVDClockInfo;
typedef struct _UVDClockInfoArray{
    UCHAR ucNumEntries;
    UVDClockInfo entries[1];
}UVDClockInfoArray;
typedef struct _ATOM_PPLIB_UVD_Clock_Voltage_Limit_Record
{
    USHORT usVoltage;
    UCHAR  ucUVDClockInfoIndex;
}ATOM_PPLIB_UVD_Clock_Voltage_Limit_Record;
typedef struct _ATOM_PPLIB_UVD_Clock_Voltage_Limit_Table
{
    UCHAR numEntries;
    ATOM_PPLIB_UVD_Clock_Voltage_Limit_Record entries[1];
}ATOM_PPLIB_UVD_Clock_Voltage_Limit_Table;
typedef struct _ATOM_PPLIB_UVD_Table
{
      UCHAR revid;
}ATOM_PPLIB_UVD_Table;
typedef struct _ATOM_PPLIB_SAMClk_Voltage_Limit_Record
{
      USHORT usVoltage;
      USHORT usSAMClockLow;
      UCHAR  ucSAMClockHigh;
}ATOM_PPLIB_SAMClk_Voltage_Limit_Record;
typedef struct _ATOM_PPLIB_SAMClk_Voltage_Limit_Table{
    UCHAR numEntries;
    ATOM_PPLIB_SAMClk_Voltage_Limit_Record entries[1];
}ATOM_PPLIB_SAMClk_Voltage_Limit_Table;
typedef struct _ATOM_PPLIB_SAMU_Table
{
      UCHAR revid;
      ATOM_PPLIB_SAMClk_Voltage_Limit_Table limits;
}ATOM_PPLIB_SAMU_Table;
typedef struct _ATOM_PPLIB_ACPClk_Voltage_Limit_Record
{
      USHORT usVoltage;
      USHORT usACPClockLow;
      UCHAR  ucACPClockHigh;
}ATOM_PPLIB_ACPClk_Voltage_Limit_Record;
typedef struct _ATOM_PPLIB_ACPClk_Voltage_Limit_Table{
    UCHAR numEntries;
    ATOM_PPLIB_ACPClk_Voltage_Limit_Record entries[1];
}ATOM_PPLIB_ACPClk_Voltage_Limit_Table;
typedef struct _ATOM_PPLIB_ACP_Table
{
      UCHAR revid;
      ATOM_PPLIB_ACPClk_Voltage_Limit_Table limits;
}ATOM_PPLIB_ACP_Table;
typedef struct _ATOM_PowerTune_Table{
    USHORT usTDP;
    USHORT usConfigurableTDP;
    USHORT usTDC;
    USHORT usBatteryPowerLimit;
    USHORT usSmallPowerLimit;
    USHORT usLowCACLeakage;
    USHORT usHighCACLeakage;
}ATOM_PowerTune_Table;
typedef struct _ATOM_PPLIB_POWERTUNE_Table
{
      UCHAR revid;
      ATOM_PowerTune_Table power_tune_table;
}ATOM_PPLIB_POWERTUNE_Table;
typedef struct _ATOM_PPLIB_POWERTUNE_Table_V1
{
      UCHAR revid;
      ATOM_PowerTune_Table power_tune_table;
      USHORT usMaximumPowerDeliveryLimit;
      USHORT usTjMax;
      USHORT usReserve[6];
} ATOM_PPLIB_POWERTUNE_Table_V1;
#define ATOM_PPM_A_A    1
#define ATOM_PPM_A_I    2
typedef struct _ATOM_PPLIB_PPM_Table
{
      UCHAR  ucRevId;
      UCHAR  ucPpmDesign;           
      USHORT usCpuCoreNumber;
      ULONG  ulPlatformTDP;
      ULONG  ulSmallACPlatformTDP;
      ULONG  ulPlatformTDC;
      ULONG  ulSmallACPlatformTDC;
      ULONG  ulApuTDP;
      ULONG  ulDGpuTDP;  
      ULONG  ulDGpuUlvPower;
      ULONG  ulTjmax;
} ATOM_PPLIB_PPM_Table;
#define    VQ_DisplayConfig_NoneAWD   1
#define    VQ_DisplayConfig_AWD       2
typedef struct ATOM_PPLIB_VQ_Budgeting_Record{
    ULONG ulDeviceID;
    ULONG ulSustainableSOCPowerLimitLow;  
    ULONG ulSustainableSOCPowerLimitHigh;  
    ULONG ulDClk;
    ULONG ulEClk;
    ULONG ulDispSclk;
    UCHAR ucDispConfig;
} ATOM_PPLIB_VQ_Budgeting_Record;
typedef struct ATOM_PPLIB_VQ_Budgeting_Table {
    UCHAR revid;
    UCHAR numEntries;
    ATOM_PPLIB_VQ_Budgeting_Record         entries[1];
} ATOM_PPLIB_VQ_Budgeting_Table;
#pragma pack()
#endif
