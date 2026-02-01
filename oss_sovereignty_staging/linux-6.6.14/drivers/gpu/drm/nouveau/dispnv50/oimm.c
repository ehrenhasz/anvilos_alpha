 
#include "oimm.h"

#include <nvif/class.h>

int
nv50_oimm_init(struct nouveau_drm *drm, struct nv50_wndw *wndw)
{
	static const struct {
		s32 oclass;
		int version;
		int (*init)(struct nouveau_drm *, s32, struct nv50_wndw *);
	} oimms[] = {
		{ GK104_DISP_OVERLAY, 0, oimm507b_init },
		{ GF110_DISP_OVERLAY, 0, oimm507b_init },
		{ GT214_DISP_OVERLAY, 0, oimm507b_init },
		{   G82_DISP_OVERLAY, 0, oimm507b_init },
		{  NV50_DISP_OVERLAY, 0, oimm507b_init },
		{}
	};
	struct nv50_disp *disp = nv50_disp(drm->dev);
	int cid;

	cid = nvif_mclass(&disp->disp->object, oimms);
	if (cid < 0) {
		NV_ERROR(drm, "No supported overlay immediate class\n");
		return cid;
	}

	return oimms[cid].init(drm, oimms[cid].oclass, wndw);
}
