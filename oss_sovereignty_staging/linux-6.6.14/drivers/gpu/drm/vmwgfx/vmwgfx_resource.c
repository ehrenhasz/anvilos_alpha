
 

#include <drm/ttm/ttm_placement.h>

#include "vmwgfx_binding.h"
#include "vmwgfx_bo.h"
#include "vmwgfx_drv.h"
#include "vmwgfx_resource_priv.h"

#define VMW_RES_EVICT_ERR_COUNT 10

 
void vmw_resource_mob_attach(struct vmw_resource *res)
{
	struct vmw_bo *gbo = res->guest_memory_bo;
	struct rb_node **new = &gbo->res_tree.rb_node, *parent = NULL;

	dma_resv_assert_held(gbo->tbo.base.resv);
	res->used_prio = (res->res_dirty) ? res->func->dirty_prio :
		res->func->prio;

	while (*new) {
		struct vmw_resource *this =
			container_of(*new, struct vmw_resource, mob_node);

		parent = *new;
		new = (res->guest_memory_offset < this->guest_memory_offset) ?
			&((*new)->rb_left) : &((*new)->rb_right);
	}

	rb_link_node(&res->mob_node, parent, new);
	rb_insert_color(&res->mob_node, &gbo->res_tree);

	vmw_bo_prio_add(gbo, res->used_prio);
}

 
void vmw_resource_mob_detach(struct vmw_resource *res)
{
	struct vmw_bo *gbo = res->guest_memory_bo;

	dma_resv_assert_held(gbo->tbo.base.resv);
	if (vmw_resource_mob_attached(res)) {
		rb_erase(&res->mob_node, &gbo->res_tree);
		RB_CLEAR_NODE(&res->mob_node);
		vmw_bo_prio_del(gbo, res->used_prio);
	}
}

struct vmw_resource *vmw_resource_reference(struct vmw_resource *res)
{
	kref_get(&res->kref);
	return res;
}

struct vmw_resource *
vmw_resource_reference_unless_doomed(struct vmw_resource *res)
{
	return kref_get_unless_zero(&res->kref) ? res : NULL;
}

 
void vmw_resource_release_id(struct vmw_resource *res)
{
	struct vmw_private *dev_priv = res->dev_priv;
	struct idr *idr = &dev_priv->res_idr[res->func->res_type];

	spin_lock(&dev_priv->resource_lock);
	if (res->id != -1)
		idr_remove(idr, res->id);
	res->id = -1;
	spin_unlock(&dev_priv->resource_lock);
}

static void vmw_resource_release(struct kref *kref)
{
	struct vmw_resource *res =
	    container_of(kref, struct vmw_resource, kref);
	struct vmw_private *dev_priv = res->dev_priv;
	int id;
	int ret;
	struct idr *idr = &dev_priv->res_idr[res->func->res_type];

	spin_lock(&dev_priv->resource_lock);
	list_del_init(&res->lru_head);
	spin_unlock(&dev_priv->resource_lock);
	if (res->guest_memory_bo) {
		struct ttm_buffer_object *bo = &res->guest_memory_bo->tbo;

		ret = ttm_bo_reserve(bo, false, false, NULL);
		BUG_ON(ret);
		if (vmw_resource_mob_attached(res) &&
		    res->func->unbind != NULL) {
			struct ttm_validate_buffer val_buf;

			val_buf.bo = bo;
			val_buf.num_shared = 0;
			res->func->unbind(res, false, &val_buf);
		}
		res->guest_memory_size = false;
		vmw_resource_mob_detach(res);
		if (res->dirty)
			res->func->dirty_free(res);
		if (res->coherent)
			vmw_bo_dirty_release(res->guest_memory_bo);
		ttm_bo_unreserve(bo);
		vmw_user_bo_unref(&res->guest_memory_bo);
	}

	if (likely(res->hw_destroy != NULL)) {
		mutex_lock(&dev_priv->binding_mutex);
		vmw_binding_res_list_kill(&res->binding_head);
		mutex_unlock(&dev_priv->binding_mutex);
		res->hw_destroy(res);
	}

	id = res->id;
	if (res->res_free != NULL)
		res->res_free(res);
	else
		kfree(res);

	spin_lock(&dev_priv->resource_lock);
	if (id != -1)
		idr_remove(idr, id);
	spin_unlock(&dev_priv->resource_lock);
}

