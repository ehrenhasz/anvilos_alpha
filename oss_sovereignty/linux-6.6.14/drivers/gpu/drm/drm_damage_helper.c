
 

#include <drm/drm_atomic.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_device.h>
#include <drm/drm_framebuffer.h>

static void convert_clip_rect_to_rect(const struct drm_clip_rect *src,
				      struct drm_mode_rect *dest,
				      uint32_t num_clips, uint32_t src_inc)
{
	while (num_clips > 0) {
		dest->x1 = src->x1;
		dest->y1 = src->y1;
		dest->x2 = src->x2;
		dest->y2 = src->y2;
		src += src_inc;
		dest++;
		num_clips--;
	}
}

 
void drm_atomic_helper_check_plane_damage(struct drm_atomic_state *state,
					  struct drm_plane_state *plane_state)
{
	struct drm_crtc_state *crtc_state;

	if (plane_state->crtc) {
		crtc_state = drm_atomic_get_new_crtc_state(state,
							   plane_state->crtc);

		if (WARN_ON(!crtc_state))
			return;

		if (drm_atomic_crtc_needs_modeset(crtc_state)) {
			drm_property_blob_put(plane_state->fb_damage_clips);
			plane_state->fb_damage_clips = NULL;
		}
	}
}
EXPORT_SYMBOL(drm_atomic_helper_check_plane_damage);

 
int drm_atomic_helper_dirtyfb(struct drm_framebuffer *fb,
			      struct drm_file *file_priv, unsigned int flags,
			      unsigned int color, struct drm_clip_rect *clips,
			      unsigned int num_clips)
{
	struct drm_modeset_acquire_ctx ctx;
	struct drm_property_blob *damage = NULL;
	struct drm_mode_rect *rects = NULL;
	struct drm_atomic_state *state;
	struct drm_plane *plane;
	int ret = 0;

	 
	drm_modeset_acquire_init(&ctx,
		file_priv ? DRM_MODESET_ACQUIRE_INTERRUPTIBLE : 0);

	state = drm_atomic_state_alloc(fb->dev);
	if (!state) {
		ret = -ENOMEM;
		goto out_drop_locks;
	}
	state->acquire_ctx = &ctx;

	if (clips) {
		uint32_t inc = 1;

		if (flags & DRM_MODE_FB_DIRTY_ANNOTATE_COPY) {
			inc = 2;
			num_clips /= 2;
		}

		rects = kcalloc(num_clips, sizeof(*rects), GFP_KERNEL);
		if (!rects) {
			ret = -ENOMEM;
			goto out;
		}

		convert_clip_rect_to_rect(clips, rects, num_clips, inc);
		damage = drm_property_create_blob(fb->dev,
						  num_clips * sizeof(*rects),
						  rects);
		if (IS_ERR(damage)) {
			ret = PTR_ERR(damage);
			damage = NULL;
			goto out;
		}
	}

retry:
	drm_for_each_plane(plane, fb->dev) {
		struct drm_plane_state *plane_state;

		ret = drm_modeset_lock(&plane->mutex, state->acquire_ctx);
		if (ret)
			goto out;

		if (plane->state->fb != fb) {
			drm_modeset_unlock(&plane->mutex);
			continue;
		}

		plane_state = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			ret = PTR_ERR(plane_state);
			goto out;
		}

		drm_property_replace_blob(&plane_state->fb_damage_clips,
					  damage);
	}

	ret = drm_atomic_commit(state);

out:
	if (ret == -EDEADLK) {
		drm_atomic_state_clear(state);
		ret = drm_modeset_backoff(&ctx);
		if (!ret)
			goto retry;
	}

	drm_property_blob_put(damage);
	kfree(rects);
	drm_atomic_state_put(state);

out_drop_locks:
	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	return ret;

}
EXPORT_SYMBOL(drm_atomic_helper_dirtyfb);

 
void
drm_atomic_helper_damage_iter_init(struct drm_atomic_helper_damage_iter *iter,
				   const struct drm_plane_state *old_state,
				   const struct drm_plane_state *state)
{
	struct drm_rect src;
	memset(iter, 0, sizeof(*iter));

	if (!state || !state->crtc || !state->fb || !state->visible)
		return;

	iter->clips = (struct drm_rect *)drm_plane_get_damage_clips(state);
	iter->num_clips = drm_plane_get_damage_clips_count(state);

	 
	src = drm_plane_state_src(state);

	iter->plane_src.x1 = src.x1 >> 16;
	iter->plane_src.y1 = src.y1 >> 16;
	iter->plane_src.x2 = (src.x2 >> 16) + !!(src.x2 & 0xFFFF);
	iter->plane_src.y2 = (src.y2 >> 16) + !!(src.y2 & 0xFFFF);

	if (!iter->clips || !drm_rect_equals(&state->src, &old_state->src)) {
		iter->clips = NULL;
		iter->num_clips = 0;
		iter->full_update = true;
	}
}
EXPORT_SYMBOL(drm_atomic_helper_damage_iter_init);

 
bool
drm_atomic_helper_damage_iter_next(struct drm_atomic_helper_damage_iter *iter,
				   struct drm_rect *rect)
{
	bool ret = false;

	if (iter->full_update) {
		*rect = iter->plane_src;
		iter->full_update = false;
		return true;
	}

	while (iter->curr_clip < iter->num_clips) {
		*rect = iter->clips[iter->curr_clip];
		iter->curr_clip++;

		if (drm_rect_intersect(rect, &iter->plane_src)) {
			ret = true;
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(drm_atomic_helper_damage_iter_next);

 
bool drm_atomic_helper_damage_merged(const struct drm_plane_state *old_state,
				     struct drm_plane_state *state,
				     struct drm_rect *rect)
{
	struct drm_atomic_helper_damage_iter iter;
	struct drm_rect clip;
	bool valid = false;

	rect->x1 = INT_MAX;
	rect->y1 = INT_MAX;
	rect->x2 = 0;
	rect->y2 = 0;

	drm_atomic_helper_damage_iter_init(&iter, old_state, state);
	drm_atomic_for_each_plane_damage(&iter, &clip) {
		rect->x1 = min(rect->x1, clip.x1);
		rect->y1 = min(rect->y1, clip.y1);
		rect->x2 = max(rect->x2, clip.x2);
		rect->y2 = max(rect->y2, clip.y2);
		valid = true;
	}

	return valid;
}
EXPORT_SYMBOL(drm_atomic_helper_damage_merged);
