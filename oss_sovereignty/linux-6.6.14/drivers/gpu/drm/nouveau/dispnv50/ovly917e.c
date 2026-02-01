 
#include "ovly.h"

static const u32
ovly917e_format[] = {
	DRM_FORMAT_YUYV,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_XBGR2101010,
	DRM_FORMAT_XRGB2101010,
	DRM_FORMAT_XBGR16161616F,
	0
};

int
ovly917e_new(struct nouveau_drm *drm, int head, s32 oclass,
	     struct nv50_wndw **pwndw)
{
	return ovly507e_new_(&ovly907e, ovly917e_format, drm, head, oclass,
			     0x00000004 << (head * 4), pwndw);
}
