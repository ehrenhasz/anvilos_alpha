 
 
#include "nouveau_bo.h"
#include "nouveau_dma.h"
#include "nouveau_mem.h"

#include <nvif/push206e.h>

 

int
nva3_bo_move_copy(struct nouveau_channel *chan, struct ttm_buffer_object *bo,
		  struct ttm_resource *old_reg, struct ttm_resource *new_reg)
{
	struct nouveau_mem *mem = nouveau_mem(old_reg);
	struct nvif_push *push = chan->chan.push;
	u64 src_offset = mem->vma[0].addr;
	u64 dst_offset = mem->vma[1].addr;
	u32 page_count = PFN_UP(new_reg->size);
	int ret;

	page_count = PFN_UP(new_reg->size);
	while (page_count) {
		int line_count = (page_count > 8191) ? 8191 : page_count;

		ret = PUSH_WAIT(push, 11);
		if (ret)
			return ret;

		PUSH_NVSQ(push, NV85B5, 0x030c, upper_32_bits(src_offset),
					0x0310, lower_32_bits(src_offset),
					0x0314, upper_32_bits(dst_offset),
					0x0318, lower_32_bits(dst_offset),
					0x031c, PAGE_SIZE,
					0x0320, PAGE_SIZE,
					0x0324, PAGE_SIZE,
					0x0328, line_count);
		PUSH_NVSQ(push, NV85B5, 0x0300, 0x00000110);

		page_count -= line_count;
		src_offset += (PAGE_SIZE * line_count);
		dst_offset += (PAGE_SIZE * line_count);
	}

	return 0;
}
