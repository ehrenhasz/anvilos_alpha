
 

#include "vmwgfx_bo.h"
#include "vmwgfx_kms.h"

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_damage_helper.h>
#include <drm/drm_fourcc.h>

#define vmw_crtc_to_sou(x) \
	container_of(x, struct vmw_screen_object_unit, base.crtc)
#define vmw_encoder_to_sou(x) \
	container_of(x, struct vmw_screen_object_unit, base.encoder)
#define vmw_connector_to_sou(x) \
	container_of(x, struct vmw_screen_object_unit, base.connector)

 
struct vmw_kms_sou_surface_dirty {
	struct vmw_kms_dirty base;
	s32 left, right, top, bottom;
	s32 dst_x, dst_y;
	u32 sid;
};

 
struct vmw_kms_sou_readback_blit {
	uint32 header;
	SVGAFifoCmdBlitScreenToGMRFB body;
};

struct vmw_kms_sou_bo_blit {
	uint32 header;
	SVGAFifoCmdBlitGMRFBToScreen body;
};

struct vmw_kms_sou_dirty_cmd {
	SVGA3dCmdHeader header;
	SVGA3dCmdBlitSurfaceToScreen body;
};

struct vmw_kms_sou_define_gmrfb {
	uint32_t header;
	SVGAFifoCmdDefineGMRFB body;
};

 
struct vmw_screen_object_unit {
	struct vmw_display_unit base;

	unsigned long buffer_size;  
	struct vmw_bo *buffer;  

	bool defined;
};

static void vmw_sou_destroy(struct vmw_screen_object_unit *sou)
{
	vmw_du_cleanup(&sou->base);
	kfree(sou);
}


 

static void vmw_sou_crtc_destroy(struct drm_crtc *crtc)
{
	vmw_sou_destroy(vmw_crtc_to_sou(crtc));
}

 
static int vmw_sou_fifo_create(struct vmw_private *dev_priv,
			       struct vmw_screen_object_unit *sou,
			       int x, int y,
			       struct drm_display_mode *mode)
{
	size_t fifo_size;

	struct {
		struct {
			uint32_t cmdType;
		} header;
		SVGAScreenObject obj;
	} *cmd;

	BUG_ON(!sou->buffer);

	fifo_size = sizeof(*cmd);
	cmd = VMW_CMD_RESERVE(dev_priv, fifo_size);
	if (unlikely(cmd == NULL))
		return -ENOMEM;

	memset(cmd, 0, fifo_size);
	cmd->header.cmdType = SVGA_CMD_DEFINE_SCREEN;
	cmd->obj.structSize = sizeof(SVGAScreenObject);
	cmd->obj.id = sou->base.unit;
	cmd->obj.flags = SVGA_SCREEN_HAS_ROOT |
		(sou->base.unit == 0 ? SVGA_SCREEN_IS_PRIMARY : 0);
	cmd->obj.size.width = mode->hdisplay;
	cmd->obj.size.height = mode->vdisplay;
	cmd->obj.root.x = x;
	cmd->obj.root.y = y;
	sou->base.set_gui_x = cmd->obj.root.x;
	sou->base.set_gui_y = cmd->obj.root.y;

	 
	vmw_bo_get_guest_ptr(&sou->buffer->tbo, &cmd->obj.backingStore.ptr);
	cmd->obj.backingStore.pitch = mode->hdisplay * 4;

	vmw_cmd_commit(dev_priv, fifo_size);

	sou->defined = true;

	return 0;
}

 
static int vmw_sou_fifo_destroy(struct vmw_private *dev_priv,
				struct vmw_screen_object_unit *sou)
{
	size_t fifo_size;
	int ret;

	struct {
		struct {
			uint32_t cmdType;
		} header;
		SVGAFifoCmdDestroyScreen body;
	} *cmd;

	 
	if (unlikely(!sou->defined))
		return 0;

	fifo_size = sizeof(*cmd);
	cmd = VMW_CMD_RESERVE(dev_priv, fifo_size);
	if (unlikely(cmd == NULL))
		return -ENOMEM;

	memset(cmd, 0, fifo_size);
	cmd->header.cmdType = SVGA_CMD_DESTROY_SCREEN;
	cmd->body.screenId = sou->base.unit;

	vmw_cmd_commit(dev_priv, fifo_size);

	 
	ret = vmw_fallback_wait(dev_priv, false, true, 0, false, 3*HZ);
	if (unlikely(ret != 0))
		DRM_ERROR("Failed to sync with HW");
	else
		sou->defined = false;

