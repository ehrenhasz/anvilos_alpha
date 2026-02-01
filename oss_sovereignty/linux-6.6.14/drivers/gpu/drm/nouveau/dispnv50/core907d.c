 
#include "core.h"
#include "head.h"

#include <nvif/push507c.h>
#include <nvif/timer.h>

#include <nvhw/class/cl907d.h>

#include "nouveau_bo.h"

int
core907d_caps_init(struct nouveau_drm *drm, struct nv50_disp *disp)
{
	struct nv50_core *core = disp->core;
	struct nouveau_bo *bo = disp->sync;
	s64 time;
	int ret;

	NVBO_WR32(bo, NV50_DISP_CORE_NTFY, NV907D_CORE_NOTIFIER_3, CAPABILITIES_4,
				     NVDEF(NV907D_CORE_NOTIFIER_3, CAPABILITIES_4, DONE, FALSE));

	ret = core507d_read_caps(disp);
	if (ret < 0)
		return ret;

	time = nvif_msec(core->chan.base.device, 2000ULL,
			 if (NVBO_TD32(bo, NV50_DISP_CORE_NTFY,
				       NV907D_CORE_NOTIFIER_3, CAPABILITIES_4, DONE, ==, TRUE))
				 break;
			 usleep_range(1, 2);
			 );
	if (time < 0)
		NV_ERROR(drm, "core caps notifier timeout\n");

	return 0;
}

static const struct nv50_core_func
core907d = {
	.init = core507d_init,
	.ntfy_init = core507d_ntfy_init,
	.caps_init = core907d_caps_init,
	.ntfy_wait_done = core507d_ntfy_wait_done,
	.update = core507d_update,
	.head = &head907d,
#if IS_ENABLED(CONFIG_DEBUG_FS)
	.crc = &crc907d,
#endif
	.dac = &dac907d,
	.sor = &sor907d,
};

int
core907d_new(struct nouveau_drm *drm, s32 oclass, struct nv50_core **pcore)
{
	return core507d_new_(&core907d, drm, oclass, pcore);
}
