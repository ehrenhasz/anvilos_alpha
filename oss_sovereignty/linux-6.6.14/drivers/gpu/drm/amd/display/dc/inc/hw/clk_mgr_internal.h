 

#ifndef __DAL_CLK_MGR_INTERNAL_H__
#define __DAL_CLK_MGR_INTERNAL_H__

#include "clk_mgr.h"
#include "dc.h"

 
#include "resource.h"


 
enum dentist_base_divider_id {
	DENTIST_BASE_DID_1 = 0x08,
	DENTIST_BASE_DID_2 = 0x40,
	DENTIST_BASE_DID_3 = 0x60,
	DENTIST_BASE_DID_4 = 0x7e,
	DENTIST_MAX_DID = 0x7f
};

 
enum dentist_divider_range {
	DENTIST_DIVIDER_RANGE_1_START = 8,    
	DENTIST_DIVIDER_RANGE_1_STEP  = 1,    
	DENTIST_DIVIDER_RANGE_2_START = 64,   
	DENTIST_DIVIDER_RANGE_2_STEP  = 2,    
	DENTIST_DIVIDER_RANGE_3_START = 128,  
	DENTIST_DIVIDER_RANGE_3_STEP  = 4,    
	DENTIST_DIVIDER_RANGE_4_START = 248,  
	DENTIST_DIVIDER_RANGE_4_STEP  = 264,  
	DENTIST_DIVIDER_RANGE_SCALE_FACTOR = 4
};

 

 

#define TO_CLK_MGR_INTERNAL(clk_mgr)\
	container_of(clk_mgr, struct clk_mgr_internal, base)

#define CTX \
	clk_mgr->base.ctx

#define DC_LOGGER \
	clk_mgr->base.ctx->logger




#define CLK_BASE(inst) \
	CLK_BASE_INNER(inst)

