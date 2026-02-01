
 
 

#include "vmwgfx_bo.h"
#include "vmwgfx_drv.h"
#include "vmwgfx_mksstat.h"
#include "vmwgfx_resource_priv.h"
#include "vmwgfx_so.h"

#include <drm/ttm/ttm_placement.h>

 
struct vmw_cotable {
	struct vmw_resource res;
	struct vmw_resource *ctx;
	size_t size_read_back;
	int seen_entries;
	u32 type;
	bool scrubbed;
	struct list_head resource_list;
};

 
struct vmw_cotable_info {
	u32 min_initial_entries;
	u32 size;
	void (*unbind_func)(struct vmw_private *, struct list_head *,
			    bool);
};


 
static const struct vmw_cotable_info co_info[] = {
	{1, sizeof(SVGACOTableDXRTViewEntry), &vmw_view_cotable_list_destroy},
	{1, sizeof(SVGACOTableDXDSViewEntry), &vmw_view_cotable_list_destroy},
	{1, sizeof(SVGACOTableDXSRViewEntry), &vmw_view_cotable_list_destroy},
	{PAGE_SIZE/sizeof(SVGACOTableDXElementLayoutEntry) + 1, sizeof(SVGACOTableDXElementLayoutEntry), NULL},
	{PAGE_SIZE/sizeof(SVGACOTableDXBlendStateEntry) + 1, sizeof(SVGACOTableDXBlendStateEntry), NULL},
	{1, sizeof(SVGACOTableDXDepthStencilEntry), NULL},
	{1, sizeof(SVGACOTableDXRasterizerStateEntry), NULL},
	{1, sizeof(SVGACOTableDXSamplerEntry), NULL},
	{1, sizeof(SVGACOTableDXStreamOutputEntry), &vmw_dx_streamoutput_cotable_list_scrub},
	{1, sizeof(SVGACOTableDXQueryEntry), NULL},
	{1, sizeof(SVGACOTableDXShaderEntry), &vmw_dx_shader_cotable_list_scrub},
	{1, sizeof(SVGACOTableDXUAViewEntry), &vmw_view_cotable_list_destroy}
};

 
const SVGACOTableType vmw_cotable_scrub_order[] = {
	SVGA_COTABLE_RTVIEW,
	SVGA_COTABLE_DSVIEW,
	SVGA_COTABLE_SRVIEW,
	SVGA_COTABLE_DXSHADER,
	SVGA_COTABLE_ELEMENTLAYOUT,
	SVGA_COTABLE_BLENDSTATE,
	SVGA_COTABLE_DEPTHSTENCIL,
	SVGA_COTABLE_RASTERIZERSTATE,
	SVGA_COTABLE_SAMPLER,
	SVGA_COTABLE_STREAMOUTPUT,
	SVGA_COTABLE_DXQUERY,
	SVGA_COTABLE_UAVIEW,
};

static int vmw_cotable_bind(struct vmw_resource *res,
			    struct ttm_validate_buffer *val_buf);
static int vmw_cotable_unbind(struct vmw_resource *res,
			      bool readback,
			      struct ttm_validate_buffer *val_buf);
static int vmw_cotable_create(struct vmw_resource *res);
static int vmw_cotable_destroy(struct vmw_resource *res);

static const struct vmw_res_func vmw_cotable_func = {
	.res_type = vmw_res_cotable,
	.needs_guest_memory = true,
	.may_evict = true,
	.prio = 3,
	.dirty_prio = 3,
	.type_name = "context guest backed object tables",
	.domain = VMW_BO_DOMAIN_MOB,
	.busy_domain = VMW_BO_DOMAIN_MOB,
	.create = vmw_cotable_create,
	.destroy = vmw_cotable_destroy,
	.bind = vmw_cotable_bind,
	.unbind = vmw_cotable_unbind,
};

 
static struct vmw_cotable *vmw_cotable(struct vmw_resource *res)
{
	return container_of(res, struct vmw_cotable, res);
}

 
static int vmw_cotable_destroy(struct vmw_resource *res)
{
	res->id = -1;
	return 0;
}

 
static int vmw_cotable_unscrub(struct vmw_resource *res)
{
	struct vmw_cotable *vcotbl = vmw_cotable(res);
	struct vmw_private *dev_priv = res->dev_priv;
	struct ttm_buffer_object *bo = &res->guest_memory_bo->tbo;
	struct {
		SVGA3dCmdHeader header;
		SVGA3dCmdDXSetCOTable body;
	} *cmd;

	WARN_ON_ONCE(bo->resource->mem_type != VMW_PL_MOB);
	dma_resv_assert_held(bo->base.resv);

	cmd = VMW_CMD_RESERVE(dev_priv, sizeof(*cmd));
	if (!cmd)
		return -ENOMEM;

	WARN_ON(vcotbl->ctx->id == SVGA3D_INVALID_ID);
	WARN_ON(bo->resource->mem_type != VMW_PL_MOB);
	cmd->header.id = SVGA_3D_CMD_DX_SET_COTABLE;
	cmd->header.size = sizeof(cmd->body);
	cmd->body.cid = vcotbl->ctx->id;
	cmd->body.type = vcotbl->type;
	cmd->body.mobid = bo->resource->start;
	cmd->body.validSizeInBytes = vcotbl->size_read_back;

	vmw_cmd_commit_flush(dev_priv, sizeof(*cmd));
	vcotbl->scrubbed = false;

	return 0;
}

 
static int vmw_cotable_bind(struct vmw_resource *res,
			    struct ttm_validate_buffer *val_buf)
{
	 
