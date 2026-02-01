
 
#include <linux/dma-buf.h>
#include <linux/list.h>
#include <linux/pagemap.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <drm/ttm/ttm_tt.h>

#include <drm/drm_exec.h>

#include "amdgpu_object.h"
#include "amdgpu_gem.h"
#include "amdgpu_vm.h"
#include "amdgpu_hmm.h"
#include "amdgpu_amdkfd.h"
#include "amdgpu_dma_buf.h"
#include <uapi/linux/kfd_ioctl.h>
#include "amdgpu_xgmi.h"
#include "kfd_priv.h"
#include "kfd_smi_events.h"

 
#define AMDGPU_USERPTR_RESTORE_DELAY_MS 1

 
#define VRAM_AVAILABLITY_ALIGN (1 << 21)

 
static struct {
	uint64_t max_system_mem_limit;
	uint64_t max_ttm_mem_limit;
	int64_t system_mem_used;
	int64_t ttm_mem_used;
	spinlock_t mem_limit_lock;
} kfd_mem_limit;

static const char * const domain_bit_to_string[] = {
		"CPU",
		"GTT",
		"VRAM",
		"GDS",
		"GWS",
		"OA"
};

#define domain_string(domain) domain_bit_to_string[ffs(domain)-1]

static void amdgpu_amdkfd_restore_userptr_worker(struct work_struct *work);

static bool kfd_mem_is_attached(struct amdgpu_vm *avm,
		struct kgd_mem *mem)
{
	struct kfd_mem_attachment *entry;

	list_for_each_entry(entry, &mem->attachments, list)
		if (entry->bo_va->base.vm == avm)
			return true;

	return false;
}

 
static bool reuse_dmamap(struct amdgpu_device *adev, struct amdgpu_device *bo_adev)
{
	return (adev->ram_is_direct_mapped && bo_adev->ram_is_direct_mapped) ||
			(adev->dev->iommu_group == bo_adev->dev->iommu_group);
}

 
void amdgpu_amdkfd_gpuvm_init_mem_limits(void)
{
	struct sysinfo si;
	uint64_t mem;

	if (kfd_mem_limit.max_system_mem_limit)
		return;

	si_meminfo(&si);
	mem = si.freeram - si.freehigh;
	mem *= si.mem_unit;

	spin_lock_init(&kfd_mem_limit.mem_limit_lock);
	kfd_mem_limit.max_system_mem_limit = mem - (mem >> 4);
	kfd_mem_limit.max_ttm_mem_limit = ttm_tt_pages_limit() << PAGE_SHIFT;
	pr_debug("Kernel memory limit %lluM, TTM limit %lluM\n",
		(kfd_mem_limit.max_system_mem_limit >> 20),
		(kfd_mem_limit.max_ttm_mem_limit >> 20));
}

void amdgpu_amdkfd_reserve_system_mem(uint64_t size)
{
	kfd_mem_limit.system_mem_used += size;
}

 

#define ESTIMATE_PT_SIZE(mem_size) max(((mem_size) >> 14), AMDGPU_VM_RESERVED_VRAM)

 
int amdgpu_amdkfd_reserve_mem_limit(struct amdgpu_device *adev,
		uint64_t size, u32 alloc_flag, int8_t xcp_id)
{
	uint64_t reserved_for_pt =
		ESTIMATE_PT_SIZE(amdgpu_amdkfd_total_mem_size);
	size_t system_mem_needed, ttm_mem_needed, vram_needed;
	int ret = 0;
	uint64_t vram_size = 0;

	system_mem_needed = 0;
	ttm_mem_needed = 0;
	vram_needed = 0;
	if (alloc_flag & KFD_IOC_ALLOC_MEM_FLAGS_GTT) {
		system_mem_needed = size;
		ttm_mem_needed = size;
	} else if (alloc_flag & KFD_IOC_ALLOC_MEM_FLAGS_VRAM) {
		 
		vram_needed = size;
		 
		if (WARN_ONCE(xcp_id < 0, "invalid XCP ID %d", xcp_id))
			return -EINVAL;

		vram_size = KFD_XCP_MEMORY_SIZE(adev, xcp_id);
		if (adev->gmc.is_app_apu) {
			system_mem_needed = size;
			ttm_mem_needed = size;
		}
	} else if (alloc_flag & KFD_IOC_ALLOC_MEM_FLAGS_USERPTR) {
		system_mem_needed = size;
	} else if (!(alloc_flag &
				(KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL |
				 KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP))) {
		pr_err("%s: Invalid BO type %#x\n", __func__, alloc_flag);
		return -ENOMEM;
	}

	spin_lock(&kfd_mem_limit.mem_limit_lock);

	if (kfd_mem_limit.system_mem_used + system_mem_needed >
	    kfd_mem_limit.max_system_mem_limit)
		pr_debug("Set no_system_mem_limit=1 if using shared memory\n");

	if ((kfd_mem_limit.system_mem_used + system_mem_needed >
	     kfd_mem_limit.max_system_mem_limit && !no_system_mem_limit) ||
	    (kfd_mem_limit.ttm_mem_used + ttm_mem_needed >
	     kfd_mem_limit.max_ttm_mem_limit) ||
	    (adev && xcp_id >= 0 && adev->kfd.vram_used[xcp_id] + vram_needed >
	     vram_size - reserved_for_pt)) {
		ret = -ENOMEM;
		goto release;
	}

	 
	WARN_ONCE(vram_needed && !adev,
		  "adev reference can't be null when vram is used");
	if (adev && xcp_id >= 0) {
		adev->kfd.vram_used[xcp_id] += vram_needed;
		adev->kfd.vram_used_aligned[xcp_id] += adev->gmc.is_app_apu ?
				vram_needed :
				ALIGN(vram_needed, VRAM_AVAILABLITY_ALIGN);
	}
	kfd_mem_limit.system_mem_used += system_mem_needed;
	kfd_mem_limit.ttm_mem_used += ttm_mem_needed;

release:
	spin_unlock(&kfd_mem_limit.mem_limit_lock);
	return ret;
}

void amdgpu_amdkfd_unreserve_mem_limit(struct amdgpu_device *adev,
		uint64_t size, u32 alloc_flag, int8_t xcp_id)
{
	spin_lock(&kfd_mem_limit.mem_limit_lock);

	if (alloc_flag & KFD_IOC_ALLOC_MEM_FLAGS_GTT) {
		kfd_mem_limit.system_mem_used -= size;
		kfd_mem_limit.ttm_mem_used -= size;
	} else if (alloc_flag & KFD_IOC_ALLOC_MEM_FLAGS_VRAM) {
		WARN_ONCE(!adev,
			  "adev reference can't be null when alloc mem flags vram is set");
		if (WARN_ONCE(xcp_id < 0, "invalid XCP ID %d", xcp_id))
			goto release;

		if (adev) {
			adev->kfd.vram_used[xcp_id] -= size;
			if (adev->gmc.is_app_apu) {
				adev->kfd.vram_used_aligned[xcp_id] -= size;
				kfd_mem_limit.system_mem_used -= size;
				kfd_mem_limit.ttm_mem_used -= size;
			} else {
				adev->kfd.vram_used_aligned[xcp_id] -=
					ALIGN(size, VRAM_AVAILABLITY_ALIGN);
			}
		}
	} else if (alloc_flag & KFD_IOC_ALLOC_MEM_FLAGS_USERPTR) {
		kfd_mem_limit.system_mem_used -= size;
	} else if (!(alloc_flag &
				(KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL |
				 KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP))) {
		pr_err("%s: Invalid BO type %#x\n", __func__, alloc_flag);
		goto release;
	}
	WARN_ONCE(adev && xcp_id >= 0 && adev->kfd.vram_used[xcp_id] < 0,
		  "KFD VRAM memory accounting unbalanced for xcp: %d", xcp_id);
	WARN_ONCE(kfd_mem_limit.ttm_mem_used < 0,
		  "KFD TTM memory accounting unbalanced");
	WARN_ONCE(kfd_mem_limit.system_mem_used < 0,
		  "KFD system memory accounting unbalanced");

release:
	spin_unlock(&kfd_mem_limit.mem_limit_lock);
}

void amdgpu_amdkfd_release_notify(struct amdgpu_bo *bo)
{
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);
	u32 alloc_flags = bo->kfd_bo->alloc_flags;
	u64 size = amdgpu_bo_size(bo);

	amdgpu_amdkfd_unreserve_mem_limit(adev, size, alloc_flags,
					  bo->xcp_id);

	kfree(bo->kfd_bo);
}

 
static int
create_dmamap_sg_bo(struct amdgpu_device *adev,
		 struct kgd_mem *mem, struct amdgpu_bo **bo_out)
{
	struct drm_gem_object *gem_obj;
	int ret;
	uint64_t flags = 0;

	ret = amdgpu_bo_reserve(mem->bo, false);
	if (ret)
		return ret;

	if (mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_USERPTR)
		flags |= mem->bo->flags & (AMDGPU_GEM_CREATE_COHERENT |
					AMDGPU_GEM_CREATE_UNCACHED);

	ret = amdgpu_gem_object_create(adev, mem->bo->tbo.base.size, 1,
			AMDGPU_GEM_DOMAIN_CPU, AMDGPU_GEM_CREATE_PREEMPTIBLE | flags,
			ttm_bo_type_sg, mem->bo->tbo.base.resv, &gem_obj, 0);

	amdgpu_bo_unreserve(mem->bo);

	if (ret) {
		pr_err("Error in creating DMA mappable SG BO on domain: %d\n", ret);
		return -EINVAL;
	}

	*bo_out = gem_to_amdgpu_bo(gem_obj);
	(*bo_out)->parent = amdgpu_bo_ref(mem->bo);
	return ret;
}

 
static int amdgpu_amdkfd_remove_eviction_fence(struct amdgpu_bo *bo,
					struct amdgpu_amdkfd_fence *ef)
{
	struct dma_fence *replacement;

	if (!ef)
		return -EINVAL;

	 
	replacement = dma_fence_get_stub();
	dma_resv_replace_fences(bo->tbo.base.resv, ef->base.context,
				replacement, DMA_RESV_USAGE_BOOKKEEP);
	dma_fence_put(replacement);
	return 0;
}

int amdgpu_amdkfd_remove_fence_on_pt_pd_bos(struct amdgpu_bo *bo)
{
	struct amdgpu_bo *root = bo;
	struct amdgpu_vm_bo_base *vm_bo;
	struct amdgpu_vm *vm;
	struct amdkfd_process_info *info;
	struct amdgpu_amdkfd_fence *ef;
	int ret;

	 
	while (root->parent)
		root = root->parent;

	vm_bo = root->vm_bo;
	if (!vm_bo)
		return 0;

	vm = vm_bo->vm;
	if (!vm)
		return 0;

	info = vm->process_info;
	if (!info || !info->eviction_fence)
		return 0;

	ef = container_of(dma_fence_get(&info->eviction_fence->base),
			struct amdgpu_amdkfd_fence, base);

	BUG_ON(!dma_resv_trylock(bo->tbo.base.resv));
	ret = amdgpu_amdkfd_remove_eviction_fence(bo, ef);
	dma_resv_unlock(bo->tbo.base.resv);

	dma_fence_put(&ef->base);
	return ret;
}

