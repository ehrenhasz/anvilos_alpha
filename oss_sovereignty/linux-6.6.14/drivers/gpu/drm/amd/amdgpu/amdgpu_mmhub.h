 
#ifndef __AMDGPU_MMHUB_H__
#define __AMDGPU_MMHUB_H__

enum amdgpu_mmhub_ras_memory_id {
	AMDGPU_MMHUB_WGMI_PAGEMEM = 0,
	AMDGPU_MMHUB_RGMI_PAGEMEM = 1,
	AMDGPU_MMHUB_WDRAM_PAGEMEM = 2,
	AMDGPU_MMHUB_RDRAM_PAGEMEM = 3,
	AMDGPU_MMHUB_WIO_CMDMEM = 4,
	AMDGPU_MMHUB_RIO_CMDMEM = 5,
	AMDGPU_MMHUB_WGMI_CMDMEM = 6,
	AMDGPU_MMHUB_RGMI_CMDMEM = 7,
	AMDGPU_MMHUB_WDRAM_CMDMEM = 8,
	AMDGPU_MMHUB_RDRAM_CMDMEM = 9,
	AMDGPU_MMHUB_MAM_DMEM0 = 10,
	AMDGPU_MMHUB_MAM_DMEM1 = 11,
	AMDGPU_MMHUB_MAM_DMEM2 = 12,
	AMDGPU_MMHUB_MAM_DMEM3 = 13,
	AMDGPU_MMHUB_WRET_TAGMEM = 19,
	AMDGPU_MMHUB_RRET_TAGMEM = 20,
	AMDGPU_MMHUB_WIO_DATAMEM = 21,
	AMDGPU_MMHUB_WGMI_DATAMEM = 22,
	AMDGPU_MMHUB_WDRAM_DATAMEM = 23,
	AMDGPU_MMHUB_MEMORY_BLOCK_LAST,
};

struct amdgpu_mmhub_ras {
	struct amdgpu_ras_block_object ras_block;
};

struct amdgpu_mmhub_funcs {
	u64 (*get_fb_location)(struct amdgpu_device *adev);
	u64 (*get_mc_fb_offset)(struct amdgpu_device *adev);
	void (*init)(struct amdgpu_device *adev);
	int (*gart_enable)(struct amdgpu_device *adev);
	void (*set_fault_enable_default)(struct amdgpu_device *adev,
			bool value);
	void (*gart_disable)(struct amdgpu_device *adev);
	int (*set_clockgating)(struct amdgpu_device *adev,
			       enum amd_clockgating_state state);
	void (*get_clockgating)(struct amdgpu_device *adev, u64 *flags);
	void (*setup_vm_pt_regs)(struct amdgpu_device *adev, uint32_t vmid,
				uint64_t page_table_base);
	void (*update_power_gating)(struct amdgpu_device *adev,
                                bool enable);
};

struct amdgpu_mmhub {
	struct ras_common_if *ras_if;
	const struct amdgpu_mmhub_funcs *funcs;
	struct amdgpu_mmhub_ras  *ras;
};

int amdgpu_mmhub_ras_sw_init(struct amdgpu_device *adev);

#endif

