 

#include <linux/file.h>
#include <linux/anon_inodes.h>
#include <linux/sched/mm.h>
#include <rdma/ib_verbs.h>
#include <rdma/uverbs_types.h>
#include <linux/rcupdate.h>
#include <rdma/uverbs_ioctl.h>
#include <rdma/rdma_user_ioctl.h>
#include "uverbs.h"
#include "core_priv.h"
#include "rdma_core.h"

static void uverbs_uobject_free(struct kref *ref)
{
	kfree_rcu(container_of(ref, struct ib_uobject, ref), rcu);
}

 
void uverbs_uobject_put(struct ib_uobject *uobject)
{
	kref_put(&uobject->ref, uverbs_uobject_free);
}
EXPORT_SYMBOL(uverbs_uobject_put);

static int uverbs_try_lock_object(struct ib_uobject *uobj,
				  enum rdma_lookup_mode mode)
{
	 
	switch (mode) {
	case UVERBS_LOOKUP_READ:
		return atomic_fetch_add_unless(&uobj->usecnt, 1, -1) == -1 ?
			-EBUSY : 0;
	case UVERBS_LOOKUP_WRITE:
		 
		return atomic_cmpxchg(&uobj->usecnt, 0, -1) == 0 ? 0 : -EBUSY;
	case UVERBS_LOOKUP_DESTROY:
		return 0;
	}
	return 0;
}

static void assert_uverbs_usecnt(struct ib_uobject *uobj,
				 enum rdma_lookup_mode mode)
{
#ifdef CONFIG_LOCKDEP
	switch (mode) {
	case UVERBS_LOOKUP_READ:
		WARN_ON(atomic_read(&uobj->usecnt) <= 0);
		break;
	case UVERBS_LOOKUP_WRITE:
		WARN_ON(atomic_read(&uobj->usecnt) != -1);
		break;
	case UVERBS_LOOKUP_DESTROY:
		break;
	}
#endif
}

 
static int uverbs_destroy_uobject(struct ib_uobject *uobj,
				  enum rdma_remove_reason reason,
				  struct uverbs_attr_bundle *attrs)
{
	struct ib_uverbs_file *ufile = attrs->ufile;
	unsigned long flags;
	int ret;

	lockdep_assert_held(&ufile->hw_destroy_rwsem);
	assert_uverbs_usecnt(uobj, UVERBS_LOOKUP_WRITE);

	if (reason == RDMA_REMOVE_ABORT) {
		WARN_ON(!list_empty(&uobj->list));
		WARN_ON(!uobj->context);
		uobj->uapi_object->type_class->alloc_abort(uobj);
	} else if (uobj->object) {
		ret = uobj->uapi_object->type_class->destroy_hw(uobj, reason,
								attrs);
		if (ret)
			 
			return ret;

		uobj->object = NULL;
	}

	uobj->context = NULL;

	 
	if (reason != RDMA_REMOVE_DESTROY)
		atomic_set(&uobj->usecnt, 0);
	else
		uobj->uapi_object->type_class->remove_handle(uobj);

	if (!list_empty(&uobj->list)) {
		spin_lock_irqsave(&ufile->uobjects_lock, flags);
		list_del_init(&uobj->list);
		spin_unlock_irqrestore(&ufile->uobjects_lock, flags);

		 
		uverbs_uobject_put(uobj);
	}

	 
	if (reason == RDMA_REMOVE_ABORT)
		uverbs_uobject_put(uobj);

	return 0;
}

 
int uobj_destroy(struct ib_uobject *uobj, struct uverbs_attr_bundle *attrs)
{
	struct ib_uverbs_file *ufile = attrs->ufile;
	int ret;

	down_read(&ufile->hw_destroy_rwsem);

	 
	ret = uverbs_try_lock_object(uobj, UVERBS_LOOKUP_WRITE);
	if (ret)
		goto out_unlock;

	ret = uverbs_destroy_uobject(uobj, RDMA_REMOVE_DESTROY, attrs);
	if (ret) {
		atomic_set(&uobj->usecnt, 0);
		goto out_unlock;
	}

out_unlock:
	up_read(&ufile->hw_destroy_rwsem);
	return ret;
}

 
struct ib_uobject *__uobj_get_destroy(const struct uverbs_api_object *obj,
				      u32 id, struct uverbs_attr_bundle *attrs)
{
	struct ib_uobject *uobj;
	int ret;

	uobj = rdma_lookup_get_uobject(obj, attrs->ufile, id,
				       UVERBS_LOOKUP_DESTROY, attrs);
	if (IS_ERR(uobj))
		return uobj;

	ret = uobj_destroy(uobj, attrs);
	if (ret) {
		rdma_lookup_put_uobject(uobj, UVERBS_LOOKUP_DESTROY);
		return ERR_PTR(ret);
	}

	return uobj;
}

 
int __uobj_perform_destroy(const struct uverbs_api_object *obj, u32 id,
			   struct uverbs_attr_bundle *attrs)
{
	struct ib_uobject *uobj;

