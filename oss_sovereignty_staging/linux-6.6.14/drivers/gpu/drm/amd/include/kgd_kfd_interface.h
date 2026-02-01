 

 

#ifndef KGD_KFD_INTERFACE_H_INCLUDED
#define KGD_KFD_INTERFACE_H_INCLUDED

#include <linux/types.h>
#include <linux/bitmap.h>
#include <linux/dma-fence.h>
#include "amdgpu_irq.h"
#include "amdgpu_gfx.h"

struct pci_dev;
struct amdgpu_device;

struct kfd_dev;
struct kgd_mem;

enum kfd_preempt_type {
	KFD_PREEMPT_TYPE_WAVEFRONT_DRAIN = 0,
	KFD_PREEMPT_TYPE_WAVEFRONT_RESET,
	KFD_PREEMPT_TYPE_WAVEFRONT_SAVE
};

struct kfd_vm_fault_info {
	uint64_t	page_addr;
	uint32_t	vmid;
	uint32_t	mc_id;
	uint32_t	status;
	bool		prot_valid;
	bool		prot_read;
	bool		prot_write;
	bool		prot_exec;
};

struct kfd_cu_info {
	uint32_t num_shader_engines;
	uint32_t num_shader_arrays_per_engine;
	uint32_t num_cu_per_sh;
	uint32_t cu_active_number;
	uint32_t cu_ao_mask;
	uint32_t simd_per_cu;
	uint32_t max_waves_per_simd;
	uint32_t wave_front_size;
	uint32_t max_scratch_slots_per_cu;
	uint32_t lds_size;
	uint32_t cu_bitmap[AMDGPU_MAX_GC_INSTANCES][4][4];
};

 
struct kfd_local_mem_info {
	uint64_t local_mem_size_private;
	uint64_t local_mem_size_public;
	uint32_t vram_width;
	uint32_t mem_clk_max;
};

enum kgd_memory_pool {
	KGD_POOL_SYSTEM_CACHEABLE = 1,
	KGD_POOL_SYSTEM_WRITECOMBINE = 2,
	KGD_POOL_FRAMEBUFFER = 3,
};

 
enum kfd_sched_policy {
	KFD_SCHED_POLICY_HWS = 0,
	KFD_SCHED_POLICY_HWS_NO_OVERSUBSCRIPTION,
	KFD_SCHED_POLICY_NO_HWS
};

struct kgd2kfd_shared_resources {
	 
	unsigned int compute_vmid_bitmap;

	 
	uint32_t num_pipe_per_mec;

	 
	uint32_t num_queue_per_pipe;

	 
	DECLARE_BITMAP(cp_queue_bitmap, KGD_MAX_QUEUES);

	 
	uint32_t *sdma_doorbell_idx;

	 
	uint32_t non_cp_doorbells_start;
	uint32_t non_cp_doorbells_end;

	 
	phys_addr_t doorbell_physical_address;

	 
	size_t doorbell_aperture_size;

	 
	size_t doorbell_start_offset;

	 
	uint64_t gpuvm_size;

	 
	int drm_render_minor;

	bool enable_mes;
};

struct tile_config {
	uint32_t *tile_config_ptr;
	uint32_t *macro_tile_config_ptr;
	uint32_t num_tile_configs;
	uint32_t num_macro_tile_configs;

	uint32_t gb_addr_config;
	uint32_t num_banks;
	uint32_t num_ranks;
};

#define KFD_MAX_NUM_OF_QUEUES_PER_DEVICE_DEFAULT 4096

 
struct kfd2kgd_calls {
	 
	void (*program_sh_mem_settings)(struct amdgpu_device *adev, uint32_t vmid,
			uint32_t sh_mem_config,	uint32_t sh_mem_ape1_base,
			uint32_t sh_mem_ape1_limit, uint32_t sh_mem_bases,
			uint32_t inst);

	int (*set_pasid_vmid_mapping)(struct amdgpu_device *adev, u32 pasid,
					unsigned int vmid, uint32_t inst);

	int (*init_interrupts)(struct amdgpu_device *adev, uint32_t pipe_id,
			uint32_t inst);

