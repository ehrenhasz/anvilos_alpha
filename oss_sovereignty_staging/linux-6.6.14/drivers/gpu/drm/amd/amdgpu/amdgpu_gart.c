 

#include <linux/pci.h>
#include <linux/vmalloc.h>

#include <drm/amdgpu_drm.h>
#ifdef CONFIG_X86
#include <asm/set_memory.h>
#endif
#include "amdgpu.h"
#include <drm/drm_drv.h>
#include <drm/ttm/ttm_tt.h>

 

 

 
static int amdgpu_gart_dummy_page_init(struct amdgpu_device *adev)
{
	struct page *dummy_page = ttm_glob.dummy_read_page;

	if (adev->dummy_page_addr)
		return 0;
	adev->dummy_page_addr = dma_map_page(&adev->pdev->dev, dummy_page, 0,
					     PAGE_SIZE, DMA_BIDIRECTIONAL);
	if (dma_mapping_error(&adev->pdev->dev, adev->dummy_page_addr)) {
		dev_err(&adev->pdev->dev, "Failed to DMA MAP the dummy page\n");
		adev->dummy_page_addr = 0;
		return -ENOMEM;
	}
	return 0;
}

 
void amdgpu_gart_dummy_page_fini(struct amdgpu_device *adev)
{
	if (!adev->dummy_page_addr)
		return;
	dma_unmap_page(&adev->pdev->dev, adev->dummy_page_addr, PAGE_SIZE,
		       DMA_BIDIRECTIONAL);
	adev->dummy_page_addr = 0;
}

 
int amdgpu_gart_table_ram_alloc(struct amdgpu_device *adev)
{
	unsigned int order = get_order(adev->gart.table_size);
	gfp_t gfp_flags = GFP_KERNEL | __GFP_ZERO;
	struct amdgpu_bo *bo = NULL;
	struct sg_table *sg = NULL;
	struct amdgpu_bo_param bp;
	dma_addr_t dma_addr;
	struct page *p;
	int ret;

	if (adev->gart.bo != NULL)
		return 0;

	p = alloc_pages(gfp_flags, order);
	if (!p)
		return -ENOMEM;

	 
	dma_addr = dma_map_page(&adev->pdev->dev, p, 0, adev->gart.table_size,
				DMA_BIDIRECTIONAL);
	if (dma_mapping_error(&adev->pdev->dev, dma_addr)) {
		dev_err(&adev->pdev->dev, "Failed to DMA MAP the GART BO page\n");
		__free_pages(p, order);
		p = NULL;
		return -EFAULT;
	}

	dev_info(adev->dev, "%s dma_addr:%pad\n", __func__, &dma_addr);
	 
	sg = kmalloc(sizeof(*sg), GFP_KERNEL);
	if (!sg) {
		ret = -ENOMEM;
		goto error;
	}
	ret = sg_alloc_table(sg, 1, GFP_KERNEL);
	if (ret)
		goto error;

	sg_dma_address(sg->sgl) = dma_addr;
	sg->sgl->length = adev->gart.table_size;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
	sg->sgl->dma_length = adev->gart.table_size;
#endif
	 
	memset(&bp, 0, sizeof(bp));
	bp.size = adev->gart.table_size;
	bp.byte_align = PAGE_SIZE;
	bp.domain = AMDGPU_GEM_DOMAIN_CPU;
	bp.type = ttm_bo_type_sg;
	bp.resv = NULL;
	bp.bo_ptr_size = sizeof(struct amdgpu_bo);
	bp.flags = 0;
	ret = amdgpu_bo_create(adev, &bp, &bo);
	if (ret)
		goto error;

	bo->tbo.sg = sg;
	bo->tbo.ttm->sg = sg;
	bo->allowed_domains = AMDGPU_GEM_DOMAIN_GTT;
	bo->preferred_domains = AMDGPU_GEM_DOMAIN_GTT;

	ret = amdgpu_bo_reserve(bo, true);
	if (ret) {
		dev_err(adev->dev, "(%d) failed to reserve bo for GART system bo\n", ret);
		goto error;
	}

	ret = amdgpu_bo_pin(bo, AMDGPU_GEM_DOMAIN_GTT);
	WARN(ret, "Pinning the GART table failed");
	if (ret)
		goto error_resv;

	adev->gart.bo = bo;
	adev->gart.ptr = page_to_virt(p);
	 
	ret = amdgpu_ttm_alloc_gart(&adev->gart.bo->tbo);
	if (ret)
		amdgpu_gart_table_ram_free(adev);
	amdgpu_bo_unreserve(bo);

	return 0;

error_resv:
	amdgpu_bo_unreserve(bo);
error:
	amdgpu_bo_unref(&bo);
	if (sg) {
		sg_free_table(sg);
		kfree(sg);
	}
	__free_pages(p, order);
	return ret;
}

 
void amdgpu_gart_table_ram_free(struct amdgpu_device *adev)
{
	unsigned int order = get_order(adev->gart.table_size);
	struct sg_table *sg = adev->gart.bo->tbo.sg;
	struct page *p;
	int ret;

	ret = amdgpu_bo_reserve(adev->gart.bo, false);
	if (!ret) {
		amdgpu_bo_unpin(adev->gart.bo);
		amdgpu_bo_unreserve(adev->gart.bo);
	}
	amdgpu_bo_unref(&adev->gart.bo);
	sg_free_table(sg);
	kfree(sg);
	p = virt_to_page(adev->gart.ptr);
	__free_pages(p, order);

	adev->gart.ptr = NULL;
}

 
int amdgpu_gart_table_vram_alloc(struct amdgpu_device *adev)
{
	if (adev->gart.bo != NULL)
		return 0;

	return amdgpu_bo_create_kernel(adev,  adev->gart.table_size, PAGE_SIZE,
				       AMDGPU_GEM_DOMAIN_VRAM, &adev->gart.bo,
				       NULL, (void *)&adev->gart.ptr);
}

 
void amdgpu_gart_table_vram_free(struct amdgpu_device *adev)
{
	amdgpu_bo_free_kernel(&adev->gart.bo, NULL, (void *)&adev->gart.ptr);
}

 
 
