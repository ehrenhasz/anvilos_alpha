
 

#include <linux/shmem_fs.h>

#include <drm/ttm/ttm_placement.h>
#include <drm/ttm/ttm_tt.h>
#include <drm/drm_buddy.h>

#include "i915_drv.h"
#include "i915_ttm_buddy_manager.h"
#include "intel_memory_region.h"
#include "intel_region_ttm.h"

#include "gem/i915_gem_mman.h"
#include "gem/i915_gem_object.h"
#include "gem/i915_gem_region.h"
#include "gem/i915_gem_ttm.h"
#include "gem/i915_gem_ttm_move.h"
#include "gem/i915_gem_ttm_pm.h"
#include "gt/intel_gpu_commands.h"

#define I915_TTM_PRIO_PURGE     0
#define I915_TTM_PRIO_NO_PAGES  1
#define I915_TTM_PRIO_HAS_PAGES 2
#define I915_TTM_PRIO_NEEDS_CPU_ACCESS 3

 
#define I915_TTM_MAX_PLACEMENTS INTEL_REGION_UNKNOWN

 
struct i915_ttm_tt {
	struct ttm_tt ttm;
	struct device *dev;
	struct i915_refct_sgt cached_rsgt;

	bool is_shmem;
	struct file *filp;
};

static const struct ttm_place sys_placement_flags = {
	.fpfn = 0,
	.lpfn = 0,
	.mem_type = I915_PL_SYSTEM,
	.flags = 0,
};

static struct ttm_placement i915_sys_placement = {
	.num_placement = 1,
	.placement = &sys_placement_flags,
	.num_busy_placement = 1,
	.busy_placement = &sys_placement_flags,
};

 
struct ttm_placement *i915_ttm_sys_placement(void)
{
	return &i915_sys_placement;
}

static int i915_ttm_err_to_gem(int err)
{
	 
	if (likely(!err))
		return 0;

	switch (err) {
	case -EBUSY:
		 
		return -EAGAIN;
	case -ENOSPC:
		 
		return -ENXIO;
	default:
		break;
	}

	return err;
}

static enum ttm_caching
i915_ttm_select_tt_caching(const struct drm_i915_gem_object *obj)
{
	 
	if (obj->mm.n_placements <= 1)
		return ttm_cached;

	return ttm_write_combined;
}

static void
i915_ttm_place_from_region(const struct intel_memory_region *mr,
			   struct ttm_place *place,
			   resource_size_t offset,
			   resource_size_t size,
			   unsigned int flags)
{
	memset(place, 0, sizeof(*place));
	place->mem_type = intel_region_to_ttm_type(mr);

	if (mr->type == INTEL_MEMORY_SYSTEM)
		return;

	if (flags & I915_BO_ALLOC_CONTIGUOUS)
		place->flags |= TTM_PL_FLAG_CONTIGUOUS;
	if (offset != I915_BO_INVALID_OFFSET) {
		WARN_ON(overflows_type(offset >> PAGE_SHIFT, place->fpfn));
		place->fpfn = offset >> PAGE_SHIFT;
		WARN_ON(overflows_type(place->fpfn + (size >> PAGE_SHIFT), place->lpfn));
		place->lpfn = place->fpfn + (size >> PAGE_SHIFT);
	} else if (mr->io_size && mr->io_size < mr->total) {
		if (flags & I915_BO_ALLOC_GPU_ONLY) {
			place->flags |= TTM_PL_FLAG_TOPDOWN;
		} else {
			place->fpfn = 0;
			WARN_ON(overflows_type(mr->io_size >> PAGE_SHIFT, place->lpfn));
			place->lpfn = mr->io_size >> PAGE_SHIFT;
		}
	}
}

static void
i915_ttm_placement_from_obj(const struct drm_i915_gem_object *obj,
			    struct ttm_place *requested,
			    struct ttm_place *busy,
			    struct ttm_placement *placement)
{
	unsigned int num_allowed = obj->mm.n_placements;
	unsigned int flags = obj->flags;
	unsigned int i;

	placement->num_placement = 1;
	i915_ttm_place_from_region(num_allowed ? obj->mm.placements[0] :
				   obj->mm.region, requested, obj->bo_offset,
				   obj->base.size, flags);

	 
	placement->num_busy_placement = num_allowed;
	for (i = 0; i < placement->num_busy_placement; ++i)
		i915_ttm_place_from_region(obj->mm.placements[i], busy + i,
					   obj->bo_offset, obj->base.size, flags);

	if (num_allowed == 0) {
		*busy = *requested;
		placement->num_busy_placement = 1;
	}

	placement->placement = requested;
	placement->busy_placement = busy;
}

