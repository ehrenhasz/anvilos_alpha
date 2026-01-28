#ifndef __AMDGPU_MCA_H__
#define __AMDGPU_MCA_H__
struct amdgpu_mca_ras_block {
	struct amdgpu_ras_block_object ras_block;
};
struct amdgpu_mca_ras {
	struct ras_common_if *ras_if;
	struct amdgpu_mca_ras_block *ras;
};
struct amdgpu_mca {
	struct amdgpu_mca_ras mp0;
	struct amdgpu_mca_ras mp1;
	struct amdgpu_mca_ras mpio;
};
void amdgpu_mca_query_correctable_error_count(struct amdgpu_device *adev,
					      uint64_t mc_status_addr,
					      unsigned long *error_count);
void amdgpu_mca_query_uncorrectable_error_count(struct amdgpu_device *adev,
						uint64_t mc_status_addr,
						unsigned long *error_count);
void amdgpu_mca_reset_error_count(struct amdgpu_device *adev,
				  uint64_t mc_status_addr);
void amdgpu_mca_query_ras_error_count(struct amdgpu_device *adev,
				      uint64_t mc_status_addr,
				      void *ras_error_status);
int amdgpu_mca_mp0_ras_sw_init(struct amdgpu_device *adev);
int amdgpu_mca_mp1_ras_sw_init(struct amdgpu_device *adev);
int amdgpu_mca_mpio_ras_sw_init(struct amdgpu_device *adev);
#endif
