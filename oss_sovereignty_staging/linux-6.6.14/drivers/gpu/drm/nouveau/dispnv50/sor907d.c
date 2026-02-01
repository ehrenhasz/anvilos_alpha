 
#include "core.h"

#include <nvif/class.h>
#include <nvif/push507c.h>

#include <nvhw/class/cl907d.h>

#include <nouveau_bo.h>

static int
sor907d_ctrl(struct nv50_core *core, int or, u32 ctrl,
	     struct nv50_head_atom *asyh)
{
	struct nvif_push *push = core->chan.push;
	int ret;

	if ((ret = PUSH_WAIT(push, 2)))
		return ret;

	PUSH_MTHD(push, NV907D, SOR_SET_CONTROL(or), ctrl);
	return 0;
}

static void
sor907d_get_caps(struct nv50_disp *disp, struct nouveau_encoder *outp, int or)
{
	struct nouveau_bo *bo = disp->sync;
	const int off = or * 2;
	outp->caps.dp_interlace =
		NVBO_RV32(bo, off, NV907D_CORE_NOTIFIER_3, CAPABILITIES_CAP_SOR0_20, DP_INTERLACE);
}

const struct nv50_outp_func
sor907d = {
	.ctrl = sor907d_ctrl,
	.get_caps = sor907d_get_caps,
};