static int i915_ttm_tt_shmem_populate(struct ttm_device *bdev,
				      struct ttm_tt *ttm,
				      struct ttm_operation_ctx *ctx)
{
	struct drm_i915_private *i915 = container_of(bdev, typeof(*i915), bdev);
	struct intel_memory_region *mr = i915->mm.regions[INTEL_MEMORY_SYSTEM];
	struct i915_ttm_tt *i915_tt = container_of(ttm, typeof(*i915_tt), ttm);
	const unsigned int max_segment = i915_sg_segment_size(i915->drm.dev);
	const size_t size = (size_t)ttm->num_pages << PAGE_SHIFT;
	struct file *filp = i915_tt->filp;
	struct sgt_iter sgt_iter;
	struct sg_table *st;
	struct page *page;
	unsigned long i;
	int err;

	if (!filp) {
		struct address_space *mapping;
		gfp_t mask;

		filp = shmem_file_setup("i915-shmem-tt", size, VM_NORESERVE);
		if (IS_ERR(filp))
			return PTR_ERR(filp);

		mask = GFP_HIGHUSER | __GFP_RECLAIMABLE;

		mapping = filp->f_mapping;
		mapping_set_gfp_mask(mapping, mask);
		GEM_BUG_ON(!(mapping_gfp_mask(mapping) & __GFP_RECLAIM));

		i915_tt->filp = filp;
	}

	st = &i915_tt->cached_rsgt.table;
	err = shmem_sg_alloc_table(i915, st, size, mr, filp->f_mapping,
				   max_segment);
	if (err)
		return err;

	err = dma_map_sgtable(i915_tt->dev, st, DMA_BIDIRECTIONAL,
			      DMA_ATTR_SKIP_CPU_SYNC);
	if (err)
		goto err_free_st;

	i = 0;
	for_each_sgt_page(page, sgt_iter, st)
		ttm->pages[i++] = page;

	if (ttm->page_flags & TTM_TT_FLAG_SWAPPED)
		ttm->page_flags &= ~TTM_TT_FLAG_SWAPPED;

	return 0;

err_free_st:
	shmem_sg_free_table(st, filp->f_mapping, false, false);

	return err;
}

static void i915_ttm_tt_shmem_unpopulate(struct ttm_tt *ttm)
{
	struct i915_ttm_tt *i915_tt = container_of(ttm, typeof(*i915_tt), ttm);
	bool backup = ttm->page_flags & TTM_TT_FLAG_SWAPPED;
	struct sg_table *st = &i915_tt->cached_rsgt.table;

	shmem_sg_free_table(st, file_inode(i915_tt->filp)->i_mapping,
			    backup, backup);
}

static void i915_ttm_tt_release(struct kref *ref)
{
	struct i915_ttm_tt *i915_tt =
		container_of(ref, typeof(*i915_tt), cached_rsgt.kref);
	struct sg_table *st = &i915_tt->cached_rsgt.table;

	GEM_WARN_ON(st->sgl);

	kfree(i915_tt);
}

static const struct i915_refct_sgt_ops tt_rsgt_ops = {
	.release = i915_ttm_tt_release
};

static struct ttm_tt *i915_ttm_tt_create(struct ttm_buffer_object *bo,
					 uint32_t page_flags)
{
	struct drm_i915_private *i915 = container_of(bo->bdev, typeof(*i915),
						     bdev);
	struct drm_i915_gem_object *obj = i915_ttm_to_gem(bo);
	unsigned long ccs_pages = 0;
	enum ttm_caching caching;
	struct i915_ttm_tt *i915_tt;
	int ret;

	if (i915_ttm_is_ghost_object(bo))
		return NULL;

	i915_tt = kzalloc(sizeof(*i915_tt), GFP_KERNEL);
	if (!i915_tt)
		return NULL;

	if (obj->flags & I915_BO_ALLOC_CPU_CLEAR && (!bo->resource ||
	    ttm_manager_type(bo->bdev, bo->resource->mem_type)->use_tt))
		page_flags |= TTM_TT_FLAG_ZERO_ALLOC;

	caching = i915_ttm_select_tt_caching(obj);
	if (i915_gem_object_is_shrinkable(obj) && caching == ttm_cached) {
		page_flags |= TTM_TT_FLAG_EXTERNAL |
			      TTM_TT_FLAG_EXTERNAL_MAPPABLE;
		i915_tt->is_shmem = true;
	}

	if (i915_gem_object_needs_ccs_pages(obj))
		ccs_pages = DIV_ROUND_UP(DIV_ROUND_UP(bo->base.size,
						      NUM_BYTES_PER_CCS_BYTE),
					 PAGE_SIZE);

	ret = ttm_tt_init(&i915_tt->ttm, bo, page_flags, caching, ccs_pages);
	if (ret)
		goto err_free;

	__i915_refct_sgt_init(&i915_tt->cached_rsgt, bo->base.size,
			      &tt_rsgt_ops);

	i915_tt->dev = obj->base.dev->dev;

	return &i915_tt->ttm;

err_free:
	kfree(i915_tt);
	return NULL;
}

static int i915_ttm_tt_populate(struct ttm_device *bdev,
				struct ttm_tt *ttm,
				struct ttm_operation_ctx *ctx)
{
	struct i915_ttm_tt *i915_tt = container_of(ttm, typeof(*i915_tt), ttm);

	if (i915_tt->is_shmem)
		return i915_ttm_tt_shmem_populate(bdev, ttm, ctx);

