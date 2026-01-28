#ifndef _VEGA20_SMUMANAGER_H_
#define _VEGA20_SMUMANAGER_H_
#include "hwmgr.h"
#include "smu11_driver_if.h"
struct smu_table_entry {
	uint32_t version;
	uint32_t size;
	uint64_t mc_addr;
	void *table;
	struct amdgpu_bo *handle;
};
struct smu_table_array {
	struct smu_table_entry entry[TABLE_COUNT];
};
struct vega20_smumgr {
	struct smu_table_array            smu_tables;
};
#define SMU_FEATURES_LOW_MASK        0x00000000FFFFFFFF
#define SMU_FEATURES_LOW_SHIFT       0
#define SMU_FEATURES_HIGH_MASK       0xFFFFFFFF00000000
#define SMU_FEATURES_HIGH_SHIFT      32
int vega20_enable_smc_features(struct pp_hwmgr *hwmgr,
		bool enable, uint64_t feature_mask);
int vega20_get_enabled_smc_features(struct pp_hwmgr *hwmgr,
		uint64_t *features_enabled);
int vega20_set_activity_monitor_coeff(struct pp_hwmgr *hwmgr,
		uint8_t *table, uint16_t workload_type);
int vega20_get_activity_monitor_coeff(struct pp_hwmgr *hwmgr,
		uint8_t *table, uint16_t workload_type);
int vega20_set_pptable_driver_address(struct pp_hwmgr *hwmgr);
bool vega20_is_smc_ram_running(struct pp_hwmgr *hwmgr);
#endif
