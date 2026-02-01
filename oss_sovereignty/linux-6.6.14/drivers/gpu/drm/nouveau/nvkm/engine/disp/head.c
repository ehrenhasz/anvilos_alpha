 
#include "head.h"

#include <core/client.h>

#include <nvif/cl0046.h>
#include <nvif/unpack.h>

struct nvkm_head *
nvkm_head_find(struct nvkm_disp *disp, int id)
{
	struct nvkm_head *head;
	list_for_each_entry(head, &disp->heads, head) {
		if (head->id == id)
			return head;
	}
	return NULL;
}

void
nvkm_head_del(struct nvkm_head **phead)
{
	struct nvkm_head *head = *phead;
	if (head) {
		HEAD_DBG(head, "dtor");
		list_del(&head->head);
		kfree(*phead);
		*phead = NULL;
	}
}

int
nvkm_head_new_(const struct nvkm_head_func *func,
	       struct nvkm_disp *disp, int id)
{
	struct nvkm_head *head;
	if (!(head = kzalloc(sizeof(*head), GFP_KERNEL)))
		return -ENOMEM;
	head->func = func;
	head->disp = disp;
	head->id = id;
	list_add_tail(&head->head, &disp->heads);
	HEAD_DBG(head, "ctor");
	return 0;
}