	uobj = __uobj_get_destroy(obj, id, attrs);
	if (IS_ERR(uobj))
		return PTR_ERR(uobj);
	uobj_put_destroy(uobj);
	return 0;
}

 
static struct ib_uobject *alloc_uobj(struct uverbs_attr_bundle *attrs,
				     const struct uverbs_api_object *obj)
{
	struct ib_uverbs_file *ufile = attrs->ufile;
	struct ib_uobject *uobj;

	if (!attrs->context) {
		struct ib_ucontext *ucontext =
			ib_uverbs_get_ucontext_file(ufile);

		if (IS_ERR(ucontext))
			return ERR_CAST(ucontext);
		attrs->context = ucontext;
	}

	uobj = kzalloc(obj->type_attrs->obj_size, GFP_KERNEL);
	if (!uobj)
		return ERR_PTR(-ENOMEM);
	 
	uobj->ufile = ufile;
	uobj->context = attrs->context;
	INIT_LIST_HEAD(&uobj->list);
	uobj->uapi_object = obj;
	 
	atomic_set(&uobj->usecnt, -1);
	kref_init(&uobj->ref);

	return uobj;
}

static int idr_add_uobj(struct ib_uobject *uobj)
{
        
	return xa_alloc(&uobj->ufile->idr, &uobj->id, NULL, xa_limit_32b,
			GFP_KERNEL);
}

 
static struct ib_uobject *
lookup_get_idr_uobject(const struct uverbs_api_object *obj,
		       struct ib_uverbs_file *ufile, s64 id,
		       enum rdma_lookup_mode mode)
{
	struct ib_uobject *uobj;

	if (id < 0 || id > ULONG_MAX)
		return ERR_PTR(-EINVAL);

	rcu_read_lock();
	 
	uobj = xa_load(&ufile->idr, id);
	if (!uobj || !kref_get_unless_zero(&uobj->ref))
		uobj = ERR_PTR(-ENOENT);
	rcu_read_unlock();
	return uobj;
}

static struct ib_uobject *
lookup_get_fd_uobject(const struct uverbs_api_object *obj,
		      struct ib_uverbs_file *ufile, s64 id,
		      enum rdma_lookup_mode mode)
{
	const struct uverbs_obj_fd_type *fd_type;
	struct file *f;
	struct ib_uobject *uobject;
	int fdno = id;

	if (fdno != id)
		return ERR_PTR(-EINVAL);

	if (mode != UVERBS_LOOKUP_READ)
		return ERR_PTR(-EOPNOTSUPP);

	if (!obj->type_attrs)
		return ERR_PTR(-EIO);
	fd_type =
		container_of(obj->type_attrs, struct uverbs_obj_fd_type, type);

	f = fget(fdno);
	if (!f)
		return ERR_PTR(-EBADF);

	uobject = f->private_data;
	 
	if (f->f_op != fd_type->fops || uobject->ufile != ufile) {
		fput(f);
		return ERR_PTR(-EBADF);
	}

	uverbs_uobject_get(uobject);
	return uobject;
}

struct ib_uobject *rdma_lookup_get_uobject(const struct uverbs_api_object *obj,
					   struct ib_uverbs_file *ufile, s64 id,
					   enum rdma_lookup_mode mode,
					   struct uverbs_attr_bundle *attrs)
{
	struct ib_uobject *uobj;
	int ret;