	int (*hqd_load)(struct amdgpu_device *adev, void *mqd, uint32_t pipe_id,
			uint32_t queue_id, uint32_t __user *wptr,
			uint32_t wptr_shift, uint32_t wptr_mask,
			struct mm_struct *mm, uint32_t inst);

	int (*hiq_mqd_load)(struct amdgpu_device *adev, void *mqd,
			    uint32_t pipe_id, uint32_t queue_id,
			    uint32_t doorbell_off, uint32_t inst);

	int (*hqd_sdma_load)(struct amdgpu_device *adev, void *mqd,
			     uint32_t __user *wptr, struct mm_struct *mm);

	int (*hqd_dump)(struct amdgpu_device *adev,
			uint32_t pipe_id, uint32_t queue_id,
			uint32_t (**dump)[2], uint32_t *n_regs, uint32_t inst);

	int (*hqd_sdma_dump)(struct amdgpu_device *adev,
			     uint32_t engine_id, uint32_t queue_id,
			     uint32_t (**dump)[2], uint32_t *n_regs);

	bool (*hqd_is_occupied)(struct amdgpu_device *adev,
				uint64_t queue_address, uint32_t pipe_id,
				uint32_t queue_id, uint32_t inst);

	int (*hqd_destroy)(struct amdgpu_device *adev, void *mqd,
				enum kfd_preempt_type reset_type,
				unsigned int timeout, uint32_t pipe_id,
				uint32_t queue_id, uint32_t inst);

	bool (*hqd_sdma_is_occupied)(struct amdgpu_device *adev, void *mqd);

	int (*hqd_sdma_destroy)(struct amdgpu_device *adev, void *mqd,
				unsigned int timeout);

	int (*wave_control_execute)(struct amdgpu_device *adev,
					uint32_t gfx_index_val,
					uint32_t sq_cmd, uint32_t inst);
	bool (*get_atc_vmid_pasid_mapping_info)(struct amdgpu_device *adev,
					uint8_t vmid,
					uint16_t *p_pasid);

	 
	void (*set_scratch_backing_va)(struct amdgpu_device *adev,
				uint64_t va, uint32_t vmid);

	void (*set_vm_context_page_table_base)(struct amdgpu_device *adev,
			uint32_t vmid, uint64_t page_table_base);
	uint32_t (*read_vmid_from_vmfault_reg)(struct amdgpu_device *adev);

	uint32_t (*enable_debug_trap)(struct amdgpu_device *adev,
					bool restore_dbg_registers,
					uint32_t vmid);
	uint32_t (*disable_debug_trap)(struct amdgpu_device *adev,
					bool keep_trap_enabled,
					uint32_t vmid);
	int (*validate_trap_override_request)(struct amdgpu_device *adev,
					uint32_t trap_override,
					uint32_t *trap_mask_supported);
	uint32_t (*set_wave_launch_trap_override)(struct amdgpu_device *adev,
					     uint32_t vmid,
					     uint32_t trap_override,
					     uint32_t trap_mask_bits,
					     uint32_t trap_mask_request,
					     uint32_t *trap_mask_prev,
					     uint32_t kfd_dbg_trap_cntl_prev);
	uint32_t (*set_wave_launch_mode)(struct amdgpu_device *adev,
					uint8_t wave_launch_mode,
					uint32_t vmid);
	uint32_t (*set_address_watch)(struct amdgpu_device *adev,
					uint64_t watch_address,
					uint32_t watch_address_mask,
					uint32_t watch_id,
					uint32_t watch_mode,
					uint32_t debug_vmid,
					uint32_t inst);
	uint32_t (*clear_address_watch)(struct amdgpu_device *adev,
			uint32_t watch_id);
	void (*get_iq_wait_times)(struct amdgpu_device *adev,
			uint32_t *wait_times,
			uint32_t inst);
	void (*build_grace_period_packet_info)(struct amdgpu_device *adev,
			uint32_t wait_times,
			uint32_t grace_period,
			uint32_t *reg_offset,
			uint32_t *reg_data);
	void (*get_cu_occupancy)(struct amdgpu_device *adev, int pasid,
			int *wave_cnt, int *max_waves_per_cu, uint32_t inst);
	void (*program_trap_handler_settings)(struct amdgpu_device *adev,
			uint32_t vmid, uint64_t tba_addr, uint64_t tma_addr,
			uint32_t inst);
};

#endif	 
