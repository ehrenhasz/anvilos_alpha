 
#ifndef SMU_11_0_7_PPTABLE_H
#define SMU_11_0_7_PPTABLE_H

#pragma pack(push, 1)

#define SMU_11_0_7_TABLE_FORMAT_REVISION                  15


#define SMU_11_0_7_PP_PLATFORM_CAP_POWERPLAY              0x1            
#define SMU_11_0_7_PP_PLATFORM_CAP_SBIOSPOWERSOURCE       0x2            
#define SMU_11_0_7_PP_PLATFORM_CAP_HARDWAREDC             0x4            
#define SMU_11_0_7_PP_PLATFORM_CAP_BACO                   0x8            
#define SMU_11_0_7_PP_PLATFORM_CAP_MACO                   0x10           
#define SMU_11_0_7_PP_PLATFORM_CAP_SHADOWPSTATE           0x20           


#define SMU_11_0_7_PP_THERMALCONTROLLER_NONE              0
#define SMU_11_0_7_PP_THERMALCONTROLLER_SIENNA_CICHLID    28

#define SMU_11_0_7_PP_OVERDRIVE_VERSION                   0x81           
#define SMU_11_0_7_PP_POWERSAVINGCLOCK_VERSION            0x01           

enum SMU_11_0_7_ODFEATURE_CAP {
    SMU_11_0_7_ODCAP_GFXCLK_LIMITS = 0, 
    SMU_11_0_7_ODCAP_GFXCLK_CURVE,    
    SMU_11_0_7_ODCAP_UCLK_LIMITS,           
    SMU_11_0_7_ODCAP_POWER_LIMIT,        
    SMU_11_0_7_ODCAP_FAN_ACOUSTIC_LIMIT,   
    SMU_11_0_7_ODCAP_FAN_SPEED_MIN,       
    SMU_11_0_7_ODCAP_TEMPERATURE_FAN,     
    SMU_11_0_7_ODCAP_TEMPERATURE_SYSTEM,  
    SMU_11_0_7_ODCAP_MEMORY_TIMING_TUNE,  
    SMU_11_0_7_ODCAP_FAN_ZERO_RPM_CONTROL, 
    SMU_11_0_7_ODCAP_AUTO_UV_ENGINE,   
    SMU_11_0_7_ODCAP_AUTO_OC_ENGINE,     
    SMU_11_0_7_ODCAP_AUTO_OC_MEMORY,     
    SMU_11_0_7_ODCAP_FAN_CURVE,
    SMU_11_0_ODCAP_AUTO_FAN_ACOUSTIC_LIMIT,
    SMU_11_0_7_ODCAP_POWER_MODE,          
    SMU_11_0_7_ODCAP_COUNT,             
};

