
 

 

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_blend.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_atomic_helper.h>

#include "uapi/drm/vc4_drm.h"

#include "vc4_drv.h"
#include "vc4_regs.h"

static const struct hvs_format {
	u32 drm;  
	u32 hvs;  
	u32 pixel_order;
	u32 pixel_order_hvs5;
	bool hvs5_only;
} hvs_formats[] = {
	{
		.drm = DRM_FORMAT_XRGB8888,
		.hvs = HVS_PIXEL_FORMAT_RGBA8888,
		.pixel_order = HVS_PIXEL_ORDER_ABGR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ARGB,
	},
	{
		.drm = DRM_FORMAT_ARGB8888,
		.hvs = HVS_PIXEL_FORMAT_RGBA8888,
		.pixel_order = HVS_PIXEL_ORDER_ABGR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ARGB,
	},
	{
		.drm = DRM_FORMAT_ABGR8888,
		.hvs = HVS_PIXEL_FORMAT_RGBA8888,
		.pixel_order = HVS_PIXEL_ORDER_ARGB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ABGR,
	},
	{
		.drm = DRM_FORMAT_XBGR8888,
		.hvs = HVS_PIXEL_FORMAT_RGBA8888,
		.pixel_order = HVS_PIXEL_ORDER_ARGB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ABGR,
	},
	{
		.drm = DRM_FORMAT_RGB565,
		.hvs = HVS_PIXEL_FORMAT_RGB565,
		.pixel_order = HVS_PIXEL_ORDER_XRGB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XRGB,
	},
	{
		.drm = DRM_FORMAT_BGR565,
		.hvs = HVS_PIXEL_FORMAT_RGB565,
		.pixel_order = HVS_PIXEL_ORDER_XBGR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XBGR,
	},
	{
		.drm = DRM_FORMAT_ARGB1555,
		.hvs = HVS_PIXEL_FORMAT_RGBA5551,
		.pixel_order = HVS_PIXEL_ORDER_ABGR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ARGB,
	},
	{
		.drm = DRM_FORMAT_XRGB1555,
		.hvs = HVS_PIXEL_FORMAT_RGBA5551,
		.pixel_order = HVS_PIXEL_ORDER_ABGR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ARGB,
	},
	{
		.drm = DRM_FORMAT_RGB888,
		.hvs = HVS_PIXEL_FORMAT_RGB888,
		.pixel_order = HVS_PIXEL_ORDER_XRGB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XRGB,
	},
	{
		.drm = DRM_FORMAT_BGR888,
		.hvs = HVS_PIXEL_FORMAT_RGB888,
		.pixel_order = HVS_PIXEL_ORDER_XBGR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XBGR,
	},
	{
		.drm = DRM_FORMAT_YUV422,
		.hvs = HVS_PIXEL_FORMAT_YCBCR_YUV422_3PLANE,
		.pixel_order = HVS_PIXEL_ORDER_XYCBCR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XYCBCR,
	},
	{
		.drm = DRM_FORMAT_YVU422,
		.hvs = HVS_PIXEL_FORMAT_YCBCR_YUV422_3PLANE,
		.pixel_order = HVS_PIXEL_ORDER_XYCRCB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XYCRCB,
	},
	{
		.drm = DRM_FORMAT_YUV420,
		.hvs = HVS_PIXEL_FORMAT_YCBCR_YUV420_3PLANE,
		.pixel_order = HVS_PIXEL_ORDER_XYCBCR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XYCBCR,
	},
	{
		.drm = DRM_FORMAT_YVU420,
		.hvs = HVS_PIXEL_FORMAT_YCBCR_YUV420_3PLANE,
		.pixel_order = HVS_PIXEL_ORDER_XYCRCB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XYCRCB,
	},
	{
		.drm = DRM_FORMAT_NV12,
		.hvs = HVS_PIXEL_FORMAT_YCBCR_YUV420_2PLANE,
		.pixel_order = HVS_PIXEL_ORDER_XYCBCR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XYCBCR,
	},
	{
		.drm = DRM_FORMAT_NV21,
		.hvs = HVS_PIXEL_FORMAT_YCBCR_YUV420_2PLANE,
		.pixel_order = HVS_PIXEL_ORDER_XYCRCB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XYCRCB,
	},
	{
		.drm = DRM_FORMAT_NV16,
		.hvs = HVS_PIXEL_FORMAT_YCBCR_YUV422_2PLANE,
		.pixel_order = HVS_PIXEL_ORDER_XYCBCR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XYCBCR,
	},
	{
		.drm = DRM_FORMAT_NV61,
		.hvs = HVS_PIXEL_FORMAT_YCBCR_YUV422_2PLANE,
		.pixel_order = HVS_PIXEL_ORDER_XYCRCB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XYCRCB,
	},
	{
		.drm = DRM_FORMAT_P030,
		.hvs = HVS_PIXEL_FORMAT_YCBCR_10BIT,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_XYCBCR,
		.hvs5_only = true,
	},
	{
		.drm = DRM_FORMAT_XRGB2101010,
		.hvs = HVS_PIXEL_FORMAT_RGBA1010102,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ARGB,
		.hvs5_only = true,
	},
	{
		.drm = DRM_FORMAT_ARGB2101010,
		.hvs = HVS_PIXEL_FORMAT_RGBA1010102,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ARGB,
		.hvs5_only = true,
	},
	{
		.drm = DRM_FORMAT_ABGR2101010,
		.hvs = HVS_PIXEL_FORMAT_RGBA1010102,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ABGR,
		.hvs5_only = true,
	},
	{
		.drm = DRM_FORMAT_XBGR2101010,
		.hvs = HVS_PIXEL_FORMAT_RGBA1010102,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ABGR,
		.hvs5_only = true,
	},
	{
		.drm = DRM_FORMAT_RGB332,
		.hvs = HVS_PIXEL_FORMAT_RGB332,
		.pixel_order = HVS_PIXEL_ORDER_ARGB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ARGB,
	},
	{
		.drm = DRM_FORMAT_BGR233,
		.hvs = HVS_PIXEL_FORMAT_RGB332,
		.pixel_order = HVS_PIXEL_ORDER_ABGR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ABGR,
	},
	{
		.drm = DRM_FORMAT_XRGB4444,
		.hvs = HVS_PIXEL_FORMAT_RGBA4444,
		.pixel_order = HVS_PIXEL_ORDER_ABGR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ARGB,
	},
	{
		.drm = DRM_FORMAT_ARGB4444,
		.hvs = HVS_PIXEL_FORMAT_RGBA4444,
		.pixel_order = HVS_PIXEL_ORDER_ABGR,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ARGB,
	},
	{
		.drm = DRM_FORMAT_XBGR4444,
		.hvs = HVS_PIXEL_FORMAT_RGBA4444,
		.pixel_order = HVS_PIXEL_ORDER_ARGB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ABGR,
	},
	{
		.drm = DRM_FORMAT_ABGR4444,
		.hvs = HVS_PIXEL_FORMAT_RGBA4444,
		.pixel_order = HVS_PIXEL_ORDER_ARGB,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_ABGR,
	},
	{
		.drm = DRM_FORMAT_BGRX4444,
		.hvs = HVS_PIXEL_FORMAT_RGBA4444,
		.pixel_order = HVS_PIXEL_ORDER_RGBA,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_BGRA,
	},
	{
		.drm = DRM_FORMAT_BGRA4444,
		.hvs = HVS_PIXEL_FORMAT_RGBA4444,
		.pixel_order = HVS_PIXEL_ORDER_RGBA,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_BGRA,
	},
	{
		.drm = DRM_FORMAT_RGBX4444,
		.hvs = HVS_PIXEL_FORMAT_RGBA4444,
		.pixel_order = HVS_PIXEL_ORDER_BGRA,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_RGBA,
	},
	{
		.drm = DRM_FORMAT_RGBA4444,
		.hvs = HVS_PIXEL_FORMAT_RGBA4444,
		.pixel_order = HVS_PIXEL_ORDER_BGRA,
		.pixel_order_hvs5 = HVS_PIXEL_ORDER_RGBA,
	},
};

