

#include <linux/iosys-map.h>
#include <linux/module.h>

#include <drm/drm_debugfs.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_gem_ttm_helper.h>
#include <drm/drm_gem_vram_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_mode.h>
#include <drm/drm_plane.h>
#include <drm/drm_prime.h>
#include <drm/drm_simple_kms_helper.h>

#include <drm/ttm/ttm_range_manager.h>
#include <drm/ttm/ttm_tt.h>

static const struct drm_gem_object_funcs drm_gem_vram_object_funcs;

 

 

static void drm_gem_vram_cleanup(struct drm_gem_vram_object *gbo)
{
	 

	WARN_ON(gbo->vmap_use_count);
	WARN_ON(iosys_map_is_set(&gbo->map));

	drm_gem_object_release(&gbo->bo.base);
}

static void drm_gem_vram_destroy(struct drm_gem_vram_object *gbo)
{
	drm_gem_vram_cleanup(gbo);
	kfree(gbo);
}

static void ttm_buffer_object_destroy(struct ttm_buffer_object *bo)
{
	struct drm_gem_vram_object *gbo = drm_gem_vram_of_bo(bo);

	drm_gem_vram_destroy(gbo);
}

static void drm_gem_vram_placement(struct drm_gem_vram_object *gbo,
				   unsigned long pl_flag)
{
	u32 invariant_flags = 0;
	unsigned int i;
	unsigned int c = 0;

	if (pl_flag & DRM_GEM_VRAM_PL_FLAG_TOPDOWN)
		invariant_flags = TTM_PL_FLAG_TOPDOWN;

	gbo->placement.placement = gbo->placements;
	gbo->placement.busy_placement = gbo->placements;

	if (pl_flag & DRM_GEM_VRAM_PL_FLAG_VRAM) {
		gbo->placements[c].mem_type = TTM_PL_VRAM;
		gbo->placements[c++].flags = invariant_flags;
	}

	if (pl_flag & DRM_GEM_VRAM_PL_FLAG_SYSTEM || !c) {
		gbo->placements[c].mem_type = TTM_PL_SYSTEM;
		gbo->placements[c++].flags = invariant_flags;
	}

	gbo->placement.num_placement = c;
	gbo->placement.num_busy_placement = c;

	for (i = 0; i < c; ++i) {
		gbo->placements[i].fpfn = 0;
		gbo->placements[i].lpfn = 0;
	}
}

 
struct drm_gem_vram_object *drm_gem_vram_create(struct drm_device *dev,
						size_t size,
						unsigned long pg_align)
{
	struct drm_gem_vram_object *gbo;
	struct drm_gem_object *gem;
	struct drm_vram_mm *vmm = dev->vram_mm;
	struct ttm_device *bdev;
	int ret;

	if (WARN_ONCE(!vmm, "VRAM MM not initialized"))
		return ERR_PTR(-EINVAL);

	if (dev->driver->gem_create_object) {
		gem = dev->driver->gem_create_object(dev, size);
		if (IS_ERR(gem))
			return ERR_CAST(gem);
		gbo = drm_gem_vram_of_gem(gem);
	} else {
		gbo = kzalloc(sizeof(*gbo), GFP_KERNEL);
		if (!gbo)
			return ERR_PTR(-ENOMEM);
		gem = &gbo->bo.base;
	}

	if (!gem->funcs)
		gem->funcs = &drm_gem_vram_object_funcs;

	ret = drm_gem_object_init(dev, gem, size);
	if (ret) {
		kfree(gbo);
		return ERR_PTR(ret);
	}

	bdev = &vmm->bdev;

	gbo->bo.bdev = bdev;
	drm_gem_vram_placement(gbo, DRM_GEM_VRAM_PL_FLAG_SYSTEM);

	 
	ret = ttm_bo_init_validate(bdev, &gbo->bo, ttm_bo_type_device,
				   &gbo->placement, pg_align, false, NULL, NULL,
				   ttm_buffer_object_destroy);
	if (ret)
		return ERR_PTR(ret);

	return gbo;
}
EXPORT_SYMBOL(drm_gem_vram_create);

 
void drm_gem_vram_put(struct drm_gem_vram_object *gbo)
{
	ttm_bo_put(&gbo->bo);
}
EXPORT_SYMBOL(drm_gem_vram_put);

