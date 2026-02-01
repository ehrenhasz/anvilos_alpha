 

 

#include "amdgpu.h"
#include "amdgpu_display.h"
#include "amdgpu_gem.h"
#include "amdgpu_dma_buf.h"
#include "amdgpu_xgmi.h"
#include <drm/amdgpu_drm.h>
#include <drm/ttm/ttm_tt.h>
#include <linux/dma-buf.h>
#include <linux/dma-fence-array.h>
#include <linux/pci-p2pdma.h>
#include <linux/pm_runtime.h>

 
static int amdgpu_dma_buf_attach(struct dma_buf *dmabuf,
				 struct dma_buf_attachment *attach)
{
	struct drm_gem_object *obj = dmabuf->priv;
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(obj);
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);
	int r;

	if (pci_p2pdma_distance(adev->pdev, attach->dev, false) < 0)
		attach->peer2peer = false;

	r = pm_runtime_get_sync(adev_to_drm(adev)->dev);
	if (r < 0)
		goto out;

	return 0;

out:
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
	return r;
}

 
static void amdgpu_dma_buf_detach(struct dma_buf *dmabuf,
				  struct dma_buf_attachment *attach)
{
	struct drm_gem_object *obj = dmabuf->priv;
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(obj);
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);

	pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
	pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
}

 
static int amdgpu_dma_buf_pin(struct dma_buf_attachment *attach)
{
	struct drm_gem_object *obj = attach->dmabuf->priv;
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(obj);

	 
	return amdgpu_bo_pin(bo, AMDGPU_GEM_DOMAIN_GTT);
}

 
static void amdgpu_dma_buf_unpin(struct dma_buf_attachment *attach)
{
	struct drm_gem_object *obj = attach->dmabuf->priv;
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(obj);

	amdgpu_bo_unpin(bo);
}

 
static struct sg_table *amdgpu_dma_buf_map(struct dma_buf_attachment *attach,
					   enum dma_data_direction dir)
{
	struct dma_buf *dma_buf = attach->dmabuf;
	struct drm_gem_object *obj = dma_buf->priv;
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(obj);
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);
	struct sg_table *sgt;
	long r;

	if (!bo->tbo.pin_count) {
		 
		struct ttm_operation_ctx ctx = { false, false };
		unsigned int domains = AMDGPU_GEM_DOMAIN_GTT;

		if (bo->preferred_domains & AMDGPU_GEM_DOMAIN_VRAM &&
		    attach->peer2peer) {
			bo->flags |= AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;
			domains |= AMDGPU_GEM_DOMAIN_VRAM;
		}
		amdgpu_bo_placement_from_domain(bo, domains);
		r = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
		if (r)
			return ERR_PTR(r);

	} else if (!(amdgpu_mem_type_to_domain(bo->tbo.resource->mem_type) &
		     AMDGPU_GEM_DOMAIN_GTT)) {
		return ERR_PTR(-EBUSY);
	}

	switch (bo->tbo.resource->mem_type) {
	case TTM_PL_TT:
		sgt = drm_prime_pages_to_sg(obj->dev,
					    bo->tbo.ttm->pages,
					    bo->tbo.ttm->num_pages);
		if (IS_ERR(sgt))
			return sgt;

		if (dma_map_sgtable(attach->dev, sgt, dir,
				    DMA_ATTR_SKIP_CPU_SYNC))
			goto error_free;
		break;

	case TTM_PL_VRAM:
		r = amdgpu_vram_mgr_alloc_sgt(adev, bo->tbo.resource, 0,
					      bo->tbo.base.size, attach->dev,
					      dir, &sgt);
		if (r)
			return ERR_PTR(r);
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	return sgt;

error_free:
	sg_free_table(sgt);
	kfree(sgt);
	return ERR_PTR(-EBUSY);
}

 
static void amdgpu_dma_buf_unmap(struct dma_buf_attachment *attach,
				 struct sg_table *sgt,
				 enum dma_data_direction dir)
{
	if (sgt->sgl->page_link) {
		dma_unmap_sgtable(attach->dev, sgt, dir, 0);
		sg_free_table(sgt);
		kfree(sgt);
	} else {
		amdgpu_vram_mgr_free_sgt(attach->dev, dir, sgt);
	}
}

 
static int amdgpu_dma_buf_begin_cpu_access(struct dma_buf *dma_buf,
					   enum dma_data_direction direction)
{
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(dma_buf->priv);
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);
	struct ttm_operation_ctx ctx = { true, false };
	u32 domain = amdgpu_display_supported_domains(adev, bo->flags);
	int ret;
	bool reads = (direction == DMA_BIDIRECTIONAL ||
		      direction == DMA_FROM_DEVICE);

	if (!reads || !(domain & AMDGPU_GEM_DOMAIN_GTT))
		return 0;

	 
	ret = amdgpu_bo_reserve(bo, false);
	if (unlikely(ret != 0))
		return ret;

	if (!bo->tbo.pin_count &&
	    (bo->allowed_domains & AMDGPU_GEM_DOMAIN_GTT)) {
		amdgpu_bo_placement_from_domain(bo, AMDGPU_GEM_DOMAIN_GTT);
		ret = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
	}

	amdgpu_bo_unreserve(bo);
	return ret;
}

