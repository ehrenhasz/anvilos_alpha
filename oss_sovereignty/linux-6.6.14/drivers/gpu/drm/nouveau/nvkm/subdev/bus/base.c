 
#include "priv.h"

static void
nvkm_bus_intr(struct nvkm_subdev *subdev)
{
	struct nvkm_bus *bus = nvkm_bus(subdev);
	bus->func->intr(bus);
}

static int
nvkm_bus_init(struct nvkm_subdev *subdev)
{
	struct nvkm_bus *bus = nvkm_bus(subdev);
	bus->func->init(bus);
	return 0;
}

static void *
nvkm_bus_dtor(struct nvkm_subdev *subdev)
{
	return nvkm_bus(subdev);
}

static const struct nvkm_subdev_func
nvkm_bus = {
	.dtor = nvkm_bus_dtor,
	.init = nvkm_bus_init,
	.intr = nvkm_bus_intr,
};

int
nvkm_bus_new_(const struct nvkm_bus_func *func, struct nvkm_device *device,
	      enum nvkm_subdev_type type, int inst, struct nvkm_bus **pbus)
{
	struct nvkm_bus *bus;
	if (!(bus = *pbus = kzalloc(sizeof(*bus), GFP_KERNEL)))
		return -ENOMEM;
	nvkm_subdev_ctor(&nvkm_bus, device, type, inst, &bus->subdev);
	bus->func = func;
	return 0;
}
