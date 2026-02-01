 

#include <linux/firmware.h>
#include <linux/module.h>

#include <drm/drm.h>
#include <drm/drm_drv.h>

#include "amdgpu.h"
#include "amdgpu_pm.h"
#include "amdgpu_vce.h"
#include "amdgpu_cs.h"
#include "cikd.h"

 
#define VCE_IDLE_TIMEOUT	msecs_to_jiffies(1000)

 
#ifdef CONFIG_DRM_AMDGPU_CIK
#define FIRMWARE_BONAIRE	"amdgpu/bonaire_vce.bin"
#define FIRMWARE_KABINI	"amdgpu/kabini_vce.bin"
#define FIRMWARE_KAVERI	"amdgpu/kaveri_vce.bin"
#define FIRMWARE_HAWAII	"amdgpu/hawaii_vce.bin"
#define FIRMWARE_MULLINS	"amdgpu/mullins_vce.bin"
#endif
#define FIRMWARE_TONGA		"amdgpu/tonga_vce.bin"
#define FIRMWARE_CARRIZO	"amdgpu/carrizo_vce.bin"
#define FIRMWARE_FIJI		"amdgpu/fiji_vce.bin"
#define FIRMWARE_STONEY		"amdgpu/stoney_vce.bin"
#define FIRMWARE_POLARIS10	"amdgpu/polaris10_vce.bin"
#define FIRMWARE_POLARIS11	"amdgpu/polaris11_vce.bin"
#define FIRMWARE_POLARIS12	"amdgpu/polaris12_vce.bin"
#define FIRMWARE_VEGAM		"amdgpu/vegam_vce.bin"

#define FIRMWARE_VEGA10		"amdgpu/vega10_vce.bin"
#define FIRMWARE_VEGA12		"amdgpu/vega12_vce.bin"
#define FIRMWARE_VEGA20		"amdgpu/vega20_vce.bin"

#ifdef CONFIG_DRM_AMDGPU_CIK
MODULE_FIRMWARE(FIRMWARE_BONAIRE);
MODULE_FIRMWARE(FIRMWARE_KABINI);
MODULE_FIRMWARE(FIRMWARE_KAVERI);
MODULE_FIRMWARE(FIRMWARE_HAWAII);
MODULE_FIRMWARE(FIRMWARE_MULLINS);
#endif
MODULE_FIRMWARE(FIRMWARE_TONGA);
MODULE_FIRMWARE(FIRMWARE_CARRIZO);
MODULE_FIRMWARE(FIRMWARE_FIJI);
MODULE_FIRMWARE(FIRMWARE_STONEY);
MODULE_FIRMWARE(FIRMWARE_POLARIS10);
MODULE_FIRMWARE(FIRMWARE_POLARIS11);
MODULE_FIRMWARE(FIRMWARE_POLARIS12);
MODULE_FIRMWARE(FIRMWARE_VEGAM);

MODULE_FIRMWARE(FIRMWARE_VEGA10);
MODULE_FIRMWARE(FIRMWARE_VEGA12);
MODULE_FIRMWARE(FIRMWARE_VEGA20);

static void amdgpu_vce_idle_work_handler(struct work_struct *work);
static int amdgpu_vce_get_create_msg(struct amdgpu_ring *ring, uint32_t handle,
				     struct dma_fence **fence);
static int amdgpu_vce_get_destroy_msg(struct amdgpu_ring *ring, uint32_t handle,
				      bool direct, struct dma_fence **fence);

 
