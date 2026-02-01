 
#ifndef _FIJI_SMUMANAGER_H_
#define _FIJI_SMUMANAGER_H_

#include "smu73_discrete.h"
#include <pp_endian.h>
#include "smu7_smumgr.h"


struct fiji_pt_defaults {
	uint8_t   SviLoadLineEn;
	uint8_t   SviLoadLineVddC;
	uint8_t   TDC_VDDC_ThrottleReleaseLimitPerc;
	uint8_t   TDC_MAWt;
	uint8_t   TdcWaterfallCtl;
	uint8_t   DTEAmbientTempBase;
};

struct fiji_smumgr {
	struct smu7_smumgr                   smu7_data;
	struct SMU73_Discrete_DpmTable       smc_state_table;
	struct SMU73_Discrete_Ulv            ulv_setting;
	struct SMU73_Discrete_PmFuses  power_tune_table;
	const struct fiji_pt_defaults  *power_tune_defaults;
};

#endif

