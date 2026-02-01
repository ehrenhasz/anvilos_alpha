 

int kgd_arcturus_hqd_sdma_load(struct amdgpu_device *adev, void *mqd,
			     uint32_t __user *wptr, struct mm_struct *mm);
int kgd_arcturus_hqd_sdma_dump(struct amdgpu_device *adev,
			     uint32_t engine_id, uint32_t queue_id,
			     uint32_t (**dump)[2], uint32_t *n_regs);
bool kgd_arcturus_hqd_sdma_is_occupied(struct amdgpu_device *adev,
				void *mqd);
int kgd_arcturus_hqd_sdma_destroy(struct amdgpu_device *adev, void *mqd,
				unsigned int utimeout);