int amdgpu_vce_sw_init(struct amdgpu_device *adev, unsigned long size)
{
	const char *fw_name;
	const struct common_firmware_header *hdr;
	unsigned int ucode_version, version_major, version_minor, binary_id;
	int i, r;

	switch (adev->asic_type) {
#ifdef CONFIG_DRM_AMDGPU_CIK
	case CHIP_BONAIRE:
		fw_name = FIRMWARE_BONAIRE;
		break;
	case CHIP_KAVERI:
		fw_name = FIRMWARE_KAVERI;
		break;
	case CHIP_KABINI:
		fw_name = FIRMWARE_KABINI;
		break;
	case CHIP_HAWAII:
		fw_name = FIRMWARE_HAWAII;
		break;
	case CHIP_MULLINS:
		fw_name = FIRMWARE_MULLINS;
		break;
#endif
	case CHIP_TONGA:
		fw_name = FIRMWARE_TONGA;
		break;
	case CHIP_CARRIZO:
		fw_name = FIRMWARE_CARRIZO;
		break;
	case CHIP_FIJI:
		fw_name = FIRMWARE_FIJI;
		break;
	case CHIP_STONEY:
		fw_name = FIRMWARE_STONEY;
		break;
	case CHIP_POLARIS10:
		fw_name = FIRMWARE_POLARIS10;
		break;
	case CHIP_POLARIS11:
		fw_name = FIRMWARE_POLARIS11;
		break;
	case CHIP_POLARIS12:
		fw_name = FIRMWARE_POLARIS12;
		break;
	case CHIP_VEGAM:
		fw_name = FIRMWARE_VEGAM;
		break;
	case CHIP_VEGA10:
		fw_name = FIRMWARE_VEGA10;
		break;
	case CHIP_VEGA12:
		fw_name = FIRMWARE_VEGA12;
		break;
	case CHIP_VEGA20:
		fw_name = FIRMWARE_VEGA20;
		break;

	default:
		return -EINVAL;
	}

	r = amdgpu_ucode_request(adev, &adev->vce.fw, fw_name);
	if (r) {
		dev_err(adev->dev, "amdgpu_vce: Can't validate firmware \"%s\"\n",
			fw_name);
		amdgpu_ucode_release(&adev->vce.fw);
		return r;
	}

	hdr = (const struct common_firmware_header *)adev->vce.fw->data;

	ucode_version = le32_to_cpu(hdr->ucode_version);
	version_major = (ucode_version >> 20) & 0xfff;
	version_minor = (ucode_version >> 8) & 0xfff;
	binary_id = ucode_version & 0xff;
	DRM_INFO("Found VCE firmware Version: %d.%d Binary ID: %d\n",
		version_major, version_minor, binary_id);
	adev->vce.fw_version = ((version_major << 24) | (version_minor << 16) |
				(binary_id << 8));

	r = amdgpu_bo_create_kernel(adev, size, PAGE_SIZE,
				    AMDGPU_GEM_DOMAIN_VRAM |
				    AMDGPU_GEM_DOMAIN_GTT,
				    &adev->vce.vcpu_bo,
				    &adev->vce.gpu_addr, &adev->vce.cpu_addr);
	if (r) {
		dev_err(adev->dev, "(%d) failed to allocate VCE bo\n", r);
		return r;
	}

	for (i = 0; i < AMDGPU_MAX_VCE_HANDLES; ++i) {
		atomic_set(&adev->vce.handles[i], 0);
		adev->vce.filp[i] = NULL;
	}

	INIT_DELAYED_WORK(&adev->vce.idle_work, amdgpu_vce_idle_work_handler);
	mutex_init(&adev->vce.idle_mutex);

	return 0;
}

 
int amdgpu_vce_sw_fini(struct amdgpu_device *adev)
{
	unsigned int i;

	if (adev->vce.vcpu_bo == NULL)
		return 0;

	drm_sched_entity_destroy(&adev->vce.entity);

	amdgpu_bo_free_kernel(&adev->vce.vcpu_bo, &adev->vce.gpu_addr,
		(void **)&adev->vce.cpu_addr);

	for (i = 0; i < adev->vce.num_rings; i++)
		amdgpu_ring_fini(&adev->vce.ring[i]);

	amdgpu_ucode_release(&adev->vce.fw);
	mutex_destroy(&adev->vce.idle_mutex);

	return 0;
}

 
int amdgpu_vce_entity_init(struct amdgpu_device *adev)
{
	struct amdgpu_ring *ring;
	struct drm_gpu_scheduler *sched;
	int r;

	ring = &adev->vce.ring[0];
	sched = &ring->sched;
	r = drm_sched_entity_init(&adev->vce.entity, DRM_SCHED_PRIORITY_NORMAL,
				  &sched, 1, NULL);
	if (r != 0) {
		DRM_ERROR("Failed setting up VCE run queue.\n");
		return r;
	}

	return 0;
}

 
int amdgpu_vce_suspend(struct amdgpu_device *adev)
{
	int i;

	cancel_delayed_work_sync(&adev->vce.idle_work);

	if (adev->vce.vcpu_bo == NULL)
		return 0;

	for (i = 0; i < AMDGPU_MAX_VCE_HANDLES; ++i)
		if (atomic_read(&adev->vce.handles[i]))
			break;

	if (i == AMDGPU_MAX_VCE_HANDLES)
		return 0;

	 
	return -EINVAL;
}

 
int amdgpu_vce_resume(struct amdgpu_device *adev)
{
	void *cpu_addr;
	const struct common_firmware_header *hdr;
	unsigned int offset;
	int r, idx;

	if (adev->vce.vcpu_bo == NULL)
		return -EINVAL;

	r = amdgpu_bo_reserve(adev->vce.vcpu_bo, false);
	if (r) {
		dev_err(adev->dev, "(%d) failed to reserve VCE bo\n", r);
		return r;
	}

	r = amdgpu_bo_kmap(adev->vce.vcpu_bo, &cpu_addr);
	if (r) {
		amdgpu_bo_unreserve(adev->vce.vcpu_bo);
		dev_err(adev->dev, "(%d) VCE map failed\n", r);
		return r;
	}

	hdr = (const struct common_firmware_header *)adev->vce.fw->data;
	offset = le32_to_cpu(hdr->ucode_array_offset_bytes);

	if (drm_dev_enter(adev_to_drm(adev), &idx)) {
		memcpy_toio(cpu_addr, adev->vce.fw->data + offset,
			    adev->vce.fw->size - offset);
		drm_dev_exit(idx);
	}

	amdgpu_bo_kunmap(adev->vce.vcpu_bo);

	amdgpu_bo_unreserve(adev->vce.vcpu_bo);

	return 0;
}

 
static void amdgpu_vce_idle_work_handler(struct work_struct *work)
{
	struct amdgpu_device *adev =
		container_of(work, struct amdgpu_device, vce.idle_work.work);
	unsigned int i, count = 0;

	for (i = 0; i < adev->vce.num_rings; i++)
		count += amdgpu_fence_count_emitted(&adev->vce.ring[i]);

	if (count == 0) {
		if (adev->pm.dpm_enabled) {
			amdgpu_dpm_enable_vce(adev, false);
		} else {
			amdgpu_asic_set_vce_clocks(adev, 0, 0);
			amdgpu_device_ip_set_powergating_state(adev, AMD_IP_BLOCK_TYPE_VCE,
							       AMD_PG_STATE_GATE);
			amdgpu_device_ip_set_clockgating_state(adev, AMD_IP_BLOCK_TYPE_VCE,
							       AMD_CG_STATE_GATE);
		}
	} else {
		schedule_delayed_work(&adev->vce.idle_work, VCE_IDLE_TIMEOUT);
	}
}

 
void amdgpu_vce_ring_begin_use(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;
	bool set_clocks;

	if (amdgpu_sriov_vf(adev))
		return;

	mutex_lock(&adev->vce.idle_mutex);
	set_clocks = !cancel_delayed_work_sync(&adev->vce.idle_work);
	if (set_clocks) {
		if (adev->pm.dpm_enabled) {
			amdgpu_dpm_enable_vce(adev, true);
		} else {
			amdgpu_asic_set_vce_clocks(adev, 53300, 40000);
			amdgpu_device_ip_set_clockgating_state(adev, AMD_IP_BLOCK_TYPE_VCE,
							       AMD_CG_STATE_UNGATE);
			amdgpu_device_ip_set_powergating_state(adev, AMD_IP_BLOCK_TYPE_VCE,
							       AMD_PG_STATE_UNGATE);

		}
	}
	mutex_unlock(&adev->vce.idle_mutex);
}

 
void amdgpu_vce_ring_end_use(struct amdgpu_ring *ring)
{
	if (!amdgpu_sriov_vf(ring->adev))
		schedule_delayed_work(&ring->adev->vce.idle_work, VCE_IDLE_TIMEOUT);
}

 
void amdgpu_vce_free_handles(struct amdgpu_device *adev, struct drm_file *filp)
{
	struct amdgpu_ring *ring = &adev->vce.ring[0];
	int i, r;

	for (i = 0; i < AMDGPU_MAX_VCE_HANDLES; ++i) {
		uint32_t handle = atomic_read(&adev->vce.handles[i]);

		if (!handle || adev->vce.filp[i] != filp)
			continue;

		r = amdgpu_vce_get_destroy_msg(ring, handle, false, NULL);
		if (r)
			DRM_ERROR("Error destroying VCE handle (%d)!\n", r);

		adev->vce.filp[i] = NULL;
		atomic_set(&adev->vce.handles[i], 0);
	}
}

 
static int amdgpu_vce_get_create_msg(struct amdgpu_ring *ring, uint32_t handle,
				     struct dma_fence **fence)
{
	const unsigned int ib_size_dw = 1024;
	struct amdgpu_job *job;
	struct amdgpu_ib *ib;
	struct amdgpu_ib ib_msg;
	struct dma_fence *f = NULL;
	uint64_t addr;
	int i, r;

