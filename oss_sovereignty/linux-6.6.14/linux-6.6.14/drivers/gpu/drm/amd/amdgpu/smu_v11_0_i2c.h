#ifndef SMU_V11_I2C_CONTROL_H
#define SMU_V11_I2C_CONTROL_H
#include <linux/types.h>
struct amdgpu_device;
int smu_v11_0_i2c_control_init(struct amdgpu_device *adev);
void smu_v11_0_i2c_control_fini(struct amdgpu_device *adev);
#endif
