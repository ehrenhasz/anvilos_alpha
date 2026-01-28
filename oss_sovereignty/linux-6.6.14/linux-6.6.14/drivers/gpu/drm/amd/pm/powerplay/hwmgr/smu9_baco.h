#ifndef __SMU9_BACO_H__
#define __SMU9_BACO_H__
#include "hwmgr.h"
#include "common_baco.h"
extern int smu9_baco_get_capability(struct pp_hwmgr *hwmgr, bool *cap);
extern int smu9_baco_get_state(struct pp_hwmgr *hwmgr, enum BACO_STATE *state);
#endif