	r = amdgpu_job_alloc_with_ib(ring->adev, &ring->adev->vce.entity,
				     AMDGPU_FENCE_OWNER_UNDEFINED,
				     ib_size_dw * 4, AMDGPU_IB_POOL_DIRECT,
				     &job);
	if (r)
		return r;

	memset(&ib_msg, 0, sizeof(ib_msg));
	 
	r = amdgpu_ib_get(ring->adev, NULL, AMDGPU_GPU_PAGE_SIZE * 2,
			  AMDGPU_IB_POOL_DIRECT,
			  &ib_msg);
	if (r)
		goto err;

	ib = &job->ibs[0];
	 
	addr = AMDGPU_GPU_PAGE_ALIGN(ib_msg.gpu_addr);

	 
	ib->length_dw = 0;
	ib->ptr[ib->length_dw++] = 0x0000000c;  
	ib->ptr[ib->length_dw++] = 0x00000001;  
	ib->ptr[ib->length_dw++] = handle;

	if ((ring->adev->vce.fw_version >> 24) >= 52)
		ib->ptr[ib->length_dw++] = 0x00000040;  
	else
		ib->ptr[ib->length_dw++] = 0x00000030;  
	ib->ptr[ib->length_dw++] = 0x01000001;  
	ib->ptr[ib->length_dw++] = 0x00000000;
	ib->ptr[ib->length_dw++] = 0x00000042;
	ib->ptr[ib->length_dw++] = 0x0000000a;
	ib->ptr[ib->length_dw++] = 0x00000001;
	ib->ptr[ib->length_dw++] = 0x00000080;
	ib->ptr[ib->length_dw++] = 0x00000060;
	ib->ptr[ib->length_dw++] = 0x00000100;
	ib->ptr[ib->length_dw++] = 0x00000100;
	ib->ptr[ib->length_dw++] = 0x0000000c;
	ib->ptr[ib->length_dw++] = 0x00000000;
	if ((ring->adev->vce.fw_version >> 24) >= 52) {
		ib->ptr[ib->length_dw++] = 0x00000000;
		ib->ptr[ib->length_dw++] = 0x00000000;
		ib->ptr[ib->length_dw++] = 0x00000000;
		ib->ptr[ib->length_dw++] = 0x00000000;
	}

