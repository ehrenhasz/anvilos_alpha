 
#include "chan.h"

#include <engine/fifo.h>

#include <nvif/event.h>
#include <nvif/unpack.h>

bool
nvkm_sw_chan_mthd(struct nvkm_sw_chan *chan, int subc, u32 mthd, u32 data)
{
	switch (mthd) {
	case 0x0000:
		return true;
	case 0x0500:
		nvkm_event_ntfy(&chan->event, 0, NVKM_SW_CHAN_EVENT_PAGE_FLIP);
		return true;
	default:
		if (chan->func->mthd)
			return chan->func->mthd(chan, subc, mthd, data);
		break;
	}
	return false;
}

static const struct nvkm_event_func
nvkm_sw_chan_event = {
};

static void *
nvkm_sw_chan_dtor(struct nvkm_object *object)
{
	struct nvkm_sw_chan *chan = nvkm_sw_chan(object);
	struct nvkm_sw *sw = chan->sw;
	unsigned long flags;
	void *data = chan;

	if (chan->func->dtor)
		data = chan->func->dtor(chan);
	nvkm_event_fini(&chan->event);

	spin_lock_irqsave(&sw->engine.lock, flags);
	list_del(&chan->head);
	spin_unlock_irqrestore(&sw->engine.lock, flags);
	return data;
}

static const struct nvkm_object_func
nvkm_sw_chan = {
	.dtor = nvkm_sw_chan_dtor,
};

int
nvkm_sw_chan_ctor(const struct nvkm_sw_chan_func *func, struct nvkm_sw *sw,
		  struct nvkm_chan *fifo, const struct nvkm_oclass *oclass,
		  struct nvkm_sw_chan *chan)
{
	unsigned long flags;

	nvkm_object_ctor(&nvkm_sw_chan, oclass, &chan->object);
	chan->func = func;
	chan->sw = sw;
	chan->fifo = fifo;
	spin_lock_irqsave(&sw->engine.lock, flags);
	list_add(&chan->head, &sw->chan);
	spin_unlock_irqrestore(&sw->engine.lock, flags);

	return nvkm_event_init(&nvkm_sw_chan_event, &sw->engine.subdev, 1, 1, &chan->event);
}
