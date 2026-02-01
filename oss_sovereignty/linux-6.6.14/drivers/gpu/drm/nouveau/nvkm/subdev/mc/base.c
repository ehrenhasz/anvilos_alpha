 
#include "priv.h"

#include <core/option.h>
#include <subdev/top.h>

void
nvkm_mc_unk260(struct nvkm_device *device, u32 data)
{
	struct nvkm_mc *mc = device->mc;
	if (likely(mc) && mc->func->unk260)
		mc->func->unk260(mc, data);
}

void
nvkm_mc_intr_mask(struct nvkm_device *device, enum nvkm_subdev_type type, int inst, bool en)
{
	struct nvkm_subdev *subdev = nvkm_device_subdev(device, type, inst);

	if (subdev) {
		if (en)
			nvkm_intr_allow(subdev, NVKM_INTR_SUBDEV);
		else
			nvkm_intr_block(subdev, NVKM_INTR_SUBDEV);
	}
}

static u32
nvkm_mc_reset_mask(struct nvkm_device *device, bool isauto, enum nvkm_subdev_type type, int inst)
{
	struct nvkm_mc *mc = device->mc;
	const struct nvkm_mc_map *map;
	u64 pmc_enable = 0;
	if (likely(mc)) {
		if (!(pmc_enable = nvkm_top_reset(device, type, inst))) {
			for (map = mc->func->reset; map && map->stat; map++) {
				if (!isauto || !map->noauto) {
					if (map->type == type && map->inst == inst) {
						pmc_enable = map->stat;
						break;
					}
				}
			}
		}
	}
	return pmc_enable;
}

void
nvkm_mc_reset(struct nvkm_device *device, enum nvkm_subdev_type type, int inst)
{
	u64 pmc_enable = nvkm_mc_reset_mask(device, true, type, inst);
	if (pmc_enable) {
		device->mc->func->device->disable(device->mc, pmc_enable);
		device->mc->func->device->enable(device->mc, pmc_enable);
	}
}

void
nvkm_mc_disable(struct nvkm_device *device, enum nvkm_subdev_type type, int inst)
{
	u64 pmc_enable = nvkm_mc_reset_mask(device, false, type, inst);
	if (pmc_enable)
		device->mc->func->device->disable(device->mc, pmc_enable);
}

void
nvkm_mc_enable(struct nvkm_device *device, enum nvkm_subdev_type type, int inst)
{
	u64 pmc_enable = nvkm_mc_reset_mask(device, false, type, inst);
	if (pmc_enable)
		device->mc->func->device->enable(device->mc, pmc_enable);
}

bool
nvkm_mc_enabled(struct nvkm_device *device, enum nvkm_subdev_type type, int inst)
{
	u64 pmc_enable = nvkm_mc_reset_mask(device, false, type, inst);

	return (pmc_enable != 0) && device->mc->func->device->enabled(device->mc, pmc_enable);
}

static int
nvkm_mc_init(struct nvkm_subdev *subdev)
{
	struct nvkm_mc *mc = nvkm_mc(subdev);
	if (mc->func->init)
		mc->func->init(mc);
	return 0;
}

static void *
nvkm_mc_dtor(struct nvkm_subdev *subdev)
{
	return nvkm_mc(subdev);
}

static const struct nvkm_subdev_func
nvkm_mc = {
	.dtor = nvkm_mc_dtor,
	.init = nvkm_mc_init,
};

int
nvkm_mc_new_(const struct nvkm_mc_func *func, struct nvkm_device *device,
	     enum nvkm_subdev_type type, int inst, struct nvkm_mc **pmc)
{
	struct nvkm_mc *mc;
	int ret;

	if (!(mc = *pmc = kzalloc(sizeof(*mc), GFP_KERNEL)))
		return -ENOMEM;

	nvkm_subdev_ctor(&nvkm_mc, device, type, inst, &mc->subdev);
	mc->func = func;

	if (mc->func->intr) {
		ret = nvkm_intr_add(mc->func->intr, mc->func->intrs, &mc->subdev,
				    mc->func->intr_nonstall ? 2 : 1, &mc->intr);
		if (ret)
			return ret;
	}

	return 0;
}