	ib->ptr[ib->length_dw++] = 0x00000014;  
	ib->ptr[ib->length_dw++] = 0x05000005;  
	ib->ptr[ib->length_dw++] = upper_32_bits(addr);
	ib->ptr[ib->length_dw++] = addr;
	ib->ptr[ib->length_dw++] = 0x00000001;

	for (i = ib->length_dw; i < ib_size_dw; ++i)
		ib->ptr[i] = 0x0;

	r = amdgpu_job_submit_direct(job, ring, &f);
	amdgpu_ib_free(ring->adev, &ib_msg, f);
	if (r)
		goto err;

	if (fence)
		*fence = dma_fence_get(f);
	dma_fence_put(f);
	return 0;

err:
	amdgpu_job_free(job);
	return r;
}

 
static int amdgpu_vce_get_destroy_msg(struct amdgpu_ring *ring, uint32_t handle,
				      bool direct, struct dma_fence **fence)
{
	const unsigned int ib_size_dw = 1024;
	struct amdgpu_job *job;
	struct amdgpu_ib *ib;
	struct dma_fence *f = NULL;
	int i, r;

	r = amdgpu_job_alloc_with_ib(ring->adev, &ring->adev->vce.entity,
				     AMDGPU_FENCE_OWNER_UNDEFINED,
				     ib_size_dw * 4,
				     direct ? AMDGPU_IB_POOL_DIRECT :
				     AMDGPU_IB_POOL_DELAYED, &job);
	if (r)
		return r;

	ib = &job->ibs[0];

	 
	ib->length_dw = 0;
	ib->ptr[ib->length_dw++] = 0x0000000c;  
	ib->ptr[ib->length_dw++] = 0x00000001;  
	ib->ptr[ib->length_dw++] = handle;

	ib->ptr[ib->length_dw++] = 0x00000020;  
	ib->ptr[ib->length_dw++] = 0x00000002;  
	ib->ptr[ib->length_dw++] = 0xffffffff;  
	ib->ptr[ib->length_dw++] = 0x00000001;  
	ib->ptr[ib->length_dw++] = 0x00000000;
	ib->ptr[ib->length_dw++] = 0x00000000;
	ib->ptr[ib->length_dw++] = 0xffffffff;  
	ib->ptr[ib->length_dw++] = 0x00000000;

	ib->ptr[ib->length_dw++] = 0x00000008;  
	ib->ptr[ib->length_dw++] = 0x02000001;  

	for (i = ib->length_dw; i < ib_size_dw; ++i)
		ib->ptr[i] = 0x0;

	if (direct)
		r = amdgpu_job_submit_direct(job, ring, &f);
	else
		f = amdgpu_job_submit(job);
	if (r)
		goto err;

	if (fence)
		*fence = dma_fence_get(f);
	dma_fence_put(f);
	return 0;

err:
	amdgpu_job_free(job);
	return r;
}

 
static int amdgpu_vce_validate_bo(struct amdgpu_cs_parser *p,
				  struct amdgpu_ib *ib, int lo, int hi,
				  unsigned int size, int32_t index)
{
	int64_t offset = ((uint64_t)size) * ((int64_t)index);
	struct ttm_operation_ctx ctx = { false, false };
	struct amdgpu_bo_va_mapping *mapping;
	unsigned int i, fpfn, lpfn;
	struct amdgpu_bo *bo;
	uint64_t addr;
	int r;

	addr = ((uint64_t)amdgpu_ib_get_value(ib, lo)) |
	       ((uint64_t)amdgpu_ib_get_value(ib, hi)) << 32;
	if (index >= 0) {
		addr += offset;
		fpfn = PAGE_ALIGN(offset) >> PAGE_SHIFT;
		lpfn = 0x100000000ULL >> PAGE_SHIFT;
	} else {
		fpfn = 0;
		lpfn = (0x100000000ULL - PAGE_ALIGN(offset)) >> PAGE_SHIFT;
	}

	r = amdgpu_cs_find_mapping(p, addr, &bo, &mapping);
	if (r) {
		DRM_ERROR("Can't find BO for addr 0x%010llx %d %d %d %d\n",
			  addr, lo, hi, size, index);
		return r;
	}

	for (i = 0; i < bo->placement.num_placement; ++i) {
		bo->placements[i].fpfn = max(bo->placements[i].fpfn, fpfn);
		bo->placements[i].lpfn = bo->placements[i].lpfn ?
			min(bo->placements[i].lpfn, lpfn) : lpfn;
	}
	return ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
}


 
static int amdgpu_vce_cs_reloc(struct amdgpu_cs_parser *p, struct amdgpu_ib *ib,
			       int lo, int hi, unsigned int size, uint32_t index)
{
	struct amdgpu_bo_va_mapping *mapping;
	struct amdgpu_bo *bo;
	uint64_t addr;
	int r;

	if (index == 0xffffffff)
		index = 0;

	addr = ((uint64_t)amdgpu_ib_get_value(ib, lo)) |
	       ((uint64_t)amdgpu_ib_get_value(ib, hi)) << 32;
	addr += ((uint64_t)size) * ((uint64_t)index);

	r = amdgpu_cs_find_mapping(p, addr, &bo, &mapping);
	if (r) {
		DRM_ERROR("Can't find BO for addr 0x%010llx %d %d %d %d\n",
			  addr, lo, hi, size, index);
		return r;
	}

	if ((addr + (uint64_t)size) >
	    (mapping->last + 1) * AMDGPU_GPU_PAGE_SIZE) {
		DRM_ERROR("BO too small for addr 0x%010llx %d %d\n",
			  addr, lo, hi);
		return -EINVAL;
	}

	addr -= mapping->start * AMDGPU_GPU_PAGE_SIZE;
	addr += amdgpu_bo_gpu_offset(bo);
	addr -= ((uint64_t)size) * ((uint64_t)index);

	amdgpu_ib_set_value(ib, lo, lower_32_bits(addr));
	amdgpu_ib_set_value(ib, hi, upper_32_bits(addr));

	return 0;
}

 
static int amdgpu_vce_validate_handle(struct amdgpu_cs_parser *p,
				      uint32_t handle, uint32_t *allocated)
{
	unsigned int i;

	 
	for (i = 0; i < AMDGPU_MAX_VCE_HANDLES; ++i) {
		if (atomic_read(&p->adev->vce.handles[i]) == handle) {
			if (p->adev->vce.filp[i] != p->filp) {
				DRM_ERROR("VCE handle collision detected!\n");
				return -EINVAL;
			}
			return i;
		}
	}

	 
	for (i = 0; i < AMDGPU_MAX_VCE_HANDLES; ++i) {
		if (!atomic_cmpxchg(&p->adev->vce.handles[i], 0, handle)) {
			p->adev->vce.filp[i] = p->filp;
			p->adev->vce.img_size[i] = 0;
			*allocated |= 1 << i;
			return i;
		}
	}

	DRM_ERROR("No more free VCE handles!\n");
	return -EINVAL;
}

 
int amdgpu_vce_ring_parse_cs(struct amdgpu_cs_parser *p,
			     struct amdgpu_job *job,
			     struct amdgpu_ib *ib)
{
	unsigned int fb_idx = 0, bs_idx = 0;
	int session_idx = -1;
	uint32_t destroyed = 0;
	uint32_t created = 0;
	uint32_t allocated = 0;
	uint32_t tmp, handle = 0;
	uint32_t *size = &tmp;
	unsigned int idx;
	int i, r = 0;

