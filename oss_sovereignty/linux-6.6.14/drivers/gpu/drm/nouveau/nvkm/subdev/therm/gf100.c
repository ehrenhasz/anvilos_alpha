 
#include <core/device.h>

#include "priv.h"

#define pack_for_each_init(init, pack, head)                          \
	for (pack = head; pack && pack->init; pack++)                 \
		  for (init = pack->init; init && init->count; init++)
void
gf100_clkgate_init(struct nvkm_therm *therm,
		   const struct nvkm_therm_clkgate_pack *p)
{
	struct nvkm_device *device = therm->subdev.device;
	const struct nvkm_therm_clkgate_pack *pack;
	const struct nvkm_therm_clkgate_init *init;
	u32 next, addr;

	pack_for_each_init(init, pack, p) {
		next = init->addr + init->count * 8;
		addr = init->addr;

		nvkm_trace(&therm->subdev, "{ 0x%06x, %d, 0x%08x }\n",
			   init->addr, init->count, init->data);
		while (addr < next) {
			nvkm_trace(&therm->subdev, "\t0x%06x = 0x%08x\n",
				   addr, init->data);
			nvkm_wr32(device, addr, init->data);
			addr += 8;
		}
	}
}

 
