 
#include "nv50.h"

#include <core/gpuobj.h>
#include <subdev/bar.h>
#include <engine/disp.h>
#include <engine/fifo.h>

#include <nvif/class.h>
#include <nvif/event.h>

 

static int
gf100_sw_chan_vblsem_release(struct nvkm_event_ntfy *notify, u32 bits)
{
	struct nv50_sw_chan *chan =
		container_of(notify, typeof(*chan), vblank.notify[notify->id]);
	struct nvkm_sw *sw = chan->base.sw;
	struct nvkm_device *device = sw->engine.subdev.device;
	u32 inst = chan->base.fifo->inst->addr >> 12;

	nvkm_wr32(device, 0x001718, 0x80000000 | inst);
	nvkm_bar_flush(device->bar);
	nvkm_wr32(device, 0x06000c, upper_32_bits(chan->vblank.offset));
	nvkm_wr32(device, 0x060010, lower_32_bits(chan->vblank.offset));
	nvkm_wr32(device, 0x060014, chan->vblank.value);

	return NVKM_EVENT_DROP;
}

static bool
gf100_sw_chan_mthd(struct nvkm_sw_chan *base, int subc, u32 mthd, u32 data)
{
	struct nv50_sw_chan *chan = nv50_sw_chan(base);
	struct nvkm_engine *engine = chan->base.object.engine;
	struct nvkm_device *device = engine->subdev.device;
	switch (mthd) {
	case 0x0400:
		chan->vblank.offset &= 0x00ffffffffULL;
		chan->vblank.offset |= (u64)data << 32;
		return true;
	case 0x0404:
		chan->vblank.offset &= 0xff00000000ULL;
		chan->vblank.offset |= data;
		return true;
	case 0x0408:
		chan->vblank.value = data;
		return true;
	case 0x040c:
		if (data < device->disp->vblank.index_nr) {
			nvkm_event_ntfy_allow(&chan->vblank.notify[data]);
			return true;
		}
		break;
	case 0x600:  
		nvkm_wr32(device, 0x419e00, data);
		return true;
	case 0x644:  
		if (!(data & ~0x001ffffe)) {
			nvkm_wr32(device, 0x419e44, data);
			return true;
		}
		break;
	case 0x6ac:  
		nvkm_wr32(device, 0x419eac, data);
		return true;
	default:
		break;
	}
	return false;
}

static const struct nvkm_sw_chan_func
gf100_sw_chan = {
	.dtor = nv50_sw_chan_dtor,
	.mthd = gf100_sw_chan_mthd,
};

static int
gf100_sw_chan_new(struct nvkm_sw *sw, struct nvkm_chan *fifoch,
		  const struct nvkm_oclass *oclass,
		  struct nvkm_object **pobject)
{
	struct nvkm_disp *disp = sw->engine.subdev.device->disp;
	struct nv50_sw_chan *chan;
	int ret, i;

	if (!(chan = kzalloc(sizeof(*chan), GFP_KERNEL)))
		return -ENOMEM;
	*pobject = &chan->base.object;

	ret = nvkm_sw_chan_ctor(&gf100_sw_chan, sw, fifoch, oclass,
				&chan->base);
	if (ret)
		return ret;

	for (i = 0; disp && i < disp->vblank.index_nr; i++) {
		nvkm_event_ntfy_add(&disp->vblank, i, NVKM_DISP_HEAD_EVENT_VBLANK, true,
				    gf100_sw_chan_vblsem_release, &chan->vblank.notify[i]);
	}

	return 0;
}

 

static const struct nvkm_sw_func
gf100_sw = {
	.chan_new = gf100_sw_chan_new,
	.sclass = {
		{ nvkm_nvsw_new, { -1, -1, NVIF_CLASS_SW_GF100 } },
		{}
	}
};

int
gf100_sw_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, struct nvkm_sw **psw)
{
	return nvkm_sw_new_(&gf100_sw, device, type, inst, psw);
}