static int amdgpu_amdkfd_bo_validate(struct amdgpu_bo *bo, uint32_t domain,
				     bool wait)
{
	struct ttm_operation_ctx ctx = { false, false };
	int ret;

	if (WARN(amdgpu_ttm_tt_get_usermm(bo->tbo.ttm),
		 "Called with userptr BO"))
		return -EINVAL;

	amdgpu_bo_placement_from_domain(bo, domain);

	ret = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
	if (ret)
		goto validate_fail;
	if (wait)
		amdgpu_bo_sync_wait(bo, AMDGPU_FENCE_OWNER_KFD, false);

validate_fail:
	return ret;
}

static int amdgpu_amdkfd_validate_vm_bo(void *_unused, struct amdgpu_bo *bo)
{
	return amdgpu_amdkfd_bo_validate(bo, bo->allowed_domains, false);
}

 
static int vm_validate_pt_pd_bos(struct amdgpu_vm *vm)
{
	struct amdgpu_bo *pd = vm->root.bo;
	struct amdgpu_device *adev = amdgpu_ttm_adev(pd->tbo.bdev);
	int ret;

	ret = amdgpu_vm_validate_pt_bos(adev, vm, amdgpu_amdkfd_validate_vm_bo, NULL);
	if (ret) {
		pr_err("failed to validate PT BOs\n");
		return ret;
	}

	vm->pd_phys_addr = amdgpu_gmc_pd_addr(vm->root.bo);

	return 0;
}

static int vm_update_pds(struct amdgpu_vm *vm, struct amdgpu_sync *sync)
{
	struct amdgpu_bo *pd = vm->root.bo;
	struct amdgpu_device *adev = amdgpu_ttm_adev(pd->tbo.bdev);
	int ret;

	ret = amdgpu_vm_update_pdes(adev, vm, false);
	if (ret)
		return ret;

	return amdgpu_sync_fence(sync, vm->last_update);
}

static uint64_t get_pte_flags(struct amdgpu_device *adev, struct kgd_mem *mem)
{
	uint32_t mapping_flags = AMDGPU_VM_PAGE_READABLE |
				 AMDGPU_VM_MTYPE_DEFAULT;

	if (mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE)
		mapping_flags |= AMDGPU_VM_PAGE_WRITEABLE;
	if (mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_EXECUTABLE)
		mapping_flags |= AMDGPU_VM_PAGE_EXECUTABLE;

	return amdgpu_gem_va_map_flags(adev, mapping_flags);
}

 
static struct sg_table *create_sg_table(uint64_t addr, uint32_t size)
{
	struct sg_table *sg = kmalloc(sizeof(*sg), GFP_KERNEL);

	if (!sg)
		return NULL;
	if (sg_alloc_table(sg, 1, GFP_KERNEL)) {
		kfree(sg);
		return NULL;
	}
	sg_dma_address(sg->sgl) = addr;
	sg->sgl->length = size;
#ifdef CONFIG_NEED_SG_DMA_LENGTH
	sg->sgl->dma_length = size;
#endif
	return sg;
}

static int
kfd_mem_dmamap_userptr(struct kgd_mem *mem,
		       struct kfd_mem_attachment *attachment)
{
	enum dma_data_direction direction =
		mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE ?
		DMA_BIDIRECTIONAL : DMA_TO_DEVICE;
	struct ttm_operation_ctx ctx = {.interruptible = true};
	struct amdgpu_bo *bo = attachment->bo_va->base.bo;
	struct amdgpu_device *adev = attachment->adev;
	struct ttm_tt *src_ttm = mem->bo->tbo.ttm;
	struct ttm_tt *ttm = bo->tbo.ttm;
	int ret;

	if (WARN_ON(ttm->num_pages != src_ttm->num_pages))
		return -EINVAL;

	ttm->sg = kmalloc(sizeof(*ttm->sg), GFP_KERNEL);
	if (unlikely(!ttm->sg))
		return -ENOMEM;

	 
	ret = sg_alloc_table_from_pages(ttm->sg, src_ttm->pages,
					ttm->num_pages, 0,
					(u64)ttm->num_pages << PAGE_SHIFT,
					GFP_KERNEL);
	if (unlikely(ret))
		goto free_sg;

	ret = dma_map_sgtable(adev->dev, ttm->sg, direction, 0);
	if (unlikely(ret))
		goto release_sg;

	amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_GTT);
	ret = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
	if (ret)
		goto unmap_sg;

	return 0;

unmap_sg:
	dma_unmap_sgtable(adev->dev, ttm->sg, direction, 0);
release_sg:
	pr_err("DMA map userptr failed: %d\n", ret);
	sg_free_table(ttm->sg);
free_sg:
	kfree(ttm->sg);
	ttm->sg = NULL;
	return ret;
}

static int
kfd_mem_dmamap_dmabuf(struct kfd_mem_attachment *attachment)
{
	struct ttm_operation_ctx ctx = {.interruptible = true};
	struct amdgpu_bo *bo = attachment->bo_va->base.bo;
	int ret;

	amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_CPU);
	ret = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
	if (ret)
		return ret;

	amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_GTT);
	return ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
}

 
static int
kfd_mem_dmamap_sg_bo(struct kgd_mem *mem,
		     struct kfd_mem_attachment *attachment)
{
	struct ttm_operation_ctx ctx = {.interruptible = true};
	struct amdgpu_bo *bo = attachment->bo_va->base.bo;
	struct amdgpu_device *adev = attachment->adev;
	struct ttm_tt *ttm = bo->tbo.ttm;
	enum dma_data_direction dir;
	dma_addr_t dma_addr;
	bool mmio;
	int ret;

	 
	mmio = (mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP);
	if (unlikely(ttm->sg)) {
		pr_err("SG Table of %d BO for peer device is UNEXPECTEDLY NON-NULL", mmio);
		return -EINVAL;
	}

	dir = mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE ?
			DMA_BIDIRECTIONAL : DMA_TO_DEVICE;
	dma_addr = mem->bo->tbo.sg->sgl->dma_address;
	pr_debug("%d BO size: %d\n", mmio, mem->bo->tbo.sg->sgl->length);
	pr_debug("%d BO address before DMA mapping: %llx\n", mmio, dma_addr);
	dma_addr = dma_map_resource(adev->dev, dma_addr,
			mem->bo->tbo.sg->sgl->length, dir, DMA_ATTR_SKIP_CPU_SYNC);
	ret = dma_mapping_error(adev->dev, dma_addr);
	if (unlikely(ret))
		return ret;
	pr_debug("%d BO address after DMA mapping: %llx\n", mmio, dma_addr);

	ttm->sg = create_sg_table(dma_addr, mem->bo->tbo.sg->sgl->length);
	if (unlikely(!ttm->sg)) {
		ret = -ENOMEM;
		goto unmap_sg;
	}

	amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_GTT);
	ret = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
	if (unlikely(ret))
		goto free_sg;

	return ret;

free_sg:
	sg_free_table(ttm->sg);
	kfree(ttm->sg);
	ttm->sg = NULL;
unmap_sg:
	dma_unmap_resource(adev->dev, dma_addr, mem->bo->tbo.sg->sgl->length,
			   dir, DMA_ATTR_SKIP_CPU_SYNC);
	return ret;
}

static int
kfd_mem_dmamap_attachment(struct kgd_mem *mem,
			  struct kfd_mem_attachment *attachment)
{
	switch (attachment->type) {
	case KFD_MEM_ATT_SHARED:
		return 0;
	case KFD_MEM_ATT_USERPTR:
		return kfd_mem_dmamap_userptr(mem, attachment);
	case KFD_MEM_ATT_DMABUF:
		return kfd_mem_dmamap_dmabuf(attachment);
	case KFD_MEM_ATT_SG:
		return kfd_mem_dmamap_sg_bo(mem, attachment);
	default:
		WARN_ON_ONCE(1);
	}
	return -EINVAL;
}

static void
kfd_mem_dmaunmap_userptr(struct kgd_mem *mem,
			 struct kfd_mem_attachment *attachment)
{
	enum dma_data_direction direction =
		mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE ?
		DMA_BIDIRECTIONAL : DMA_TO_DEVICE;
	struct ttm_operation_ctx ctx = {.interruptible = false};
	struct amdgpu_bo *bo = attachment->bo_va->base.bo;
	struct amdgpu_device *adev = attachment->adev;
	struct ttm_tt *ttm = bo->tbo.ttm;

	if (unlikely(!ttm->sg))
		return;

	amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_CPU);
	ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);

	dma_unmap_sgtable(adev->dev, ttm->sg, direction, 0);
	sg_free_table(ttm->sg);
	kfree(ttm->sg);
	ttm->sg = NULL;
}

static void
kfd_mem_dmaunmap_dmabuf(struct kfd_mem_attachment *attachment)
{
	 
}

 
static void
kfd_mem_dmaunmap_sg_bo(struct kgd_mem *mem,
		       struct kfd_mem_attachment *attachment)
{
	struct ttm_operation_ctx ctx = {.interruptible = true};
	struct amdgpu_bo *bo = attachment->bo_va->base.bo;
	struct amdgpu_device *adev = attachment->adev;
	struct ttm_tt *ttm = bo->tbo.ttm;
	enum dma_data_direction dir;

	if (unlikely(!ttm->sg)) {
		pr_err("SG Table of BO is UNEXPECTEDLY NULL");
		return;
	}

	amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_CPU);
	ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);

	dir = mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE ?
				DMA_BIDIRECTIONAL : DMA_TO_DEVICE;
	dma_unmap_resource(adev->dev, ttm->sg->sgl->dma_address,
			ttm->sg->sgl->length, dir, DMA_ATTR_SKIP_CPU_SYNC);
	sg_free_table(ttm->sg);
	kfree(ttm->sg);
	ttm->sg = NULL;
	bo->tbo.sg = NULL;
}

static void
kfd_mem_dmaunmap_attachment(struct kgd_mem *mem,
			    struct kfd_mem_attachment *attachment)
{
	switch (attachment->type) {
	case KFD_MEM_ATT_SHARED:
		break;
	case KFD_MEM_ATT_USERPTR:
		kfd_mem_dmaunmap_userptr(mem, attachment);
		break;
	case KFD_MEM_ATT_DMABUF:
		kfd_mem_dmaunmap_dmabuf(attachment);
		break;
	case KFD_MEM_ATT_SG:
		kfd_mem_dmaunmap_sg_bo(mem, attachment);
		break;
	default:
		WARN_ON_ONCE(1);
	}
}

static int kfd_mem_export_dmabuf(struct kgd_mem *mem)
{
	if (!mem->dmabuf) {
		struct dma_buf *ret = amdgpu_gem_prime_export(
			&mem->bo->tbo.base,
			mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE ?
				DRM_RDWR : 0);
		if (IS_ERR(ret))
			return PTR_ERR(ret);
		mem->dmabuf = ret;
	}

	return 0;
}

