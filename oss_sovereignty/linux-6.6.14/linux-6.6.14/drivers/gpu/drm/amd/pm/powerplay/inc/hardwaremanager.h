#ifndef _HARDWARE_MANAGER_H_
#define _HARDWARE_MANAGER_H_
struct pp_hwmgr;
struct pp_hw_power_state;
struct pp_power_state;
enum amd_dpm_forced_level;
struct PP_TemperatureRange;
struct phm_fan_speed_info {
	uint32_t min_percent;
	uint32_t max_percent;
	uint32_t min_rpm;
	uint32_t max_rpm;
	bool supports_percent_read;
	bool supports_percent_write;
	bool supports_rpm_read;
	bool supports_rpm_write;
};
enum PHM_AutoThrottleSource {
    PHM_AutoThrottleSource_Thermal,
    PHM_AutoThrottleSource_External
};
typedef enum PHM_AutoThrottleSource PHM_AutoThrottleSource;
enum phm_platform_caps {
	PHM_PlatformCaps_AtomBiosPpV1 = 0,
	PHM_PlatformCaps_PowerPlaySupport,
	PHM_PlatformCaps_ACOverdriveSupport,
	PHM_PlatformCaps_BacklightSupport,
	PHM_PlatformCaps_ThermalController,
	PHM_PlatformCaps_BiosPowerSourceControl,
	PHM_PlatformCaps_DisableVoltageTransition,
	PHM_PlatformCaps_DisableEngineTransition,
	PHM_PlatformCaps_DisableMemoryTransition,
	PHM_PlatformCaps_DynamicPowerManagement,
	PHM_PlatformCaps_EnableASPML0s,
	PHM_PlatformCaps_EnableASPML1,
	PHM_PlatformCaps_OD5inACSupport,
	PHM_PlatformCaps_OD5inDCSupport,
	PHM_PlatformCaps_SoftStateOD5,
	PHM_PlatformCaps_NoOD5Support,
	PHM_PlatformCaps_ContinuousHardwarePerformanceRange,
	PHM_PlatformCaps_ActivityReporting,
	PHM_PlatformCaps_EnableBackbias,
	PHM_PlatformCaps_OverdriveDisabledByPowerBudget,
	PHM_PlatformCaps_ShowPowerBudgetWarning,
	PHM_PlatformCaps_PowerBudgetWaiverAvailable,
	PHM_PlatformCaps_GFXClockGatingSupport,
	PHM_PlatformCaps_MMClockGatingSupport,
	PHM_PlatformCaps_AutomaticDCTransition,
	PHM_PlatformCaps_GeminiPrimary,
	PHM_PlatformCaps_MemorySpreadSpectrumSupport,
	PHM_PlatformCaps_EngineSpreadSpectrumSupport,
	PHM_PlatformCaps_StepVddc,
	PHM_PlatformCaps_DynamicPCIEGen2Support,
	PHM_PlatformCaps_SMC,
	PHM_PlatformCaps_FaultyInternalThermalReading,           
	PHM_PlatformCaps_EnableVoltageControl,                   
	PHM_PlatformCaps_EnableSideportControl,                  
	PHM_PlatformCaps_VideoPlaybackEEUNotification,           
	PHM_PlatformCaps_TurnOffPll_ASPML1,                      
	PHM_PlatformCaps_EnableHTLinkControl,                    
	PHM_PlatformCaps_PerformanceStateOnly,                   
	PHM_PlatformCaps_ExclusiveModeAlwaysHigh,                
	PHM_PlatformCaps_DisableMGClockGating,                   
	PHM_PlatformCaps_DisableMGCGTSSM,                        
	PHM_PlatformCaps_UVDAlwaysHigh,                          
	PHM_PlatformCaps_DisablePowerGating,                     
	PHM_PlatformCaps_CustomThermalPolicy,                    
	PHM_PlatformCaps_StayInBootState,                        
	PHM_PlatformCaps_SMCAllowSeparateSWThermalState,         
	PHM_PlatformCaps_MultiUVDStateSupport,                   
	PHM_PlatformCaps_EnableSCLKDeepSleepForUVD,              
	PHM_PlatformCaps_EnableMCUHTLinkControl,                 
	PHM_PlatformCaps_ABM,                                    
	PHM_PlatformCaps_KongThermalPolicy,                      
	PHM_PlatformCaps_SwitchVDDNB,                            
	PHM_PlatformCaps_ULPS,                                   
	PHM_PlatformCaps_NativeULPS,                             
	PHM_PlatformCaps_EnableMVDDControl,                      
	PHM_PlatformCaps_ControlVDDCI,                           
	PHM_PlatformCaps_DisableDCODT,                           
	PHM_PlatformCaps_DynamicACTiming,                        
	PHM_PlatformCaps_EnableThermalIntByGPIO,                 
	PHM_PlatformCaps_BootStateOnAlert,                       
	PHM_PlatformCaps_DontWaitForVBlankOnAlert,               
	PHM_PlatformCaps_Force3DClockSupport,                    
	PHM_PlatformCaps_MicrocodeFanControl,                    
	PHM_PlatformCaps_AdjustUVDPriorityForSP,
	PHM_PlatformCaps_DisableLightSleep,                      
	PHM_PlatformCaps_DisableMCLS,                            
	PHM_PlatformCaps_RegulatorHot,                           
	PHM_PlatformCaps_BACO,                                   
	PHM_PlatformCaps_DisableDPM,                             
	PHM_PlatformCaps_DynamicM3Arbiter,                       
	PHM_PlatformCaps_SclkDeepSleep,                          
	PHM_PlatformCaps_DynamicPatchPowerState,                 
	PHM_PlatformCaps_ThermalAutoThrottling,                  
	PHM_PlatformCaps_SumoThermalPolicy,                      
	PHM_PlatformCaps_PCIEPerformanceRequest,                 
	PHM_PlatformCaps_BLControlledByGPU,                      
	PHM_PlatformCaps_PowerContainment,                       
	PHM_PlatformCaps_SQRamping,                              
	PHM_PlatformCaps_CAC,                                    
	PHM_PlatformCaps_NIChipsets,                             
	PHM_PlatformCaps_TrinityChipsets,                        
	PHM_PlatformCaps_EvergreenChipsets,                      
	PHM_PlatformCaps_PowerControl,                           
	PHM_PlatformCaps_DisableLSClockGating,                   
	PHM_PlatformCaps_BoostState,                             
	PHM_PlatformCaps_UserMaxClockForMultiDisplays,           
	PHM_PlatformCaps_RegWriteDelay,                          
	PHM_PlatformCaps_NonABMSupportInPPLib,                   
	PHM_PlatformCaps_GFXDynamicMGPowerGating,                
	PHM_PlatformCaps_DisableSMUUVDHandshake,                 
	PHM_PlatformCaps_DTE,                                    
	PHM_PlatformCaps_W5100Specifc_SmuSkipMsgDTE,             
	PHM_PlatformCaps_UVDPowerGating,                         
	PHM_PlatformCaps_UVDDynamicPowerGating,                  
	PHM_PlatformCaps_VCEPowerGating,                         
	PHM_PlatformCaps_SamuPowerGating,                        
	PHM_PlatformCaps_UVDDPM,                                 
	PHM_PlatformCaps_VCEDPM,                                 
	PHM_PlatformCaps_SamuDPM,                                
	PHM_PlatformCaps_AcpDPM,                                 
	PHM_PlatformCaps_SclkDeepSleepAboveLow,                  
	PHM_PlatformCaps_DynamicUVDState,                        
	PHM_PlatformCaps_WantSAMClkWithDummyBackEnd,             
	PHM_PlatformCaps_WantUVDClkWithDummyBackEnd,             
	PHM_PlatformCaps_WantVCEClkWithDummyBackEnd,             
	PHM_PlatformCaps_WantACPClkWithDummyBackEnd,             
	PHM_PlatformCaps_OD6inACSupport,                         
	PHM_PlatformCaps_OD6inDCSupport,                         
	PHM_PlatformCaps_EnablePlatformPowerManagement,          
	PHM_PlatformCaps_SurpriseRemoval,                        
	PHM_PlatformCaps_NewCACVoltage,                          
	PHM_PlatformCaps_DiDtSupport,                            
	PHM_PlatformCaps_DBRamping,                              
	PHM_PlatformCaps_TDRamping,                              
	PHM_PlatformCaps_TCPRamping,                             
	PHM_PlatformCaps_DBRRamping,                             
	PHM_PlatformCaps_DiDtEDCEnable,                          
	PHM_PlatformCaps_GCEDC,                                  
	PHM_PlatformCaps_PSM,                                    
	PHM_PlatformCaps_EnableSMU7ThermalManagement,            
	PHM_PlatformCaps_FPS,                                    
	PHM_PlatformCaps_ACP,                                    
	PHM_PlatformCaps_SclkThrottleLowNotification,            
	PHM_PlatformCaps_XDMAEnabled,                            
	PHM_PlatformCaps_UseDummyBackEnd,                        
	PHM_PlatformCaps_EnableDFSBypass,                        
	PHM_PlatformCaps_VddNBDirectRequest,
	PHM_PlatformCaps_PauseMMSessions,
	PHM_PlatformCaps_UnTabledHardwareInterface,              
	PHM_PlatformCaps_SMU7,                                   
	PHM_PlatformCaps_RevertGPIO5Polarity,                    
	PHM_PlatformCaps_Thermal2GPIO17,                         
	PHM_PlatformCaps_ThermalOutGPIO,                         
	PHM_PlatformCaps_DisableMclkSwitchingForFrameLock,       
	PHM_PlatformCaps_ForceMclkHigh,                          
	PHM_PlatformCaps_VRHotGPIOConfigurable,                  
	PHM_PlatformCaps_TempInversion,                          
	PHM_PlatformCaps_IOIC3,
	PHM_PlatformCaps_ConnectedStandby,
	PHM_PlatformCaps_EVV,
	PHM_PlatformCaps_EnableLongIdleBACOSupport,
	PHM_PlatformCaps_CombinePCCWithThermalSignal,
	PHM_PlatformCaps_DisableUsingActualTemperatureForPowerCalc,
	PHM_PlatformCaps_StablePState,
	PHM_PlatformCaps_OD6PlusinACSupport,
	PHM_PlatformCaps_OD6PlusinDCSupport,
	PHM_PlatformCaps_ODThermalLimitUnlock,
	PHM_PlatformCaps_ReducePowerLimit,
	PHM_PlatformCaps_ODFuzzyFanControlSupport,
	PHM_PlatformCaps_GeminiRegulatorFanControlSupport,
	PHM_PlatformCaps_ControlVDDGFX,
	PHM_PlatformCaps_BBBSupported,
	PHM_PlatformCaps_DisableVoltageIsland,
	PHM_PlatformCaps_FanSpeedInTableIsRPM,
	PHM_PlatformCaps_GFXClockGatingManagedInCAIL,
	PHM_PlatformCaps_IcelandULPSSWWorkAround,
	PHM_PlatformCaps_FPSEnhancement,
	PHM_PlatformCaps_LoadPostProductionFirmware,
	PHM_PlatformCaps_VpuRecoveryInProgress,
	PHM_PlatformCaps_Falcon_QuickTransition,
	PHM_PlatformCaps_AVFS,
	PHM_PlatformCaps_ClockStretcher,
	PHM_PlatformCaps_TablelessHardwareInterface,
	PHM_PlatformCaps_EnableDriverEVV,
	PHM_PlatformCaps_SPLLShutdownSupport,
	PHM_PlatformCaps_VirtualBatteryState,
	PHM_PlatformCaps_IgnoreForceHighClockRequestsInAPUs,
	PHM_PlatformCaps_DisableMclkSwitchForVR,
	PHM_PlatformCaps_SMU8,
	PHM_PlatformCaps_VRHotPolarityHigh,
	PHM_PlatformCaps_IPS_UlpsExclusive,
	PHM_PlatformCaps_SMCtoPPLIBAcdcGpioScheme,
	PHM_PlatformCaps_GeminiAsymmetricPower,
	PHM_PlatformCaps_OCLPowerOptimization,
	PHM_PlatformCaps_MaxPCIEBandWidth,
	PHM_PlatformCaps_PerfPerWattOptimizationSupport,
	PHM_PlatformCaps_UVDClientMCTuning,
	PHM_PlatformCaps_ODNinACSupport,
	PHM_PlatformCaps_ODNinDCSupport,
	PHM_PlatformCaps_OD8inACSupport,
	PHM_PlatformCaps_OD8inDCSupport,
	PHM_PlatformCaps_UMDPState,
	PHM_PlatformCaps_AutoWattmanSupport,
	PHM_PlatformCaps_AutoWattmanEnable_CCCState,
	PHM_PlatformCaps_FreeSyncActive,
	PHM_PlatformCaps_EnableShadowPstate,
	PHM_PlatformCaps_customThermalManagement,
	PHM_PlatformCaps_staticFanControl,
	PHM_PlatformCaps_Virtual_System,
	PHM_PlatformCaps_LowestUclkReservedForUlv,
	PHM_PlatformCaps_EnableBoostState,
	PHM_PlatformCaps_AVFSSupport,
	PHM_PlatformCaps_ThermalPolicyDelay,
	PHM_PlatformCaps_CustomFanControlSupport,
	PHM_PlatformCaps_BAMACO,
	PHM_PlatformCaps_Max
};
#define PHM_MAX_NUM_CAPS_BITS_PER_FIELD (sizeof(uint32_t)*8)
#define PHM_MAX_NUM_CAPS_ULONG_ENTRIES \
	((PHM_PlatformCaps_Max + ((PHM_MAX_NUM_CAPS_BITS_PER_FIELD) - 1)) / (PHM_MAX_NUM_CAPS_BITS_PER_FIELD))
