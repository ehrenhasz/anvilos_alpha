#ifndef _SMU7_CLOCK_POWER_GATING_H_
#define _SMU7_CLOCK_POWER_GATING_H_
#include "smu7_hwmgr.h"
void smu7_powergate_vce(struct pp_hwmgr *hwmgr, bool bgate);
void smu7_powergate_uvd(struct pp_hwmgr *hwmgr, bool bgate);
int smu7_powerdown_uvd(struct pp_hwmgr *hwmgr);
int smu7_powergate_acp(struct pp_hwmgr *hwmgr, bool bgate);
int smu7_disable_clock_power_gating(struct pp_hwmgr *hwmgr);
int smu7_update_clock_gatings(struct pp_hwmgr *hwmgr,
					const uint32_t *msg_id);
int smu7_powergate_gfx(struct pp_hwmgr *hwmgr, bool enable);
#endif