static const struct hvs_format *vc4_get_hvs_format(u32 drm_format)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(hvs_formats); i++) {
		if (hvs_formats[i].drm == drm_format)
			return &hvs_formats[i];
	}

	return NULL;
}

static enum vc4_scaling_mode vc4_get_scaling_mode(u32 src, u32 dst)
{
	if (dst == src)
		return VC4_SCALING_NONE;
	if (3 * dst >= 2 * src)
		return VC4_SCALING_PPF;
	else
		return VC4_SCALING_TPZ;
}

static bool plane_enabled(struct drm_plane_state *state)
{
	return state->fb && !WARN_ON(!state->crtc);
}

static struct drm_plane_state *vc4_plane_duplicate_state(struct drm_plane *plane)
{
	struct vc4_plane_state *vc4_state;

	if (WARN_ON(!plane->state))
		return NULL;

	vc4_state = kmemdup(plane->state, sizeof(*vc4_state), GFP_KERNEL);
	if (!vc4_state)
		return NULL;

	memset(&vc4_state->lbm, 0, sizeof(vc4_state->lbm));
	vc4_state->dlist_initialized = 0;

	__drm_atomic_helper_plane_duplicate_state(plane, &vc4_state->base);

	if (vc4_state->dlist) {
		vc4_state->dlist = kmemdup(vc4_state->dlist,
					   vc4_state->dlist_count * 4,
					   GFP_KERNEL);
		if (!vc4_state->dlist) {
			kfree(vc4_state);
			return NULL;
		}
		vc4_state->dlist_size = vc4_state->dlist_count;
	}

	return &vc4_state->base;
}

static void vc4_plane_destroy_state(struct drm_plane *plane,
				    struct drm_plane_state *state)
{
	struct vc4_dev *vc4 = to_vc4_dev(plane->dev);
	struct vc4_plane_state *vc4_state = to_vc4_plane_state(state);

	if (drm_mm_node_allocated(&vc4_state->lbm)) {
		unsigned long irqflags;

		spin_lock_irqsave(&vc4->hvs->mm_lock, irqflags);
		drm_mm_remove_node(&vc4_state->lbm);
		spin_unlock_irqrestore(&vc4->hvs->mm_lock, irqflags);
	}

	kfree(vc4_state->dlist);
	__drm_atomic_helper_plane_destroy_state(&vc4_state->base);
	kfree(state);
}

 
static void vc4_plane_reset(struct drm_plane *plane)
{
	struct vc4_plane_state *vc4_state;

	WARN_ON(plane->state);

	vc4_state = kzalloc(sizeof(*vc4_state), GFP_KERNEL);
	if (!vc4_state)
		return;

	__drm_atomic_helper_plane_reset(plane, &vc4_state->base);
}

static void vc4_dlist_counter_increment(struct vc4_plane_state *vc4_state)
{
	if (vc4_state->dlist_count == vc4_state->dlist_size) {
		u32 new_size = max(4u, vc4_state->dlist_count * 2);
		u32 *new_dlist = kmalloc_array(new_size, 4, GFP_KERNEL);

		if (!new_dlist)
			return;
		memcpy(new_dlist, vc4_state->dlist, vc4_state->dlist_count * 4);

		kfree(vc4_state->dlist);
		vc4_state->dlist = new_dlist;
		vc4_state->dlist_size = new_size;
	}

	vc4_state->dlist_count++;
}

static void vc4_dlist_write(struct vc4_plane_state *vc4_state, u32 val)
{
	unsigned int idx = vc4_state->dlist_count;

	vc4_dlist_counter_increment(vc4_state);
	vc4_state->dlist[idx] = val;
}

 
static u32 vc4_get_scl_field(struct drm_plane_state *state, int plane)
{
	struct vc4_plane_state *vc4_state = to_vc4_plane_state(state);

	switch (vc4_state->x_scaling[plane] << 2 | vc4_state->y_scaling[plane]) {
	case VC4_SCALING_PPF << 2 | VC4_SCALING_PPF:
		return SCALER_CTL0_SCL_H_PPF_V_PPF;
	case VC4_SCALING_TPZ << 2 | VC4_SCALING_PPF:
		return SCALER_CTL0_SCL_H_TPZ_V_PPF;
	case VC4_SCALING_PPF << 2 | VC4_SCALING_TPZ:
		return SCALER_CTL0_SCL_H_PPF_V_TPZ;
	case VC4_SCALING_TPZ << 2 | VC4_SCALING_TPZ:
		return SCALER_CTL0_SCL_H_TPZ_V_TPZ;
	case VC4_SCALING_PPF << 2 | VC4_SCALING_NONE:
		return SCALER_CTL0_SCL_H_PPF_V_NONE;
	case VC4_SCALING_NONE << 2 | VC4_SCALING_PPF:
		return SCALER_CTL0_SCL_H_NONE_V_PPF;
	case VC4_SCALING_NONE << 2 | VC4_SCALING_TPZ:
		return SCALER_CTL0_SCL_H_NONE_V_TPZ;
	case VC4_SCALING_TPZ << 2 | VC4_SCALING_NONE:
		return SCALER_CTL0_SCL_H_TPZ_V_NONE;
	default:
	case VC4_SCALING_NONE << 2 | VC4_SCALING_NONE:
		 
		return 0;
	}
}

static int vc4_plane_margins_adj(struct drm_plane_state *pstate)
{
	struct vc4_plane_state *vc4_pstate = to_vc4_plane_state(pstate);
	unsigned int left, right, top, bottom, adjhdisplay, adjvdisplay;
	struct drm_crtc_state *crtc_state;

	crtc_state = drm_atomic_get_new_crtc_state(pstate->state,
						   pstate->crtc);

	vc4_crtc_get_margins(crtc_state, &left, &right, &top, &bottom);
	if (!left && !right && !top && !bottom)
		return 0;

	if (left + right >= crtc_state->mode.hdisplay ||
	    top + bottom >= crtc_state->mode.vdisplay)
		return -EINVAL;

	adjhdisplay = crtc_state->mode.hdisplay - (left + right);
	vc4_pstate->crtc_x = DIV_ROUND_CLOSEST(vc4_pstate->crtc_x *
					       adjhdisplay,
					       crtc_state->mode.hdisplay);
	vc4_pstate->crtc_x += left;
	if (vc4_pstate->crtc_x > crtc_state->mode.hdisplay - right)
		vc4_pstate->crtc_x = crtc_state->mode.hdisplay - right;

	adjvdisplay = crtc_state->mode.vdisplay - (top + bottom);
	vc4_pstate->crtc_y = DIV_ROUND_CLOSEST(vc4_pstate->crtc_y *
					       adjvdisplay,
					       crtc_state->mode.vdisplay);
	vc4_pstate->crtc_y += top;
	if (vc4_pstate->crtc_y > crtc_state->mode.vdisplay - bottom)
		vc4_pstate->crtc_y = crtc_state->mode.vdisplay - bottom;

	vc4_pstate->crtc_w = DIV_ROUND_CLOSEST(vc4_pstate->crtc_w *
					       adjhdisplay,
					       crtc_state->mode.hdisplay);
	vc4_pstate->crtc_h = DIV_ROUND_CLOSEST(vc4_pstate->crtc_h *
					       adjvdisplay,
					       crtc_state->mode.vdisplay);

	if (!vc4_pstate->crtc_w || !vc4_pstate->crtc_h)
		return -EINVAL;

	return 0;
}

