#ifndef __AMDGPU_GMC_H__
#define __AMDGPU_GMC_H__
#include <linux/types.h>
#include "amdgpu_irq.h"
#include "amdgpu_ras.h"
#define AMDGPU_GMC_HOLE_START	0x0000800000000000ULL
#define AMDGPU_GMC_HOLE_END	0xffff800000000000ULL
#define AMDGPU_GMC_HOLE_MASK	0x0000ffffffffffffULL
#define AMDGPU_GMC_FAULT_RING_ORDER	8
#define AMDGPU_GMC_FAULT_RING_SIZE	(1 << AMDGPU_GMC_FAULT_RING_ORDER)
#define AMDGPU_GMC_FAULT_HASH_ORDER	8
#define AMDGPU_GMC_FAULT_HASH_SIZE	(1 << AMDGPU_GMC_FAULT_HASH_ORDER)
#define AMDGPU_GMC_FAULT_TIMEOUT	5000ULL
struct firmware;
enum amdgpu_memory_partition {
	UNKNOWN_MEMORY_PARTITION_MODE = 0,
	AMDGPU_NPS1_PARTITION_MODE = 1,
	AMDGPU_NPS2_PARTITION_MODE = 2,
	AMDGPU_NPS3_PARTITION_MODE = 3,
	AMDGPU_NPS4_PARTITION_MODE = 4,
	AMDGPU_NPS6_PARTITION_MODE = 6,
	AMDGPU_NPS8_PARTITION_MODE = 8,
};
struct amdgpu_gmc_fault {
	uint64_t	timestamp:48;
	uint64_t	next:AMDGPU_GMC_FAULT_RING_ORDER;
	atomic64_t	key;
	uint64_t	timestamp_expiry:48;
};
struct amdgpu_vmhub_funcs {
	void (*print_l2_protection_fault_status)(struct amdgpu_device *adev,
						 uint32_t status);
	uint32_t (*get_invalidate_req)(unsigned int vmid, uint32_t flush_type);
};
struct amdgpu_vmhub {
	uint32_t	ctx0_ptb_addr_lo32;
	uint32_t	ctx0_ptb_addr_hi32;
	uint32_t	vm_inv_eng0_sem;
	uint32_t	vm_inv_eng0_req;
	uint32_t	vm_inv_eng0_ack;
	uint32_t	vm_context0_cntl;
	uint32_t	vm_l2_pro_fault_status;
	uint32_t	vm_l2_pro_fault_cntl;
	uint32_t	ctx_distance;
	uint32_t	ctx_addr_distance;  
	uint32_t	eng_distance;
	uint32_t	eng_addr_distance;  
	uint32_t        vm_cntx_cntl;
	uint32_t	vm_cntx_cntl_vm_fault;
	uint32_t	vm_l2_bank_select_reserved_cid2;
	uint32_t	vm_contexts_disable;
	const struct amdgpu_vmhub_funcs *vmhub_funcs;
};
struct amdgpu_gmc_funcs {
	void (*flush_gpu_tlb)(struct amdgpu_device *adev, uint32_t vmid,
				uint32_t vmhub, uint32_t flush_type);
	int (*flush_gpu_tlb_pasid)(struct amdgpu_device *adev, uint16_t pasid,
					uint32_t flush_type, bool all_hub,
					uint32_t inst);
	uint64_t (*emit_flush_gpu_tlb)(struct amdgpu_ring *ring, unsigned vmid,
				       uint64_t pd_addr);
	void (*emit_pasid_mapping)(struct amdgpu_ring *ring, unsigned vmid,
				   unsigned pasid);
	void (*set_prt)(struct amdgpu_device *adev, bool enable);
	uint64_t (*map_mtype)(struct amdgpu_device *adev, uint32_t flags);
	void (*get_vm_pde)(struct amdgpu_device *adev, int level,
			   u64 *dst, u64 *flags);
	void (*get_vm_pte)(struct amdgpu_device *adev,
			   struct amdgpu_bo_va_mapping *mapping,
			   uint64_t *flags);
	void (*override_vm_pte_flags)(struct amdgpu_device *dev,
				      struct amdgpu_vm *vm,
				      uint64_t addr, uint64_t *flags);
	unsigned int (*get_vbios_fb_size)(struct amdgpu_device *adev);
	enum amdgpu_memory_partition (*query_mem_partition_mode)(
		struct amdgpu_device *adev);
};
struct amdgpu_xgmi_ras {
	struct amdgpu_ras_block_object ras_block;
};
struct amdgpu_xgmi {
	u64 node_id;
	u64 hive_id;
	u64 node_segment_size;
	unsigned physical_node_id;
	unsigned num_physical_nodes;
	struct list_head head;
	bool supported;
	struct ras_common_if *ras_if;
	bool connected_to_cpu;
	bool pending_reset;
	struct amdgpu_xgmi_ras *ras;
};
struct amdgpu_mem_partition_info {
	union {
		struct {
			uint32_t fpfn;
			uint32_t lpfn;
		} range;
		struct {
			int node;
		} numa;
	};
	uint64_t size;
};
#define INVALID_PFN    -1
struct amdgpu_gmc {
	resource_size_t		aper_size;
	resource_size_t		aper_base;
	u64			mc_vram_size;
	u64			visible_vram_size;
	u64			agp_size;
	u64			agp_start;
	u64			agp_end;
	u64			gart_size;
	u64			gart_start;
	u64			gart_end;
	u64			vram_start;
	u64			vram_end;
	u64			fb_start;
	u64			fb_end;
	unsigned		vram_width;
	u64			real_vram_size;
	int			vram_mtrr;
	u64                     mc_mask;
	const struct firmware   *fw;	 
	uint32_t                fw_version;
	struct amdgpu_irq_src	vm_fault;
	uint32_t		vram_type;
	uint8_t			vram_vendor;
	uint32_t                srbm_soft_reset;
	bool			prt_warning;
	uint32_t		sdpif_register;
	u64			shared_aperture_start;
	u64			shared_aperture_end;
	u64			private_aperture_start;
	u64			private_aperture_end;
	spinlock_t		invalidate_lock;
	bool			translate_further;
	struct kfd_vm_fault_info *vm_fault_info;
	atomic_t		vm_fault_info_updated;
	struct amdgpu_gmc_fault	fault_ring[AMDGPU_GMC_FAULT_RING_SIZE];
	struct {
		uint64_t	idx:AMDGPU_GMC_FAULT_RING_ORDER;
	} fault_hash[AMDGPU_GMC_FAULT_HASH_SIZE];
	uint64_t		last_fault:AMDGPU_GMC_FAULT_RING_ORDER;
	bool tmz_enabled;
	bool is_app_apu;
	struct amdgpu_mem_partition_info *mem_partitions;
	uint8_t num_mem_partitions;
	const struct amdgpu_gmc_funcs	*gmc_funcs;
	struct amdgpu_xgmi xgmi;
	struct amdgpu_irq_src	ecc_irq;
	int noretry;
	uint32_t	vmid0_page_table_block_size;
	uint32_t	vmid0_page_table_depth;
	struct amdgpu_bo		*pdb0_bo;
	void				*ptr_pdb0;
	u64 mall_size;
	uint32_t m_half_use;
	int num_umc;
	u64 VM_L2_CNTL;
	u64 VM_L2_CNTL2;
	u64 VM_DUMMY_PAGE_FAULT_CNTL;
	u64 VM_DUMMY_PAGE_FAULT_ADDR_LO32;
	u64 VM_DUMMY_PAGE_FAULT_ADDR_HI32;
	u64 VM_L2_PROTECTION_FAULT_CNTL;
	u64 VM_L2_PROTECTION_FAULT_CNTL2;
	u64 VM_L2_PROTECTION_FAULT_MM_CNTL3;
	u64 VM_L2_PROTECTION_FAULT_MM_CNTL4;
	u64 VM_L2_PROTECTION_FAULT_ADDR_LO32;
	u64 VM_L2_PROTECTION_FAULT_ADDR_HI32;
	u64 VM_DEBUG;
	u64 VM_L2_MM_GROUP_RT_CLASSES;
	u64 VM_L2_BANK_SELECT_RESERVED_CID;
	u64 VM_L2_BANK_SELECT_RESERVED_CID2;
	u64 VM_L2_CACHE_PARITY_CNTL;
	u64 VM_L2_IH_LOG_CNTL;
	u64 VM_CONTEXT_CNTL[16];
	u64 VM_CONTEXT_PAGE_TABLE_BASE_ADDR_LO32[16];
	u64 VM_CONTEXT_PAGE_TABLE_BASE_ADDR_HI32[16];
	u64 VM_CONTEXT_PAGE_TABLE_START_ADDR_LO32[16];
	u64 VM_CONTEXT_PAGE_TABLE_START_ADDR_HI32[16];
	u64 VM_CONTEXT_PAGE_TABLE_END_ADDR_LO32[16];
	u64 VM_CONTEXT_PAGE_TABLE_END_ADDR_HI32[16];
	u64 MC_VM_MX_L1_TLB_CNTL;
	u64 noretry_flags;
};
#define amdgpu_gmc_flush_gpu_tlb(adev, vmid, vmhub, type) ((adev)->gmc.gmc_funcs->flush_gpu_tlb((adev), (vmid), (vmhub), (type)))
#define amdgpu_gmc_flush_gpu_tlb_pasid(adev, pasid, type, allhub, inst) \
	((adev)->gmc.gmc_funcs->flush_gpu_tlb_pasid \
	((adev), (pasid), (type), (allhub), (inst)))
