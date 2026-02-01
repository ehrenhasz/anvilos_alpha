 
 

#ifndef VMWGFX_KMS_H_
#define VMWGFX_KMS_H_

#include <drm/drm_encoder.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_probe_helper.h>

#include "vmwgfx_drv.h"

 
struct vmw_du_update_plane {
	 
	uint32_t (*calc_fifo_size)(struct vmw_du_update_plane *update,
				   uint32_t num_hits);

	 
	uint32_t (*post_prepare)(struct vmw_du_update_plane *update, void *cmd);

	 
	uint32_t (*pre_clip)(struct vmw_du_update_plane *update, void *cmd,
			     uint32_t num_hits);

	 
	uint32_t (*clip)(struct vmw_du_update_plane *update, void *cmd,
			 struct drm_rect *clip, uint32_t src_x, uint32_t src_y);

	 
	uint32_t (*post_clip)(struct vmw_du_update_plane *update, void *cmd,
				    struct drm_rect *bb);

	struct drm_plane *plane;
	struct drm_plane_state *old_state;
	struct vmw_private *dev_priv;
	struct vmw_display_unit *du;
	struct vmw_framebuffer *vfb;
	struct vmw_fence_obj **out_fence;
	struct mutex *mutex;
	bool intr;
};

 
struct vmw_du_update_plane_surface {
	struct vmw_du_update_plane base;
	 
	void *cmd_start;
};

 
struct vmw_du_update_plane_buffer {
	struct vmw_du_update_plane base;
	int fb_left, fb_top;
};

 
struct vmw_kms_dirty {
	void (*fifo_commit)(struct vmw_kms_dirty *);
	void (*clip)(struct vmw_kms_dirty *);
	size_t fifo_reserve_size;
	struct vmw_private *dev_priv;
	struct vmw_display_unit *unit;
	void *cmd;
	struct drm_crtc *crtc;
	u32 num_hits;
	s32 fb_x;
	s32 fb_y;
	s32 unit_x1;
	s32 unit_y1;
	s32 unit_x2;
	s32 unit_y2;
};

#define VMWGFX_NUM_DISPLAY_UNITS 8


#define vmw_framebuffer_to_vfb(x) \
	container_of(x, struct vmw_framebuffer, base)
#define vmw_framebuffer_to_vfbs(x) \
	container_of(x, struct vmw_framebuffer_surface, base.base)
#define vmw_framebuffer_to_vfbd(x) \
	container_of(x, struct vmw_framebuffer_bo, base.base)

 
struct vmw_framebuffer {
	struct drm_framebuffer base;
	bool bo;
	uint32_t user_handle;
};

 
struct vmw_clip_rect {
	int x1, x2, y1, y2;
};

struct vmw_framebuffer_surface {
	struct vmw_framebuffer base;
	struct vmw_surface *surface;
	struct vmw_bo *buffer;
	struct list_head head;
	bool is_bo_proxy;   
};


struct vmw_framebuffer_bo {
	struct vmw_framebuffer base;
	struct vmw_bo *buffer;
};


static const uint32_t __maybe_unused vmw_primary_plane_formats[] = {
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
};

static const uint32_t __maybe_unused vmw_cursor_plane_formats[] = {
	DRM_FORMAT_ARGB8888,
};


#define vmw_crtc_state_to_vcs(x) container_of(x, struct vmw_crtc_state, base)
#define vmw_plane_state_to_vps(x) container_of(x, struct vmw_plane_state, base)
#define vmw_connector_state_to_vcs(x) \
		container_of(x, struct vmw_connector_state, base)
#define vmw_plane_to_vcp(x) container_of(x, struct vmw_cursor_plane, base)

 
struct vmw_crtc_state {
	struct drm_crtc_state base;
};

struct vmw_cursor_plane_state {
	struct vmw_bo *bo;
	s32 hotspot_x;
	s32 hotspot_y;
};

 
struct vmw_plane_state {
	struct drm_plane_state base;
	struct vmw_surface *surf;
	struct vmw_bo *bo;

	int content_fb_type;
	unsigned long bo_size;

	int pinned;

	 
	unsigned int cpp;

	bool surf_mapped;
	struct vmw_cursor_plane_state cursor;
};


 
struct vmw_connector_state {
	struct drm_connector_state base;

	 
	int gui_x;

	 
	int gui_y;
};

 
struct vmw_cursor_plane {
	struct drm_plane base;