const struct dma_buf_ops amdgpu_dmabuf_ops = {
	.attach = amdgpu_dma_buf_attach,
	.detach = amdgpu_dma_buf_detach,
	.pin = amdgpu_dma_buf_pin,
	.unpin = amdgpu_dma_buf_unpin,
	.map_dma_buf = amdgpu_dma_buf_map,
	.unmap_dma_buf = amdgpu_dma_buf_unmap,
	.release = drm_gem_dmabuf_release,
	.begin_cpu_access = amdgpu_dma_buf_begin_cpu_access,
	.mmap = drm_gem_dmabuf_mmap,
	.vmap = drm_gem_dmabuf_vmap,
	.vunmap = drm_gem_dmabuf_vunmap,
};

 
struct dma_buf *amdgpu_gem_prime_export(struct drm_gem_object *gobj,
					int flags)
{
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(gobj);
	struct dma_buf *buf;

	if (amdgpu_ttm_tt_get_usermm(bo->tbo.ttm) ||
	    bo->flags & AMDGPU_GEM_CREATE_VM_ALWAYS_VALID)
		return ERR_PTR(-EPERM);

	buf = drm_gem_prime_export(gobj, flags);
	if (!IS_ERR(buf))
		buf->ops = &amdgpu_dmabuf_ops;

	return buf;
}

 
static struct drm_gem_object *
amdgpu_dma_buf_create_obj(struct drm_device *dev, struct dma_buf *dma_buf)
{
	struct dma_resv *resv = dma_buf->resv;
	struct amdgpu_device *adev = drm_to_adev(dev);
	struct drm_gem_object *gobj;
	struct amdgpu_bo *bo;
	uint64_t flags = 0;
	int ret;

	dma_resv_lock(resv, NULL);

	if (dma_buf->ops == &amdgpu_dmabuf_ops) {
		struct amdgpu_bo *other = gem_to_amdgpu_bo(dma_buf->priv);

		flags |= other->flags & (AMDGPU_GEM_CREATE_CPU_GTT_USWC |
					 AMDGPU_GEM_CREATE_COHERENT |
					 AMDGPU_GEM_CREATE_UNCACHED);
	}

	ret = amdgpu_gem_object_create(adev, dma_buf->size, PAGE_SIZE,
				       AMDGPU_GEM_DOMAIN_CPU, flags,
				       ttm_bo_type_sg, resv, &gobj, 0);
	if (ret)
		goto error;

	bo = gem_to_amdgpu_bo(gobj);
	bo->allowed_domains = AMDGPU_GEM_DOMAIN_GTT;
	bo->preferred_domains = AMDGPU_GEM_DOMAIN_GTT;

	dma_resv_unlock(resv);
	return gobj;

error:
	dma_resv_unlock(resv);
	return ERR_PTR(ret);
}

 
static void
amdgpu_dma_buf_move_notify(struct dma_buf_attachment *attach)
{
	struct drm_gem_object *obj = attach->importer_priv;
	struct ww_acquire_ctx *ticket = dma_resv_locking_ctx(obj->resv);
	struct amdgpu_bo *bo = gem_to_amdgpu_bo(obj);
	struct amdgpu_device *adev = amdgpu_ttm_adev(bo->tbo.bdev);
	struct ttm_operation_ctx ctx = { false, false };
	struct ttm_placement placement = {};
	struct amdgpu_vm_bo_base *bo_base;
	int r;

	if (!bo->tbo.resource || bo->tbo.resource->mem_type == TTM_PL_SYSTEM)
		return;

	r = ttm_bo_validate(&bo->tbo, &placement, &ctx);
	if (r) {
		DRM_ERROR("Failed to invalidate DMA-buf import (%d))\n", r);
		return;
	}

	for (bo_base = bo->vm_bo; bo_base; bo_base = bo_base->next) {
		struct amdgpu_vm *vm = bo_base->vm;
		struct dma_resv *resv = vm->root.bo->tbo.base.resv;

		if (ticket) {
			 
			r = dma_resv_lock(resv, ticket);
			if (r)
				continue;

		} else {
			 
			if (!dma_resv_trylock(resv))
				continue;
		}

		 
		r = dma_resv_reserve_fences(resv, 2);
		if (!r)
			r = amdgpu_vm_clear_freed(adev, vm, NULL);
		if (!r)
			r = amdgpu_vm_handle_moved(adev, vm);

		if (r && r != -EBUSY)
			DRM_ERROR("Failed to invalidate VM page tables (%d))\n",
				  r);

		dma_resv_unlock(resv);
	}
}

