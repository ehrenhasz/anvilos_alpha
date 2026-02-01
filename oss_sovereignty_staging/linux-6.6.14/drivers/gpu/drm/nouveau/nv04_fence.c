 
#include "nouveau_drv.h"
#include "nouveau_dma.h"
#include "nouveau_fence.h"

#include <nvif/if0004.h>
#include <nvif/push006c.h>

struct nv04_fence_chan {
	struct nouveau_fence_chan base;
};

struct nv04_fence_priv {
	struct nouveau_fence_priv base;
};

static int
nv04_fence_emit(struct nouveau_fence *fence)
{
	struct nvif_push *push = unrcu_pointer(fence->channel)->chan.push;
	int ret = PUSH_WAIT(push, 2);
	if (ret == 0) {
		PUSH_NVSQ(push, NV_SW, 0x0150, fence->base.seqno);
		PUSH_KICK(push);
	}
	return ret;
}

static int
nv04_fence_sync(struct nouveau_fence *fence,
		struct nouveau_channel *prev, struct nouveau_channel *chan)
{
	return -ENODEV;
}

static u32
nv04_fence_read(struct nouveau_channel *chan)
{
	struct nv04_nvsw_get_ref_v0 args = {};
	WARN_ON(nvif_object_mthd(&chan->nvsw, NV04_NVSW_GET_REF,
				 &args, sizeof(args)));
	return args.ref;
}

static void
nv04_fence_context_del(struct nouveau_channel *chan)
{
	struct nv04_fence_chan *fctx = chan->fence;
	nouveau_fence_context_del(&fctx->base);
	chan->fence = NULL;
	nouveau_fence_context_free(&fctx->base);
}

static int
nv04_fence_context_new(struct nouveau_channel *chan)
{
	struct nv04_fence_chan *fctx = kzalloc(sizeof(*fctx), GFP_KERNEL);
	if (fctx) {
		nouveau_fence_context_new(chan, &fctx->base);
		fctx->base.emit = nv04_fence_emit;
		fctx->base.sync = nv04_fence_sync;
		fctx->base.read = nv04_fence_read;
		chan->fence = fctx;
		return 0;
	}
	return -ENOMEM;
}

static void
nv04_fence_destroy(struct nouveau_drm *drm)
{
	struct nv04_fence_priv *priv = drm->fence;
	drm->fence = NULL;
	kfree(priv);
}

int
nv04_fence_create(struct nouveau_drm *drm)
{
	struct nv04_fence_priv *priv;

	priv = drm->fence = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base.dtor = nv04_fence_destroy;
	priv->base.context_new = nv04_fence_context_new;
	priv->base.context_del = nv04_fence_context_del;
	return 0;
}