static int
kfd_mem_attach_dmabuf(struct amdgpu_device *adev, struct kgd_mem *mem,
		      struct amdgpu_bo **bo)
{
	struct drm_gem_object *gobj;
	int ret;

	ret = kfd_mem_export_dmabuf(mem);
	if (ret)
		return ret;

	gobj = amdgpu_gem_prime_import(adev_to_drm(adev), mem->dmabuf);
	if (IS_ERR(gobj))
		return PTR_ERR(gobj);

	*bo = gem_to_amdgpu_bo(gobj);
	(*bo)->flags |= AMDGPU_GEM_CREATE_PREEMPTIBLE;

	return 0;
}

 
static int kfd_mem_attach(struct amdgpu_device *adev, struct kgd_mem *mem,
		struct amdgpu_vm *vm, bool is_aql)
{
	struct amdgpu_device *bo_adev = amdgpu_ttm_adev(mem->bo->tbo.bdev);
	unsigned long bo_size = mem->bo->tbo.base.size;
	uint64_t va = mem->va;
	struct kfd_mem_attachment *attachment[2] = {NULL, NULL};
	struct amdgpu_bo *bo[2] = {NULL, NULL};
	bool same_hive = false;
	int i, ret;

	if (!va) {
		pr_err("Invalid VA when adding BO to VM\n");
		return -EINVAL;
	}

	 
	if ((adev != bo_adev && !adev->gmc.is_app_apu) &&
	    ((mem->domain == AMDGPU_GEM_DOMAIN_VRAM) ||
	     (mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL) ||
	     (mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP))) {
		if (mem->domain == AMDGPU_GEM_DOMAIN_VRAM)
			same_hive = amdgpu_xgmi_same_hive(adev, bo_adev);
		if (!same_hive && !amdgpu_device_is_peer_accessible(bo_adev, adev))
			return -EINVAL;
	}

	for (i = 0; i <= is_aql; i++) {
		attachment[i] = kzalloc(sizeof(*attachment[i]), GFP_KERNEL);
		if (unlikely(!attachment[i])) {
			ret = -ENOMEM;
			goto unwind;
		}

		pr_debug("\t add VA 0x%llx - 0x%llx to vm %p\n", va,
			 va + bo_size, vm);

		if ((adev == bo_adev && !(mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP)) ||
		    (amdgpu_ttm_tt_get_usermm(mem->bo->tbo.ttm) && reuse_dmamap(adev, bo_adev)) ||
			same_hive) {
			 
			attachment[i]->type = KFD_MEM_ATT_SHARED;
			bo[i] = mem->bo;
			drm_gem_object_get(&bo[i]->tbo.base);
		} else if (i > 0) {
			 
			attachment[i]->type = KFD_MEM_ATT_SHARED;
			bo[i] = bo[0];
			drm_gem_object_get(&bo[i]->tbo.base);
		} else if (amdgpu_ttm_tt_get_usermm(mem->bo->tbo.ttm)) {
			 
			attachment[i]->type = KFD_MEM_ATT_USERPTR;
			ret = create_dmamap_sg_bo(adev, mem, &bo[i]);
			if (ret)
				goto unwind;
		 
		} else if (mem->bo->tbo.type == ttm_bo_type_sg) {
			WARN_ONCE(!(mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL ||
				    mem->alloc_flags & KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP),
				  "Handing invalid SG BO in ATTACH request");
			attachment[i]->type = KFD_MEM_ATT_SG;
			ret = create_dmamap_sg_bo(adev, mem, &bo[i]);
			if (ret)
				goto unwind;
		 
		} else if (mem->domain == AMDGPU_GEM_DOMAIN_GTT ||
			   mem->domain == AMDGPU_GEM_DOMAIN_VRAM) {
			attachment[i]->type = KFD_MEM_ATT_DMABUF;
			ret = kfd_mem_attach_dmabuf(adev, mem, &bo[i]);
			if (ret)
				goto unwind;
			pr_debug("Employ DMABUF mechanism to enable peer GPU access\n");
		} else {
			WARN_ONCE(true, "Handling invalid ATTACH request");
			ret = -EINVAL;
			goto unwind;
		}

		 
		ret = amdgpu_bo_reserve(bo[i], false);
		if (ret) {
			pr_debug("Unable to reserve BO during memory attach");
			goto unwind;
		}
		attachment[i]->bo_va = amdgpu_vm_bo_add(adev, vm, bo[i]);
		amdgpu_bo_unreserve(bo[i]);
		if (unlikely(!attachment[i]->bo_va)) {
			ret = -ENOMEM;
			pr_err("Failed to add BO object to VM. ret == %d\n",
			       ret);
			goto unwind;
		}
		attachment[i]->va = va;
		attachment[i]->pte_flags = get_pte_flags(adev, mem);
		attachment[i]->adev = adev;
		list_add(&attachment[i]->list, &mem->attachments);

		va += bo_size;
	}

	return 0;

unwind:
	for (; i >= 0; i--) {
		if (!attachment[i])
			continue;
		if (attachment[i]->bo_va) {
			amdgpu_bo_reserve(bo[i], true);
			amdgpu_vm_bo_del(adev, attachment[i]->bo_va);
			amdgpu_bo_unreserve(bo[i]);
			list_del(&attachment[i]->list);
		}
		if (bo[i])
			drm_gem_object_put(&bo[i]->tbo.base);
		kfree(attachment[i]);
	}
	return ret;
}

static void kfd_mem_detach(struct kfd_mem_attachment *attachment)
{
	struct amdgpu_bo *bo = attachment->bo_va->base.bo;

	pr_debug("\t remove VA 0x%llx in entry %p\n",
			attachment->va, attachment);
	amdgpu_vm_bo_del(attachment->adev, attachment->bo_va);
	drm_gem_object_put(&bo->tbo.base);
	list_del(&attachment->list);
	kfree(attachment);
}

static void add_kgd_mem_to_kfd_bo_list(struct kgd_mem *mem,
				struct amdkfd_process_info *process_info,
				bool userptr)
{
	mutex_lock(&process_info->lock);
	if (userptr)
		list_add_tail(&mem->validate_list,
			      &process_info->userptr_valid_list);
	else
		list_add_tail(&mem->validate_list, &process_info->kfd_bo_list);
	mutex_unlock(&process_info->lock);
}

static void remove_kgd_mem_from_kfd_bo_list(struct kgd_mem *mem,
		struct amdkfd_process_info *process_info)
{
	mutex_lock(&process_info->lock);
	list_del(&mem->validate_list);
	mutex_unlock(&process_info->lock);
}

 
static int init_user_pages(struct kgd_mem *mem, uint64_t user_addr,
			   bool criu_resume)
{
	struct amdkfd_process_info *process_info = mem->process_info;
	struct amdgpu_bo *bo = mem->bo;
	struct ttm_operation_ctx ctx = { true, false };
	struct hmm_range *range;
	int ret = 0;

	mutex_lock(&process_info->lock);

	ret = amdgpu_ttm_tt_set_userptr(&bo->tbo, user_addr, 0);
	if (ret) {
		pr_err("%s: Failed to set userptr: %d\n", __func__, ret);
		goto out;
	}

	ret = amdgpu_hmm_register(bo, user_addr);
	if (ret) {
		pr_err("%s: Failed to register MMU notifier: %d\n",
		       __func__, ret);
		goto out;
	}

	if (criu_resume) {
		 
		mutex_lock(&process_info->notifier_lock);
		mem->invalid++;
		mutex_unlock(&process_info->notifier_lock);
		mutex_unlock(&process_info->lock);
		return 0;
	}

	ret = amdgpu_ttm_tt_get_user_pages(bo, bo->tbo.ttm->pages, &range);
	if (ret) {
		pr_err("%s: Failed to get user pages: %d\n", __func__, ret);
		goto unregister_out;
	}

	ret = amdgpu_bo_reserve(bo, true);
	if (ret) {
		pr_err("%s: Failed to reserve BO\n", __func__);
		goto release_out;
	}
	amdgpu_bo_placement_from_domain(bo, mem->domain);
	ret = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
	if (ret)
		pr_err("%s: failed to validate BO\n", __func__);
	amdgpu_bo_unreserve(bo);

release_out:
	amdgpu_ttm_tt_get_user_pages_done(bo->tbo.ttm, range);
unregister_out:
	if (ret)
		amdgpu_hmm_unregister(bo);
out:
	mutex_unlock(&process_info->lock);
	return ret;
}

 
struct bo_vm_reservation_context {
	 
	struct drm_exec exec;
	 
	unsigned int n_vms;
	 
	struct amdgpu_sync *sync;
};

enum bo_vm_match {
	BO_VM_NOT_MAPPED = 0,	 
	BO_VM_MAPPED,		 
	BO_VM_ALL,		 
};

 
static int reserve_bo_and_vm(struct kgd_mem *mem,
			      struct amdgpu_vm *vm,
			      struct bo_vm_reservation_context *ctx)
{
	struct amdgpu_bo *bo = mem->bo;
	int ret;

	WARN_ON(!vm);

	ctx->n_vms = 1;
	ctx->sync = &mem->sync;
	drm_exec_init(&ctx->exec, DRM_EXEC_INTERRUPTIBLE_WAIT);
	drm_exec_until_all_locked(&ctx->exec) {
		ret = amdgpu_vm_lock_pd(vm, &ctx->exec, 2);
		drm_exec_retry_on_contention(&ctx->exec);
		if (unlikely(ret))
			goto error;

		ret = drm_exec_prepare_obj(&ctx->exec, &bo->tbo.base, 1);
		drm_exec_retry_on_contention(&ctx->exec);
		if (unlikely(ret))
			goto error;
	}
	return 0;

error:
	pr_err("Failed to reserve buffers in ttm.\n");
	drm_exec_fini(&ctx->exec);
	return ret;
}

 
static int reserve_bo_and_cond_vms(struct kgd_mem *mem,
				struct amdgpu_vm *vm, enum bo_vm_match map_type,
				struct bo_vm_reservation_context *ctx)
{
	struct kfd_mem_attachment *entry;
	struct amdgpu_bo *bo = mem->bo;
	int ret;

	ctx->sync = &mem->sync;
	drm_exec_init(&ctx->exec, DRM_EXEC_INTERRUPTIBLE_WAIT);
	drm_exec_until_all_locked(&ctx->exec) {
		ctx->n_vms = 0;
		list_for_each_entry(entry, &mem->attachments, list) {
			if ((vm && vm != entry->bo_va->base.vm) ||
				(entry->is_mapped != map_type
				&& map_type != BO_VM_ALL))
				continue;

			ret = amdgpu_vm_lock_pd(entry->bo_va->base.vm,
						&ctx->exec, 2);
			drm_exec_retry_on_contention(&ctx->exec);
			if (unlikely(ret))
				goto error;
			++ctx->n_vms;
		}

		ret = drm_exec_prepare_obj(&ctx->exec, &bo->tbo.base, 1);
		drm_exec_retry_on_contention(&ctx->exec);
		if (unlikely(ret))
			goto error;
	}
	return 0;

error:
	pr_err("Failed to reserve buffers in ttm.\n");
	drm_exec_fini(&ctx->exec);
	return ret;
}

 
static int unreserve_bo_and_vms(struct bo_vm_reservation_context *ctx,
				 bool wait, bool intr)
{
	int ret = 0;

	if (wait)
		ret = amdgpu_sync_wait(ctx->sync, intr);

	drm_exec_fini(&ctx->exec);
	ctx->sync = NULL;
	return ret;
}

