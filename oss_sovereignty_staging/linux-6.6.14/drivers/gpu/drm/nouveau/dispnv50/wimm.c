 
#include "wimm.h"

#include <nvif/class.h>

int
nv50_wimm_init(struct nouveau_drm *drm, struct nv50_wndw *wndw)
{
	struct {
		s32 oclass;
		int version;
		int (*init)(struct nouveau_drm *, s32, struct nv50_wndw *);
	} wimms[] = {
		{ GA102_DISP_WINDOW_IMM_CHANNEL_DMA, 0, wimmc37b_init },
		{ TU102_DISP_WINDOW_IMM_CHANNEL_DMA, 0, wimmc37b_init },
		{ GV100_DISP_WINDOW_IMM_CHANNEL_DMA, 0, wimmc37b_init },
		{}
	};
	struct nv50_disp *disp = nv50_disp(drm->dev);
	int cid;

	cid = nvif_mclass(&disp->disp->object, wimms);
	if (cid < 0) {
		NV_ERROR(drm, "No supported window immediate class\n");
		return cid;
	}

	return wimms[cid].init(drm, wimms[cid].oclass, wndw);
}
