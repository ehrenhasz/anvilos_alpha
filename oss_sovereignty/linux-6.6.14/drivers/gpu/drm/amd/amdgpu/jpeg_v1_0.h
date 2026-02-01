 

#ifndef __JPEG_V1_0_H__
#define __JPEG_V1_0_H__

int jpeg_v1_0_early_init(void *handle);
int jpeg_v1_0_sw_init(void *handle);
void jpeg_v1_0_sw_fini(void *handle);
void jpeg_v1_0_start(struct amdgpu_device *adev, int mode);

#endif  