#define CLK_SRI(reg_name, block, inst)\
	.reg_name = CLK_BASE(mm ## block ## _ ## inst ## _ ## reg_name ## _BASE_IDX) + \
					mm ## block ## _ ## inst ## _ ## reg_name

#define CLK_COMMON_REG_LIST_DCE_BASE() \
	.DPREFCLK_CNTL = mmDPREFCLK_CNTL, \
	.DENTIST_DISPCLK_CNTL = mmDENTIST_DISPCLK_CNTL

#if defined(CONFIG_DRM_AMD_DC_SI)
#define CLK_COMMON_REG_LIST_DCE60_BASE() \
	SR(DENTIST_DISPCLK_CNTL)
#endif

#define CLK_COMMON_REG_LIST_DCN_BASE() \
	SR(DENTIST_DISPCLK_CNTL)

#define VBIOS_SMU_MSG_BOX_REG_LIST_RV() \
	.MP1_SMN_C2PMSG_91 = mmMP1_SMN_C2PMSG_91, \
	.MP1_SMN_C2PMSG_83 = mmMP1_SMN_C2PMSG_83, \
	.MP1_SMN_C2PMSG_67 = mmMP1_SMN_C2PMSG_67

#define CLK_COMMON_REG_LIST_DCN_201() \
	SR(DENTIST_DISPCLK_CNTL), \
	CLK_SRI(CLK4_CLK_PLL_REQ, CLK4, 0), \
	CLK_SRI(CLK4_CLK2_CURRENT_CNT, CLK4, 0)

#define CLK_REG_LIST_NV10() \
	SR(DENTIST_DISPCLK_CNTL), \
	CLK_SRI(CLK3_CLK_PLL_REQ, CLK3, 0), \
	CLK_SRI(CLK3_CLK2_DFS_CNTL, CLK3, 0)

#define CLK_REG_LIST_DCN3()	  \
	CLK_COMMON_REG_LIST_DCN_BASE(), \
	CLK_SRI(CLK0_CLK_PLL_REQ,   CLK02, 0), \
	CLK_SRI(CLK0_CLK2_DFS_CNTL, CLK02, 0)

#define CLK_SF(reg_name, field_name, post_fix)\
	.field_name = reg_name ## __ ## field_name ## post_fix

#define CLK_COMMON_MASK_SH_LIST_DCE_COMMON_BASE(mask_sh) \
	CLK_SF(DPREFCLK_CNTL, DPREFCLK_SRC_SEL, mask_sh), \
	CLK_SF(DENTIST_DISPCLK_CNTL, DENTIST_DPREFCLK_WDIVIDER, mask_sh)

#if defined(CONFIG_DRM_AMD_DC_SI)
#define CLK_COMMON_MASK_SH_LIST_DCE60_COMMON_BASE(mask_sh) \
	CLK_SF(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_WDIVIDER, mask_sh),\
	CLK_SF(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_CHG_DONE, mask_sh)
#endif

#define CLK_COMMON_MASK_SH_LIST_DCN_COMMON_BASE(mask_sh) \
	CLK_SF(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_WDIVIDER, mask_sh),\
	CLK_SF(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_CHG_DONE, mask_sh)

#define CLK_MASK_SH_LIST_RV1(mask_sh) \
	CLK_COMMON_MASK_SH_LIST_DCN_COMMON_BASE(mask_sh),\
	CLK_SF(MP1_SMN_C2PMSG_67, CONTENT, mask_sh),\
	CLK_SF(MP1_SMN_C2PMSG_83, CONTENT, mask_sh),\
	CLK_SF(MP1_SMN_C2PMSG_91, CONTENT, mask_sh),

#define CLK_COMMON_MASK_SH_LIST_DCN20_BASE(mask_sh) \
	CLK_COMMON_MASK_SH_LIST_DCN_COMMON_BASE(mask_sh),\
	CLK_SF(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_WDIVIDER, mask_sh),\
	CLK_SF(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_CHG_DONE, mask_sh)

#define CLK_MASK_SH_LIST_NV10(mask_sh) \
	CLK_COMMON_MASK_SH_LIST_DCN20_BASE(mask_sh),\
	CLK_SF(CLK3_0_CLK3_CLK_PLL_REQ, FbMult_int, mask_sh),\
	CLK_SF(CLK3_0_CLK3_CLK_PLL_REQ, FbMult_frac, mask_sh)

#define CLK_COMMON_MASK_SH_LIST_DCN201_BASE(mask_sh) \
	CLK_COMMON_MASK_SH_LIST_DCN_COMMON_BASE(mask_sh),\
	CLK_SF(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_WDIVIDER, mask_sh),\
	CLK_SF(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_CHG_DONE, mask_sh),\
	CLK_SF(CLK4_0_CLK4_CLK_PLL_REQ, FbMult_int, mask_sh)

#define CLK_REG_LIST_DCN32()	  \
	SR(DENTIST_DISPCLK_CNTL), \
	CLK_SR_DCN32(CLK1_CLK_PLL_REQ), \
	CLK_SR_DCN32(CLK1_CLK0_DFS_CNTL), \
	CLK_SR_DCN32(CLK1_CLK1_DFS_CNTL), \
	CLK_SR_DCN32(CLK1_CLK2_DFS_CNTL), \
	CLK_SR_DCN32(CLK1_CLK3_DFS_CNTL), \
	CLK_SR_DCN32(CLK1_CLK4_DFS_CNTL)

#define CLK_COMMON_MASK_SH_LIST_DCN32(mask_sh) \
	CLK_COMMON_MASK_SH_LIST_DCN20_BASE(mask_sh),\
	CLK_SF(CLK1_CLK_PLL_REQ, FbMult_int, mask_sh),\
	CLK_SF(CLK1_CLK_PLL_REQ, FbMult_frac, mask_sh)

#define CLK_REG_LIST_DCN321()	  \
	SR(DENTIST_DISPCLK_CNTL), \
	CLK_SR_DCN321(CLK0_CLK_PLL_REQ,   CLK01, 0), \
	CLK_SR_DCN321(CLK0_CLK0_DFS_CNTL, CLK01, 0), \
	CLK_SR_DCN321(CLK0_CLK1_DFS_CNTL, CLK01, 0), \
	CLK_SR_DCN321(CLK0_CLK2_DFS_CNTL, CLK01, 0), \
	CLK_SR_DCN321(CLK0_CLK3_DFS_CNTL, CLK01, 0), \
	CLK_SR_DCN321(CLK0_CLK4_DFS_CNTL, CLK01, 0)

#define CLK_COMMON_MASK_SH_LIST_DCN321(mask_sh) \
	CLK_COMMON_MASK_SH_LIST_DCN20_BASE(mask_sh),\
	CLK_SF(CLK0_CLK_PLL_REQ, FbMult_int, mask_sh),\
	CLK_SF(CLK0_CLK_PLL_REQ, FbMult_frac, mask_sh)

#define CLK_REG_FIELD_LIST(type) \
	type DPREFCLK_SRC_SEL; \
	type DENTIST_DPREFCLK_WDIVIDER; \
	type DENTIST_DISPCLK_WDIVIDER; \
	type DENTIST_DISPCLK_CHG_DONE;

 
#define CLK20_REG_FIELD_LIST(type) \
	type DENTIST_DPPCLK_WDIVIDER; \
	type DENTIST_DPPCLK_CHG_DONE; \
	type FbMult_int; \
	type FbMult_frac;

#define VBIOS_SMU_REG_FIELD_LIST(type) \
	type CONTENT;

struct clk_mgr_shift {
	CLK_REG_FIELD_LIST(uint8_t)
	CLK20_REG_FIELD_LIST(uint8_t)
	VBIOS_SMU_REG_FIELD_LIST(uint32_t)
};

struct clk_mgr_mask {
	CLK_REG_FIELD_LIST(uint32_t)
	CLK20_REG_FIELD_LIST(uint32_t)
	VBIOS_SMU_REG_FIELD_LIST(uint32_t)
};

struct clk_mgr_registers {
	uint32_t DPREFCLK_CNTL;
	uint32_t DENTIST_DISPCLK_CNTL;
	uint32_t CLK4_CLK2_CURRENT_CNT;
	uint32_t CLK4_CLK_PLL_REQ;

	uint32_t CLK3_CLK2_DFS_CNTL;
	uint32_t CLK3_CLK_PLL_REQ;

	uint32_t CLK0_CLK2_DFS_CNTL;
	uint32_t CLK0_CLK_PLL_REQ;

	uint32_t CLK1_CLK_PLL_REQ;
	uint32_t CLK1_CLK0_DFS_CNTL;
	uint32_t CLK1_CLK1_DFS_CNTL;
	uint32_t CLK1_CLK2_DFS_CNTL;
	uint32_t CLK1_CLK3_DFS_CNTL;
	uint32_t CLK1_CLK4_DFS_CNTL;

	uint32_t CLK0_CLK0_DFS_CNTL;
	uint32_t CLK0_CLK1_DFS_CNTL;
	uint32_t CLK0_CLK3_DFS_CNTL;
	uint32_t CLK0_CLK4_DFS_CNTL;

	uint32_t MP1_SMN_C2PMSG_67;
	uint32_t MP1_SMN_C2PMSG_83;
	uint32_t MP1_SMN_C2PMSG_91;
};

enum clock_type {
	clock_type_dispclk = 1,
	clock_type_dcfclk,
	clock_type_socclk,
	clock_type_pixelclk,
	clock_type_phyclk,
	clock_type_dppclk,
	clock_type_fclk,
	clock_type_dcfdsclk,
	clock_type_dscclk,
	clock_type_uclk,
	clock_type_dramclk,
};


struct state_dependent_clocks {
	int display_clk_khz;
	int pixel_clk_khz;
};

struct clk_mgr_internal {
	struct clk_mgr base;
	int smu_ver;
	struct pp_smu_funcs *pp_smu;
	struct clk_mgr_internal_funcs *funcs;

	struct dccg *dccg;

	 
	const struct clk_mgr_registers *regs;
	const struct clk_mgr_shift *clk_mgr_shift;
	const struct clk_mgr_mask *clk_mgr_mask;

	struct state_dependent_clocks max_clks_by_state[DM_PP_CLOCKS_MAX_STATES];

	 
	 
	bool dfs_bypass_enabled;
	 
	bool dfs_bypass_active;

	uint32_t dfs_ref_freq_khz;
	 
	int dfs_bypass_disp_clk;

	 
	bool ss_on_dprefclk;

	 
	bool xgmi_enabled;

	 
	int dprefclk_ss_percentage;

	 
	int dprefclk_ss_divider;

	enum dm_pp_clocks_state max_clks_state;
	enum dm_pp_clocks_state cur_min_clks_state;
	bool periodic_retraining_disabled;

	unsigned int cur_phyclk_req_table[MAX_PIPES * 2];

	bool smu_present;
	void *wm_range_table;
	long long wm_range_table_addr;

	bool dpm_present;
};

struct clk_mgr_internal_funcs {
	int (*set_dispclk)(struct clk_mgr_internal *clk_mgr, int requested_dispclk_khz);
	int (*set_dprefclk)(struct clk_mgr_internal *clk_mgr);
};


 


static inline bool should_set_clock(bool safe_to_lower, int calc_clk, int cur_clk)
{
	return ((safe_to_lower && calc_clk < cur_clk) || calc_clk > cur_clk);
}

static inline bool should_update_pstate_support(bool safe_to_lower, bool calc_support, bool cur_support)
{
	if (cur_support != calc_support) {
		if (calc_support && safe_to_lower)
			return true;
		else if (!calc_support && !safe_to_lower)
			return true;
	}

	return false;
}

static inline int khz_to_mhz_ceil(int khz)
{
	return (khz + 999) / 1000;
}

int clk_mgr_helper_get_active_display_cnt(
		struct dc *dc,
		struct dc_state *context);

int clk_mgr_helper_get_active_plane_cnt(
		struct dc *dc,
		struct dc_state *context);



#endif 
