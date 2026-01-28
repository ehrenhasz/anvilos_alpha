#ifndef __SMU11_DRIVER_IF_CYAN_SKILLFISH_H__
#define __SMU11_DRIVER_IF_CYAN_SKILLFISH_H__
#define MP1_DRIVER_IF_VERSION 0x8
#define TABLE_BIOS_IF            0  
#define TABLE_WATERMARKS         1  
#define TABLE_PMSTATUSLOG        3  
#define TABLE_DPMCLOCKS          4  
#define TABLE_MOMENTARY_PM       5  
#define TABLE_SMU_METRICS        6  
#define TABLE_COUNT              7
typedef struct SmuMetricsTable_t {
	uint16_t CoreFrequency[6];               
	uint32_t CorePower[6];                   
	uint16_t CoreTemperature[6];             
	uint16_t L3Frequency[2];                 
	uint16_t L3Temperature[2];               
	uint16_t C0Residency[6];                 
	uint16_t GfxclkFrequency;                
	uint16_t GfxTemperature;                 
	uint16_t SocclkFrequency;                
	uint16_t VclkFrequency;                  
	uint16_t DclkFrequency;                  
	uint16_t MemclkFrequency;                
	uint32_t Voltage[2];                     
	uint32_t Current[2];                     
	uint32_t Power[2];                       
	uint32_t CurrentSocketPower;             
	uint16_t SocTemperature;                 
	uint16_t EdgeTemperature;
	uint16_t ThrottlerStatus;
	uint16_t Spare;
} SmuMetricsTable_t;
typedef struct SmuMetrics_t {
	SmuMetricsTable_t Current;
	SmuMetricsTable_t Average;
	uint32_t SampleStartTime;
	uint32_t SampleStopTime;
	uint32_t Accnt;
} SmuMetrics_t;
#endif
