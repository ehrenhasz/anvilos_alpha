 

#include <linux/io-64-nonatomic-lo-hi.h>
#ifdef CONFIG_X86
#include <asm/hypervisor.h>
#endif

#include "amdgpu.h"
#include "amdgpu_gmc.h"
#include "amdgpu_ras.h"
#include "amdgpu_xgmi.h"

#include <drm/drm_drv.h>
#include <drm/ttm/ttm_tt.h>

 
int amdgpu_gmc_pdb0_alloc(struct amdgpu_device *adev)
{
	int r;
	struct amdgpu_bo_param bp;
	u64 vram_size = adev->gmc.xgmi.node_segment_size * adev->gmc.xgmi.num_physical_nodes;
	uint32_t pde0_page_shift = adev->gmc.vmid0_page_table_block_size + 21;
	uint32_t npdes = (vram_size + (1ULL << pde0_page_shift) -1) >> pde0_page_shift;

	memset(&bp, 0, sizeof(bp));
	bp.size = PAGE_ALIGN((npdes + 1) * 8);
	bp.byte_align = PAGE_SIZE;
	bp.domain = AMDGPU_GEM_DOMAIN_VRAM;
	bp.flags = AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED |
		AMDGPU_GEM_CREATE_VRAM_CONTIGUOUS;
	bp.type = ttm_bo_type_kernel;
	bp.resv = NULL;
	bp.bo_ptr_size = sizeof(struct amdgpu_bo);

	r = amdgpu_bo_create(adev, &bp, &adev->gmc.pdb0_bo);
	if (r)
		return r;

	r = amdgpu_bo_reserve(adev->gmc.pdb0_bo, false);
	if (unlikely(r != 0))
		goto bo_reserve_failure;

	r = amdgpu_bo_pin(adev->gmc.pdb0_bo, AMDGPU_GEM_DOMAIN_VRAM);
	if (r)
		goto bo_pin_failure;
	r = amdgpu_bo_kmap(adev->gmc.pdb0_bo, &adev->gmc.ptr_pdb0);
	if (r)
		goto bo_kmap_failure;

	amdgpu_bo_unreserve(adev->gmc.pdb0_bo);
	return 0;

bo_kmap_failure:
	amdgpu_bo_unpin(adev->gmc.pdb0_bo);
bo_pin_failure:
	amdgpu_bo_unreserve(adev->gmc.pdb0_bo);
bo_reserve_failure:
	amdgpu_bo_unref(&adev->gmc.pdb0_bo);
	return r;
}

 
void amdgpu_gmc_get_pde_for_bo(struct amdgpu_bo *bo, int level,
			       uint64_t *addr, uint64_t *flags)
{
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);

	switch (bo->tbo.resource->mem_type) {
	case TTM_PL_TT:
		*addr = bo->tbo.ttm->dma_address[0];
		break;
	case TTM_PL_VRAM:
		*addr = amdgpu_bo_gpu_offset(bo);
		break;
	default:
		*addr = 0;
		break;
	}
	*flags = amdgpu_ttm_tt_pde_flags(bo->tbo.ttm, bo->tbo.resource);
	amdgpu_gmc_get_vm_pde(adev, level, addr, flags);
}

 
uint64_t amdgpu_gmc_pd_addr(struct amdgpu_bo *bo)
{
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);
	uint64_t pd_addr;

	 
	if (adev->asic_type >= CHIP_VEGA10) {
		uint64_t flags = AMDGPU_PTE_VALID;

		amdgpu_gmc_get_pde_for_bo(bo, -1, &pd_addr, &flags);
		pd_addr |= flags;
	} else {
		pd_addr = amdgpu_bo_gpu_offset(bo);
	}
	return pd_addr;
}

 
int amdgpu_gmc_set_pte_pde(struct amdgpu_device *adev, void *cpu_pt_addr,
				uint32_t gpu_page_idx, uint64_t addr,
				uint64_t flags)
{
	void __iomem *ptr = (void *)cpu_pt_addr;
	uint64_t value;

	 
	value = addr & 0x0000FFFFFFFFF000ULL;
	value |= flags;
	writeq(value, ptr + (gpu_page_idx * 8));

	return 0;
}

 
uint64_t amdgpu_gmc_agp_addr(struct ttm_buffer_object *bo)
{
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->bdev);

	if (bo->ttm->num_pages != 1 || bo->ttm->caching == ttm_cached)
		return AMDGPU_BO_INVALID_OFFSET;

	if (bo->ttm->dma_address[0] + PAGE_SIZE >= adev->gmc.agp_size)
		return AMDGPU_BO_INVALID_OFFSET;

	return adev->gmc.agp_start + bo->ttm->dma_address[0];
}

 
void amdgpu_gmc_vram_location(struct amdgpu_device *adev, struct amdgpu_gmc *mc,
			      u64 base)
{
	uint64_t vis_limit = (uint64_t)amdgpu_vis_vram_limit << 20;
	uint64_t limit = (uint64_t)amdgpu_vram_limit << 20;

	mc->vram_start = base;
	mc->vram_end = mc->vram_start + mc->mc_vram_size - 1;
	if (limit < mc->real_vram_size)
		mc->real_vram_size = limit;

	if (vis_limit && vis_limit < mc->visible_vram_size)
		mc->visible_vram_size = vis_limit;

	if (mc->real_vram_size < mc->visible_vram_size)
		mc->visible_vram_size = mc->real_vram_size;

	if (mc->xgmi.num_physical_nodes == 0) {
		mc->fb_start = mc->vram_start;
		mc->fb_end = mc->vram_end;
	}
	dev_info(adev->dev, "VRAM: %lluM 0x%016llX - 0x%016llX (%lluM used)\n",
			mc->mc_vram_size >> 20, mc->vram_start,
			mc->vram_end, mc->real_vram_size >> 20);
}

 
void amdgpu_gmc_sysvm_location(struct amdgpu_device *adev, struct amdgpu_gmc *mc)
{
	u64 hive_vram_start = 0;
	u64 hive_vram_end = mc->xgmi.node_segment_size * mc->xgmi.num_physical_nodes - 1;
	mc->vram_start = mc->xgmi.node_segment_size * mc->xgmi.physical_node_id;
	mc->vram_end = mc->vram_start + mc->xgmi.node_segment_size - 1;
	mc->gart_start = hive_vram_end + 1;
	mc->gart_end = mc->gart_start + mc->gart_size - 1;
	mc->fb_start = hive_vram_start;
	mc->fb_end = hive_vram_end;
	dev_info(adev->dev, "VRAM: %lluM 0x%016llX - 0x%016llX (%lluM used)\n",
			mc->mc_vram_size >> 20, mc->vram_start,
			mc->vram_end, mc->real_vram_size >> 20);
	dev_info(adev->dev, "GART: %lluM 0x%016llX - 0x%016llX\n",
			mc->gart_size >> 20, mc->gart_start, mc->gart_end);
}

 
void amdgpu_gmc_gart_location(struct amdgpu_device *adev, struct amdgpu_gmc *mc)
{
	const uint64_t four_gb = 0x100000000ULL;
	u64 size_af, size_bf;
	 
	u64 max_mc_address = min(adev->gmc.mc_mask, AMDGPU_GMC_HOLE_START - 1);

	 
	size_bf = mc->fb_start;
	size_af = max_mc_address + 1 - ALIGN(mc->fb_end + 1, four_gb);

	if (mc->gart_size > max(size_bf, size_af)) {
		dev_warn(adev->dev, "limiting GART\n");
		mc->gart_size = max(size_bf, size_af);
	}

	if ((size_bf >= mc->gart_size && size_bf < size_af) ||
	    (size_af < mc->gart_size))
		mc->gart_start = 0;
	else
		mc->gart_start = max_mc_address - mc->gart_size + 1;

	mc->gart_start &= ~(four_gb - 1);
	mc->gart_end = mc->gart_start + mc->gart_size - 1;
	dev_info(adev->dev, "GART: %lluM 0x%016llX - 0x%016llX\n",
			mc->gart_size >> 20, mc->gart_start, mc->gart_end);
}

 
void amdgpu_gmc_agp_location(struct amdgpu_device *adev, struct amdgpu_gmc *mc)
{
	const uint64_t sixteen_gb = 1ULL << 34;
	const uint64_t sixteen_gb_mask = ~(sixteen_gb - 1);
	u64 size_af, size_bf;

	if (amdgpu_sriov_vf(adev)) {
		mc->agp_start = 0xffffffffffff;
		mc->agp_end = 0x0;
		mc->agp_size = 0;

		return;
	}

	if (mc->fb_start > mc->gart_start) {
		size_bf = (mc->fb_start & sixteen_gb_mask) -
			ALIGN(mc->gart_end + 1, sixteen_gb);
		size_af = mc->mc_mask + 1 - ALIGN(mc->fb_end + 1, sixteen_gb);
	} else {
		size_bf = mc->fb_start & sixteen_gb_mask;
		size_af = (mc->gart_start & sixteen_gb_mask) -
			ALIGN(mc->fb_end + 1, sixteen_gb);
	}

	if (size_bf > size_af) {
		mc->agp_start = (mc->fb_start - size_bf) & sixteen_gb_mask;
		mc->agp_size = size_bf;
	} else {
		mc->agp_start = ALIGN(mc->fb_end + 1, sixteen_gb);
		mc->agp_size = size_af;
	}

	mc->agp_end = mc->agp_start + mc->agp_size - 1;
	dev_info(adev->dev, "AGP: %lluM 0x%016llX - 0x%016llX\n",
			mc->agp_size >> 20, mc->agp_start, mc->agp_end);
}

 
static inline uint64_t amdgpu_gmc_fault_key(uint64_t addr, uint16_t pasid)
{
	return addr << 4 | pasid;
}

 
bool amdgpu_gmc_filter_faults(struct amdgpu_device *adev,
			      struct amdgpu_ih_ring *ih, uint64_t addr,
			      uint16_t pasid, uint64_t timestamp)
{
	struct amdgpu_gmc *gmc = &adev->gmc;
	uint64_t stamp, key = amdgpu_gmc_fault_key(addr, pasid);
	struct amdgpu_gmc_fault *fault;
	uint32_t hash;

	 
	if (amdgpu_ih_ts_after(timestamp, ih->processed_timestamp))
		return true;

	 
	stamp = max(timestamp, AMDGPU_GMC_FAULT_TIMEOUT + 1) -
		AMDGPU_GMC_FAULT_TIMEOUT;
	if (gmc->fault_ring[gmc->last_fault].timestamp >= stamp)
		return true;

	 
	hash = hash_64(key, AMDGPU_GMC_FAULT_HASH_ORDER);
	fault = &gmc->fault_ring[gmc->fault_hash[hash].idx];
	while (fault->timestamp >= stamp) {
		uint64_t tmp;

		if (atomic64_read(&fault->key) == key) {
			 
			if (fault->timestamp_expiry != 0 &&
			    amdgpu_ih_ts_after(fault->timestamp_expiry,
					       timestamp))
				break;
			else
				return true;
		}

		tmp = fault->timestamp;
		fault = &gmc->fault_ring[fault->next];

		 
		if (fault->timestamp >= tmp)
			break;
	}

	 
	fault = &gmc->fault_ring[gmc->last_fault];
	atomic64_set(&fault->key, key);
	fault->timestamp = timestamp;

	 
	fault->next = gmc->fault_hash[hash].idx;
	gmc->fault_hash[hash].idx = gmc->last_fault++;
	return false;
}

 
void amdgpu_gmc_filter_faults_remove(struct amdgpu_device *adev, uint64_t addr,
				     uint16_t pasid)
{
	struct amdgpu_gmc *gmc = &adev->gmc;
	uint64_t key = amdgpu_gmc_fault_key(addr, pasid);
	struct amdgpu_ih_ring *ih;
	struct amdgpu_gmc_fault *fault;
	uint32_t last_wptr;
	uint64_t last_ts;
	uint32_t hash;
	uint64_t tmp;

	ih = adev->irq.retry_cam_enabled ? &adev->irq.ih_soft : &adev->irq.ih1;
	 
	last_wptr = amdgpu_ih_get_wptr(adev, ih);
	 
	rmb();
	 
	last_ts = amdgpu_ih_decode_iv_ts(adev, ih, last_wptr, -1);

	hash = hash_64(key, AMDGPU_GMC_FAULT_HASH_ORDER);
	fault = &gmc->fault_ring[gmc->fault_hash[hash].idx];
	do {
		if (atomic64_read(&fault->key) == key) {
			 
			fault->timestamp_expiry = last_ts;
			break;
		}

		tmp = fault->timestamp;
		fault = &gmc->fault_ring[fault->next];
	} while (fault->timestamp < tmp);
}

