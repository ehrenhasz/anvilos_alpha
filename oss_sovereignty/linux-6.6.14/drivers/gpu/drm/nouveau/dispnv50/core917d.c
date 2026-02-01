 
#include "core.h"
#include "head.h"

static const struct nv50_core_func
core917d = {
	.init = core507d_init,
	.ntfy_init = core507d_ntfy_init,
	.caps_init = core907d_caps_init,
	.ntfy_wait_done = core507d_ntfy_wait_done,
	.update = core507d_update,
	.head = &head917d,
#if IS_ENABLED(CONFIG_DEBUG_FS)
	.crc = &crc907d,
#endif
	.dac = &dac907d,
	.sor = &sor907d,
};

int
core917d_new(struct nouveau_drm *drm, s32 oclass, struct nv50_core **pcore)
{
	return core507d_new_(&core917d, drm, oclass, pcore);
}