static void unmap_bo_from_gpuvm(struct kgd_mem *mem,
				struct kfd_mem_attachment *entry,
				struct amdgpu_sync *sync)
{
	struct amdgpu_bo_va *bo_va = entry->bo_va;
	struct amdgpu_device *adev = entry->adev;
	struct amdgpu_vm *vm = bo_va->base.vm;

	amdgpu_vm_bo_unmap(adev, bo_va, entry->va);

	amdgpu_vm_clear_freed(adev, vm, &bo_va->last_pt_update);

	amdgpu_sync_fence(sync, bo_va->last_pt_update);

	kfd_mem_dmaunmap_attachment(mem, entry);
}

static int update_gpuvm_pte(struct kgd_mem *mem,
			    struct kfd_mem_attachment *entry,
			    struct amdgpu_sync *sync)
{
	struct amdgpu_bo_va *bo_va = entry->bo_va;
	struct amdgpu_device *adev = entry->adev;
	int ret;

	ret = kfd_mem_dmamap_attachment(mem, entry);
	if (ret)
		return ret;

	 
	ret = amdgpu_vm_bo_update(adev, bo_va, false);
	if (ret) {
		pr_err("amdgpu_vm_bo_update failed\n");
		return ret;
	}

	return amdgpu_sync_fence(sync, bo_va->last_pt_update);
}

static int map_bo_to_gpuvm(struct kgd_mem *mem,
			   struct kfd_mem_attachment *entry,
			   struct amdgpu_sync *sync,
			   bool no_update_pte)
{
	int ret;

	 
	ret = amdgpu_vm_bo_map(entry->adev, entry->bo_va, entry->va, 0,
			       amdgpu_bo_size(entry->bo_va->base.bo),
			       entry->pte_flags);
	if (ret) {
		pr_err("Failed to map VA 0x%llx in vm. ret %d\n",
				entry->va, ret);
		return ret;
	}

	if (no_update_pte)
		return 0;

	ret = update_gpuvm_pte(mem, entry, sync);
	if (ret) {
		pr_err("update_gpuvm_pte() failed\n");
		goto update_gpuvm_pte_failed;
	}

	return 0;

update_gpuvm_pte_failed:
	unmap_bo_from_gpuvm(mem, entry, sync);
	return ret;
}

static int process_validate_vms(struct amdkfd_process_info *process_info)
{
	struct amdgpu_vm *peer_vm;
	int ret;

	list_for_each_entry(peer_vm, &process_info->vm_list_head,
			    vm_list_node) {
		ret = vm_validate_pt_pd_bos(peer_vm);
		if (ret)
			return ret;
	}

	return 0;
}

static int process_sync_pds_resv(struct amdkfd_process_info *process_info,
				 struct amdgpu_sync *sync)
{
	struct amdgpu_vm *peer_vm;
	int ret;

	list_for_each_entry(peer_vm, &process_info->vm_list_head,
			    vm_list_node) {
		struct amdgpu_bo *pd = peer_vm->root.bo;

		ret = amdgpu_sync_resv(NULL, sync, pd->tbo.base.resv,
				       AMDGPU_SYNC_NE_OWNER,
				       AMDGPU_FENCE_OWNER_KFD);
		if (ret)
			return ret;
	}

	return 0;
}

static int process_update_pds(struct amdkfd_process_info *process_info,
			      struct amdgpu_sync *sync)
{
	struct amdgpu_vm *peer_vm;
	int ret;

	list_for_each_entry(peer_vm, &process_info->vm_list_head,
			    vm_list_node) {
		ret = vm_update_pds(peer_vm, sync);
		if (ret)
			return ret;
	}

	return 0;
}

static int init_kfd_vm(struct amdgpu_vm *vm, void **process_info,
		       struct dma_fence **ef)
{
	struct amdkfd_process_info *info = NULL;
	int ret;

	if (!*process_info) {
		info = kzalloc(sizeof(*info), GFP_KERNEL);
		if (!info)
			return -ENOMEM;

		mutex_init(&info->lock);
		mutex_init(&info->notifier_lock);
		INIT_LIST_HEAD(&info->vm_list_head);
		INIT_LIST_HEAD(&info->kfd_bo_list);
		INIT_LIST_HEAD(&info->userptr_valid_list);
		INIT_LIST_HEAD(&info->userptr_inval_list);

		info->eviction_fence =
			amdgpu_amdkfd_fence_create(dma_fence_context_alloc(1),
						   current->mm,
						   NULL);
		if (!info->eviction_fence) {
			pr_err("Failed to create eviction fence\n");
			ret = -ENOMEM;
			goto create_evict_fence_fail;
		}

		info->pid = get_task_pid(current->group_leader, PIDTYPE_PID);
		INIT_DELAYED_WORK(&info->restore_userptr_work,
				  amdgpu_amdkfd_restore_userptr_worker);

		*process_info = info;
		*ef = dma_fence_get(&info->eviction_fence->base);
	}

	vm->process_info = *process_info;

	 
	ret = amdgpu_bo_reserve(vm->root.bo, true);
	if (ret)
		goto reserve_pd_fail;
	ret = vm_validate_pt_pd_bos(vm);
	if (ret) {
		pr_err("validate_pt_pd_bos() failed\n");
		goto validate_pd_fail;
	}
	ret = amdgpu_bo_sync_wait(vm->root.bo,
				  AMDGPU_FENCE_OWNER_KFD, false);
	if (ret)
		goto wait_pd_fail;
	ret = dma_resv_reserve_fences(vm->root.bo->tbo.base.resv, 1);
	if (ret)
		goto reserve_shared_fail;
	dma_resv_add_fence(vm->root.bo->tbo.base.resv,
			   &vm->process_info->eviction_fence->base,
			   DMA_RESV_USAGE_BOOKKEEP);
	amdgpu_bo_unreserve(vm->root.bo);

	 
	mutex_lock(&vm->process_info->lock);
	list_add_tail(&vm->vm_list_node,
			&(vm->process_info->vm_list_head));
	vm->process_info->n_vms++;
	mutex_unlock(&vm->process_info->lock);

	return 0;

reserve_shared_fail:
wait_pd_fail:
validate_pd_fail:
	amdgpu_bo_unreserve(vm->root.bo);
reserve_pd_fail:
	vm->process_info = NULL;
	if (info) {
		 
		dma_fence_put(&info->eviction_fence->base);
		dma_fence_put(*ef);
		*ef = NULL;
		*process_info = NULL;
		put_pid(info->pid);
create_evict_fence_fail:
		mutex_destroy(&info->lock);
		mutex_destroy(&info->notifier_lock);
		kfree(info);
	}
	return ret;
}

 
static int amdgpu_amdkfd_gpuvm_pin_bo(struct amdgpu_bo *bo, u32 domain)
{
	int ret = 0;

	ret = amdgpu_bo_reserve(bo, false);
	if (unlikely(ret))
		return ret;

	ret = amdgpu_bo_pin_restricted(bo, domain, 0, 0);
	if (ret)
		pr_err("Error in Pinning BO to domain: %d\n", domain);

	amdgpu_bo_sync_wait(bo, AMDGPU_FENCE_OWNER_KFD, false);
	amdgpu_bo_unreserve(bo);

	return ret;
}

 
static void amdgpu_amdkfd_gpuvm_unpin_bo(struct amdgpu_bo *bo)
{
	int ret = 0;

	ret = amdgpu_bo_reserve(bo, false);
	if (unlikely(ret))
		return;

	amdgpu_bo_unpin(bo);
	amdgpu_bo_unreserve(bo);
}

int amdgpu_amdkfd_gpuvm_set_vm_pasid(struct amdgpu_device *adev,
				     struct amdgpu_vm *avm, u32 pasid)

{
	int ret;

	 
	if (avm->pasid) {
		amdgpu_pasid_free(avm->pasid);
		amdgpu_vm_set_pasid(adev, avm, 0);
	}

	ret = amdgpu_vm_set_pasid(adev, avm, pasid);
	if (ret)
		return ret;

	return 0;
}

int amdgpu_amdkfd_gpuvm_acquire_process_vm(struct amdgpu_device *adev,
					   struct amdgpu_vm *avm,
					   void **process_info,
					   struct dma_fence **ef)
{
	int ret;

	 
	if (avm->process_info)
		return -EINVAL;

	 
	ret = amdgpu_vm_make_compute(adev, avm);
	if (ret)
		return ret;

	 
	ret = init_kfd_vm(avm, process_info, ef);
	if (ret)
		return ret;

	amdgpu_vm_set_task_info(avm);

	return 0;
}

void amdgpu_amdkfd_gpuvm_destroy_cb(struct amdgpu_device *adev,
				    struct amdgpu_vm *vm)
{
	struct amdkfd_process_info *process_info = vm->process_info;

	if (!process_info)
		return;

	 
	mutex_lock(&process_info->lock);
	process_info->n_vms--;
	list_del(&vm->vm_list_node);
	mutex_unlock(&process_info->lock);

	vm->process_info = NULL;

	 
	if (!process_info->n_vms) {
		WARN_ON(!list_empty(&process_info->kfd_bo_list));
		WARN_ON(!list_empty(&process_info->userptr_valid_list));
		WARN_ON(!list_empty(&process_info->userptr_inval_list));

		dma_fence_put(&process_info->eviction_fence->base);
		cancel_delayed_work_sync(&process_info->restore_userptr_work);
		put_pid(process_info->pid);
		mutex_destroy(&process_info->lock);
		mutex_destroy(&process_info->notifier_lock);
		kfree(process_info);
	}
}

void amdgpu_amdkfd_gpuvm_release_process_vm(struct amdgpu_device *adev,
					    void *drm_priv)
{
	struct amdgpu_vm *avm;

	if (WARN_ON(!adev || !drm_priv))
		return;

	avm = drm_priv_to_vm(drm_priv);

	pr_debug("Releasing process vm %p\n", avm);

	 
	amdgpu_vm_release_compute(adev, avm);
}

uint64_t amdgpu_amdkfd_gpuvm_get_process_page_dir(void *drm_priv)
{
	struct amdgpu_vm *avm = drm_priv_to_vm(drm_priv);
	struct amdgpu_bo *pd = avm->root.bo;
	struct amdgpu_device *adev = amdgpu_ttm_adev(pd->tbo.bdev);

	if (adev->asic_type < CHIP_VEGA10)
		return avm->pd_phys_addr >> AMDGPU_GPU_PAGE_SHIFT;
	return avm->pd_phys_addr;
}

