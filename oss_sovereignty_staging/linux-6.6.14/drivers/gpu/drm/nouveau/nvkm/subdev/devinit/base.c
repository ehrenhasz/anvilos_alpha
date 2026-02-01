 
#include "priv.h"

#include <core/option.h>
#include <subdev/vga.h>

u32
nvkm_devinit_mmio(struct nvkm_devinit *init, u32 addr)
{
	if (init->func->mmio)
		addr = init->func->mmio(init, addr);
	return addr;
}

int
nvkm_devinit_pll_set(struct nvkm_devinit *init, u32 type, u32 khz)
{
	return init->func->pll_set(init, type, khz);
}

void
nvkm_devinit_meminit(struct nvkm_devinit *init)
{
	if (init->func->meminit)
		init->func->meminit(init);
}

u64
nvkm_devinit_disable(struct nvkm_devinit *init)
{
	if (init && init->func->disable)
		init->func->disable(init);

	return 0;
}

int
nvkm_devinit_post(struct nvkm_devinit *init)
{
	int ret = 0;
	if (init && init->func->post)
		ret = init->func->post(init, init->post);
	nvkm_devinit_disable(init);
	return ret;
}

static int
nvkm_devinit_fini(struct nvkm_subdev *subdev, bool suspend)
{
	struct nvkm_devinit *init = nvkm_devinit(subdev);
	 
	if (suspend)
		init->post = true;
	return 0;
}

static int
nvkm_devinit_preinit(struct nvkm_subdev *subdev)
{
	struct nvkm_devinit *init = nvkm_devinit(subdev);

	if (init->func->preinit)
		init->func->preinit(init);

	 
	if (init->force_post) {
		init->post = init->force_post;
		init->force_post = false;
	}

	 
	nvkm_lockvgac(subdev->device, false);
	return 0;
}

static int
nvkm_devinit_init(struct nvkm_subdev *subdev)
{
	struct nvkm_devinit *init = nvkm_devinit(subdev);
	if (init->func->init)
		init->func->init(init);
	return 0;
}

static void *
nvkm_devinit_dtor(struct nvkm_subdev *subdev)
{
	struct nvkm_devinit *init = nvkm_devinit(subdev);
	void *data = init;

	if (init->func->dtor)
		data = init->func->dtor(init);

	 
	nvkm_lockvgac(subdev->device, true);
	return data;
}

static const struct nvkm_subdev_func
nvkm_devinit = {
	.dtor = nvkm_devinit_dtor,
	.preinit = nvkm_devinit_preinit,
	.init = nvkm_devinit_init,
	.fini = nvkm_devinit_fini,
};

void
nvkm_devinit_ctor(const struct nvkm_devinit_func *func, struct nvkm_device *device,
		  enum nvkm_subdev_type type, int inst, struct nvkm_devinit *init)
{
	nvkm_subdev_ctor(&nvkm_devinit, device, type, inst, &init->subdev);
	init->func = func;
	init->force_post = nvkm_boolopt(device->cfgopt, "NvForcePost", false);
}