	if (obj == ERR_PTR(-ENOMSG)) {
		 
		uobj = lookup_get_idr_uobject(NULL, ufile, id, mode);
		if (IS_ERR(uobj))
			return uobj;
	} else {
		if (IS_ERR(obj))
			return ERR_PTR(-EINVAL);

		uobj = obj->type_class->lookup_get(obj, ufile, id, mode);
		if (IS_ERR(uobj))
			return uobj;

		if (uobj->uapi_object != obj) {
			ret = -EINVAL;
			goto free;
		}
	}

	 
	if (mode != UVERBS_LOOKUP_DESTROY &&
	    !srcu_dereference(ufile->device->ib_dev,
			      &ufile->device->disassociate_srcu)) {
		ret = -EIO;
		goto free;
	}

	ret = uverbs_try_lock_object(uobj, mode);
	if (ret)
		goto free;
	if (attrs)
		attrs->context = uobj->context;

	return uobj;
free:
	uobj->uapi_object->type_class->lookup_put(uobj, mode);
	uverbs_uobject_put(uobj);
	return ERR_PTR(ret);
}

static struct ib_uobject *
alloc_begin_idr_uobject(const struct uverbs_api_object *obj,
			struct uverbs_attr_bundle *attrs)
{
	int ret;
	struct ib_uobject *uobj;

	uobj = alloc_uobj(attrs, obj);
	if (IS_ERR(uobj))
		return uobj;

	ret = idr_add_uobj(uobj);
	if (ret)
		goto uobj_put;

	ret = ib_rdmacg_try_charge(&uobj->cg_obj, uobj->context->device,
				   RDMACG_RESOURCE_HCA_OBJECT);
	if (ret)
		goto remove;

	return uobj;

remove:
	xa_erase(&attrs->ufile->idr, uobj->id);
uobj_put:
	uverbs_uobject_put(uobj);
	return ERR_PTR(ret);
}

static struct ib_uobject *
alloc_begin_fd_uobject(const struct uverbs_api_object *obj,
		       struct uverbs_attr_bundle *attrs)
{
	const struct uverbs_obj_fd_type *fd_type;
	int new_fd;
	struct ib_uobject *uobj, *ret;
	struct file *filp;

	uobj = alloc_uobj(attrs, obj);
	if (IS_ERR(uobj))
		return uobj;

	fd_type =
		container_of(obj->type_attrs, struct uverbs_obj_fd_type, type);
	if (WARN_ON(fd_type->fops->release != &uverbs_uobject_fd_release &&
		    fd_type->fops->release != &uverbs_async_event_release)) {
		ret = ERR_PTR(-EINVAL);
		goto err_fd;
	}

	new_fd = get_unused_fd_flags(O_CLOEXEC);
	if (new_fd < 0) {
		ret = ERR_PTR(new_fd);
		goto err_fd;
	}

	 
	filp = anon_inode_getfile(fd_type->name, fd_type->fops, NULL,
				  fd_type->flags);
	if (IS_ERR(filp)) {
		ret = ERR_CAST(filp);
		goto err_getfile;
	}
	uobj->object = filp;

	uobj->id = new_fd;
	return uobj;

err_getfile:
	put_unused_fd(new_fd);
err_fd:
	uverbs_uobject_put(uobj);
	return ret;
}

struct ib_uobject *rdma_alloc_begin_uobject(const struct uverbs_api_object *obj,
					    struct uverbs_attr_bundle *attrs)
{
	struct ib_uverbs_file *ufile = attrs->ufile;
	struct ib_uobject *ret;

	if (IS_ERR(obj))
		return ERR_PTR(-EINVAL);

	 
	if (!down_read_trylock(&ufile->hw_destroy_rwsem))
		return ERR_PTR(-EIO);

	ret = obj->type_class->alloc_begin(obj, attrs);
	if (IS_ERR(ret)) {
		up_read(&ufile->hw_destroy_rwsem);
		return ret;
	}
	return ret;
}

static void alloc_abort_idr_uobject(struct ib_uobject *uobj)
{
	ib_rdmacg_uncharge(&uobj->cg_obj, uobj->context->device,
			   RDMACG_RESOURCE_HCA_OBJECT);

	xa_erase(&uobj->ufile->idr, uobj->id);
}

static int __must_check destroy_hw_idr_uobject(struct ib_uobject *uobj,
					       enum rdma_remove_reason why,
					       struct uverbs_attr_bundle *attrs)
{
	const struct uverbs_obj_idr_type *idr_type =
		container_of(uobj->uapi_object->type_attrs,
			     struct uverbs_obj_idr_type, type);
	int ret = idr_type->destroy_object(uobj, why, attrs);