void amdgpu_amdkfd_block_mmu_notifications(void *p)
{
	struct amdkfd_process_info *pinfo = (struct amdkfd_process_info *)p;

	mutex_lock(&pinfo->lock);
	WRITE_ONCE(pinfo->block_mmu_notifications, true);
	mutex_unlock(&pinfo->lock);
}

int amdgpu_amdkfd_criu_resume(void *p)
{
	int ret = 0;
	struct amdkfd_process_info *pinfo = (struct amdkfd_process_info *)p;

	mutex_lock(&pinfo->lock);
	pr_debug("scheduling work\n");
	mutex_lock(&pinfo->notifier_lock);
	pinfo->evicted_bos++;
	mutex_unlock(&pinfo->notifier_lock);
	if (!READ_ONCE(pinfo->block_mmu_notifications)) {
		ret = -EINVAL;
		goto out_unlock;
	}
	WRITE_ONCE(pinfo->block_mmu_notifications, false);
	schedule_delayed_work(&pinfo->restore_userptr_work, 0);

out_unlock:
	mutex_unlock(&pinfo->lock);
	return ret;
}

size_t amdgpu_amdkfd_get_available_memory(struct amdgpu_device *adev,
					  uint8_t xcp_id)
{
	uint64_t reserved_for_pt =
		ESTIMATE_PT_SIZE(amdgpu_amdkfd_total_mem_size);
	ssize_t available;
	uint64_t vram_available, system_mem_available, ttm_mem_available;

	spin_lock(&kfd_mem_limit.mem_limit_lock);
	vram_available = KFD_XCP_MEMORY_SIZE(adev, xcp_id)
		- adev->kfd.vram_used_aligned[xcp_id]
		- atomic64_read(&adev->vram_pin_size)
		- reserved_for_pt;

	if (adev->gmc.is_app_apu) {
		system_mem_available = no_system_mem_limit ?
					kfd_mem_limit.max_system_mem_limit :
					kfd_mem_limit.max_system_mem_limit -
					kfd_mem_limit.system_mem_used;

		ttm_mem_available = kfd_mem_limit.max_ttm_mem_limit -
				kfd_mem_limit.ttm_mem_used;

		available = min3(system_mem_available, ttm_mem_available,
				 vram_available);
		available = ALIGN_DOWN(available, PAGE_SIZE);
	} else {
		available = ALIGN_DOWN(vram_available, VRAM_AVAILABLITY_ALIGN);
	}

	spin_unlock(&kfd_mem_limit.mem_limit_lock);

	if (available < 0)
		available = 0;

	return available;
}

int amdgpu_amdkfd_gpuvm_alloc_memory_of_gpu(
		struct amdgpu_device *adev, uint64_t va, uint64_t size,
		void *drm_priv, struct kgd_mem **mem,
		uint64_t *offset, uint32_t flags, bool criu_resume)
{
	struct amdgpu_vm *avm = drm_priv_to_vm(drm_priv);
	struct amdgpu_fpriv *fpriv = container_of(avm, struct amdgpu_fpriv, vm);
	enum ttm_bo_type bo_type = ttm_bo_type_device;
	struct sg_table *sg = NULL;
	uint64_t user_addr = 0;
	struct amdgpu_bo *bo;
	struct drm_gem_object *gobj = NULL;
	u32 domain, alloc_domain;
	uint64_t aligned_size;
	int8_t xcp_id = -1;
	u64 alloc_flags;
	int ret;

	 
	if (flags & KFD_IOC_ALLOC_MEM_FLAGS_VRAM) {
		domain = alloc_domain = AMDGPU_GEM_DOMAIN_VRAM;

		if (adev->gmc.is_app_apu) {
			domain = AMDGPU_GEM_DOMAIN_GTT;
			alloc_domain = AMDGPU_GEM_DOMAIN_GTT;
			alloc_flags = 0;
		} else {
			alloc_flags = AMDGPU_GEM_CREATE_VRAM_WIPE_ON_RELEASE;
			alloc_flags |= (flags & KFD_IOC_ALLOC_MEM_FLAGS_PUBLIC) ?
			AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED : 0;
		}
		xcp_id = fpriv->xcp_id == AMDGPU_XCP_NO_PARTITION ?
					0 : fpriv->xcp_id;
	} else if (flags & KFD_IOC_ALLOC_MEM_FLAGS_GTT) {
		domain = alloc_domain = AMDGPU_GEM_DOMAIN_GTT;
		alloc_flags = 0;
	} else {
		domain = AMDGPU_GEM_DOMAIN_GTT;
		alloc_domain = AMDGPU_GEM_DOMAIN_CPU;
		alloc_flags = AMDGPU_GEM_CREATE_PREEMPTIBLE;

		if (flags & KFD_IOC_ALLOC_MEM_FLAGS_USERPTR) {
			if (!offset || !*offset)
				return -EINVAL;
			user_addr = untagged_addr(*offset);
		} else if (flags & (KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL |
				    KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP)) {
			bo_type = ttm_bo_type_sg;
			if (size > UINT_MAX)
				return -EINVAL;
			sg = create_sg_table(*offset, size);
			if (!sg)
				return -ENOMEM;
		} else {
			return -EINVAL;
		}
	}

	if (flags & KFD_IOC_ALLOC_MEM_FLAGS_COHERENT)
		alloc_flags |= AMDGPU_GEM_CREATE_COHERENT;
	if (flags & KFD_IOC_ALLOC_MEM_FLAGS_UNCACHED)
		alloc_flags |= AMDGPU_GEM_CREATE_UNCACHED;

	*mem = kzalloc(sizeof(struct kgd_mem), GFP_KERNEL);
	if (!*mem) {
		ret = -ENOMEM;
		goto err;
	}
	INIT_LIST_HEAD(&(*mem)->attachments);
	mutex_init(&(*mem)->lock);
	(*mem)->aql_queue = !!(flags & KFD_IOC_ALLOC_MEM_FLAGS_AQL_QUEUE_MEM);

	 
	if ((*mem)->aql_queue)
		size >>= 1;
	aligned_size = PAGE_ALIGN(size);

	(*mem)->alloc_flags = flags;

	amdgpu_sync_create(&(*mem)->sync);

	ret = amdgpu_amdkfd_reserve_mem_limit(adev, aligned_size, flags,
					      xcp_id);
	if (ret) {
		pr_debug("Insufficient memory\n");
		goto err_reserve_limit;
	}

	pr_debug("\tcreate BO VA 0x%llx size 0x%llx domain %s xcp_id %d\n",
		 va, (*mem)->aql_queue ? size << 1 : size,
		 domain_string(alloc_domain), xcp_id);

	ret = amdgpu_gem_object_create(adev, aligned_size, 1, alloc_domain, alloc_flags,
				       bo_type, NULL, &gobj, xcp_id + 1);
	if (ret) {
		pr_debug("Failed to create BO on domain %s. ret %d\n",
			 domain_string(alloc_domain), ret);
		goto err_bo_create;
	}
	ret = drm_vma_node_allow(&gobj->vma_node, drm_priv);
	if (ret) {
		pr_debug("Failed to allow vma node access. ret %d\n", ret);
		goto err_node_allow;
	}
	bo = gem_to_amdgpu_bo(gobj);
	if (bo_type == ttm_bo_type_sg) {
		bo->tbo.sg = sg;
		bo->tbo.ttm->sg = sg;
	}
	bo->kfd_bo = *mem;
	(*mem)->bo = bo;
	if (user_addr)
		bo->flags |= AMDGPU_AMDKFD_CREATE_USERPTR_BO;

	(*mem)->va = va;
	(*mem)->domain = domain;
	(*mem)->mapped_to_gpu_memory = 0;
	(*mem)->process_info = avm->process_info;

	add_kgd_mem_to_kfd_bo_list(*mem, avm->process_info, user_addr);

	if (user_addr) {
		pr_debug("creating userptr BO for user_addr = %llx\n", user_addr);
		ret = init_user_pages(*mem, user_addr, criu_resume);
		if (ret)
			goto allocate_init_user_pages_failed;
	} else  if (flags & (KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL |
				KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP)) {
		ret = amdgpu_amdkfd_gpuvm_pin_bo(bo, AMDGPU_GEM_DOMAIN_GTT);
		if (ret) {
			pr_err("Pinning MMIO/DOORBELL BO during ALLOC FAILED\n");
			goto err_pin_bo;
		}
		bo->allowed_domains = AMDGPU_GEM_DOMAIN_GTT;
		bo->preferred_domains = AMDGPU_GEM_DOMAIN_GTT;
	}

	if (offset)
		*offset = amdgpu_bo_mmap_offset(bo);

	return 0;

allocate_init_user_pages_failed:
err_pin_bo:
	remove_kgd_mem_from_kfd_bo_list(*mem, avm->process_info);
	drm_vma_node_revoke(&gobj->vma_node, drm_priv);
err_node_allow:
	 
	goto err_reserve_limit;
err_bo_create:
	amdgpu_amdkfd_unreserve_mem_limit(adev, aligned_size, flags, xcp_id);
err_reserve_limit:
	mutex_destroy(&(*mem)->lock);
	if (gobj)
		drm_gem_object_put(gobj);
	else
		kfree(*mem);
err:
	if (sg) {
		sg_free_table(sg);
		kfree(sg);
	}
	return ret;
}

int amdgpu_amdkfd_gpuvm_free_memory_of_gpu(
		struct amdgpu_device *adev, struct kgd_mem *mem, void *drm_priv,
		uint64_t *size)
{
	struct amdkfd_process_info *process_info = mem->process_info;
	unsigned long bo_size = mem->bo->tbo.base.size;
	bool use_release_notifier = (mem->bo->kfd_bo == mem);
	struct kfd_mem_attachment *entry, *tmp;
	struct bo_vm_reservation_context ctx;
	unsigned int mapped_to_gpu_memory;
	int ret;
	bool is_imported = false;

	mutex_lock(&mem->lock);

	 
	if (mem->alloc_flags &
	    (KFD_IOC_ALLOC_MEM_FLAGS_DOORBELL |
	     KFD_IOC_ALLOC_MEM_FLAGS_MMIO_REMAP)) {
		amdgpu_amdkfd_gpuvm_unpin_bo(mem->bo);
	}

	mapped_to_gpu_memory = mem->mapped_to_gpu_memory;
	is_imported = mem->is_imported;
	mutex_unlock(&mem->lock);
	 

	if (mapped_to_gpu_memory > 0) {
		pr_debug("BO VA 0x%llx size 0x%lx is still mapped.\n",
				mem->va, bo_size);
		return -EBUSY;
	}

	 
	mutex_lock(&process_info->lock);
	list_del(&mem->validate_list);
	mutex_unlock(&process_info->lock);

	 
	if (amdgpu_ttm_tt_get_usermm(mem->bo->tbo.ttm)) {
		amdgpu_hmm_unregister(mem->bo);
		mutex_lock(&process_info->notifier_lock);
		amdgpu_ttm_tt_discard_user_pages(mem->bo->tbo.ttm, mem->range);
		mutex_unlock(&process_info->notifier_lock);
	}

	ret = reserve_bo_and_cond_vms(mem, NULL, BO_VM_ALL, &ctx);
	if (unlikely(ret))
		return ret;

	 
	amdgpu_amdkfd_remove_eviction_fence(mem->bo,
					process_info->eviction_fence);
	pr_debug("Release VA 0x%llx - 0x%llx\n", mem->va,
		mem->va + bo_size * (1 + mem->aql_queue));

	 
	list_for_each_entry_safe(entry, tmp, &mem->attachments, list)
		kfd_mem_detach(entry);

	ret = unreserve_bo_and_vms(&ctx, false, false);

	 
	amdgpu_sync_free(&mem->sync);

	 
	if (mem->bo->tbo.sg) {
		sg_free_table(mem->bo->tbo.sg);
		kfree(mem->bo->tbo.sg);
	}

	 
	if (size) {
		if (!is_imported &&
		   (mem->bo->preferred_domains == AMDGPU_GEM_DOMAIN_VRAM ||
		   (adev->gmc.is_app_apu &&
		    mem->bo->preferred_domains == AMDGPU_GEM_DOMAIN_GTT)))
			*size = bo_size;
		else
			*size = 0;
	}

	 
	drm_vma_node_revoke(&mem->bo->tbo.base.vma_node, drm_priv);
	if (mem->dmabuf)
		dma_buf_put(mem->dmabuf);
	mutex_destroy(&mem->lock);

	 
	drm_gem_object_put(&mem->bo->tbo.base);

	 
	if (!use_release_notifier)
		kfree(mem);

	return ret;
}

