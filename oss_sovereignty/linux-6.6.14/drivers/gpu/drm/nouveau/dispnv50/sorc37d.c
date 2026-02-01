 
#include "core.h"

#include <nvif/pushc37b.h>

#include <nvhw/class/clc37d.h>

static int
sorc37d_ctrl(struct nv50_core *core, int or, u32 ctrl,
	     struct nv50_head_atom *asyh)
{
	struct nvif_push *push = core->chan.push;
	int ret;

	if ((ret = PUSH_WAIT(push, 2)))
		return ret;

	PUSH_MTHD(push, NVC37D, SOR_SET_CONTROL(or), ctrl);
	return 0;
}

static void
sorc37d_get_caps(struct nv50_disp *disp, struct nouveau_encoder *outp, int or)
{
	u32 tmp = nvif_rd32(&disp->caps, 0x000144 + (or * 8));

	outp->caps.dp_interlace = !!(tmp & 0x04000000);
}

const struct nv50_outp_func
sorc37d = {
	.ctrl = sorc37d_ctrl,
	.get_caps = sorc37d_get_caps,
};
