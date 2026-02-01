 
#include "nvsw.h"
#include "chan.h"

#include <nvif/if0004.h>

static int
nvkm_nvsw_uevent(struct nvkm_object *object, void *argv, u32 argc, struct nvkm_uevent *uevent)
{
	union nv04_nvsw_event_args *args = argv;

	if (!uevent)
		return 0;
	if (argc != sizeof(args->vn))
		return -ENOSYS;

	return nvkm_uevent_add(uevent, &nvkm_nvsw(object)->chan->event, 0,
			       NVKM_SW_CHAN_EVENT_PAGE_FLIP, NULL);
}

static int
nvkm_nvsw_mthd(struct nvkm_object *object, u32 mthd, void *data, u32 size)
{
	struct nvkm_nvsw *nvsw = nvkm_nvsw(object);

	if (nvsw->func->mthd)
		return nvsw->func->mthd(nvsw, mthd, data, size);

	return -ENODEV;
}

static const struct nvkm_object_func
nvkm_nvsw_ = {
	.mthd = nvkm_nvsw_mthd,
	.uevent = nvkm_nvsw_uevent,
};

int
nvkm_nvsw_new_(const struct nvkm_nvsw_func *func, struct nvkm_sw_chan *chan,
	       const struct nvkm_oclass *oclass, void *data, u32 size,
	       struct nvkm_object **pobject)
{
	struct nvkm_nvsw *nvsw;

	if (!(nvsw = kzalloc(sizeof(*nvsw), GFP_KERNEL)))
		return -ENOMEM;
	*pobject = &nvsw->object;

	nvkm_object_ctor(&nvkm_nvsw_, oclass, &nvsw->object);
	nvsw->func = func;
	nvsw->chan = chan;
	return 0;
}

static const struct nvkm_nvsw_func
nvkm_nvsw = {
};

int
nvkm_nvsw_new(struct nvkm_sw_chan *chan, const struct nvkm_oclass *oclass,
	      void *data, u32 size, struct nvkm_object **pobject)
{
	return nvkm_nvsw_new_(&nvkm_nvsw, chan, oclass, data, size, pobject);
}
