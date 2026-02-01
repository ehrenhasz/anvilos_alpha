 
 
 

#include <drm/ttm/ttm_device.h>
#include <drm/ttm/ttm_placement.h>
#include <drm/ttm/ttm_range_manager.h>
#include <drm/ttm/ttm_bo.h>
#include <drm/drm_mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

 

struct ttm_range_manager {
	struct ttm_resource_manager manager;
	struct drm_mm mm;
	spinlock_t lock;
};

static inline struct ttm_range_manager *
to_range_manager(struct ttm_resource_manager *man)
{
	return container_of(man, struct ttm_range_manager, manager);
}

static int ttm_range_man_alloc(struct ttm_resource_manager *man,
			       struct ttm_buffer_object *bo,
			       const struct ttm_place *place,
			       struct ttm_resource **res)
{
	struct ttm_range_manager *rman = to_range_manager(man);
	struct ttm_range_mgr_node *node;
	struct drm_mm *mm = &rman->mm;
	enum drm_mm_insert_mode mode;
	unsigned long lpfn;
	int ret;

	lpfn = place->lpfn;
	if (!lpfn)
		lpfn = man->size;

	node = kzalloc(struct_size(node, mm_nodes, 1), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	mode = DRM_MM_INSERT_BEST;
	if (place->flags & TTM_PL_FLAG_TOPDOWN)
		mode = DRM_MM_INSERT_HIGH;

	ttm_resource_init(bo, place, &node->base);

	spin_lock(&rman->lock);
	ret = drm_mm_insert_node_in_range(mm, &node->mm_nodes[0],
					  PFN_UP(node->base.size),
					  bo->page_alignment, 0,
					  place->fpfn, lpfn, mode);
	spin_unlock(&rman->lock);

	if (unlikely(ret)) {
		ttm_resource_fini(man, &node->base);
		kfree(node);
		return ret;
	}

	node->base.start = node->mm_nodes[0].start;
	*res = &node->base;
	return 0;
}

static void ttm_range_man_free(struct ttm_resource_manager *man,
			       struct ttm_resource *res)
{
	struct ttm_range_mgr_node *node = to_ttm_range_mgr_node(res);
	struct ttm_range_manager *rman = to_range_manager(man);

	spin_lock(&rman->lock);
	drm_mm_remove_node(&node->mm_nodes[0]);
	spin_unlock(&rman->lock);

	ttm_resource_fini(man, res);
	kfree(node);
}

static bool ttm_range_man_intersects(struct ttm_resource_manager *man,
				     struct ttm_resource *res,
				     const struct ttm_place *place,
				     size_t size)
{
	struct drm_mm_node *node = &to_ttm_range_mgr_node(res)->mm_nodes[0];
	u32 num_pages = PFN_UP(size);

	 
	if (place->fpfn >= (node->start + num_pages) ||
	    (place->lpfn && place->lpfn <= node->start))
		return false;

	return true;
}

static bool ttm_range_man_compatible(struct ttm_resource_manager *man,
				     struct ttm_resource *res,
				     const struct ttm_place *place,
				     size_t size)
{
	struct drm_mm_node *node = &to_ttm_range_mgr_node(res)->mm_nodes[0];
	u32 num_pages = PFN_UP(size);

	if (node->start < place->fpfn ||
	    (place->lpfn && (node->start + num_pages) > place->lpfn))
		return false;

	return true;
}

static void ttm_range_man_debug(struct ttm_resource_manager *man,
				struct drm_printer *printer)
{
	struct ttm_range_manager *rman = to_range_manager(man);

	spin_lock(&rman->lock);
	drm_mm_print(&rman->mm, printer);
	spin_unlock(&rman->lock);
}

static const struct ttm_resource_manager_func ttm_range_manager_func = {
	.alloc = ttm_range_man_alloc,
	.free = ttm_range_man_free,
	.intersects = ttm_range_man_intersects,
	.compatible = ttm_range_man_compatible,
	.debug = ttm_range_man_debug
};

 
int ttm_range_man_init_nocheck(struct ttm_device *bdev,
		       unsigned type, bool use_tt,
		       unsigned long p_size)
{
	struct ttm_resource_manager *man;
	struct ttm_range_manager *rman;

	rman = kzalloc(sizeof(*rman), GFP_KERNEL);
	if (!rman)
		return -ENOMEM;

	man = &rman->manager;
	man->use_tt = use_tt;

	man->func = &ttm_range_manager_func;

	ttm_resource_manager_init(man, bdev, p_size);

	drm_mm_init(&rman->mm, 0, p_size);
	spin_lock_init(&rman->lock);

	ttm_set_driver_manager(bdev, type, &rman->manager);
	ttm_resource_manager_set_used(man, true);
	return 0;
}
EXPORT_SYMBOL(ttm_range_man_init_nocheck);

 
int ttm_range_man_fini_nocheck(struct ttm_device *bdev,
		       unsigned type)
{
	struct ttm_resource_manager *man = ttm_manager_type(bdev, type);
	struct ttm_range_manager *rman = to_range_manager(man);
	struct drm_mm *mm = &rman->mm;
	int ret;

	if (!man)
		return 0;

	ttm_resource_manager_set_used(man, false);

	ret = ttm_resource_manager_evict_all(bdev, man);
	if (ret)
		return ret;

	spin_lock(&rman->lock);
	drm_mm_takedown(mm);
	spin_unlock(&rman->lock);

	ttm_resource_manager_cleanup(man);
	ttm_set_driver_manager(bdev, type, NULL);
	kfree(rman);
	return 0;
}
EXPORT_SYMBOL(ttm_range_man_fini_nocheck);