	return ret;
}

 
static void vmw_sou_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct vmw_private *dev_priv;
	struct vmw_screen_object_unit *sou;
	struct vmw_framebuffer *vfb;
	struct drm_framebuffer *fb;
	struct drm_plane_state *ps;
	struct vmw_plane_state *vps;
	int ret;

	sou = vmw_crtc_to_sou(crtc);
	dev_priv = vmw_priv(crtc->dev);
	ps = crtc->primary->state;
	fb = ps->fb;
	vps = vmw_plane_state_to_vps(ps);

	vfb = (fb) ? vmw_framebuffer_to_vfb(fb) : NULL;

	if (sou->defined) {
		ret = vmw_sou_fifo_destroy(dev_priv, sou);
		if (ret) {
			DRM_ERROR("Failed to destroy Screen Object\n");
			return;
		}
	}

	if (vfb) {
		struct drm_connector_state *conn_state;
		struct vmw_connector_state *vmw_conn_state;
		int x, y;

		sou->buffer = vps->bo;
		sou->buffer_size = vps->bo_size;

		conn_state = sou->base.connector.state;
		vmw_conn_state = vmw_connector_state_to_vcs(conn_state);

		x = vmw_conn_state->gui_x;
		y = vmw_conn_state->gui_y;

		ret = vmw_sou_fifo_create(dev_priv, sou, x, y, &crtc->mode);
		if (ret)
			DRM_ERROR("Failed to define Screen Object %dx%d\n",
				  crtc->x, crtc->y);

	} else {
		sou->buffer = NULL;
		sou->buffer_size = 0;
	}
}

 
static void vmw_sou_crtc_helper_prepare(struct drm_crtc *crtc)
{
}

 
static void vmw_sou_crtc_atomic_enable(struct drm_crtc *crtc,
				       struct drm_atomic_state *state)
{
}

 
static void vmw_sou_crtc_atomic_disable(struct drm_crtc *crtc,
					struct drm_atomic_state *state)
{
	struct vmw_private *dev_priv;
	struct vmw_screen_object_unit *sou;
	int ret;


	if (!crtc) {
		DRM_ERROR("CRTC is NULL\n");
		return;
	}

	sou = vmw_crtc_to_sou(crtc);
	dev_priv = vmw_priv(crtc->dev);

	if (sou->defined) {
		ret = vmw_sou_fifo_destroy(dev_priv, sou);
		if (ret)
			DRM_ERROR("Failed to destroy Screen Object\n");
	}
}

static const struct drm_crtc_funcs vmw_screen_object_crtc_funcs = {
	.gamma_set = vmw_du_crtc_gamma_set,
	.destroy = vmw_sou_crtc_destroy,
	.reset = vmw_du_crtc_reset,
	.atomic_duplicate_state = vmw_du_crtc_duplicate_state,
	.atomic_destroy_state = vmw_du_crtc_destroy_state,
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
};

 

static void vmw_sou_encoder_destroy(struct drm_encoder *encoder)
{
	vmw_sou_destroy(vmw_encoder_to_sou(encoder));
}

static const struct drm_encoder_funcs vmw_screen_object_encoder_funcs = {
	.destroy = vmw_sou_encoder_destroy,
};

 

static void vmw_sou_connector_destroy(struct drm_connector *connector)
{
	vmw_sou_destroy(vmw_connector_to_sou(connector));
}

static const struct drm_connector_funcs vmw_sou_connector_funcs = {
	.dpms = vmw_du_connector_dpms,
	.detect = vmw_du_connector_detect,
	.fill_modes = vmw_du_connector_fill_modes,
	.destroy = vmw_sou_connector_destroy,
	.reset = vmw_du_connector_reset,
	.atomic_duplicate_state = vmw_du_connector_duplicate_state,
	.atomic_destroy_state = vmw_du_connector_destroy_state,
};


static const struct
drm_connector_helper_funcs vmw_sou_connector_helper_funcs = {
};



 

 
static void
vmw_sou_primary_plane_cleanup_fb(struct drm_plane *plane,
				 struct drm_plane_state *old_state)
{
	struct vmw_plane_state *vps = vmw_plane_state_to_vps(old_state);
	struct drm_crtc *crtc = plane->state->crtc ?
		plane->state->crtc : old_state->crtc;

	if (vps->bo)
		vmw_bo_unpin(vmw_priv(crtc->dev), vps->bo, false);
	vmw_bo_unreference(&vps->bo);
	vps->bo_size = 0;

	vmw_du_plane_cleanup_fb(plane, old_state);
}


 
static int
vmw_sou_primary_plane_prepare_fb(struct drm_plane *plane,
				 struct drm_plane_state *new_state)
{
	struct drm_framebuffer *new_fb = new_state->fb;
	struct drm_crtc *crtc = plane->state->crtc ?: new_state->crtc;
	struct vmw_plane_state *vps = vmw_plane_state_to_vps(new_state);
	struct vmw_private *dev_priv;
	int ret;
	struct vmw_bo_params bo_params = {
		.domain = VMW_BO_DOMAIN_VRAM,
		.busy_domain = VMW_BO_DOMAIN_VRAM,
		.bo_type = ttm_bo_type_device,
		.pin = true
	};