static int vc4_plane_setup_clipping_and_scaling(struct drm_plane_state *state)
{
	struct vc4_plane_state *vc4_state = to_vc4_plane_state(state);
	struct drm_framebuffer *fb = state->fb;
	struct drm_gem_dma_object *bo;
	int num_planes = fb->format->num_planes;
	struct drm_crtc_state *crtc_state;
	u32 h_subsample = fb->format->hsub;
	u32 v_subsample = fb->format->vsub;
	int i, ret;

	crtc_state = drm_atomic_get_existing_crtc_state(state->state,
							state->crtc);
	if (!crtc_state) {
		DRM_DEBUG_KMS("Invalid crtc state\n");
		return -EINVAL;
	}

	ret = drm_atomic_helper_check_plane_state(state, crtc_state, 1,
						  INT_MAX, true, true);
	if (ret)
		return ret;

	for (i = 0; i < num_planes; i++) {
		bo = drm_fb_dma_get_gem_obj(fb, i);
		vc4_state->offsets[i] = bo->dma_addr + fb->offsets[i];
	}

	 
	vc4_state->src_x = DIV_ROUND_CLOSEST(state->src.x1, 1 << 16);
	vc4_state->src_y = DIV_ROUND_CLOSEST(state->src.y1, 1 << 16);
	vc4_state->src_w[0] = DIV_ROUND_CLOSEST(state->src.x2, 1 << 16) - vc4_state->src_x;
	vc4_state->src_h[0] = DIV_ROUND_CLOSEST(state->src.y2, 1 << 16) - vc4_state->src_y;

	vc4_state->crtc_x = state->dst.x1;
	vc4_state->crtc_y = state->dst.y1;
	vc4_state->crtc_w = state->dst.x2 - state->dst.x1;
	vc4_state->crtc_h = state->dst.y2 - state->dst.y1;

	ret = vc4_plane_margins_adj(state);
	if (ret)
		return ret;

	vc4_state->x_scaling[0] = vc4_get_scaling_mode(vc4_state->src_w[0],
						       vc4_state->crtc_w);
	vc4_state->y_scaling[0] = vc4_get_scaling_mode(vc4_state->src_h[0],
						       vc4_state->crtc_h);

	vc4_state->is_unity = (vc4_state->x_scaling[0] == VC4_SCALING_NONE &&
			       vc4_state->y_scaling[0] == VC4_SCALING_NONE);

	if (num_planes > 1) {
		vc4_state->is_yuv = true;

		vc4_state->src_w[1] = vc4_state->src_w[0] / h_subsample;
		vc4_state->src_h[1] = vc4_state->src_h[0] / v_subsample;

		vc4_state->x_scaling[1] =
			vc4_get_scaling_mode(vc4_state->src_w[1],
					     vc4_state->crtc_w);
		vc4_state->y_scaling[1] =
			vc4_get_scaling_mode(vc4_state->src_h[1],
					     vc4_state->crtc_h);

		 
		if (vc4_state->x_scaling[1] == VC4_SCALING_NONE)
			vc4_state->x_scaling[1] = VC4_SCALING_PPF;
	} else {
		vc4_state->is_yuv = false;
		vc4_state->x_scaling[1] = VC4_SCALING_NONE;
		vc4_state->y_scaling[1] = VC4_SCALING_NONE;
	}

	return 0;
}

static void vc4_write_tpz(struct vc4_plane_state *vc4_state, u32 src, u32 dst)
{
	u32 scale, recip;

	scale = (1 << 16) * src / dst;

	 
	recip = ~0 / scale;

	vc4_dlist_write(vc4_state,
			VC4_SET_FIELD(scale, SCALER_TPZ0_SCALE) |
			VC4_SET_FIELD(0, SCALER_TPZ0_IPHASE));
	vc4_dlist_write(vc4_state,
			VC4_SET_FIELD(recip, SCALER_TPZ1_RECIP));
}

static void vc4_write_ppf(struct vc4_plane_state *vc4_state, u32 src, u32 dst)
{
	u32 scale = (1 << 16) * src / dst;

	vc4_dlist_write(vc4_state,
			SCALER_PPF_AGC |
			VC4_SET_FIELD(scale, SCALER_PPF_SCALE) |
			VC4_SET_FIELD(0, SCALER_PPF_IPHASE));
}

static u32 vc4_lbm_size(struct drm_plane_state *state)
{
	struct vc4_plane_state *vc4_state = to_vc4_plane_state(state);
	struct vc4_dev *vc4 = to_vc4_dev(state->plane->dev);
	u32 pix_per_line;
	u32 lbm;

	 
	if (vc4_state->y_scaling[0] == VC4_SCALING_NONE &&
	    vc4_state->y_scaling[1] == VC4_SCALING_NONE)
		return 0;

	 
	if (vc4_state->x_scaling[0] == VC4_SCALING_TPZ)
		pix_per_line = vc4_state->crtc_w;
	else
		pix_per_line = vc4_state->src_w[0];

	if (!vc4_state->is_yuv) {
		if (vc4_state->y_scaling[0] == VC4_SCALING_TPZ)
			lbm = pix_per_line * 8;
		else {
			 
			lbm = pix_per_line * 16;
		}
	} else {
		 
		lbm = pix_per_line * 16;
	}

	 
	lbm = roundup(lbm, vc4->is_vc5 ? 128 : 64);

	 
	lbm /= vc4->is_vc5 ? 4 : 2;

	return lbm;
}

static void vc4_write_scaling_parameters(struct drm_plane_state *state,
					 int channel)
{
	struct vc4_plane_state *vc4_state = to_vc4_plane_state(state);

	 
	if (vc4_state->x_scaling[channel] == VC4_SCALING_PPF) {
		vc4_write_ppf(vc4_state,
			      vc4_state->src_w[channel], vc4_state->crtc_w);
	}

	 
	if (vc4_state->y_scaling[channel] == VC4_SCALING_PPF) {
		vc4_write_ppf(vc4_state,
			      vc4_state->src_h[channel], vc4_state->crtc_h);
		vc4_dlist_write(vc4_state, 0xc0c0c0c0);
	}

	 
	if (vc4_state->x_scaling[channel] == VC4_SCALING_TPZ) {
		vc4_write_tpz(vc4_state,
			      vc4_state->src_w[channel], vc4_state->crtc_w);
	}

	 
	if (vc4_state->y_scaling[channel] == VC4_SCALING_TPZ) {
		vc4_write_tpz(vc4_state,
			      vc4_state->src_h[channel], vc4_state->crtc_h);
		vc4_dlist_write(vc4_state, 0xc0c0c0c0);
	}
}

