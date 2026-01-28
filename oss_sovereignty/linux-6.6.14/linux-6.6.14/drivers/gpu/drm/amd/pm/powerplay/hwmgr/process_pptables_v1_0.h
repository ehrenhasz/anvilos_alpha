#ifndef _PROCESSPPTABLES_V1_0_H
#define _PROCESSPPTABLES_V1_0_H
#include "hwmgr.h"
extern const struct pp_table_func pptable_v1_0_funcs;
extern int get_number_of_powerplay_table_entries_v1_0(struct pp_hwmgr *hwmgr);
extern int get_powerplay_table_entry_v1_0(struct pp_hwmgr *hwmgr, uint32_t entry_index,
		struct pp_power_state *power_state, int (*call_back_func)(struct pp_hwmgr *, void *,
				struct pp_power_state *, void *, uint32_t));
#endif
