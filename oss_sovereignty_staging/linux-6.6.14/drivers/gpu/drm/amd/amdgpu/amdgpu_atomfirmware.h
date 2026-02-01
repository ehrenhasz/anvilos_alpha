 

#ifndef __AMDGPU_ATOMFIRMWARE_H__
#define __AMDGPU_ATOMFIRMWARE_H__

#define get_index_into_master_table(master_table, table_name) (offsetof(struct master_table, table_name) / sizeof(uint16_t))

uint32_t amdgpu_atomfirmware_query_firmware_capability(struct amdgpu_device *adev);
bool amdgpu_atomfirmware_gpu_virtualization_supported(struct amdgpu_device *adev);
void amdgpu_atomfirmware_scratch_regs_init(struct amdgpu_device *adev);
int amdgpu_atomfirmware_allocate_fb_scratch(struct amdgpu_device *adev);
int amdgpu_atomfirmware_get_vram_info(struct amdgpu_device *adev,
	int *vram_width, int *vram_type, int *vram_vendor);
int amdgpu_atomfirmware_get_clock_info(struct amdgpu_device *adev);
int amdgpu_atomfirmware_get_gfx_info(struct amdgpu_device *adev);
bool amdgpu_atomfirmware_mem_ecc_supported(struct amdgpu_device *adev);
bool amdgpu_atomfirmware_sram_ecc_supported(struct amdgpu_device *adev);
bool amdgpu_atomfirmware_ras_rom_addr(struct amdgpu_device *adev, uint8_t* i2c_address);
bool amdgpu_atomfirmware_mem_training_supported(struct amdgpu_device *adev);
bool amdgpu_atomfirmware_dynamic_boot_config_supported(struct amdgpu_device *adev);
int amdgpu_atomfirmware_get_fw_reserved_fb_size(struct amdgpu_device *adev);
int amdgpu_atomfirmware_asic_init(struct amdgpu_device *adev, bool fb_reset);

#endif
