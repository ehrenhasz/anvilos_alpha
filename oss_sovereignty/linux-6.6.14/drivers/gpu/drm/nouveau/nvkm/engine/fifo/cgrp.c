 
#include "cgrp.h"
#include "chan.h"
#include "chid.h"
#include "runl.h"
#include "priv.h"

#include <core/gpuobj.h>
#include <subdev/mmu.h>

static void
nvkm_cgrp_ectx_put(struct nvkm_cgrp *cgrp, struct nvkm_ectx **pectx)
{
	struct nvkm_ectx *ectx = *pectx;

	if (ectx) {
		struct nvkm_engn *engn = ectx->engn;

		if (refcount_dec_and_test(&ectx->refs)) {
			CGRP_TRACE(cgrp, "dtor ectx %d[%s]", engn->id, engn->engine->subdev.name);
			nvkm_object_del(&ectx->object);
			list_del(&ectx->head);
			kfree(ectx);
		}

		*pectx = NULL;
	}
}

static int
nvkm_cgrp_ectx_get(struct nvkm_cgrp *cgrp, struct nvkm_engn *engn, struct nvkm_ectx **pectx,
		   struct nvkm_chan *chan, struct nvkm_client *client)
{
	struct nvkm_engine *engine = engn->engine;
	struct nvkm_oclass cclass = {
		.client = client,
		.engine = engine,
	};
	struct nvkm_ectx *ectx;
	int ret = 0;

	 
	ectx = nvkm_list_find(ectx, &cgrp->ectxs, head, ectx->engn == engn);
	if (ectx) {
		refcount_inc(&ectx->refs);
		*pectx = ectx;
		return 0;
	}

	 
	CGRP_TRACE(cgrp, "ctor ectx %d[%s]", engn->id, engn->engine->subdev.name);
	if (!(ectx = *pectx = kzalloc(sizeof(*ectx), GFP_KERNEL)))
		return -ENOMEM;

	ectx->engn = engn;
	refcount_set(&ectx->refs, 1);
	refcount_set(&ectx->uses, 0);
	list_add_tail(&ectx->head, &cgrp->ectxs);

	 
	if (engine->func->fifo.cclass)
		ret = engine->func->fifo.cclass(chan, &cclass, &ectx->object);
	else if (engine->func->cclass)
		ret = nvkm_object_new_(engine->func->cclass, &cclass, NULL, 0, &ectx->object);

	if (ret)
		nvkm_cgrp_ectx_put(cgrp, pectx);

	return ret;
}

void
nvkm_cgrp_vctx_put(struct nvkm_cgrp *cgrp, struct nvkm_vctx **pvctx)
{
	struct nvkm_vctx *vctx = *pvctx;

	if (vctx) {
		struct nvkm_engn *engn = vctx->ectx->engn;

		if (refcount_dec_and_test(&vctx->refs)) {
			CGRP_TRACE(cgrp, "dtor vctx %d[%s]", engn->id, engn->engine->subdev.name);
			nvkm_vmm_put(vctx->vmm, &vctx->vma);
			nvkm_gpuobj_del(&vctx->inst);

			nvkm_cgrp_ectx_put(cgrp, &vctx->ectx);
			if (vctx->vmm) {
				atomic_dec(&vctx->vmm->engref[engn->engine->subdev.type]);
				nvkm_vmm_unref(&vctx->vmm);
			}
			list_del(&vctx->head);
			kfree(vctx);
		}

		*pvctx = NULL;
	}
}

