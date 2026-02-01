 
#include "curs.h"
#include "atom.h"

#include <nvhw/class/clc37a.h>

static int
cursc37a_update(struct nv50_wndw *wndw, u32 *interlock)
{
	struct nvif_object *user = &wndw->wimm.base.user;
	int ret = nvif_chan_wait(&wndw->wimm, 1);
	if (ret == 0)
		NVIF_WR32(user, NVC37A, UPDATE, 0x00000001);
	return ret;
}

static int
cursc37a_point(struct nv50_wndw *wndw, struct nv50_wndw_atom *asyw)
{
	struct nvif_object *user = &wndw->wimm.base.user;
	int ret = nvif_chan_wait(&wndw->wimm, 1);
	if (ret == 0) {
		NVIF_WR32(user, NVC37A, SET_CURSOR_HOT_SPOT_POINT_OUT(0),
			  NVVAL(NVC37A, SET_CURSOR_HOT_SPOT_POINT_OUT, X, asyw->point.x) |
			  NVVAL(NVC37A, SET_CURSOR_HOT_SPOT_POINT_OUT, Y, asyw->point.y));
	}
	return ret;
}

static const struct nv50_wimm_func
cursc37a = {
	.point = cursc37a_point,
	.update = cursc37a_update,
};

int
cursc37a_new(struct nouveau_drm *drm, int head, s32 oclass,
	     struct nv50_wndw **pwndw)
{
	return curs507a_new_(&cursc37a, drm, head, oclass,
			     0x00000001 << head, pwndw);
}
