#ifndef __VEGA20_BACO_H__
#define __VEGA20_BACO_H__
#include "hwmgr.h"
#include "common_baco.h"
extern int vega20_baco_get_capability(struct pp_hwmgr *hwmgr, bool *cap);
extern int vega20_baco_get_state(struct pp_hwmgr *hwmgr, enum BACO_STATE *state);
extern int vega20_baco_set_state(struct pp_hwmgr *hwmgr, enum BACO_STATE state);
extern int vega20_baco_apply_vdci_flush_workaround(struct pp_hwmgr *hwmgr);
#endif