	return ttm_pool_alloc(&bdev->pool, ttm, ctx);
}

static void i915_ttm_tt_unpopulate(struct ttm_device *bdev, struct ttm_tt *ttm)
{
	struct i915_ttm_tt *i915_tt = container_of(ttm, typeof(*i915_tt), ttm);
	struct sg_table *st = &i915_tt->cached_rsgt.table;

	if (st->sgl)
		dma_unmap_sgtable(i915_tt->dev, st, DMA_BIDIRECTIONAL, 0);

	if (i915_tt->is_shmem) {
		i915_ttm_tt_shmem_unpopulate(ttm);
	} else {
		sg_free_table(st);
		ttm_pool_free(&bdev->pool, ttm);
	}
}

static void i915_ttm_tt_destroy(struct ttm_device *bdev, struct ttm_tt *ttm)
{
	struct i915_ttm_tt *i915_tt = container_of(ttm, typeof(*i915_tt), ttm);

	if (i915_tt->filp)
		fput(i915_tt->filp);

	ttm_tt_fini(ttm);
	i915_refct_sgt_put(&i915_tt->cached_rsgt);
}

static bool i915_ttm_eviction_valuable(struct ttm_buffer_object *bo,
				       const struct ttm_place *place)
{
	struct drm_i915_gem_object *obj = i915_ttm_to_gem(bo);

	if (i915_ttm_is_ghost_object(bo))
		return false;

	 
	if (bo->ttm && bo->ttm->page_flags & TTM_TT_FLAG_EXTERNAL)
		return false;

	 
	if (!i915_gem_object_evictable(obj))
		return false;

	return ttm_bo_eviction_valuable(bo, place);
}

static void i915_ttm_evict_flags(struct ttm_buffer_object *bo,
				 struct ttm_placement *placement)
{
	*placement = i915_sys_placement;
}

 
void i915_ttm_free_cached_io_rsgt(struct drm_i915_gem_object *obj)
{
	struct radix_tree_iter iter;
	void __rcu **slot;

	if (!obj->ttm.cached_io_rsgt)
		return;

	rcu_read_lock();
	radix_tree_for_each_slot(slot, &obj->ttm.get_io_page.radix, &iter, 0)
		radix_tree_delete(&obj->ttm.get_io_page.radix, iter.index);
	rcu_read_unlock();

	i915_refct_sgt_put(obj->ttm.cached_io_rsgt);
	obj->ttm.cached_io_rsgt = NULL;
}

 
int i915_ttm_purge(struct drm_i915_gem_object *obj)
{
	struct ttm_buffer_object *bo = i915_gem_to_ttm(obj);
	struct i915_ttm_tt *i915_tt =
		container_of(bo->ttm, typeof(*i915_tt), ttm);
	struct ttm_operation_ctx ctx = {
		.interruptible = true,
		.no_wait_gpu = false,
	};
	struct ttm_placement place = {};
	int ret;

	if (obj->mm.madv == __I915_MADV_PURGED)
		return 0;

	ret = ttm_bo_validate(bo, &place, &ctx);
	if (ret)
		return ret;

	if (bo->ttm && i915_tt->filp) {
		 
		shmem_truncate_range(file_inode(i915_tt->filp),
				     0, (loff_t)-1);
		fput(fetch_and_zero(&i915_tt->filp));
	}

	obj->write_domain = 0;
	obj->read_domains = 0;
	i915_ttm_adjust_gem_after_move(obj);
	i915_ttm_free_cached_io_rsgt(obj);
	obj->mm.madv = __I915_MADV_PURGED;

	return 0;
}

static int i915_ttm_shrink(struct drm_i915_gem_object *obj, unsigned int flags)
{
	struct ttm_buffer_object *bo = i915_gem_to_ttm(obj);
	struct i915_ttm_tt *i915_tt =
		container_of(bo->ttm, typeof(*i915_tt), ttm);
	struct ttm_operation_ctx ctx = {
		.interruptible = true,
		.no_wait_gpu = flags & I915_GEM_OBJECT_SHRINK_NO_GPU_WAIT,
	};
	struct ttm_placement place = {};
	int ret;

	if (!bo->ttm || i915_ttm_cpu_maps_iomem(bo->resource))
		return 0;

	GEM_BUG_ON(!i915_tt->is_shmem);

	if (!i915_tt->filp)
		return 0;

	ret = ttm_bo_wait_ctx(bo, &ctx);
	if (ret)
		return ret;

	switch (obj->mm.madv) {
	case I915_MADV_DONTNEED:
		return i915_ttm_purge(obj);
	case __I915_MADV_PURGED:
		return 0;
	}

	if (bo->ttm->page_flags & TTM_TT_FLAG_SWAPPED)
		return 0;

	bo->ttm->page_flags |= TTM_TT_FLAG_SWAPPED;
	ret = ttm_bo_validate(bo, &place, &ctx);
	if (ret) {
		bo->ttm->page_flags &= ~TTM_TT_FLAG_SWAPPED;
		return ret;
	}

	if (flags & I915_GEM_OBJECT_SHRINK_WRITEBACK)
		__shmem_writeback(obj->base.size, i915_tt->filp->f_mapping);

	return 0;
}

