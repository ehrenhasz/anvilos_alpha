
 

#include <linux/slab.h>
#include <linux/module.h>

#include <drm/drm_damage_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_modeset_helper.h>

#include "drm_internal.h"

MODULE_IMPORT_NS(DMA_BUF);

#define AFBC_HEADER_SIZE		16
#define AFBC_TH_LAYOUT_ALIGNMENT	8
#define AFBC_HDR_ALIGN			64
#define AFBC_SUPERBLOCK_PIXELS		256
#define AFBC_SUPERBLOCK_ALIGNMENT	128
#define AFBC_TH_BODY_START_ALIGNMENT	4096

 

 
struct drm_gem_object *drm_gem_fb_get_obj(struct drm_framebuffer *fb,
					  unsigned int plane)
{
	struct drm_device *dev = fb->dev;

	if (drm_WARN_ON_ONCE(dev, plane >= ARRAY_SIZE(fb->obj)))
		return NULL;
	else if (drm_WARN_ON_ONCE(dev, !fb->obj[plane]))
		return NULL;

	return fb->obj[plane];
}
EXPORT_SYMBOL_GPL(drm_gem_fb_get_obj);

static int
drm_gem_fb_init(struct drm_device *dev,
		 struct drm_framebuffer *fb,
		 const struct drm_mode_fb_cmd2 *mode_cmd,
		 struct drm_gem_object **obj, unsigned int num_planes,
		 const struct drm_framebuffer_funcs *funcs)
{
	unsigned int i;
	int ret;

	drm_helper_mode_fill_fb_struct(dev, fb, mode_cmd);

	for (i = 0; i < num_planes; i++)
		fb->obj[i] = obj[i];

	ret = drm_framebuffer_init(dev, fb, funcs);
	if (ret)
		drm_err(dev, "Failed to init framebuffer: %d\n", ret);

	return ret;
}

 
void drm_gem_fb_destroy(struct drm_framebuffer *fb)
{
	unsigned int i;

	for (i = 0; i < fb->format->num_planes; i++)
		drm_gem_object_put(fb->obj[i]);

	drm_framebuffer_cleanup(fb);
	kfree(fb);
}
EXPORT_SYMBOL(drm_gem_fb_destroy);

 
int drm_gem_fb_create_handle(struct drm_framebuffer *fb, struct drm_file *file,
			     unsigned int *handle)
{
	return drm_gem_handle_create(file, fb->obj[0], handle);
}
EXPORT_SYMBOL(drm_gem_fb_create_handle);

 
int drm_gem_fb_init_with_funcs(struct drm_device *dev,
			       struct drm_framebuffer *fb,
			       struct drm_file *file,
			       const struct drm_mode_fb_cmd2 *mode_cmd,
			       const struct drm_framebuffer_funcs *funcs)
{
	const struct drm_format_info *info;
	struct drm_gem_object *objs[DRM_FORMAT_MAX_PLANES];
	unsigned int i;
	int ret;

	info = drm_get_format_info(dev, mode_cmd);
	if (!info) {
		drm_dbg_kms(dev, "Failed to get FB format info\n");
		return -EINVAL;
	}

	if (drm_drv_uses_atomic_modeset(dev) &&
	    !drm_any_plane_has_format(dev, mode_cmd->pixel_format,
				      mode_cmd->modifier[0])) {
		drm_dbg_kms(dev, "Unsupported pixel format %p4cc / modifier 0x%llx\n",
			    &mode_cmd->pixel_format, mode_cmd->modifier[0]);
		return -EINVAL;
	}

	for (i = 0; i < info->num_planes; i++) {
		unsigned int width = mode_cmd->width / (i ? info->hsub : 1);
		unsigned int height = mode_cmd->height / (i ? info->vsub : 1);
		unsigned int min_size;

		objs[i] = drm_gem_object_lookup(file, mode_cmd->handles[i]);
		if (!objs[i]) {
			drm_dbg_kms(dev, "Failed to lookup GEM object\n");
			ret = -ENOENT;
			goto err_gem_object_put;
		}

		min_size = (height - 1) * mode_cmd->pitches[i]
			 + drm_format_info_min_pitch(info, i, width)
			 + mode_cmd->offsets[i];

		if (objs[i]->size < min_size) {
			drm_dbg_kms(dev,
				    "GEM object size (%zu) smaller than minimum size (%u) for plane %d\n",
				    objs[i]->size, min_size, i);
			drm_gem_object_put(objs[i]);
			ret = -EINVAL;
			goto err_gem_object_put;
		}
	}

	ret = drm_gem_fb_init(dev, fb, mode_cmd, objs, i, funcs);
	if (ret)
		goto err_gem_object_put;

	return 0;

err_gem_object_put:
	while (i > 0) {
		--i;
		drm_gem_object_put(objs[i]);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(drm_gem_fb_init_with_funcs);

 
struct drm_framebuffer *
drm_gem_fb_create_with_funcs(struct drm_device *dev, struct drm_file *file,
			     const struct drm_mode_fb_cmd2 *mode_cmd,
			     const struct drm_framebuffer_funcs *funcs)
{
	struct drm_framebuffer *fb;
	int ret;

	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if (!fb)
		return ERR_PTR(-ENOMEM);

	ret = drm_gem_fb_init_with_funcs(dev, fb, file, mode_cmd, funcs);
	if (ret) {
		kfree(fb);
		return ERR_PTR(ret);
	}

	return fb;
}
EXPORT_SYMBOL_GPL(drm_gem_fb_create_with_funcs);

static const struct drm_framebuffer_funcs drm_gem_fb_funcs = {
	.destroy	= drm_gem_fb_destroy,
	.create_handle	= drm_gem_fb_create_handle,
};

 
struct drm_framebuffer *
drm_gem_fb_create(struct drm_device *dev, struct drm_file *file,
		  const struct drm_mode_fb_cmd2 *mode_cmd)
{
	return drm_gem_fb_create_with_funcs(dev, file, mode_cmd,
					    &drm_gem_fb_funcs);
}
EXPORT_SYMBOL_GPL(drm_gem_fb_create);

static const struct drm_framebuffer_funcs drm_gem_fb_funcs_dirtyfb = {
	.destroy	= drm_gem_fb_destroy,
	.create_handle	= drm_gem_fb_create_handle,
	.dirty		= drm_atomic_helper_dirtyfb,
};

 
struct drm_framebuffer *
drm_gem_fb_create_with_dirty(struct drm_device *dev, struct drm_file *file,
			     const struct drm_mode_fb_cmd2 *mode_cmd)
{
	return drm_gem_fb_create_with_funcs(dev, file, mode_cmd,
					    &drm_gem_fb_funcs_dirtyfb);
}
EXPORT_SYMBOL_GPL(drm_gem_fb_create_with_dirty);

 
int drm_gem_fb_vmap(struct drm_framebuffer *fb, struct iosys_map *map,
		    struct iosys_map *data)
{
	struct drm_gem_object *obj;
	unsigned int i;
	int ret;

	for (i = 0; i < fb->format->num_planes; ++i) {
		obj = drm_gem_fb_get_obj(fb, i);
		if (!obj) {
			ret = -EINVAL;
			goto err_drm_gem_vunmap;
		}
		ret = drm_gem_vmap_unlocked(obj, &map[i]);
		if (ret)
			goto err_drm_gem_vunmap;
	}

	if (data) {
		for (i = 0; i < fb->format->num_planes; ++i) {
			memcpy(&data[i], &map[i], sizeof(data[i]));
			if (iosys_map_is_null(&data[i]))
				continue;
			iosys_map_incr(&data[i], fb->offsets[i]);
		}
	}

	return 0;

err_drm_gem_vunmap:
	while (i) {
		--i;
		obj = drm_gem_fb_get_obj(fb, i);
		if (!obj)
			continue;
		drm_gem_vunmap_unlocked(obj, &map[i]);
	}
	return ret;
}
EXPORT_SYMBOL(drm_gem_fb_vmap);

 
void drm_gem_fb_vunmap(struct drm_framebuffer *fb, struct iosys_map *map)
{
	unsigned int i = fb->format->num_planes;
	struct drm_gem_object *obj;

	while (i) {
		--i;
		obj = drm_gem_fb_get_obj(fb, i);
		if (!obj)
			continue;
		if (iosys_map_is_null(&map[i]))
			continue;
		drm_gem_vunmap_unlocked(obj, &map[i]);
	}
}
EXPORT_SYMBOL(drm_gem_fb_vunmap);

static void __drm_gem_fb_end_cpu_access(struct drm_framebuffer *fb, enum dma_data_direction dir,
					unsigned int num_planes)
{
	struct dma_buf_attachment *import_attach;
	struct drm_gem_object *obj;
	int ret;

	while (num_planes) {
		--num_planes;
		obj = drm_gem_fb_get_obj(fb, num_planes);
		if (!obj)
			continue;
		import_attach = obj->import_attach;
		if (!import_attach)
			continue;
		ret = dma_buf_end_cpu_access(import_attach->dmabuf, dir);
		if (ret)
			drm_err(fb->dev, "dma_buf_end_cpu_access(%u, %d) failed: %d\n",
				ret, num_planes, dir);
	}
}

 
int drm_gem_fb_begin_cpu_access(struct drm_framebuffer *fb, enum dma_data_direction dir)
{
	struct dma_buf_attachment *import_attach;
	struct drm_gem_object *obj;
	unsigned int i;
	int ret;

	for (i = 0; i < fb->format->num_planes; ++i) {
		obj = drm_gem_fb_get_obj(fb, i);
		if (!obj) {
			ret = -EINVAL;
			goto err___drm_gem_fb_end_cpu_access;
		}
		import_attach = obj->import_attach;
		if (!import_attach)
			continue;
		ret = dma_buf_begin_cpu_access(import_attach->dmabuf, dir);
		if (ret)
			goto err___drm_gem_fb_end_cpu_access;
	}

	return 0;

err___drm_gem_fb_end_cpu_access:
	__drm_gem_fb_end_cpu_access(fb, dir, i);
	return ret;
}
EXPORT_SYMBOL(drm_gem_fb_begin_cpu_access);

 
void drm_gem_fb_end_cpu_access(struct drm_framebuffer *fb, enum dma_data_direction dir)
{
	__drm_gem_fb_end_cpu_access(fb, dir, fb->format->num_planes);
}
EXPORT_SYMBOL(drm_gem_fb_end_cpu_access);

 
 
static __u32 drm_gem_afbc_get_bpp(struct drm_device *dev,
				  const struct drm_mode_fb_cmd2 *mode_cmd)
{
	const struct drm_format_info *info;

	info = drm_get_format_info(dev, mode_cmd);

	switch (info->format) {
	case DRM_FORMAT_YUV420_8BIT:
		return 12;
	case DRM_FORMAT_YUV420_10BIT:
		return 15;
	case DRM_FORMAT_VUY101010:
		return 30;
	default:
		return drm_format_info_bpp(info, 0);
	}
}

static int drm_gem_afbc_min_size(struct drm_device *dev,
				 const struct drm_mode_fb_cmd2 *mode_cmd,
				 struct drm_afbc_framebuffer *afbc_fb)
{
	__u32 n_blocks, w_alignment, h_alignment, hdr_alignment;
	 
	__u32 bpp;

	switch (mode_cmd->modifier[0] & AFBC_FORMAT_MOD_BLOCK_SIZE_MASK) {
	case AFBC_FORMAT_MOD_BLOCK_SIZE_16x16:
		afbc_fb->block_width = 16;
		afbc_fb->block_height = 16;
		break;
	case AFBC_FORMAT_MOD_BLOCK_SIZE_32x8:
		afbc_fb->block_width = 32;
		afbc_fb->block_height = 8;
		break;
	 
	case AFBC_FORMAT_MOD_BLOCK_SIZE_64x4:
	case AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4:
	default:
		drm_dbg_kms(dev, "Invalid AFBC_FORMAT_MOD_BLOCK_SIZE: %lld.\n",
			    mode_cmd->modifier[0]
			    & AFBC_FORMAT_MOD_BLOCK_SIZE_MASK);
		return -EINVAL;
	}

	 
	w_alignment = afbc_fb->block_width;
	h_alignment = afbc_fb->block_height;
	hdr_alignment = AFBC_HDR_ALIGN;
	if (mode_cmd->modifier[0] & AFBC_FORMAT_MOD_TILED) {
		w_alignment *= AFBC_TH_LAYOUT_ALIGNMENT;
		h_alignment *= AFBC_TH_LAYOUT_ALIGNMENT;
		hdr_alignment = AFBC_TH_BODY_START_ALIGNMENT;
	}

	afbc_fb->aligned_width = ALIGN(mode_cmd->width, w_alignment);
	afbc_fb->aligned_height = ALIGN(mode_cmd->height, h_alignment);
	afbc_fb->offset = mode_cmd->offsets[0];

	bpp = drm_gem_afbc_get_bpp(dev, mode_cmd);
	if (!bpp) {
		drm_dbg_kms(dev, "Invalid AFBC bpp value: %d\n", bpp);
		return -EINVAL;
	}

	n_blocks = (afbc_fb->aligned_width * afbc_fb->aligned_height)
		   / AFBC_SUPERBLOCK_PIXELS;
	afbc_fb->afbc_size = ALIGN(n_blocks * AFBC_HEADER_SIZE, hdr_alignment);
	afbc_fb->afbc_size += n_blocks * ALIGN(bpp * AFBC_SUPERBLOCK_PIXELS / 8,
					       AFBC_SUPERBLOCK_ALIGNMENT);

	return 0;
}

 
int drm_gem_fb_afbc_init(struct drm_device *dev,
			 const struct drm_mode_fb_cmd2 *mode_cmd,
			 struct drm_afbc_framebuffer *afbc_fb)
{
	const struct drm_format_info *info;
	struct drm_gem_object **objs;
	int ret;

	objs = afbc_fb->base.obj;
	info = drm_get_format_info(dev, mode_cmd);
	if (!info)
		return -EINVAL;

	ret = drm_gem_afbc_min_size(dev, mode_cmd, afbc_fb);
	if (ret < 0)
		return ret;

	if (objs[0]->size < afbc_fb->afbc_size)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL_GPL(drm_gem_fb_afbc_init);