	struct vmw_bo *cursor_mobs[3];
};

 
struct vmw_display_unit {
	struct drm_crtc crtc;
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct drm_plane primary;
	struct vmw_cursor_plane cursor;

	struct vmw_surface *cursor_surface;
	struct vmw_bo *cursor_bo;
	size_t cursor_age;

	int cursor_x;
	int cursor_y;

	int hotspot_x;
	int hotspot_y;
	s32 core_hotspot_x;
	s32 core_hotspot_y;

	unsigned unit;

	 
	unsigned pref_width;
	unsigned pref_height;
	bool pref_active;
	struct drm_display_mode *pref_mode;

	 
	int gui_x;
	int gui_y;
	bool is_implicit;
	int set_gui_x;
	int set_gui_y;
};

struct vmw_validation_ctx {
	struct vmw_resource *res;
	struct vmw_bo *buf;
};

#define vmw_crtc_to_du(x) \
	container_of(x, struct vmw_display_unit, crtc)
#define vmw_connector_to_du(x) \
	container_of(x, struct vmw_display_unit, connector)


 
void vmw_du_cleanup(struct vmw_display_unit *du);
void vmw_du_crtc_save(struct drm_crtc *crtc);
void vmw_du_crtc_restore(struct drm_crtc *crtc);
int vmw_du_crtc_gamma_set(struct drm_crtc *crtc,
			   u16 *r, u16 *g, u16 *b,
			   uint32_t size,
			   struct drm_modeset_acquire_ctx *ctx);
int vmw_du_connector_set_property(struct drm_connector *connector,
				  struct drm_property *property,
				  uint64_t val);
int vmw_du_connector_atomic_set_property(struct drm_connector *connector,
					 struct drm_connector_state *state,
					 struct drm_property *property,
					 uint64_t val);
int
vmw_du_connector_atomic_get_property(struct drm_connector *connector,
				     const struct drm_connector_state *state,
				     struct drm_property *property,
				     uint64_t *val);
int vmw_du_connector_dpms(struct drm_connector *connector, int mode);
void vmw_du_connector_save(struct drm_connector *connector);
void vmw_du_connector_restore(struct drm_connector *connector);
enum drm_connector_status
vmw_du_connector_detect(struct drm_connector *connector, bool force);
int vmw_du_connector_fill_modes(struct drm_connector *connector,
				uint32_t max_width, uint32_t max_height);
int vmw_kms_helper_dirty(struct vmw_private *dev_priv,
			 struct vmw_framebuffer *framebuffer,
			 const struct drm_clip_rect *clips,
			 const struct drm_vmw_rect *vclips,
			 s32 dest_x, s32 dest_y,
			 int num_clips,
			 int increment,
			 struct vmw_kms_dirty *dirty);

void vmw_kms_helper_validation_finish(struct vmw_private *dev_priv,
				      struct drm_file *file_priv,
				      struct vmw_validation_context *ctx,
				      struct vmw_fence_obj **out_fence,
				      struct drm_vmw_fence_rep __user *
				      user_fence_rep);
int vmw_kms_readback(struct vmw_private *dev_priv,
		     struct drm_file *file_priv,
		     struct vmw_framebuffer *vfb,
		     struct drm_vmw_fence_rep __user *user_fence_rep,
		     struct drm_vmw_rect *vclips,
		     uint32_t num_clips);
struct vmw_framebuffer *
vmw_kms_new_framebuffer(struct vmw_private *dev_priv,
			struct vmw_bo *bo,
			struct vmw_surface *surface,
			bool only_2d,
			const struct drm_mode_fb_cmd2 *mode_cmd);
void vmw_guess_mode_timing(struct drm_display_mode *mode);
void vmw_kms_update_implicit_fb(struct vmw_private *dev_priv);
void vmw_kms_create_implicit_placement_property(struct vmw_private *dev_priv);

 
void vmw_du_primary_plane_destroy(struct drm_plane *plane);
void vmw_du_cursor_plane_destroy(struct drm_plane *plane);

 
int vmw_du_primary_plane_atomic_check(struct drm_plane *plane,
				      struct drm_atomic_state *state);
int vmw_du_cursor_plane_atomic_check(struct drm_plane *plane,
				     struct drm_atomic_state *state);
void vmw_du_cursor_plane_atomic_update(struct drm_plane *plane,
				       struct drm_atomic_state *state);
int vmw_du_cursor_plane_prepare_fb(struct drm_plane *plane,
				   struct drm_plane_state *new_state);