void vmw_resource_unreference(struct vmw_resource **p_res)
{
	struct vmw_resource *res = *p_res;

	*p_res = NULL;
	kref_put(&res->kref, vmw_resource_release);
}


 
int vmw_resource_alloc_id(struct vmw_resource *res)
{
	struct vmw_private *dev_priv = res->dev_priv;
	int ret;
	struct idr *idr = &dev_priv->res_idr[res->func->res_type];

	BUG_ON(res->id != -1);

	idr_preload(GFP_KERNEL);
	spin_lock(&dev_priv->resource_lock);

	ret = idr_alloc(idr, res, 1, 0, GFP_NOWAIT);
	if (ret >= 0)
		res->id = ret;

	spin_unlock(&dev_priv->resource_lock);
	idr_preload_end();
	return ret < 0 ? ret : 0;
}

 
int vmw_resource_init(struct vmw_private *dev_priv, struct vmw_resource *res,
		      bool delay_id,
		      void (*res_free) (struct vmw_resource *res),
		      const struct vmw_res_func *func)
{
	kref_init(&res->kref);
	res->hw_destroy = NULL;
	res->res_free = res_free;
	res->dev_priv = dev_priv;
	res->func = func;
	RB_CLEAR_NODE(&res->mob_node);
	INIT_LIST_HEAD(&res->lru_head);
	INIT_LIST_HEAD(&res->binding_head);
	res->id = -1;
	res->guest_memory_bo = NULL;
	res->guest_memory_offset = 0;
	res->guest_memory_dirty = false;
	res->res_dirty = false;
	res->coherent = false;
	res->used_prio = 3;
	res->dirty = NULL;
	if (delay_id)
		return 0;
	else
		return vmw_resource_alloc_id(res);
}


 
int vmw_user_resource_lookup_handle(struct vmw_private *dev_priv,
				    struct ttm_object_file *tfile,
				    uint32_t handle,
				    const struct vmw_user_resource_conv
				    *converter,
				    struct vmw_resource **p_res)
{
	struct ttm_base_object *base;
	struct vmw_resource *res;
	int ret = -EINVAL;

	base = ttm_base_object_lookup(tfile, handle);
	if (unlikely(!base))
		return -EINVAL;

	if (unlikely(ttm_base_object_type(base) != converter->object_type))
		goto out_bad_resource;

	res = converter->base_obj_to_res(base);
	kref_get(&res->kref);

	*p_res = res;
	ret = 0;

out_bad_resource:
	ttm_base_object_unref(&base);

	return ret;
}

 
int vmw_user_lookup_handle(struct vmw_private *dev_priv,
			   struct drm_file *filp,
			   uint32_t handle,
			   struct vmw_surface **out_surf,
			   struct vmw_bo **out_buf)
{
	struct ttm_object_file *tfile = vmw_fpriv(filp)->tfile;
	struct vmw_resource *res;
	int ret;

	BUG_ON(*out_surf || *out_buf);

	ret = vmw_user_resource_lookup_handle(dev_priv, tfile, handle,
					      user_surface_converter,
					      &res);
	if (!ret) {
		*out_surf = vmw_res_to_srf(res);
		return 0;
	}

