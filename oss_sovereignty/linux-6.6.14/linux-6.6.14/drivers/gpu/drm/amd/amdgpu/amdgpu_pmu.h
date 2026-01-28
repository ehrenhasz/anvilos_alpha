#ifndef _AMDGPU_PMU_H_
#define _AMDGPU_PMU_H_
enum amdgpu_pmu_perf_type {
	AMDGPU_PMU_PERF_TYPE_NONE = 0,
	AMDGPU_PMU_PERF_TYPE_DF,
	AMDGPU_PMU_PERF_TYPE_ALL
};
enum amdgpu_pmu_event_config_type {
	AMDGPU_PMU_EVENT_CONFIG_TYPE_NONE = 0,
	AMDGPU_PMU_EVENT_CONFIG_TYPE_DF,
	AMDGPU_PMU_EVENT_CONFIG_TYPE_XGMI,
	AMDGPU_PMU_EVENT_CONFIG_TYPE_MAX
};
#define AMDGPU_PMU_EVENT_CONFIG_TYPE_SHIFT	56
#define AMDGPU_PMU_EVENT_CONFIG_TYPE_MASK	0xff
int amdgpu_pmu_init(struct amdgpu_device *adev);
void amdgpu_pmu_fini(struct amdgpu_device *adev);
#endif  
