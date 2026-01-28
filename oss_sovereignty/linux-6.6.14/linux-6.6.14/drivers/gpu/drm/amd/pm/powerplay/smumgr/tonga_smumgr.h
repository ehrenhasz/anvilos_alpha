#ifndef _TONGA_SMUMGR_H_
#define _TONGA_SMUMGR_H_
#include "smu72_discrete.h"
#include "smu7_smumgr.h"
#include "smu72.h"
#define ASICID_IS_TONGA_P(wDID, bRID)	 \
	(((wDID == 0x6930) && ((bRID == 0xF0) || (bRID == 0xF1) || (bRID == 0xFF))) \
	|| ((wDID == 0x6920) && ((bRID == 0) || (bRID == 1))))
struct tonga_pt_defaults {
	uint8_t   svi_load_line_en;
	uint8_t   svi_load_line_vddC;
	uint8_t   tdc_vddc_throttle_release_limit_perc;
	uint8_t   tdc_mawt;
	uint8_t   tdc_waterfall_ctl;
	uint8_t   dte_ambient_temp_base;
	uint32_t  display_cac;
	uint32_t  bapm_temp_gradient;
	uint16_t  bapmti_r[SMU72_DTE_ITERATIONS * SMU72_DTE_SOURCES * SMU72_DTE_SINKS];
	uint16_t  bapmti_rc[SMU72_DTE_ITERATIONS * SMU72_DTE_SOURCES * SMU72_DTE_SINKS];
};
struct tonga_mc_reg_entry {
	uint32_t mclk_max;
	uint32_t mc_data[SMU72_DISCRETE_MC_REGISTER_ARRAY_SIZE];
};
struct tonga_mc_reg_table {
	uint8_t   last;                
	uint8_t   num_entries;         
	uint16_t  validflag;           
	struct tonga_mc_reg_entry    mc_reg_table_entry[MAX_AC_TIMING_ENTRIES];
	SMU72_Discrete_MCRegisterAddress mc_reg_address[SMU72_DISCRETE_MC_REGISTER_ARRAY_SIZE];
};
struct tonga_smumgr {
	struct smu7_smumgr                   smu7_data;
	struct SMU72_Discrete_DpmTable       smc_state_table;
	struct SMU72_Discrete_Ulv            ulv_setting;
	struct SMU72_Discrete_PmFuses  power_tune_table;
	const struct tonga_pt_defaults  *power_tune_defaults;
	SMU72_Discrete_MCRegisters      mc_regs;
	struct tonga_mc_reg_table mc_reg_table;
};
#endif