int amdgpu_gmc_ras_sw_init(struct amdgpu_device *adev)
{
	int r;

	 
	r = amdgpu_umc_ras_sw_init(adev);
	if (r)
		return r;

	 
	r = amdgpu_mmhub_ras_sw_init(adev);
	if (r)
		return r;

	 
	r = amdgpu_hdp_ras_sw_init(adev);
	if (r)
		return r;

	 
	r = amdgpu_mca_mp0_ras_sw_init(adev);
	if (r)
		return r;

	r = amdgpu_mca_mp1_ras_sw_init(adev);
	if (r)
		return r;

	r = amdgpu_mca_mpio_ras_sw_init(adev);
	if (r)
		return r;

	 
	r = amdgpu_xgmi_ras_sw_init(adev);
	if (r)
		return r;

	return 0;
}

int amdgpu_gmc_ras_late_init(struct amdgpu_device *adev)
{
	return 0;
}

void amdgpu_gmc_ras_fini(struct amdgpu_device *adev)
{

}

	 
#define AMDGPU_VMHUB_INV_ENG_BITMAP		0x1FFF3

int amdgpu_gmc_allocate_vm_inv_eng(struct amdgpu_device *adev)
{
	struct amdgpu_ring *ring;
	unsigned vm_inv_engs[AMDGPU_MAX_VMHUBS] = {0};
	unsigned i;
	unsigned vmhub, inv_eng;

	 
	for_each_set_bit(i, adev->vmhubs_mask, AMDGPU_MAX_VMHUBS) {
		vm_inv_engs[i] = AMDGPU_VMHUB_INV_ENG_BITMAP;
		 
		if (adev->enable_mes)
			vm_inv_engs[i] &= ~(1 << 5);
	}

	for (i = 0; i < adev->num_rings; ++i) {
		ring = adev->rings[i];
		vmhub = ring->vm_hub;

		if (ring == &adev->mes.ring)
			continue;

		inv_eng = ffs(vm_inv_engs[vmhub]);
		if (!inv_eng) {
			dev_err(adev->dev, "no VM inv eng for ring %s\n",
				ring->name);
			return -EINVAL;
		}

		ring->vm_inv_eng = inv_eng - 1;
		vm_inv_engs[vmhub] &= ~(1 << ring->vm_inv_eng);

		dev_info(adev->dev, "ring %s uses VM inv eng %u on hub %u\n",
			 ring->name, ring->vm_inv_eng, ring->vm_hub);
	}

	return 0;
}

 
void amdgpu_gmc_tmz_set(struct amdgpu_device *adev)
{
	switch (adev->ip_versions[GC_HWIP][0]) {
	 
	case IP_VERSION(9, 2, 2):
	case IP_VERSION(9, 1, 0):
	 
	case IP_VERSION(9, 3, 0):
	 
	case IP_VERSION(10, 3, 7):
	 
	case IP_VERSION(11, 0, 1):
		if (amdgpu_tmz == 0) {
			adev->gmc.tmz_enabled = false;
			dev_info(adev->dev,
				 "Trusted Memory Zone (TMZ) feature disabled (cmd line)\n");
		} else {
			adev->gmc.tmz_enabled = true;
			dev_info(adev->dev,
				 "Trusted Memory Zone (TMZ) feature enabled\n");
		}
		break;
	case IP_VERSION(10, 1, 10):
	case IP_VERSION(10, 1, 1):
	case IP_VERSION(10, 1, 2):
	case IP_VERSION(10, 1, 3):
	case IP_VERSION(10, 3, 0):
	case IP_VERSION(10, 3, 2):
	case IP_VERSION(10, 3, 4):
	case IP_VERSION(10, 3, 5):
	case IP_VERSION(10, 3, 6):
	 
	case IP_VERSION(10, 3, 1):
	 
	case IP_VERSION(10, 3, 3):
	case IP_VERSION(11, 0, 4):
		 
		if (amdgpu_tmz < 1) {
			adev->gmc.tmz_enabled = false;
			dev_info(adev->dev,
				 "Trusted Memory Zone (TMZ) feature disabled as experimental (default)\n");
		} else {
			adev->gmc.tmz_enabled = true;
			dev_info(adev->dev,
				 "Trusted Memory Zone (TMZ) feature enabled as experimental (cmd line)\n");
		}
		break;
	default:
		adev->gmc.tmz_enabled = false;
		dev_info(adev->dev,
			 "Trusted Memory Zone (TMZ) feature not supported\n");
		break;
	}
}

 
void amdgpu_gmc_noretry_set(struct amdgpu_device *adev)
{
	struct amdgpu_gmc *gmc = &adev->gmc;
	uint32_t gc_ver = adev->ip_versions[GC_HWIP][0];
	bool noretry_default = (gc_ver == IP_VERSION(9, 0, 1) ||
				gc_ver == IP_VERSION(9, 3, 0) ||
				gc_ver == IP_VERSION(9, 4, 0) ||
				gc_ver == IP_VERSION(9, 4, 1) ||
				gc_ver == IP_VERSION(9, 4, 2) ||
				gc_ver == IP_VERSION(9, 4, 3) ||
				gc_ver >= IP_VERSION(10, 3, 0));

	gmc->noretry = (amdgpu_noretry == -1) ? noretry_default : amdgpu_noretry;
}