	if (!new_fb) {
		vmw_bo_unreference(&vps->bo);
		vps->bo_size = 0;

		return 0;
	}

	bo_params.size = new_state->crtc_w * new_state->crtc_h * 4;
	dev_priv = vmw_priv(crtc->dev);

	if (vps->bo) {
		if (vps->bo_size == bo_params.size) {
			 
			return vmw_bo_pin_in_vram(dev_priv, vps->bo,
						      true);
		}

		vmw_bo_unreference(&vps->bo);
		vps->bo_size = 0;
	}

	vmw_svga_enable(dev_priv);

	 
	vmw_overlay_pause_all(dev_priv);
	ret = vmw_bo_create(dev_priv, &bo_params, &vps->bo);
	vmw_overlay_resume_all(dev_priv);
	if (ret)
		return ret;

	vps->bo_size = bo_params.size;

	 
	return vmw_bo_pin_in_vram(dev_priv, vps->bo, true);
}

static uint32_t vmw_sou_bo_fifo_size(struct vmw_du_update_plane *update,
				     uint32_t num_hits)
{
	return sizeof(struct vmw_kms_sou_define_gmrfb) +
		sizeof(struct vmw_kms_sou_bo_blit) * num_hits;
}

static uint32_t vmw_sou_bo_define_gmrfb(struct vmw_du_update_plane *update,
					void *cmd)
{
	struct vmw_framebuffer_bo *vfbbo =
		container_of(update->vfb, typeof(*vfbbo), base);
	struct vmw_kms_sou_define_gmrfb *gmr = cmd;
	int depth = update->vfb->base.format->depth;

	 
	if (depth == 32)
		depth = 24;

	gmr->header = SVGA_CMD_DEFINE_GMRFB;

	gmr->body.format.bitsPerPixel = update->vfb->base.format->cpp[0] * 8;
	gmr->body.format.colorDepth = depth;
	gmr->body.format.reserved = 0;
	gmr->body.bytesPerLine = update->vfb->base.pitches[0];
	vmw_bo_get_guest_ptr(&vfbbo->buffer->tbo, &gmr->body.ptr);

	return sizeof(*gmr);
}

static uint32_t vmw_sou_bo_populate_clip(struct vmw_du_update_plane  *update,
					 void *cmd, struct drm_rect *clip,
					 uint32_t fb_x, uint32_t fb_y)
{
	struct vmw_kms_sou_bo_blit *blit = cmd;

	blit->header = SVGA_CMD_BLIT_GMRFB_TO_SCREEN;
	blit->body.destScreenId = update->du->unit;
	blit->body.srcOrigin.x = fb_x;
	blit->body.srcOrigin.y = fb_y;
	blit->body.destRect.left = clip->x1;
	blit->body.destRect.top = clip->y1;
	blit->body.destRect.right = clip->x2;
	blit->body.destRect.bottom = clip->y2;

	return sizeof(*blit);
}

static uint32_t vmw_stud_bo_post_clip(struct vmw_du_update_plane  *update,
				      void *cmd, struct drm_rect *bb)
{
	return 0;
}

 
static int vmw_sou_plane_update_bo(struct vmw_private *dev_priv,
				   struct drm_plane *plane,
				   struct drm_plane_state *old_state,
				   struct vmw_framebuffer *vfb,
				   struct vmw_fence_obj **out_fence)
{
	struct vmw_du_update_plane_buffer bo_update;

	memset(&bo_update, 0, sizeof(struct vmw_du_update_plane_buffer));
	bo_update.base.plane = plane;
	bo_update.base.old_state = old_state;
	bo_update.base.dev_priv = dev_priv;
	bo_update.base.du = vmw_crtc_to_du(plane->state->crtc);
	bo_update.base.vfb = vfb;
	bo_update.base.out_fence = out_fence;
	bo_update.base.mutex = NULL;
	bo_update.base.intr = true;

	bo_update.base.calc_fifo_size = vmw_sou_bo_fifo_size;
	bo_update.base.post_prepare = vmw_sou_bo_define_gmrfb;
	bo_update.base.clip = vmw_sou_bo_populate_clip;
	bo_update.base.post_clip = vmw_stud_bo_post_clip;

	return vmw_du_helper_plane_update(&bo_update.base);
}

static uint32_t vmw_sou_surface_fifo_size(struct vmw_du_update_plane *update,
					  uint32_t num_hits)
{
	return sizeof(struct vmw_kms_sou_dirty_cmd) + sizeof(SVGASignedRect) *
		num_hits;
}

static uint32_t vmw_sou_surface_post_prepare(struct vmw_du_update_plane *update,
					     void *cmd)
{
	struct vmw_du_update_plane_surface *srf_update;

	srf_update = container_of(update, typeof(*srf_update), base);

	 
	srf_update->cmd_start = cmd;

	return 0;
}

