
 

#include "exp_rcv.h"
#include "trace.h"

 
static void hfi1_exp_tid_set_init(struct exp_tid_set *set)
{
	INIT_LIST_HEAD(&set->list);
	set->count = 0;
}

 
void hfi1_exp_tid_group_init(struct hfi1_ctxtdata *rcd)
{
	hfi1_exp_tid_set_init(&rcd->tid_group_list);
	hfi1_exp_tid_set_init(&rcd->tid_used_list);
	hfi1_exp_tid_set_init(&rcd->tid_full_list);
}

 
int hfi1_alloc_ctxt_rcv_groups(struct hfi1_ctxtdata *rcd)
{
	struct hfi1_devdata *dd = rcd->dd;
	u32 tidbase;
	struct tid_group *grp;
	int i;
	u32 ngroups;

	ngroups = rcd->expected_count / dd->rcv_entries.group_size;
	rcd->groups =
		kcalloc_node(ngroups, sizeof(*rcd->groups),
			     GFP_KERNEL, rcd->numa_id);
	if (!rcd->groups)
		return -ENOMEM;
	tidbase = rcd->expected_base;
	for (i = 0; i < ngroups; i++) {
		grp = &rcd->groups[i];
		grp->size = dd->rcv_entries.group_size;
		grp->base = tidbase;
		tid_group_add_tail(grp, &rcd->tid_group_list);
		tidbase += dd->rcv_entries.group_size;
	}

	return 0;
}

 
void hfi1_free_ctxt_rcv_groups(struct hfi1_ctxtdata *rcd)
{
	kfree(rcd->groups);
	rcd->groups = NULL;
	hfi1_exp_tid_group_init(rcd);

	hfi1_clear_tids(rcd);
}