int amdgpu_amdkfd_gpuvm_map_memory_to_gpu(
		struct amdgpu_device *adev, struct kgd_mem *mem,
		void *drm_priv)
{
	struct amdgpu_vm *avm = drm_priv_to_vm(drm_priv);
	int ret;
	struct amdgpu_bo *bo;
	uint32_t domain;
	struct kfd_mem_attachment *entry;
	struct bo_vm_reservation_context ctx;
	unsigned long bo_size;
	bool is_invalid_userptr = false;

	bo = mem->bo;
	if (!bo) {
		pr_err("Invalid BO when mapping memory to GPU\n");
		return -EINVAL;
	}

	 
	mutex_lock(&mem->process_info->lock);

	 
	if (amdgpu_ttm_tt_get_usermm(bo->tbo.ttm)) {
		mutex_lock(&mem->process_info->notifier_lock);
		is_invalid_userptr = !!mem->invalid;
		mutex_unlock(&mem->process_info->notifier_lock);
	}

	mutex_lock(&mem->lock);

	domain = mem->domain;
	bo_size = bo->tbo.base.size;

	pr_debug("Map VA 0x%llx - 0x%llx to vm %p domain %s\n",
			mem->va,
			mem->va + bo_size * (1 + mem->aql_queue),
			avm, domain_string(domain));

	if (!kfd_mem_is_attached(avm, mem)) {
		ret = kfd_mem_attach(adev, mem, avm, mem->aql_queue);
		if (ret)
			goto out;
	}

	ret = reserve_bo_and_vm(mem, avm, &ctx);
	if (unlikely(ret))
		goto out;

	 
	if (amdgpu_ttm_tt_get_usermm(bo->tbo.ttm) &&
	    bo->tbo.resource->mem_type == TTM_PL_SYSTEM)
		is_invalid_userptr = true;

	ret = vm_validate_pt_pd_bos(avm);
	if (unlikely(ret))
		goto out_unreserve;

	if (mem->mapped_to_gpu_memory == 0 &&
	    !amdgpu_ttm_tt_get_usermm(bo->tbo.ttm)) {
		 
		ret = amdgpu_amdkfd_bo_validate(bo, domain, true);
		if (ret) {
			pr_debug("Validate failed\n");
			goto out_unreserve;
		}
	}

	list_for_each_entry(entry, &mem->attachments, list) {
		if (entry->bo_va->base.vm != avm || entry->is_mapped)
			continue;

		pr_debug("\t map VA 0x%llx - 0x%llx in entry %p\n",
			 entry->va, entry->va + bo_size, entry);

		ret = map_bo_to_gpuvm(mem, entry, ctx.sync,
				      is_invalid_userptr);
		if (ret) {
			pr_err("Failed to map bo to gpuvm\n");
			goto out_unreserve;
		}

		ret = vm_update_pds(avm, ctx.sync);
		if (ret) {
			pr_err("Failed to update page directories\n");
			goto out_unreserve;
		}

		entry->is_mapped = true;
		mem->mapped_to_gpu_memory++;
		pr_debug("\t INC mapping count %d\n",
			 mem->mapped_to_gpu_memory);
	}

	if (!amdgpu_ttm_tt_get_usermm(bo->tbo.ttm) && !bo->tbo.pin_count)
		dma_resv_add_fence(bo->tbo.base.resv,
				   &avm->process_info->eviction_fence->base,
				   DMA_RESV_USAGE_BOOKKEEP);
	ret = unreserve_bo_and_vms(&ctx, false, false);

	goto out;

out_unreserve:
	unreserve_bo_and_vms(&ctx, false, false);
out:
	mutex_unlock(&mem->process_info->lock);
	mutex_unlock(&mem->lock);
	return ret;
}

int amdgpu_amdkfd_gpuvm_unmap_memory_from_gpu(
		struct amdgpu_device *adev, struct kgd_mem *mem, void *drm_priv)
{
	struct amdgpu_vm *avm = drm_priv_to_vm(drm_priv);
	struct amdkfd_process_info *process_info = avm->process_info;
	unsigned long bo_size = mem->bo->tbo.base.size;
	struct kfd_mem_attachment *entry;
	struct bo_vm_reservation_context ctx;
	int ret;

	mutex_lock(&mem->lock);

	ret = reserve_bo_and_cond_vms(mem, avm, BO_VM_MAPPED, &ctx);
	if (unlikely(ret))
		goto out;
	 
	if (ctx.n_vms == 0) {
		ret = -EINVAL;
		goto unreserve_out;
	}

	ret = vm_validate_pt_pd_bos(avm);
	if (unlikely(ret))
		goto unreserve_out;

	pr_debug("Unmap VA 0x%llx - 0x%llx from vm %p\n",
		mem->va,
		mem->va + bo_size * (1 + mem->aql_queue),
		avm);

	list_for_each_entry(entry, &mem->attachments, list) {
		if (entry->bo_va->base.vm != avm || !entry->is_mapped)
			continue;

		pr_debug("\t unmap VA 0x%llx - 0x%llx from entry %p\n",
			 entry->va, entry->va + bo_size, entry);

		unmap_bo_from_gpuvm(mem, entry, ctx.sync);
		entry->is_mapped = false;

		mem->mapped_to_gpu_memory--;
		pr_debug("\t DEC mapping count %d\n",
			 mem->mapped_to_gpu_memory);
	}

	 
	if (mem->mapped_to_gpu_memory == 0 &&
	    !amdgpu_ttm_tt_get_usermm(mem->bo->tbo.ttm) &&
	    !mem->bo->tbo.pin_count)
		amdgpu_amdkfd_remove_eviction_fence(mem->bo,
						process_info->eviction_fence);

unreserve_out:
	unreserve_bo_and_vms(&ctx, false, false);
out:
	mutex_unlock(&mem->lock);
	return ret;
}

int amdgpu_amdkfd_gpuvm_sync_memory(
		struct amdgpu_device *adev, struct kgd_mem *mem, bool intr)
{
	struct amdgpu_sync sync;
	int ret;

	amdgpu_sync_create(&sync);

	mutex_lock(&mem->lock);
	amdgpu_sync_clone(&mem->sync, &sync);
	mutex_unlock(&mem->lock);

	ret = amdgpu_sync_wait(&sync, intr);
	amdgpu_sync_free(&sync);
	return ret;
}

 
int amdgpu_amdkfd_map_gtt_bo_to_gart(struct amdgpu_device *adev, struct amdgpu_bo *bo)
{
	int ret;

	ret = amdgpu_bo_reserve(bo, true);
	if (ret) {
		pr_err("Failed to reserve bo. ret %d\n", ret);
		goto err_reserve_bo_failed;
	}

	ret = amdgpu_bo_pin(bo, AMDGPU_GEM_DOMAIN_GTT);
	if (ret) {
		pr_err("Failed to pin bo. ret %d\n", ret);
		goto err_pin_bo_failed;
	}

	ret = amdgpu_ttm_alloc_gart(&bo->tbo);
	if (ret) {
		pr_err("Failed to bind bo to GART. ret %d\n", ret);
		goto err_map_bo_gart_failed;
	}

	amdgpu_amdkfd_remove_eviction_fence(
		bo, bo->vm_bo->vm->process_info->eviction_fence);

	amdgpu_bo_unreserve(bo);

	bo = amdgpu_bo_ref(bo);

	return 0;

err_map_bo_gart_failed:
	amdgpu_bo_unpin(bo);
err_pin_bo_failed:
	amdgpu_bo_unreserve(bo);
err_reserve_bo_failed:

	return ret;
}

 
int amdgpu_amdkfd_gpuvm_map_gtt_bo_to_kernel(struct kgd_mem *mem,
					     void **kptr, uint64_t *size)
{
	int ret;
	struct amdgpu_bo *bo = mem->bo;

	if (amdgpu_ttm_tt_get_usermm(bo->tbo.ttm)) {
		pr_err("userptr can't be mapped to kernel\n");
		return -EINVAL;
	}

	mutex_lock(&mem->process_info->lock);

	ret = amdgpu_bo_reserve(bo, true);
	if (ret) {
		pr_err("Failed to reserve bo. ret %d\n", ret);
		goto bo_reserve_failed;
	}

	ret = amdgpu_bo_pin(bo, AMDGPU_GEM_DOMAIN_GTT);
	if (ret) {
		pr_err("Failed to pin bo. ret %d\n", ret);
		goto pin_failed;
	}

	ret = amdgpu_bo_kmap(bo, kptr);
	if (ret) {
		pr_err("Failed to map bo to kernel. ret %d\n", ret);
		goto kmap_failed;
	}

	amdgpu_amdkfd_remove_eviction_fence(
		bo, mem->process_info->eviction_fence);

	if (size)
		*size = amdgpu_bo_size(bo);

	amdgpu_bo_unreserve(bo);

	mutex_unlock(&mem->process_info->lock);
	return 0;

kmap_failed:
	amdgpu_bo_unpin(bo);
pin_failed:
	amdgpu_bo_unreserve(bo);
bo_reserve_failed:
	mutex_unlock(&mem->process_info->lock);

	return ret;
}

 
void amdgpu_amdkfd_gpuvm_unmap_gtt_bo_from_kernel(struct kgd_mem *mem)
{
	struct amdgpu_bo *bo = mem->bo;

	amdgpu_bo_reserve(bo, true);
	amdgpu_bo_kunmap(bo);
	amdgpu_bo_unpin(bo);
	amdgpu_bo_unreserve(bo);
}