static void i915_ttm_delete_mem_notify(struct ttm_buffer_object *bo)
{
	struct drm_i915_gem_object *obj = i915_ttm_to_gem(bo);

	 
	if ((bo->resource || bo->ttm) && !i915_ttm_is_ghost_object(bo)) {
		__i915_gem_object_pages_fini(obj);
		i915_ttm_free_cached_io_rsgt(obj);
	}
}

static struct i915_refct_sgt *i915_ttm_tt_get_st(struct ttm_tt *ttm)
{
	struct i915_ttm_tt *i915_tt = container_of(ttm, typeof(*i915_tt), ttm);
	struct sg_table *st;
	int ret;

	if (i915_tt->cached_rsgt.table.sgl)
		return i915_refct_sgt_get(&i915_tt->cached_rsgt);

	st = &i915_tt->cached_rsgt.table;
	ret = sg_alloc_table_from_pages_segment(st,
			ttm->pages, ttm->num_pages,
			0, (unsigned long)ttm->num_pages << PAGE_SHIFT,
			i915_sg_segment_size(i915_tt->dev), GFP_KERNEL);
	if (ret) {
		st->sgl = NULL;
		return ERR_PTR(ret);
	}

	ret = dma_map_sgtable(i915_tt->dev, st, DMA_BIDIRECTIONAL, 0);
	if (ret) {
		sg_free_table(st);
		return ERR_PTR(ret);
	}

	return i915_refct_sgt_get(&i915_tt->cached_rsgt);
}

 
struct i915_refct_sgt *
i915_ttm_resource_get_st(struct drm_i915_gem_object *obj,
			 struct ttm_resource *res)
{
	struct ttm_buffer_object *bo = i915_gem_to_ttm(obj);
	u32 page_alignment;

	if (!i915_ttm_gtt_binds_lmem(res))
		return i915_ttm_tt_get_st(bo->ttm);

	page_alignment = bo->page_alignment << PAGE_SHIFT;
	if (!page_alignment)
		page_alignment = obj->mm.region->min_page_size;

	 
	GEM_WARN_ON(!i915_ttm_cpu_maps_iomem(res));
	if (bo->resource == res) {
		if (!obj->ttm.cached_io_rsgt) {
			struct i915_refct_sgt *rsgt;

			rsgt = intel_region_ttm_resource_to_rsgt(obj->mm.region,
								 res,
								 page_alignment);
			if (IS_ERR(rsgt))
				return rsgt;

			obj->ttm.cached_io_rsgt = rsgt;
		}
		return i915_refct_sgt_get(obj->ttm.cached_io_rsgt);
	}

	return intel_region_ttm_resource_to_rsgt(obj->mm.region, res,
						 page_alignment);
}

static int i915_ttm_truncate(struct drm_i915_gem_object *obj)
{
	struct ttm_buffer_object *bo = i915_gem_to_ttm(obj);
	long err;

	WARN_ON_ONCE(obj->mm.madv == I915_MADV_WILLNEED);

	err = dma_resv_wait_timeout(bo->base.resv, DMA_RESV_USAGE_BOOKKEEP,
				    true, 15 * HZ);
	if (err < 0)
		return err;
	if (err == 0)
		return -EBUSY;

	err = i915_ttm_move_notify(bo);
	if (err)
		return err;

	return i915_ttm_purge(obj);
}

static void i915_ttm_swap_notify(struct ttm_buffer_object *bo)
{
	struct drm_i915_gem_object *obj = i915_ttm_to_gem(bo);
	int ret;

	if (i915_ttm_is_ghost_object(bo))
		return;

	ret = i915_ttm_move_notify(bo);
	GEM_WARN_ON(ret);
	GEM_WARN_ON(obj->ttm.cached_io_rsgt);
	if (!ret && obj->mm.madv != I915_MADV_WILLNEED)
		i915_ttm_purge(obj);
}

 
bool i915_ttm_resource_mappable(struct ttm_resource *res)
{
	struct i915_ttm_buddy_resource *bman_res = to_ttm_buddy_resource(res);

	if (!i915_ttm_cpu_maps_iomem(res))
		return true;

	return bman_res->used_visible_size == PFN_UP(bman_res->base.size);
}

static int i915_ttm_io_mem_reserve(struct ttm_device *bdev, struct ttm_resource *mem)
{
	struct drm_i915_gem_object *obj = i915_ttm_to_gem(mem->bo);
	bool unknown_state;

	if (i915_ttm_is_ghost_object(mem->bo))
		return -EINVAL;

	if (!kref_get_unless_zero(&obj->base.refcount))
		return -EINVAL;

	assert_object_held(obj);

	unknown_state = i915_gem_object_has_unknown_state(obj);
	i915_gem_object_put(obj);
	if (unknown_state)
		return -EINVAL;

	if (!i915_ttm_cpu_maps_iomem(mem))
		return 0;

	if (!i915_ttm_resource_mappable(mem))
		return -EINVAL;

	mem->bus.caching = ttm_write_combined;
	mem->bus.is_iomem = true;

	return 0;
}