int
nvkm_cgrp_vctx_get(struct nvkm_cgrp *cgrp, struct nvkm_engn *engn, struct nvkm_chan *chan,
		   struct nvkm_vctx **pvctx, struct nvkm_client *client)
{
	struct nvkm_ectx *ectx;
	struct nvkm_vctx *vctx;
	int ret;

	 
	vctx = nvkm_list_find(vctx, &cgrp->vctxs, head,
			      vctx->ectx->engn == engn && vctx->vmm == chan->vmm);
	if (vctx) {
		refcount_inc(&vctx->refs);
		*pvctx = vctx;
		return 0;
	}

	 
	ret = nvkm_cgrp_ectx_get(cgrp, engn, &ectx, chan, client);
	if (ret) {
		CGRP_ERROR(cgrp, "ectx %d[%s]: %d", engn->id, engn->engine->subdev.name, ret);
		return ret;
	}

	 
	CGRP_TRACE(cgrp, "ctor vctx %d[%s]", engn->id, engn->engine->subdev.name);
	if (!(vctx = *pvctx = kzalloc(sizeof(*vctx), GFP_KERNEL))) {
		nvkm_cgrp_ectx_put(cgrp, &ectx);
		return -ENOMEM;
	}

	vctx->ectx = ectx;
	vctx->vmm = nvkm_vmm_ref(chan->vmm);
	refcount_set(&vctx->refs, 1);
	list_add_tail(&vctx->head, &cgrp->vctxs);

	 
	if (vctx->vmm)
		atomic_inc(&vctx->vmm->engref[engn->engine->subdev.type]);

	 
	if (engn->func->bind) {
		ret = nvkm_object_bind(vctx->ectx->object, NULL, 0, &vctx->inst);
		if (ret == 0 && engn->func->ctor)
			ret = engn->func->ctor(engn, vctx);
	}

	if (ret)
		nvkm_cgrp_vctx_put(cgrp, pvctx);

	return ret;
}

static void
nvkm_cgrp_del(struct kref *kref)
{
	struct nvkm_cgrp *cgrp = container_of(kref, typeof(*cgrp), kref);
	struct nvkm_runl *runl = cgrp->runl;

	if (runl->cgid)
		nvkm_chid_put(runl->cgid, cgrp->id, &cgrp->lock);

	mutex_destroy(&cgrp->mutex);
	nvkm_vmm_unref(&cgrp->vmm);
	kfree(cgrp);
}

void
nvkm_cgrp_unref(struct nvkm_cgrp **pcgrp)
{
	struct nvkm_cgrp *cgrp = *pcgrp;

	if (!cgrp)
		return;

	kref_put(&cgrp->kref, nvkm_cgrp_del);
	*pcgrp = NULL;
}

struct nvkm_cgrp *
nvkm_cgrp_ref(struct nvkm_cgrp *cgrp)
{
	if (cgrp)
		kref_get(&cgrp->kref);

	return cgrp;
}

void
nvkm_cgrp_put(struct nvkm_cgrp **pcgrp, unsigned long irqflags)
{
	struct nvkm_cgrp *cgrp = *pcgrp;

	if (!cgrp)
		return;

	*pcgrp = NULL;
	spin_unlock_irqrestore(&cgrp->lock, irqflags);
}

int
nvkm_cgrp_new(struct nvkm_runl *runl, const char *name, struct nvkm_vmm *vmm, bool hw,
	      struct nvkm_cgrp **pcgrp)
{
	struct nvkm_cgrp *cgrp;

	if (!(cgrp = *pcgrp = kmalloc(sizeof(*cgrp), GFP_KERNEL)))
		return -ENOMEM;

	cgrp->func = runl->fifo->func->cgrp.func;
	strscpy(cgrp->name, name, sizeof(cgrp->name));
	cgrp->runl = runl;
	cgrp->vmm = nvkm_vmm_ref(vmm);
	cgrp->hw = hw;
	cgrp->id = -1;
	kref_init(&cgrp->kref);
	INIT_LIST_HEAD(&cgrp->chans);
	cgrp->chan_nr = 0;
	spin_lock_init(&cgrp->lock);
	INIT_LIST_HEAD(&cgrp->ectxs);
	INIT_LIST_HEAD(&cgrp->vctxs);
	mutex_init(&cgrp->mutex);
	atomic_set(&cgrp->rc, NVKM_CGRP_RC_NONE);

	if (runl->cgid) {
		cgrp->id = nvkm_chid_get(runl->cgid, cgrp);
		if (cgrp->id < 0) {
			RUNL_ERROR(runl, "!cgids");
			nvkm_cgrp_unref(pcgrp);
			return -ENOSPC;
		}
	}

	return 0;
}
