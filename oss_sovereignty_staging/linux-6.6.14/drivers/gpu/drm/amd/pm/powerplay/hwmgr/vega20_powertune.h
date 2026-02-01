 
#ifndef _VEGA20_POWERTUNE_H_
#define _VEGA20_POWERTUNE_H_

int vega20_set_power_limit(struct pp_hwmgr *hwmgr, uint32_t n);
int vega20_power_control_set_level(struct pp_hwmgr *hwmgr);
int vega20_validate_power_level_request(struct pp_hwmgr *hwmgr,
		uint32_t tdp_percentage_adjustment,
		uint32_t tdp_absolute_value_adjustment);
#endif   