static const struct dma_buf_attach_ops amdgpu_dma_buf_attach_ops = {
	.allow_peer2peer = true,
	.move_notify = amdgpu_dma_buf_move_notify
};

 
struct drm_gem_object *amdgpu_gem_prime_import(struct drm_device *dev,
					       struct dma_buf *dma_buf)
{
	struct dma_buf_attachment *attach;
	struct drm_gem_object *obj;

	if (dma_buf->ops == &amdgpu_dmabuf_ops) {
		obj = dma_buf->priv;
		if (obj->dev == dev) {
			 
			drm_gem_object_get(obj);
			return obj;
		}
	}

	obj = amdgpu_dma_buf_create_obj(dev, dma_buf);
	if (IS_ERR(obj))
		return obj;

	attach = dma_buf_dynamic_attach(dma_buf, dev->dev,
					&amdgpu_dma_buf_attach_ops, obj);
	if (IS_ERR(attach)) {
		drm_gem_object_put(obj);
		return ERR_CAST(attach);
	}

	get_dma_buf(dma_buf);
	obj->import_attach = attach;
	return obj;
}

 
bool amdgpu_dmabuf_is_xgmi_accessible(struct amdgpu_device *adev,
				      struct amdgpu_bo *bo)
{
	struct drm_gem_object *obj = &bo->tbo.base;
	struct drm_gem_object *gobj;

	if (obj->import_attach) {
		struct dma_buf *dma_buf = obj->import_attach->dmabuf;

		if (dma_buf->ops != &amdgpu_dmabuf_ops)
			 
			return false;

		gobj = dma_buf->priv;
		bo = gem_to_amdgpu_bo(gobj);
	}

	if (amdgpu_xgmi_same_hive(adev, amdgpu_ttm_adev(bo->tbo.bdev)) &&
			(bo->preferred_domains & AMDGPU_GEM_DOMAIN_VRAM))
		return true;

	return false;
}