struct pp_hw_descriptor {
	uint32_t hw_caps[PHM_MAX_NUM_CAPS_ULONG_ENTRIES];
};
enum PHM_PerformanceLevelDesignation {
	PHM_PerformanceLevelDesignation_Activity,
	PHM_PerformanceLevelDesignation_PowerContainment
};
typedef enum PHM_PerformanceLevelDesignation PHM_PerformanceLevelDesignation;
struct PHM_PerformanceLevel {
    uint32_t    coreClock;
    uint32_t    memory_clock;
    uint32_t  vddc;
    uint32_t  vddci;
    uint32_t    nonLocalMemoryFreq;
    uint32_t nonLocalMemoryWidth;
};
typedef struct PHM_PerformanceLevel PHM_PerformanceLevel;
static inline void phm_cap_set(uint32_t *caps,
			enum phm_platform_caps c)
{
	caps[c / PHM_MAX_NUM_CAPS_BITS_PER_FIELD] |= (1UL <<
			     (c & (PHM_MAX_NUM_CAPS_BITS_PER_FIELD - 1)));
}
static inline void phm_cap_unset(uint32_t *caps,
			enum phm_platform_caps c)
{
	caps[c / PHM_MAX_NUM_CAPS_BITS_PER_FIELD] &= ~(1UL << (c & (PHM_MAX_NUM_CAPS_BITS_PER_FIELD - 1)));
}
static inline bool phm_cap_enabled(const uint32_t *caps, enum phm_platform_caps c)
{
	return (0 != (caps[c / PHM_MAX_NUM_CAPS_BITS_PER_FIELD] &
		  (1UL << (c & (PHM_MAX_NUM_CAPS_BITS_PER_FIELD - 1)))));
}
#define PP_CAP(c) phm_cap_enabled(hwmgr->platform_descriptor.platformCaps, (c))
#define PP_PCIEGenInvalid  0xffff
enum PP_PCIEGen {
    PP_PCIEGen1 = 0,                 
    PP_PCIEGen2,                     
    PP_PCIEGen3                      
};
typedef enum PP_PCIEGen PP_PCIEGen;
#define PP_Min_PCIEGen     PP_PCIEGen1
#define PP_Max_PCIEGen     PP_PCIEGen3
#define PP_Min_PCIELane    1
#define PP_Max_PCIELane    16
enum phm_clock_Type {
	PHM_DispClock = 1,
	PHM_SClock,
	PHM_MemClock
};
#define MAX_NUM_CLOCKS 16
struct PP_Clocks {
	uint32_t engineClock;
	uint32_t memoryClock;
	uint32_t BusBandwidth;
	uint32_t engineClockInSR;
	uint32_t dcefClock;
	uint32_t dcefClockInSR;
};
struct pp_clock_info {
	uint32_t min_mem_clk;
	uint32_t max_mem_clk;
	uint32_t min_eng_clk;
	uint32_t max_eng_clk;
	uint32_t min_bus_bandwidth;
	uint32_t max_bus_bandwidth;
};
struct phm_platform_descriptor {
	uint32_t platformCaps[PHM_MAX_NUM_CAPS_ULONG_ENTRIES];
	uint32_t vbiosInterruptId;
	struct PP_Clocks overdriveLimit;
	struct PP_Clocks clockStep;
	uint32_t hardwareActivityPerformanceLevels;
	uint32_t minimumClocksReductionPercentage;
	uint32_t minOverdriveVDDC;
	uint32_t maxOverdriveVDDC;
	uint32_t overdriveVDDCStep;
	uint32_t hardwarePerformanceLevels;
	uint16_t powerBudget;
	uint32_t TDPLimit;
	uint32_t nearTDPLimit;
	uint32_t nearTDPLimitAdjusted;
	uint32_t SQRampingThreshold;
	uint32_t CACLeakage;
	uint16_t TDPODLimit;
	uint32_t TDPAdjustment;
	bool TDPAdjustmentPolarity;
	uint16_t LoadLineSlope;
	uint32_t  VidMinLimit;
	uint32_t  VidMaxLimit;
	uint32_t  VidStep;
	uint32_t  VidAdjustment;
	bool VidAdjustmentPolarity;
};
struct phm_clocks {
	uint32_t num_of_entries;
	uint32_t clock[MAX_NUM_CLOCKS];
};
#define DPMTABLE_OD_UPDATE_SCLK     0x00000001
#define DPMTABLE_OD_UPDATE_MCLK     0x00000002
#define DPMTABLE_UPDATE_SCLK        0x00000004
#define DPMTABLE_UPDATE_MCLK        0x00000008
#define DPMTABLE_OD_UPDATE_VDDC     0x00000010
#define DPMTABLE_UPDATE_SOCCLK      0x00000020
struct phm_odn_performance_level {
	uint32_t clock;
	uint32_t vddc;
	bool enabled;
};
struct phm_odn_clock_levels {
	uint32_t size;
	uint32_t options;
	uint32_t flags;
	uint32_t num_of_pl;
	struct phm_odn_performance_level entries[8];
};
extern int phm_disable_clock_power_gatings(struct pp_hwmgr *hwmgr);
extern int phm_powerdown_uvd(struct pp_hwmgr *hwmgr);
extern int phm_setup_asic(struct pp_hwmgr *hwmgr);
extern int phm_enable_dynamic_state_management(struct pp_hwmgr *hwmgr);
extern int phm_disable_dynamic_state_management(struct pp_hwmgr *hwmgr);
extern int phm_set_power_state(struct pp_hwmgr *hwmgr,
		    const struct pp_hw_power_state *pcurrent_state,
		 const struct pp_hw_power_state *pnew_power_state);
