 
#include "priv.h"
#include "chan.h"

#include <engine/fifo.h>

bool
nvkm_sw_mthd(struct nvkm_sw *sw, int chid, int subc, u32 mthd, u32 data)
{
	struct nvkm_sw_chan *chan;
	bool handled = false;
	unsigned long flags;

	spin_lock_irqsave(&sw->engine.lock, flags);
	list_for_each_entry(chan, &sw->chan, head) {
		if (chan->fifo->id == chid) {
			handled = nvkm_sw_chan_mthd(chan, subc, mthd, data);
			list_del(&chan->head);
			list_add(&chan->head, &sw->chan);
			break;
		}
	}
	spin_unlock_irqrestore(&sw->engine.lock, flags);
	return handled;
}

static int
nvkm_sw_oclass_new(const struct nvkm_oclass *oclass, void *data, u32 size,
		   struct nvkm_object **pobject)
{
	struct nvkm_sw_chan *chan = nvkm_sw_chan(oclass->parent);
	const struct nvkm_sw_chan_sclass *sclass = oclass->engn;
	return sclass->ctor(chan, oclass, data, size, pobject);
}

static int
nvkm_sw_oclass_get(struct nvkm_oclass *oclass, int index)
{
	struct nvkm_sw *sw = nvkm_sw(oclass->engine);
	int c = 0;

	while (sw->func->sclass[c].ctor) {
		if (c++ == index) {
			oclass->engn = &sw->func->sclass[index];
			oclass->base =  sw->func->sclass[index].base;
			oclass->base.ctor = nvkm_sw_oclass_new;
			return index;
		}
	}

	return c;
}

static int
nvkm_sw_cclass_get(struct nvkm_chan *fifoch, const struct nvkm_oclass *oclass,
		   struct nvkm_object **pobject)
{
	struct nvkm_sw *sw = nvkm_sw(oclass->engine);
	return sw->func->chan_new(sw, fifoch, oclass, pobject);
}

static void *
nvkm_sw_dtor(struct nvkm_engine *engine)
{
	return nvkm_sw(engine);
}

static const struct nvkm_engine_func
nvkm_sw = {
	.dtor = nvkm_sw_dtor,
	.fifo.cclass = nvkm_sw_cclass_get,
	.fifo.sclass = nvkm_sw_oclass_get,
};

int
nvkm_sw_new_(const struct nvkm_sw_func *func, struct nvkm_device *device,
	     enum nvkm_subdev_type type, int inst, struct nvkm_sw **psw)
{
	struct nvkm_sw *sw;

	if (!(sw = *psw = kzalloc(sizeof(*sw), GFP_KERNEL)))
		return -ENOMEM;
	INIT_LIST_HEAD(&sw->chan);
	sw->func = func;

	return nvkm_engine_ctor(&nvkm_sw, device, type, inst, true, &sw->engine);
}