int amdgpu_amdkfd_gpuvm_get_vm_fault_info(struct amdgpu_device *adev,
					  struct kfd_vm_fault_info *mem)
{
	if (atomic_read(&adev->gmc.vm_fault_info_updated) == 1) {
		*mem = *adev->gmc.vm_fault_info;
		mb();  
		atomic_set(&adev->gmc.vm_fault_info_updated, 0);
	}
	return 0;
}

int amdgpu_amdkfd_gpuvm_import_dmabuf(struct amdgpu_device *adev,
				      struct dma_buf *dma_buf,
				      uint64_t va, void *drm_priv,
				      struct kgd_mem **mem, uint64_t *size,
				      uint64_t *mmap_offset)
{
	struct amdgpu_vm *avm = drm_priv_to_vm(drm_priv);
	struct drm_gem_object *obj;
	struct amdgpu_bo *bo;
	int ret;

	obj = amdgpu_gem_prime_import(adev_to_drm(adev), dma_buf);
	if (IS_ERR(obj))
		return PTR_ERR(obj);

	bo = gem_to_amdgpu_bo(obj);
	if (!(bo->preferred_domains & (AMDGPU_GEM_DOMAIN_VRAM |
				    AMDGPU_GEM_DOMAIN_GTT))) {
		 
		ret = -EINVAL;
		goto err_put_obj;
	}

	*mem = kzalloc(sizeof(struct kgd_mem), GFP_KERNEL);
	if (!*mem) {
		ret = -ENOMEM;
		goto err_put_obj;
	}

	ret = drm_vma_node_allow(&obj->vma_node, drm_priv);
	if (ret)
		goto err_free_mem;

	if (size)
		*size = amdgpu_bo_size(bo);

	if (mmap_offset)
		*mmap_offset = amdgpu_bo_mmap_offset(bo);

	INIT_LIST_HEAD(&(*mem)->attachments);
	mutex_init(&(*mem)->lock);

	(*mem)->alloc_flags =
		((bo->preferred_domains & AMDGPU_GEM_DOMAIN_VRAM) ?
		KFD_IOC_ALLOC_MEM_FLAGS_VRAM : KFD_IOC_ALLOC_MEM_FLAGS_GTT)
		| KFD_IOC_ALLOC_MEM_FLAGS_WRITABLE
		| KFD_IOC_ALLOC_MEM_FLAGS_EXECUTABLE;

	get_dma_buf(dma_buf);
	(*mem)->dmabuf = dma_buf;
	(*mem)->bo = bo;
	(*mem)->va = va;
	(*mem)->domain = (bo->preferred_domains & AMDGPU_GEM_DOMAIN_VRAM) && !adev->gmc.is_app_apu ?
		AMDGPU_GEM_DOMAIN_VRAM : AMDGPU_GEM_DOMAIN_GTT;

	(*mem)->mapped_to_gpu_memory = 0;
	(*mem)->process_info = avm->process_info;
	add_kgd_mem_to_kfd_bo_list(*mem, avm->process_info, false);
	amdgpu_sync_create(&(*mem)->sync);
	(*mem)->is_imported = true;

	return 0;

err_free_mem:
	kfree(*mem);
err_put_obj:
	drm_gem_object_put(obj);
	return ret;
}

int amdgpu_amdkfd_gpuvm_export_dmabuf(struct kgd_mem *mem,
				      struct dma_buf **dma_buf)
{
	int ret;

	mutex_lock(&mem->lock);
	ret = kfd_mem_export_dmabuf(mem);
	if (ret)
		goto out;

	get_dma_buf(mem->dmabuf);
	*dma_buf = mem->dmabuf;
out:
	mutex_unlock(&mem->lock);
	return ret;
}

 
int amdgpu_amdkfd_evict_userptr(struct mmu_interval_notifier *mni,
				unsigned long cur_seq, struct kgd_mem *mem)
{
	struct amdkfd_process_info *process_info = mem->process_info;
	int r = 0;

	 
	if (READ_ONCE(process_info->block_mmu_notifications))
		return 0;

	mutex_lock(&process_info->notifier_lock);
	mmu_interval_set_seq(mni, cur_seq);

	mem->invalid++;
	if (++process_info->evicted_bos == 1) {
		 
		r = kgd2kfd_quiesce_mm(mni->mm,
				       KFD_QUEUE_EVICTION_TRIGGER_USERPTR);
		if (r)
			pr_err("Failed to quiesce KFD\n");
		schedule_delayed_work(&process_info->restore_userptr_work,
			msecs_to_jiffies(AMDGPU_USERPTR_RESTORE_DELAY_MS));
	}
	mutex_unlock(&process_info->notifier_lock);

	return r;
}

 
static int update_invalid_user_pages(struct amdkfd_process_info *process_info,
				     struct mm_struct *mm)
{
	struct kgd_mem *mem, *tmp_mem;
	struct amdgpu_bo *bo;
	struct ttm_operation_ctx ctx = { false, false };
	uint32_t invalid;
	int ret = 0;

	mutex_lock(&process_info->notifier_lock);

	 
	list_for_each_entry_safe(mem, tmp_mem,
				 &process_info->userptr_valid_list,
				 validate_list)
		if (mem->invalid)
			list_move_tail(&mem->validate_list,
				       &process_info->userptr_inval_list);

	 
	list_for_each_entry(mem, &process_info->userptr_inval_list,
			    validate_list) {
		invalid = mem->invalid;
		if (!invalid)
			 
			continue;

		bo = mem->bo;

		amdgpu_ttm_tt_discard_user_pages(bo->tbo.ttm, mem->range);
		mem->range = NULL;

		 
		mutex_unlock(&process_info->notifier_lock);

		 
		if (bo->tbo.resource->mem_type != TTM_PL_SYSTEM) {
			if (amdgpu_bo_reserve(bo, true))
				return -EAGAIN;
			amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_CPU);
			ret = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
			amdgpu_bo_unreserve(bo);
			if (ret) {
				pr_err("%s: Failed to invalidate userptr BO\n",
				       __func__);
				return -EAGAIN;
			}
		}

		 
		ret = amdgpu_ttm_tt_get_user_pages(bo, bo->tbo.ttm->pages,
						   &mem->range);
		if (ret) {
			pr_debug("Failed %d to get user pages\n", ret);

			 
			if (ret != -EFAULT)
				return ret;

			ret = 0;
		}

		mutex_lock(&process_info->notifier_lock);

		 
		if (mem->invalid != invalid) {
			ret = -EAGAIN;
			goto unlock_out;
		}
		  
		if (mem->range)
			mem->invalid = 0;
	}

unlock_out:
	mutex_unlock(&process_info->notifier_lock);

	return ret;
}

 
static int validate_invalid_user_pages(struct amdkfd_process_info *process_info)
{
	struct ttm_operation_ctx ctx = { false, false };
	struct amdgpu_sync sync;
	struct drm_exec exec;

	struct amdgpu_vm *peer_vm;
	struct kgd_mem *mem, *tmp_mem;
	struct amdgpu_bo *bo;
	int ret;

	amdgpu_sync_create(&sync);

	drm_exec_init(&exec, 0);
	 
	drm_exec_until_all_locked(&exec) {
		 
		list_for_each_entry(peer_vm, &process_info->vm_list_head,
				    vm_list_node) {
			ret = amdgpu_vm_lock_pd(peer_vm, &exec, 2);
			drm_exec_retry_on_contention(&exec);
			if (unlikely(ret))
				goto unreserve_out;
		}

		 
		list_for_each_entry(mem, &process_info->userptr_inval_list,
				    validate_list) {
			struct drm_gem_object *gobj;

			gobj = &mem->bo->tbo.base;
			ret = drm_exec_prepare_obj(&exec, gobj, 1);
			drm_exec_retry_on_contention(&exec);
			if (unlikely(ret))
				goto unreserve_out;
		}
	}

	ret = process_validate_vms(process_info);
	if (ret)
		goto unreserve_out;

	 
	list_for_each_entry_safe(mem, tmp_mem,
				 &process_info->userptr_inval_list,
				 validate_list) {
		struct kfd_mem_attachment *attachment;

		bo = mem->bo;

		 
		if (bo->tbo.ttm->pages[0]) {
			amdgpu_bo_placement_from_domain(bo, mem->domain);
			ret = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
			if (ret) {
				pr_err("%s: failed to validate BO\n", __func__);
				goto unreserve_out;
			}
		}

		 
		list_for_each_entry(attachment, &mem->attachments, list) {
			if (!attachment->is_mapped)
				continue;

			kfd_mem_dmaunmap_attachment(mem, attachment);
			ret = update_gpuvm_pte(mem, attachment, &sync);
			if (ret) {
				pr_err("%s: update PTE failed\n", __func__);
				 
				mutex_lock(&process_info->notifier_lock);
				mem->invalid++;
				mutex_unlock(&process_info->notifier_lock);
				goto unreserve_out;
			}
		}
	}

	 
	ret = process_update_pds(process_info, &sync);

unreserve_out:
	drm_exec_fini(&exec);
	amdgpu_sync_wait(&sync, false);
	amdgpu_sync_free(&sync);

	return ret;
}

 
static int confirm_valid_user_pages_locked(struct amdkfd_process_info *process_info)
{
	struct kgd_mem *mem, *tmp_mem;
	int ret = 0;

	list_for_each_entry_safe(mem, tmp_mem,
				 &process_info->userptr_inval_list,
				 validate_list) {
		bool valid;

		 
		if (!mem->range)
			 continue;

		 
		valid = amdgpu_ttm_tt_get_user_pages_done(
					mem->bo->tbo.ttm, mem->range);

		mem->range = NULL;
		if (!valid) {
			WARN(!mem->invalid, "Invalid BO not marked invalid");
			ret = -EAGAIN;
			continue;
		}

		if (mem->invalid) {
			WARN(1, "Valid BO is marked invalid");
			ret = -EAGAIN;
			continue;
		}

		list_move_tail(&mem->validate_list,
			       &process_info->userptr_valid_list);
	}

	return ret;
}

 
static void amdgpu_amdkfd_restore_userptr_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct amdkfd_process_info *process_info =
		container_of(dwork, struct amdkfd_process_info,
			     restore_userptr_work);
	struct task_struct *usertask;
	struct mm_struct *mm;
	uint32_t evicted_bos;

	mutex_lock(&process_info->notifier_lock);
	evicted_bos = process_info->evicted_bos;
	mutex_unlock(&process_info->notifier_lock);
	if (!evicted_bos)
		return;

	 
	usertask = get_pid_task(process_info->pid, PIDTYPE_PID);
	if (!usertask)
		return;
	mm = get_task_mm(usertask);
	if (!mm) {
		put_task_struct(usertask);
		return;
	}

	mutex_lock(&process_info->lock);

	if (update_invalid_user_pages(process_info, mm))
		goto unlock_out;
	 
	if (!list_empty(&process_info->userptr_inval_list)) {
		if (validate_invalid_user_pages(process_info))
			goto unlock_out;
	}
	 
	mutex_lock(&process_info->notifier_lock);
	if (process_info->evicted_bos != evicted_bos)
		goto unlock_notifier_out;

	if (confirm_valid_user_pages_locked(process_info)) {
		WARN(1, "User pages unexpectedly invalid");
		goto unlock_notifier_out;
	}

	process_info->evicted_bos = evicted_bos = 0;

	if (kgd2kfd_resume_mm(mm)) {
		pr_err("%s: Failed to resume KFD\n", __func__);
		 
	}

