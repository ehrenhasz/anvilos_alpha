#ifndef __AMDGPU_DISCOVERY__
#define __AMDGPU_DISCOVERY__
#define DISCOVERY_TMR_SIZE      (8 << 10)
#define DISCOVERY_TMR_OFFSET    (64 << 10)
void amdgpu_discovery_fini(struct amdgpu_device *adev);
int amdgpu_discovery_set_ip_blocks(struct amdgpu_device *adev);
#endif  