extern int phm_apply_state_adjust_rules(struct pp_hwmgr *hwmgr,
				   struct pp_power_state *adjusted_ps,
			     const struct pp_power_state *current_ps);
extern int phm_apply_clock_adjust_rules(struct pp_hwmgr *hwmgr);
extern int phm_force_dpm_levels(struct pp_hwmgr *hwmgr, enum amd_dpm_forced_level level);
extern int phm_pre_display_configuration_changed(struct pp_hwmgr *hwmgr);
extern int phm_display_configuration_changed(struct pp_hwmgr *hwmgr);
extern int phm_notify_smc_display_config_after_ps_adjustment(struct pp_hwmgr *hwmgr);
extern int phm_register_irq_handlers(struct pp_hwmgr *hwmgr);
extern int phm_start_thermal_controller(struct pp_hwmgr *hwmgr);
extern int phm_stop_thermal_controller(struct pp_hwmgr *hwmgr);
extern bool phm_check_smc_update_required_for_display_configuration(struct pp_hwmgr *hwmgr);
extern int phm_check_states_equal(struct pp_hwmgr *hwmgr,
				 const struct pp_hw_power_state *pstate1,
				 const struct pp_hw_power_state *pstate2,
				 bool *equal);
extern int phm_store_dal_configuration_data(struct pp_hwmgr *hwmgr,
		const struct amd_pp_display_configuration *display_config);
