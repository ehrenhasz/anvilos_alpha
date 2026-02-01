 
 
#include "nouveau_bo.h"
#include "nouveau_dma.h"
#include "nouveau_mem.h"

#include <nvif/push906f.h>

#include <nvhw/class/cl9039.h>

int
nvc0_bo_move_m2mf(struct nouveau_channel *chan, struct ttm_buffer_object *bo,
		  struct ttm_resource *old_reg, struct ttm_resource *new_reg)
{
	struct nvif_push *push = chan->chan.push;
	struct nouveau_mem *mem = nouveau_mem(old_reg);
	u64 src_offset = mem->vma[0].addr;
	u64 dst_offset = mem->vma[1].addr;
	u32 page_count = PFN_UP(new_reg->size);
	int ret;

	page_count = PFN_UP(new_reg->size);
	while (page_count) {
		int line_count = (page_count > 2047) ? 2047 : page_count;

		ret = PUSH_WAIT(push, 12);
		if (ret)
			return ret;

		PUSH_MTHD(push, NV9039, OFFSET_OUT_UPPER,
			  NVVAL(NV9039, OFFSET_OUT_UPPER, VALUE, upper_32_bits(dst_offset)),

					OFFSET_OUT, lower_32_bits(dst_offset));

		PUSH_MTHD(push, NV9039, OFFSET_IN_UPPER,
			  NVVAL(NV9039, OFFSET_IN_UPPER, VALUE, upper_32_bits(src_offset)),

					OFFSET_IN, lower_32_bits(src_offset),
					PITCH_IN, PAGE_SIZE,
					PITCH_OUT, PAGE_SIZE,
					LINE_LENGTH_IN, PAGE_SIZE,
					LINE_COUNT, line_count);

		PUSH_MTHD(push, NV9039, LAUNCH_DMA,
			  NVDEF(NV9039, LAUNCH_DMA, SRC_INLINE, FALSE) |
			  NVDEF(NV9039, LAUNCH_DMA, SRC_MEMORY_LAYOUT, PITCH) |
			  NVDEF(NV9039, LAUNCH_DMA, DST_MEMORY_LAYOUT, PITCH) |
			  NVDEF(NV9039, LAUNCH_DMA, COMPLETION_TYPE, FLUSH_DISABLE) |
			  NVDEF(NV9039, LAUNCH_DMA, INTERRUPT_TYPE, NONE) |
			  NVDEF(NV9039, LAUNCH_DMA, SEMAPHORE_STRUCT_SIZE, ONE_WORD));

		page_count -= line_count;
		src_offset += (PAGE_SIZE * line_count);
		dst_offset += (PAGE_SIZE * line_count);
	}

	return 0;
}

int
nvc0_bo_move_init(struct nouveau_channel *chan, u32 handle)
{
	struct nvif_push *push = chan->chan.push;
	int ret;

	ret = PUSH_WAIT(push, 2);
	if (ret)
		return ret;

	PUSH_MTHD(push, NV9039, SET_OBJECT, handle);
	return 0;
}
