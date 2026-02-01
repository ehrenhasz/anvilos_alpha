 
 
#include "nouveau_bo.h"
#include "nouveau_dma.h"
#include "nouveau_mem.h"

#include <nvif/push206e.h>

int
nv84_bo_move_exec(struct nouveau_channel *chan, struct ttm_buffer_object *bo,
		  struct ttm_resource *old_reg, struct ttm_resource *new_reg)
{
	struct nouveau_mem *mem = nouveau_mem(old_reg);
	struct nvif_push *push = chan->chan.push;
	int ret;

	ret = PUSH_WAIT(push, 7);
	if (ret)
		return ret;

	PUSH_NVSQ(push, NV74C1, 0x0304, new_reg->size,
				0x0308, upper_32_bits(mem->vma[0].addr),
				0x030c, lower_32_bits(mem->vma[0].addr),
				0x0310, upper_32_bits(mem->vma[1].addr),
				0x0314, lower_32_bits(mem->vma[1].addr),
				0x0318, 0x00000000  );
	return 0;
}
