 
#include "priv.h"
#include <subdev/acr.h>

MODULE_FIRMWARE("nvidia/gp108/sec2/desc.bin");
MODULE_FIRMWARE("nvidia/gp108/sec2/image.bin");
MODULE_FIRMWARE("nvidia/gp108/sec2/sig.bin");
MODULE_FIRMWARE("nvidia/gv100/sec2/desc.bin");
MODULE_FIRMWARE("nvidia/gv100/sec2/image.bin");
MODULE_FIRMWARE("nvidia/gv100/sec2/sig.bin");

static const struct nvkm_sec2_fwif
gp108_sec2_fwif[] = {
	{ 0, gp102_sec2_load, &gp102_sec2, &gp102_sec2_acr_1 },
	{}
};

int
gp108_sec2_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	       struct nvkm_sec2 **psec2)
{
	return nvkm_sec2_new_(gp108_sec2_fwif, device, type, inst, 0, psec2);
}
