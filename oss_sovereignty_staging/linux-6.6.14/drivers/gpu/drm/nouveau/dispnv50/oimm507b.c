 
#include "oimm.h"

#include <nvif/if0014.h>

static int
oimm507b_init_(const struct nv50_wimm_func *func, struct nouveau_drm *drm,
	       s32 oclass, struct nv50_wndw *wndw)
{
	struct nvif_disp_chan_v0 args = {
		.id = wndw->id,
	};
	struct nv50_disp *disp = nv50_disp(drm->dev);
	int ret;

	ret = nvif_object_ctor(&disp->disp->object, "kmsOvim", 0, oclass,
			       &args, sizeof(args), &wndw->wimm.base.user);
	if (ret) {
		NV_ERROR(drm, "oimm%04x allocation failed: %d\n", oclass, ret);
		return ret;
	}

	nvif_object_map(&wndw->wimm.base.user, NULL, 0);
	wndw->immd = func;
	return 0;
}

int
oimm507b_init(struct nouveau_drm *drm, s32 oclass, struct nv50_wndw *wndw)
{
	return oimm507b_init_(&curs507a, drm, oclass, wndw);
}
