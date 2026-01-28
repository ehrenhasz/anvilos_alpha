#ifndef TONGA_PPTABLE_H
#define TONGA_PPTABLE_H
#pragma pack(push, 1)
#include "hwmgr.h"
#define ATOM_TONGA_PP_FANPARAMETERS_TACHOMETER_PULSES_PER_REVOLUTION_MASK 0x0f
#define ATOM_TONGA_PP_FANPARAMETERS_NOFAN                                 0x80     
#define ATOM_TONGA_PP_THERMALCONTROLLER_NONE      0
#define ATOM_TONGA_PP_THERMALCONTROLLER_LM96163   17
#define ATOM_TONGA_PP_THERMALCONTROLLER_TONGA     21
#define ATOM_TONGA_PP_THERMALCONTROLLER_FIJI      22
#define ATOM_TONGA_PP_THERMALCONTROLLER_ADT7473_WITH_INTERNAL   0x89     
#define ATOM_TONGA_PP_THERMALCONTROLLER_EMC2103_WITH_INTERNAL   0x8D     
#define ATOM_TONGA_PP_PLATFORM_CAP_VDDGFX_CONTROL              0x1             
#define ATOM_TONGA_PP_PLATFORM_CAP_POWERPLAY                   0x2             
#define ATOM_TONGA_PP_PLATFORM_CAP_SBIOSPOWERSOURCE            0x4             
#define ATOM_TONGA_PP_PLATFORM_CAP_DISABLE_VOLTAGE_ISLAND      0x8             
#define ____RETIRE16____                                0x10
#define ATOM_TONGA_PP_PLATFORM_CAP_HARDWAREDC                 0x20             
#define ____RETIRE64____                                0x40
#define ____RETIRE128____                               0x80
#define ____RETIRE256____                              0x100
#define ____RETIRE512____                              0x200
#define ____RETIRE1024____                             0x400
#define ____RETIRE2048____                             0x800
#define ATOM_TONGA_PP_PLATFORM_CAP_MVDD_CONTROL             0x1000             
#define ____RETIRE2000____                            0x2000
#define ____RETIRE4000____                            0x4000
#define ATOM_TONGA_PP_PLATFORM_CAP_VDDCI_CONTROL            0x8000             
#define ____RETIRE10000____                          0x10000
#define ATOM_TONGA_PP_PLATFORM_CAP_BACO                    0x20000             
#define ATOM_TONGA_PP_PLATFORM_CAP_OUTPUT_THERMAL2GPIO17         0x100000      
#define ATOM_TONGA_PP_PLATFORM_COMBINE_PCC_WITH_THERMAL_SIGNAL  0x1000000      
#define ATOM_TONGA_PLATFORM_LOAD_POST_PRODUCTION_FIRMWARE       0x2000000
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
#define ATOM_Tonga_DISALLOW_ON_DC                       0x00004000
#define ATOM_Tonga_ENABLE_VARIBRIGHT                    0x00008000
#define ATOM_Tonga_TABLE_REVISION_TONGA                 7
typedef struct _ATOM_Tonga_POWERPLAYTABLE {
	ATOM_COMMON_TABLE_HEADER sHeader;
	UCHAR  ucTableRevision;
	USHORT usTableSize;						 
	ULONG	ulGoldenPPID;
	ULONG	ulGoldenRevision;
	USHORT	usFormatID;
	USHORT	usVoltageTime;					  
	ULONG	ulPlatformCaps;					   
	ULONG	ulMaxODEngineClock; 			    
	ULONG	ulMaxODMemoryClock; 			    
	USHORT	usPowerControlLimit;
	USHORT	usUlvVoltageOffset;				   
	USHORT	usStateArrayOffset;				   
	USHORT	usFanTableOffset;				   
	USHORT	usThermalControllerOffset;		    
	USHORT	usReserv;						    
	USHORT	usMclkDependencyTableOffset;	    
	USHORT	usSclkDependencyTableOffset;	    
	USHORT	usVddcLookupTableOffset;		    
	USHORT	usVddgfxLookupTableOffset; 		 
	USHORT	usMMDependencyTableOffset;		   
	USHORT	usVCEStateTableOffset;			    
	USHORT	usPPMTableOffset;				   
	USHORT	usPowerTuneTableOffset;			   
	USHORT	usHardLimitTableOffset; 		    
	USHORT	usPCIETableOffset;				   
	USHORT	usGPIOTableOffset;				   
	USHORT	usReserved[6];					    
} ATOM_Tonga_POWERPLAYTABLE;
typedef struct _ATOM_Tonga_State {
	UCHAR  ucEngineClockIndexHigh;
	UCHAR  ucEngineClockIndexLow;
	UCHAR  ucMemoryClockIndexHigh;
	UCHAR  ucMemoryClockIndexLow;
	UCHAR  ucPCIEGenLow;
	UCHAR  ucPCIEGenHigh;
	UCHAR  ucPCIELaneLow;
	UCHAR  ucPCIELaneHigh;
	USHORT usClassification;
	ULONG ulCapsAndSettings;
	USHORT usClassification2;
	UCHAR  ucUnused[4];
} ATOM_Tonga_State;
typedef struct _ATOM_Tonga_State_Array {
	UCHAR ucRevId;
	UCHAR ucNumEntries;		 
	ATOM_Tonga_State entries[];	 
} ATOM_Tonga_State_Array;
typedef struct _ATOM_Tonga_MCLK_Dependency_Record {
	UCHAR  ucVddcInd;	 
	USHORT usVddci;
	USHORT usVddgfxOffset;	 
	USHORT usMvdd;
	ULONG ulMclk;
	USHORT usReserved;
} ATOM_Tonga_MCLK_Dependency_Record;
typedef struct _ATOM_Tonga_MCLK_Dependency_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries; 										 
	ATOM_Tonga_MCLK_Dependency_Record entries[];				 
} ATOM_Tonga_MCLK_Dependency_Table;
typedef struct _ATOM_Tonga_SCLK_Dependency_Record {
	UCHAR  ucVddInd;											 
	USHORT usVddcOffset;										 
	ULONG ulSclk;
	USHORT usEdcCurrent;
	UCHAR  ucReliabilityTemperature;
	UCHAR  ucCKSVOffsetandDisable;							   
} ATOM_Tonga_SCLK_Dependency_Record;
typedef struct _ATOM_Tonga_SCLK_Dependency_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries; 										 
	ATOM_Tonga_SCLK_Dependency_Record entries[];				  
} ATOM_Tonga_SCLK_Dependency_Table;
typedef struct _ATOM_Polaris_SCLK_Dependency_Record {
	UCHAR  ucVddInd;											 
	USHORT usVddcOffset;										 
	ULONG ulSclk;
	USHORT usEdcCurrent;
	UCHAR  ucReliabilityTemperature;
	UCHAR  ucCKSVOffsetandDisable;			 
	ULONG  ulSclkOffset;
} ATOM_Polaris_SCLK_Dependency_Record;
typedef struct _ATOM_Polaris_SCLK_Dependency_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries;							 
	ATOM_Polaris_SCLK_Dependency_Record entries[];				  
} ATOM_Polaris_SCLK_Dependency_Table;
typedef struct _ATOM_Tonga_PCIE_Record {
	UCHAR ucPCIEGenSpeed;
	UCHAR usPCIELaneWidth;
	UCHAR ucReserved[2];
} ATOM_Tonga_PCIE_Record;
typedef struct _ATOM_Tonga_PCIE_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries; 										 
	ATOM_Tonga_PCIE_Record entries[];							 
} ATOM_Tonga_PCIE_Table;
typedef struct _ATOM_Polaris10_PCIE_Record {
	UCHAR ucPCIEGenSpeed;
	UCHAR usPCIELaneWidth;
	UCHAR ucReserved[2];
	ULONG ulPCIE_Sclk;
} ATOM_Polaris10_PCIE_Record;
typedef struct _ATOM_Polaris10_PCIE_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries;                                          
	ATOM_Polaris10_PCIE_Record entries[];                       
} ATOM_Polaris10_PCIE_Table;
typedef struct _ATOM_Tonga_MM_Dependency_Record {
	UCHAR   ucVddcInd;											  
	USHORT  usVddgfxOffset;									   
	ULONG  ulDClk;												 
	ULONG  ulVClk;												 
	ULONG  ulEClk;												 
	ULONG  ulAClk;												 
	ULONG  ulSAMUClk;											 
} ATOM_Tonga_MM_Dependency_Record;
typedef struct _ATOM_Tonga_MM_Dependency_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries; 										 
	ATOM_Tonga_MM_Dependency_Record entries[]; 			    
} ATOM_Tonga_MM_Dependency_Table;
typedef struct _ATOM_Tonga_Voltage_Lookup_Record {
	USHORT usVdd;											    
	USHORT usCACLow;
	USHORT usCACMid;
	USHORT usCACHigh;
} ATOM_Tonga_Voltage_Lookup_Record;
typedef struct _ATOM_Tonga_Voltage_Lookup_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries; 										 
	ATOM_Tonga_Voltage_Lookup_Record entries[];				 
} ATOM_Tonga_Voltage_Lookup_Table;
typedef struct _ATOM_Tonga_Fan_Table {
	UCHAR   ucRevId;						  
	UCHAR   ucTHyst;						  
	USHORT  usTMin; 						  
	USHORT  usTMed; 						  
	USHORT  usTHigh;						  
	USHORT  usPWMMin;						  
	USHORT  usPWMMed;						  
	USHORT  usPWMHigh;						  
	USHORT  usTMax; 						  
	UCHAR   ucFanControlMode;				   
	USHORT  usFanPWMMax;					   
	USHORT  usFanOutputSensitivity;		   
	USHORT  usFanRPMMax;					   
	ULONG  ulMinFanSCLKAcousticLimit;	    
	UCHAR   ucTargetTemperature;			  
	UCHAR   ucMinimumPWMLimit; 			   
	USHORT  usReserved;
} ATOM_Tonga_Fan_Table;
typedef struct _ATOM_Fiji_Fan_Table {
	UCHAR   ucRevId;						  
	UCHAR   ucTHyst;						  
	USHORT  usTMin; 						  
	USHORT  usTMed; 						  
	USHORT  usTHigh;						  
	USHORT  usPWMMin;						  
	USHORT  usPWMMed;						  
	USHORT  usPWMHigh;						  
	USHORT  usTMax; 						  
	UCHAR   ucFanControlMode;				   
	USHORT  usFanPWMMax;					   
	USHORT  usFanOutputSensitivity;		   
	USHORT  usFanRPMMax;					   
	ULONG  ulMinFanSCLKAcousticLimit;		 
	UCHAR   ucTargetTemperature;			  
	UCHAR   ucMinimumPWMLimit; 			   
	USHORT  usFanGainEdge;
	USHORT  usFanGainHotspot;
	USHORT  usFanGainLiquid;
	USHORT  usFanGainVrVddc;
	USHORT  usFanGainVrMvdd;
	USHORT  usFanGainPlx;
	USHORT  usFanGainHbm;
	USHORT  usReserved;
} ATOM_Fiji_Fan_Table;
typedef struct _ATOM_Polaris_Fan_Table {
	UCHAR   ucRevId;						  
	UCHAR   ucTHyst;						  
	USHORT  usTMin; 						  
	USHORT  usTMed; 						  
	USHORT  usTHigh;						  
	USHORT  usPWMMin;						  
	USHORT  usPWMMed;						  
	USHORT  usPWMHigh;						  
	USHORT  usTMax; 						  
	UCHAR   ucFanControlMode;				   
	USHORT  usFanPWMMax;					   
	USHORT  usFanOutputSensitivity;		   
	USHORT  usFanRPMMax;					   
	ULONG  ulMinFanSCLKAcousticLimit;		 
	UCHAR   ucTargetTemperature;			  
	UCHAR   ucMinimumPWMLimit; 			   
	USHORT  usFanGainEdge;
	USHORT  usFanGainHotspot;
	USHORT  usFanGainLiquid;
	USHORT  usFanGainVrVddc;
	USHORT  usFanGainVrMvdd;
	USHORT  usFanGainPlx;
	USHORT  usFanGainHbm;
	UCHAR   ucEnableZeroRPM;
	UCHAR   ucFanStopTemperature;
	UCHAR   ucFanStartTemperature;
	USHORT  usReserved;
} ATOM_Polaris_Fan_Table;
typedef struct _ATOM_Tonga_Thermal_Controller {
	UCHAR ucRevId;
	UCHAR ucType;		    
	UCHAR ucI2cLine;		 
	UCHAR ucI2cAddress;
	UCHAR ucFanParameters;	 
	UCHAR ucFanMinRPM; 	  
	UCHAR ucFanMaxRPM; 	  
	UCHAR ucReserved;
	UCHAR ucFlags;		    
} ATOM_Tonga_Thermal_Controller;
typedef struct _ATOM_Tonga_VCE_State_Record {
	UCHAR  ucVCEClockIndex;	 
	UCHAR  ucFlag;		 
	UCHAR  ucSCLKIndex;		 
	UCHAR  ucMCLKIndex;		 
} ATOM_Tonga_VCE_State_Record;
typedef struct _ATOM_Tonga_VCE_State_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries;
	ATOM_Tonga_VCE_State_Record entries[1];
} ATOM_Tonga_VCE_State_Table;
typedef struct _ATOM_Tonga_PowerTune_Table {
	UCHAR  ucRevId;
	USHORT usTDP;
	USHORT usConfigurableTDP;
	USHORT usTDC;
	USHORT usBatteryPowerLimit;
	USHORT usSmallPowerLimit;
	USHORT usLowCACLeakage;
	USHORT usHighCACLeakage;
	USHORT usMaximumPowerDeliveryLimit;
	USHORT usTjMax;
	USHORT usPowerTuneDataSetID;
	USHORT usEDCLimit;
	USHORT usSoftwareShutdownTemp;
	USHORT usClockStretchAmount;
	USHORT usReserve[2];
} ATOM_Tonga_PowerTune_Table;
typedef struct _ATOM_Fiji_PowerTune_Table {
	UCHAR  ucRevId;
	USHORT usTDP;
	USHORT usConfigurableTDP;
	USHORT usTDC;
	USHORT usBatteryPowerLimit;
	USHORT usSmallPowerLimit;
	USHORT usLowCACLeakage;
	USHORT usHighCACLeakage;
	USHORT usMaximumPowerDeliveryLimit;
	USHORT usTjMax;   
	USHORT usPowerTuneDataSetID;
	USHORT usEDCLimit;
	USHORT usSoftwareShutdownTemp;
	USHORT usClockStretchAmount;
	USHORT usTemperatureLimitHotspot;   
	USHORT usTemperatureLimitLiquid1;
	USHORT usTemperatureLimitLiquid2;
	USHORT usTemperatureLimitVrVddc;
	USHORT usTemperatureLimitVrMvdd;
	USHORT usTemperatureLimitPlx;
	UCHAR  ucLiquid1_I2C_address;   
	UCHAR  ucLiquid2_I2C_address;
	UCHAR  ucLiquid_I2C_Line;
	UCHAR  ucVr_I2C_address;	 
	UCHAR  ucVr_I2C_Line;
	UCHAR  ucPlx_I2C_address;   
	UCHAR  ucPlx_I2C_Line;
	USHORT usReserved;
} ATOM_Fiji_PowerTune_Table;
typedef struct _ATOM_Polaris_PowerTune_Table {
    UCHAR  ucRevId;
    USHORT usTDP;
    USHORT usConfigurableTDP;
    USHORT usTDC;
    USHORT usBatteryPowerLimit;
    USHORT usSmallPowerLimit;
    USHORT usLowCACLeakage;
    USHORT usHighCACLeakage;
    USHORT usMaximumPowerDeliveryLimit;
    USHORT usTjMax;   
    USHORT usPowerTuneDataSetID;
    USHORT usEDCLimit;
    USHORT usSoftwareShutdownTemp;
    USHORT usClockStretchAmount;
    USHORT usTemperatureLimitHotspot;   
    USHORT usTemperatureLimitLiquid1;
    USHORT usTemperatureLimitLiquid2;
    USHORT usTemperatureLimitVrVddc;
    USHORT usTemperatureLimitVrMvdd;
    USHORT usTemperatureLimitPlx;
    UCHAR  ucLiquid1_I2C_address;   
    UCHAR  ucLiquid2_I2C_address;
    UCHAR  ucLiquid_I2C_Line;
    UCHAR  ucVr_I2C_address;   
    UCHAR  ucVr_I2C_Line;
    UCHAR  ucPlx_I2C_address;   
    UCHAR  ucPlx_I2C_Line;
    USHORT usBoostPowerLimit;
    UCHAR  ucCKS_LDO_REFSEL;
    UCHAR  ucHotSpotOnly;
    UCHAR  ucReserve;
    USHORT usReserve;
} ATOM_Polaris_PowerTune_Table;
#define ATOM_PPM_A_A    1
#define ATOM_PPM_A_I    2
typedef struct _ATOM_Tonga_PPM_Table {
	UCHAR   ucRevId;
	UCHAR   ucPpmDesign;		   
	USHORT  usCpuCoreNumber;
	ULONG  ulPlatformTDP;
	ULONG  ulSmallACPlatformTDP;
	ULONG  ulPlatformTDC;
	ULONG  ulSmallACPlatformTDC;
	ULONG  ulApuTDP;
	ULONG  ulDGpuTDP;
	ULONG  ulDGpuUlvPower;
	ULONG  ulTjmax;
} ATOM_Tonga_PPM_Table;
typedef struct _ATOM_Tonga_Hard_Limit_Record {
	ULONG  ulSCLKLimit;
	ULONG  ulMCLKLimit;
	USHORT  usVddcLimit;
	USHORT  usVddciLimit;
	USHORT  usVddgfxLimit;
} ATOM_Tonga_Hard_Limit_Record;
typedef struct _ATOM_Tonga_Hard_Limit_Table {
	UCHAR ucRevId;
	UCHAR ucNumEntries;
	ATOM_Tonga_Hard_Limit_Record entries[1];
} ATOM_Tonga_Hard_Limit_Table;
typedef struct _ATOM_Tonga_GPIO_Table {
	UCHAR  ucRevId;
	UCHAR  ucVRHotTriggeredSclkDpmIndex;		 
	UCHAR  ucReserve[5];
} ATOM_Tonga_GPIO_Table;
typedef struct _PPTable_Generic_SubTable_Header {
	UCHAR  ucRevId;
} PPTable_Generic_SubTable_Header;
#pragma pack(pop)
#endif
