#ifndef VEGA20_THERMAL_H
#define VEGA20_THERMAL_H
#include "hwmgr.h"
struct vega20_temperature {
	uint16_t edge_temp;
	uint16_t hot_spot_temp;
	uint16_t hbm_temp;
	uint16_t vr_soc_temp;
	uint16_t vr_mem_temp;
	uint16_t liquid1_temp;
	uint16_t liquid2_temp;
	uint16_t plx_temp;
};
#define VEGA20_THERMAL_HIGH_ALERT_MASK         0x1
#define VEGA20_THERMAL_LOW_ALERT_MASK          0x2
#define VEGA20_THERMAL_MINIMUM_TEMP_READING    -256
#define VEGA20_THERMAL_MAXIMUM_TEMP_READING    255
#define VEGA20_THERMAL_MINIMUM_ALERT_TEMP      0
#define VEGA20_THERMAL_MAXIMUM_ALERT_TEMP      255
#define FDO_PWM_MODE_STATIC  1
#define FDO_PWM_MODE_STATIC_RPM 5
extern int vega20_thermal_get_temperature(struct pp_hwmgr *hwmgr);
extern int vega20_fan_ctrl_get_fan_speed_info(struct pp_hwmgr *hwmgr,
		struct phm_fan_speed_info *fan_speed_info);
extern int vega20_fan_ctrl_get_fan_speed_rpm(struct pp_hwmgr *hwmgr,
		uint32_t *speed);
extern int vega20_fan_ctrl_set_fan_speed_rpm(struct pp_hwmgr *hwmgr,
		uint32_t speed);
extern int vega20_fan_ctrl_get_fan_speed_pwm(struct pp_hwmgr *hwmgr,
		uint32_t *speed);
extern int vega20_fan_ctrl_set_fan_speed_pwm(struct pp_hwmgr *hwmgr,
		uint32_t speed);
extern int vega20_fan_ctrl_stop_smc_fan_control(struct pp_hwmgr *hwmgr);
extern int vega20_fan_ctrl_start_smc_fan_control(struct pp_hwmgr *hwmgr);
extern int vega20_thermal_disable_alert(struct pp_hwmgr *hwmgr);
extern int vega20_start_thermal_controller(struct pp_hwmgr *hwmgr,
				struct PP_TemperatureRange *range);
extern int vega20_thermal_stop_thermal_controller(struct pp_hwmgr *hwmgr);
#endif