#define amdgpu_gmc_emit_flush_gpu_tlb(r, vmid, addr) (r)->adev->gmc.gmc_funcs->emit_flush_gpu_tlb((r), (vmid), (addr))
#define amdgpu_gmc_emit_pasid_mapping(r, vmid, pasid) (r)->adev->gmc.gmc_funcs->emit_pasid_mapping((r), (vmid), (pasid))
#define amdgpu_gmc_map_mtype(adev, flags) (adev)->gmc.gmc_funcs->map_mtype((adev),(flags))
#define amdgpu_gmc_get_vm_pde(adev, level, dst, flags) (adev)->gmc.gmc_funcs->get_vm_pde((adev), (level), (dst), (flags))
#define amdgpu_gmc_get_vm_pte(adev, mapping, flags) (adev)->gmc.gmc_funcs->get_vm_pte((adev), (mapping), (flags))
#define amdgpu_gmc_override_vm_pte_flags(adev, vm, addr, pte_flags)	\
	(adev)->gmc.gmc_funcs->override_vm_pte_flags			\
		((adev), (vm), (addr), (pte_flags))
#define amdgpu_gmc_get_vbios_fb_size(adev) (adev)->gmc.gmc_funcs->get_vbios_fb_size((adev))
static inline bool amdgpu_gmc_vram_full_visible(struct amdgpu_gmc *gmc)
{
	WARN_ON(gmc->real_vram_size < gmc->visible_vram_size);
	return (gmc->real_vram_size == gmc->visible_vram_size);
}
static inline uint64_t amdgpu_gmc_sign_extend(uint64_t addr)
{
	if (addr >= AMDGPU_GMC_HOLE_START)
		addr |= AMDGPU_GMC_HOLE_END;
	return addr;
}
int amdgpu_gmc_pdb0_alloc(struct amdgpu_device *adev);
void amdgpu_gmc_get_pde_for_bo(struct amdgpu_bo *bo, int level,
			       uint64_t *addr, uint64_t *flags);
