 
#include "core.h"

#include <nvif/push507c.h>

#include <nvhw/class/cl507d.h>

static int
dac507d_ctrl(struct nv50_core *core, int or, u32 ctrl,
	     struct nv50_head_atom *asyh)
{
	struct nvif_push *push = core->chan.push;
	u32 sync = 0;
	int ret;

	if (asyh) {
		sync |= NVVAL(NV507D, DAC_SET_POLARITY, HSYNC, asyh->or.nhsync);
		sync |= NVVAL(NV507D, DAC_SET_POLARITY, VSYNC, asyh->or.nvsync);
	}

	if ((ret = PUSH_WAIT(push, 3)))
		return ret;

	PUSH_MTHD(push, NV507D, DAC_SET_CONTROL(or), ctrl,
				DAC_SET_POLARITY(or), sync);
	return 0;
}

const struct nv50_outp_func
dac507d = {
	.ctrl = dac507d_ctrl,
};