static uint32_t vmw_sou_surface_pre_clip(struct vmw_du_update_plane *update,
					 void *cmd, uint32_t num_hits)
{
	struct vmw_kms_sou_dirty_cmd *blit = cmd;
	struct vmw_framebuffer_surface *vfbs;

	vfbs = container_of(update->vfb, typeof(*vfbs), base);

	blit->header.id = SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN;
	blit->header.size = sizeof(blit->body) + sizeof(SVGASignedRect) *
		num_hits;

	blit->body.srcImage.sid = vfbs->surface->res.id;
	blit->body.destScreenId = update->du->unit;

	 
	blit->body.srcRect.left = 0;
	blit->body.srcRect.top = 0;
	blit->body.srcRect.right = 0;
	blit->body.srcRect.bottom = 0;

	blit->body.destRect.left = 0;
	blit->body.destRect.top = 0;
	blit->body.destRect.right = 0;
	blit->body.destRect.bottom = 0;

	return sizeof(*blit);
}

static uint32_t vmw_sou_surface_clip_rect(struct vmw_du_update_plane *update,
					  void *cmd, struct drm_rect *clip,
					  uint32_t src_x, uint32_t src_y)
{
	SVGASignedRect *rect = cmd;

	 
	rect->left = clip->x1;
	rect->top = clip->y1;
	rect->right = clip->x2;
	rect->bottom = clip->y2;

	return sizeof(*rect);
}

static uint32_t vmw_sou_surface_post_clip(struct vmw_du_update_plane *update,
					  void *cmd, struct drm_rect *bb)
{
	struct vmw_du_update_plane_surface *srf_update;
	struct drm_plane_state *state = update->plane->state;
	struct drm_rect src_bb;
	struct vmw_kms_sou_dirty_cmd *blit;
	SVGASignedRect *rect;
	uint32_t num_hits;
	int translate_src_x;
	int translate_src_y;
	int i;

	srf_update = container_of(update, typeof(*srf_update), base);

	blit = srf_update->cmd_start;
	rect = (SVGASignedRect *)&blit[1];

	num_hits = (blit->header.size - sizeof(blit->body))/
		sizeof(SVGASignedRect);

	src_bb = *bb;

	 
	translate_src_x = (state->src_x >> 16) - state->crtc_x;
	translate_src_y = (state->src_y >> 16) - state->crtc_y;

	drm_rect_translate(&src_bb, translate_src_x, translate_src_y);

	blit->body.srcRect.left = src_bb.x1;
	blit->body.srcRect.top = src_bb.y1;
	blit->body.srcRect.right = src_bb.x2;
	blit->body.srcRect.bottom = src_bb.y2;

	blit->body.destRect.left = bb->x1;
	blit->body.destRect.top = bb->y1;
	blit->body.destRect.right = bb->x2;
	blit->body.destRect.bottom = bb->y2;

	 
	for (i = 0; i < num_hits; i++) {
		rect->left -= bb->x1;
		rect->top -= bb->y1;
		rect->right -= bb->x1;
		rect->bottom -= bb->y1;
		rect++;
	}

	return 0;
}

 
static int vmw_sou_plane_update_surface(struct vmw_private *dev_priv,
					struct drm_plane *plane,
					struct drm_plane_state *old_state,
					struct vmw_framebuffer *vfb,
					struct vmw_fence_obj **out_fence)
{
	struct vmw_du_update_plane_surface srf_update;

	memset(&srf_update, 0, sizeof(struct vmw_du_update_plane_surface));
	srf_update.base.plane = plane;
	srf_update.base.old_state = old_state;
	srf_update.base.dev_priv = dev_priv;
	srf_update.base.du = vmw_crtc_to_du(plane->state->crtc);
	srf_update.base.vfb = vfb;
	srf_update.base.out_fence = out_fence;
	srf_update.base.mutex = &dev_priv->cmdbuf_mutex;
	srf_update.base.intr = true;

	srf_update.base.calc_fifo_size = vmw_sou_surface_fifo_size;
	srf_update.base.post_prepare = vmw_sou_surface_post_prepare;
	srf_update.base.pre_clip = vmw_sou_surface_pre_clip;
	srf_update.base.clip = vmw_sou_surface_clip_rect;
	srf_update.base.post_clip = vmw_sou_surface_post_clip;

	return vmw_du_helper_plane_update(&srf_update.base);
}

static void
vmw_sou_primary_plane_atomic_update(struct drm_plane *plane,
				    struct drm_atomic_state *state)
{
	struct drm_plane_state *old_state = drm_atomic_get_old_plane_state(state, plane);
	struct drm_plane_state *new_state = drm_atomic_get_new_plane_state(state, plane);
	struct drm_crtc *crtc = new_state->crtc;
	struct vmw_fence_obj *fence = NULL;
	int ret;

	 
	if (crtc && new_state->fb) {
		struct vmw_private *dev_priv = vmw_priv(crtc->dev);
		struct vmw_framebuffer *vfb =
			vmw_framebuffer_to_vfb(new_state->fb);

		if (vfb->bo)
			ret = vmw_sou_plane_update_bo(dev_priv, plane,
						      old_state, vfb, &fence);
		else
			ret = vmw_sou_plane_update_surface(dev_priv, plane,
							   old_state, vfb,
							   &fence);
		if (ret != 0)
			DRM_ERROR("Failed to update screen.\n");
	} else {
		 
		return;
	}

