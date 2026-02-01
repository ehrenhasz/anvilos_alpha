 
 
#include "nouveau_bo.h"
#include "nouveau_dma.h"
#include "nouveau_drv.h"

#include <nvif/push006c.h>

#include <nvhw/class/cl0039.h>

static inline uint32_t
nouveau_bo_mem_ctxdma(struct ttm_buffer_object *bo,
		      struct nouveau_channel *chan, struct ttm_resource *reg)
{
	if (reg->mem_type == TTM_PL_TT)
		return NvDmaTT;
	return chan->vram.handle;
}

int
nv04_bo_move_m2mf(struct nouveau_channel *chan, struct ttm_buffer_object *bo,
		  struct ttm_resource *old_reg, struct ttm_resource *new_reg)
{
	struct nvif_push *push = chan->chan.push;
	u32 src_ctxdma = nouveau_bo_mem_ctxdma(bo, chan, old_reg);
	u32 src_offset = old_reg->start << PAGE_SHIFT;
	u32 dst_ctxdma = nouveau_bo_mem_ctxdma(bo, chan, new_reg);
	u32 dst_offset = new_reg->start << PAGE_SHIFT;
	u32 page_count = PFN_UP(new_reg->size);
	int ret;

	ret = PUSH_WAIT(push, 3);
	if (ret)
		return ret;

	PUSH_MTHD(push, NV039, SET_CONTEXT_DMA_BUFFER_IN, src_ctxdma,
			       SET_CONTEXT_DMA_BUFFER_OUT, dst_ctxdma);

	page_count = PFN_UP(new_reg->size);
	while (page_count) {
		int line_count = (page_count > 2047) ? 2047 : page_count;

		ret = PUSH_WAIT(push, 11);
		if (ret)
			return ret;

		PUSH_MTHD(push, NV039, OFFSET_IN, src_offset,
				       OFFSET_OUT, dst_offset,
				       PITCH_IN, PAGE_SIZE,
				       PITCH_OUT, PAGE_SIZE,
				       LINE_LENGTH_IN, PAGE_SIZE,
				       LINE_COUNT, line_count,

				       FORMAT,
			  NVVAL(NV039, FORMAT, IN, 1) |
			  NVVAL(NV039, FORMAT, OUT, 1),

				       BUFFER_NOTIFY, NV039_BUFFER_NOTIFY_WRITE_ONLY);

		PUSH_MTHD(push, NV039, NO_OPERATION, 0x00000000);

		page_count -= line_count;
		src_offset += (PAGE_SIZE * line_count);
		dst_offset += (PAGE_SIZE * line_count);
	}

	return 0;
}

int
nv04_bo_move_init(struct nouveau_channel *chan, u32 handle)
{
	struct nvif_push *push = chan->chan.push;
	int ret;

	ret = PUSH_WAIT(push, 4);
	if (ret)
		return ret;

	PUSH_MTHD(push, NV039, SET_OBJECT, handle);
	PUSH_MTHD(push, NV039, SET_CONTEXT_DMA_NOTIFIES, chan->drm->ntfy.handle);
	return 0;
}
