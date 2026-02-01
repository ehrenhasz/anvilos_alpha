 

#ifndef PP_SMU10_SMUMANAGER_H
#define PP_SMU10_SMUMANAGER_H

#include "rv_ppsmc.h"
#include "smu10_driver_if.h"

#define MAX_SMU_TABLE 2

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

struct smu10_smumgr {
	struct smu_table_array            smu_tables;
};


#endif
