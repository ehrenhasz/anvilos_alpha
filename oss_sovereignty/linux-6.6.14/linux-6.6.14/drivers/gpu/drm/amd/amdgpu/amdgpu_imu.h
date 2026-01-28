#ifndef __AMDGPU_IMU_H__
#define __AMDGPU_IMU_H__
enum imu_work_mode {
	DEBUG_MODE,
	MISSION_MODE
};
struct amdgpu_imu_funcs {
    int (*init_microcode)(struct amdgpu_device *adev);
    int (*load_microcode)(struct amdgpu_device *adev);
    void (*setup_imu)(struct amdgpu_device *adev);
    int (*start_imu)(struct amdgpu_device *adev);
    void (*program_rlc_ram)(struct amdgpu_device *adev);
    int (*wait_for_reset_status)(struct amdgpu_device *adev);
};
struct imu_rlc_ram_golden {
    u32 hwip;
    u32 instance;
    u32 segment;
    u32 reg;
    u32 data;
    u32 addr_mask;
};
#define IMU_RLC_RAM_GOLDEN_VALUE(ip, inst, reg, data, addr_mask) \
    { ip##_HWIP, inst, reg##_BASE_IDX, reg, data, addr_mask }
struct amdgpu_imu {
    const struct amdgpu_imu_funcs *funcs;
    enum imu_work_mode mode;
};
#endif