	if (ret)
		return ret;

	if (why == RDMA_REMOVE_ABORT)
		return 0;

	ib_rdmacg_uncharge(&uobj->cg_obj, uobj->context->device,
			   RDMACG_RESOURCE_HCA_OBJECT);

	return 0;
}

static void remove_handle_idr_uobject(struct ib_uobject *uobj)
{
	xa_erase(&uobj->ufile->idr, uobj->id);
	 
	uverbs_uobject_put(uobj);
}

static void alloc_abort_fd_uobject(struct ib_uobject *uobj)
{
	struct file *filp = uobj->object;

	fput(filp);
	put_unused_fd(uobj->id);
}

static int __must_check destroy_hw_fd_uobject(struct ib_uobject *uobj,
					      enum rdma_remove_reason why,
					      struct uverbs_attr_bundle *attrs)
{
	const struct uverbs_obj_fd_type *fd_type = container_of(
		uobj->uapi_object->type_attrs, struct uverbs_obj_fd_type, type);

	fd_type->destroy_object(uobj, why);
	return 0;
}

static void remove_handle_fd_uobject(struct ib_uobject *uobj)
{
}

static void alloc_commit_idr_uobject(struct ib_uobject *uobj)
{
	struct ib_uverbs_file *ufile = uobj->ufile;
	void *old;

	 
	old = xa_store(&ufile->idr, uobj->id, uobj, GFP_KERNEL);
	WARN_ON(old != NULL);
}

static void swap_idr_uobjects(struct ib_uobject *obj_old,
			     struct ib_uobject *obj_new)
{
	struct ib_uverbs_file *ufile = obj_old->ufile;
	void *old;

	 
	old = xa_cmpxchg(&ufile->idr, obj_old->id, obj_old, XA_ZERO_ENTRY,
			 GFP_KERNEL);
	if (WARN_ON(old != obj_old))
		return;

	swap(obj_old->id, obj_new->id);

	old = xa_cmpxchg(&ufile->idr, obj_old->id, NULL, obj_old, GFP_KERNEL);
	WARN_ON(old != NULL);
}

static void alloc_commit_fd_uobject(struct ib_uobject *uobj)
{
	int fd = uobj->id;
	struct file *filp = uobj->object;

	 
	kref_get(&uobj->ufile->ref);

	 
	uobj->id = 0;

	 
	filp->private_data = uobj;
	fd_install(fd, filp);
}

 
void rdma_alloc_commit_uobject(struct ib_uobject *uobj,
			       struct uverbs_attr_bundle *attrs)
{
	struct ib_uverbs_file *ufile = attrs->ufile;

	 
	uverbs_uobject_get(uobj);
	spin_lock_irq(&ufile->uobjects_lock);
	list_add(&uobj->list, &ufile->uobjects);
	spin_unlock_irq(&ufile->uobjects_lock);

	 
	atomic_set(&uobj->usecnt, 0);

	 
	uobj->uapi_object->type_class->alloc_commit(uobj);

	 
	up_read(&ufile->hw_destroy_rwsem);
}

 
void rdma_assign_uobject(struct ib_uobject *to_uobj, struct ib_uobject *new_uobj,
			struct uverbs_attr_bundle *attrs)
{
	assert_uverbs_usecnt(new_uobj, UVERBS_LOOKUP_WRITE);

	if (WARN_ON(to_uobj->uapi_object != new_uobj->uapi_object ||
		    !to_uobj->uapi_object->type_class->swap_uobjects))
		return;

	to_uobj->uapi_object->type_class->swap_uobjects(to_uobj, new_uobj);

	 
	uverbs_destroy_uobject(to_uobj, RDMA_REMOVE_DESTROY, attrs);
}

 
void rdma_alloc_abort_uobject(struct ib_uobject *uobj,
			      struct uverbs_attr_bundle *attrs,
			      bool hw_obj_valid)
{
	struct ib_uverbs_file *ufile = uobj->ufile;
	int ret;

	if (hw_obj_valid) {
		ret = uobj->uapi_object->type_class->destroy_hw(
			uobj, RDMA_REMOVE_ABORT, attrs);
		 
		if (WARN_ON(ret))
			return rdma_alloc_commit_uobject(uobj, attrs);
	}

	uverbs_destroy_uobject(uobj, RDMA_REMOVE_ABORT, attrs);

	 
	up_read(&ufile->hw_destroy_rwsem);
}