void amdgpu_gart_unbind(struct amdgpu_device *adev, uint64_t offset,
			int pages)
{
	unsigned t;
	unsigned p;
	int i, j;
	u64 page_base;
	 
	uint64_t flags = 0;
	int idx;

	if (!adev->gart.ptr)
		return;

	if (!drm_dev_enter(adev_to_drm(adev), &idx))
		return;

	t = offset / AMDGPU_GPU_PAGE_SIZE;
	p = t / AMDGPU_GPU_PAGES_IN_CPU_PAGE;
	for (i = 0; i < pages; i++, p++) {
		page_base = adev->dummy_page_addr;
		if (!adev->gart.ptr)
			continue;

		for (j = 0; j < AMDGPU_GPU_PAGES_IN_CPU_PAGE; j++, t++) {
			amdgpu_gmc_set_pte_pde(adev, adev->gart.ptr,
					       t, page_base, flags);
			page_base += AMDGPU_GPU_PAGE_SIZE;
		}
	}
	mb();
	amdgpu_device_flush_hdp(adev, NULL);
	for_each_set_bit(i, adev->vmhubs_mask, AMDGPU_MAX_VMHUBS)
		amdgpu_gmc_flush_gpu_tlb(adev, 0, i, 0);

	drm_dev_exit(idx);
}

 
void amdgpu_gart_map(struct amdgpu_device *adev, uint64_t offset,
		    int pages, dma_addr_t *dma_addr, uint64_t flags,
		    void *dst)
{
	uint64_t page_base;
	unsigned i, j, t;
	int idx;

	if (!drm_dev_enter(adev_to_drm(adev), &idx))
		return;

	t = offset / AMDGPU_GPU_PAGE_SIZE;

	for (i = 0; i < pages; i++) {
		page_base = dma_addr[i];
		for (j = 0; j < AMDGPU_GPU_PAGES_IN_CPU_PAGE; j++, t++) {
			amdgpu_gmc_set_pte_pde(adev, dst, t, page_base, flags);
			page_base += AMDGPU_GPU_PAGE_SIZE;
		}
	}
	drm_dev_exit(idx);
}

 
void amdgpu_gart_bind(struct amdgpu_device *adev, uint64_t offset,
		     int pages, dma_addr_t *dma_addr,
		     uint64_t flags)
{
	if (!adev->gart.ptr)
		return;

	amdgpu_gart_map(adev, offset, pages, dma_addr, flags, adev->gart.ptr);
}

 
void amdgpu_gart_invalidate_tlb(struct amdgpu_device *adev)
{
	int i;

	if (!adev->gart.ptr)
		return;

	mb();
	amdgpu_device_flush_hdp(adev, NULL);
	for_each_set_bit(i, adev->vmhubs_mask, AMDGPU_MAX_VMHUBS)
		amdgpu_gmc_flush_gpu_tlb(adev, 0, i, 0);
}

 
int amdgpu_gart_init(struct amdgpu_device *adev)
{
	int r;

	if (adev->dummy_page_addr)
		return 0;

	 
	if (PAGE_SIZE < AMDGPU_GPU_PAGE_SIZE) {
		DRM_ERROR("Page size is smaller than GPU page size!\n");
		return -EINVAL;
	}
	r = amdgpu_gart_dummy_page_init(adev);
	if (r)
		return r;
	 
	adev->gart.num_cpu_pages = adev->gmc.gart_size / PAGE_SIZE;
	adev->gart.num_gpu_pages = adev->gmc.gart_size / AMDGPU_GPU_PAGE_SIZE;
	DRM_INFO("GART: num cpu pages %u, num gpu pages %u\n",
		 adev->gart.num_cpu_pages, adev->gart.num_gpu_pages);

	return 0;
}