	if (fence)
		vmw_fence_obj_unreference(&fence);
}


static const struct drm_plane_funcs vmw_sou_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = vmw_du_primary_plane_destroy,
	.reset = vmw_du_plane_reset,
	.atomic_duplicate_state = vmw_du_plane_duplicate_state,
	.atomic_destroy_state = vmw_du_plane_destroy_state,
};

static const struct drm_plane_funcs vmw_sou_cursor_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = vmw_du_cursor_plane_destroy,
	.reset = vmw_du_plane_reset,
	.atomic_duplicate_state = vmw_du_plane_duplicate_state,
	.atomic_destroy_state = vmw_du_plane_destroy_state,
};

 
static const struct
drm_plane_helper_funcs vmw_sou_cursor_plane_helper_funcs = {
	.atomic_check = vmw_du_cursor_plane_atomic_check,
	.atomic_update = vmw_du_cursor_plane_atomic_update,
	.prepare_fb = vmw_du_cursor_plane_prepare_fb,
	.cleanup_fb = vmw_du_cursor_plane_cleanup_fb,
};

static const struct
drm_plane_helper_funcs vmw_sou_primary_plane_helper_funcs = {
	.atomic_check = vmw_du_primary_plane_atomic_check,
	.atomic_update = vmw_sou_primary_plane_atomic_update,
	.prepare_fb = vmw_sou_primary_plane_prepare_fb,
	.cleanup_fb = vmw_sou_primary_plane_cleanup_fb,
};

static const struct drm_crtc_helper_funcs vmw_sou_crtc_helper_funcs = {
	.prepare = vmw_sou_crtc_helper_prepare,
	.mode_set_nofb = vmw_sou_crtc_mode_set_nofb,
	.atomic_check = vmw_du_crtc_atomic_check,
	.atomic_begin = vmw_du_crtc_atomic_begin,
	.atomic_flush = vmw_du_crtc_atomic_flush,
	.atomic_enable = vmw_sou_crtc_atomic_enable,
	.atomic_disable = vmw_sou_crtc_atomic_disable,
};


static int vmw_sou_init(struct vmw_private *dev_priv, unsigned unit)
{
	struct vmw_screen_object_unit *sou;
	struct drm_device *dev = &dev_priv->drm;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct drm_plane *primary;
	struct vmw_cursor_plane *cursor;
	struct drm_crtc *crtc;
	int ret;

	sou = kzalloc(sizeof(*sou), GFP_KERNEL);
	if (!sou)
		return -ENOMEM;

	sou->base.unit = unit;
	crtc = &sou->base.crtc;
	encoder = &sou->base.encoder;
	connector = &sou->base.connector;
	primary = &sou->base.primary;
	cursor = &sou->base.cursor;

	sou->base.pref_active = (unit == 0);
	sou->base.pref_width = dev_priv->initial_width;
	sou->base.pref_height = dev_priv->initial_height;
	sou->base.pref_mode = NULL;

	 
	sou->base.is_implicit = false;

	 
	ret = drm_universal_plane_init(dev, primary,
				       0, &vmw_sou_plane_funcs,
				       vmw_primary_plane_formats,
				       ARRAY_SIZE(vmw_primary_plane_formats),
				       NULL, DRM_PLANE_TYPE_PRIMARY, NULL);
	if (ret) {
		DRM_ERROR("Failed to initialize primary plane");
		goto err_free;
	}

	drm_plane_helper_add(primary, &vmw_sou_primary_plane_helper_funcs);
	drm_plane_enable_fb_damage_clips(primary);

	 
	ret = drm_universal_plane_init(dev, &cursor->base,
			0, &vmw_sou_cursor_funcs,
			vmw_cursor_plane_formats,
			ARRAY_SIZE(vmw_cursor_plane_formats),
			NULL, DRM_PLANE_TYPE_CURSOR, NULL);
	if (ret) {
		DRM_ERROR("Failed to initialize cursor plane");
		drm_plane_cleanup(&sou->base.primary);
		goto err_free;
	}

	drm_plane_helper_add(&cursor->base, &vmw_sou_cursor_plane_helper_funcs);

	ret = drm_connector_init(dev, connector, &vmw_sou_connector_funcs,
				 DRM_MODE_CONNECTOR_VIRTUAL);
	if (ret) {
		DRM_ERROR("Failed to initialize connector\n");
		goto err_free;
	}

	drm_connector_helper_add(connector, &vmw_sou_connector_helper_funcs);
	connector->status = vmw_du_connector_detect(connector, true);

	ret = drm_encoder_init(dev, encoder, &vmw_screen_object_encoder_funcs,
			       DRM_MODE_ENCODER_VIRTUAL, NULL);
	if (ret) {
		DRM_ERROR("Failed to initialize encoder\n");
		goto err_free_connector;
	}

	(void) drm_connector_attach_encoder(connector, encoder);
	encoder->possible_crtcs = (1 << unit);
	encoder->possible_clones = 0;

	ret = drm_connector_register(connector);
	if (ret) {
		DRM_ERROR("Failed to register connector\n");
		goto err_free_encoder;
	}

	ret = drm_crtc_init_with_planes(dev, crtc, primary,
					&cursor->base,
					&vmw_screen_object_crtc_funcs, NULL);
	if (ret) {
		DRM_ERROR("Failed to initialize CRTC\n");
		goto err_free_unregister;
	}

	drm_crtc_helper_add(crtc, &vmw_sou_crtc_helper_funcs);

	drm_mode_crtc_set_gamma_size(crtc, 256);

	drm_object_attach_property(&connector->base,
				   dev_priv->hotplug_mode_update_property, 1);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.suggested_x_property, 0);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.suggested_y_property, 0);
	return 0;

