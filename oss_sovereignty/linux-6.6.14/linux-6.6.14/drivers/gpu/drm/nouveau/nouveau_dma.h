#ifndef __NOUVEAU_DMA_H__
#define __NOUVEAU_DMA_H__
#include "nouveau_bo.h"
#include "nouveau_chan.h"
int nouveau_dma_wait(struct nouveau_channel *, int slots, int size);
void nv50_dma_push(struct nouveau_channel *, u64 addr, u32 length,
		   bool no_prefetch);
#define NOUVEAU_DMA_SKIPS (128 / 4)
#define NV50_DMA_PUSH_MAX_LENGTH 0x7fffff
#define NV50_DMA_IB_MAX ((0x02000 / 8) - 1)
enum {
	NvDmaFB		= 0x80000002,
	NvDmaTT		= 0x80000003,
	NvNotify0       = 0x80000006,
	NvSema		= 0x8000000f,
	NvEvoSema0	= 0x80000010,
	NvEvoSema1	= 0x80000011,
};
static __must_check inline int
RING_SPACE(struct nouveau_channel *chan, int size)
{
	int ret;
	ret = nouveau_dma_wait(chan, 1, size);
	if (ret)
		return ret;
	chan->dma.free -= size;
	return 0;
}
static inline void
OUT_RING(struct nouveau_channel *chan, int data)
{
	nouveau_bo_wr32(chan->push.buffer, chan->dma.cur++, data);
}
#define WRITE_PUT(val) do {                                                    \
	mb();                                                   \
	nouveau_bo_rd32(chan->push.buffer, 0);                                 \
	nvif_wr32(&chan->user, chan->user_put, ((val) << 2) + chan->push.addr);\
} while (0)
static inline void
FIRE_RING(struct nouveau_channel *chan)
{
	if (chan->dma.cur == chan->dma.put)
		return;
	chan->accel_done = true;
	if (chan->dma.ib_max) {
		nv50_dma_push(chan, chan->push.addr + (chan->dma.put << 2),
			      (chan->dma.cur - chan->dma.put) << 2, false);
	} else {
		WRITE_PUT(chan->dma.cur);
	}
	chan->dma.put = chan->dma.cur;
}
static inline void
WIND_RING(struct nouveau_channel *chan)
{
	chan->dma.cur = chan->dma.put;
}
#define NV_SW_DMA_VBLSEM                                             0x0000018c
#define NV_SW_VBLSEM_OFFSET                                          0x00000400
#define NV_SW_VBLSEM_RELEASE_VALUE                                   0x00000404
#define NV_SW_VBLSEM_RELEASE                                         0x00000408
#define NV_SW_PAGE_FLIP                                              0x00000500
#endif
