 
#include "core.h"
#include "head.h"

static const struct nv50_core_func
core827d = {
	.init = core507d_init,
	.ntfy_init = core507d_ntfy_init,
	.caps_init = core507d_caps_init,
	.ntfy_wait_done = core507d_ntfy_wait_done,
	.update = core507d_update,
	.head = &head827d,
	.dac = &dac507d,
	.sor = &sor507d,
	.pior = &pior507d,
};

int
core827d_new(struct nouveau_drm *drm, s32 oclass, struct nv50_core **pcore)
{
	return core507d_new_(&core827d, drm, oclass, pcore);
}