void vmw_du_cursor_plane_cleanup_fb(struct drm_plane *plane,
			     struct drm_plane_state *old_state);
void vmw_du_plane_cleanup_fb(struct drm_plane *plane,
			     struct drm_plane_state *old_state);
void vmw_du_plane_reset(struct drm_plane *plane);
struct drm_plane_state *vmw_du_plane_duplicate_state(struct drm_plane *plane);
void vmw_du_plane_destroy_state(struct drm_plane *plane,
				struct drm_plane_state *state);
void vmw_du_plane_unpin_surf(struct vmw_plane_state *vps,
			     bool unreference);

int vmw_du_crtc_atomic_check(struct drm_crtc *crtc,
			     struct drm_atomic_state *state);
void vmw_du_crtc_atomic_begin(struct drm_crtc *crtc,
			      struct drm_atomic_state *state);
void vmw_du_crtc_atomic_flush(struct drm_crtc *crtc,
			      struct drm_atomic_state *state);
void vmw_du_crtc_reset(struct drm_crtc *crtc);
struct drm_crtc_state *vmw_du_crtc_duplicate_state(struct drm_crtc *crtc);
void vmw_du_crtc_destroy_state(struct drm_crtc *crtc,
				struct drm_crtc_state *state);
void vmw_du_connector_reset(struct drm_connector *connector);
struct drm_connector_state *
vmw_du_connector_duplicate_state(struct drm_connector *connector);

void vmw_du_connector_destroy_state(struct drm_connector *connector,
				    struct drm_connector_state *state);

 
int vmw_kms_ldu_init_display(struct vmw_private *dev_priv);
int vmw_kms_ldu_close_display(struct vmw_private *dev_priv);
int vmw_kms_update_proxy(struct vmw_resource *res,
			 const struct drm_clip_rect *clips,
			 unsigned num_clips,
			 int increment);

 
int vmw_kms_sou_init_display(struct vmw_private *dev_priv);
int vmw_kms_sou_do_surface_dirty(struct vmw_private *dev_priv,
				 struct vmw_framebuffer *framebuffer,
				 struct drm_clip_rect *clips,
				 struct drm_vmw_rect *vclips,
				 struct vmw_resource *srf,
				 s32 dest_x,
				 s32 dest_y,
				 unsigned num_clips, int inc,
				 struct vmw_fence_obj **out_fence,
				 struct drm_crtc *crtc);
int vmw_kms_sou_do_bo_dirty(struct vmw_private *dev_priv,
			    struct vmw_framebuffer *framebuffer,
			    struct drm_clip_rect *clips,
			    struct drm_vmw_rect *vclips,
			    unsigned int num_clips, int increment,
			    bool interruptible,
			    struct vmw_fence_obj **out_fence,
			    struct drm_crtc *crtc);
int vmw_kms_sou_readback(struct vmw_private *dev_priv,
			 struct drm_file *file_priv,
			 struct vmw_framebuffer *vfb,
			 struct drm_vmw_fence_rep __user *user_fence_rep,
			 struct drm_vmw_rect *vclips,
			 uint32_t num_clips,
			 struct drm_crtc *crtc);

 
int vmw_kms_stdu_init_display(struct vmw_private *dev_priv);
int vmw_kms_stdu_surface_dirty(struct vmw_private *dev_priv,
			       struct vmw_framebuffer *framebuffer,
			       struct drm_clip_rect *clips,
			       struct drm_vmw_rect *vclips,
			       struct vmw_resource *srf,
			       s32 dest_x,
			       s32 dest_y,
			       unsigned num_clips, int inc,
			       struct vmw_fence_obj **out_fence,
			       struct drm_crtc *crtc);
int vmw_kms_stdu_readback(struct vmw_private *dev_priv,
			  struct drm_file *file_priv,
			  struct vmw_framebuffer *vfb,
			  struct drm_vmw_fence_rep __user *user_fence_rep,
			  struct drm_clip_rect *clips,
			  struct drm_vmw_rect *vclips,
			  uint32_t num_clips,
			  int increment,
			  struct drm_crtc *crtc);

int vmw_du_helper_plane_update(struct vmw_du_update_plane *update);

 
static inline void vmw_du_translate_to_crtc(struct drm_plane_state *state,
					    struct drm_rect *r)
{
	int translate_crtc_x = -((state->src_x >> 16) - state->crtc_x);
	int translate_crtc_y = -((state->src_y >> 16) - state->crtc_y);

	drm_rect_translate(r, translate_crtc_x, translate_crtc_y);
}

#endif