static unsigned long i915_ttm_io_mem_pfn(struct ttm_buffer_object *bo,
					 unsigned long page_offset)
{
	struct drm_i915_gem_object *obj = i915_ttm_to_gem(bo);
	struct scatterlist *sg;
	unsigned long base;
	unsigned int ofs;

	GEM_BUG_ON(i915_ttm_is_ghost_object(bo));
	GEM_WARN_ON(bo->ttm);

	base = obj->mm.region->iomap.base - obj->mm.region->region.start;
	sg = i915_gem_object_page_iter_get_sg(obj, &obj->ttm.get_io_page, page_offset, &ofs);

	return ((base + sg_dma_address(sg)) >> PAGE_SHIFT) + ofs;
}

static int i915_ttm_access_memory(struct ttm_buffer_object *bo,
				  unsigned long offset, void *buf,
				  int len, int write)
{
	struct drm_i915_gem_object *obj = i915_ttm_to_gem(bo);
	resource_size_t iomap = obj->mm.region->iomap.base -
		obj->mm.region->region.start;
	unsigned long page = offset >> PAGE_SHIFT;
	unsigned long bytes_left = len;

	 
	if (!i915_ttm_resource_mappable(bo->resource))
		return -EIO;

	offset -= page << PAGE_SHIFT;
	do {
		unsigned long bytes = min(bytes_left, PAGE_SIZE - offset);
		void __iomem *ptr;
		dma_addr_t daddr;

		daddr = i915_gem_object_get_dma_address(obj, page);
		ptr = ioremap_wc(iomap + daddr + offset, bytes);
		if (!ptr)
			return -EIO;

		if (write)
			memcpy_toio(ptr, buf, bytes);
		else
			memcpy_fromio(buf, ptr, bytes);
		iounmap(ptr);

		page++;
		buf += bytes;
		bytes_left -= bytes;
		offset = 0;
	} while (bytes_left);

	return len;
}

 
static struct ttm_device_funcs i915_ttm_bo_driver = {
	.ttm_tt_create = i915_ttm_tt_create,
	.ttm_tt_populate = i915_ttm_tt_populate,
	.ttm_tt_unpopulate = i915_ttm_tt_unpopulate,
	.ttm_tt_destroy = i915_ttm_tt_destroy,
	.eviction_valuable = i915_ttm_eviction_valuable,
	.evict_flags = i915_ttm_evict_flags,
	.move = i915_ttm_move,
	.swap_notify = i915_ttm_swap_notify,
	.delete_mem_notify = i915_ttm_delete_mem_notify,
	.io_mem_reserve = i915_ttm_io_mem_reserve,
	.io_mem_pfn = i915_ttm_io_mem_pfn,
	.access_memory = i915_ttm_access_memory,
};

 
struct ttm_device_funcs *i915_ttm_driver(void)
{
	return &i915_ttm_bo_driver;
}

static int __i915_ttm_get_pages(struct drm_i915_gem_object *obj,
				struct ttm_placement *placement)
{
	struct ttm_buffer_object *bo = i915_gem_to_ttm(obj);
	struct ttm_operation_ctx ctx = {
		.interruptible = true,
		.no_wait_gpu = false,
	};
	int real_num_busy;
	int ret;

	 
	real_num_busy = fetch_and_zero(&placement->num_busy_placement);
	ret = ttm_bo_validate(bo, placement, &ctx);
	if (ret) {
		ret = i915_ttm_err_to_gem(ret);
		 
		if (ret == -EDEADLK || ret == -EINTR || ret == -ERESTARTSYS ||
		    ret == -EAGAIN)
			return ret;

		 
		placement->num_busy_placement = real_num_busy;
		ret = ttm_bo_validate(bo, placement, &ctx);
		if (ret)
			return i915_ttm_err_to_gem(ret);
	}

	if (bo->ttm && !ttm_tt_is_populated(bo->ttm)) {
		ret = ttm_tt_populate(bo->bdev, bo->ttm, &ctx);
		if (ret)
			return ret;

		i915_ttm_adjust_domains_after_move(obj);
		i915_ttm_adjust_gem_after_move(obj);
	}

	if (!i915_gem_object_has_pages(obj)) {
		struct i915_refct_sgt *rsgt =
			i915_ttm_resource_get_st(obj, bo->resource);

		if (IS_ERR(rsgt))
			return PTR_ERR(rsgt);

		GEM_BUG_ON(obj->mm.rsgt);
		obj->mm.rsgt = rsgt;
		__i915_gem_object_set_pages(obj, &rsgt->table);
	}

	GEM_BUG_ON(bo->ttm && ((obj->base.size >> PAGE_SHIFT) < bo->ttm->num_pages));
	i915_ttm_adjust_lru(obj);
	return ret;
}

