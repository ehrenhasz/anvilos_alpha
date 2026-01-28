#ifndef __AMDGPU_FRU_EEPROM_H__
#define __AMDGPU_FRU_EEPROM_H__
int amdgpu_fru_get_product_info(struct amdgpu_device *adev);
int amdgpu_fru_sysfs_init(struct amdgpu_device *adev);
void amdgpu_fru_sysfs_fini(struct amdgpu_device *adev);
#endif   
