
 

#include "rdma_core.h"
#include "uverbs.h"
#include <rdma/uverbs_std_types.h>

static int uverbs_free_counters(struct ib_uobject *uobject,
				enum rdma_remove_reason why,
				struct uverbs_attr_bundle *attrs)
{
	struct ib_counters *counters = uobject->object;
	int ret;

	if (atomic_read(&counters->usecnt))
		return -EBUSY;

	ret = counters->device->ops.destroy_counters(counters);
	if (ret)
		return ret;
	kfree(counters);
	return 0;
}

static int UVERBS_HANDLER(UVERBS_METHOD_COUNTERS_CREATE)(
	struct uverbs_attr_bundle *attrs)
{
	struct ib_uobject *uobj = uverbs_attr_get_uobject(
		attrs, UVERBS_ATTR_CREATE_COUNTERS_HANDLE);
	struct ib_device *ib_dev = attrs->context->device;
	struct ib_counters *counters;
	int ret;

	 
	if (!ib_dev->ops.create_counters)
		return -EOPNOTSUPP;

	counters = rdma_zalloc_drv_obj(ib_dev, ib_counters);
	if (!counters)
		return -ENOMEM;

	counters->device = ib_dev;
	counters->uobject = uobj;
	uobj->object = counters;
	atomic_set(&counters->usecnt, 0);

	ret = ib_dev->ops.create_counters(counters, attrs);
	if (ret)
		kfree(counters);

	return ret;
}

static int UVERBS_HANDLER(UVERBS_METHOD_COUNTERS_READ)(
	struct uverbs_attr_bundle *attrs)
{
	struct ib_counters_read_attr read_attr = {};
	const struct uverbs_attr *uattr;
	struct ib_counters *counters =
		uverbs_attr_get_obj(attrs, UVERBS_ATTR_READ_COUNTERS_HANDLE);
	int ret;

	if (!counters->device->ops.read_counters)
		return -EOPNOTSUPP;

	if (!atomic_read(&counters->usecnt))
		return -EINVAL;

	ret = uverbs_get_flags32(&read_attr.flags, attrs,
				 UVERBS_ATTR_READ_COUNTERS_FLAGS,
				 IB_UVERBS_READ_COUNTERS_PREFER_CACHED);
	if (ret)
		return ret;

	uattr = uverbs_attr_get(attrs, UVERBS_ATTR_READ_COUNTERS_BUFF);
	if (IS_ERR(uattr))
		return PTR_ERR(uattr);
	read_attr.ncounters = uattr->ptr_attr.len / sizeof(u64);
	read_attr.counters_buff = uverbs_zalloc(
		attrs, array_size(read_attr.ncounters, sizeof(u64)));
	if (IS_ERR(read_attr.counters_buff))
		return PTR_ERR(read_attr.counters_buff);

	ret = counters->device->ops.read_counters(counters, &read_attr, attrs);
	if (ret)
		return ret;

	return uverbs_copy_to(attrs, UVERBS_ATTR_READ_COUNTERS_BUFF,
			      read_attr.counters_buff,
			      read_attr.ncounters * sizeof(u64));
}

DECLARE_UVERBS_NAMED_METHOD(
	UVERBS_METHOD_COUNTERS_CREATE,
	UVERBS_ATTR_IDR(UVERBS_ATTR_CREATE_COUNTERS_HANDLE,
			UVERBS_OBJECT_COUNTERS,
			UVERBS_ACCESS_NEW,
			UA_MANDATORY));

DECLARE_UVERBS_NAMED_METHOD_DESTROY(
	UVERBS_METHOD_COUNTERS_DESTROY,
	UVERBS_ATTR_IDR(UVERBS_ATTR_DESTROY_COUNTERS_HANDLE,
			UVERBS_OBJECT_COUNTERS,
			UVERBS_ACCESS_DESTROY,
			UA_MANDATORY));

DECLARE_UVERBS_NAMED_METHOD(
	UVERBS_METHOD_COUNTERS_READ,
	UVERBS_ATTR_IDR(UVERBS_ATTR_READ_COUNTERS_HANDLE,
			UVERBS_OBJECT_COUNTERS,
			UVERBS_ACCESS_READ,
			UA_MANDATORY),
	UVERBS_ATTR_PTR_OUT(UVERBS_ATTR_READ_COUNTERS_BUFF,
			    UVERBS_ATTR_MIN_SIZE(0),
			    UA_MANDATORY),
	UVERBS_ATTR_FLAGS_IN(UVERBS_ATTR_READ_COUNTERS_FLAGS,
			     enum ib_uverbs_read_counters_flags));

DECLARE_UVERBS_NAMED_OBJECT(UVERBS_OBJECT_COUNTERS,
			    UVERBS_TYPE_ALLOC_IDR(uverbs_free_counters),
			    &UVERBS_METHOD(UVERBS_METHOD_COUNTERS_CREATE),
			    &UVERBS_METHOD(UVERBS_METHOD_COUNTERS_DESTROY),
			    &UVERBS_METHOD(UVERBS_METHOD_COUNTERS_READ));

const struct uapi_definition uverbs_def_obj_counters[] = {
	UAPI_DEF_CHAIN_OBJ_TREE_NAMED(UVERBS_OBJECT_COUNTERS,
				      UAPI_DEF_OBJ_NEEDS_FN(destroy_counters)),
	{}
};