	job->vm = NULL;
	ib->gpu_addr = amdgpu_sa_bo_gpu_addr(ib->sa_bo);

	for (idx = 0; idx < ib->length_dw;) {
		uint32_t len = amdgpu_ib_get_value(ib, idx);
		uint32_t cmd = amdgpu_ib_get_value(ib, idx + 1);

		if ((len < 8) || (len & 3)) {
			DRM_ERROR("invalid VCE command length (%d)!\n", len);
			r = -EINVAL;
			goto out;
		}

		switch (cmd) {
		case 0x00000002:  
			fb_idx = amdgpu_ib_get_value(ib, idx + 6);
			bs_idx = amdgpu_ib_get_value(ib, idx + 7);
			break;

		case 0x03000001:  
			r = amdgpu_vce_validate_bo(p, ib, idx + 10, idx + 9,
						   0, 0);
			if (r)
				goto out;

			r = amdgpu_vce_validate_bo(p, ib, idx + 12, idx + 11,
						   0, 0);
			if (r)
				goto out;
			break;

		case 0x05000001:  
			r = amdgpu_vce_validate_bo(p, ib, idx + 3, idx + 2,
						   0, 0);
			if (r)
				goto out;
			break;

		case 0x05000004:  
			tmp = amdgpu_ib_get_value(ib, idx + 4);
			r = amdgpu_vce_validate_bo(p, ib, idx + 3, idx + 2,
						   tmp, bs_idx);
			if (r)
				goto out;
			break;

		case 0x05000005:  
			r = amdgpu_vce_validate_bo(p, ib, idx + 3, idx + 2,
						   4096, fb_idx);
			if (r)
				goto out;
			break;

		case 0x0500000d:  
			r = amdgpu_vce_validate_bo(p, ib, idx + 3, idx + 2,
						   0, 0);
			if (r)
				goto out;

			r = amdgpu_vce_validate_bo(p, ib, idx + 8, idx + 7,
						   0, 0);
			if (r)
				goto out;
			break;
		}

		idx += len / 4;
	}