void amdgpu_gmc_set_vm_fault_masks(struct amdgpu_device *adev, int hub_type,
				   bool enable)
{
	struct amdgpu_vmhub *hub;
	u32 tmp, reg, i;

	hub = &adev->vmhub[hub_type];
	for (i = 0; i < 16; i++) {
		reg = hub->vm_context0_cntl + hub->ctx_distance * i;

		tmp = (hub_type == AMDGPU_GFXHUB(0)) ?
			RREG32_SOC15_IP(GC, reg) :
			RREG32_SOC15_IP(MMHUB, reg);

		if (enable)
			tmp |= hub->vm_cntx_cntl_vm_fault;
		else
			tmp &= ~hub->vm_cntx_cntl_vm_fault;

		(hub_type == AMDGPU_GFXHUB(0)) ?
			WREG32_SOC15_IP(GC, reg, tmp) :
			WREG32_SOC15_IP(MMHUB, reg, tmp);
	}
}

void amdgpu_gmc_get_vbios_allocations(struct amdgpu_device *adev)
{
	unsigned size;

	 
	adev->mman.stolen_reserved_offset = 0;
	adev->mman.stolen_reserved_size = 0;

	 
	switch (adev->asic_type) {
	case CHIP_VEGA10:
		adev->mman.keep_stolen_vga_memory = true;
		 
#ifdef CONFIG_X86
		if (amdgpu_sriov_vf(adev) && hypervisor_is_type(X86_HYPER_MS_HYPERV)) {
			adev->mman.stolen_reserved_offset = 0x500000;
			adev->mman.stolen_reserved_size = 0x200000;
		}
#endif
		break;
	case CHIP_RAVEN:
	case CHIP_RENOIR:
		adev->mman.keep_stolen_vga_memory = true;
		break;
	case CHIP_YELLOW_CARP:
		if (amdgpu_discovery == 0) {
			adev->mman.stolen_reserved_offset = 0x1ffb0000;
			adev->mman.stolen_reserved_size = 64 * PAGE_SIZE;
		}
		break;
	default:
		adev->mman.keep_stolen_vga_memory = false;
		break;
	}

	if (amdgpu_sriov_vf(adev) ||
	    !amdgpu_device_has_display_hardware(adev)) {
		size = 0;
	} else {
		size = amdgpu_gmc_get_vbios_fb_size(adev);

		if (adev->mman.keep_stolen_vga_memory)
			size = max(size, (unsigned)AMDGPU_VBIOS_VGA_ALLOCATION);
	}

	 
	if ((adev->gmc.real_vram_size - size) < (8 * 1024 * 1024))
		size = 0;

	if (size > AMDGPU_VBIOS_VGA_ALLOCATION) {
		adev->mman.stolen_vga_size = AMDGPU_VBIOS_VGA_ALLOCATION;
		adev->mman.stolen_extended_size = size - adev->mman.stolen_vga_size;
	} else {
		adev->mman.stolen_vga_size = size;
		adev->mman.stolen_extended_size = 0;
	}
}

 
void amdgpu_gmc_init_pdb0(struct amdgpu_device *adev)
{
	int i;
	uint64_t flags = adev->gart.gart_pte_flags; 
	 
	u64 vram_size = adev->gmc.xgmi.node_segment_size * adev->gmc.xgmi.num_physical_nodes;
	u64 pde0_page_size = (1ULL<<adev->gmc.vmid0_page_table_block_size)<<21;
	u64 vram_addr = adev->vm_manager.vram_base_offset -
		adev->gmc.xgmi.physical_node_id * adev->gmc.xgmi.node_segment_size;
	u64 vram_end = vram_addr + vram_size;
	u64 gart_ptb_gpu_pa = amdgpu_gmc_vram_pa(adev, adev->gart.bo);
	int idx;

	if (!drm_dev_enter(adev_to_drm(adev), &idx))
		return;

	flags |= AMDGPU_PTE_VALID | AMDGPU_PTE_READABLE;
	flags |= AMDGPU_PTE_WRITEABLE;
	flags |= AMDGPU_PTE_SNOOPED;
	flags |= AMDGPU_PTE_FRAG((adev->gmc.vmid0_page_table_block_size + 9*1));
	flags |= AMDGPU_PDE_PTE;

	 
	for (i = 0; vram_addr < vram_end; i++, vram_addr += pde0_page_size)
		amdgpu_gmc_set_pte_pde(adev, adev->gmc.ptr_pdb0, i, vram_addr, flags);

	 
	flags = AMDGPU_PTE_VALID;
	flags |= AMDGPU_PDE_BFS(0) | AMDGPU_PTE_SNOOPED;
	 
	amdgpu_gmc_set_pte_pde(adev, adev->gmc.ptr_pdb0, i, gart_ptb_gpu_pa, flags);
	drm_dev_exit(idx);
}

 
uint64_t amdgpu_gmc_vram_mc2pa(struct amdgpu_device *adev, uint64_t mc_addr)
{
	return mc_addr - adev->gmc.vram_start + adev->vm_manager.vram_base_offset;
}

 
uint64_t amdgpu_gmc_vram_pa(struct amdgpu_device *adev, struct amdgpu_bo *bo)
{
	return amdgpu_gmc_vram_mc2pa(adev, amdgpu_bo_gpu_offset(bo));
}

 
uint64_t amdgpu_gmc_vram_cpu_pa(struct amdgpu_device *adev, struct amdgpu_bo *bo)
{
	return amdgpu_bo_gpu_offset(bo) - adev->gmc.vram_start + adev->gmc.aper_base;
}

