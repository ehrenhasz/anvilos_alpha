 

#ifndef __NOUVEAU_CRTC_H__
#define __NOUVEAU_CRTC_H__
#include <drm/drm_crtc.h>

#include <nvif/head.h>
#include <nvif/event.h>

struct nouveau_crtc {
	struct drm_crtc base;

	struct nvif_head head;
	int index;
	struct nvif_event vblank;

	uint32_t dpms_saved_fp_control;
	uint32_t fp_users;
	int saturation;
	int sharpness;
	int last_dpms;

	int cursor_saved_x, cursor_saved_y;

	struct {
		int cpp;
		bool blanked;
		uint32_t offset;
		uint32_t handle;
	} fb;

	struct {
		struct nouveau_bo *nvbo;
		uint32_t offset;
		void (*set_offset)(struct nouveau_crtc *, uint32_t offset);
		void (*set_pos)(struct nouveau_crtc *, int x, int y);
		void (*hide)(struct nouveau_crtc *, bool update);
		void (*show)(struct nouveau_crtc *, bool update);
	} cursor;

	struct {
		int depth;
	} lut;

	void (*save)(struct drm_crtc *crtc);
	void (*restore)(struct drm_crtc *crtc);
};

static inline struct nouveau_crtc *nouveau_crtc(struct drm_crtc *crtc)
{
	return crtc ? container_of(crtc, struct nouveau_crtc, base) : NULL;
}

static inline struct drm_crtc *to_drm_crtc(struct nouveau_crtc *crtc)
{
	return &crtc->base;
}

int nv04_cursor_init(struct nouveau_crtc *);

#endif  