static void lookup_put_idr_uobject(struct ib_uobject *uobj,
				   enum rdma_lookup_mode mode)
{
}

static void lookup_put_fd_uobject(struct ib_uobject *uobj,
				  enum rdma_lookup_mode mode)
{
	struct file *filp = uobj->object;

	WARN_ON(mode != UVERBS_LOOKUP_READ);
	 
	fput(filp);
}

void rdma_lookup_put_uobject(struct ib_uobject *uobj,
			     enum rdma_lookup_mode mode)
{
	assert_uverbs_usecnt(uobj, mode);
	 
	switch (mode) {
	case UVERBS_LOOKUP_READ:
		atomic_dec(&uobj->usecnt);
		break;
	case UVERBS_LOOKUP_WRITE:
		atomic_set(&uobj->usecnt, 0);
		break;
	case UVERBS_LOOKUP_DESTROY:
		break;
	}

	uobj->uapi_object->type_class->lookup_put(uobj, mode);
	 
	uverbs_uobject_put(uobj);
}

void setup_ufile_idr_uobject(struct ib_uverbs_file *ufile)
{
	xa_init_flags(&ufile->idr, XA_FLAGS_ALLOC);
}

void release_ufile_idr_uobject(struct ib_uverbs_file *ufile)
{
	struct ib_uobject *entry;
	unsigned long id;

	 
	xa_for_each(&ufile->idr, id, entry) {
		WARN_ON(entry->object);
		uverbs_uobject_put(entry);
	}

	xa_destroy(&ufile->idr);
}

const struct uverbs_obj_type_class uverbs_idr_class = {
	.alloc_begin = alloc_begin_idr_uobject,
	.lookup_get = lookup_get_idr_uobject,
	.alloc_commit = alloc_commit_idr_uobject,
	.alloc_abort = alloc_abort_idr_uobject,
	.lookup_put = lookup_put_idr_uobject,
	.destroy_hw = destroy_hw_idr_uobject,
	.remove_handle = remove_handle_idr_uobject,
	.swap_uobjects = swap_idr_uobjects,
};
EXPORT_SYMBOL(uverbs_idr_class);

 
int uverbs_uobject_fd_release(struct inode *inode, struct file *filp)
{
	struct ib_uverbs_file *ufile;
	struct ib_uobject *uobj;

	 
	if (!filp->private_data)
		return 0;
	uobj = filp->private_data;
	ufile = uobj->ufile;

	if (down_read_trylock(&ufile->hw_destroy_rwsem)) {
		struct uverbs_attr_bundle attrs = {
			.context = uobj->context,
			.ufile = ufile,
		};

		 
		WARN_ON(uverbs_try_lock_object(uobj, UVERBS_LOOKUP_WRITE));
		uverbs_destroy_uobject(uobj, RDMA_REMOVE_CLOSE, &attrs);
		up_read(&ufile->hw_destroy_rwsem);
	}

	 
	kref_put(&ufile->ref, ib_uverbs_release_file);

	 
	uverbs_uobject_put(uobj);
	return 0;
}
EXPORT_SYMBOL(uverbs_uobject_fd_release);

 
static void ufile_destroy_ucontext(struct ib_uverbs_file *ufile,
				   enum rdma_remove_reason reason)
{
	struct ib_ucontext *ucontext = ufile->ucontext;
	struct ib_device *ib_dev = ucontext->device;

	 
	if (reason == RDMA_REMOVE_DRIVER_REMOVE) {
		uverbs_user_mmap_disassociate(ufile);
		if (ib_dev->ops.disassociate_ucontext)
			ib_dev->ops.disassociate_ucontext(ucontext);
	}

	ib_rdmacg_uncharge(&ucontext->cg_obj, ib_dev,
			   RDMACG_RESOURCE_HCA_HANDLE);

	rdma_restrack_del(&ucontext->res);

	ib_dev->ops.dealloc_ucontext(ucontext);
	WARN_ON(!xa_empty(&ucontext->mmap_xa));
	kfree(ucontext);

	ufile->ucontext = NULL;
}