static u64 drm_gem_vram_pg_offset(struct drm_gem_vram_object *gbo)
{
	 
	if (WARN_ON_ONCE(!gbo->bo.resource ||
			 gbo->bo.resource->mem_type == TTM_PL_SYSTEM))
		return 0;

	return gbo->bo.resource->start;
}

 
s64 drm_gem_vram_offset(struct drm_gem_vram_object *gbo)
{
	if (WARN_ON_ONCE(!gbo->bo.pin_count))
		return (s64)-ENODEV;
	return drm_gem_vram_pg_offset(gbo) << PAGE_SHIFT;
}
EXPORT_SYMBOL(drm_gem_vram_offset);

static int drm_gem_vram_pin_locked(struct drm_gem_vram_object *gbo,
				   unsigned long pl_flag)
{
	struct ttm_operation_ctx ctx = { false, false };
	int ret;

	if (gbo->bo.pin_count)
		goto out;

	if (pl_flag)
		drm_gem_vram_placement(gbo, pl_flag);

	ret = ttm_bo_validate(&gbo->bo, &gbo->placement, &ctx);
	if (ret < 0)
		return ret;

out:
	ttm_bo_pin(&gbo->bo);

	return 0;
}

 
int drm_gem_vram_pin(struct drm_gem_vram_object *gbo, unsigned long pl_flag)
{
	int ret;

	ret = ttm_bo_reserve(&gbo->bo, true, false, NULL);
	if (ret)
		return ret;
	ret = drm_gem_vram_pin_locked(gbo, pl_flag);
	ttm_bo_unreserve(&gbo->bo);

	return ret;
}
EXPORT_SYMBOL(drm_gem_vram_pin);

static void drm_gem_vram_unpin_locked(struct drm_gem_vram_object *gbo)
{
	ttm_bo_unpin(&gbo->bo);
}

 
int drm_gem_vram_unpin(struct drm_gem_vram_object *gbo)
{
	int ret;

	ret = ttm_bo_reserve(&gbo->bo, true, false, NULL);
	if (ret)
		return ret;

	drm_gem_vram_unpin_locked(gbo);
	ttm_bo_unreserve(&gbo->bo);

	return 0;
}
EXPORT_SYMBOL(drm_gem_vram_unpin);

static int drm_gem_vram_kmap_locked(struct drm_gem_vram_object *gbo,
				    struct iosys_map *map)
{
	int ret;

	if (gbo->vmap_use_count > 0)
		goto out;

	 
	if (iosys_map_is_null(&gbo->map)) {
		ret = ttm_bo_vmap(&gbo->bo, &gbo->map);
		if (ret)
			return ret;
	}

out:
	++gbo->vmap_use_count;
	*map = gbo->map;

	return 0;
}

static void drm_gem_vram_kunmap_locked(struct drm_gem_vram_object *gbo,
				       struct iosys_map *map)
{
	struct drm_device *dev = gbo->bo.base.dev;

	if (drm_WARN_ON_ONCE(dev, !gbo->vmap_use_count))
		return;

	if (drm_WARN_ON_ONCE(dev, !iosys_map_is_equal(&gbo->map, map)))
		return;  

	if (--gbo->vmap_use_count > 0)
		return;

	 
}

 
int drm_gem_vram_vmap(struct drm_gem_vram_object *gbo, struct iosys_map *map)
{
	int ret;

	dma_resv_assert_held(gbo->bo.base.resv);

	ret = drm_gem_vram_pin_locked(gbo, 0);
	if (ret)
		return ret;
	ret = drm_gem_vram_kmap_locked(gbo, map);
	if (ret)
		goto err_drm_gem_vram_unpin_locked;

	return 0;

err_drm_gem_vram_unpin_locked:
	drm_gem_vram_unpin_locked(gbo);
	return ret;
}
EXPORT_SYMBOL(drm_gem_vram_vmap);

 
void drm_gem_vram_vunmap(struct drm_gem_vram_object *gbo,
			 struct iosys_map *map)
{
	dma_resv_assert_held(gbo->bo.base.resv);

