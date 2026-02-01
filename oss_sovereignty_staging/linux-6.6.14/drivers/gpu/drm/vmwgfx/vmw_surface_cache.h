 

#ifndef VMW_SURFACE_CACHE_H
#define VMW_SURFACE_CACHE_H

#include "device_include/svga3d_surfacedefs.h"

#include <drm/vmwgfx_drm.h>

static inline u32 clamped_umul32(u32 a, u32 b)
{
	uint64_t tmp = (uint64_t) a*b;
	return (tmp > (uint64_t) ((u32) -1)) ? (u32) -1 : tmp;
}

 
static inline const SVGA3dSurfaceDesc *
vmw_surface_get_desc(SVGA3dSurfaceFormat format)
{
	if (format < ARRAY_SIZE(g_SVGA3dSurfaceDescs))
		return &g_SVGA3dSurfaceDescs[format];

	return &g_SVGA3dSurfaceDescs[SVGA3D_FORMAT_INVALID];
}

 
static inline struct drm_vmw_size
vmw_surface_get_mip_size(struct drm_vmw_size base_level, u32 mip_level)
{
	struct drm_vmw_size size = {
		.width = max_t(u32, base_level.width >> mip_level, 1),
		.height = max_t(u32, base_level.height >> mip_level, 1),
		.depth = max_t(u32, base_level.depth >> mip_level, 1)
	};

	return size;
}

static inline void
vmw_surface_get_size_in_blocks(const SVGA3dSurfaceDesc *desc,
				 const struct drm_vmw_size *pixel_size,
				 SVGA3dSize *block_size)
{
	block_size->width = __KERNEL_DIV_ROUND_UP(pixel_size->width,
						  desc->blockSize.width);
	block_size->height = __KERNEL_DIV_ROUND_UP(pixel_size->height,
						   desc->blockSize.height);
	block_size->depth = __KERNEL_DIV_ROUND_UP(pixel_size->depth,
						  desc->blockSize.depth);
}

static inline bool
vmw_surface_is_planar_surface(const SVGA3dSurfaceDesc *desc)
{
	return (desc->blockDesc & SVGA3DBLOCKDESC_PLANAR_YUV) != 0;
}

static inline u32
vmw_surface_calculate_pitch(const SVGA3dSurfaceDesc *desc,
			      const struct drm_vmw_size *size)
{
	u32 pitch;
	SVGA3dSize blocks;

	vmw_surface_get_size_in_blocks(desc, size, &blocks);

	pitch = blocks.width * desc->pitchBytesPerBlock;

	return pitch;
}

 
static inline u32
vmw_surface_get_image_buffer_size(const SVGA3dSurfaceDesc *desc,
				    const struct drm_vmw_size *size,
				    u32 pitch)
{
	SVGA3dSize image_blocks;
	u32 slice_size, total_size;

	vmw_surface_get_size_in_blocks(desc, size, &image_blocks);

	if (vmw_surface_is_planar_surface(desc)) {
		total_size = clamped_umul32(image_blocks.width,
					    image_blocks.height);
		total_size = clamped_umul32(total_size, image_blocks.depth);
		total_size = clamped_umul32(total_size, desc->bytesPerBlock);
		return total_size;
	}

	if (pitch == 0)
		pitch = vmw_surface_calculate_pitch(desc, size);

	slice_size = clamped_umul32(image_blocks.height, pitch);
	total_size = clamped_umul32(slice_size, image_blocks.depth);

	return total_size;
}

 
static inline u32
vmw_surface_get_serialized_size(SVGA3dSurfaceFormat format,
				  struct drm_vmw_size base_level_size,
				  u32 num_mip_levels,
				  u32 num_layers)
{
	const SVGA3dSurfaceDesc *desc = vmw_surface_get_desc(format);
	u32 total_size = 0;
	u32 mip;

	for (mip = 0; mip < num_mip_levels; mip++) {
		struct drm_vmw_size size =
			vmw_surface_get_mip_size(base_level_size, mip);
		total_size += vmw_surface_get_image_buffer_size(desc,
								  &size, 0);
	}

	return total_size * num_layers;
}

 
static inline u32
vmw_surface_get_serialized_size_extended(SVGA3dSurfaceFormat format,
					   struct drm_vmw_size base_level_size,
					   u32 num_mip_levels,
					   u32 num_layers,
					   u32 num_samples)
{
	uint64_t total_size =
		vmw_surface_get_serialized_size(format,
						  base_level_size,
						  num_mip_levels,
						  num_layers);
	total_size *= max_t(u32, 1, num_samples);

	return min_t(uint64_t, total_size, (uint64_t)U32_MAX);
}

 
static inline u32
vmw_surface_get_pixel_offset(SVGA3dSurfaceFormat format,
			       u32 width, u32 height,
			       u32 x, u32 y, u32 z)
{
	const SVGA3dSurfaceDesc *desc = vmw_surface_get_desc(format);
	const u32 bw = desc->blockSize.width, bh = desc->blockSize.height;
	const u32 bd = desc->blockSize.depth;
	const u32 rowstride = __KERNEL_DIV_ROUND_UP(width, bw) *
			      desc->bytesPerBlock;
	const u32 imgstride = __KERNEL_DIV_ROUND_UP(height, bh) * rowstride;
	const u32 offset = (z / bd * imgstride +
			    y / bh * rowstride +
			    x / bw * desc->bytesPerBlock);
	return offset;
}