err_free_unregister:
	drm_connector_unregister(connector);
err_free_encoder:
	drm_encoder_cleanup(encoder);
err_free_connector:
	drm_connector_cleanup(connector);
err_free:
	kfree(sou);
	return ret;
}

int vmw_kms_sou_init_display(struct vmw_private *dev_priv)
{
	struct drm_device *dev = &dev_priv->drm;
	int i;

	 
	if (!dev_priv->has_gmr)
		return -ENOSYS;

	if (!(dev_priv->capabilities & SVGA_CAP_SCREEN_OBJECT_2)) {
		return -ENOSYS;
	}

	for (i = 0; i < VMWGFX_NUM_DISPLAY_UNITS; ++i)
		vmw_sou_init(dev_priv, i);

	dev_priv->active_display_unit = vmw_du_screen_object;

	drm_mode_config_reset(dev);

	return 0;
}

static int do_bo_define_gmrfb(struct vmw_private *dev_priv,
				  struct vmw_framebuffer *framebuffer)
{
	struct vmw_bo *buf =
		container_of(framebuffer, struct vmw_framebuffer_bo,
			     base)->buffer;
	int depth = framebuffer->base.format->depth;
	struct {
		uint32_t header;
		SVGAFifoCmdDefineGMRFB body;
	} *cmd;

	 
	if (depth == 32)
		depth = 24;

	cmd = VMW_CMD_RESERVE(dev_priv, sizeof(*cmd));
	if (!cmd)
		return -ENOMEM;

	cmd->header = SVGA_CMD_DEFINE_GMRFB;
	cmd->body.format.bitsPerPixel = framebuffer->base.format->cpp[0] * 8;
	cmd->body.format.colorDepth = depth;
	cmd->body.format.reserved = 0;
	cmd->body.bytesPerLine = framebuffer->base.pitches[0];
	 
	vmw_bo_get_guest_ptr(&buf->tbo, &cmd->body.ptr);
	vmw_cmd_commit(dev_priv, sizeof(*cmd));