	val_buf->bo = &res->guest_memory_bo->tbo;

	return vmw_cotable_unscrub(res);
}

 
int vmw_cotable_scrub(struct vmw_resource *res, bool readback)
{
	struct vmw_cotable *vcotbl = vmw_cotable(res);
	struct vmw_private *dev_priv = res->dev_priv;
	size_t submit_size;

	struct {
		SVGA3dCmdHeader header;
		SVGA3dCmdDXReadbackCOTable body;
	} *cmd0;
	struct {
		SVGA3dCmdHeader header;
		SVGA3dCmdDXSetCOTable body;
	} *cmd1;

	if (vcotbl->scrubbed)
		return 0;

	if (co_info[vcotbl->type].unbind_func)
		co_info[vcotbl->type].unbind_func(dev_priv,
						  &vcotbl->resource_list,
						  readback);
	submit_size = sizeof(*cmd1);
	if (readback)
		submit_size += sizeof(*cmd0);

	cmd1 = VMW_CMD_RESERVE(dev_priv, submit_size);
	if (!cmd1)
		return -ENOMEM;

	vcotbl->size_read_back = 0;
	if (readback) {
		cmd0 = (void *) cmd1;
		cmd0->header.id = SVGA_3D_CMD_DX_READBACK_COTABLE;
		cmd0->header.size = sizeof(cmd0->body);
		cmd0->body.cid = vcotbl->ctx->id;
		cmd0->body.type = vcotbl->type;
		cmd1 = (void *) &cmd0[1];
		vcotbl->size_read_back = res->guest_memory_size;
	}
	cmd1->header.id = SVGA_3D_CMD_DX_SET_COTABLE;
	cmd1->header.size = sizeof(cmd1->body);
	cmd1->body.cid = vcotbl->ctx->id;
	cmd1->body.type = vcotbl->type;
	cmd1->body.mobid = SVGA3D_INVALID_ID;
	cmd1->body.validSizeInBytes = 0;
	vmw_cmd_commit_flush(dev_priv, submit_size);
	vcotbl->scrubbed = true;

	 
	res->id = -1;

	return 0;
}

 
static int vmw_cotable_unbind(struct vmw_resource *res,
			      bool readback,
			      struct ttm_validate_buffer *val_buf)
{
	struct vmw_cotable *vcotbl = vmw_cotable(res);
	struct vmw_private *dev_priv = res->dev_priv;
	struct ttm_buffer_object *bo = val_buf->bo;
	struct vmw_fence_obj *fence;

	if (!vmw_resource_mob_attached(res))
		return 0;

	WARN_ON_ONCE(bo->resource->mem_type != VMW_PL_MOB);
	dma_resv_assert_held(bo->base.resv);

	mutex_lock(&dev_priv->binding_mutex);
	if (!vcotbl->scrubbed)
		vmw_dx_context_scrub_cotables(vcotbl->ctx, readback);
	mutex_unlock(&dev_priv->binding_mutex);
	(void) vmw_execbuf_fence_commands(NULL, dev_priv, &fence, NULL);
	vmw_bo_fence_single(bo, fence);
	if (likely(fence != NULL))
		vmw_fence_obj_unreference(&fence);

