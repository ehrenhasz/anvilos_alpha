

#include <linux/dma-resv.h>
#include <linux/dma-fence-chain.h>

#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_simple_kms_helper.h>

#include "drm_internal.h"

 

 

 
int drm_gem_plane_helper_prepare_fb(struct drm_plane *plane,
				    struct drm_plane_state *state)
{
	struct dma_fence *fence = dma_fence_get(state->fence);
	enum dma_resv_usage usage;
	size_t i;
	int ret;

	if (!state->fb)
		return 0;

	 
	usage = fence ? DMA_RESV_USAGE_KERNEL : DMA_RESV_USAGE_WRITE;

	for (i = 0; i < state->fb->format->num_planes; ++i) {
		struct drm_gem_object *obj = drm_gem_fb_get_obj(state->fb, i);
		struct dma_fence *new;

		if (!obj) {
			ret = -EINVAL;
			goto error;
		}

		ret = dma_resv_get_singleton(obj->resv, usage, &new);
		if (ret)
			goto error;

		if (new && fence) {
			struct dma_fence_chain *chain = dma_fence_chain_alloc();

			if (!chain) {
				ret = -ENOMEM;
				goto error;
			}

			dma_fence_chain_init(chain, fence, new, 1);
			fence = &chain->base;

		} else if (new) {
			fence = new;
		}
	}

	dma_fence_put(state->fence);
	state->fence = fence;
	return 0;

error:
	dma_fence_put(fence);
	return ret;
}
EXPORT_SYMBOL_GPL(drm_gem_plane_helper_prepare_fb);

 

 
void
__drm_gem_duplicate_shadow_plane_state(struct drm_plane *plane,
				       struct drm_shadow_plane_state *new_shadow_plane_state)
{
	__drm_atomic_helper_plane_duplicate_state(plane, &new_shadow_plane_state->base);
}
EXPORT_SYMBOL(__drm_gem_duplicate_shadow_plane_state);

 
struct drm_plane_state *
drm_gem_duplicate_shadow_plane_state(struct drm_plane *plane)
{
	struct drm_plane_state *plane_state = plane->state;
	struct drm_shadow_plane_state *new_shadow_plane_state;

	if (!plane_state)
		return NULL;

	new_shadow_plane_state = kzalloc(sizeof(*new_shadow_plane_state), GFP_KERNEL);
	if (!new_shadow_plane_state)
		return NULL;
	__drm_gem_duplicate_shadow_plane_state(plane, new_shadow_plane_state);

	return &new_shadow_plane_state->base;
}
EXPORT_SYMBOL(drm_gem_duplicate_shadow_plane_state);

 
void __drm_gem_destroy_shadow_plane_state(struct drm_shadow_plane_state *shadow_plane_state)
{
	__drm_atomic_helper_plane_destroy_state(&shadow_plane_state->base);
}
EXPORT_SYMBOL(__drm_gem_destroy_shadow_plane_state);

 
void drm_gem_destroy_shadow_plane_state(struct drm_plane *plane,
					struct drm_plane_state *plane_state)
{
	struct drm_shadow_plane_state *shadow_plane_state =
		to_drm_shadow_plane_state(plane_state);

	__drm_gem_destroy_shadow_plane_state(shadow_plane_state);
	kfree(shadow_plane_state);
}
EXPORT_SYMBOL(drm_gem_destroy_shadow_plane_state);

 
void __drm_gem_reset_shadow_plane(struct drm_plane *plane,
				  struct drm_shadow_plane_state *shadow_plane_state)
{
	__drm_atomic_helper_plane_reset(plane, &shadow_plane_state->base);
}
EXPORT_SYMBOL(__drm_gem_reset_shadow_plane);

 
void drm_gem_reset_shadow_plane(struct drm_plane *plane)
{
	struct drm_shadow_plane_state *shadow_plane_state;

	if (plane->state) {
		drm_gem_destroy_shadow_plane_state(plane, plane->state);
		plane->state = NULL;  
	}

	shadow_plane_state = kzalloc(sizeof(*shadow_plane_state), GFP_KERNEL);
	if (!shadow_plane_state)
		return;
	__drm_gem_reset_shadow_plane(plane, shadow_plane_state);
}
EXPORT_SYMBOL(drm_gem_reset_shadow_plane);

 
int drm_gem_begin_shadow_fb_access(struct drm_plane *plane, struct drm_plane_state *plane_state)
{
	struct drm_shadow_plane_state *shadow_plane_state = to_drm_shadow_plane_state(plane_state);
	struct drm_framebuffer *fb = plane_state->fb;

	if (!fb)
		return 0;

	return drm_gem_fb_vmap(fb, shadow_plane_state->map, shadow_plane_state->data);
}
EXPORT_SYMBOL(drm_gem_begin_shadow_fb_access);

 
void drm_gem_end_shadow_fb_access(struct drm_plane *plane, struct drm_plane_state *plane_state)
{
	struct drm_shadow_plane_state *shadow_plane_state = to_drm_shadow_plane_state(plane_state);
	struct drm_framebuffer *fb = plane_state->fb;

	if (!fb)
		return;

	drm_gem_fb_vunmap(fb, shadow_plane_state->map);
}
EXPORT_SYMBOL(drm_gem_end_shadow_fb_access);

 
int drm_gem_simple_kms_begin_shadow_fb_access(struct drm_simple_display_pipe *pipe,
					      struct drm_plane_state *plane_state)
{
	return drm_gem_begin_shadow_fb_access(&pipe->plane, plane_state);
}
EXPORT_SYMBOL(drm_gem_simple_kms_begin_shadow_fb_access);

 
void drm_gem_simple_kms_end_shadow_fb_access(struct drm_simple_display_pipe *pipe,
					     struct drm_plane_state *plane_state)
{
	drm_gem_end_shadow_fb_access(&pipe->plane, plane_state);
}
EXPORT_SYMBOL(drm_gem_simple_kms_end_shadow_fb_access);

 
void drm_gem_simple_kms_reset_shadow_plane(struct drm_simple_display_pipe *pipe)
{
	drm_gem_reset_shadow_plane(&pipe->plane);
}
EXPORT_SYMBOL(drm_gem_simple_kms_reset_shadow_plane);

 
struct drm_plane_state *
drm_gem_simple_kms_duplicate_shadow_plane_state(struct drm_simple_display_pipe *pipe)
{
	return drm_gem_duplicate_shadow_plane_state(&pipe->plane);
}
EXPORT_SYMBOL(drm_gem_simple_kms_duplicate_shadow_plane_state);

 
void drm_gem_simple_kms_destroy_shadow_plane_state(struct drm_simple_display_pipe *pipe,
						   struct drm_plane_state *plane_state)
{
	drm_gem_destroy_shadow_plane_state(&pipe->plane, plane_state);
}
EXPORT_SYMBOL(drm_gem_simple_kms_destroy_shadow_plane_state);
