#ifndef SMU10_DRIVER_IF_H
#define SMU10_DRIVER_IF_H
#define SMU10_DRIVER_IF_VERSION 0x6
#define NUM_DSPCLK_LEVELS 8
typedef struct {
	int32_t value;
	uint32_t numFractionalBits;
} FloatInIntFormat_t;
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
typedef struct {
	uint16_t MinClock;  
	uint16_t MaxClock;  
	uint16_t MinMclk;
	uint16_t MaxMclk;
	uint8_t  WmSetting;
	uint8_t  WmType;
	uint8_t  Padding[2];
} WatermarkRowGeneric_t;
#define NUM_WM_RANGES 4
typedef enum {
	WM_SOCCLK = 0,
	WM_DCFCLK,
	WM_COUNT,
} WM_CLOCK_e;
typedef struct {
	WatermarkRowGeneric_t WatermarkRow[WM_COUNT][NUM_WM_RANGES];
	uint32_t              MmHubPadding[7];
} Watermarks_t;
typedef enum {
	CUSTOM_DPM_SETTING_GFXCLK,
	CUSTOM_DPM_SETTING_CCLK,
	CUSTOM_DPM_SETTING_FCLK_CCX,
	CUSTOM_DPM_SETTING_FCLK_GFX,
	CUSTOM_DPM_SETTING_FCLK_STALLS,
	CUSTOM_DPM_SETTING_LCLK,
	CUSTOM_DPM_SETTING_COUNT,
} CUSTOM_DPM_SETTING_e;
typedef struct {
	uint8_t             ActiveHystLimit;
	uint8_t             IdleHystLimit;
	uint8_t             FPS;
	uint8_t             MinActiveFreqType;
	FloatInIntFormat_t  MinActiveFreq;
	FloatInIntFormat_t  PD_Data_limit;
	FloatInIntFormat_t  PD_Data_time_constant;
	FloatInIntFormat_t  PD_Data_error_coeff;
	FloatInIntFormat_t  PD_Data_error_rate_coeff;
} DpmActivityMonitorCoeffExt_t;
typedef struct {
	DpmActivityMonitorCoeffExt_t DpmActivityMonitorCoeff[CUSTOM_DPM_SETTING_COUNT];
} CustomDpmSettings_t;
#define NUM_SOCCLK_DPM_LEVELS  8
#define NUM_DCEFCLK_DPM_LEVELS 4
#define NUM_FCLK_DPM_LEVELS    4
#define NUM_MEMCLK_DPM_LEVELS  4
typedef struct {
	uint32_t  Freq;  
	uint32_t  Vol;   
} DpmClock_t;
typedef struct {
	DpmClock_t DcefClocks[NUM_DCEFCLK_DPM_LEVELS];
	DpmClock_t SocClocks[NUM_SOCCLK_DPM_LEVELS];
	DpmClock_t FClocks[NUM_FCLK_DPM_LEVELS];
	DpmClock_t MemClocks[NUM_MEMCLK_DPM_LEVELS];
} DpmClocks_t;
#endif