	return 0;
}

 
static int vmw_cotable_readback(struct vmw_resource *res)
{
	struct vmw_cotable *vcotbl = vmw_cotable(res);
	struct vmw_private *dev_priv = res->dev_priv;

	struct {
		SVGA3dCmdHeader header;
		SVGA3dCmdDXReadbackCOTable body;
	} *cmd;
	struct vmw_fence_obj *fence;

	if (!vcotbl->scrubbed) {
		cmd = VMW_CMD_RESERVE(dev_priv, sizeof(*cmd));
		if (!cmd)
			return -ENOMEM;

		cmd->header.id = SVGA_3D_CMD_DX_READBACK_COTABLE;
		cmd->header.size = sizeof(cmd->body);
		cmd->body.cid = vcotbl->ctx->id;
		cmd->body.type = vcotbl->type;
		vcotbl->size_read_back = res->guest_memory_size;
		vmw_cmd_commit(dev_priv, sizeof(*cmd));
	}

	(void) vmw_execbuf_fence_commands(NULL, dev_priv, &fence, NULL);
	vmw_bo_fence_single(&res->guest_memory_bo->tbo, fence);
	vmw_fence_obj_unreference(&fence);

	return 0;
}

 
static int vmw_cotable_resize(struct vmw_resource *res, size_t new_size)
{
	struct ttm_operation_ctx ctx = { false, false };
	struct vmw_private *dev_priv = res->dev_priv;
	struct vmw_cotable *vcotbl = vmw_cotable(res);
	struct vmw_bo *buf, *old_buf = res->guest_memory_bo;
	struct ttm_buffer_object *bo, *old_bo = &res->guest_memory_bo->tbo;
	size_t old_size = res->guest_memory_size;
	size_t old_size_read_back = vcotbl->size_read_back;
	size_t cur_size_read_back;
	struct ttm_bo_kmap_obj old_map, new_map;
	int ret;
	size_t i;
	struct vmw_bo_params bo_params = {
		.domain = VMW_BO_DOMAIN_MOB,
		.busy_domain = VMW_BO_DOMAIN_MOB,
		.bo_type = ttm_bo_type_device,
		.size = new_size,
		.pin = true
	};

	MKS_STAT_TIME_DECL(MKSSTAT_KERN_COTABLE_RESIZE);
	MKS_STAT_TIME_PUSH(MKSSTAT_KERN_COTABLE_RESIZE);

	ret = vmw_cotable_readback(res);
	if (ret)
		goto out_done;

	cur_size_read_back = vcotbl->size_read_back;
	vcotbl->size_read_back = old_size_read_back;

	 
	ret = vmw_gem_object_create(dev_priv, &bo_params, &buf);
	if (ret) {
		DRM_ERROR("Failed initializing new cotable MOB.\n");
		goto out_done;
	}

	bo = &buf->tbo;
	WARN_ON_ONCE(ttm_bo_reserve(bo, false, true, NULL));

	ret = ttm_bo_wait(old_bo, false, false);
	if (unlikely(ret != 0)) {
		DRM_ERROR("Failed waiting for cotable unbind.\n");
		goto out_wait;
	}

	 
	for (i = 0; i < PFN_UP(old_bo->resource->size); ++i) {
		bool dummy;

		ret = ttm_bo_kmap(old_bo, i, 1, &old_map);
		if (unlikely(ret != 0)) {
			DRM_ERROR("Failed mapping old COTable on resize.\n");
			goto out_wait;
		}
		ret = ttm_bo_kmap(bo, i, 1, &new_map);
		if (unlikely(ret != 0)) {
			DRM_ERROR("Failed mapping new COTable on resize.\n");
			goto out_map_new;
		}
		memcpy(ttm_kmap_obj_virtual(&new_map, &dummy),
		       ttm_kmap_obj_virtual(&old_map, &dummy),
		       PAGE_SIZE);
		ttm_bo_kunmap(&new_map);
		ttm_bo_kunmap(&old_map);
	}

	 
	vmw_bo_placement_set(buf,
			     VMW_BO_DOMAIN_MOB,
			     VMW_BO_DOMAIN_MOB);
	ret = ttm_bo_validate(bo, &buf->placement, &ctx);
	if (unlikely(ret != 0)) {
		DRM_ERROR("Failed validating new COTable backup buffer.\n");
		goto out_wait;
	}

	vmw_resource_mob_detach(res);
	res->guest_memory_bo = buf;
	res->guest_memory_size = new_size;
	vcotbl->size_read_back = cur_size_read_back;

	 
	ret = vmw_cotable_unscrub(res);
	if (ret) {
		DRM_ERROR("Failed switching COTable backup buffer.\n");
		res->guest_memory_bo = old_buf;
		res->guest_memory_size = old_size;
		vcotbl->size_read_back = old_size_read_back;
		vmw_resource_mob_attach(res);
		goto out_wait;
	}

	vmw_resource_mob_attach(res);
	 
	vmw_user_bo_unref(&old_buf);
	res->id = vcotbl->type;

	ret = dma_resv_reserve_fences(bo->base.resv, 1);
	if (unlikely(ret))
		goto out_wait;

	 
	ttm_bo_unpin(bo);

	MKS_STAT_TIME_POP(MKSSTAT_KERN_COTABLE_RESIZE);

	return 0;

out_map_new:
	ttm_bo_kunmap(&old_map);
out_wait:
	ttm_bo_unpin(bo);
	ttm_bo_unreserve(bo);
	vmw_user_bo_unref(&buf);

out_done:
	MKS_STAT_TIME_POP(MKSSTAT_KERN_COTABLE_RESIZE);

	return ret;
}

 
static int vmw_cotable_create(struct vmw_resource *res)
{
	struct vmw_cotable *vcotbl = vmw_cotable(res);
	size_t new_size = res->guest_memory_size;
	size_t needed_size;
	int ret;

	 
	needed_size = (vcotbl->seen_entries + 1) * co_info[vcotbl->type].size;
	while (needed_size > new_size)
		new_size *= 2;

	if (likely(new_size <= res->guest_memory_size)) {
		if (vcotbl->scrubbed && vmw_resource_mob_attached(res)) {
			ret = vmw_cotable_unscrub(res);
			if (ret)
				return ret;
		}
		res->id = vcotbl->type;
		return 0;
	}

	return vmw_cotable_resize(res, new_size);
}

 
static void vmw_hw_cotable_destroy(struct vmw_resource *res)
{
	(void) vmw_cotable_destroy(res);
}

 
static void vmw_cotable_free(struct vmw_resource *res)
{
	kfree(res);
}

 
struct vmw_resource *vmw_cotable_alloc(struct vmw_private *dev_priv,
				       struct vmw_resource *ctx,
				       u32 type)
{
	struct vmw_cotable *vcotbl;
	int ret;
	u32 num_entries;

	vcotbl = kzalloc(sizeof(*vcotbl), GFP_KERNEL);
	if (unlikely(!vcotbl)) {
		ret = -ENOMEM;
		goto out_no_alloc;
	}

	ret = vmw_resource_init(dev_priv, &vcotbl->res, true,
				vmw_cotable_free, &vmw_cotable_func);
	if (unlikely(ret != 0))
		goto out_no_init;

	INIT_LIST_HEAD(&vcotbl->resource_list);
	vcotbl->res.id = type;
	vcotbl->res.guest_memory_size = PAGE_SIZE;
	num_entries = PAGE_SIZE / co_info[type].size;
	if (num_entries < co_info[type].min_initial_entries) {
		vcotbl->res.guest_memory_size = co_info[type].min_initial_entries *
			co_info[type].size;
		vcotbl->res.guest_memory_size = PFN_ALIGN(vcotbl->res.guest_memory_size);
	}

	vcotbl->scrubbed = true;
	vcotbl->seen_entries = -1;
	vcotbl->type = type;
	vcotbl->ctx = ctx;

	vcotbl->res.hw_destroy = vmw_hw_cotable_destroy;

	return &vcotbl->res;

out_no_init:
	kfree(vcotbl);
out_no_alloc:
	return ERR_PTR(ret);
}

 
int vmw_cotable_notify(struct vmw_resource *res, int id)
{
	struct vmw_cotable *vcotbl = vmw_cotable(res);

	if (id < 0 || id >= SVGA_COTABLE_MAX_IDS) {
		DRM_ERROR("Illegal COTable id. Type is %u. Id is %d\n",
			  (unsigned) vcotbl->type, id);
		return -EINVAL;
	}

	if (vcotbl->seen_entries < id) {
		 
		res->id = -1;
		vcotbl->seen_entries = id;
	}

	return 0;
}

 
void vmw_cotable_add_resource(struct vmw_resource *res, struct list_head *head)
{
	struct vmw_cotable *vcotbl =
		container_of(res, struct vmw_cotable, res);

	list_add_tail(head, &vcotbl->resource_list);
}
