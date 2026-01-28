#ifndef _VEGA12_SMUMANAGER_H_
#define _VEGA12_SMUMANAGER_H_
#include "hwmgr.h"
#include "vega12/smu9_driver_if.h"
#include "vega12_hwmgr.h"
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
struct vega12_smumgr {
	struct smu_table_array            smu_tables;
};
#define SMU_FEATURES_LOW_MASK        0x00000000FFFFFFFF
#define SMU_FEATURES_LOW_SHIFT       0
#define SMU_FEATURES_HIGH_MASK       0xFFFFFFFF00000000
#define SMU_FEATURES_HIGH_SHIFT      32
int vega12_enable_smc_features(struct pp_hwmgr *hwmgr,
		bool enable, uint64_t feature_mask);
int vega12_get_enabled_smc_features(struct pp_hwmgr *hwmgr,
		uint64_t *features_enabled);
#endif