extern int phm_get_dal_power_level(struct pp_hwmgr *hwmgr,
		struct amd_pp_simple_clock_info *info);
extern int phm_set_cpu_power_state(struct pp_hwmgr *hwmgr);
extern int phm_power_down_asic(struct pp_hwmgr *hwmgr);
extern int phm_get_performance_level(struct pp_hwmgr *hwmgr, const struct pp_hw_power_state *state,
				PHM_PerformanceLevelDesignation designation, uint32_t index,
				PHM_PerformanceLevel *level);
extern int phm_get_clock_info(struct pp_hwmgr *hwmgr, const struct pp_hw_power_state *state,
			struct pp_clock_info *pclock_info,
			PHM_PerformanceLevelDesignation designation);
extern int phm_get_current_shallow_sleep_clocks(struct pp_hwmgr *hwmgr, const struct pp_hw_power_state *state, struct pp_clock_info *clock_info);
extern int phm_get_clock_by_type(struct pp_hwmgr *hwmgr, enum amd_pp_clock_type type, struct amd_pp_clocks *clocks);
extern int phm_get_clock_by_type_with_latency(struct pp_hwmgr *hwmgr,
		enum amd_pp_clock_type type,
		struct pp_clock_levels_with_latency *clocks);
extern int phm_get_clock_by_type_with_voltage(struct pp_hwmgr *hwmgr,
		enum amd_pp_clock_type type,
		struct pp_clock_levels_with_voltage *clocks);
extern int phm_set_watermarks_for_clocks_ranges(struct pp_hwmgr *hwmgr,
						void *clock_ranges);
extern int phm_display_clock_voltage_request(struct pp_hwmgr *hwmgr,
		struct pp_display_clock_request *clock);
extern int phm_get_max_high_clocks(struct pp_hwmgr *hwmgr, struct amd_pp_simple_clock_info *clocks);
extern int phm_disable_smc_firmware_ctf(struct pp_hwmgr *hwmgr);
extern int phm_set_active_display_count(struct pp_hwmgr *hwmgr, uint32_t count);
#endif  
