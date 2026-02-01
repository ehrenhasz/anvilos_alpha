 

#ifndef _ICELAND_SMUMGR_H_
#define _ICELAND_SMUMGR_H_


#include "smu7_smumgr.h"
#include "pp_endian.h"
#include "smu71_discrete.h"

struct iceland_pt_defaults {
	uint8_t   svi_load_line_en;
	uint8_t   svi_load_line_vddc;
	uint8_t   tdc_vddc_throttle_release_limit_perc;
	uint8_t   tdc_mawt;
	uint8_t   tdc_waterfall_ctl;
	uint8_t   dte_ambient_temp_base;
	uint32_t  display_cac;
	uint32_t  bapm_temp_gradient;
	uint16_t  bapmti_r[SMU71_DTE_ITERATIONS * SMU71_DTE_SOURCES * SMU71_DTE_SINKS];
	uint16_t  bapmti_rc[SMU71_DTE_ITERATIONS * SMU71_DTE_SOURCES * SMU71_DTE_SINKS];
};

struct iceland_mc_reg_entry {
	uint32_t mclk_max;
	uint32_t mc_data[SMU71_DISCRETE_MC_REGISTER_ARRAY_SIZE];
};

struct iceland_mc_reg_table {
	uint8_t   last;                
	uint8_t   num_entries;         
	uint16_t  validflag;           
	struct iceland_mc_reg_entry    mc_reg_table_entry[MAX_AC_TIMING_ENTRIES];
	SMU71_Discrete_MCRegisterAddress mc_reg_address[SMU71_DISCRETE_MC_REGISTER_ARRAY_SIZE];
};

struct iceland_smumgr {
	struct smu7_smumgr smu7_data;
	struct SMU71_Discrete_DpmTable       smc_state_table;
	struct SMU71_Discrete_PmFuses  power_tune_table;
	struct SMU71_Discrete_Ulv            ulv_setting;
	const struct iceland_pt_defaults  *power_tune_defaults;
	SMU71_Discrete_MCRegisters      mc_regs;
	struct iceland_mc_reg_table mc_reg_table;
};

#endif
