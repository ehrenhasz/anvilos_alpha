
 

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>

#include "omap_dmm_tiler.h"
#include "omap_drv.h"

 
static const char * const overlay_id_to_name[] = {
	[OMAP_DSS_GFX] = "gfx",
	[OMAP_DSS_VIDEO1] = "vid1",
	[OMAP_DSS_VIDEO2] = "vid2",
	[OMAP_DSS_VIDEO3] = "vid3",
};

 
static struct omap_hw_overlay *
omap_plane_find_free_overlay(struct drm_device *dev, struct drm_plane *hwoverlay_to_plane[],
			     u32 caps, u32 fourcc)
{
	struct omap_drm_private *priv = dev->dev_private;
	int i;

	DBG("caps: %x fourcc: %x", caps, fourcc);

	for (i = 0; i < priv->num_ovls; i++) {
		struct omap_hw_overlay *cur = priv->overlays[i];

		DBG("%d: id: %d cur->caps: %x",
		    cur->idx, cur->id, cur->caps);

		 
		if (hwoverlay_to_plane[cur->idx])
			continue;

		 
		if (caps & ~cur->caps)
			continue;

		 
		if (!dispc_ovl_color_mode_supported(priv->dispc,
						    cur->id, fourcc))
			continue;

		return cur;
	}

	DBG("no match");
	return NULL;
}

 
int omap_overlay_assign(struct drm_atomic_state *s, struct drm_plane *plane,
			u32 caps, u32 fourcc, struct omap_hw_overlay **overlay,
			struct omap_hw_overlay **r_overlay)
{
	 
	struct omap_global_state *state = omap_get_global_state(s);
	struct drm_plane **overlay_map = state->hwoverlay_to_plane;
	struct omap_hw_overlay *ovl, *r_ovl;

	ovl = omap_plane_find_free_overlay(s->dev, overlay_map, caps, fourcc);
	if (!ovl)
		return -ENOMEM;

	overlay_map[ovl->idx] = plane;
	*overlay = ovl;

	if (r_overlay) {
		r_ovl = omap_plane_find_free_overlay(s->dev, overlay_map,
						     caps, fourcc);
		if (!r_ovl) {
			overlay_map[ovl->idx] = NULL;
			*overlay = NULL;
			return -ENOMEM;
		}

		overlay_map[r_ovl->idx] = plane;
		*r_overlay = r_ovl;
	}

	DBG("%s: assign to plane %s caps %x", ovl->name, plane->name, caps);

	if (r_overlay) {
		DBG("%s: assign to right of plane %s caps %x",
		    r_ovl->name, plane->name, caps);
	}

	return 0;
}

 
void omap_overlay_release(struct drm_atomic_state *s, struct omap_hw_overlay *overlay)
{
	 
	struct omap_global_state *state = omap_get_global_state(s);
	struct drm_plane **overlay_map = state->hwoverlay_to_plane;

	if (!overlay)
		return;

	if (WARN_ON(!overlay_map[overlay->idx]))
		return;

	DBG("%s: release from plane %s", overlay->name, overlay_map[overlay->idx]->name);

	overlay_map[overlay->idx] = NULL;
}

 
void omap_overlay_update_state(struct omap_drm_private *priv,
			       struct omap_hw_overlay *overlay)
{
	struct omap_global_state *state = omap_get_existing_global_state(priv);
	struct drm_plane **overlay_map = state->hwoverlay_to_plane;

	 
	if (!overlay_map[overlay->idx]) {
		DBG("%s: disabled", overlay->name);

		 
		dispc_ovl_enable(priv->dispc, overlay->id, false);
	}
}

static void omap_overlay_destroy(struct omap_hw_overlay *overlay)
{
	kfree(overlay);
}

static struct omap_hw_overlay *omap_overlay_init(enum omap_plane_id overlay_id,
						 enum omap_overlay_caps caps)
{
	struct omap_hw_overlay *overlay;

	overlay = kzalloc(sizeof(*overlay), GFP_KERNEL);
	if (!overlay)
		return ERR_PTR(-ENOMEM);

	overlay->name = overlay_id_to_name[overlay_id];
	overlay->id = overlay_id;
	overlay->caps = caps;

	return overlay;
}

int omap_hwoverlays_init(struct omap_drm_private *priv)
{
	static const enum omap_plane_id hw_plane_ids[] = {
			OMAP_DSS_GFX, OMAP_DSS_VIDEO1,
			OMAP_DSS_VIDEO2, OMAP_DSS_VIDEO3,
	};
	u32 num_overlays = dispc_get_num_ovls(priv->dispc);
	enum omap_overlay_caps caps;
	int i, ret;

	for (i = 0; i < num_overlays; i++) {
		struct omap_hw_overlay *overlay;

		caps = dispc_ovl_get_caps(priv->dispc, hw_plane_ids[i]);
		overlay = omap_overlay_init(hw_plane_ids[i], caps);
		if (IS_ERR(overlay)) {
			ret = PTR_ERR(overlay);
			dev_err(priv->dev, "failed to construct overlay for %s (%d)\n",
				overlay_id_to_name[i], ret);
			omap_hwoverlays_destroy(priv);
			return ret;
		}
		overlay->idx = priv->num_ovls;
		priv->overlays[priv->num_ovls++] = overlay;
	}

	return 0;
}

void omap_hwoverlays_destroy(struct omap_drm_private *priv)
{
	int i;

	for (i = 0; i < priv->num_ovls; i++) {
		omap_overlay_destroy(priv->overlays[i]);
		priv->overlays[i] = NULL;
	}

	priv->num_ovls = 0;
}