	*out_surf = NULL;
	ret = vmw_user_bo_lookup(filp, handle, out_buf);
	return ret;
}

 
static int vmw_resource_buf_alloc(struct vmw_resource *res,
				  bool interruptible)
{
	unsigned long size = PFN_ALIGN(res->guest_memory_size);
	struct vmw_bo *gbo;
	struct vmw_bo_params bo_params = {
		.domain = res->func->domain,
		.busy_domain = res->func->busy_domain,
		.bo_type = ttm_bo_type_device,
		.size = res->guest_memory_size,
		.pin = false
	};
	int ret;

	if (likely(res->guest_memory_bo)) {
		BUG_ON(res->guest_memory_bo->tbo.base.size < size);
		return 0;
	}

	ret = vmw_gem_object_create(res->dev_priv, &bo_params, &gbo);
	if (unlikely(ret != 0))
		goto out_no_bo;

	res->guest_memory_bo = gbo;

out_no_bo:
	return ret;
}

 
static int vmw_resource_do_validate(struct vmw_resource *res,
				    struct ttm_validate_buffer *val_buf,
				    bool dirtying)
{
	int ret = 0;
	const struct vmw_res_func *func = res->func;

	if (unlikely(res->id == -1)) {
		ret = func->create(res);
		if (unlikely(ret != 0))
			return ret;
	}

	if (func->bind &&
	    ((func->needs_guest_memory && !vmw_resource_mob_attached(res) &&
	      val_buf->bo) ||
	     (!func->needs_guest_memory && val_buf->bo))) {
		ret = func->bind(res, val_buf);
		if (unlikely(ret != 0))
			goto out_bind_failed;
		if (func->needs_guest_memory)
			vmw_resource_mob_attach(res);
	}

	 
	if (func->dirty_alloc && vmw_resource_mob_attached(res) &&
	    !res->coherent) {
		if (res->guest_memory_bo->dirty && !res->dirty) {
			ret = func->dirty_alloc(res);
			if (ret)
				return ret;
		} else if (!res->guest_memory_bo->dirty && res->dirty) {
			func->dirty_free(res);
		}
	}

	 
	if (res->dirty) {
		if (dirtying && !res->res_dirty) {
			pgoff_t start = res->guest_memory_offset >> PAGE_SHIFT;
			pgoff_t end = __KERNEL_DIV_ROUND_UP
				(res->guest_memory_offset + res->guest_memory_size,
				 PAGE_SIZE);

			vmw_bo_dirty_unmap(res->guest_memory_bo, start, end);
		}

		vmw_bo_dirty_transfer_to_res(res);
		return func->dirty_sync(res);
	}

	return 0;

out_bind_failed:
	func->destroy(res);

	return ret;
}

 
void vmw_resource_unreserve(struct vmw_resource *res,
			    bool dirty_set,
			    bool dirty,
			    bool switch_guest_memory,
			    struct vmw_bo *new_guest_memory_bo,
			    unsigned long new_guest_memory_offset)
{
	struct vmw_private *dev_priv = res->dev_priv;

	if (!list_empty(&res->lru_head))
		return;

	if (switch_guest_memory && new_guest_memory_bo != res->guest_memory_bo) {
		if (res->guest_memory_bo) {
			vmw_resource_mob_detach(res);
			if (res->coherent)
				vmw_bo_dirty_release(res->guest_memory_bo);
			vmw_user_bo_unref(&res->guest_memory_bo);
		}

		if (new_guest_memory_bo) {
			res->guest_memory_bo = vmw_user_bo_ref(new_guest_memory_bo);

			 
			WARN_ON(res->coherent && !new_guest_memory_bo->dirty);

			vmw_resource_mob_attach(res);
		} else {
			res->guest_memory_bo = NULL;
		}
	} else if (switch_guest_memory && res->coherent) {
		vmw_bo_dirty_release(res->guest_memory_bo);
	}

	if (switch_guest_memory)
		res->guest_memory_offset = new_guest_memory_offset;

	if (dirty_set)
		res->res_dirty = dirty;

	if (!res->func->may_evict || res->id == -1 || res->pin_count)
		return;

	spin_lock(&dev_priv->resource_lock);
	list_add_tail(&res->lru_head,
		      &res->dev_priv->res_lru[res->func->res_type]);
	spin_unlock(&dev_priv->resource_lock);
}

 
static int
vmw_resource_check_buffer(struct ww_acquire_ctx *ticket,
			  struct vmw_resource *res,
			  bool interruptible,
			  struct ttm_validate_buffer *val_buf)
{
	struct ttm_operation_ctx ctx = { true, false };
	struct list_head val_list;
	bool guest_memory_dirty = false;
	int ret;

