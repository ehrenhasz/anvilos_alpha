 

#ifndef __AMD_SHARED_H__
#define __AMD_SHARED_H__

#include <drm/amd_asic_type.h>


#define AMD_MAX_USEC_TIMEOUT		1000000   

 
enum amd_chip_flags {
	AMD_ASIC_MASK = 0x0000ffffUL,
	AMD_FLAGS_MASK  = 0xffff0000UL,
	AMD_IS_MOBILITY = 0x00010000UL,
	AMD_IS_APU      = 0x00020000UL,
	AMD_IS_PX       = 0x00040000UL,
	AMD_EXP_HW_SUPPORT = 0x00080000UL,
};

enum amd_apu_flags {
	AMD_APU_IS_RAVEN = 0x00000001UL,
	AMD_APU_IS_RAVEN2 = 0x00000002UL,
	AMD_APU_IS_PICASSO = 0x00000004UL,
	AMD_APU_IS_RENOIR = 0x00000008UL,
	AMD_APU_IS_GREEN_SARDINE = 0x00000010UL,
	AMD_APU_IS_VANGOGH = 0x00000020UL,
	AMD_APU_IS_CYAN_SKILLFISH2 = 0x00000040UL,
};

 

 
enum amd_ip_block_type {
	AMD_IP_BLOCK_TYPE_COMMON,
	AMD_IP_BLOCK_TYPE_GMC,
	AMD_IP_BLOCK_TYPE_IH,
	AMD_IP_BLOCK_TYPE_SMC,
	AMD_IP_BLOCK_TYPE_PSP,
	AMD_IP_BLOCK_TYPE_DCE,
	AMD_IP_BLOCK_TYPE_GFX,
	AMD_IP_BLOCK_TYPE_SDMA,
	AMD_IP_BLOCK_TYPE_UVD,
	AMD_IP_BLOCK_TYPE_VCE,
	AMD_IP_BLOCK_TYPE_ACP,
	AMD_IP_BLOCK_TYPE_VCN,
	AMD_IP_BLOCK_TYPE_MES,
	AMD_IP_BLOCK_TYPE_JPEG,
	AMD_IP_BLOCK_TYPE_NUM,
};

enum amd_clockgating_state {
	AMD_CG_STATE_GATE = 0,
	AMD_CG_STATE_UNGATE,
};


enum amd_powergating_state {
	AMD_PG_STATE_GATE = 0,
	AMD_PG_STATE_UNGATE,
};


 
#define AMD_CG_SUPPORT_GFX_MGCG			(1ULL << 0)
#define AMD_CG_SUPPORT_GFX_MGLS			(1ULL << 1)
#define AMD_CG_SUPPORT_GFX_CGCG			(1ULL << 2)
#define AMD_CG_SUPPORT_GFX_CGLS			(1ULL << 3)
#define AMD_CG_SUPPORT_GFX_CGTS			(1ULL << 4)
#define AMD_CG_SUPPORT_GFX_CGTS_LS		(1ULL << 5)
#define AMD_CG_SUPPORT_GFX_CP_LS		(1ULL << 6)
#define AMD_CG_SUPPORT_GFX_RLC_LS		(1ULL << 7)
#define AMD_CG_SUPPORT_MC_LS			(1ULL << 8)
#define AMD_CG_SUPPORT_MC_MGCG			(1ULL << 9)
#define AMD_CG_SUPPORT_SDMA_LS			(1ULL << 10)
#define AMD_CG_SUPPORT_SDMA_MGCG		(1ULL << 11)
#define AMD_CG_SUPPORT_BIF_LS			(1ULL << 12)
#define AMD_CG_SUPPORT_UVD_MGCG			(1ULL << 13)
#define AMD_CG_SUPPORT_VCE_MGCG			(1ULL << 14)
#define AMD_CG_SUPPORT_HDP_LS			(1ULL << 15)
#define AMD_CG_SUPPORT_HDP_MGCG			(1ULL << 16)
#define AMD_CG_SUPPORT_ROM_MGCG			(1ULL << 17)
#define AMD_CG_SUPPORT_DRM_LS			(1ULL << 18)
#define AMD_CG_SUPPORT_BIF_MGCG			(1ULL << 19)
#define AMD_CG_SUPPORT_GFX_3D_CGCG		(1ULL << 20)
#define AMD_CG_SUPPORT_GFX_3D_CGLS		(1ULL << 21)
#define AMD_CG_SUPPORT_DRM_MGCG			(1ULL << 22)
#define AMD_CG_SUPPORT_DF_MGCG			(1ULL << 23)
#define AMD_CG_SUPPORT_VCN_MGCG			(1ULL << 24)
#define AMD_CG_SUPPORT_HDP_DS			(1ULL << 25)
#define AMD_CG_SUPPORT_HDP_SD			(1ULL << 26)
#define AMD_CG_SUPPORT_IH_CG			(1ULL << 27)
#define AMD_CG_SUPPORT_ATHUB_LS			(1ULL << 28)
#define AMD_CG_SUPPORT_ATHUB_MGCG		(1ULL << 29)
#define AMD_CG_SUPPORT_JPEG_MGCG		(1ULL << 30)
#define AMD_CG_SUPPORT_GFX_FGCG			(1ULL << 31)
#define AMD_CG_SUPPORT_REPEATER_FGCG		(1ULL << 32)
#define AMD_CG_SUPPORT_GFX_PERF_CLK		(1ULL << 33)
 
