
 

#include <drm/drm_damage_helper.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_plane.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>

 

 
struct drm_gem_dma_object *drm_fb_dma_get_gem_obj(struct drm_framebuffer *fb,
						  unsigned int plane)
{
	struct drm_gem_object *gem;

	gem = drm_gem_fb_get_obj(fb, plane);
	if (!gem)
		return NULL;

	return to_drm_gem_dma_obj(gem);
}
EXPORT_SYMBOL_GPL(drm_fb_dma_get_gem_obj);

 
dma_addr_t drm_fb_dma_get_gem_addr(struct drm_framebuffer *fb,
				   struct drm_plane_state *state,
				   unsigned int plane)
{
	struct drm_gem_dma_object *obj;
	dma_addr_t dma_addr;
	u8 h_div = 1, v_div = 1;
	u32 block_w = drm_format_info_block_width(fb->format, plane);
	u32 block_h = drm_format_info_block_height(fb->format, plane);
	u32 block_size = fb->format->char_per_block[plane];
	u32 sample_x;
	u32 sample_y;
	u32 block_start_y;
	u32 num_hblocks;

	obj = drm_fb_dma_get_gem_obj(fb, plane);
	if (!obj)
		return 0;

	dma_addr = obj->dma_addr + fb->offsets[plane];

	if (plane > 0) {
		h_div = fb->format->hsub;
		v_div = fb->format->vsub;
	}

	sample_x = (state->src_x >> 16) / h_div;
	sample_y = (state->src_y >> 16) / v_div;
	block_start_y = (sample_y / block_h) * block_h;
	num_hblocks = sample_x / block_w;

	dma_addr += fb->pitches[plane] * block_start_y;
	dma_addr += block_size * num_hblocks;

	return dma_addr;
}
EXPORT_SYMBOL_GPL(drm_fb_dma_get_gem_addr);

 
void drm_fb_dma_sync_non_coherent(struct drm_device *drm,
				  struct drm_plane_state *old_state,
				  struct drm_plane_state *state)
{
	const struct drm_format_info *finfo = state->fb->format;
	struct drm_atomic_helper_damage_iter iter;
	const struct drm_gem_dma_object *dma_obj;
	unsigned int offset, i;
	struct drm_rect clip;
	dma_addr_t daddr;
	size_t nb_bytes;

	for (i = 0; i < finfo->num_planes; i++) {
		dma_obj = drm_fb_dma_get_gem_obj(state->fb, i);
		if (!dma_obj->map_noncoherent)
			continue;

		daddr = drm_fb_dma_get_gem_addr(state->fb, state, i);
		drm_atomic_helper_damage_iter_init(&iter, old_state, state);

		drm_atomic_for_each_plane_damage(&iter, &clip) {
			 
			offset = clip.y1 * state->fb->pitches[i];

			nb_bytes = (clip.y2 - clip.y1) * state->fb->pitches[i];
			dma_sync_single_for_device(drm->dev, daddr + offset,
						   nb_bytes, DMA_TO_DEVICE);
		}
	}
}
EXPORT_SYMBOL_GPL(drm_fb_dma_sync_non_coherent);
