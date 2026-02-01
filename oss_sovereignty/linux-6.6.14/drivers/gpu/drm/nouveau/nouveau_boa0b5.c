 
 
#include "nouveau_bo.h"
#include "nouveau_dma.h"
#include "nouveau_mem.h"

#include <nvif/push906f.h>

#include <nvhw/class/cla0b5.h>

int
nve0_bo_move_copy(struct nouveau_channel *chan, struct ttm_buffer_object *bo,
		  struct ttm_resource *old_reg, struct ttm_resource *new_reg)
{
	struct nouveau_mem *mem = nouveau_mem(old_reg);
	struct nvif_push *push = chan->chan.push;
	int ret;

	ret = PUSH_WAIT(push, 10);
	if (ret)
		return ret;

	PUSH_MTHD(push, NVA0B5, OFFSET_IN_UPPER,
		  NVVAL(NVA0B5, OFFSET_IN_UPPER, UPPER, upper_32_bits(mem->vma[0].addr)),

				OFFSET_IN_LOWER, lower_32_bits(mem->vma[0].addr),

				OFFSET_OUT_UPPER,
		  NVVAL(NVA0B5, OFFSET_OUT_UPPER, UPPER, upper_32_bits(mem->vma[1].addr)),

				OFFSET_OUT_LOWER, lower_32_bits(mem->vma[1].addr),
				PITCH_IN, PAGE_SIZE,
				PITCH_OUT, PAGE_SIZE,
				LINE_LENGTH_IN, PAGE_SIZE,
				LINE_COUNT, PFN_UP(new_reg->size));

	PUSH_IMMD(push, NVA0B5, LAUNCH_DMA,
		  NVDEF(NVA0B5, LAUNCH_DMA, DATA_TRANSFER_TYPE, NON_PIPELINED) |
		  NVDEF(NVA0B5, LAUNCH_DMA, FLUSH_ENABLE, TRUE) |
		  NVDEF(NVA0B5, LAUNCH_DMA, SEMAPHORE_TYPE, NONE) |
		  NVDEF(NVA0B5, LAUNCH_DMA, INTERRUPT_TYPE, NONE) |
		  NVDEF(NVA0B5, LAUNCH_DMA, SRC_MEMORY_LAYOUT, PITCH) |
		  NVDEF(NVA0B5, LAUNCH_DMA, DST_MEMORY_LAYOUT, PITCH) |
		  NVDEF(NVA0B5, LAUNCH_DMA, MULTI_LINE_ENABLE, TRUE) |
		  NVDEF(NVA0B5, LAUNCH_DMA, REMAP_ENABLE, FALSE) |
		  NVDEF(NVA0B5, LAUNCH_DMA, BYPASS_L2, USE_PTE_SETTING) |
		  NVDEF(NVA0B5, LAUNCH_DMA, SRC_TYPE, VIRTUAL) |
		  NVDEF(NVA0B5, LAUNCH_DMA, DST_TYPE, VIRTUAL));
	return 0;
}

int
nve0_bo_move_init(struct nouveau_channel *chan, u32 handle)
{
	struct nvif_push *push = chan->chan.push;
	int ret;

	ret = PUSH_WAIT(push, 2);
	if (ret)
		return ret;

	PUSH_NVSQ(push, NVA0B5, 0x0000, handle & 0x0000ffff);
	return 0;
}
