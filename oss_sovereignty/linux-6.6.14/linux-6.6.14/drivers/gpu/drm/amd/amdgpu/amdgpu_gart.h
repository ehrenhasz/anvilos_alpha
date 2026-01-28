#ifndef __AMDGPU_GART_H__
#define __AMDGPU_GART_H__
#include <linux/types.h>
struct amdgpu_device;
struct amdgpu_bo;
#define AMDGPU_GPU_PAGE_SIZE 4096
#define AMDGPU_GPU_PAGE_MASK (AMDGPU_GPU_PAGE_SIZE - 1)
#define AMDGPU_GPU_PAGE_SHIFT 12
#define AMDGPU_GPU_PAGE_ALIGN(a) (((a) + AMDGPU_GPU_PAGE_MASK) & ~AMDGPU_GPU_PAGE_MASK)
#define AMDGPU_GPU_PAGES_IN_CPU_PAGE (PAGE_SIZE / AMDGPU_GPU_PAGE_SIZE)
struct amdgpu_gart {
	struct amdgpu_bo		*bo;
	void				*ptr;
	unsigned			num_gpu_pages;
	unsigned			num_cpu_pages;
	unsigned			table_size;
	uint64_t			gart_pte_flags;
};
int amdgpu_gart_table_ram_alloc(struct amdgpu_device *adev);
void amdgpu_gart_table_ram_free(struct amdgpu_device *adev);
int amdgpu_gart_table_vram_alloc(struct amdgpu_device *adev);
void amdgpu_gart_table_vram_free(struct amdgpu_device *adev);
int amdgpu_gart_table_vram_pin(struct amdgpu_device *adev);
void amdgpu_gart_table_vram_unpin(struct amdgpu_device *adev);
int amdgpu_gart_init(struct amdgpu_device *adev);
void amdgpu_gart_dummy_page_fini(struct amdgpu_device *adev);
void amdgpu_gart_unbind(struct amdgpu_device *adev, uint64_t offset,
			int pages);
void amdgpu_gart_map(struct amdgpu_device *adev, uint64_t offset,
		     int pages, dma_addr_t *dma_addr, uint64_t flags,
		     void *dst);
void amdgpu_gart_bind(struct amdgpu_device *adev, uint64_t offset,
		      int pages, dma_addr_t *dma_addr, uint64_t flags);
void amdgpu_gart_invalidate_tlb(struct amdgpu_device *adev);
#endif
