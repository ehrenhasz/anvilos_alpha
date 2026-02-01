 
#include "base.h"

#include <nvif/class.h>

int
nv50_base_new(struct nouveau_drm *drm, int head, struct nv50_wndw **pwndw)
{
	struct {
		s32 oclass;
		int version;
		int (*new)(struct nouveau_drm *, int, s32, struct nv50_wndw **);
	} bases[] = {
		{ GK110_DISP_BASE_CHANNEL_DMA, 0, base917c_new },
		{ GK104_DISP_BASE_CHANNEL_DMA, 0, base917c_new },
		{ GF110_DISP_BASE_CHANNEL_DMA, 0, base907c_new },
		{ GT214_DISP_BASE_CHANNEL_DMA, 0, base827c_new },
		{ GT200_DISP_BASE_CHANNEL_DMA, 0, base827c_new },
		{   G82_DISP_BASE_CHANNEL_DMA, 0, base827c_new },
		{  NV50_DISP_BASE_CHANNEL_DMA, 0, base507c_new },
		{}
	};
	struct nv50_disp *disp = nv50_disp(drm->dev);
	int cid;

	cid = nvif_mclass(&disp->disp->object, bases);
	if (cid < 0) {
		NV_ERROR(drm, "No supported base class\n");
		return cid;
	}

	return bases[cid].new(drm, head, bases[cid].oclass, pwndw);
}