enum SMU_11_0_7_ODFEATURE_ID {
    SMU_11_0_7_ODFEATURE_GFXCLK_LIMITS         = 1 << SMU_11_0_7_ODCAP_GFXCLK_LIMITS,            
    SMU_11_0_7_ODFEATURE_GFXCLK_CURVE          = 1 << SMU_11_0_7_ODCAP_GFXCLK_CURVE,             
    SMU_11_0_7_ODFEATURE_UCLK_LIMITS           = 1 << SMU_11_0_7_ODCAP_UCLK_LIMITS,              
    SMU_11_0_7_ODFEATURE_POWER_LIMIT           = 1 << SMU_11_0_7_ODCAP_POWER_LIMIT,              
    SMU_11_0_7_ODFEATURE_FAN_ACOUSTIC_LIMIT    = 1 << SMU_11_0_7_ODCAP_FAN_ACOUSTIC_LIMIT,       
    SMU_11_0_7_ODFEATURE_FAN_SPEED_MIN         = 1 << SMU_11_0_7_ODCAP_FAN_SPEED_MIN,            
    SMU_11_0_7_ODFEATURE_TEMPERATURE_FAN       = 1 << SMU_11_0_7_ODCAP_TEMPERATURE_FAN,          
    SMU_11_0_7_ODFEATURE_TEMPERATURE_SYSTEM    = 1 << SMU_11_0_7_ODCAP_TEMPERATURE_SYSTEM,       
    SMU_11_0_7_ODFEATURE_MEMORY_TIMING_TUNE    = 1 << SMU_11_0_7_ODCAP_MEMORY_TIMING_TUNE,       
    SMU_11_0_7_ODFEATURE_FAN_ZERO_RPM_CONTROL  = 1 << SMU_11_0_7_ODCAP_FAN_ZERO_RPM_CONTROL,     
    SMU_11_0_7_ODFEATURE_AUTO_UV_ENGINE        = 1 << SMU_11_0_7_ODCAP_AUTO_UV_ENGINE,           
    SMU_11_0_7_ODFEATURE_AUTO_OC_ENGINE        = 1 << SMU_11_0_7_ODCAP_AUTO_OC_ENGINE,           
    SMU_11_0_7_ODFEATURE_AUTO_OC_MEMORY        = 1 << SMU_11_0_7_ODCAP_AUTO_OC_MEMORY,           
    SMU_11_0_7_ODFEATURE_FAN_CURVE             = 1 << SMU_11_0_7_ODCAP_FAN_CURVE,                
    SMU_11_0_ODFEATURE_AUTO_FAN_ACOUSTIC_LIMIT = 1 << SMU_11_0_ODCAP_AUTO_FAN_ACOUSTIC_LIMIT,  
    SMU_11_0_7_ODFEATURE_POWER_MODE            = 1 << SMU_11_0_7_ODCAP_POWER_MODE,               
    SMU_11_0_7_ODFEATURE_COUNT                 = 16,
};

#define SMU_11_0_7_MAX_ODFEATURE    32          

enum SMU_11_0_7_ODSETTING_ID {
    SMU_11_0_7_ODSETTING_GFXCLKFMAX = 0,
    SMU_11_0_7_ODSETTING_GFXCLKFMIN,
    SMU_11_0_7_ODSETTING_CUSTOM_GFX_VF_CURVE_A,
    SMU_11_0_7_ODSETTING_CUSTOM_GFX_VF_CURVE_B,
    SMU_11_0_7_ODSETTING_CUSTOM_GFX_VF_CURVE_C,
    SMU_11_0_7_ODSETTING_CUSTOM_CURVE_VFT_FMIN,
    SMU_11_0_7_ODSETTING_UCLKFMIN,
    SMU_11_0_7_ODSETTING_UCLKFMAX,
    SMU_11_0_7_ODSETTING_POWERPERCENTAGE,
    SMU_11_0_7_ODSETTING_FANRPMMIN,
    SMU_11_0_7_ODSETTING_FANRPMACOUSTICLIMIT,
    SMU_11_0_7_ODSETTING_FANTARGETTEMPERATURE,
    SMU_11_0_7_ODSETTING_OPERATINGTEMPMAX,
    SMU_11_0_7_ODSETTING_ACTIMING,
    SMU_11_0_7_ODSETTING_FAN_ZERO_RPM_CONTROL,
    SMU_11_0_7_ODSETTING_AUTOUVENGINE,
    SMU_11_0_7_ODSETTING_AUTOOCENGINE,
    SMU_11_0_7_ODSETTING_AUTOOCMEMORY,
    SMU_11_0_7_ODSETTING_FAN_CURVE_TEMPERATURE_1,
    SMU_11_0_7_ODSETTING_FAN_CURVE_SPEED_1,
    SMU_11_0_7_ODSETTING_FAN_CURVE_TEMPERATURE_2,
    SMU_11_0_7_ODSETTING_FAN_CURVE_SPEED_2,
    SMU_11_0_7_ODSETTING_FAN_CURVE_TEMPERATURE_3,
    SMU_11_0_7_ODSETTING_FAN_CURVE_SPEED_3,
    SMU_11_0_7_ODSETTING_FAN_CURVE_TEMPERATURE_4,
    SMU_11_0_7_ODSETTING_FAN_CURVE_SPEED_4,
    SMU_11_0_7_ODSETTING_FAN_CURVE_TEMPERATURE_5,
    SMU_11_0_7_ODSETTING_FAN_CURVE_SPEED_5,
    SMU_11_0_7_ODSETTING_AUTO_FAN_ACOUSTIC_LIMIT,
    SMU_11_0_7_ODSETTING_POWER_MODE,
    SMU_11_0_7_ODSETTING_COUNT,
};
#define SMU_11_0_7_MAX_ODSETTING    64          