	drm_gem_vram_kunmap_locked(gbo, map);
	drm_gem_vram_unpin_locked(gbo);
}
EXPORT_SYMBOL(drm_gem_vram_vunmap);

 
int drm_gem_vram_fill_create_dumb(struct drm_file *file,
				  struct drm_device *dev,
				  unsigned long pg_align,
				  unsigned long pitch_align,
				  struct drm_mode_create_dumb *args)
{
	size_t pitch, size;
	struct drm_gem_vram_object *gbo;
	int ret;
	u32 handle;

	pitch = args->width * DIV_ROUND_UP(args->bpp, 8);
	if (pitch_align) {
		if (WARN_ON_ONCE(!is_power_of_2(pitch_align)))
			return -EINVAL;
		pitch = ALIGN(pitch, pitch_align);
	}
	size = pitch * args->height;

	size = roundup(size, PAGE_SIZE);
	if (!size)
		return -EINVAL;

	gbo = drm_gem_vram_create(dev, size, pg_align);
	if (IS_ERR(gbo))
		return PTR_ERR(gbo);

	ret = drm_gem_handle_create(file, &gbo->bo.base, &handle);
	if (ret)
		goto err_drm_gem_object_put;

	drm_gem_object_put(&gbo->bo.base);

	args->pitch = pitch;
	args->size = size;
	args->handle = handle;

	return 0;

err_drm_gem_object_put:
	drm_gem_object_put(&gbo->bo.base);
	return ret;
}
EXPORT_SYMBOL(drm_gem_vram_fill_create_dumb);

 

static bool drm_is_gem_vram(struct ttm_buffer_object *bo)
{
	return (bo->destroy == ttm_buffer_object_destroy);
}

static void drm_gem_vram_bo_driver_evict_flags(struct drm_gem_vram_object *gbo,
					       struct ttm_placement *pl)
{
	drm_gem_vram_placement(gbo, DRM_GEM_VRAM_PL_FLAG_SYSTEM);
	*pl = gbo->placement;
}

static void drm_gem_vram_bo_driver_move_notify(struct drm_gem_vram_object *gbo)
{
	struct ttm_buffer_object *bo = &gbo->bo;
	struct drm_device *dev = bo->base.dev;

	if (drm_WARN_ON_ONCE(dev, gbo->vmap_use_count))
		return;

	ttm_bo_vunmap(bo, &gbo->map);
	iosys_map_clear(&gbo->map);  
}

static int drm_gem_vram_bo_driver_move(struct drm_gem_vram_object *gbo,
				       bool evict,
				       struct ttm_operation_ctx *ctx,
				       struct ttm_resource *new_mem)
{
	drm_gem_vram_bo_driver_move_notify(gbo);
	return ttm_bo_move_memcpy(&gbo->bo, ctx, new_mem);
}

 

 
static void drm_gem_vram_object_free(struct drm_gem_object *gem)
{
	struct drm_gem_vram_object *gbo = drm_gem_vram_of_gem(gem);

	drm_gem_vram_put(gbo);
}

 

 
int drm_gem_vram_driver_dumb_create(struct drm_file *file,
				    struct drm_device *dev,
				    struct drm_mode_create_dumb *args)
{
	if (WARN_ONCE(!dev->vram_mm, "VRAM MM not initialized"))
		return -EINVAL;

	return drm_gem_vram_fill_create_dumb(file, dev, 0, 0, args);
}
EXPORT_SYMBOL(drm_gem_vram_driver_dumb_create);

 

