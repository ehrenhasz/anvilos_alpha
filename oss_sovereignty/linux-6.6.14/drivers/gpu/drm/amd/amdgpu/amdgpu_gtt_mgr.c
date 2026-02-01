 

#include <drm/ttm/ttm_range_manager.h>

#include "amdgpu.h"

static inline struct amdgpu_gtt_mgr *
to_gtt_mgr(struct ttm_resource_manager *man)
{
	return container_of(man, struct amdgpu_gtt_mgr, manager);
}

 
static ssize_t amdgpu_mem_info_gtt_total_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	struct ttm_resource_manager *man;

	man = ttm_manager_type(&adev->mman.bdev, TTM_PL_TT);
	return sysfs_emit(buf, "%llu\n", man->size);
}

 
static ssize_t amdgpu_mem_info_gtt_used_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	struct ttm_resource_manager *man = &adev->mman.gtt_mgr.manager;

	return sysfs_emit(buf, "%llu\n", ttm_resource_manager_usage(man));
}

static DEVICE_ATTR(mem_info_gtt_total, S_IRUGO,
	           amdgpu_mem_info_gtt_total_show, NULL);
static DEVICE_ATTR(mem_info_gtt_used, S_IRUGO,
	           amdgpu_mem_info_gtt_used_show, NULL);

static struct attribute *amdgpu_gtt_mgr_attributes[] = {
	&dev_attr_mem_info_gtt_total.attr,
	&dev_attr_mem_info_gtt_used.attr,
	NULL
};

const struct attribute_group amdgpu_gtt_mgr_attr_group = {
	.attrs = amdgpu_gtt_mgr_attributes
};

 
bool amdgpu_gtt_mgr_has_gart_addr(struct ttm_resource *res)
{
	struct ttm_range_mgr_node *node = to_ttm_range_mgr_node(res);

	return drm_mm_node_allocated(&node->mm_nodes[0]);
}

 
static int amdgpu_gtt_mgr_new(struct ttm_resource_manager *man,
			      struct ttm_buffer_object *tbo,
			      const struct ttm_place *place,
			      struct ttm_resource **res)
{
	struct amdgpu_gtt_mgr *mgr = to_gtt_mgr(man);
	uint32_t num_pages = PFN_UP(tbo->base.size);
	struct ttm_range_mgr_node *node;
	int r;

	node = kzalloc(struct_size(node, mm_nodes, 1), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	ttm_resource_init(tbo, place, &node->base);
	if (!(place->flags & TTM_PL_FLAG_TEMPORARY) &&
	    ttm_resource_manager_usage(man) > man->size) {
		r = -ENOSPC;
		goto err_free;
	}

	if (place->lpfn) {
		spin_lock(&mgr->lock);
		r = drm_mm_insert_node_in_range(&mgr->mm, &node->mm_nodes[0],
						num_pages, tbo->page_alignment,
						0, place->fpfn, place->lpfn,
						DRM_MM_INSERT_BEST);
		spin_unlock(&mgr->lock);
		if (unlikely(r))
			goto err_free;

		node->base.start = node->mm_nodes[0].start;
	} else {
		node->mm_nodes[0].start = 0;
		node->mm_nodes[0].size = PFN_UP(node->base.size);
		node->base.start = AMDGPU_BO_INVALID_OFFSET;
	}

	*res = &node->base;
	return 0;

err_free:
	ttm_resource_fini(man, &node->base);
	kfree(node);
	return r;
}

 
static void amdgpu_gtt_mgr_del(struct ttm_resource_manager *man,
			       struct ttm_resource *res)
{
	struct ttm_range_mgr_node *node = to_ttm_range_mgr_node(res);
	struct amdgpu_gtt_mgr *mgr = to_gtt_mgr(man);

	spin_lock(&mgr->lock);
	if (drm_mm_node_allocated(&node->mm_nodes[0]))
		drm_mm_remove_node(&node->mm_nodes[0]);
	spin_unlock(&mgr->lock);

	ttm_resource_fini(man, res);
	kfree(node);
}

 
void amdgpu_gtt_mgr_recover(struct amdgpu_gtt_mgr *mgr)
{
	struct ttm_range_mgr_node *node;
	struct drm_mm_node *mm_node;
	struct amdgpu_device *adev;

	adev = container_of(mgr, typeof(*adev), mman.gtt_mgr);
	spin_lock(&mgr->lock);
	drm_mm_for_each_node(mm_node, &mgr->mm) {
		node = container_of(mm_node, typeof(*node), mm_nodes[0]);
		amdgpu_ttm_recover_gart(node->base.bo);
	}
	spin_unlock(&mgr->lock);

	amdgpu_gart_invalidate_tlb(adev);
}

 
static bool amdgpu_gtt_mgr_intersects(struct ttm_resource_manager *man,
				      struct ttm_resource *res,
				      const struct ttm_place *place,
				      size_t size)
{
	return !place->lpfn || amdgpu_gtt_mgr_has_gart_addr(res);
}

 
static bool amdgpu_gtt_mgr_compatible(struct ttm_resource_manager *man,
				      struct ttm_resource *res,
				      const struct ttm_place *place,
				      size_t size)
{
	return !place->lpfn || amdgpu_gtt_mgr_has_gart_addr(res);
}

 
static void amdgpu_gtt_mgr_debug(struct ttm_resource_manager *man,
				 struct drm_printer *printer)
{
	struct amdgpu_gtt_mgr *mgr = to_gtt_mgr(man);

	spin_lock(&mgr->lock);
	drm_mm_print(&mgr->mm, printer);
	spin_unlock(&mgr->lock);
}

static const struct ttm_resource_manager_func amdgpu_gtt_mgr_func = {
	.alloc = amdgpu_gtt_mgr_new,
	.free = amdgpu_gtt_mgr_del,
	.intersects = amdgpu_gtt_mgr_intersects,
	.compatible = amdgpu_gtt_mgr_compatible,
	.debug = amdgpu_gtt_mgr_debug
};

 
int amdgpu_gtt_mgr_init(struct amdgpu_device *adev, uint64_t gtt_size)
{
	struct amdgpu_gtt_mgr *mgr = &adev->mman.gtt_mgr;
	struct ttm_resource_manager *man = &mgr->manager;
	uint64_t start, size;

	man->use_tt = true;
	man->func = &amdgpu_gtt_mgr_func;

	ttm_resource_manager_init(man, &adev->mman.bdev, gtt_size);

	start = AMDGPU_GTT_MAX_TRANSFER_SIZE * AMDGPU_GTT_NUM_TRANSFER_WINDOWS;
	size = (adev->gmc.gart_size >> PAGE_SHIFT) - start;
	drm_mm_init(&mgr->mm, start, size);
	spin_lock_init(&mgr->lock);

	ttm_set_driver_manager(&adev->mman.bdev, TTM_PL_TT, &mgr->manager);
	ttm_resource_manager_set_used(man, true);
	return 0;
}

 
void amdgpu_gtt_mgr_fini(struct amdgpu_device *adev)
{
	struct amdgpu_gtt_mgr *mgr = &adev->mman.gtt_mgr;
	struct ttm_resource_manager *man = &mgr->manager;
	int ret;

	ttm_resource_manager_set_used(man, false);

	ret = ttm_resource_manager_evict_all(&adev->mman.bdev, man);
	if (ret)
		return;

	spin_lock(&mgr->lock);
	drm_mm_takedown(&mgr->mm);
	spin_unlock(&mgr->lock);

	ttm_resource_manager_cleanup(man);
	ttm_set_driver_manager(&adev->mman.bdev, TTM_PL_TT, NULL);
}