static void vc4_plane_calc_load(struct drm_plane_state *state)
{
	unsigned int hvs_load_shift, vrefresh, i;
	struct drm_framebuffer *fb = state->fb;
	struct vc4_plane_state *vc4_state;
	struct drm_crtc_state *crtc_state;
	unsigned int vscale_factor;

	vc4_state = to_vc4_plane_state(state);
	crtc_state = drm_atomic_get_existing_crtc_state(state->state,
							state->crtc);
	vrefresh = drm_mode_vrefresh(&crtc_state->adjusted_mode);

	 
	if (vc4_state->x_scaling[0] != VC4_SCALING_NONE ||
	    vc4_state->x_scaling[1] != VC4_SCALING_NONE ||
	    vc4_state->y_scaling[0] != VC4_SCALING_NONE ||
	    vc4_state->y_scaling[1] != VC4_SCALING_NONE)
		hvs_load_shift = 1;
	else
		hvs_load_shift = 2;

	vc4_state->membus_load = 0;
	vc4_state->hvs_load = 0;
	for (i = 0; i < fb->format->num_planes; i++) {
		 
		vscale_factor = DIV_ROUND_UP(vc4_state->src_h[i],
					     vc4_state->crtc_h);
		vc4_state->membus_load += vc4_state->src_w[i] *
					  vc4_state->src_h[i] * vscale_factor *
					  fb->format->cpp[i];
		vc4_state->hvs_load += vc4_state->crtc_h * vc4_state->crtc_w;
	}

	vc4_state->hvs_load *= vrefresh;
	vc4_state->hvs_load >>= hvs_load_shift;
	vc4_state->membus_load *= vrefresh;
}

static int vc4_plane_allocate_lbm(struct drm_plane_state *state)
{
	struct vc4_dev *vc4 = to_vc4_dev(state->plane->dev);
	struct vc4_plane_state *vc4_state = to_vc4_plane_state(state);
	unsigned long irqflags;
	u32 lbm_size;

	lbm_size = vc4_lbm_size(state);
	if (!lbm_size)
		return 0;

	if (WARN_ON(!vc4_state->lbm_offset))
		return -EINVAL;

	 
	if (!drm_mm_node_allocated(&vc4_state->lbm)) {
		int ret;

		spin_lock_irqsave(&vc4->hvs->mm_lock, irqflags);
		ret = drm_mm_insert_node_generic(&vc4->hvs->lbm_mm,
						 &vc4_state->lbm,
						 lbm_size,
						 vc4->is_vc5 ? 64 : 32,
						 0, 0);
		spin_unlock_irqrestore(&vc4->hvs->mm_lock, irqflags);

		if (ret)
			return ret;
	} else {
		WARN_ON_ONCE(lbm_size != vc4_state->lbm.size);
	}

	vc4_state->dlist[vc4_state->lbm_offset] = vc4_state->lbm.start;

	return 0;
}

 
static const u32 colorspace_coeffs[2][DRM_COLOR_ENCODING_MAX][3] = {
	{
		 
		{
			 
			SCALER_CSC0_ITR_R_601_5,
			SCALER_CSC1_ITR_R_601_5,
			SCALER_CSC2_ITR_R_601_5,
		}, {
			 
			SCALER_CSC0_ITR_R_709_3,
			SCALER_CSC1_ITR_R_709_3,
			SCALER_CSC2_ITR_R_709_3,
		}, {
			 
			SCALER_CSC0_ITR_R_2020,
			SCALER_CSC1_ITR_R_2020,
			SCALER_CSC2_ITR_R_2020,
		}
	}, {
		 
		{
			 
			SCALER_CSC0_JPEG_JFIF,
			SCALER_CSC1_JPEG_JFIF,
			SCALER_CSC2_JPEG_JFIF,
		}, {
			 
			SCALER_CSC0_ITR_R_709_3_FR,
			SCALER_CSC1_ITR_R_709_3_FR,
			SCALER_CSC2_ITR_R_709_3_FR,
		}, {
			 
			SCALER_CSC0_ITR_R_2020_FR,
			SCALER_CSC1_ITR_R_2020_FR,
			SCALER_CSC2_ITR_R_2020_FR,
		}
	}
};

static u32 vc4_hvs4_get_alpha_blend_mode(struct drm_plane_state *state)
{
	if (!state->fb->format->has_alpha)
		return VC4_SET_FIELD(SCALER_POS2_ALPHA_MODE_FIXED,
				     SCALER_POS2_ALPHA_MODE);

	switch (state->pixel_blend_mode) {
	case DRM_MODE_BLEND_PIXEL_NONE:
		return VC4_SET_FIELD(SCALER_POS2_ALPHA_MODE_FIXED,
				     SCALER_POS2_ALPHA_MODE);
	default:
	case DRM_MODE_BLEND_PREMULTI:
		return VC4_SET_FIELD(SCALER_POS2_ALPHA_MODE_PIPELINE,
				     SCALER_POS2_ALPHA_MODE) |
			SCALER_POS2_ALPHA_PREMULT;
	case DRM_MODE_BLEND_COVERAGE:
		return VC4_SET_FIELD(SCALER_POS2_ALPHA_MODE_PIPELINE,
				     SCALER_POS2_ALPHA_MODE);
	}
}

static u32 vc4_hvs5_get_alpha_blend_mode(struct drm_plane_state *state)
{
	if (!state->fb->format->has_alpha)
		return VC4_SET_FIELD(SCALER5_CTL2_ALPHA_MODE_FIXED,
				     SCALER5_CTL2_ALPHA_MODE);

	switch (state->pixel_blend_mode) {
	case DRM_MODE_BLEND_PIXEL_NONE:
		return VC4_SET_FIELD(SCALER5_CTL2_ALPHA_MODE_FIXED,
				     SCALER5_CTL2_ALPHA_MODE);
	default:
	case DRM_MODE_BLEND_PREMULTI:
		return VC4_SET_FIELD(SCALER5_CTL2_ALPHA_MODE_PIPELINE,
				     SCALER5_CTL2_ALPHA_MODE) |
			SCALER5_CTL2_ALPHA_PREMULT;
	case DRM_MODE_BLEND_COVERAGE:
		return VC4_SET_FIELD(SCALER5_CTL2_ALPHA_MODE_PIPELINE,
				     SCALER5_CTL2_ALPHA_MODE);
	}
}

 
static int vc4_plane_mode_set(struct drm_plane *plane,
			      struct drm_plane_state *state)
{
	struct vc4_dev *vc4 = to_vc4_dev(plane->dev);
	struct vc4_plane_state *vc4_state = to_vc4_plane_state(state);
	struct drm_framebuffer *fb = state->fb;
	u32 ctl0_offset = vc4_state->dlist_count;
	const struct hvs_format *format = vc4_get_hvs_format(fb->format->format);
	u64 base_format_mod = fourcc_mod_broadcom_mod(fb->modifier);
	int num_planes = fb->format->num_planes;
	u32 h_subsample = fb->format->hsub;
	u32 v_subsample = fb->format->vsub;
	bool mix_plane_alpha;
	bool covers_screen;
	u32 scl0, scl1, pitch0;
	u32 tiling, src_y;
	u32 hvs_format = format->hvs;
	unsigned int rotation;
	int ret, i;

	if (vc4_state->dlist_initialized)
		return 0;

