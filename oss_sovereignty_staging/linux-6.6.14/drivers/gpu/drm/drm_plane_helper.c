 

#include <linux/list.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_encoder.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_rect.h>

#define SUBPIXEL_MASK 0xffff

 

 
static int get_connectors_for_crtc(struct drm_crtc *crtc,
				   struct drm_connector **connector_list,
				   int num_connectors)
{
	struct drm_device *dev = crtc->dev;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	int count = 0;

	 
	WARN_ON(!drm_modeset_is_locked(&dev->mode_config.connection_mutex));

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if (connector->encoder && connector->encoder->crtc == crtc) {
			if (connector_list != NULL && count < num_connectors)
				*(connector_list++) = connector;

			count++;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	return count;
}

static int drm_plane_helper_check_update(struct drm_plane *plane,
					 struct drm_crtc *crtc,
					 struct drm_framebuffer *fb,
					 struct drm_rect *src,
					 struct drm_rect *dst,
					 unsigned int rotation,
					 int min_scale,
					 int max_scale,
					 bool can_position,
					 bool can_update_disabled,
					 bool *visible)
{
	struct drm_plane_state plane_state = {
		.plane = plane,
		.crtc = crtc,
		.fb = fb,
		.src_x = src->x1,
		.src_y = src->y1,
		.src_w = drm_rect_width(src),
		.src_h = drm_rect_height(src),
		.crtc_x = dst->x1,
		.crtc_y = dst->y1,
		.crtc_w = drm_rect_width(dst),
		.crtc_h = drm_rect_height(dst),
		.rotation = rotation,
	};
	struct drm_crtc_state crtc_state = {
		.crtc = crtc,
		.enable = crtc->enabled,
		.mode = crtc->mode,
	};
	int ret;

	ret = drm_atomic_helper_check_plane_state(&plane_state, &crtc_state,
						  min_scale, max_scale,
						  can_position,
						  can_update_disabled);
	if (ret)
		return ret;

	*src = plane_state.src;
	*dst = plane_state.dst;
	*visible = plane_state.visible;

	return 0;
}

 
int drm_plane_helper_update_primary(struct drm_plane *plane, struct drm_crtc *crtc,
				    struct drm_framebuffer *fb,
				    int crtc_x, int crtc_y,
				    unsigned int crtc_w, unsigned int crtc_h,
				    uint32_t src_x, uint32_t src_y,
				    uint32_t src_w, uint32_t src_h,
				    struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_mode_set set = {
		.crtc = crtc,
		.fb = fb,
		.mode = &crtc->mode,
		.x = src_x >> 16,
		.y = src_y >> 16,
	};
	struct drm_rect src = {
		.x1 = src_x,
		.y1 = src_y,
		.x2 = src_x + src_w,
		.y2 = src_y + src_h,
	};
	struct drm_rect dest = {
		.x1 = crtc_x,
		.y1 = crtc_y,
		.x2 = crtc_x + crtc_w,
		.y2 = crtc_y + crtc_h,
	};
	struct drm_device *dev = plane->dev;
	struct drm_connector **connector_list;
	int num_connectors, ret;
	bool visible;

	if (drm_WARN_ON_ONCE(dev, drm_drv_uses_atomic_modeset(dev)))
		return -EINVAL;

	ret = drm_plane_helper_check_update(plane, crtc, fb,
					    &src, &dest,
					    DRM_MODE_ROTATE_0,
					    DRM_PLANE_NO_SCALING,
					    DRM_PLANE_NO_SCALING,
					    false, false, &visible);
	if (ret)
		return ret;

	if (!visible)
		 
		return plane->funcs->disable_plane(plane, ctx);

	 
	num_connectors = get_connectors_for_crtc(crtc, NULL, 0);
	BUG_ON(num_connectors == 0);
	connector_list = kcalloc(num_connectors, sizeof(*connector_list),
				 GFP_KERNEL);
	if (!connector_list)
		return -ENOMEM;
	get_connectors_for_crtc(crtc, connector_list, num_connectors);

	set.connectors = connector_list;
	set.num_connectors = num_connectors;

	 
	ret = crtc->funcs->set_config(&set, ctx);

	kfree(connector_list);
	return ret;
}
EXPORT_SYMBOL(drm_plane_helper_update_primary);

 
int drm_plane_helper_disable_primary(struct drm_plane *plane,
				     struct drm_modeset_acquire_ctx *ctx)
{
	struct drm_device *dev = plane->dev;

	drm_WARN_ON_ONCE(dev, drm_drv_uses_atomic_modeset(dev));

	return -EINVAL;
}
EXPORT_SYMBOL(drm_plane_helper_disable_primary);

 
void drm_plane_helper_destroy(struct drm_plane *plane)
{
	drm_plane_cleanup(plane);
	kfree(plane);
}
EXPORT_SYMBOL(drm_plane_helper_destroy);

 
int drm_plane_helper_atomic_check(struct drm_plane *plane, struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state, plane);
	struct drm_crtc *new_crtc = new_plane_state->crtc;
	struct drm_crtc_state *new_crtc_state = NULL;

	if (new_crtc)
		new_crtc_state = drm_atomic_get_new_crtc_state(state, new_crtc);

	return drm_atomic_helper_check_plane_state(new_plane_state, new_crtc_state,
						   DRM_PLANE_NO_SCALING,
						   DRM_PLANE_NO_SCALING,
						   false, false);
}
EXPORT_SYMBOL(drm_plane_helper_atomic_check);