static int __uverbs_cleanup_ufile(struct ib_uverbs_file *ufile,
				  enum rdma_remove_reason reason)
{
	struct ib_uobject *obj, *next_obj;
	int ret = -EINVAL;
	struct uverbs_attr_bundle attrs = { .ufile = ufile };

	 
	list_for_each_entry_safe(obj, next_obj, &ufile->uobjects, list) {
		attrs.context = obj->context;
		 
		WARN_ON(uverbs_try_lock_object(obj, UVERBS_LOOKUP_WRITE));
		if (reason == RDMA_REMOVE_DRIVER_FAILURE)
			obj->object = NULL;
		if (!uverbs_destroy_uobject(obj, reason, &attrs))
			ret = 0;
		else
			atomic_set(&obj->usecnt, 0);
	}

	if (reason == RDMA_REMOVE_DRIVER_FAILURE) {
		WARN_ON(!list_empty(&ufile->uobjects));
		return 0;
	}
	return ret;
}

 
void uverbs_destroy_ufile_hw(struct ib_uverbs_file *ufile,
			     enum rdma_remove_reason reason)
{
	down_write(&ufile->hw_destroy_rwsem);

	 
	if (!ufile->ucontext)
		goto done;

	while (!list_empty(&ufile->uobjects) &&
	       !__uverbs_cleanup_ufile(ufile, reason)) {
	}

	if (WARN_ON(!list_empty(&ufile->uobjects)))
		__uverbs_cleanup_ufile(ufile, RDMA_REMOVE_DRIVER_FAILURE);
	ufile_destroy_ucontext(ufile, reason);

done:
	up_write(&ufile->hw_destroy_rwsem);
}

const struct uverbs_obj_type_class uverbs_fd_class = {
	.alloc_begin = alloc_begin_fd_uobject,
	.lookup_get = lookup_get_fd_uobject,
	.alloc_commit = alloc_commit_fd_uobject,
	.alloc_abort = alloc_abort_fd_uobject,
	.lookup_put = lookup_put_fd_uobject,
	.destroy_hw = destroy_hw_fd_uobject,
	.remove_handle = remove_handle_fd_uobject,
};
EXPORT_SYMBOL(uverbs_fd_class);

struct ib_uobject *
uverbs_get_uobject_from_file(u16 object_id, enum uverbs_obj_access access,
			     s64 id, struct uverbs_attr_bundle *attrs)
{
	const struct uverbs_api_object *obj =
		uapi_get_object(attrs->ufile->device->uapi, object_id);

	switch (access) {
	case UVERBS_ACCESS_READ:
		return rdma_lookup_get_uobject(obj, attrs->ufile, id,
					       UVERBS_LOOKUP_READ, attrs);
	case UVERBS_ACCESS_DESTROY:
		 
		return rdma_lookup_get_uobject(obj, attrs->ufile, id,
					       UVERBS_LOOKUP_DESTROY, attrs);
	case UVERBS_ACCESS_WRITE:
		return rdma_lookup_get_uobject(obj, attrs->ufile, id,
					       UVERBS_LOOKUP_WRITE, attrs);
	case UVERBS_ACCESS_NEW:
		return rdma_alloc_begin_uobject(obj, attrs);
	default:
		WARN_ON(true);
		return ERR_PTR(-EOPNOTSUPP);
	}
}

void uverbs_finalize_object(struct ib_uobject *uobj,
			    enum uverbs_obj_access access, bool hw_obj_valid,
			    bool commit, struct uverbs_attr_bundle *attrs)
{
	 

	switch (access) {
	case UVERBS_ACCESS_READ:
		rdma_lookup_put_uobject(uobj, UVERBS_LOOKUP_READ);
		break;
	case UVERBS_ACCESS_WRITE:
		rdma_lookup_put_uobject(uobj, UVERBS_LOOKUP_WRITE);
		break;
	case UVERBS_ACCESS_DESTROY:
		if (uobj)
			rdma_lookup_put_uobject(uobj, UVERBS_LOOKUP_DESTROY);
		break;
	case UVERBS_ACCESS_NEW:
		if (commit)
			rdma_alloc_commit_uobject(uobj, attrs);
		else
			rdma_alloc_abort_uobject(uobj, attrs, hw_obj_valid);
		break;
	default:
		WARN_ON(true);
	}
}