static inline u32
vmw_surface_get_image_offset(SVGA3dSurfaceFormat format,
			       struct drm_vmw_size baseLevelSize,
			       u32 numMipLevels,
			       u32 face,
			       u32 mip)

{
	u32 offset;
	u32 mipChainBytes;
	u32 mipChainBytesToLevel;
	u32 i;
	const SVGA3dSurfaceDesc *desc;
	struct drm_vmw_size mipSize;
	u32 bytes;

	desc = vmw_surface_get_desc(format);

	mipChainBytes = 0;
	mipChainBytesToLevel = 0;
	for (i = 0; i < numMipLevels; i++) {
		mipSize = vmw_surface_get_mip_size(baseLevelSize, i);
		bytes = vmw_surface_get_image_buffer_size(desc, &mipSize, 0);
		mipChainBytes += bytes;
		if (i < mip)
			mipChainBytesToLevel += bytes;
	}

	offset = mipChainBytes * face + mipChainBytesToLevel;

	return offset;
}


 
static inline bool
vmw_surface_is_gb_screen_target_format(SVGA3dSurfaceFormat format)
{
	return (format == SVGA3D_X8R8G8B8 ||
		format == SVGA3D_A8R8G8B8 ||
		format == SVGA3D_R5G6B5   ||
		format == SVGA3D_X1R5G5B5 ||
		format == SVGA3D_A1R5G5B5 ||
		format == SVGA3D_P8);
}


 
static inline bool
vmw_surface_is_dx_screen_target_format(SVGA3dSurfaceFormat format)
{
	return (format == SVGA3D_R8G8B8A8_UNORM ||
		format == SVGA3D_B8G8R8A8_UNORM ||
		format == SVGA3D_B8G8R8X8_UNORM);
}


 
static inline bool
vmw_surface_is_screen_target_format(SVGA3dSurfaceFormat format)
{
	if (vmw_surface_is_gb_screen_target_format(format)) {
		return true;
	}
	return vmw_surface_is_dx_screen_target_format(format);
}

 
struct vmw_surface_mip {
	size_t bytes;
	size_t img_stride;
	size_t row_stride;
	struct drm_vmw_size size;

};

 
struct vmw_surface_cache {
	const SVGA3dSurfaceDesc *desc;
	struct vmw_surface_mip mip[DRM_VMW_MAX_MIP_LEVELS];
	size_t mip_chain_bytes;
	size_t sheet_bytes;
	u32 num_mip_levels;
	u32 num_layers;
};

 
struct vmw_surface_loc {
	u32 sheet;
	u32 sub_resource;
	u32 x, y, z;
};

 
static inline u32 vmw_surface_subres(const struct vmw_surface_cache *cache,
				       u32 mip_level, u32 layer)
{
	return cache->num_mip_levels * layer + mip_level;
}

 
static inline int vmw_surface_setup_cache(const struct drm_vmw_size *size,
					    SVGA3dSurfaceFormat format,
					    u32 num_mip_levels,
					    u32 num_layers,
					    u32 num_samples,
					    struct vmw_surface_cache *cache)
{
	const SVGA3dSurfaceDesc *desc;
	u32 i;

