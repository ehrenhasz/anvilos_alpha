
 

#include "amdgpu.h"

 
u32 amdgpu_mm_rdoorbell(struct amdgpu_device *adev, u32 index)
{
	if (amdgpu_device_skip_hw_access(adev))
		return 0;

	if (index < adev->doorbell.num_kernel_doorbells)
		return readl(adev->doorbell.cpu_addr + index);

	DRM_ERROR("reading beyond doorbell aperture: 0x%08x!\n", index);
	return 0;
}

 
void amdgpu_mm_wdoorbell(struct amdgpu_device *adev, u32 index, u32 v)
{
	if (amdgpu_device_skip_hw_access(adev))
		return;

	if (index < adev->doorbell.num_kernel_doorbells)
		writel(v, adev->doorbell.cpu_addr + index);
	else
		DRM_ERROR("writing beyond doorbell aperture: 0x%08x!\n", index);
}

 
u64 amdgpu_mm_rdoorbell64(struct amdgpu_device *adev, u32 index)
{
	if (amdgpu_device_skip_hw_access(adev))
		return 0;

	if (index < adev->doorbell.num_kernel_doorbells)
		return atomic64_read((atomic64_t *)(adev->doorbell.cpu_addr + index));

	DRM_ERROR("reading beyond doorbell aperture: 0x%08x!\n", index);
	return 0;
}

 
void amdgpu_mm_wdoorbell64(struct amdgpu_device *adev, u32 index, u64 v)
{
	if (amdgpu_device_skip_hw_access(adev))
		return;

	if (index < adev->doorbell.num_kernel_doorbells)
		atomic64_set((atomic64_t *)(adev->doorbell.cpu_addr + index), v);
	else
		DRM_ERROR("writing beyond doorbell aperture: 0x%08x!\n", index);
}

 
uint32_t amdgpu_doorbell_index_on_bar(struct amdgpu_device *adev,
				      struct amdgpu_bo *db_bo,
				      uint32_t doorbell_index,
				      uint32_t db_size)
{
	int db_bo_offset;

	db_bo_offset = amdgpu_bo_gpu_offset_no_check(db_bo);

	 
	return db_bo_offset / sizeof(u32) + doorbell_index *
	       DIV_ROUND_UP(db_size, 4);
}

 
int amdgpu_doorbell_create_kernel_doorbells(struct amdgpu_device *adev)
{
	int r;
	int size;

	 
	if (adev->doorbell.num_kernel_doorbells == 0)
		return 0;

	 
	size = ALIGN(adev->doorbell.num_kernel_doorbells * sizeof(u32), PAGE_SIZE);

	 
	adev->mes.db_start_dw_offset = size / sizeof(u32);
	size += PAGE_SIZE;

	r = amdgpu_bo_create_kernel(adev,
				    size,
				    PAGE_SIZE,
				    AMDGPU_GEM_DOMAIN_DOORBELL,
				    &adev->doorbell.kernel_doorbells,
				    NULL,
				    (void **)&adev->doorbell.cpu_addr);
	if (r) {
		DRM_ERROR("Failed to allocate kernel doorbells, err=%d\n", r);
		return r;
	}

	adev->doorbell.num_kernel_doorbells = size / sizeof(u32);
	return 0;
}

 
 
int amdgpu_doorbell_init(struct amdgpu_device *adev)
{

	 
	if (adev->asic_type < CHIP_BONAIRE) {
		adev->doorbell.base = 0;
		adev->doorbell.size = 0;
		adev->doorbell.num_kernel_doorbells = 0;
		return 0;
	}

	if (pci_resource_flags(adev->pdev, 2) & IORESOURCE_UNSET)
		return -EINVAL;

	amdgpu_asic_init_doorbell_index(adev);

	 
	adev->doorbell.base = pci_resource_start(adev->pdev, 2);
	adev->doorbell.size = pci_resource_len(adev->pdev, 2);

	adev->doorbell.num_kernel_doorbells =
		min_t(u32, adev->doorbell.size / sizeof(u32),
		      adev->doorbell_index.max_assignment + 1);
	if (adev->doorbell.num_kernel_doorbells == 0)
		return -EINVAL;

	 
	if (adev->asic_type >= CHIP_VEGA10)
		adev->doorbell.num_kernel_doorbells += 0x400;

	return 0;
}

 
void amdgpu_doorbell_fini(struct amdgpu_device *adev)
{
	amdgpu_bo_free_kernel(&adev->doorbell.kernel_doorbells,
			      NULL,
			      (void **)&adev->doorbell.cpu_addr);
}
