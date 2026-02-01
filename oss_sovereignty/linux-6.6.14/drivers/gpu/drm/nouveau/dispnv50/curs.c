 
#include "curs.h"

#include <nvif/class.h>

int
nv50_curs_new(struct nouveau_drm *drm, int head, struct nv50_wndw **pwndw)
{
	struct {
		s32 oclass;
		int version;
		int (*new)(struct nouveau_drm *, int, s32, struct nv50_wndw **);
	} curses[] = {
		{ GA102_DISP_CURSOR, 0, cursc37a_new },
		{ TU102_DISP_CURSOR, 0, cursc37a_new },
		{ GV100_DISP_CURSOR, 0, cursc37a_new },
		{ GK104_DISP_CURSOR, 0, curs907a_new },
		{ GF110_DISP_CURSOR, 0, curs907a_new },
		{ GT214_DISP_CURSOR, 0, curs507a_new },
		{   G82_DISP_CURSOR, 0, curs507a_new },
		{  NV50_DISP_CURSOR, 0, curs507a_new },
		{}
	};
	struct nv50_disp *disp = nv50_disp(drm->dev);
	int cid;

	cid = nvif_mclass(&disp->disp->object, curses);
	if (cid < 0) {
		NV_ERROR(drm, "No supported cursor immediate class\n");
		return cid;
	}

	return curses[cid].new(drm, head, curses[cid].oclass, pwndw);
}