static void __drm_gem_vram_plane_helper_cleanup_fb(struct drm_plane *plane,
						   struct drm_plane_state *state,
						   unsigned int num_planes)
{
	struct drm_gem_object *obj;
	struct drm_gem_vram_object *gbo;
	struct drm_framebuffer *fb = state->fb;

	while (num_planes) {
		--num_planes;
		obj = drm_gem_fb_get_obj(fb, num_planes);
		if (!obj)
			continue;
		gbo = drm_gem_vram_of_gem(obj);
		drm_gem_vram_unpin(gbo);
	}
}

 
int
drm_gem_vram_plane_helper_prepare_fb(struct drm_plane *plane,
				     struct drm_plane_state *new_state)
{
	struct drm_framebuffer *fb = new_state->fb;
	struct drm_gem_vram_object *gbo;
	struct drm_gem_object *obj;
	unsigned int i;
	int ret;

	if (!fb)
		return 0;

	for (i = 0; i < fb->format->num_planes; ++i) {
		obj = drm_gem_fb_get_obj(fb, i);
		if (!obj) {
			ret = -EINVAL;
			goto err_drm_gem_vram_unpin;
		}
		gbo = drm_gem_vram_of_gem(obj);
		ret = drm_gem_vram_pin(gbo, DRM_GEM_VRAM_PL_FLAG_VRAM);
		if (ret)
			goto err_drm_gem_vram_unpin;
	}

	ret = drm_gem_plane_helper_prepare_fb(plane, new_state);
	if (ret)
		goto err_drm_gem_vram_unpin;

	return 0;

err_drm_gem_vram_unpin:
	__drm_gem_vram_plane_helper_cleanup_fb(plane, new_state, i);
	return ret;
}
EXPORT_SYMBOL(drm_gem_vram_plane_helper_prepare_fb);

 
void
drm_gem_vram_plane_helper_cleanup_fb(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct drm_framebuffer *fb = old_state->fb;

	if (!fb)
		return;

	__drm_gem_vram_plane_helper_cleanup_fb(plane, old_state, fb->format->num_planes);
}
EXPORT_SYMBOL(drm_gem_vram_plane_helper_cleanup_fb);

 

 
int drm_gem_vram_simple_display_pipe_prepare_fb(
	struct drm_simple_display_pipe *pipe,
	struct drm_plane_state *new_state)
{
	return drm_gem_vram_plane_helper_prepare_fb(&pipe->plane, new_state);
}
EXPORT_SYMBOL(drm_gem_vram_simple_display_pipe_prepare_fb);

 
void drm_gem_vram_simple_display_pipe_cleanup_fb(
	struct drm_simple_display_pipe *pipe,
	struct drm_plane_state *old_state)
{
	drm_gem_vram_plane_helper_cleanup_fb(&pipe->plane, old_state);
}
EXPORT_SYMBOL(drm_gem_vram_simple_display_pipe_cleanup_fb);

 

 
static int drm_gem_vram_object_pin(struct drm_gem_object *gem)
{
	struct drm_gem_vram_object *gbo = drm_gem_vram_of_gem(gem);

	 
	return drm_gem_vram_pin(gbo, 0);
}

 
static void drm_gem_vram_object_unpin(struct drm_gem_object *gem)
{
	struct drm_gem_vram_object *gbo = drm_gem_vram_of_gem(gem);

	drm_gem_vram_unpin(gbo);
}

 
static int drm_gem_vram_object_vmap(struct drm_gem_object *gem,
				    struct iosys_map *map)
{
	struct drm_gem_vram_object *gbo = drm_gem_vram_of_gem(gem);

	return drm_gem_vram_vmap(gbo, map);
}

 
static void drm_gem_vram_object_vunmap(struct drm_gem_object *gem,
				       struct iosys_map *map)
{
	struct drm_gem_vram_object *gbo = drm_gem_vram_of_gem(gem);

	drm_gem_vram_vunmap(gbo, map);
}

 

static const struct drm_gem_object_funcs drm_gem_vram_object_funcs = {
	.free	= drm_gem_vram_object_free,
	.pin	= drm_gem_vram_object_pin,
	.unpin	= drm_gem_vram_object_unpin,
	.vmap	= drm_gem_vram_object_vmap,
	.vunmap	= drm_gem_vram_object_vunmap,
	.mmap   = drm_gem_ttm_mmap,
	.print_info = drm_gem_ttm_print_info,
};

 

 

