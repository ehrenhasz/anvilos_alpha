 
#include "core.h"

#include <nvif/push507c.h>

#include <nvhw/class/cl907d.h>

static int
dac907d_ctrl(struct nv50_core *core, int or, u32 ctrl,
	     struct nv50_head_atom *asyh)
{
	struct nvif_push *push = core->chan.push;
	int ret;

	if ((ret = PUSH_WAIT(push, 2)))
		return ret;

	PUSH_MTHD(push, NV907D, DAC_SET_CONTROL(or), ctrl);
	return 0;
}

const struct nv50_outp_func
dac907d = {
	.ctrl = dac907d_ctrl,
};
