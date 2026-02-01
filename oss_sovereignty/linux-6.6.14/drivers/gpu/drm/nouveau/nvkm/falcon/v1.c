 
#include "priv.h"

#include <core/gpuobj.h>
#include <core/memory.h>
#include <subdev/timer.h>

void
nvkm_falcon_v1_load_imem(struct nvkm_falcon *falcon, void *data, u32 start,
			 u32 size, u16 tag, u8 port, bool secure)
{
	u8 rem = size % 4;
	u32 reg;
	int i;

	size -= rem;

	reg = start | BIT(24) | (secure ? BIT(28) : 0);
	nvkm_falcon_wr32(falcon, 0x180 + (port * 16), reg);
	for (i = 0; i < size / 4; i++) {
		 
		if ((i & 0x3f) == 0)
			nvkm_falcon_wr32(falcon, 0x188 + (port * 16), tag++);
		nvkm_falcon_wr32(falcon, 0x184 + (port * 16), ((u32 *)data)[i]);
	}

	 
	if (rem) {
		u32 extra = ((u32 *)data)[i];

		 
		if ((i & 0x3f) == 0)
			nvkm_falcon_wr32(falcon, 0x188 + (port * 16), tag++);
		nvkm_falcon_wr32(falcon, 0x184 + (port * 16),
				 extra & (BIT(rem * 8) - 1));
		++i;
	}

	 
	for (; i & 0x3f; i++)
		nvkm_falcon_wr32(falcon, 0x184 + (port * 16), 0);
}

void
nvkm_falcon_v1_load_dmem(struct nvkm_falcon *falcon, void *data, u32 start,
			 u32 size, u8 port)
{
	u8 rem = size % 4;
	int i;

	size -= rem;

	nvkm_falcon_wr32(falcon, 0x1c0 + (port * 8), start | (0x1 << 24));
	for (i = 0; i < size / 4; i++)
		nvkm_falcon_wr32(falcon, 0x1c4 + (port * 8), ((u32 *)data)[i]);

	 
	if (rem) {
		u32 extra = ((u32 *)data)[i];

		nvkm_falcon_wr32(falcon, 0x1c4 + (port * 8),
				 extra & (BIT(rem * 8) - 1));
	}
}

void
nvkm_falcon_v1_start(struct nvkm_falcon *falcon)
{
	u32 reg = nvkm_falcon_rd32(falcon, 0x100);

	if (reg & BIT(6))
		nvkm_falcon_wr32(falcon, 0x130, 0x2);
	else
		nvkm_falcon_wr32(falcon, 0x100, 0x2);
}
