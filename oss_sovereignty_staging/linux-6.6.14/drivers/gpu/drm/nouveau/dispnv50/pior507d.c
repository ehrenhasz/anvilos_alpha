 
#include "core.h"

#include <nvif/push507c.h>

#include <nvhw/class/cl507d.h>
#include <nvhw/class/cl837d.h>

static int
pior507d_ctrl(struct nv50_core *core, int or, u32 ctrl,
	      struct nv50_head_atom *asyh)
{
	struct nvif_push *push = core->chan.push;
	int ret;

	if (asyh) {
		ctrl |= NVVAL(NV507D, PIOR_SET_CONTROL, HSYNC_POLARITY, asyh->or.nhsync);
		ctrl |= NVVAL(NV507D, PIOR_SET_CONTROL, VSYNC_POLARITY, asyh->or.nvsync);
		ctrl |= NVVAL(NV837D, PIOR_SET_CONTROL, PIXEL_DEPTH, asyh->or.depth);
	}

	if ((ret = PUSH_WAIT(push, 2)))
		return ret;

	PUSH_MTHD(push, NV507D, PIOR_SET_CONTROL(or), ctrl);
	return 0;
}

static void
pior507d_get_caps(struct nv50_disp *disp, struct nouveau_encoder *outp,
		  int or)
{
	outp->caps.dp_interlace = true;
}

const struct nv50_outp_func
pior507d = {
	.ctrl = pior507d_ctrl,
	.get_caps = pior507d_get_caps,
};
