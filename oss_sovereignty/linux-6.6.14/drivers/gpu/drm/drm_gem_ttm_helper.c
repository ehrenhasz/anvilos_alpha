

#include <linux/module.h>

#include <drm/drm_gem_ttm_helper.h>
#include <drm/ttm/ttm_placement.h>
#include <drm/ttm/ttm_tt.h>

 

 
void drm_gem_ttm_print_info(struct drm_printer *p, unsigned int indent,
			    const struct drm_gem_object *gem)
{
	static const char * const plname[] = {
		[ TTM_PL_SYSTEM ] = "system",
		[ TTM_PL_TT     ] = "tt",
		[ TTM_PL_VRAM   ] = "vram",
		[ TTM_PL_PRIV   ] = "priv",

		[ 16 ]            = "cached",
		[ 17 ]            = "uncached",
		[ 18 ]            = "wc",
		[ 19 ]            = "contig",

		[ 21 ]            = "pinned",  
		[ 22 ]            = "topdown",
	};
	const struct ttm_buffer_object *bo = drm_gem_ttm_of_gem(gem);

	drm_printf_indent(p, indent, "placement=");
	drm_print_bits(p, bo->resource->placement, plname, ARRAY_SIZE(plname));
	drm_printf(p, "\n");

	if (bo->resource->bus.is_iomem)
		drm_printf_indent(p, indent, "bus.offset=%lx\n",
				  (unsigned long)bo->resource->bus.offset);
}
EXPORT_SYMBOL(drm_gem_ttm_print_info);

 
int drm_gem_ttm_vmap(struct drm_gem_object *gem,
		     struct iosys_map *map)
{
	struct ttm_buffer_object *bo = drm_gem_ttm_of_gem(gem);

	return ttm_bo_vmap(bo, map);
}
EXPORT_SYMBOL(drm_gem_ttm_vmap);

 
void drm_gem_ttm_vunmap(struct drm_gem_object *gem,
			struct iosys_map *map)
{
	struct ttm_buffer_object *bo = drm_gem_ttm_of_gem(gem);

	ttm_bo_vunmap(bo, map);
}
EXPORT_SYMBOL(drm_gem_ttm_vunmap);

 
int drm_gem_ttm_mmap(struct drm_gem_object *gem,
		     struct vm_area_struct *vma)
{
	struct ttm_buffer_object *bo = drm_gem_ttm_of_gem(gem);
	int ret;

	ret = ttm_bo_mmap_obj(vma, bo);
	if (ret < 0)
		return ret;

	 
	drm_gem_object_put(gem);

	return 0;
}
EXPORT_SYMBOL(drm_gem_ttm_mmap);

 
int drm_gem_ttm_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
				uint32_t handle, uint64_t *offset)
{
	struct drm_gem_object *gem;

	gem = drm_gem_object_lookup(file, handle);
	if (!gem)
		return -ENOENT;

	*offset = drm_vma_node_offset_addr(&gem->vma_node);

	drm_gem_object_put(gem);

	return 0;
}
EXPORT_SYMBOL(drm_gem_ttm_dumb_map_offset);

MODULE_DESCRIPTION("DRM gem ttm helpers");
MODULE_LICENSE("GPL");