	ret = vc4_plane_setup_clipping_and_scaling(state);
	if (ret)
		return ret;

	 
	if (num_planes == 1) {
		scl0 = vc4_get_scl_field(state, 0);
		scl1 = scl0;
	} else {
		scl0 = vc4_get_scl_field(state, 1);
		scl1 = vc4_get_scl_field(state, 0);
	}

	rotation = drm_rotation_simplify(state->rotation,
					 DRM_MODE_ROTATE_0 |
					 DRM_MODE_REFLECT_X |
					 DRM_MODE_REFLECT_Y);

	 
	src_y = vc4_state->src_y;
	if (rotation & DRM_MODE_REFLECT_Y)
		src_y += vc4_state->src_h[0] - 1;

	switch (base_format_mod) {
	case DRM_FORMAT_MOD_LINEAR:
		tiling = SCALER_CTL0_TILING_LINEAR;
		pitch0 = VC4_SET_FIELD(fb->pitches[0], SCALER_SRC_PITCH);

		 
		for (i = 0; i < num_planes; i++) {
			vc4_state->offsets[i] += src_y /
						 (i ? v_subsample : 1) *
						 fb->pitches[i];

			vc4_state->offsets[i] += vc4_state->src_x /
						 (i ? h_subsample : 1) *
						 fb->format->cpp[i];
		}

		break;

	case DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED: {
		u32 tile_size_shift = 12;  
		 
		u32 tile_w_shift = fb->format->cpp[0] == 2 ? 6 : 5;
		u32 tile_h_shift = 5;  
		u32 tile_w_mask = (1 << tile_w_shift) - 1;
		 
		u32 tile_h_mask = (2 << tile_h_shift) - 1;
		 
		u32 tiles_w = fb->pitches[0] >> (tile_size_shift - tile_h_shift);
		u32 tiles_l = vc4_state->src_x >> tile_w_shift;
		u32 tiles_r = tiles_w - tiles_l;
		u32 tiles_t = src_y >> tile_h_shift;
		 
		u32 tile_y = (src_y >> 4) & 1;
		u32 subtile_y = (src_y >> 2) & 3;
		u32 utile_y = src_y & 3;
		u32 x_off = vc4_state->src_x & tile_w_mask;
		u32 y_off = src_y & tile_h_mask;

		 
		if (rotation & DRM_MODE_REFLECT_Y) {
			y_off = tile_h_mask - y_off;
			pitch0 = SCALER_PITCH0_TILE_LINE_DIR;
		} else {
			pitch0 = 0;
		}

		tiling = SCALER_CTL0_TILING_256B_OR_T;
		pitch0 |= (VC4_SET_FIELD(x_off, SCALER_PITCH0_SINK_PIX) |
			   VC4_SET_FIELD(y_off, SCALER_PITCH0_TILE_Y_OFFSET) |
			   VC4_SET_FIELD(tiles_l, SCALER_PITCH0_TILE_WIDTH_L) |
			   VC4_SET_FIELD(tiles_r, SCALER_PITCH0_TILE_WIDTH_R));
		vc4_state->offsets[0] += tiles_t * (tiles_w << tile_size_shift);
		vc4_state->offsets[0] += subtile_y << 8;
		vc4_state->offsets[0] += utile_y << 4;

		 
		if (tiles_t & 1) {
			pitch0 |= SCALER_PITCH0_TILE_INITIAL_LINE_DIR;
			vc4_state->offsets[0] += (tiles_w - tiles_l) <<
						 tile_size_shift;
			vc4_state->offsets[0] -= (1 + !tile_y) << 10;
		} else {
			vc4_state->offsets[0] += tiles_l << tile_size_shift;
			vc4_state->offsets[0] += tile_y << 10;
		}

		break;
	}

	case DRM_FORMAT_MOD_BROADCOM_SAND64:
	case DRM_FORMAT_MOD_BROADCOM_SAND128:
	case DRM_FORMAT_MOD_BROADCOM_SAND256: {
		uint32_t param = fourcc_mod_broadcom_param(fb->modifier);

		if (param > SCALER_TILE_HEIGHT_MASK) {
			DRM_DEBUG_KMS("SAND height too large (%d)\n",
				      param);
			return -EINVAL;
		}

		if (fb->format->format == DRM_FORMAT_P030) {
			hvs_format = HVS_PIXEL_FORMAT_YCBCR_10BIT;
			tiling = SCALER_CTL0_TILING_128B;
		} else {
			hvs_format = HVS_PIXEL_FORMAT_H264;

			switch (base_format_mod) {
			case DRM_FORMAT_MOD_BROADCOM_SAND64:
				tiling = SCALER_CTL0_TILING_64B;
				break;
			case DRM_FORMAT_MOD_BROADCOM_SAND128:
				tiling = SCALER_CTL0_TILING_128B;
				break;
			case DRM_FORMAT_MOD_BROADCOM_SAND256:
				tiling = SCALER_CTL0_TILING_256B_OR_T;
				break;
			default:
				return -EINVAL;
			}
		}

		 
		for (i = 0; i < num_planes; i++) {
			u32 tile_w, tile, x_off, pix_per_tile;

			if (fb->format->format == DRM_FORMAT_P030) {
				 
				u32 remaining_pixels = vc4_state->src_x % 96;
				u32 aligned = remaining_pixels / 12;
				u32 last_bits = remaining_pixels % 12;

				x_off = aligned * 16 + last_bits;
				tile_w = 128;
				pix_per_tile = 96;
			} else {
				switch (base_format_mod) {
				case DRM_FORMAT_MOD_BROADCOM_SAND64:
					tile_w = 64;
					break;
				case DRM_FORMAT_MOD_BROADCOM_SAND128:
					tile_w = 128;
					break;
				case DRM_FORMAT_MOD_BROADCOM_SAND256:
					tile_w = 256;
					break;
				default:
					return -EINVAL;
				}
				pix_per_tile = tile_w / fb->format->cpp[0];
				x_off = (vc4_state->src_x % pix_per_tile) /
					(i ? h_subsample : 1) *
					fb->format->cpp[i];
			}

			tile = vc4_state->src_x / pix_per_tile;

			vc4_state->offsets[i] += param * tile_w * tile;
			vc4_state->offsets[i] += src_y /
						 (i ? v_subsample : 1) *
						 tile_w;
			vc4_state->offsets[i] += x_off & ~(i ? 1 : 0);
		}

		pitch0 = VC4_SET_FIELD(param, SCALER_TILE_HEIGHT);
		break;
	}

	default:
		DRM_DEBUG_KMS("Unsupported FB tiling flag 0x%16llx",
			      (long long)fb->modifier);
		return -EINVAL;
	}

	 
	mix_plane_alpha = state->alpha != DRM_BLEND_ALPHA_OPAQUE &&
			  fb->format->has_alpha;