static int i915_ttm_get_pages(struct drm_i915_gem_object *obj)
{
	struct ttm_place requested, busy[I915_TTM_MAX_PLACEMENTS];
	struct ttm_placement placement;

	 
	if (overflows_type(obj->base.size >> PAGE_SHIFT, unsigned int))
		return -E2BIG;

	GEM_BUG_ON(obj->mm.n_placements > I915_TTM_MAX_PLACEMENTS);

	 
	i915_ttm_placement_from_obj(obj, &requested, busy, &placement);

	return __i915_ttm_get_pages(obj, &placement);
}

 
static int __i915_ttm_migrate(struct drm_i915_gem_object *obj,
			      struct intel_memory_region *mr,
			      unsigned int flags)
{
	struct ttm_place requested;
	struct ttm_placement placement;
	int ret;

	i915_ttm_place_from_region(mr, &requested, obj->bo_offset,
				   obj->base.size, flags);
	placement.num_placement = 1;
	placement.num_busy_placement = 1;
	placement.placement = &requested;
	placement.busy_placement = &requested;

	ret = __i915_ttm_get_pages(obj, &placement);
	if (ret)
		return ret;

	 
	if (obj->mm.region != mr) {
		i915_gem_object_release_memory_region(obj);
		i915_gem_object_init_memory_region(obj, mr);
	}

	return 0;
}

static int i915_ttm_migrate(struct drm_i915_gem_object *obj,
			    struct intel_memory_region *mr,
			    unsigned int flags)
{
	return __i915_ttm_migrate(obj, mr, flags);
}

static void i915_ttm_put_pages(struct drm_i915_gem_object *obj,
			       struct sg_table *st)
{
	 

	if (obj->mm.rsgt)
		i915_refct_sgt_put(fetch_and_zero(&obj->mm.rsgt));
}

 
void i915_ttm_adjust_lru(struct drm_i915_gem_object *obj)
{
	struct ttm_buffer_object *bo = i915_gem_to_ttm(obj);
	struct i915_ttm_tt *i915_tt =
		container_of(bo->ttm, typeof(*i915_tt), ttm);
	bool shrinkable =
		bo->ttm && i915_tt->filp && ttm_tt_is_populated(bo->ttm);

	 
	if (!kref_read(&bo->kref))
		return;

	 
	if (kref_get_unless_zero(&obj->base.refcount)) {
		if (shrinkable != obj->mm.ttm_shrinkable) {
			if (shrinkable) {
				if (obj->mm.madv == I915_MADV_WILLNEED)
					__i915_gem_object_make_shrinkable(obj);
				else
					__i915_gem_object_make_purgeable(obj);
			} else {
				i915_gem_object_make_unshrinkable(obj);
			}

			obj->mm.ttm_shrinkable = shrinkable;
		}
		i915_gem_object_put(obj);
	}

	 
	spin_lock(&bo->bdev->lru_lock);
	if (shrinkable) {
		 
		bo->priority = TTM_MAX_BO_PRIORITY - 1;
	} else if (obj->mm.madv != I915_MADV_WILLNEED) {
		bo->priority = I915_TTM_PRIO_PURGE;
	} else if (!i915_gem_object_has_pages(obj)) {
		bo->priority = I915_TTM_PRIO_NO_PAGES;
	} else {
		struct ttm_resource_manager *man =
			ttm_manager_type(bo->bdev, bo->resource->mem_type);

		 
		if (i915_ttm_cpu_maps_iomem(bo->resource) &&
		    i915_ttm_buddy_man_visible_size(man) < man->size &&
		    !(obj->flags & I915_BO_ALLOC_GPU_ONLY))
			bo->priority = I915_TTM_PRIO_NEEDS_CPU_ACCESS;
		else
			bo->priority = I915_TTM_PRIO_HAS_PAGES;
	}

	ttm_bo_move_to_lru_tail(bo);
	spin_unlock(&bo->bdev->lru_lock);
}

 
static void i915_ttm_delayed_free(struct drm_i915_gem_object *obj)
{
	GEM_BUG_ON(!obj->ttm.created);

	ttm_bo_put(i915_gem_to_ttm(obj));
}