	for (idx = 0; idx < ib->length_dw;) {
		uint32_t len = amdgpu_ib_get_value(ib, idx);
		uint32_t cmd = amdgpu_ib_get_value(ib, idx + 1);

		switch (cmd) {
		case 0x00000001:  
			handle = amdgpu_ib_get_value(ib, idx + 2);
			session_idx = amdgpu_vce_validate_handle(p, handle,
								 &allocated);
			if (session_idx < 0) {
				r = session_idx;
				goto out;
			}
			size = &p->adev->vce.img_size[session_idx];
			break;

		case 0x00000002:  
			fb_idx = amdgpu_ib_get_value(ib, idx + 6);
			bs_idx = amdgpu_ib_get_value(ib, idx + 7);
			break;

		case 0x01000001:  
			created |= 1 << session_idx;
			if (destroyed & (1 << session_idx)) {
				destroyed &= ~(1 << session_idx);
				allocated |= 1 << session_idx;

			} else if (!(allocated & (1 << session_idx))) {
				DRM_ERROR("Handle already in use!\n");
				r = -EINVAL;
				goto out;
			}

			*size = amdgpu_ib_get_value(ib, idx + 8) *
				amdgpu_ib_get_value(ib, idx + 10) *
				8 * 3 / 2;
			break;

		case 0x04000001:  
		case 0x04000002:  
		case 0x04000005:  
		case 0x04000007:  
		case 0x04000008:  
		case 0x04000009:  
		case 0x05000002:  
		case 0x05000009:  
			break;

		case 0x0500000c:  
			switch (p->adev->asic_type) {
#ifdef CONFIG_DRM_AMDGPU_CIK
			case CHIP_KAVERI:
			case CHIP_MULLINS:
#endif
			case CHIP_CARRIZO:
				break;
			default:
				r = -EINVAL;
				goto out;
			}
			break;

		case 0x03000001:  
			r = amdgpu_vce_cs_reloc(p, ib, idx + 10, idx + 9,
						*size, 0);
			if (r)
				goto out;

			r = amdgpu_vce_cs_reloc(p, ib, idx + 12, idx + 11,
						*size / 3, 0);
			if (r)
				goto out;
			break;

		case 0x02000001:  
			destroyed |= 1 << session_idx;
			break;

		case 0x05000001:  
			r = amdgpu_vce_cs_reloc(p, ib, idx + 3, idx + 2,
						*size * 2, 0);
			if (r)
				goto out;
			break;

		case 0x05000004:  
			tmp = amdgpu_ib_get_value(ib, idx + 4);
			r = amdgpu_vce_cs_reloc(p, ib, idx + 3, idx + 2,
						tmp, bs_idx);
			if (r)
				goto out;
			break;

		case 0x05000005:  
			r = amdgpu_vce_cs_reloc(p, ib, idx + 3, idx + 2,
						4096, fb_idx);
			if (r)
				goto out;
			break;

		case 0x0500000d:  
			r = amdgpu_vce_cs_reloc(p, ib, idx + 3,
						idx + 2, *size, 0);
			if (r)
				goto out;

			r = amdgpu_vce_cs_reloc(p, ib, idx + 8,
						idx + 7, *size / 12, 0);
			if (r)
				goto out;
			break;

		default:
			DRM_ERROR("invalid VCE command (0x%x)!\n", cmd);
			r = -EINVAL;
			goto out;
		}

		if (session_idx == -1) {
			DRM_ERROR("no session command at start of IB\n");
			r = -EINVAL;
			goto out;
		}

		idx += len / 4;
	}