	if (!vc4->is_vc5) {
	 
		vc4_dlist_write(vc4_state,
				SCALER_CTL0_VALID |
				(rotation & DRM_MODE_REFLECT_X ? SCALER_CTL0_HFLIP : 0) |
				(rotation & DRM_MODE_REFLECT_Y ? SCALER_CTL0_VFLIP : 0) |
				VC4_SET_FIELD(SCALER_CTL0_RGBA_EXPAND_ROUND, SCALER_CTL0_RGBA_EXPAND) |
				(format->pixel_order << SCALER_CTL0_ORDER_SHIFT) |
				(hvs_format << SCALER_CTL0_PIXEL_FORMAT_SHIFT) |
				VC4_SET_FIELD(tiling, SCALER_CTL0_TILING) |
				(vc4_state->is_unity ? SCALER_CTL0_UNITY : 0) |
				VC4_SET_FIELD(scl0, SCALER_CTL0_SCL0) |
				VC4_SET_FIELD(scl1, SCALER_CTL0_SCL1));

		 
		vc4_state->pos0_offset = vc4_state->dlist_count;
		vc4_dlist_write(vc4_state,
				VC4_SET_FIELD(state->alpha >> 8, SCALER_POS0_FIXED_ALPHA) |
				VC4_SET_FIELD(vc4_state->crtc_x, SCALER_POS0_START_X) |
				VC4_SET_FIELD(vc4_state->crtc_y, SCALER_POS0_START_Y));

		 
		if (!vc4_state->is_unity) {
			vc4_dlist_write(vc4_state,
					VC4_SET_FIELD(vc4_state->crtc_w,
						      SCALER_POS1_SCL_WIDTH) |
					VC4_SET_FIELD(vc4_state->crtc_h,
						      SCALER_POS1_SCL_HEIGHT));
		}

		 
		vc4_state->pos2_offset = vc4_state->dlist_count;
		vc4_dlist_write(vc4_state,
				(mix_plane_alpha ? SCALER_POS2_ALPHA_MIX : 0) |
				vc4_hvs4_get_alpha_blend_mode(state) |
				VC4_SET_FIELD(vc4_state->src_w[0],
					      SCALER_POS2_WIDTH) |
				VC4_SET_FIELD(vc4_state->src_h[0],
					      SCALER_POS2_HEIGHT));

		 
		vc4_dlist_write(vc4_state, 0xc0c0c0c0);

	} else {
		 
		vc4_dlist_write(vc4_state,
				SCALER_CTL0_VALID |
				(format->pixel_order_hvs5 << SCALER_CTL0_ORDER_SHIFT) |
				(hvs_format << SCALER_CTL0_PIXEL_FORMAT_SHIFT) |
				VC4_SET_FIELD(tiling, SCALER_CTL0_TILING) |
				(vc4_state->is_unity ?
						SCALER5_CTL0_UNITY : 0) |
				VC4_SET_FIELD(scl0, SCALER_CTL0_SCL0) |
				VC4_SET_FIELD(scl1, SCALER_CTL0_SCL1) |
				SCALER5_CTL0_ALPHA_EXPAND |
				SCALER5_CTL0_RGB_EXPAND);

		 
		vc4_state->pos0_offset = vc4_state->dlist_count;
		vc4_dlist_write(vc4_state,
				(rotation & DRM_MODE_REFLECT_Y ?
						SCALER5_POS0_VFLIP : 0) |
				VC4_SET_FIELD(vc4_state->crtc_x,
					      SCALER_POS0_START_X) |
				(rotation & DRM_MODE_REFLECT_X ?
					      SCALER5_POS0_HFLIP : 0) |
				VC4_SET_FIELD(vc4_state->crtc_y,
					      SCALER5_POS0_START_Y)
			       );

		 
		vc4_dlist_write(vc4_state,
				VC4_SET_FIELD(state->alpha >> 4,
					      SCALER5_CTL2_ALPHA) |
				vc4_hvs5_get_alpha_blend_mode(state) |
				(mix_plane_alpha ?
					SCALER5_CTL2_ALPHA_MIX : 0)
			       );

		 
		if (!vc4_state->is_unity) {
			vc4_dlist_write(vc4_state,
					VC4_SET_FIELD(vc4_state->crtc_w,
						      SCALER5_POS1_SCL_WIDTH) |
					VC4_SET_FIELD(vc4_state->crtc_h,
						      SCALER5_POS1_SCL_HEIGHT));
		}

		 
		vc4_state->pos2_offset = vc4_state->dlist_count;
		vc4_dlist_write(vc4_state,
				VC4_SET_FIELD(vc4_state->src_w[0],
					      SCALER5_POS2_WIDTH) |
				VC4_SET_FIELD(vc4_state->src_h[0],
					      SCALER5_POS2_HEIGHT));

		 
		vc4_dlist_write(vc4_state, 0xc0c0c0c0);
	}


	 
	vc4_state->ptr0_offset = vc4_state->dlist_count;
	for (i = 0; i < num_planes; i++)
		vc4_dlist_write(vc4_state, vc4_state->offsets[i]);

	 
	for (i = 0; i < num_planes; i++)
		vc4_dlist_write(vc4_state, 0xc0c0c0c0);

	 
	vc4_dlist_write(vc4_state, pitch0);

	 
	for (i = 1; i < num_planes; i++) {
		if (hvs_format != HVS_PIXEL_FORMAT_H264 &&
		    hvs_format != HVS_PIXEL_FORMAT_YCBCR_10BIT) {
			vc4_dlist_write(vc4_state,
					VC4_SET_FIELD(fb->pitches[i],
						      SCALER_SRC_PITCH));
		} else {
			vc4_dlist_write(vc4_state, pitch0);
		}
	}

	 
	if (vc4_state->is_yuv) {
		enum drm_color_encoding color_encoding = state->color_encoding;
		enum drm_color_range color_range = state->color_range;
		const u32 *ccm;

		if (color_encoding >= DRM_COLOR_ENCODING_MAX)
			color_encoding = DRM_COLOR_YCBCR_BT601;
		if (color_range >= DRM_COLOR_RANGE_MAX)
			color_range = DRM_COLOR_YCBCR_LIMITED_RANGE;

		ccm = colorspace_coeffs[color_range][color_encoding];

		vc4_dlist_write(vc4_state, ccm[0]);
		vc4_dlist_write(vc4_state, ccm[1]);
		vc4_dlist_write(vc4_state, ccm[2]);
	}

	vc4_state->lbm_offset = 0;

	if (vc4_state->x_scaling[0] != VC4_SCALING_NONE ||
	    vc4_state->x_scaling[1] != VC4_SCALING_NONE ||
	    vc4_state->y_scaling[0] != VC4_SCALING_NONE ||
	    vc4_state->y_scaling[1] != VC4_SCALING_NONE) {
		 
		if (vc4_state->y_scaling[0] != VC4_SCALING_NONE ||
		    vc4_state->y_scaling[1] != VC4_SCALING_NONE) {
			vc4_state->lbm_offset = vc4_state->dlist_count;
			vc4_dlist_counter_increment(vc4_state);
		}

		if (num_planes > 1) {
			 
			vc4_write_scaling_parameters(state, 1);
		}
		vc4_write_scaling_parameters(state, 0);

		 
		if (vc4_state->x_scaling[0] == VC4_SCALING_PPF ||
		    vc4_state->y_scaling[0] == VC4_SCALING_PPF ||
		    vc4_state->x_scaling[1] == VC4_SCALING_PPF ||
		    vc4_state->y_scaling[1] == VC4_SCALING_PPF) {
			u32 kernel = VC4_SET_FIELD(vc4->hvs->mitchell_netravali_filter.start,
						   SCALER_PPF_KERNEL_OFFSET);

			 
			vc4_dlist_write(vc4_state, kernel);
			 
			vc4_dlist_write(vc4_state, kernel);
			 
			vc4_dlist_write(vc4_state, kernel);
			 
			vc4_dlist_write(vc4_state, kernel);
		}
	}