	return 0;
}

 
static void vmw_sou_surface_fifo_commit(struct vmw_kms_dirty *dirty)
{
	struct vmw_kms_sou_surface_dirty *sdirty =
		container_of(dirty, typeof(*sdirty), base);
	struct vmw_kms_sou_dirty_cmd *cmd = dirty->cmd;
	s32 trans_x = dirty->unit->crtc.x - sdirty->dst_x;
	s32 trans_y = dirty->unit->crtc.y - sdirty->dst_y;
	size_t region_size = dirty->num_hits * sizeof(SVGASignedRect);
	SVGASignedRect *blit = (SVGASignedRect *) &cmd[1];
	int i;

	if (!dirty->num_hits) {
		vmw_cmd_commit(dirty->dev_priv, 0);
		return;
	}

	cmd->header.id = SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN;
	cmd->header.size = sizeof(cmd->body) + region_size;

	 
	cmd->body.destRect.left = sdirty->left;
	cmd->body.destRect.right = sdirty->right;
	cmd->body.destRect.top = sdirty->top;
	cmd->body.destRect.bottom = sdirty->bottom;

	cmd->body.srcRect.left = sdirty->left + trans_x;
	cmd->body.srcRect.right = sdirty->right + trans_x;
	cmd->body.srcRect.top = sdirty->top + trans_y;
	cmd->body.srcRect.bottom = sdirty->bottom + trans_y;

	cmd->body.srcImage.sid = sdirty->sid;
	cmd->body.destScreenId = dirty->unit->unit;

	 
	for (i = 0; i < dirty->num_hits; ++i, ++blit) {
		blit->left -= sdirty->left;
		blit->right -= sdirty->left;
		blit->top -= sdirty->top;
		blit->bottom -= sdirty->top;
	}

	vmw_cmd_commit(dirty->dev_priv, region_size + sizeof(*cmd));

	sdirty->left = sdirty->top = S32_MAX;
	sdirty->right = sdirty->bottom = S32_MIN;
}

 
static void vmw_sou_surface_clip(struct vmw_kms_dirty *dirty)
{
	struct vmw_kms_sou_surface_dirty *sdirty =
		container_of(dirty, typeof(*sdirty), base);
	struct vmw_kms_sou_dirty_cmd *cmd = dirty->cmd;
	SVGASignedRect *blit = (SVGASignedRect *) &cmd[1];

	 
	blit += dirty->num_hits;
	blit->left = dirty->unit_x1;
	blit->top = dirty->unit_y1;
	blit->right = dirty->unit_x2;
	blit->bottom = dirty->unit_y2;

	 
	sdirty->left = min_t(s32, sdirty->left, dirty->unit_x1);
	sdirty->top = min_t(s32, sdirty->top, dirty->unit_y1);
	sdirty->right = max_t(s32, sdirty->right, dirty->unit_x2);
	sdirty->bottom = max_t(s32, sdirty->bottom, dirty->unit_y2);

	dirty->num_hits++;
}

 
int vmw_kms_sou_do_surface_dirty(struct vmw_private *dev_priv,
				 struct vmw_framebuffer *framebuffer,
				 struct drm_clip_rect *clips,
				 struct drm_vmw_rect *vclips,
				 struct vmw_resource *srf,
				 s32 dest_x,
				 s32 dest_y,
				 unsigned num_clips, int inc,
				 struct vmw_fence_obj **out_fence,
				 struct drm_crtc *crtc)
{
	struct vmw_framebuffer_surface *vfbs =
		container_of(framebuffer, typeof(*vfbs), base);
	struct vmw_kms_sou_surface_dirty sdirty;
	DECLARE_VAL_CONTEXT(val_ctx, NULL, 0);
	int ret;

	if (!srf)
		srf = &vfbs->surface->res;

	ret = vmw_validation_add_resource(&val_ctx, srf, 0, VMW_RES_DIRTY_NONE,
					  NULL, NULL);
	if (ret)
		return ret;

	ret = vmw_validation_prepare(&val_ctx, &dev_priv->cmdbuf_mutex, true);
	if (ret)
		goto out_unref;

	sdirty.base.fifo_commit = vmw_sou_surface_fifo_commit;
	sdirty.base.clip = vmw_sou_surface_clip;
	sdirty.base.dev_priv = dev_priv;
	sdirty.base.fifo_reserve_size = sizeof(struct vmw_kms_sou_dirty_cmd) +
	  sizeof(SVGASignedRect) * num_clips;
	sdirty.base.crtc = crtc;

	sdirty.sid = srf->id;
	sdirty.left = sdirty.top = S32_MAX;
	sdirty.right = sdirty.bottom = S32_MIN;
	sdirty.dst_x = dest_x;
	sdirty.dst_y = dest_y;

	ret = vmw_kms_helper_dirty(dev_priv, framebuffer, clips, vclips,
				   dest_x, dest_y, num_clips, inc,
				   &sdirty.base);
	vmw_kms_helper_validation_finish(dev_priv, NULL, &val_ctx, out_fence,
					 NULL);

	return ret;

out_unref:
	vmw_validation_unref_lists(&val_ctx);
	return ret;
}

 
static void vmw_sou_bo_fifo_commit(struct vmw_kms_dirty *dirty)
{
	if (!dirty->num_hits) {
		vmw_cmd_commit(dirty->dev_priv, 0);
		return;
	}

	vmw_cmd_commit(dirty->dev_priv,
			sizeof(struct vmw_kms_sou_bo_blit) *
			dirty->num_hits);
}

 
static void vmw_sou_bo_clip(struct vmw_kms_dirty *dirty)
{
	struct vmw_kms_sou_bo_blit *blit = dirty->cmd;

	blit += dirty->num_hits;
	blit->header = SVGA_CMD_BLIT_GMRFB_TO_SCREEN;
	blit->body.destScreenId = dirty->unit->unit;
	blit->body.srcOrigin.x = dirty->fb_x;
	blit->body.srcOrigin.y = dirty->fb_y;
	blit->body.destRect.left = dirty->unit_x1;
	blit->body.destRect.top = dirty->unit_y1;
	blit->body.destRect.right = dirty->unit_x2;
	blit->body.destRect.bottom = dirty->unit_y2;
	dirty->num_hits++;
}

 
int vmw_kms_sou_do_bo_dirty(struct vmw_private *dev_priv,
				struct vmw_framebuffer *framebuffer,
				struct drm_clip_rect *clips,
				struct drm_vmw_rect *vclips,
				unsigned num_clips, int increment,
				bool interruptible,
				struct vmw_fence_obj **out_fence,
				struct drm_crtc *crtc)
{
	struct vmw_bo *buf =
		container_of(framebuffer, struct vmw_framebuffer_bo,
			     base)->buffer;
	struct vmw_kms_dirty dirty;
	DECLARE_VAL_CONTEXT(val_ctx, NULL, 0);
	int ret;