	if (unlikely(!res->guest_memory_bo)) {
		ret = vmw_resource_buf_alloc(res, interruptible);
		if (unlikely(ret != 0))
			return ret;
	}

	INIT_LIST_HEAD(&val_list);
	ttm_bo_get(&res->guest_memory_bo->tbo);
	val_buf->bo = &res->guest_memory_bo->tbo;
	val_buf->num_shared = 0;
	list_add_tail(&val_buf->head, &val_list);
	ret = ttm_eu_reserve_buffers(ticket, &val_list, interruptible, NULL);
	if (unlikely(ret != 0))
		goto out_no_reserve;

	if (res->func->needs_guest_memory && !vmw_resource_mob_attached(res))
		return 0;

	guest_memory_dirty = res->guest_memory_dirty;
	vmw_bo_placement_set(res->guest_memory_bo, res->func->domain,
			     res->func->busy_domain);
	ret = ttm_bo_validate(&res->guest_memory_bo->tbo,
			      &res->guest_memory_bo->placement,
			      &ctx);

	if (unlikely(ret != 0))
		goto out_no_validate;

	return 0;

out_no_validate:
	ttm_eu_backoff_reservation(ticket, &val_list);
out_no_reserve:
	ttm_bo_put(val_buf->bo);
	val_buf->bo = NULL;
	if (guest_memory_dirty)
		vmw_user_bo_unref(&res->guest_memory_bo);

	return ret;
}

 
int vmw_resource_reserve(struct vmw_resource *res, bool interruptible,
			 bool no_guest_memory)
{
	struct vmw_private *dev_priv = res->dev_priv;
	int ret;

	spin_lock(&dev_priv->resource_lock);
	list_del_init(&res->lru_head);
	spin_unlock(&dev_priv->resource_lock);

	if (res->func->needs_guest_memory && !res->guest_memory_bo &&
	    !no_guest_memory) {
		ret = vmw_resource_buf_alloc(res, interruptible);
		if (unlikely(ret != 0)) {
			DRM_ERROR("Failed to allocate a guest memory buffer "
				  "of size %lu. bytes\n",
				  (unsigned long) res->guest_memory_size);
			return ret;
		}
	}

	return 0;
}

 
static void
vmw_resource_backoff_reservation(struct ww_acquire_ctx *ticket,
				 struct ttm_validate_buffer *val_buf)
{
	struct list_head val_list;

	if (likely(val_buf->bo == NULL))
		return;

	INIT_LIST_HEAD(&val_list);
	list_add_tail(&val_buf->head, &val_list);
	ttm_eu_backoff_reservation(ticket, &val_list);
	ttm_bo_put(val_buf->bo);
	val_buf->bo = NULL;
}

 
static int vmw_resource_do_evict(struct ww_acquire_ctx *ticket,
				 struct vmw_resource *res, bool interruptible)
{
	struct ttm_validate_buffer val_buf;
	const struct vmw_res_func *func = res->func;
	int ret;

	BUG_ON(!func->may_evict);

	val_buf.bo = NULL;
	val_buf.num_shared = 0;
	ret = vmw_resource_check_buffer(ticket, res, interruptible, &val_buf);
	if (unlikely(ret != 0))
		return ret;

	if (unlikely(func->unbind != NULL &&
		     (!func->needs_guest_memory || vmw_resource_mob_attached(res)))) {
		ret = func->unbind(res, res->res_dirty, &val_buf);
		if (unlikely(ret != 0))
			goto out_no_unbind;
		vmw_resource_mob_detach(res);
	}
	ret = func->destroy(res);
	res->guest_memory_dirty = true;
	res->res_dirty = false;
out_no_unbind:
	vmw_resource_backoff_reservation(ticket, &val_buf);