	vc4_state->dlist[ctl0_offset] |=
		VC4_SET_FIELD(vc4_state->dlist_count, SCALER_CTL0_SIZE);

	 
	covers_screen = vc4_state->crtc_x == 0 && vc4_state->crtc_y == 0 &&
			vc4_state->crtc_w == state->crtc->mode.hdisplay &&
			vc4_state->crtc_h == state->crtc->mode.vdisplay;
	 
	vc4_state->needs_bg_fill = fb->format->has_alpha || !covers_screen ||
				   state->alpha != DRM_BLEND_ALPHA_OPAQUE;

	 
	vc4_state->dlist_initialized = 1;

	vc4_plane_calc_load(state);

	return 0;
}

 
static int vc4_plane_atomic_check(struct drm_plane *plane,
				  struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
										 plane);
	struct vc4_plane_state *vc4_state = to_vc4_plane_state(new_plane_state);
	int ret;

	vc4_state->dlist_count = 0;

	if (!plane_enabled(new_plane_state))
		return 0;

	ret = vc4_plane_mode_set(plane, new_plane_state);
	if (ret)
		return ret;

	return vc4_plane_allocate_lbm(new_plane_state);
}

static void vc4_plane_atomic_update(struct drm_plane *plane,
				    struct drm_atomic_state *state)
{
	 
}

u32 vc4_plane_write_dlist(struct drm_plane *plane, u32 __iomem *dlist)
{
	struct vc4_plane_state *vc4_state = to_vc4_plane_state(plane->state);
	int i;
	int idx;

	if (!drm_dev_enter(plane->dev, &idx))
		goto out;

	vc4_state->hw_dlist = dlist;

	 
	for (i = 0; i < vc4_state->dlist_count; i++)
		writel(vc4_state->dlist[i], &dlist[i]);

	drm_dev_exit(idx);

out:
	return vc4_state->dlist_count;
}

u32 vc4_plane_dlist_size(const struct drm_plane_state *state)
{
	const struct vc4_plane_state *vc4_state = to_vc4_plane_state(state);

	return vc4_state->dlist_count;
}

 
void vc4_plane_async_set_fb(struct drm_plane *plane, struct drm_framebuffer *fb)
{
	struct vc4_plane_state *vc4_state = to_vc4_plane_state(plane->state);
	struct drm_gem_dma_object *bo = drm_fb_dma_get_gem_obj(fb, 0);
	uint32_t addr;
	int idx;

	if (!drm_dev_enter(plane->dev, &idx))
		return;

	 
	WARN_ON_ONCE(plane->state->crtc_x < 0 || plane->state->crtc_y < 0);
	addr = bo->dma_addr + fb->offsets[0];

	 
	writel(addr, &vc4_state->hw_dlist[vc4_state->ptr0_offset]);

	 
	vc4_state->dlist[vc4_state->ptr0_offset] = addr;

	drm_dev_exit(idx);
}

static void vc4_plane_atomic_async_update(struct drm_plane *plane,
					  struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
										 plane);
	struct vc4_plane_state *vc4_state, *new_vc4_state;
	int idx;

	if (!drm_dev_enter(plane->dev, &idx))
		return;

	swap(plane->state->fb, new_plane_state->fb);
	plane->state->crtc_x = new_plane_state->crtc_x;
	plane->state->crtc_y = new_plane_state->crtc_y;
	plane->state->crtc_w = new_plane_state->crtc_w;
	plane->state->crtc_h = new_plane_state->crtc_h;
	plane->state->src_x = new_plane_state->src_x;
	plane->state->src_y = new_plane_state->src_y;
	plane->state->src_w = new_plane_state->src_w;
	plane->state->src_h = new_plane_state->src_h;
	plane->state->alpha = new_plane_state->alpha;
	plane->state->pixel_blend_mode = new_plane_state->pixel_blend_mode;
	plane->state->rotation = new_plane_state->rotation;
	plane->state->zpos = new_plane_state->zpos;
	plane->state->normalized_zpos = new_plane_state->normalized_zpos;
	plane->state->color_encoding = new_plane_state->color_encoding;
	plane->state->color_range = new_plane_state->color_range;
	plane->state->src = new_plane_state->src;
	plane->state->dst = new_plane_state->dst;
	plane->state->visible = new_plane_state->visible;

	new_vc4_state = to_vc4_plane_state(new_plane_state);
	vc4_state = to_vc4_plane_state(plane->state);

	vc4_state->crtc_x = new_vc4_state->crtc_x;
	vc4_state->crtc_y = new_vc4_state->crtc_y;
	vc4_state->crtc_h = new_vc4_state->crtc_h;
	vc4_state->crtc_w = new_vc4_state->crtc_w;
	vc4_state->src_x = new_vc4_state->src_x;
	vc4_state->src_y = new_vc4_state->src_y;
	memcpy(vc4_state->src_w, new_vc4_state->src_w,
	       sizeof(vc4_state->src_w));
	memcpy(vc4_state->src_h, new_vc4_state->src_h,
	       sizeof(vc4_state->src_h));
	memcpy(vc4_state->x_scaling, new_vc4_state->x_scaling,
	       sizeof(vc4_state->x_scaling));
	memcpy(vc4_state->y_scaling, new_vc4_state->y_scaling,
	       sizeof(vc4_state->y_scaling));
	vc4_state->is_unity = new_vc4_state->is_unity;
	vc4_state->is_yuv = new_vc4_state->is_yuv;
	memcpy(vc4_state->offsets, new_vc4_state->offsets,
	       sizeof(vc4_state->offsets));
	vc4_state->needs_bg_fill = new_vc4_state->needs_bg_fill;

	 
	vc4_state->dlist[vc4_state->pos0_offset] =
		new_vc4_state->dlist[vc4_state->pos0_offset];
	vc4_state->dlist[vc4_state->pos2_offset] =
		new_vc4_state->dlist[vc4_state->pos2_offset];
	vc4_state->dlist[vc4_state->ptr0_offset] =
		new_vc4_state->dlist[vc4_state->ptr0_offset];

	 
	writel(vc4_state->dlist[vc4_state->pos0_offset],
	       &vc4_state->hw_dlist[vc4_state->pos0_offset]);
	writel(vc4_state->dlist[vc4_state->pos2_offset],
	       &vc4_state->hw_dlist[vc4_state->pos2_offset]);
	writel(vc4_state->dlist[vc4_state->ptr0_offset],
	       &vc4_state->hw_dlist[vc4_state->ptr0_offset]);

	drm_dev_exit(idx);
}

static int vc4_plane_atomic_async_check(struct drm_plane *plane,
					struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
										 plane);
	struct vc4_plane_state *old_vc4_state, *new_vc4_state;
	int ret;
	u32 i;

	ret = vc4_plane_mode_set(plane, new_plane_state);
	if (ret)
		return ret;

	old_vc4_state = to_vc4_plane_state(plane->state);
	new_vc4_state = to_vc4_plane_state(new_plane_state);

	if (!new_vc4_state->hw_dlist)
		return -EINVAL;

	if (old_vc4_state->dlist_count != new_vc4_state->dlist_count ||
	    old_vc4_state->pos0_offset != new_vc4_state->pos0_offset ||
	    old_vc4_state->pos2_offset != new_vc4_state->pos2_offset ||
	    old_vc4_state->ptr0_offset != new_vc4_state->ptr0_offset ||
	    vc4_lbm_size(plane->state) != vc4_lbm_size(new_plane_state))
		return -EINVAL;

	 
	for (i = 0; i < new_vc4_state->dlist_count; i++) {
		if (i == new_vc4_state->pos0_offset ||
		    i == new_vc4_state->pos2_offset ||
		    i == new_vc4_state->ptr0_offset ||
		    (new_vc4_state->lbm_offset &&
		     i == new_vc4_state->lbm_offset))
			continue;

		if (new_vc4_state->dlist[i] != old_vc4_state->dlist[i])
			return -EINVAL;
	}

	return 0;
}

