 
#include "priv.h"
#include "chan.h"
#include "nvsw.h"

#include <nvif/class.h>

 

static const struct nvkm_sw_chan_func
nv10_sw_chan = {
};

static int
nv10_sw_chan_new(struct nvkm_sw *sw, struct nvkm_chan *fifo,
		 const struct nvkm_oclass *oclass, struct nvkm_object **pobject)
{
	struct nvkm_sw_chan *chan;

	if (!(chan = kzalloc(sizeof(*chan), GFP_KERNEL)))
		return -ENOMEM;
	*pobject = &chan->object;

	return nvkm_sw_chan_ctor(&nv10_sw_chan, sw, fifo, oclass, chan);
}

 

static const struct nvkm_sw_func
nv10_sw = {
	.chan_new = nv10_sw_chan_new,
	.sclass = {
		{ nvkm_nvsw_new, { -1, -1, NVIF_CLASS_SW_NV10 } },
		{}
	}
};

int
nv10_sw_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_sw **psw)
{
	return nvkm_sw_new_(&nv10_sw, device, type, inst, psw);
}
