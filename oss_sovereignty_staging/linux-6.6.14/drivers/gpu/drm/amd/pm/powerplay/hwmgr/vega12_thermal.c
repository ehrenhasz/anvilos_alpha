 

#include "vega12_thermal.h"
#include "vega12_hwmgr.h"
#include "vega12_smumgr.h"
#include "vega12_ppsmc.h"
#include "vega12_inc.h"
#include "soc15_common.h"
#include "pp_debug.h"

static int vega12_get_current_rpm(struct pp_hwmgr *hwmgr, uint32_t *current_rpm)
{
	PP_ASSERT_WITH_CODE(!smum_send_msg_to_smc(hwmgr,
				PPSMC_MSG_GetCurrentRpm,
				current_rpm),
			"Attempt to get current RPM from SMC Failed!",
			return -EINVAL);

	return 0;
}

int vega12_fan_ctrl_get_fan_speed_info(struct pp_hwmgr *hwmgr,
		struct phm_fan_speed_info *fan_speed_info)
{
	memset(fan_speed_info, 0, sizeof(*fan_speed_info));
	fan_speed_info->supports_percent_read = false;
	fan_speed_info->supports_percent_write = false;
	fan_speed_info->supports_rpm_read = true;
	fan_speed_info->supports_rpm_write = true;

	return 0;
}

int vega12_fan_ctrl_get_fan_speed_rpm(struct pp_hwmgr *hwmgr, uint32_t *speed)
{
	*speed = 0;

	return vega12_get_current_rpm(hwmgr, speed);
}

 
static int vega12_enable_fan_control_feature(struct pp_hwmgr *hwmgr)
{
#if 0
	struct vega12_hwmgr *data = (struct vega12_hwmgr *)(hwmgr->backend);

	if (data->smu_features[GNLD_FAN_CONTROL].supported) {
		PP_ASSERT_WITH_CODE(!vega12_enable_smc_features(
				hwmgr, true,
				data->smu_features[GNLD_FAN_CONTROL].
				smu_feature_bitmap),
				"Attempt to Enable FAN CONTROL feature Failed!",
				return -1);
		data->smu_features[GNLD_FAN_CONTROL].enabled = true;
	}
#endif
	return 0;
}

static int vega12_disable_fan_control_feature(struct pp_hwmgr *hwmgr)
{
#if 0
	struct vega12_hwmgr *data = (struct vega12_hwmgr *)(hwmgr->backend);

	if (data->smu_features[GNLD_FAN_CONTROL].supported) {
		PP_ASSERT_WITH_CODE(!vega12_enable_smc_features(
				hwmgr, false,
				data->smu_features[GNLD_FAN_CONTROL].
				smu_feature_bitmap),
				"Attempt to Enable FAN CONTROL feature Failed!",
				return -1);
		data->smu_features[GNLD_FAN_CONTROL].enabled = false;
	}
#endif
	return 0;
}

int vega12_fan_ctrl_start_smc_fan_control(struct pp_hwmgr *hwmgr)
{
	struct vega12_hwmgr *data = (struct vega12_hwmgr *)(hwmgr->backend);

	if (data->smu_features[GNLD_FAN_CONTROL].supported)
		PP_ASSERT_WITH_CODE(
				!vega12_enable_fan_control_feature(hwmgr),
				"Attempt to Enable SMC FAN CONTROL Feature Failed!",
				return -1);

	return 0;
}


int vega12_fan_ctrl_stop_smc_fan_control(struct pp_hwmgr *hwmgr)
{
	struct vega12_hwmgr *data = (struct vega12_hwmgr *)(hwmgr->backend);

	if (data->smu_features[GNLD_FAN_CONTROL].supported)
		PP_ASSERT_WITH_CODE(!vega12_disable_fan_control_feature(hwmgr),
				"Attempt to Disable SMC FAN CONTROL Feature Failed!",
				return -1);

	return 0;
}

 
int vega12_fan_ctrl_reset_fan_speed_to_default(struct pp_hwmgr *hwmgr)
{
	return vega12_fan_ctrl_start_smc_fan_control(hwmgr);
}

 
int vega12_thermal_get_temperature(struct pp_hwmgr *hwmgr)
{
	struct amdgpu_device *adev = hwmgr->adev;
	int temp = 0;

	temp = RREG32_SOC15(THM, 0, mmCG_MULT_THERMAL_STATUS);

	temp = (temp & CG_MULT_THERMAL_STATUS__CTF_TEMP_MASK) >>
			CG_MULT_THERMAL_STATUS__CTF_TEMP__SHIFT;

	temp = temp & 0x1ff;

	temp *= PP_TEMPERATURE_UNITS_PER_CENTIGRADES;
	return temp;
}

 
static int vega12_thermal_set_temperature_range(struct pp_hwmgr *hwmgr,
		struct PP_TemperatureRange *range)
{
	struct phm_ppt_v3_information *pptable_information =
		(struct phm_ppt_v3_information *)hwmgr->pptable;
	struct amdgpu_device *adev = hwmgr->adev;
	int low = VEGA12_THERMAL_MINIMUM_ALERT_TEMP;
	int high = VEGA12_THERMAL_MAXIMUM_ALERT_TEMP;
	uint32_t val;

	 
	if (low < range->min / PP_TEMPERATURE_UNITS_PER_CENTIGRADES)
		low = range->min / PP_TEMPERATURE_UNITS_PER_CENTIGRADES;
	if (high > pptable_information->us_software_shutdown_temp)
		high = pptable_information->us_software_shutdown_temp;