static void bo_driver_ttm_tt_destroy(struct ttm_device *bdev, struct ttm_tt *tt)
{
	ttm_tt_fini(tt);
	kfree(tt);
}

 

static struct ttm_tt *bo_driver_ttm_tt_create(struct ttm_buffer_object *bo,
					      uint32_t page_flags)
{
	struct ttm_tt *tt;
	int ret;

	tt = kzalloc(sizeof(*tt), GFP_KERNEL);
	if (!tt)
		return NULL;

	ret = ttm_tt_init(tt, bo, page_flags, ttm_cached, 0);
	if (ret < 0)
		goto err_ttm_tt_init;

	return tt;

err_ttm_tt_init:
	kfree(tt);
	return NULL;
}

static void bo_driver_evict_flags(struct ttm_buffer_object *bo,
				  struct ttm_placement *placement)
{
	struct drm_gem_vram_object *gbo;

	 
	if (!drm_is_gem_vram(bo))
		return;

	gbo = drm_gem_vram_of_bo(bo);

	drm_gem_vram_bo_driver_evict_flags(gbo, placement);
}

static void bo_driver_delete_mem_notify(struct ttm_buffer_object *bo)
{
	struct drm_gem_vram_object *gbo;

	 
	if (!drm_is_gem_vram(bo))
		return;

	gbo = drm_gem_vram_of_bo(bo);

	drm_gem_vram_bo_driver_move_notify(gbo);
}

static int bo_driver_move(struct ttm_buffer_object *bo,
			  bool evict,
			  struct ttm_operation_ctx *ctx,
			  struct ttm_resource *new_mem,
			  struct ttm_place *hop)
{
	struct drm_gem_vram_object *gbo;

	if (!bo->resource) {
		if (new_mem->mem_type != TTM_PL_SYSTEM) {
			hop->mem_type = TTM_PL_SYSTEM;
			hop->flags = TTM_PL_FLAG_TEMPORARY;
			return -EMULTIHOP;
		}

		ttm_bo_move_null(bo, new_mem);
		return 0;
	}

	gbo = drm_gem_vram_of_bo(bo);

	return drm_gem_vram_bo_driver_move(gbo, evict, ctx, new_mem);
}

static int bo_driver_io_mem_reserve(struct ttm_device *bdev,
				    struct ttm_resource *mem)
{
	struct drm_vram_mm *vmm = drm_vram_mm_of_bdev(bdev);

