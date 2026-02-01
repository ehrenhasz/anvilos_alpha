 
#include "runq.h"
#include "priv.h"

void
nvkm_runq_del(struct nvkm_runq *runq)
{
	list_del(&runq->head);
	kfree(runq);
}

struct nvkm_runq *
nvkm_runq_new(struct nvkm_fifo *fifo, int pbid)
{
	struct nvkm_runq *runq;

	if (!(runq = kzalloc(sizeof(*runq), GFP_KERNEL)))
		return NULL;

	runq->func = fifo->func->runq;
	runq->fifo = fifo;
	runq->id = pbid;
	list_add_tail(&runq->head, &fifo->runqs);
	return runq;
}
