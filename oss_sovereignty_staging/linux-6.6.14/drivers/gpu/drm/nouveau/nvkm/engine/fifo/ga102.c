 
#include "priv.h"

#include <nvif/class.h>

static const struct nvkm_fifo_func
ga102_fifo = {
	.runl_ctor = ga100_fifo_runl_ctor,
	.mmu_fault = &tu102_fifo_mmu_fault,
	.nonstall_ctor = ga100_fifo_nonstall_ctor,
	.nonstall = &ga100_fifo_nonstall,
	.runl = &ga100_runl,
	.runq = &ga100_runq,
	.engn = &ga100_engn,
	.engn_ce = &ga100_engn_ce,
	.cgrp = {{ 0, 0, KEPLER_CHANNEL_GROUP_A  }, &ga100_cgrp, .force = true },
	.chan = {{ 0, 0, AMPERE_CHANNEL_GPFIFO_B }, &ga100_chan },
};

int
ga102_fifo_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_fifo **pfifo)
{
	return nvkm_fifo_new_(&ga102_fifo, device, type, inst, pfifo);
}