int amdgpu_gmc_vram_checking(struct amdgpu_device *adev)
{
	struct amdgpu_bo *vram_bo = NULL;
	uint64_t vram_gpu = 0;
	void *vram_ptr = NULL;

	int ret, size = 0x100000;
	uint8_t cptr[10];

	ret = amdgpu_bo_create_kernel(adev, size, PAGE_SIZE,
				AMDGPU_GEM_DOMAIN_VRAM,
				&vram_bo,
				&vram_gpu,
				&vram_ptr);
	if (ret)
		return ret;

	memset(vram_ptr, 0x86, size);
	memset(cptr, 0x86, 10);

	 
	ret = memcmp(vram_ptr, cptr, 10);
	if (ret)
		return ret;

	ret = memcmp(vram_ptr + (size / 2), cptr, 10);
	if (ret)
		return ret;

	ret = memcmp(vram_ptr + size - 10, cptr, 10);
	if (ret)
		return ret;

	amdgpu_bo_free_kernel(&vram_bo, &vram_gpu,
			&vram_ptr);

	return 0;
}

static ssize_t current_memory_partition_show(
	struct device *dev, struct device_attribute *addr, char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	enum amdgpu_memory_partition mode;

	mode = adev->gmc.gmc_funcs->query_mem_partition_mode(adev);
	switch (mode) {
	case AMDGPU_NPS1_PARTITION_MODE:
		return sysfs_emit(buf, "NPS1\n");
	case AMDGPU_NPS2_PARTITION_MODE:
		return sysfs_emit(buf, "NPS2\n");
	case AMDGPU_NPS3_PARTITION_MODE:
		return sysfs_emit(buf, "NPS3\n");
	case AMDGPU_NPS4_PARTITION_MODE:
		return sysfs_emit(buf, "NPS4\n");
	case AMDGPU_NPS6_PARTITION_MODE:
		return sysfs_emit(buf, "NPS6\n");
	case AMDGPU_NPS8_PARTITION_MODE:
		return sysfs_emit(buf, "NPS8\n");
	default:
		return sysfs_emit(buf, "UNKNOWN\n");
	}

	return sysfs_emit(buf, "UNKNOWN\n");
}

static DEVICE_ATTR_RO(current_memory_partition);

int amdgpu_gmc_sysfs_init(struct amdgpu_device *adev)
{
	if (!adev->gmc.gmc_funcs->query_mem_partition_mode)
		return 0;

	return device_create_file(adev->dev,
				  &dev_attr_current_memory_partition);
}

void amdgpu_gmc_sysfs_fini(struct amdgpu_device *adev)
{
	device_remove_file(adev->dev, &dev_attr_current_memory_partition);
}