	vmw_bo_placement_set(buf, VMW_BO_DOMAIN_GMR | VMW_BO_DOMAIN_VRAM,
			     VMW_BO_DOMAIN_GMR | VMW_BO_DOMAIN_VRAM);
	ret = vmw_validation_add_bo(&val_ctx, buf);
	if (ret)
		return ret;

	ret = vmw_validation_prepare(&val_ctx, NULL, interruptible);
	if (ret)
		goto out_unref;

	ret = do_bo_define_gmrfb(dev_priv, framebuffer);
	if (unlikely(ret != 0))
		goto out_revert;

	dirty.crtc = crtc;
	dirty.fifo_commit = vmw_sou_bo_fifo_commit;
	dirty.clip = vmw_sou_bo_clip;
	dirty.fifo_reserve_size = sizeof(struct vmw_kms_sou_bo_blit) *
		num_clips;
	ret = vmw_kms_helper_dirty(dev_priv, framebuffer, clips, vclips,
				   0, 0, num_clips, increment, &dirty);
	vmw_kms_helper_validation_finish(dev_priv, NULL, &val_ctx, out_fence,
					 NULL);

	return ret;

out_revert:
	vmw_validation_revert(&val_ctx);
out_unref:
	vmw_validation_unref_lists(&val_ctx);

	return ret;
}


 
static void vmw_sou_readback_fifo_commit(struct vmw_kms_dirty *dirty)
{
	if (!dirty->num_hits) {
		vmw_cmd_commit(dirty->dev_priv, 0);
		return;
	}

	vmw_cmd_commit(dirty->dev_priv,
			sizeof(struct vmw_kms_sou_readback_blit) *
			dirty->num_hits);
}

 
static void vmw_sou_readback_clip(struct vmw_kms_dirty *dirty)
{
	struct vmw_kms_sou_readback_blit *blit = dirty->cmd;

	blit += dirty->num_hits;
	blit->header = SVGA_CMD_BLIT_SCREEN_TO_GMRFB;
	blit->body.srcScreenId = dirty->unit->unit;
	blit->body.destOrigin.x = dirty->fb_x;
	blit->body.destOrigin.y = dirty->fb_y;
	blit->body.srcRect.left = dirty->unit_x1;
	blit->body.srcRect.top = dirty->unit_y1;
	blit->body.srcRect.right = dirty->unit_x2;
	blit->body.srcRect.bottom = dirty->unit_y2;
	dirty->num_hits++;
}

 
int vmw_kms_sou_readback(struct vmw_private *dev_priv,
			 struct drm_file *file_priv,
			 struct vmw_framebuffer *vfb,
			 struct drm_vmw_fence_rep __user *user_fence_rep,
			 struct drm_vmw_rect *vclips,
			 uint32_t num_clips,
			 struct drm_crtc *crtc)
{
	struct vmw_bo *buf =
		container_of(vfb, struct vmw_framebuffer_bo, base)->buffer;
	struct vmw_kms_dirty dirty;
	DECLARE_VAL_CONTEXT(val_ctx, NULL, 0);
	int ret;

	vmw_bo_placement_set(buf, VMW_BO_DOMAIN_GMR | VMW_BO_DOMAIN_VRAM,
			     VMW_BO_DOMAIN_GMR | VMW_BO_DOMAIN_VRAM);
	ret = vmw_validation_add_bo(&val_ctx, buf);
	if (ret)
		return ret;

	ret = vmw_validation_prepare(&val_ctx, NULL, true);
	if (ret)
		goto out_unref;

	ret = do_bo_define_gmrfb(dev_priv, vfb);
	if (unlikely(ret != 0))
		goto out_revert;

	dirty.crtc = crtc;
	dirty.fifo_commit = vmw_sou_readback_fifo_commit;
	dirty.clip = vmw_sou_readback_clip;
	dirty.fifo_reserve_size = sizeof(struct vmw_kms_sou_readback_blit) *
		num_clips;
	ret = vmw_kms_helper_dirty(dev_priv, vfb, NULL, vclips,
				   0, 0, num_clips, 1, &dirty);
	vmw_kms_helper_validation_finish(dev_priv, file_priv, &val_ctx, NULL,
					 user_fence_rep);

	return ret;

out_revert:
	vmw_validation_revert(&val_ctx);
out_unref:
	vmw_validation_unref_lists(&val_ctx);

	return ret;
}
