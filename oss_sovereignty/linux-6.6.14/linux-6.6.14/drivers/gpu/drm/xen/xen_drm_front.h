#ifndef __XEN_DRM_FRONT_H_
#define __XEN_DRM_FRONT_H_
#include <linux/scatterlist.h>
#include <drm/drm_connector.h>
#include <drm/drm_simple_kms_helper.h>
#include "xen_drm_front_cfg.h"
struct drm_device;
struct drm_framebuffer;
struct drm_gem_object;
struct drm_pending_vblank_event;
#define XEN_DRM_FRONT_WAIT_BACK_MS	3000
struct xen_drm_front_info {
	struct xenbus_device *xb_dev;
	struct xen_drm_front_drm_info *drm_info;
	spinlock_t io_lock;
	int num_evt_pairs;
	struct xen_drm_front_evtchnl_pair *evt_pairs;
	struct xen_drm_front_cfg cfg;
	struct list_head dbuf_list;
};
struct xen_drm_front_drm_pipeline {
	struct xen_drm_front_drm_info *drm_info;
	int index;
	struct drm_simple_display_pipe pipe;
	struct drm_connector conn;
	int width, height;
	struct drm_pending_vblank_event *pending_event;
	struct delayed_work pflip_to_worker;
	bool conn_connected;
};
struct xen_drm_front_drm_info {
	struct xen_drm_front_info *front_info;
	struct drm_device *drm_dev;
	struct xen_drm_front_drm_pipeline pipeline[XEN_DRM_FRONT_MAX_CRTCS];
};
static inline u64 xen_drm_front_fb_to_cookie(struct drm_framebuffer *fb)
{
	return (uintptr_t)fb;
}
static inline u64 xen_drm_front_dbuf_to_cookie(struct drm_gem_object *gem_obj)
{
	return (uintptr_t)gem_obj;
}
int xen_drm_front_mode_set(struct xen_drm_front_drm_pipeline *pipeline,
			   u32 x, u32 y, u32 width, u32 height,
			   u32 bpp, u64 fb_cookie);
int xen_drm_front_dbuf_create(struct xen_drm_front_info *front_info,
			      u64 dbuf_cookie, u32 width, u32 height,
			      u32 bpp, u64 size, u32 offset, struct page **pages);
int xen_drm_front_fb_attach(struct xen_drm_front_info *front_info,
			    u64 dbuf_cookie, u64 fb_cookie, u32 width,
			    u32 height, u32 pixel_format);
int xen_drm_front_fb_detach(struct xen_drm_front_info *front_info,
			    u64 fb_cookie);
int xen_drm_front_page_flip(struct xen_drm_front_info *front_info,
			    int conn_idx, u64 fb_cookie);
void xen_drm_front_on_frame_done(struct xen_drm_front_info *front_info,
				 int conn_idx, u64 fb_cookie);
void xen_drm_front_gem_object_free(struct drm_gem_object *obj);
#endif  
