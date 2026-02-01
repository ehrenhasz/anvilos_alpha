 
#include "nv04.h"

#include <subdev/bios.h>
#include <subdev/bios/init.h>

static const struct nvkm_devinit_func
nv1a_devinit = {
	.dtor = nv04_devinit_dtor,
	.preinit = nv04_devinit_preinit,
	.post = nv04_devinit_post,
	.pll_set = nv04_devinit_pll_set,
};

int
nv1a_devinit_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
		 struct nvkm_devinit **pinit)
{
	return nv04_devinit_new_(&nv1a_devinit, device, type, inst, pinit);
}
