 

#include "rdma_core.h"
#include "uverbs.h"
#include <rdma/uverbs_std_types.h>

static int uverbs_free_flow_action(struct ib_uobject *uobject,
				   enum rdma_remove_reason why,
				   struct uverbs_attr_bundle *attrs)
{
	struct ib_flow_action *action = uobject->object;

	if (atomic_read(&action->usecnt))
		return -EBUSY;

	return action->device->ops.destroy_flow_action(action);
}

DECLARE_UVERBS_NAMED_METHOD_DESTROY(
	UVERBS_METHOD_FLOW_ACTION_DESTROY,
	UVERBS_ATTR_IDR(UVERBS_ATTR_DESTROY_FLOW_ACTION_HANDLE,
			UVERBS_OBJECT_FLOW_ACTION,
			UVERBS_ACCESS_DESTROY,
			UA_MANDATORY));

DECLARE_UVERBS_NAMED_OBJECT(
	UVERBS_OBJECT_FLOW_ACTION,
	UVERBS_TYPE_ALLOC_IDR(uverbs_free_flow_action),
	&UVERBS_METHOD(UVERBS_METHOD_FLOW_ACTION_DESTROY));

const struct uapi_definition uverbs_def_obj_flow_action[] = {
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(
		UVERBS_OBJECT_FLOW_ACTION,
		UAPI_DEF_OBJ_NEEDS_FN(destroy_flow_action)),
	{}
};