	if (low > high)
		return -EINVAL;

	val = RREG32_SOC15(THM, 0, mmTHM_THERMAL_INT_CTRL);

	val = REG_SET_FIELD(val, THM_THERMAL_INT_CTRL, MAX_IH_CREDIT, 5);
	val = REG_SET_FIELD(val, THM_THERMAL_INT_CTRL, THERM_IH_HW_ENA, 1);
	val = REG_SET_FIELD(val, THM_THERMAL_INT_CTRL, DIG_THERM_INTH, high);
	val = REG_SET_FIELD(val, THM_THERMAL_INT_CTRL, DIG_THERM_INTL, low);
	val &= ~THM_THERMAL_INT_CTRL__THERM_TRIGGER_MASK_MASK;
	val &= ~THM_THERMAL_INT_CTRL__THERM_INTH_MASK_MASK;
	val &= ~THM_THERMAL_INT_CTRL__THERM_INTL_MASK_MASK;

	WREG32_SOC15(THM, 0, mmTHM_THERMAL_INT_CTRL, val);

	return 0;
}

 
static int vega12_thermal_enable_alert(struct pp_hwmgr *hwmgr)
{
	struct amdgpu_device *adev = hwmgr->adev;
	uint32_t val = 0;

	val |= (1 << THM_THERMAL_INT_ENA__THERM_INTH_CLR__SHIFT);
	val |= (1 << THM_THERMAL_INT_ENA__THERM_INTL_CLR__SHIFT);
	val |= (1 << THM_THERMAL_INT_ENA__THERM_TRIGGER_CLR__SHIFT);

	WREG32_SOC15(THM, 0, mmTHM_THERMAL_INT_ENA, val);

	return 0;
}

 
int vega12_thermal_disable_alert(struct pp_hwmgr *hwmgr)
{
	struct amdgpu_device *adev = hwmgr->adev;

	WREG32_SOC15(THM, 0, mmTHM_THERMAL_INT_ENA, 0);

	return 0;
}

 
int vega12_thermal_stop_thermal_controller(struct pp_hwmgr *hwmgr)
{
	int result = vega12_thermal_disable_alert(hwmgr);

	return result;
}

 
static int vega12_thermal_setup_fan_table(struct pp_hwmgr *hwmgr)
{
	int ret;
	struct vega12_hwmgr *data = (struct vega12_hwmgr *)(hwmgr->backend);
	PPTable_t *table = &(data->smc_state_table.pp_table);

	ret = smum_send_msg_to_smc_with_parameter(hwmgr,
				PPSMC_MSG_SetFanTemperatureTarget,
				(uint32_t)table->FanTargetTemperature,
				NULL);

	return ret;
}

 
static int vega12_thermal_start_smc_fan_control(struct pp_hwmgr *hwmgr)
{
	 
	if (PP_CAP(PHM_PlatformCaps_MicrocodeFanControl))
		vega12_fan_ctrl_start_smc_fan_control(hwmgr);

	return 0;
}


int vega12_start_thermal_controller(struct pp_hwmgr *hwmgr,
				struct PP_TemperatureRange *range)
{
	int ret = 0;

	if (range == NULL)
		return -EINVAL;

	ret = vega12_thermal_set_temperature_range(hwmgr, range);
	if (ret)
		return -EINVAL;

	vega12_thermal_enable_alert(hwmgr);
	 
	ret = vega12_thermal_setup_fan_table(hwmgr);
	if (ret)
		return -EINVAL;

	vega12_thermal_start_smc_fan_control(hwmgr);

	return 0;
};