	switch (mem->mem_type) {
	case TTM_PL_SYSTEM:	 
		break;
	case TTM_PL_VRAM:
		mem->bus.offset = (mem->start << PAGE_SHIFT) + vmm->vram_base;
		mem->bus.is_iomem = true;
		mem->bus.caching = ttm_write_combined;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct ttm_device_funcs bo_driver = {
	.ttm_tt_create = bo_driver_ttm_tt_create,
	.ttm_tt_destroy = bo_driver_ttm_tt_destroy,
	.eviction_valuable = ttm_bo_eviction_valuable,
	.evict_flags = bo_driver_evict_flags,
	.move = bo_driver_move,
	.delete_mem_notify = bo_driver_delete_mem_notify,
	.io_mem_reserve = bo_driver_io_mem_reserve,
};

 

static int drm_vram_mm_debugfs(struct seq_file *m, void *data)
{
	struct drm_debugfs_entry *entry = m->private;
	struct drm_vram_mm *vmm = entry->dev->vram_mm;
	struct ttm_resource_manager *man = ttm_manager_type(&vmm->bdev, TTM_PL_VRAM);
	struct drm_printer p = drm_seq_file_printer(m);

	ttm_resource_manager_debug(man, &p);
	return 0;
}

static const struct drm_debugfs_info drm_vram_mm_debugfs_list[] = {
	{ "vram-mm", drm_vram_mm_debugfs, 0, NULL },
};

 
void drm_vram_mm_debugfs_init(struct drm_minor *minor)
{
	drm_debugfs_add_files(minor->dev, drm_vram_mm_debugfs_list,
			      ARRAY_SIZE(drm_vram_mm_debugfs_list));
}
EXPORT_SYMBOL(drm_vram_mm_debugfs_init);

static int drm_vram_mm_init(struct drm_vram_mm *vmm, struct drm_device *dev,
			    uint64_t vram_base, size_t vram_size)
{
	int ret;

	vmm->vram_base = vram_base;
	vmm->vram_size = vram_size;

	ret = ttm_device_init(&vmm->bdev, &bo_driver, dev->dev,
				 dev->anon_inode->i_mapping,
				 dev->vma_offset_manager,
				 false, true);
	if (ret)
		return ret;

	ret = ttm_range_man_init(&vmm->bdev, TTM_PL_VRAM,
				 false, vram_size >> PAGE_SHIFT);
	if (ret)
		return ret;

	return 0;
}

static void drm_vram_mm_cleanup(struct drm_vram_mm *vmm)
{
	ttm_range_man_fini(&vmm->bdev, TTM_PL_VRAM);
	ttm_device_fini(&vmm->bdev);
}

 

static struct drm_vram_mm *drm_vram_helper_alloc_mm(struct drm_device *dev, uint64_t vram_base,
						    size_t vram_size)
{
	int ret;

	if (WARN_ON(dev->vram_mm))
		return dev->vram_mm;

	dev->vram_mm = kzalloc(sizeof(*dev->vram_mm), GFP_KERNEL);
	if (!dev->vram_mm)
		return ERR_PTR(-ENOMEM);

	ret = drm_vram_mm_init(dev->vram_mm, dev, vram_base, vram_size);
	if (ret)
		goto err_kfree;

	return dev->vram_mm;

err_kfree:
	kfree(dev->vram_mm);
	dev->vram_mm = NULL;
	return ERR_PTR(ret);
}

static void drm_vram_helper_release_mm(struct drm_device *dev)
{
	if (!dev->vram_mm)
		return;

	drm_vram_mm_cleanup(dev->vram_mm);
	kfree(dev->vram_mm);
	dev->vram_mm = NULL;
}

static void drm_vram_mm_release(struct drm_device *dev, void *ptr)
{
	drm_vram_helper_release_mm(dev);
}

 
int drmm_vram_helper_init(struct drm_device *dev, uint64_t vram_base,
			  size_t vram_size)
{
	struct drm_vram_mm *vram_mm;

	if (drm_WARN_ON_ONCE(dev, dev->vram_mm))
		return 0;

	vram_mm = drm_vram_helper_alloc_mm(dev, vram_base, vram_size);
	if (IS_ERR(vram_mm))
		return PTR_ERR(vram_mm);
	return drmm_add_action_or_reset(dev, drm_vram_mm_release, NULL);
}
EXPORT_SYMBOL(drmm_vram_helper_init);

 

static enum drm_mode_status
drm_vram_helper_mode_valid_internal(struct drm_device *dev,
				    const struct drm_display_mode *mode,
				    unsigned long max_bpp)
{
	struct drm_vram_mm *vmm = dev->vram_mm;
	unsigned long fbsize, fbpages, max_fbpages;

	if (WARN_ON(!dev->vram_mm))
		return MODE_BAD;

	max_fbpages = (vmm->vram_size / 2) >> PAGE_SHIFT;

	fbsize = mode->hdisplay * mode->vdisplay * max_bpp;
	fbpages = DIV_ROUND_UP(fbsize, PAGE_SIZE);

	if (fbpages > max_fbpages)
		return MODE_MEM;

	return MODE_OK;
}

 
enum drm_mode_status
drm_vram_helper_mode_valid(struct drm_device *dev,
			   const struct drm_display_mode *mode)
{
	static const unsigned long max_bpp = 4;  

	return drm_vram_helper_mode_valid_internal(dev, mode, max_bpp);
}
EXPORT_SYMBOL(drm_vram_helper_mode_valid);

MODULE_DESCRIPTION("DRM VRAM memory-management helpers");
MODULE_LICENSE("GPL");