static int vc4_prepare_fb(struct drm_plane *plane,
			  struct drm_plane_state *state)
{
	struct vc4_bo *bo;

	if (!state->fb)
		return 0;

	bo = to_vc4_bo(&drm_fb_dma_get_gem_obj(state->fb, 0)->base);

	drm_gem_plane_helper_prepare_fb(plane, state);

	if (plane->state->fb == state->fb)
		return 0;

	return vc4_bo_inc_usecnt(bo);
}

static void vc4_cleanup_fb(struct drm_plane *plane,
			   struct drm_plane_state *state)
{
	struct vc4_bo *bo;

	if (plane->state->fb == state->fb || !state->fb)
		return;

	bo = to_vc4_bo(&drm_fb_dma_get_gem_obj(state->fb, 0)->base);
	vc4_bo_dec_usecnt(bo);
}

static const struct drm_plane_helper_funcs vc4_plane_helper_funcs = {
	.atomic_check = vc4_plane_atomic_check,
	.atomic_update = vc4_plane_atomic_update,
	.prepare_fb = vc4_prepare_fb,
	.cleanup_fb = vc4_cleanup_fb,
	.atomic_async_check = vc4_plane_atomic_async_check,
	.atomic_async_update = vc4_plane_atomic_async_update,
};

static const struct drm_plane_helper_funcs vc5_plane_helper_funcs = {
	.atomic_check = vc4_plane_atomic_check,
	.atomic_update = vc4_plane_atomic_update,
	.atomic_async_check = vc4_plane_atomic_async_check,
	.atomic_async_update = vc4_plane_atomic_async_update,
};

static bool vc4_format_mod_supported(struct drm_plane *plane,
				     uint32_t format,
				     uint64_t modifier)
{
	 
	switch (format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
		switch (fourcc_mod_broadcom_mod(modifier)) {
		case DRM_FORMAT_MOD_LINEAR:
		case DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED:
			return true;
		default:
			return false;
		}
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		switch (fourcc_mod_broadcom_mod(modifier)) {
		case DRM_FORMAT_MOD_LINEAR:
		case DRM_FORMAT_MOD_BROADCOM_SAND64:
		case DRM_FORMAT_MOD_BROADCOM_SAND128:
		case DRM_FORMAT_MOD_BROADCOM_SAND256:
			return true;
		default:
			return false;
		}
	case DRM_FORMAT_P030:
		switch (fourcc_mod_broadcom_mod(modifier)) {
		case DRM_FORMAT_MOD_BROADCOM_SAND128:
			return true;
		default:
			return false;
		}
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRX1010102:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_RGB332:
	case DRM_FORMAT_BGR233:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	default:
		return (modifier == DRM_FORMAT_MOD_LINEAR);
	}
}

static const struct drm_plane_funcs vc4_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.reset = vc4_plane_reset,
	.atomic_duplicate_state = vc4_plane_duplicate_state,
	.atomic_destroy_state = vc4_plane_destroy_state,
	.format_mod_supported = vc4_format_mod_supported,
};

struct drm_plane *vc4_plane_init(struct drm_device *dev,
				 enum drm_plane_type type,
				 uint32_t possible_crtcs)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct drm_plane *plane;
	struct vc4_plane *vc4_plane;
	u32 formats[ARRAY_SIZE(hvs_formats)];
	int num_formats = 0;
	unsigned i;
	static const uint64_t modifiers[] = {
		DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED,
		DRM_FORMAT_MOD_BROADCOM_SAND128,
		DRM_FORMAT_MOD_BROADCOM_SAND64,
		DRM_FORMAT_MOD_BROADCOM_SAND256,
		DRM_FORMAT_MOD_LINEAR,
		DRM_FORMAT_MOD_INVALID
	};

	for (i = 0; i < ARRAY_SIZE(hvs_formats); i++) {
		if (!hvs_formats[i].hvs5_only || vc4->is_vc5) {
			formats[num_formats] = hvs_formats[i].drm;
			num_formats++;
		}
	}

	vc4_plane = drmm_universal_plane_alloc(dev, struct vc4_plane, base,
					       possible_crtcs,
					       &vc4_plane_funcs,
					       formats, num_formats,
					       modifiers, type, NULL);
	if (IS_ERR(vc4_plane))
		return ERR_CAST(vc4_plane);
	plane = &vc4_plane->base;

	if (vc4->is_vc5)
		drm_plane_helper_add(plane, &vc5_plane_helper_funcs);
	else
		drm_plane_helper_add(plane, &vc4_plane_helper_funcs);

	drm_plane_create_alpha_property(plane);
	drm_plane_create_blend_mode_property(plane,
					     BIT(DRM_MODE_BLEND_PIXEL_NONE) |
					     BIT(DRM_MODE_BLEND_PREMULTI) |
					     BIT(DRM_MODE_BLEND_COVERAGE));
	drm_plane_create_rotation_property(plane, DRM_MODE_ROTATE_0,
					   DRM_MODE_ROTATE_0 |
					   DRM_MODE_ROTATE_180 |
					   DRM_MODE_REFLECT_X |
					   DRM_MODE_REFLECT_Y);

	drm_plane_create_color_properties(plane,
					  BIT(DRM_COLOR_YCBCR_BT601) |
					  BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
					  BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) |
					  BIT(DRM_COLOR_YCBCR_FULL_RANGE),
					  DRM_COLOR_YCBCR_BT709,
					  DRM_COLOR_YCBCR_LIMITED_RANGE);

	if (type == DRM_PLANE_TYPE_PRIMARY)
		drm_plane_create_zpos_immutable_property(plane, 0);

	return plane;
}

#define VC4_NUM_OVERLAY_PLANES	16

int vc4_plane_create_additional_planes(struct drm_device *drm)
{
	struct drm_plane *cursor_plane;
	struct drm_crtc *crtc;
	unsigned int i;

	 
	for (i = 0; i < VC4_NUM_OVERLAY_PLANES; i++) {
		struct drm_plane *plane =
			vc4_plane_init(drm, DRM_PLANE_TYPE_OVERLAY,
				       GENMASK(drm->mode_config.num_crtc - 1, 0));

		if (IS_ERR(plane))
			continue;

		 
		drm_plane_create_zpos_property(plane, i + 1, 1,
					       VC4_NUM_OVERLAY_PLANES + 1);
	}

	drm_for_each_crtc(crtc, drm) {
		 
		cursor_plane = vc4_plane_init(drm, DRM_PLANE_TYPE_CURSOR,
					      drm_crtc_mask(crtc));
		if (!IS_ERR(cursor_plane)) {
			crtc->cursor = cursor_plane;

			drm_plane_create_zpos_property(cursor_plane,
						       VC4_NUM_OVERLAY_PLANES + 1,
						       1,
						       VC4_NUM_OVERLAY_PLANES + 1);
		}
	}

	return 0;
}
