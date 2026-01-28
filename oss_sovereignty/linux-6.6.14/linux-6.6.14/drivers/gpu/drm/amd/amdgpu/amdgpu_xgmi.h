#ifndef __AMDGPU_XGMI_H__
#define __AMDGPU_XGMI_H__
#include <drm/task_barrier.h>
#include "amdgpu_psp.h"
#include "amdgpu_ras.h"
struct amdgpu_hive_info {
	struct kobject kobj;
	uint64_t hive_id;
	struct list_head device_list;
	struct list_head node;
	atomic_t number_devices;
	struct mutex hive_lock;
	int hi_req_count;
	struct amdgpu_device *hi_req_gpu;
	struct task_barrier tb;
	enum {
		AMDGPU_XGMI_PSTATE_MIN,
		AMDGPU_XGMI_PSTATE_MAX_VEGA20,
		AMDGPU_XGMI_PSTATE_UNKNOWN
	} pstate;
	struct amdgpu_reset_domain *reset_domain;
	uint32_t device_remove_count;
};
struct amdgpu_pcs_ras_field {
	const char *err_name;
	uint32_t pcs_err_mask;
	uint32_t pcs_err_shift;
};
extern struct amdgpu_xgmi_ras  xgmi_ras;
struct amdgpu_hive_info *amdgpu_get_xgmi_hive(struct amdgpu_device *adev);
void amdgpu_put_xgmi_hive(struct amdgpu_hive_info *hive);
int amdgpu_xgmi_update_topology(struct amdgpu_hive_info *hive, struct amdgpu_device *adev);
int amdgpu_xgmi_add_device(struct amdgpu_device *adev);
int amdgpu_xgmi_remove_device(struct amdgpu_device *adev);
int amdgpu_xgmi_set_pstate(struct amdgpu_device *adev, int pstate);
int amdgpu_xgmi_get_hops_count(struct amdgpu_device *adev,
		struct amdgpu_device *peer_adev);
int amdgpu_xgmi_get_num_links(struct amdgpu_device *adev,
		struct amdgpu_device *peer_adev);
uint64_t amdgpu_xgmi_get_relative_phy_addr(struct amdgpu_device *adev,
					   uint64_t addr);
static inline bool amdgpu_xgmi_same_hive(struct amdgpu_device *adev,
		struct amdgpu_device *bo_adev)
{
	return (amdgpu_use_xgmi_p2p &&
		adev != bo_adev &&
		adev->gmc.xgmi.hive_id &&
		adev->gmc.xgmi.hive_id == bo_adev->gmc.xgmi.hive_id);
}
int amdgpu_xgmi_ras_sw_init(struct amdgpu_device *adev);
#endif
