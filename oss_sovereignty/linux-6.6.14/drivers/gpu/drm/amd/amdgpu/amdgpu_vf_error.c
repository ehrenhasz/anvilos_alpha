 

#include "amdgpu.h"
#include "amdgpu_vf_error.h"
#include "mxgpu_ai.h"

void amdgpu_vf_error_put(struct amdgpu_device *adev,
			 uint16_t sub_error_code,
			 uint16_t error_flags,
			 uint64_t error_data)
{
	int index;
	uint16_t error_code;

	if (!amdgpu_sriov_vf(adev))
		return;

	error_code = AMDGIM_ERROR_CODE(AMDGIM_ERROR_CATEGORY_VF, sub_error_code);

	mutex_lock(&adev->virt.vf_errors.lock);
	index = adev->virt.vf_errors.write_count % AMDGPU_VF_ERROR_ENTRY_SIZE;
	adev->virt.vf_errors.code [index] = error_code;
	adev->virt.vf_errors.flags [index] = error_flags;
	adev->virt.vf_errors.data [index] = error_data;
	adev->virt.vf_errors.write_count ++;
	mutex_unlock(&adev->virt.vf_errors.lock);
}


void amdgpu_vf_error_trans_all(struct amdgpu_device *adev)
{
	 
	u32 data1, data2, data3;
	int index;

	if ((NULL == adev) || (!amdgpu_sriov_vf(adev)) ||
	    (!adev->virt.ops) || (!adev->virt.ops->trans_msg)) {
		return;
	}
 

	mutex_lock(&adev->virt.vf_errors.lock);
	 
	if (adev->virt.vf_errors.write_count - adev->virt.vf_errors.read_count > AMDGPU_VF_ERROR_ENTRY_SIZE) {
		adev->virt.vf_errors.read_count = adev->virt.vf_errors.write_count - AMDGPU_VF_ERROR_ENTRY_SIZE;
	}

	while (adev->virt.vf_errors.read_count < adev->virt.vf_errors.write_count) {
		index =adev->virt.vf_errors.read_count % AMDGPU_VF_ERROR_ENTRY_SIZE;
		data1 = AMDGIM_ERROR_CODE_FLAGS_TO_MAILBOX(adev->virt.vf_errors.code[index],
							   adev->virt.vf_errors.flags[index]);
		data2 = adev->virt.vf_errors.data[index] & 0xFFFFFFFF;
		data3 = (adev->virt.vf_errors.data[index] >> 32) & 0xFFFFFFFF;

		adev->virt.ops->trans_msg(adev, IDH_LOG_VF_ERROR, data1, data2, data3);
		adev->virt.vf_errors.read_count ++;
	}
	mutex_unlock(&adev->virt.vf_errors.lock);
}