	return ret;
}


 
int vmw_resource_validate(struct vmw_resource *res, bool intr,
			  bool dirtying)
{
	int ret;
	struct vmw_resource *evict_res;
	struct vmw_private *dev_priv = res->dev_priv;
	struct list_head *lru_list = &dev_priv->res_lru[res->func->res_type];
	struct ttm_validate_buffer val_buf;
	unsigned err_count = 0;

	if (!res->func->create)
		return 0;

	val_buf.bo = NULL;
	val_buf.num_shared = 0;
	if (res->guest_memory_bo)
		val_buf.bo = &res->guest_memory_bo->tbo;
	do {
		ret = vmw_resource_do_validate(res, &val_buf, dirtying);
		if (likely(ret != -EBUSY))
			break;

		spin_lock(&dev_priv->resource_lock);
		if (list_empty(lru_list) || !res->func->may_evict) {
			DRM_ERROR("Out of device device resources "
				  "for %s.\n", res->func->type_name);
			ret = -EBUSY;
			spin_unlock(&dev_priv->resource_lock);
			break;
		}

		evict_res = vmw_resource_reference
			(list_first_entry(lru_list, struct vmw_resource,
					  lru_head));
		list_del_init(&evict_res->lru_head);

		spin_unlock(&dev_priv->resource_lock);

		 
		ret = vmw_resource_do_evict(NULL, evict_res, intr);
		if (unlikely(ret != 0)) {
			spin_lock(&dev_priv->resource_lock);
			list_add_tail(&evict_res->lru_head, lru_list);
			spin_unlock(&dev_priv->resource_lock);
			if (ret == -ERESTARTSYS ||
			    ++err_count > VMW_RES_EVICT_ERR_COUNT) {
				vmw_resource_unreference(&evict_res);
				goto out_no_validate;
			}
		}

		vmw_resource_unreference(&evict_res);
	} while (1);

	if (unlikely(ret != 0))
		goto out_no_validate;
	else if (!res->func->needs_guest_memory && res->guest_memory_bo) {
		WARN_ON_ONCE(vmw_resource_mob_attached(res));
		vmw_user_bo_unref(&res->guest_memory_bo);
	}

	return 0;

out_no_validate:
	return ret;
}


 
void vmw_resource_unbind_list(struct vmw_bo *vbo)
{
	struct ttm_validate_buffer val_buf = {
		.bo = &vbo->tbo,
		.num_shared = 0
	};

	dma_resv_assert_held(vbo->tbo.base.resv);
	while (!RB_EMPTY_ROOT(&vbo->res_tree)) {
		struct rb_node *node = vbo->res_tree.rb_node;
		struct vmw_resource *res =
			container_of(node, struct vmw_resource, mob_node);

		if (!WARN_ON_ONCE(!res->func->unbind))
			(void) res->func->unbind(res, res->res_dirty, &val_buf);

		res->guest_memory_size = true;
		res->res_dirty = false;
		vmw_resource_mob_detach(res);
	}

	(void) ttm_bo_wait(&vbo->tbo, false, false);
}


 
int vmw_query_readback_all(struct vmw_bo *dx_query_mob)
{
	struct vmw_resource *dx_query_ctx;
	struct vmw_private *dev_priv;
	struct {
		SVGA3dCmdHeader header;
		SVGA3dCmdDXReadbackAllQuery body;
	} *cmd;


	 
	if (!dx_query_mob || !dx_query_mob->dx_query_ctx)
		return 0;

	dx_query_ctx = dx_query_mob->dx_query_ctx;
	dev_priv     = dx_query_ctx->dev_priv;

	cmd = VMW_CMD_CTX_RESERVE(dev_priv, sizeof(*cmd), dx_query_ctx->id);
	if (unlikely(cmd == NULL))
		return -ENOMEM;

	cmd->header.id   = SVGA_3D_CMD_DX_READBACK_ALL_QUERY;
	cmd->header.size = sizeof(cmd->body);
	cmd->body.cid    = dx_query_ctx->id;

	vmw_cmd_commit(dev_priv, sizeof(*cmd));

	 
	dx_query_mob->dx_query_ctx = NULL;

	return 0;
}



 
void vmw_query_move_notify(struct ttm_buffer_object *bo,
			   struct ttm_resource *old_mem,
			   struct ttm_resource *new_mem)
{
	struct vmw_bo *dx_query_mob;
	struct ttm_device *bdev = bo->bdev;
	struct vmw_private *dev_priv = vmw_priv_from_ttm(bdev);

