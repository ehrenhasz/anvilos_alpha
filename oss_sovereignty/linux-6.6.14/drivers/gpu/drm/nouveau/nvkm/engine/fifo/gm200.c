 
#include "priv.h"

#include <nvif/class.h>

int
gm200_fifo_runq_nr(struct nvkm_fifo *fifo)
{
	return nvkm_rd32(fifo->engine.subdev.device, 0x002004) & 0x000000ff;
}

int
gm200_fifo_chid_nr(struct nvkm_fifo *fifo)
{
	return nvkm_rd32(fifo->engine.subdev.device, 0x002008);
}

static const struct nvkm_fifo_func
gm200_fifo = {
	.chid_nr = gm200_fifo_chid_nr,
	.chid_ctor = gk110_fifo_chid_ctor,
	.runq_nr = gm200_fifo_runq_nr,
	.runl_ctor = gk104_fifo_runl_ctor,
	.init = gk104_fifo_init,
	.init_pbdmas = gk104_fifo_init_pbdmas,
	.intr = gk104_fifo_intr,
	.intr_mmu_fault_unit = gm107_fifo_intr_mmu_fault_unit,
	.intr_ctxsw_timeout = gf100_fifo_intr_ctxsw_timeout,
	.mmu_fault = &gm107_fifo_mmu_fault,
	.nonstall = &gf100_fifo_nonstall,
	.runl = &gm107_runl,
	.runq = &gk208_runq,
	.engn = &gk104_engn,
	.engn_ce = &gk104_engn_ce,
	.cgrp = {{ 0, 0,  KEPLER_CHANNEL_GROUP_A  }, &gk110_cgrp },
	.chan = {{ 0, 0, MAXWELL_CHANNEL_GPFIFO_A }, &gm107_chan },
};

int
gm200_fifo_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_fifo **pfifo)
{
	return nvkm_fifo_new_(&gm200_fifo, device, type, inst, pfifo);
}
