 
 
#ifndef _KOMEDA_FRAMEBUFFER_H_
#define _KOMEDA_FRAMEBUFFER_H_

#include <drm/drm_framebuffer.h>
#include "komeda_format_caps.h"

 
struct komeda_fb {
	 
	struct drm_framebuffer base;
	 
	const struct komeda_format_caps *format_caps;
	 
	bool is_va;
	 
	u32 aligned_w;
	 
	u32 aligned_h;
	 
	u32 afbc_size;
	 
	u32 offset_payload;
};

#define to_kfb(dfb)	container_of(dfb, struct komeda_fb, base)

struct drm_framebuffer *
komeda_fb_create(struct drm_device *dev, struct drm_file *file,
		const struct drm_mode_fb_cmd2 *mode_cmd);
int komeda_fb_check_src_coords(const struct komeda_fb *kfb,
			       u32 src_x, u32 src_y, u32 src_w, u32 src_h);
dma_addr_t
komeda_fb_get_pixel_addr(struct komeda_fb *kfb, int x, int y, int plane);
bool komeda_fb_is_layer_supported(struct komeda_fb *kfb, u32 layer_type,
		u32 rot);

#endif