	if (allocated & ~created) {
		DRM_ERROR("New session without create command!\n");
		r = -ENOENT;
	}

out:
	if (!r) {
		 
		tmp = destroyed;
	} else {
		 
		tmp = allocated;
	}

	for (i = 0; i < AMDGPU_MAX_VCE_HANDLES; ++i)
		if (tmp & (1 << i))
			atomic_set(&p->adev->vce.handles[i], 0);

	return r;
}

 
int amdgpu_vce_ring_parse_cs_vm(struct amdgpu_cs_parser *p,
				struct amdgpu_job *job,
				struct amdgpu_ib *ib)
{
	int session_idx = -1;
	uint32_t destroyed = 0;
	uint32_t created = 0;
	uint32_t allocated = 0;
	uint32_t tmp, handle = 0;
	int i, r = 0, idx = 0;

	while (idx < ib->length_dw) {
		uint32_t len = amdgpu_ib_get_value(ib, idx);
		uint32_t cmd = amdgpu_ib_get_value(ib, idx + 1);

		if ((len < 8) || (len & 3)) {
			DRM_ERROR("invalid VCE command length (%d)!\n", len);
			r = -EINVAL;
			goto out;
		}

		switch (cmd) {
		case 0x00000001:  
			handle = amdgpu_ib_get_value(ib, idx + 2);
			session_idx = amdgpu_vce_validate_handle(p, handle,
								 &allocated);
			if (session_idx < 0) {
				r = session_idx;
				goto out;
			}
			break;

		case 0x01000001:  
			created |= 1 << session_idx;
			if (destroyed & (1 << session_idx)) {
				destroyed &= ~(1 << session_idx);
				allocated |= 1 << session_idx;

			} else if (!(allocated & (1 << session_idx))) {
				DRM_ERROR("Handle already in use!\n");
				r = -EINVAL;
				goto out;
			}

			break;

		case 0x02000001:  
			destroyed |= 1 << session_idx;
			break;

		default:
			break;
		}

		if (session_idx == -1) {
			DRM_ERROR("no session command at start of IB\n");
			r = -EINVAL;
			goto out;
		}

		idx += len / 4;
	}

