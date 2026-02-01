
 

#include "vmwgfx_drv.h"
#include "vmwgfx_resource_priv.h"

 
struct vmw_user_simple_resource {
	struct ttm_base_object base;
	struct vmw_simple_resource simple;
 
};


 
static int vmw_simple_resource_init(struct vmw_private *dev_priv,
				    struct vmw_simple_resource *simple,
				    void *data,
				    void (*res_free)(struct vmw_resource *res))
{
	struct vmw_resource *res = &simple->res;
	int ret;

	ret = vmw_resource_init(dev_priv, res, false, res_free,
				&simple->func->res_func);

	if (ret) {
		res_free(res);
		return ret;
	}

	ret = simple->func->init(res, data);
	if (ret) {
		vmw_resource_unreference(&res);
		return ret;
	}

	simple->res.hw_destroy = simple->func->hw_destroy;

	return 0;
}

 
static void vmw_simple_resource_free(struct vmw_resource *res)
{
	struct vmw_user_simple_resource *usimple =
		container_of(res, struct vmw_user_simple_resource,
			     simple.res);

	ttm_base_object_kfree(usimple, base);
}

 
static void vmw_simple_resource_base_release(struct ttm_base_object **p_base)
{
	struct ttm_base_object *base = *p_base;
	struct vmw_user_simple_resource *usimple =
		container_of(base, struct vmw_user_simple_resource, base);
	struct vmw_resource *res = &usimple->simple.res;

	*p_base = NULL;
	vmw_resource_unreference(&res);
}

 
int
vmw_simple_resource_create_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv,
				 const struct vmw_simple_resource_func *func)
{
	struct vmw_private *dev_priv = vmw_priv(dev);
	struct vmw_user_simple_resource *usimple;
	struct vmw_resource *res;
	struct vmw_resource *tmp;
	struct ttm_object_file *tfile = vmw_fpriv(file_priv)->tfile;
	size_t alloc_size;
	int ret;

	alloc_size = offsetof(struct vmw_user_simple_resource, simple) +
	  func->size;

	usimple = kzalloc(alloc_size, GFP_KERNEL);
	if (!usimple) {
		ret = -ENOMEM;
		goto out_ret;
	}

	usimple->simple.func = func;
	res = &usimple->simple.res;
	usimple->base.shareable = false;
	usimple->base.tfile = NULL;

	 
	ret = vmw_simple_resource_init(dev_priv, &usimple->simple,
				       data, vmw_simple_resource_free);
	if (ret)
		goto out_ret;

	tmp = vmw_resource_reference(res);
	ret = ttm_base_object_init(tfile, &usimple->base, false,
				   func->ttm_res_type,
				   &vmw_simple_resource_base_release);

	if (ret) {
		vmw_resource_unreference(&tmp);
		goto out_err;
	}

	func->set_arg_handle(data, usimple->base.handle);
out_err:
	vmw_resource_unreference(&res);
out_ret:
	return ret;
}

 
struct vmw_resource *
vmw_simple_resource_lookup(struct ttm_object_file *tfile,
			   uint32_t handle,
			   const struct vmw_simple_resource_func *func)
{
	struct vmw_user_simple_resource *usimple;
	struct ttm_base_object *base;
	struct vmw_resource *res;

	base = ttm_base_object_lookup(tfile, handle);
	if (!base) {
		VMW_DEBUG_USER("Invalid %s handle 0x%08lx.\n",
			       func->res_func.type_name,
			       (unsigned long) handle);
		return ERR_PTR(-ESRCH);
	}

	if (ttm_base_object_type(base) != func->ttm_res_type) {
		ttm_base_object_unref(&base);
		VMW_DEBUG_USER("Invalid type of %s handle 0x%08lx.\n",
			       func->res_func.type_name,
			       (unsigned long) handle);
		return ERR_PTR(-EINVAL);
	}

	usimple = container_of(base, typeof(*usimple), base);
	res = vmw_resource_reference(&usimple->simple.res);
	ttm_base_object_unref(&base);

	return res;
}