static vm_fault_t vm_fault_ttm(struct vm_fault *vmf)
{
	struct vm_area_struct *area = vmf->vma;
	struct ttm_buffer_object *bo = area->vm_private_data;
	struct drm_device *dev = bo->base.dev;
	struct drm_i915_gem_object *obj = i915_ttm_to_gem(bo);
	intel_wakeref_t wakeref = 0;
	vm_fault_t ret;
	int idx;

	 
	if (unlikely(i915_gem_object_is_readonly(obj) &&
		     area->vm_flags & VM_WRITE))
		return VM_FAULT_SIGBUS;

	ret = ttm_bo_vm_reserve(bo, vmf);
	if (ret)
		return ret;

	if (obj->mm.madv != I915_MADV_WILLNEED) {
		dma_resv_unlock(bo->base.resv);
		return VM_FAULT_SIGBUS;
	}

	 
	if (!bo->resource) {
		struct ttm_operation_ctx ctx = {
			.interruptible = true,
			.no_wait_gpu = true,  
		};
		int err;

		GEM_BUG_ON(!bo->ttm || !(bo->ttm->page_flags & TTM_TT_FLAG_SWAPPED));

		err = ttm_bo_validate(bo, i915_ttm_sys_placement(), &ctx);
		if (err) {
			dma_resv_unlock(bo->base.resv);
			return VM_FAULT_SIGBUS;
		}
	} else if (!i915_ttm_resource_mappable(bo->resource)) {
		int err = -ENODEV;
		int i;

		for (i = 0; i < obj->mm.n_placements; i++) {
			struct intel_memory_region *mr = obj->mm.placements[i];
			unsigned int flags;

			if (!mr->io_size && mr->type != INTEL_MEMORY_SYSTEM)
				continue;

			flags = obj->flags;
			flags &= ~I915_BO_ALLOC_GPU_ONLY;
			err = __i915_ttm_migrate(obj, mr, flags);
			if (!err)
				break;
		}

		if (err) {
			drm_dbg(dev, "Unable to make resource CPU accessible(err = %pe)\n",
				ERR_PTR(err));
			dma_resv_unlock(bo->base.resv);
			ret = VM_FAULT_SIGBUS;
			goto out_rpm;
		}
	}

	if (i915_ttm_cpu_maps_iomem(bo->resource))
		wakeref = intel_runtime_pm_get(&to_i915(obj->base.dev)->runtime_pm);

	if (drm_dev_enter(dev, &idx)) {
		ret = ttm_bo_vm_fault_reserved(vmf, vmf->vma->vm_page_prot,
					       TTM_BO_VM_NUM_PREFAULT);
		drm_dev_exit(idx);
	} else {
		ret = ttm_bo_vm_dummy_page(vmf, vmf->vma->vm_page_prot);
	}

	if (ret == VM_FAULT_RETRY && !(vmf->flags & FAULT_FLAG_RETRY_NOWAIT))
		goto out_rpm;

	 
	if (ret == VM_FAULT_NOPAGE && wakeref && !obj->userfault_count) {
		obj->userfault_count = 1;
		spin_lock(&to_i915(obj->base.dev)->runtime_pm.lmem_userfault_lock);
		list_add(&obj->userfault_link, &to_i915(obj->base.dev)->runtime_pm.lmem_userfault_list);
		spin_unlock(&to_i915(obj->base.dev)->runtime_pm.lmem_userfault_lock);

		GEM_WARN_ON(!i915_ttm_cpu_maps_iomem(bo->resource));
	}

	if (wakeref & CONFIG_DRM_I915_USERFAULT_AUTOSUSPEND)
		intel_wakeref_auto(&to_i915(obj->base.dev)->runtime_pm.userfault_wakeref,
				   msecs_to_jiffies_timeout(CONFIG_DRM_I915_USERFAULT_AUTOSUSPEND));

	i915_ttm_adjust_lru(obj);

	dma_resv_unlock(bo->base.resv);

out_rpm:
	if (wakeref)
		intel_runtime_pm_put(&to_i915(obj->base.dev)->runtime_pm, wakeref);

	return ret;
}

static int
vm_access_ttm(struct vm_area_struct *area, unsigned long addr,
	      void *buf, int len, int write)
{
	struct drm_i915_gem_object *obj =
		i915_ttm_to_gem(area->vm_private_data);

	if (i915_gem_object_is_readonly(obj) && write)
		return -EACCES;

	return ttm_bo_vm_access(area, addr, buf, len, write);
}

static void ttm_vm_open(struct vm_area_struct *vma)
{
	struct drm_i915_gem_object *obj =
		i915_ttm_to_gem(vma->vm_private_data);

	GEM_BUG_ON(i915_ttm_is_ghost_object(vma->vm_private_data));
	i915_gem_object_get(obj);
}

static void ttm_vm_close(struct vm_area_struct *vma)
{
	struct drm_i915_gem_object *obj =
		i915_ttm_to_gem(vma->vm_private_data);

	GEM_BUG_ON(i915_ttm_is_ghost_object(vma->vm_private_data));
	i915_gem_object_put(obj);
}

static const struct vm_operations_struct vm_ops_ttm = {
	.fault = vm_fault_ttm,
	.access = vm_access_ttm,
	.open = ttm_vm_open,
	.close = ttm_vm_close,
};

static u64 i915_ttm_mmap_offset(struct drm_i915_gem_object *obj)
{
	 
	GEM_BUG_ON(!drm_mm_node_allocated(&obj->base.vma_node.vm_node));

	return drm_vma_node_offset_addr(&obj->base.vma_node);
}