	if (allocated & ~created) {
		DRM_ERROR("New session without create command!\n");
		r = -ENOENT;
	}

out:
	if (!r) {
		 
		tmp = destroyed;
		amdgpu_ib_free(p->adev, ib, NULL);
	} else {
		 
		tmp = allocated;
	}

	for (i = 0; i < AMDGPU_MAX_VCE_HANDLES; ++i)
		if (tmp & (1 << i))
			atomic_set(&p->adev->vce.handles[i], 0);

	return r;
}

 
void amdgpu_vce_ring_emit_ib(struct amdgpu_ring *ring,
				struct amdgpu_job *job,
				struct amdgpu_ib *ib,
				uint32_t flags)
{
	amdgpu_ring_write(ring, VCE_CMD_IB);
	amdgpu_ring_write(ring, lower_32_bits(ib->gpu_addr));
	amdgpu_ring_write(ring, upper_32_bits(ib->gpu_addr));
	amdgpu_ring_write(ring, ib->length_dw);
}

 
void amdgpu_vce_ring_emit_fence(struct amdgpu_ring *ring, u64 addr, u64 seq,
				unsigned int flags)
{
	WARN_ON(flags & AMDGPU_FENCE_FLAG_64BIT);

	amdgpu_ring_write(ring, VCE_CMD_FENCE);
	amdgpu_ring_write(ring, addr);
	amdgpu_ring_write(ring, upper_32_bits(addr));
	amdgpu_ring_write(ring, seq);
	amdgpu_ring_write(ring, VCE_CMD_TRAP);
	amdgpu_ring_write(ring, VCE_CMD_END);
}

 
int amdgpu_vce_ring_test_ring(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;
	uint32_t rptr;
	unsigned int i;
	int r, timeout = adev->usec_timeout;

	 
	if (amdgpu_sriov_vf(adev))
		return 0;

	r = amdgpu_ring_alloc(ring, 16);
	if (r)
		return r;

	rptr = amdgpu_ring_get_rptr(ring);

	amdgpu_ring_write(ring, VCE_CMD_END);
	amdgpu_ring_commit(ring);

	for (i = 0; i < timeout; i++) {
		if (amdgpu_ring_get_rptr(ring) != rptr)
			break;
		udelay(1);
	}

	if (i >= timeout)
		r = -ETIMEDOUT;

	return r;
}

 
int amdgpu_vce_ring_test_ib(struct amdgpu_ring *ring, long timeout)
{
	struct dma_fence *fence = NULL;
	long r;

	 
	if (ring != &ring->adev->vce.ring[0])
		return 0;

	r = amdgpu_vce_get_create_msg(ring, 1, NULL);
	if (r)
		goto error;

	r = amdgpu_vce_get_destroy_msg(ring, 1, true, &fence);
	if (r)
		goto error;

	r = dma_fence_wait_timeout(fence, false, timeout);
	if (r == 0)
		r = -ETIMEDOUT;
	else if (r > 0)
		r = 0;

error:
	dma_fence_put(fence);
	return r;
}

enum amdgpu_ring_priority_level amdgpu_vce_get_ring_prio(int ring)
{
	switch (ring) {
	case 0:
		return AMDGPU_RING_PRIO_0;
	case 1:
		return AMDGPU_RING_PRIO_1;
	case 2:
		return AMDGPU_RING_PRIO_2;
	default:
		return AMDGPU_RING_PRIO_0;
	}
}
