#ifndef _VEGA10_PPTABLE_H_
#define _VEGA10_PPTABLE_H_
#pragma pack(push, 1)
#define ATOM_VEGA10_PP_FANPARAMETERS_TACHOMETER_PULSES_PER_REVOLUTION_MASK 0x0f
#define ATOM_VEGA10_PP_FANPARAMETERS_NOFAN                                 0x80
#define ATOM_VEGA10_PP_THERMALCONTROLLER_NONE      0
#define ATOM_VEGA10_PP_THERMALCONTROLLER_LM96163   17
#define ATOM_VEGA10_PP_THERMALCONTROLLER_VEGA10    24
#define ATOM_VEGA10_PP_THERMALCONTROLLER_ADT7473_WITH_INTERNAL   0x89
#define ATOM_VEGA10_PP_THERMALCONTROLLER_EMC2103_WITH_INTERNAL   0x8D
#define ATOM_VEGA10_PP_PLATFORM_CAP_POWERPLAY                   0x1
#define ATOM_VEGA10_PP_PLATFORM_CAP_SBIOSPOWERSOURCE            0x2
#define ATOM_VEGA10_PP_PLATFORM_CAP_HARDWAREDC                  0x4
#define ATOM_VEGA10_PP_PLATFORM_CAP_BACO                        0x8
#define ATOM_VEGA10_PP_PLATFORM_COMBINE_PCC_WITH_THERMAL_SIGNAL 0x10
#define ATOM_PPLIB_CLASSIFICATION_UI_MASK               0x0007
#define ATOM_PPLIB_CLASSIFICATION_UI_SHIFT              0
#define ATOM_PPLIB_CLASSIFICATION_UI_NONE               0
#define ATOM_PPLIB_CLASSIFICATION_UI_BATTERY            1
#define ATOM_PPLIB_CLASSIFICATION_UI_BALANCED           3
#define ATOM_PPLIB_CLASSIFICATION_UI_PERFORMANCE        5
#define ATOM_PPLIB_CLASSIFICATION_BOOT                  0x0008
#define ATOM_PPLIB_CLASSIFICATION_THERMAL               0x0010
#define ATOM_PPLIB_CLASSIFICATION_LIMITEDPOWERSOURCE    0x0020
#define ATOM_PPLIB_CLASSIFICATION_REST                  0x0040
#define ATOM_PPLIB_CLASSIFICATION_FORCED                0x0080
#define ATOM_PPLIB_CLASSIFICATION_ACPI                  0x1000
#define ATOM_PPLIB_CLASSIFICATION2_LIMITEDPOWERSOURCE_2 0x0001
#define ATOM_Vega10_DISALLOW_ON_DC                   0x00004000
#define ATOM_Vega10_ENABLE_VARIBRIGHT                0x00008000
#define ATOM_Vega10_TABLE_REVISION_VEGA10         8
#define ATOM_Vega10_VoltageMode_AVFS_Interpolate     0
#define ATOM_Vega10_VoltageMode_AVFS_WorstCase       1
#define ATOM_Vega10_VoltageMode_Static               2
typedef struct _ATOM_Vega10_POWERPLAYTABLE {
	struct atom_common_table_header sHeader;
	UCHAR  ucTableRevision;
	USHORT usTableSize;                         
	ULONG  ulGoldenPPID;                        
	ULONG  ulGoldenRevision;                    
	USHORT usFormatID;                          
	ULONG  ulPlatformCaps;                      
	ULONG  ulMaxODEngineClock;                  
	ULONG  ulMaxODMemoryClock;                  
	USHORT usPowerControlLimit;
	USHORT usUlvVoltageOffset;                  
	USHORT usUlvSmnclkDid;
	USHORT usUlvMp1clkDid;
	USHORT usUlvGfxclkBypass;
	USHORT usGfxclkSlewRate;
	UCHAR  ucGfxVoltageMode;
	UCHAR  ucSocVoltageMode;
	UCHAR  ucUclkVoltageMode;
	UCHAR  ucUvdVoltageMode;
	UCHAR  ucVceVoltageMode;
	UCHAR  ucMp0VoltageMode;
	UCHAR  ucDcefVoltageMode;
	USHORT usStateArrayOffset;                  
	USHORT usFanTableOffset;                    
	USHORT usThermalControllerOffset;           
	USHORT usSocclkDependencyTableOffset;       
	USHORT usMclkDependencyTableOffset;         
	USHORT usGfxclkDependencyTableOffset;       
	USHORT usDcefclkDependencyTableOffset;      
	USHORT usVddcLookupTableOffset;             
	USHORT usVddmemLookupTableOffset;           
	USHORT usMMDependencyTableOffset;           
	USHORT usVCEStateTableOffset;               
	USHORT usReserve;                           
	USHORT usPowerTuneTableOffset;              
	USHORT usHardLimitTableOffset;              
	USHORT usVddciLookupTableOffset;            
	USHORT usPCIETableOffset;                   
	USHORT usPixclkDependencyTableOffset;       
	USHORT usDispClkDependencyTableOffset;      
	USHORT usPhyClkDependencyTableOffset;       
} ATOM_Vega10_POWERPLAYTABLE;
typedef struct _ATOM_Vega10_State {
	UCHAR  ucSocClockIndexHigh;
	UCHAR  ucSocClockIndexLow;
	UCHAR  ucGfxClockIndexHigh;
	UCHAR  ucGfxClockIndexLow;
	UCHAR  ucMemClockIndexHigh;
	UCHAR  ucMemClockIndexLow;
	USHORT usClassification;
	ULONG  ulCapsAndSettings;
	USHORT usClassification2;
} ATOM_Vega10_State;
typedef struct _ATOM_Vega10_State_Array {
	UCHAR ucRevId;
	UCHAR ucNumEntries;                                          
	ATOM_Vega10_State states[1];                              
} ATOM_Vega10_State_Array;
typedef struct _ATOM_Vega10_CLK_Dependency_Record {
	ULONG  ulClk;                                                
	UCHAR  ucVddInd;                                             
} ATOM_Vega10_CLK_Dependency_Record;
typedef struct _ATOM_Vega10_GFXCLK_Dependency_Record {
	ULONG  ulClk;                                                
	UCHAR  ucVddInd;                                             
	USHORT usCKSVOffsetandDisable;                               
	USHORT usAVFSOffset;                                         
} ATOM_Vega10_GFXCLK_Dependency_Record;
typedef struct _ATOM_Vega10_GFXCLK_Dependency_Record_V2 {
	ULONG  ulClk;
	UCHAR  ucVddInd;
	USHORT usCKSVOffsetandDisable;
	USHORT usAVFSOffset;
	UCHAR  ucACGEnable;
	UCHAR  ucReserved[3];
} ATOM_Vega10_GFXCLK_Dependency_Record_V2;
typedef struct _ATOM_Vega10_MCLK_Dependency_Record {
	ULONG  ulMemClk;                                             
	UCHAR  ucVddInd;                                             
	UCHAR  ucVddMemInd;                                          
	UCHAR  ucVddciInd;                                           
} ATOM_Vega10_MCLK_Dependency_Record;
typedef struct _ATOM_Vega10_GFXCLK_Dependency_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries;					 
	ATOM_Vega10_GFXCLK_Dependency_Record entries[];		 
} ATOM_Vega10_GFXCLK_Dependency_Table;
typedef struct _ATOM_Vega10_MCLK_Dependency_Table {
    UCHAR ucRevId;
    UCHAR ucNumEntries;                                          
    ATOM_Vega10_MCLK_Dependency_Record entries[1];             
} ATOM_Vega10_MCLK_Dependency_Table;
typedef struct _ATOM_Vega10_SOCCLK_Dependency_Table {
    UCHAR ucRevId;
    UCHAR ucNumEntries;                                          
    ATOM_Vega10_CLK_Dependency_Record entries[1];             
} ATOM_Vega10_SOCCLK_Dependency_Table;
typedef struct _ATOM_Vega10_DCEFCLK_Dependency_Table {
    UCHAR ucRevId;
    UCHAR ucNumEntries;                                          
    ATOM_Vega10_CLK_Dependency_Record entries[1];             
} ATOM_Vega10_DCEFCLK_Dependency_Table;
typedef struct _ATOM_Vega10_PIXCLK_Dependency_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries;                                          
	ATOM_Vega10_CLK_Dependency_Record entries[1];             
} ATOM_Vega10_PIXCLK_Dependency_Table;
typedef struct _ATOM_Vega10_DISPCLK_Dependency_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries;                                          
	ATOM_Vega10_CLK_Dependency_Record entries[1];             
} ATOM_Vega10_DISPCLK_Dependency_Table;
typedef struct _ATOM_Vega10_PHYCLK_Dependency_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries;                                          
	ATOM_Vega10_CLK_Dependency_Record entries[1];             
} ATOM_Vega10_PHYCLK_Dependency_Table;
typedef struct _ATOM_Vega10_MM_Dependency_Record {
    UCHAR  ucVddcInd;                                            
    ULONG  ulDClk;                                               
    ULONG  ulVClk;                                               
    ULONG  ulEClk;                                               
    ULONG  ulPSPClk;                                             
} ATOM_Vega10_MM_Dependency_Record;
typedef struct _ATOM_Vega10_MM_Dependency_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries;                                          
	ATOM_Vega10_MM_Dependency_Record entries[1];              
} ATOM_Vega10_MM_Dependency_Table;
typedef struct _ATOM_Vega10_PCIE_Record {
	ULONG ulLCLK;                                                
	UCHAR ucPCIEGenSpeed;                                        
	UCHAR ucPCIELaneWidth;                                       
} ATOM_Vega10_PCIE_Record;
typedef struct _ATOM_Vega10_PCIE_Table {
	UCHAR  ucRevId;
	UCHAR  ucNumEntries;                                         
	ATOM_Vega10_PCIE_Record entries[1];                       
} ATOM_Vega10_PCIE_Table;
typedef struct _ATOM_Vega10_Voltage_Lookup_Record {
	USHORT usVdd;                                                
} ATOM_Vega10_Voltage_Lookup_Record;
typedef struct _ATOM_Vega10_Voltage_Lookup_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries;                                           
	ATOM_Vega10_Voltage_Lookup_Record entries[1];              
} ATOM_Vega10_Voltage_Lookup_Table;
typedef struct _ATOM_Vega10_Fan_Table {
	UCHAR   ucRevId;                          
	USHORT  usFanOutputSensitivity;           
	USHORT  usFanRPMMax;                      
	USHORT  usThrottlingRPM;
	USHORT  usFanAcousticLimit;               
	USHORT  usTargetTemperature;              
	USHORT  usMinimumPWMLimit;                
	USHORT  usTargetGfxClk;                    
	USHORT  usFanGainEdge;
	USHORT  usFanGainHotspot;
	USHORT  usFanGainLiquid;
	USHORT  usFanGainVrVddc;
	USHORT  usFanGainVrMvdd;
	USHORT  usFanGainPlx;
	USHORT  usFanGainHbm;
	UCHAR   ucEnableZeroRPM;
	USHORT  usFanStopTemperature;
	USHORT  usFanStartTemperature;
} ATOM_Vega10_Fan_Table;
typedef struct _ATOM_Vega10_Fan_Table_V2 {
	UCHAR   ucRevId;
	USHORT  usFanOutputSensitivity;
	USHORT  usFanAcousticLimitRpm;
	USHORT  usThrottlingRPM;
	USHORT  usTargetTemperature;
	USHORT  usMinimumPWMLimit;
	USHORT  usTargetGfxClk;
	USHORT  usFanGainEdge;
	USHORT  usFanGainHotspot;
	USHORT  usFanGainLiquid;
	USHORT  usFanGainVrVddc;
	USHORT  usFanGainVrMvdd;
	USHORT  usFanGainPlx;
	USHORT  usFanGainHbm;
	UCHAR   ucEnableZeroRPM;
	USHORT  usFanStopTemperature;
	USHORT  usFanStartTemperature;
	UCHAR   ucFanParameters;
	UCHAR   ucFanMinRPM;
	UCHAR   ucFanMaxRPM;
} ATOM_Vega10_Fan_Table_V2;
typedef struct _ATOM_Vega10_Fan_Table_V3 {
	UCHAR   ucRevId;
	USHORT  usFanOutputSensitivity;
	USHORT  usFanAcousticLimitRpm;
	USHORT  usThrottlingRPM;
	USHORT  usTargetTemperature;
	USHORT  usMinimumPWMLimit;
	USHORT  usTargetGfxClk;
	USHORT  usFanGainEdge;
	USHORT  usFanGainHotspot;
	USHORT  usFanGainLiquid;
	USHORT  usFanGainVrVddc;
	USHORT  usFanGainVrMvdd;
	USHORT  usFanGainPlx;
	USHORT  usFanGainHbm;
	UCHAR   ucEnableZeroRPM;
	USHORT  usFanStopTemperature;
	USHORT  usFanStartTemperature;
	UCHAR   ucFanParameters;
	UCHAR   ucFanMinRPM;
	UCHAR   ucFanMaxRPM;
	USHORT  usMGpuThrottlingRPM;
} ATOM_Vega10_Fan_Table_V3;
typedef struct _ATOM_Vega10_Thermal_Controller {
	UCHAR ucRevId;
	UCHAR ucType;            
	UCHAR ucI2cLine;         
	UCHAR ucI2cAddress;
	UCHAR ucFanParameters;   
	UCHAR ucFanMinRPM;       
	UCHAR ucFanMaxRPM;       
    UCHAR ucFlags;           
} ATOM_Vega10_Thermal_Controller;
typedef struct _ATOM_Vega10_VCE_State_Record {
    UCHAR  ucVCEClockIndex;          
    UCHAR  ucFlag;                   
    UCHAR  ucSCLKIndex;              
    UCHAR  ucMCLKIndex;              
} ATOM_Vega10_VCE_State_Record;
typedef struct _ATOM_Vega10_VCE_State_Table {
    UCHAR ucRevId;
    UCHAR ucNumEntries;
    ATOM_Vega10_VCE_State_Record entries[1];
} ATOM_Vega10_VCE_State_Table;
typedef struct _ATOM_Vega10_PowerTune_Table {
	UCHAR  ucRevId;
	USHORT usSocketPowerLimit;
	USHORT usBatteryPowerLimit;
	USHORT usSmallPowerLimit;
	USHORT usTdcLimit;
	USHORT usEdcLimit;
	USHORT usSoftwareShutdownTemp;
	USHORT usTemperatureLimitHotSpot;
	USHORT usTemperatureLimitLiquid1;
	USHORT usTemperatureLimitLiquid2;
	USHORT usTemperatureLimitHBM;
	USHORT usTemperatureLimitVrSoc;
	USHORT usTemperatureLimitVrMem;
	USHORT usTemperatureLimitPlx;
	USHORT usLoadLineResistance;
	UCHAR  ucLiquid1_I2C_address;
	UCHAR  ucLiquid2_I2C_address;
	UCHAR  ucVr_I2C_address;
	UCHAR  ucPlx_I2C_address;
	UCHAR  ucLiquid_I2C_LineSCL;
	UCHAR  ucLiquid_I2C_LineSDA;
	UCHAR  ucVr_I2C_LineSCL;
	UCHAR  ucVr_I2C_LineSDA;
	UCHAR  ucPlx_I2C_LineSCL;
	UCHAR  ucPlx_I2C_LineSDA;
	USHORT usTemperatureLimitTedge;
} ATOM_Vega10_PowerTune_Table;
typedef struct _ATOM_Vega10_PowerTune_Table_V2 {
	UCHAR  ucRevId;
	USHORT usSocketPowerLimit;
	USHORT usBatteryPowerLimit;
	USHORT usSmallPowerLimit;
	USHORT usTdcLimit;
	USHORT usEdcLimit;
	USHORT usSoftwareShutdownTemp;
	USHORT usTemperatureLimitHotSpot;
	USHORT usTemperatureLimitLiquid1;
	USHORT usTemperatureLimitLiquid2;
	USHORT usTemperatureLimitHBM;
	USHORT usTemperatureLimitVrSoc;
	USHORT usTemperatureLimitVrMem;
	USHORT usTemperatureLimitPlx;
	USHORT usLoadLineResistance;
	UCHAR ucLiquid1_I2C_address;
	UCHAR ucLiquid2_I2C_address;
	UCHAR ucLiquid_I2C_Line;
	UCHAR ucVr_I2C_address;
	UCHAR ucVr_I2C_Line;
	UCHAR ucPlx_I2C_address;
	UCHAR ucPlx_I2C_Line;
	USHORT usTemperatureLimitTedge;
} ATOM_Vega10_PowerTune_Table_V2;
typedef struct _ATOM_Vega10_PowerTune_Table_V3 {
	UCHAR  ucRevId;
	USHORT usSocketPowerLimit;
	USHORT usBatteryPowerLimit;
	USHORT usSmallPowerLimit;
	USHORT usTdcLimit;
	USHORT usEdcLimit;
	USHORT usSoftwareShutdownTemp;
	USHORT usTemperatureLimitHotSpot;
	USHORT usTemperatureLimitLiquid1;
	USHORT usTemperatureLimitLiquid2;
	USHORT usTemperatureLimitHBM;
	USHORT usTemperatureLimitVrSoc;
	USHORT usTemperatureLimitVrMem;
	USHORT usTemperatureLimitPlx;
	USHORT usLoadLineResistance;
	UCHAR  ucLiquid1_I2C_address;
	UCHAR  ucLiquid2_I2C_address;
	UCHAR  ucLiquid_I2C_Line;
	UCHAR  ucVr_I2C_address;
	UCHAR  ucVr_I2C_Line;
	UCHAR  ucPlx_I2C_address;
	UCHAR  ucPlx_I2C_Line;
	USHORT usTemperatureLimitTedge;
	USHORT usBoostStartTemperature;
	USHORT usBoostStopTemperature;
	ULONG  ulBoostClock;
	ULONG  Reserved[2];
} ATOM_Vega10_PowerTune_Table_V3;
typedef struct _ATOM_Vega10_Hard_Limit_Record {
    ULONG  ulSOCCLKLimit;
    ULONG  ulGFXCLKLimit;
    ULONG  ulMCLKLimit;
    USHORT usVddcLimit;
    USHORT usVddciLimit;
    USHORT usVddMemLimit;
} ATOM_Vega10_Hard_Limit_Record;
typedef struct _ATOM_Vega10_Hard_Limit_Table {
    UCHAR ucRevId;
    UCHAR ucNumEntries;
    ATOM_Vega10_Hard_Limit_Record entries[1];
} ATOM_Vega10_Hard_Limit_Table;
typedef struct _Vega10_PPTable_Generic_SubTable_Header {
    UCHAR  ucRevId;
} Vega10_PPTable_Generic_SubTable_Header;
#pragma pack(pop)
#endif