static void i915_ttm_unmap_virtual(struct drm_i915_gem_object *obj)
{
	struct ttm_buffer_object *bo = i915_gem_to_ttm(obj);
	intel_wakeref_t wakeref = 0;

	assert_object_held_shared(obj);

	if (i915_ttm_cpu_maps_iomem(bo->resource)) {
		wakeref = intel_runtime_pm_get(&to_i915(obj->base.dev)->runtime_pm);

		 
		if (obj->userfault_count) {
			spin_lock(&to_i915(obj->base.dev)->runtime_pm.lmem_userfault_lock);
			list_del(&obj->userfault_link);
			spin_unlock(&to_i915(obj->base.dev)->runtime_pm.lmem_userfault_lock);
			obj->userfault_count = 0;
		}
	}

	GEM_WARN_ON(obj->userfault_count);

	ttm_bo_unmap_virtual(i915_gem_to_ttm(obj));

	if (wakeref)
		intel_runtime_pm_put(&to_i915(obj->base.dev)->runtime_pm, wakeref);
}

static const struct drm_i915_gem_object_ops i915_gem_ttm_obj_ops = {
	.name = "i915_gem_object_ttm",
	.flags = I915_GEM_OBJECT_IS_SHRINKABLE |
		 I915_GEM_OBJECT_SELF_MANAGED_SHRINK_LIST,

	.get_pages = i915_ttm_get_pages,
	.put_pages = i915_ttm_put_pages,
	.truncate = i915_ttm_truncate,
	.shrink = i915_ttm_shrink,

	.adjust_lru = i915_ttm_adjust_lru,
	.delayed_free = i915_ttm_delayed_free,
	.migrate = i915_ttm_migrate,

	.mmap_offset = i915_ttm_mmap_offset,
	.unmap_virtual = i915_ttm_unmap_virtual,
	.mmap_ops = &vm_ops_ttm,
};

void i915_ttm_bo_destroy(struct ttm_buffer_object *bo)
{
	struct drm_i915_gem_object *obj = i915_ttm_to_gem(bo);

	i915_gem_object_release_memory_region(obj);
	mutex_destroy(&obj->ttm.get_io_page.lock);

	if (obj->ttm.created) {
		 
		if (obj->mm.ttm_shrinkable)
			i915_gem_object_make_unshrinkable(obj);

		i915_ttm_backup_free(obj);

		 
		__i915_gem_free_object(obj);

		call_rcu(&obj->rcu, __i915_gem_free_object_rcu);
	} else {
		__i915_gem_object_fini(obj);
	}
}

 
int __i915_gem_ttm_object_init(struct intel_memory_region *mem,
			       struct drm_i915_gem_object *obj,
			       resource_size_t offset,
			       resource_size_t size,
			       resource_size_t page_size,
			       unsigned int flags)
{
	static struct lock_class_key lock_class;
	struct drm_i915_private *i915 = mem->i915;
	struct ttm_operation_ctx ctx = {
		.interruptible = true,
		.no_wait_gpu = false,
	};
	enum ttm_bo_type bo_type;
	int ret;

	drm_gem_private_object_init(&i915->drm, &obj->base, size);
	i915_gem_object_init(obj, &i915_gem_ttm_obj_ops, &lock_class, flags);

	obj->bo_offset = offset;

	 
	obj->mm.region = mem;
	INIT_LIST_HEAD(&obj->mm.region_link);

	INIT_RADIX_TREE(&obj->ttm.get_io_page.radix, GFP_KERNEL | __GFP_NOWARN);
	mutex_init(&obj->ttm.get_io_page.lock);
	bo_type = (obj->flags & I915_BO_ALLOC_USER) ? ttm_bo_type_device :
		ttm_bo_type_kernel;

	obj->base.vma_node.driver_private = i915_gem_to_ttm(obj);

	 
	GEM_BUG_ON(page_size && obj->mm.n_placements);

	 
	i915_gem_object_make_unshrinkable(obj);

	 
	ret = ttm_bo_init_reserved(&i915->bdev, i915_gem_to_ttm(obj), bo_type,
				   &i915_sys_placement, page_size >> PAGE_SHIFT,
				   &ctx, NULL, NULL, i915_ttm_bo_destroy);

	 
	if (size >> PAGE_SHIFT > INT_MAX && ret == -ENOSPC)
		ret = -E2BIG;

	if (ret)
		return i915_ttm_err_to_gem(ret);

	obj->ttm.created = true;
	i915_gem_object_release_memory_region(obj);
	i915_gem_object_init_memory_region(obj, mem);
	i915_ttm_adjust_domains_after_move(obj);
	i915_ttm_adjust_gem_after_move(obj);
	i915_gem_object_unlock(obj);

	return 0;
}

static const struct intel_memory_region_ops ttm_system_region_ops = {
	.init_object = __i915_gem_ttm_object_init,
	.release = intel_region_ttm_fini,
};

struct intel_memory_region *
i915_gem_ttm_system_setup(struct drm_i915_private *i915,
			  u16 type, u16 instance)
{
	struct intel_memory_region *mr;

	mr = intel_memory_region_create(i915, 0,
					totalram_pages() << PAGE_SHIFT,
					PAGE_SIZE, 0, 0,
					type, instance,
					&ttm_system_region_ops);
	if (IS_ERR(mr))
		return mr;

	intel_memory_region_set_name(mr, "system-ttm");
	return mr;
}