#define AMD_PG_SUPPORT_GFX_PG			(1 << 0)
#define AMD_PG_SUPPORT_GFX_SMG			(1 << 1)
#define AMD_PG_SUPPORT_GFX_DMG			(1 << 2)
#define AMD_PG_SUPPORT_UVD			(1 << 3)
#define AMD_PG_SUPPORT_VCE			(1 << 4)
#define AMD_PG_SUPPORT_CP			(1 << 5)
#define AMD_PG_SUPPORT_GDS			(1 << 6)
#define AMD_PG_SUPPORT_RLC_SMU_HS		(1 << 7)
#define AMD_PG_SUPPORT_SDMA			(1 << 8)
#define AMD_PG_SUPPORT_ACP			(1 << 9)
#define AMD_PG_SUPPORT_SAMU			(1 << 10)
#define AMD_PG_SUPPORT_GFX_QUICK_MG		(1 << 11)
#define AMD_PG_SUPPORT_GFX_PIPELINE		(1 << 12)
#define AMD_PG_SUPPORT_MMHUB			(1 << 13)
#define AMD_PG_SUPPORT_VCN			(1 << 14)
#define AMD_PG_SUPPORT_VCN_DPG			(1 << 15)
#define AMD_PG_SUPPORT_ATHUB			(1 << 16)
#define AMD_PG_SUPPORT_JPEG			(1 << 17)
#define AMD_PG_SUPPORT_IH_SRAM_PG		(1 << 18)

 
enum PP_FEATURE_MASK {
	PP_SCLK_DPM_MASK = 0x1,
	PP_MCLK_DPM_MASK = 0x2,
	PP_PCIE_DPM_MASK = 0x4,
	PP_SCLK_DEEP_SLEEP_MASK = 0x8,
	PP_POWER_CONTAINMENT_MASK = 0x10,
	PP_UVD_HANDSHAKE_MASK = 0x20,
	PP_SMC_VOLTAGE_CONTROL_MASK = 0x40,
	PP_VBI_TIME_SUPPORT_MASK = 0x80,
	PP_ULV_MASK = 0x100,
	PP_ENABLE_GFX_CG_THRU_SMU = 0x200,
	PP_CLOCK_STRETCH_MASK = 0x400,
	PP_OD_FUZZY_FAN_CONTROL_MASK = 0x800,
	PP_SOCCLK_DPM_MASK = 0x1000,
	PP_DCEFCLK_DPM_MASK = 0x2000,
	PP_OVERDRIVE_MASK = 0x4000,
	PP_GFXOFF_MASK = 0x8000,
	PP_ACG_MASK = 0x10000,
	PP_STUTTER_MODE = 0x20000,
	PP_AVFS_MASK = 0x40000,
	PP_GFX_DCS_MASK = 0x80000,
};

enum amd_harvest_ip_mask {
    AMD_HARVEST_IP_VCN_MASK = 0x1,
    AMD_HARVEST_IP_JPEG_MASK = 0x2,
    AMD_HARVEST_IP_DMU_MASK = 0x4,
};

enum DC_FEATURE_MASK {
	
	DC_FBC_MASK = (1 << 0), 
	DC_MULTI_MON_PP_MCLK_SWITCH_MASK = (1 << 1), 
	DC_DISABLE_FRACTIONAL_PWM_MASK = (1 << 2), 
	DC_PSR_MASK = (1 << 3), 
	DC_EDP_NO_POWER_SEQUENCING = (1 << 4), 
	DC_DISABLE_LTTPR_DP1_4A = (1 << 5), 
	DC_DISABLE_LTTPR_DP2_0 = (1 << 6), 
	DC_PSR_ALLOW_SMU_OPT = (1 << 7), 
	DC_PSR_ALLOW_MULTI_DISP_OPT = (1 << 8), 
	DC_REPLAY_MASK = (1 << 9), 
};

enum DC_DEBUG_MASK {
	DC_DISABLE_PIPE_SPLIT = 0x1,
	DC_DISABLE_STUTTER = 0x2,
	DC_DISABLE_DSC = 0x4,
	DC_DISABLE_CLOCK_GATING = 0x8,
	DC_DISABLE_PSR = 0x10,
	DC_FORCE_SUBVP_MCLK_SWITCH = 0x20,
	DC_DISABLE_MPO = 0x40,
	DC_DISABLE_REPLAY = 0x50,
	DC_ENABLE_DPIA_TRACE = 0x80,
};

enum amd_dpm_forced_level;

 
struct amd_ip_funcs {
	char *name;
	int (*early_init)(void *handle);
	int (*late_init)(void *handle);
	int (*sw_init)(void *handle);
	int (*sw_fini)(void *handle);
	int (*early_fini)(void *handle);
	int (*hw_init)(void *handle);
	int (*hw_fini)(void *handle);
	void (*late_fini)(void *handle);
	int (*suspend)(void *handle);
	int (*resume)(void *handle);
	bool (*is_idle)(void *handle);
	int (*wait_for_idle)(void *handle);
	bool (*check_soft_reset)(void *handle);
	int (*pre_soft_reset)(void *handle);
	int (*soft_reset)(void *handle);
	int (*post_soft_reset)(void *handle);
	int (*set_clockgating_state)(void *handle,
				     enum amd_clockgating_state state);
	int (*set_powergating_state)(void *handle,
				     enum amd_powergating_state state);
	void (*get_clockgating_state)(void *handle, u64 *flags);
};


#endif  