enum SMU_11_0_7_PWRMODE_SETTING {
    SMU_11_0_7_PMSETTING_POWER_LIMIT_QUIET = 0,
    SMU_11_0_7_PMSETTING_POWER_LIMIT_BALANCE,
    SMU_11_0_7_PMSETTING_POWER_LIMIT_TURBO,
    SMU_11_0_7_PMSETTING_POWER_LIMIT_RAGE,
    SMU_11_0_7_PMSETTING_ACOUSTIC_TEMP_QUIET,
    SMU_11_0_7_PMSETTING_ACOUSTIC_TEMP_BALANCE,
    SMU_11_0_7_PMSETTING_ACOUSTIC_TEMP_TURBO,
    SMU_11_0_7_PMSETTING_ACOUSTIC_TEMP_RAGE,
};
#define SMU_11_0_7_MAX_PMSETTING      32        

struct smu_11_0_7_overdrive_table
{
    uint8_t  revision;                                        
    uint8_t  reserve[3];                                      
    uint32_t feature_count;                                   
    uint32_t setting_count;                                   
    uint8_t  cap[SMU_11_0_7_MAX_ODFEATURE];                   
    uint32_t max[SMU_11_0_7_MAX_ODSETTING];                   
    uint32_t min[SMU_11_0_7_MAX_ODSETTING];                   
    int16_t  pm_setting[SMU_11_0_7_MAX_PMSETTING];            
};

enum SMU_11_0_7_PPCLOCK_ID {
    SMU_11_0_7_PPCLOCK_GFXCLK = 0,
    SMU_11_0_7_PPCLOCK_SOCCLK,
    SMU_11_0_7_PPCLOCK_UCLK,
    SMU_11_0_7_PPCLOCK_FCLK,
    SMU_11_0_7_PPCLOCK_DCLK_0,
    SMU_11_0_7_PPCLOCK_VCLK_0,
    SMU_11_0_7_PPCLOCK_DCLK_1,
    SMU_11_0_7_PPCLOCK_VCLK_1,
    SMU_11_0_7_PPCLOCK_DCEFCLK,
    SMU_11_0_7_PPCLOCK_DISPCLK,
    SMU_11_0_7_PPCLOCK_PIXCLK,
    SMU_11_0_7_PPCLOCK_PHYCLK,
    SMU_11_0_7_PPCLOCK_DTBCLK,
    SMU_11_0_7_PPCLOCK_COUNT,
};
#define SMU_11_0_7_MAX_PPCLOCK      16          

struct smu_11_0_7_power_saving_clock_table
{
    uint8_t  revision;                                        
    uint8_t  reserve[3];                                      
    uint32_t count;                                           
    uint32_t max[SMU_11_0_7_MAX_PPCLOCK];                       
    uint32_t min[SMU_11_0_7_MAX_PPCLOCK];                       
};

struct smu_11_0_7_powerplay_table
{
      struct atom_common_table_header header;       
      uint8_t  table_revision;                      
      uint16_t table_size;                          
      uint32_t golden_pp_id;                        
      uint32_t golden_revision;                     
      uint16_t format_id;                           
      uint32_t platform_caps;                       
                                                    
      uint8_t  thermal_controller_type;             

      uint16_t small_power_limit1;
      uint16_t small_power_limit2;
      uint16_t boost_power_limit;                   
      uint16_t software_shutdown_temp;

      uint16_t reserve[8];                          

      struct smu_11_0_7_power_saving_clock_table      power_saving_clock;
      struct smu_11_0_7_overdrive_table               overdrive_table;

      PPTable_t smc_pptable;                        
};

#pragma pack(pop)

#endif