	mutex_lock(&dev_priv->binding_mutex);

	 
	if (old_mem &&
	    new_mem->mem_type == TTM_PL_SYSTEM &&
	    old_mem->mem_type == VMW_PL_MOB) {
		struct vmw_fence_obj *fence;

		dx_query_mob = to_vmw_bo(&bo->base);
		if (!dx_query_mob || !dx_query_mob->dx_query_ctx) {
			mutex_unlock(&dev_priv->binding_mutex);
			return;
		}

		(void) vmw_query_readback_all(dx_query_mob);
		mutex_unlock(&dev_priv->binding_mutex);

		 
		(void) vmw_execbuf_fence_commands(NULL, dev_priv, &fence, NULL);
		vmw_bo_fence_single(bo, fence);

		if (fence != NULL)
			vmw_fence_obj_unreference(&fence);

		(void) ttm_bo_wait(bo, false, false);
	} else
		mutex_unlock(&dev_priv->binding_mutex);
}

 
bool vmw_resource_needs_backup(const struct vmw_resource *res)
{
	return res->func->needs_guest_memory;
}

 
static void vmw_resource_evict_type(struct vmw_private *dev_priv,
				    enum vmw_res_type type)
{
	struct list_head *lru_list = &dev_priv->res_lru[type];
	struct vmw_resource *evict_res;
	unsigned err_count = 0;
	int ret;
	struct ww_acquire_ctx ticket;

