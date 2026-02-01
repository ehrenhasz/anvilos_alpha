 
#ifndef _VEGA10_SMUMANAGER_H_
#define _VEGA10_SMUMANAGER_H_

#define MAX_SMU_TABLE 5

struct smu_table_entry {
	uint32_t version;
	uint32_t size;
	uint32_t table_id;
	uint64_t mc_addr;
	void *table;
	struct amdgpu_bo *handle;
};

struct smu_table_array {
	struct smu_table_entry entry[MAX_SMU_TABLE];
};

struct vega10_smumgr {
	struct smu_table_array            smu_tables;
};

int vega10_enable_smc_features(struct pp_hwmgr *hwmgr,
			       bool enable, uint32_t feature_mask);
int vega10_get_enabled_smc_features(struct pp_hwmgr *hwmgr,
				    uint64_t *features_enabled);

#endif

