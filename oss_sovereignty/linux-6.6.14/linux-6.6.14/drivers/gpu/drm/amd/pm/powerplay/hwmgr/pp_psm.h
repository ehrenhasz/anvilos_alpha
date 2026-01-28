#ifndef PP_PSM_H
#define PP_PSM_H
#include "hwmgr.h"
int psm_init_power_state_table(struct pp_hwmgr *hwmgr);
int psm_fini_power_state_table(struct pp_hwmgr *hwmgr);
int psm_set_boot_states(struct pp_hwmgr *hwmgr);
int psm_set_performance_states(struct pp_hwmgr *hwmgr);
int psm_set_user_performance_state(struct pp_hwmgr *hwmgr,
					enum PP_StateUILabel label_id,
					struct pp_power_state **state);
int psm_adjust_power_state_dynamic(struct pp_hwmgr *hwmgr,
				bool skip_display_settings,
				struct pp_power_state *new_ps);
#endif