	do {
		spin_lock(&dev_priv->resource_lock);

		if (list_empty(lru_list))
			goto out_unlock;

		evict_res = vmw_resource_reference(
			list_first_entry(lru_list, struct vmw_resource,
					 lru_head));
		list_del_init(&evict_res->lru_head);
		spin_unlock(&dev_priv->resource_lock);

		 
		ret = vmw_resource_do_evict(&ticket, evict_res, false);
		if (unlikely(ret != 0)) {
			spin_lock(&dev_priv->resource_lock);
			list_add_tail(&evict_res->lru_head, lru_list);
			spin_unlock(&dev_priv->resource_lock);
			if (++err_count > VMW_RES_EVICT_ERR_COUNT) {
				vmw_resource_unreference(&evict_res);
				return;
			}
		}

		vmw_resource_unreference(&evict_res);
	} while (1);

out_unlock:
	spin_unlock(&dev_priv->resource_lock);
}

 
void vmw_resource_evict_all(struct vmw_private *dev_priv)
{
	enum vmw_res_type type;

	mutex_lock(&dev_priv->cmdbuf_mutex);

	for (type = 0; type < vmw_res_max; ++type)
		vmw_resource_evict_type(dev_priv, type);

	mutex_unlock(&dev_priv->cmdbuf_mutex);
}

 
int vmw_resource_pin(struct vmw_resource *res, bool interruptible)
{
	struct ttm_operation_ctx ctx = { interruptible, false };
	struct vmw_private *dev_priv = res->dev_priv;
	int ret;

	mutex_lock(&dev_priv->cmdbuf_mutex);
	ret = vmw_resource_reserve(res, interruptible, false);
	if (ret)
		goto out_no_reserve;

	if (res->pin_count == 0) {
		struct vmw_bo *vbo = NULL;

		if (res->guest_memory_bo) {
			vbo = res->guest_memory_bo;

			ret = ttm_bo_reserve(&vbo->tbo, interruptible, false, NULL);
			if (ret)
				goto out_no_validate;
			if (!vbo->tbo.pin_count) {
				vmw_bo_placement_set(vbo,
						     res->func->domain,
						     res->func->busy_domain);
				ret = ttm_bo_validate
					(&vbo->tbo,
					 &vbo->placement,
					 &ctx);
				if (ret) {
					ttm_bo_unreserve(&vbo->tbo);
					goto out_no_validate;
				}
			}

			 
			vmw_bo_pin_reserved(vbo, true);
		}
		ret = vmw_resource_validate(res, interruptible, true);
		if (vbo)
			ttm_bo_unreserve(&vbo->tbo);
		if (ret)
			goto out_no_validate;
	}
	res->pin_count++;

out_no_validate:
	vmw_resource_unreserve(res, false, false, false, NULL, 0UL);
out_no_reserve:
	mutex_unlock(&dev_priv->cmdbuf_mutex);

	return ret;
}

 
void vmw_resource_unpin(struct vmw_resource *res)
{
	struct vmw_private *dev_priv = res->dev_priv;
	int ret;

	mutex_lock(&dev_priv->cmdbuf_mutex);

	ret = vmw_resource_reserve(res, false, true);
	WARN_ON(ret);

	WARN_ON(res->pin_count == 0);
	if (--res->pin_count == 0 && res->guest_memory_bo) {
		struct vmw_bo *vbo = res->guest_memory_bo;

		(void) ttm_bo_reserve(&vbo->tbo, false, false, NULL);
		vmw_bo_pin_reserved(vbo, false);
		ttm_bo_unreserve(&vbo->tbo);
	}

	vmw_resource_unreserve(res, false, false, false, NULL, 0UL);

	mutex_unlock(&dev_priv->cmdbuf_mutex);
}

 
enum vmw_res_type vmw_res_type(const struct vmw_resource *res)
{
	return res->func->res_type;
}

 
void vmw_resource_dirty_update(struct vmw_resource *res, pgoff_t start,
			       pgoff_t end)
{
	if (res->dirty)
		res->func->dirty_range_add(res, start << PAGE_SHIFT,
					   end << PAGE_SHIFT);
}

 
int vmw_resources_clean(struct vmw_bo *vbo, pgoff_t start,
			pgoff_t end, pgoff_t *num_prefault)
{
	struct rb_node *cur = vbo->res_tree.rb_node;
	struct vmw_resource *found = NULL;
	unsigned long res_start = start << PAGE_SHIFT;
	unsigned long res_end = end << PAGE_SHIFT;
	unsigned long last_cleaned = 0;

	 
	while (cur) {
		struct vmw_resource *cur_res =
			container_of(cur, struct vmw_resource, mob_node);

		if (cur_res->guest_memory_offset >= res_end) {
			cur = cur->rb_left;
		} else if (cur_res->guest_memory_offset + cur_res->guest_memory_size <=
			   res_start) {
			cur = cur->rb_right;
		} else {
			found = cur_res;
			cur = cur->rb_left;
			 
		}
	}

	 
	while (found) {
		if (found->res_dirty) {
			int ret;

			if (!found->func->clean)
				return -EINVAL;

			ret = found->func->clean(found);
			if (ret)
				return ret;

			found->res_dirty = false;
		}
		last_cleaned = found->guest_memory_offset + found->guest_memory_size;
		cur = rb_next(&found->mob_node);
		if (!cur)
			break;

		found = container_of(cur, struct vmw_resource, mob_node);
		if (found->guest_memory_offset >= res_end)
			break;
	}

	 
	*num_prefault = 1;
	if (last_cleaned > res_start) {
		struct ttm_buffer_object *bo = &vbo->tbo;

		*num_prefault = __KERNEL_DIV_ROUND_UP(last_cleaned - res_start,
						      PAGE_SIZE);
		vmw_bo_fence_single(bo, NULL);
	}

	return 0;
}