unlock_notifier_out:
	mutex_unlock(&process_info->notifier_lock);
unlock_out:
	mutex_unlock(&process_info->lock);

	 
	if (evicted_bos) {
		schedule_delayed_work(&process_info->restore_userptr_work,
			msecs_to_jiffies(AMDGPU_USERPTR_RESTORE_DELAY_MS));

		kfd_smi_event_queue_restore_rescheduled(mm);
	}
	mmput(mm);
	put_task_struct(usertask);
}

 
int amdgpu_amdkfd_gpuvm_restore_process_bos(void *info, struct dma_fence **ef)
{
	struct amdkfd_process_info *process_info = info;
	struct amdgpu_vm *peer_vm;
	struct kgd_mem *mem;
	struct amdgpu_amdkfd_fence *new_fence;
	struct list_head duplicate_save;
	struct amdgpu_sync sync_obj;
	unsigned long failed_size = 0;
	unsigned long total_size = 0;
	struct drm_exec exec;
	int ret;

	INIT_LIST_HEAD(&duplicate_save);

	mutex_lock(&process_info->lock);

	drm_exec_init(&exec, 0);
	drm_exec_until_all_locked(&exec) {
		list_for_each_entry(peer_vm, &process_info->vm_list_head,
				    vm_list_node) {
			ret = amdgpu_vm_lock_pd(peer_vm, &exec, 2);
			drm_exec_retry_on_contention(&exec);
			if (unlikely(ret))
				goto ttm_reserve_fail;
		}

		 
		list_for_each_entry(mem, &process_info->kfd_bo_list,
				    validate_list) {
			struct drm_gem_object *gobj;

			gobj = &mem->bo->tbo.base;
			ret = drm_exec_prepare_obj(&exec, gobj, 1);
			drm_exec_retry_on_contention(&exec);
			if (unlikely(ret))
				goto ttm_reserve_fail;
		}
	}

	amdgpu_sync_create(&sync_obj);

	 
	ret = process_validate_vms(process_info);
	if (ret)
		goto validate_map_fail;

	ret = process_sync_pds_resv(process_info, &sync_obj);
	if (ret) {
		pr_debug("Memory eviction: Failed to sync to PD BO moving fence. Try again\n");
		goto validate_map_fail;
	}

	 
	list_for_each_entry(mem, &process_info->kfd_bo_list,
			    validate_list) {

		struct amdgpu_bo *bo = mem->bo;
		uint32_t domain = mem->domain;
		struct kfd_mem_attachment *attachment;
		struct dma_resv_iter cursor;
		struct dma_fence *fence;

		total_size += amdgpu_bo_size(bo);

		ret = amdgpu_amdkfd_bo_validate(bo, domain, false);
		if (ret) {
			pr_debug("Memory eviction: Validate BOs failed\n");
			failed_size += amdgpu_bo_size(bo);
			ret = amdgpu_amdkfd_bo_validate(bo,
						AMDGPU_GEM_DOMAIN_GTT, false);
			if (ret) {
				pr_debug("Memory eviction: Try again\n");
				goto validate_map_fail;
			}
		}
		dma_resv_for_each_fence(&cursor, bo->tbo.base.resv,
					DMA_RESV_USAGE_KERNEL, fence) {
			ret = amdgpu_sync_fence(&sync_obj, fence);
			if (ret) {
				pr_debug("Memory eviction: Sync BO fence failed. Try again\n");
				goto validate_map_fail;
			}
		}
		list_for_each_entry(attachment, &mem->attachments, list) {
			if (!attachment->is_mapped)
				continue;

			if (attachment->bo_va->base.bo->tbo.pin_count)
				continue;

			kfd_mem_dmaunmap_attachment(mem, attachment);
			ret = update_gpuvm_pte(mem, attachment, &sync_obj);
			if (ret) {
				pr_debug("Memory eviction: update PTE failed. Try again\n");
				goto validate_map_fail;
			}
		}
	}

	if (failed_size)
		pr_debug("0x%lx/0x%lx in system\n", failed_size, total_size);

	 
	ret = process_update_pds(process_info, &sync_obj);
	if (ret) {
		pr_debug("Memory eviction: update PDs failed. Try again\n");
		goto validate_map_fail;
	}

	 
	amdgpu_sync_wait(&sync_obj, false);

	 
	new_fence = amdgpu_amdkfd_fence_create(
				process_info->eviction_fence->base.context,
				process_info->eviction_fence->mm,
				NULL);
	if (!new_fence) {
		pr_err("Failed to create eviction fence\n");
		ret = -ENOMEM;
		goto validate_map_fail;
	}
	dma_fence_put(&process_info->eviction_fence->base);
	process_info->eviction_fence = new_fence;
	*ef = dma_fence_get(&new_fence->base);

	 
	list_for_each_entry(mem, &process_info->kfd_bo_list, validate_list) {
		if (mem->bo->tbo.pin_count)
			continue;

		dma_resv_add_fence(mem->bo->tbo.base.resv,
				   &process_info->eviction_fence->base,
				   DMA_RESV_USAGE_BOOKKEEP);
	}
	 
	list_for_each_entry(peer_vm, &process_info->vm_list_head,
			    vm_list_node) {
		struct amdgpu_bo *bo = peer_vm->root.bo;

		dma_resv_add_fence(bo->tbo.base.resv,
				   &process_info->eviction_fence->base,
				   DMA_RESV_USAGE_BOOKKEEP);
	}

validate_map_fail:
	amdgpu_sync_free(&sync_obj);
ttm_reserve_fail:
	drm_exec_fini(&exec);
	mutex_unlock(&process_info->lock);
	return ret;
}

int amdgpu_amdkfd_add_gws_to_process(void *info, void *gws, struct kgd_mem **mem)
{
	struct amdkfd_process_info *process_info = (struct amdkfd_process_info *)info;
	struct amdgpu_bo *gws_bo = (struct amdgpu_bo *)gws;
	int ret;

	if (!info || !gws)
		return -EINVAL;

	*mem = kzalloc(sizeof(struct kgd_mem), GFP_KERNEL);
	if (!*mem)
		return -ENOMEM;

	mutex_init(&(*mem)->lock);
	INIT_LIST_HEAD(&(*mem)->attachments);
	(*mem)->bo = amdgpu_bo_ref(gws_bo);
	(*mem)->domain = AMDGPU_GEM_DOMAIN_GWS;
	(*mem)->process_info = process_info;
	add_kgd_mem_to_kfd_bo_list(*mem, process_info, false);
	amdgpu_sync_create(&(*mem)->sync);


	 
	mutex_lock(&(*mem)->process_info->lock);
	ret = amdgpu_bo_reserve(gws_bo, false);
	if (unlikely(ret)) {
		pr_err("Reserve gws bo failed %d\n", ret);
		goto bo_reservation_failure;
	}

	ret = amdgpu_amdkfd_bo_validate(gws_bo, AMDGPU_GEM_DOMAIN_GWS, true);
	if (ret) {
		pr_err("GWS BO validate failed %d\n", ret);
		goto bo_validation_failure;
	}
	 
	ret = dma_resv_reserve_fences(gws_bo->tbo.base.resv, 1);
	if (ret)
		goto reserve_shared_fail;
	dma_resv_add_fence(gws_bo->tbo.base.resv,
			   &process_info->eviction_fence->base,
			   DMA_RESV_USAGE_BOOKKEEP);
	amdgpu_bo_unreserve(gws_bo);
	mutex_unlock(&(*mem)->process_info->lock);

	return ret;

reserve_shared_fail:
bo_validation_failure:
	amdgpu_bo_unreserve(gws_bo);
bo_reservation_failure:
	mutex_unlock(&(*mem)->process_info->lock);
	amdgpu_sync_free(&(*mem)->sync);
	remove_kgd_mem_from_kfd_bo_list(*mem, process_info);
	amdgpu_bo_unref(&gws_bo);
	mutex_destroy(&(*mem)->lock);
	kfree(*mem);
	*mem = NULL;
	return ret;
}

int amdgpu_amdkfd_remove_gws_from_process(void *info, void *mem)
{
	int ret;
	struct amdkfd_process_info *process_info = (struct amdkfd_process_info *)info;
	struct kgd_mem *kgd_mem = (struct kgd_mem *)mem;
	struct amdgpu_bo *gws_bo = kgd_mem->bo;

	 
	remove_kgd_mem_from_kfd_bo_list(kgd_mem, process_info);

	ret = amdgpu_bo_reserve(gws_bo, false);
	if (unlikely(ret)) {
		pr_err("Reserve gws bo failed %d\n", ret);
		 
		return ret;
	}
	amdgpu_amdkfd_remove_eviction_fence(gws_bo,
			process_info->eviction_fence);
	amdgpu_bo_unreserve(gws_bo);
	amdgpu_sync_free(&kgd_mem->sync);
	amdgpu_bo_unref(&gws_bo);
	mutex_destroy(&kgd_mem->lock);
	kfree(mem);
	return 0;
}

 
int amdgpu_amdkfd_get_tile_config(struct amdgpu_device *adev,
				struct tile_config *config)
{
	config->gb_addr_config = adev->gfx.config.gb_addr_config;
	config->tile_config_ptr = adev->gfx.config.tile_mode_array;
	config->num_tile_configs =
			ARRAY_SIZE(adev->gfx.config.tile_mode_array);
	config->macro_tile_config_ptr =
			adev->gfx.config.macrotile_mode_array;
	config->num_macro_tile_configs =
			ARRAY_SIZE(adev->gfx.config.macrotile_mode_array);

	 
	config->num_banks = adev->gfx.config.num_banks;
	config->num_ranks = adev->gfx.config.num_ranks;

	return 0;
}

bool amdgpu_amdkfd_bo_mapped_to_dev(struct amdgpu_device *adev, struct kgd_mem *mem)
{
	struct kfd_mem_attachment *entry;

	list_for_each_entry(entry, &mem->attachments, list) {
		if (entry->is_mapped && entry->adev == adev)
			return true;
	}
	return false;
}

#if defined(CONFIG_DEBUG_FS)

int kfd_debugfs_kfd_mem_limits(struct seq_file *m, void *data)
{

	spin_lock(&kfd_mem_limit.mem_limit_lock);
	seq_printf(m, "System mem used %lldM out of %lluM\n",
		  (kfd_mem_limit.system_mem_used >> 20),
		  (kfd_mem_limit.max_system_mem_limit >> 20));
	seq_printf(m, "TTM mem used %lldM out of %lluM\n",
		  (kfd_mem_limit.ttm_mem_used >> 20),
		  (kfd_mem_limit.max_ttm_mem_limit >> 20));
	spin_unlock(&kfd_mem_limit.mem_limit_lock);

	return 0;
}

#endif