	memset(cache, 0, sizeof(*cache));
	cache->desc = desc = vmw_surface_get_desc(format);
	cache->num_mip_levels = num_mip_levels;
	cache->num_layers = num_layers;
	for (i = 0; i < cache->num_mip_levels; i++) {
		struct vmw_surface_mip *mip = &cache->mip[i];

		mip->size = vmw_surface_get_mip_size(*size, i);
		mip->bytes = vmw_surface_get_image_buffer_size
			(desc, &mip->size, 0);
		mip->row_stride =
			__KERNEL_DIV_ROUND_UP(mip->size.width,
					      desc->blockSize.width) *
			desc->bytesPerBlock * num_samples;
		if (!mip->row_stride)
			goto invalid_dim;

		mip->img_stride =
			__KERNEL_DIV_ROUND_UP(mip->size.height,
					      desc->blockSize.height) *
			mip->row_stride;
		if (!mip->img_stride)
			goto invalid_dim;

		cache->mip_chain_bytes += mip->bytes;
	}
	cache->sheet_bytes = cache->mip_chain_bytes * num_layers;
	if (!cache->sheet_bytes)
		goto invalid_dim;

	return 0;

invalid_dim:
	VMW_DEBUG_USER("Invalid surface layout for dirty tracking.\n");
	return -EINVAL;
}

 
static inline void
vmw_surface_get_loc(const struct vmw_surface_cache *cache,
		      struct vmw_surface_loc *loc,
		      size_t offset)
{
	const struct vmw_surface_mip *mip = &cache->mip[0];
	const SVGA3dSurfaceDesc *desc = cache->desc;
	u32 layer;
	int i;

	loc->sheet = offset / cache->sheet_bytes;
	offset -= loc->sheet * cache->sheet_bytes;

	layer = offset / cache->mip_chain_bytes;
	offset -= layer * cache->mip_chain_bytes;
	for (i = 0; i < cache->num_mip_levels; ++i, ++mip) {
		if (mip->bytes > offset)
			break;
		offset -= mip->bytes;
	}

	loc->sub_resource = vmw_surface_subres(cache, i, layer);
	loc->z = offset / mip->img_stride;
	offset -= loc->z * mip->img_stride;
	loc->z *= desc->blockSize.depth;
	loc->y = offset / mip->row_stride;
	offset -= loc->y * mip->row_stride;
	loc->y *= desc->blockSize.height;
	loc->x = offset / desc->bytesPerBlock;
	loc->x *= desc->blockSize.width;
}

 
static inline void
vmw_surface_inc_loc(const struct vmw_surface_cache *cache,
		      struct vmw_surface_loc *loc)
{
	const SVGA3dSurfaceDesc *desc = cache->desc;
	u32 mip = loc->sub_resource % cache->num_mip_levels;
	const struct drm_vmw_size *size = &cache->mip[mip].size;

	loc->sub_resource++;
	loc->x += desc->blockSize.width;
	if (loc->x > size->width)
		loc->x = size->width;
	loc->y += desc->blockSize.height;
	if (loc->y > size->height)
		loc->y = size->height;
	loc->z += desc->blockSize.depth;
	if (loc->z > size->depth)
		loc->z = size->depth;
}

 
static inline void
vmw_surface_min_loc(const struct vmw_surface_cache *cache,
		      u32 sub_resource,
		      struct vmw_surface_loc *loc)
{
	loc->sheet = 0;
	loc->sub_resource = sub_resource;
	loc->x = loc->y = loc->z = 0;
}

 
static inline void
vmw_surface_max_loc(const struct vmw_surface_cache *cache,
		      u32 sub_resource,
		      struct vmw_surface_loc *loc)
{
	const struct drm_vmw_size *size;
	u32 mip;

	loc->sheet = 0;
	loc->sub_resource = sub_resource + 1;
	mip = sub_resource % cache->num_mip_levels;
	size = &cache->mip[mip].size;
	loc->x = size->width;
	loc->y = size->height;
	loc->z = size->depth;
}


#endif  