int amdgpu_gmc_set_pte_pde(struct amdgpu_device *adev, void *cpu_pt_addr,
				uint32_t gpu_page_idx, uint64_t addr,
				uint64_t flags);
uint64_t amdgpu_gmc_pd_addr(struct amdgpu_bo *bo);
uint64_t amdgpu_gmc_agp_addr(struct ttm_buffer_object *bo);
void amdgpu_gmc_sysvm_location(struct amdgpu_device *adev, struct amdgpu_gmc *mc);
void amdgpu_gmc_vram_location(struct amdgpu_device *adev, struct amdgpu_gmc *mc,
			      u64 base);
void amdgpu_gmc_gart_location(struct amdgpu_device *adev,
			      struct amdgpu_gmc *mc);
void amdgpu_gmc_agp_location(struct amdgpu_device *adev,
			     struct amdgpu_gmc *mc);
bool amdgpu_gmc_filter_faults(struct amdgpu_device *adev,
			      struct amdgpu_ih_ring *ih, uint64_t addr,
			      uint16_t pasid, uint64_t timestamp);
void amdgpu_gmc_filter_faults_remove(struct amdgpu_device *adev, uint64_t addr,
				     uint16_t pasid);
int amdgpu_gmc_ras_sw_init(struct amdgpu_device *adev);
int amdgpu_gmc_ras_late_init(struct amdgpu_device *adev);
void amdgpu_gmc_ras_fini(struct amdgpu_device *adev);
int amdgpu_gmc_allocate_vm_inv_eng(struct amdgpu_device *adev);
extern void amdgpu_gmc_tmz_set(struct amdgpu_device *adev);
extern void amdgpu_gmc_noretry_set(struct amdgpu_device *adev);
extern void
amdgpu_gmc_set_vm_fault_masks(struct amdgpu_device *adev, int hub_type,
			      bool enable);
void amdgpu_gmc_get_vbios_allocations(struct amdgpu_device *adev);
void amdgpu_gmc_init_pdb0(struct amdgpu_device *adev);
uint64_t amdgpu_gmc_vram_mc2pa(struct amdgpu_device *adev, uint64_t mc_addr);
uint64_t amdgpu_gmc_vram_pa(struct amdgpu_device *adev, struct amdgpu_bo *bo);
uint64_t amdgpu_gmc_vram_cpu_pa(struct amdgpu_device *adev, struct amdgpu_bo *bo);
int amdgpu_gmc_vram_checking(struct amdgpu_device *adev);
int amdgpu_gmc_sysfs_init(struct amdgpu_device *adev);
void amdgpu_gmc_sysfs_fini(struct amdgpu_device *adev);
#endif
