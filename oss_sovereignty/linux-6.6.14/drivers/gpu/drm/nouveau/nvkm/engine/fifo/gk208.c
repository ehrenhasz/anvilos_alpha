 
#include "priv.h"
#include "runq.h"

#include <nvif/class.h>

void
gk208_runq_init(struct nvkm_runq *runq)
{
	gk104_runq_init(runq);

	nvkm_wr32(runq->fifo->engine.subdev.device, 0x04012c + (runq->id * 0x2000), 0x000f4240);
}

const struct nvkm_runq_func
gk208_runq = {
	.init = gk208_runq_init,
	.intr = gk104_runq_intr,
	.intr_0_names = gk104_runq_intr_0_names,
	.idle = gk104_runq_idle,
};

static int
gk208_fifo_chid_nr(struct nvkm_fifo *fifo)
{
	return 1024;
}

static const struct nvkm_fifo_func
gk208_fifo = {
	.chid_nr = gk208_fifo_chid_nr,
	.chid_ctor = gk110_fifo_chid_ctor,
	.runq_nr = gf100_fifo_runq_nr,
	.runl_ctor = gk104_fifo_runl_ctor,
	.init = gk104_fifo_init,
	.init_pbdmas = gk104_fifo_init_pbdmas,
	.intr = gk104_fifo_intr,
	.intr_mmu_fault_unit = gf100_fifo_intr_mmu_fault_unit,
	.intr_ctxsw_timeout = gf100_fifo_intr_ctxsw_timeout,
	.mmu_fault = &gk104_fifo_mmu_fault,
	.nonstall = &gf100_fifo_nonstall,
	.runl = &gk110_runl,
	.runq = &gk208_runq,
	.engn = &gk104_engn,
	.engn_ce = &gk104_engn_ce,
	.cgrp = {{ 0, 0, KEPLER_CHANNEL_GROUP_A  }, &gk110_cgrp },
	.chan = {{ 0, 0, KEPLER_CHANNEL_GPFIFO_A }, &gk110_chan },
};

int
gk208_fifo_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_fifo **pfifo)
{
	return nvkm_fifo_new_(&gk208_fifo, device, type, inst, pfifo);
}
