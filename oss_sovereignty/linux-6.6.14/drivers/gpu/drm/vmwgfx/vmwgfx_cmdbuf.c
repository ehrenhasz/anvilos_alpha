
 

#include "vmwgfx_bo.h"
#include "vmwgfx_drv.h"

#include <drm/ttm/ttm_bo.h>

#include <linux/dmapool.h>
#include <linux/pci.h>

 
#define VMW_CMDBUF_INLINE_ALIGN 64
#define VMW_CMDBUF_INLINE_SIZE \
	(1024 - ALIGN(sizeof(SVGACBHeader), VMW_CMDBUF_INLINE_ALIGN))

 
struct vmw_cmdbuf_context {
	struct list_head submitted;
	struct list_head hw_submitted;
	struct list_head preempted;
	unsigned num_hw_submitted;
	bool block_submission;
};

 
struct vmw_cmdbuf_man {
	struct mutex cur_mutex;
	struct mutex space_mutex;
	struct mutex error_mutex;
	struct work_struct work;
	struct vmw_private *dev_priv;
	struct vmw_cmdbuf_context ctx[SVGA_CB_CONTEXT_MAX];
	struct list_head error;
	struct drm_mm mm;
	struct vmw_bo *cmd_space;
	u8 *map;
	struct vmw_cmdbuf_header *cur;
	size_t cur_pos;
	size_t default_size;
	unsigned max_hw_submitted;
	spinlock_t lock;
	struct dma_pool *headers;
	struct dma_pool *dheaders;
	wait_queue_head_t alloc_queue;
	wait_queue_head_t idle_queue;
	bool irq_on;
	bool using_mob;
	bool has_pool;
	dma_addr_t handle;
	size_t size;
	u32 num_contexts;
};

 
struct vmw_cmdbuf_header {
	struct vmw_cmdbuf_man *man;
	SVGACBHeader *cb_header;
	SVGACBContext cb_context;
	struct list_head list;
	struct drm_mm_node node;
	dma_addr_t handle;
	u8 *cmd;
	size_t size;
	size_t reserved;
	bool inline_space;
};

 
struct vmw_cmdbuf_dheader {
	SVGACBHeader cb_header;
	u8 cmd[VMW_CMDBUF_INLINE_SIZE] __aligned(VMW_CMDBUF_INLINE_ALIGN);
};

 
struct vmw_cmdbuf_alloc_info {
	size_t page_size;
	struct drm_mm_node *node;
	bool done;
};

 
#define for_each_cmdbuf_ctx(_man, _i, _ctx)				\
	for (_i = 0, _ctx = &(_man)->ctx[0]; (_i) < (_man)->num_contexts; \
	     ++(_i), ++(_ctx))

static int vmw_cmdbuf_startstop(struct vmw_cmdbuf_man *man, u32 context,
				bool enable);
static int vmw_cmdbuf_preempt(struct vmw_cmdbuf_man *man, u32 context);

 
static int vmw_cmdbuf_cur_lock(struct vmw_cmdbuf_man *man, bool interruptible)
{
	if (interruptible) {
		if (mutex_lock_interruptible(&man->cur_mutex))
			return -ERESTARTSYS;
	} else {
		mutex_lock(&man->cur_mutex);
	}

	return 0;
}

 
static void vmw_cmdbuf_cur_unlock(struct vmw_cmdbuf_man *man)
{
	mutex_unlock(&man->cur_mutex);
}

 
static void vmw_cmdbuf_header_inline_free(struct vmw_cmdbuf_header *header)
{
	struct vmw_cmdbuf_dheader *dheader;

	if (WARN_ON_ONCE(!header->inline_space))
		return;

	dheader = container_of(header->cb_header, struct vmw_cmdbuf_dheader,
			       cb_header);
	dma_pool_free(header->man->dheaders, dheader, header->handle);
	kfree(header);
}

 
static void __vmw_cmdbuf_header_free(struct vmw_cmdbuf_header *header)
{
	struct vmw_cmdbuf_man *man = header->man;

	lockdep_assert_held_once(&man->lock);

	if (header->inline_space) {
		vmw_cmdbuf_header_inline_free(header);
		return;
	}

	drm_mm_remove_node(&header->node);
	wake_up_all(&man->alloc_queue);
	if (header->cb_header)
		dma_pool_free(man->headers, header->cb_header,
			      header->handle);
	kfree(header);
}

 
void vmw_cmdbuf_header_free(struct vmw_cmdbuf_header *header)
{
	struct vmw_cmdbuf_man *man = header->man;

	 
	if (header->inline_space) {
		vmw_cmdbuf_header_inline_free(header);
		return;
	}
	spin_lock(&man->lock);
	__vmw_cmdbuf_header_free(header);
	spin_unlock(&man->lock);
}


 
static int vmw_cmdbuf_header_submit(struct vmw_cmdbuf_header *header)
{
	struct vmw_cmdbuf_man *man = header->man;
	u32 val;

	val = upper_32_bits(header->handle);
	vmw_write(man->dev_priv, SVGA_REG_COMMAND_HIGH, val);

	val = lower_32_bits(header->handle);
	val |= header->cb_context & SVGA_CB_CONTEXT_MASK;
	vmw_write(man->dev_priv, SVGA_REG_COMMAND_LOW, val);

	return header->cb_header->status;
}

 
static void vmw_cmdbuf_ctx_init(struct vmw_cmdbuf_context *ctx)
{
	INIT_LIST_HEAD(&ctx->hw_submitted);
	INIT_LIST_HEAD(&ctx->submitted);
	INIT_LIST_HEAD(&ctx->preempted);
	ctx->num_hw_submitted = 0;
}

 
static void vmw_cmdbuf_ctx_submit(struct vmw_cmdbuf_man *man,
				  struct vmw_cmdbuf_context *ctx)
{
	while (ctx->num_hw_submitted < man->max_hw_submitted &&
	       !list_empty(&ctx->submitted) &&
	       !ctx->block_submission) {
		struct vmw_cmdbuf_header *entry;
		SVGACBStatus status;

		entry = list_first_entry(&ctx->submitted,
					 struct vmw_cmdbuf_header,
					 list);

		status = vmw_cmdbuf_header_submit(entry);

		 
		if (WARN_ON_ONCE(status == SVGA_CB_STATUS_QUEUE_FULL)) {
			entry->cb_header->status = SVGA_CB_STATUS_NONE;
			break;
		}

		list_move_tail(&entry->list, &ctx->hw_submitted);
		ctx->num_hw_submitted++;
	}

}

 
static void vmw_cmdbuf_ctx_process(struct vmw_cmdbuf_man *man,
				   struct vmw_cmdbuf_context *ctx,
				   int *notempty)
{
	struct vmw_cmdbuf_header *entry, *next;

	vmw_cmdbuf_ctx_submit(man, ctx);

	list_for_each_entry_safe(entry, next, &ctx->hw_submitted, list) {
		SVGACBStatus status = entry->cb_header->status;

		if (status == SVGA_CB_STATUS_NONE)
			break;

		list_del(&entry->list);
		wake_up_all(&man->idle_queue);
		ctx->num_hw_submitted--;
		switch (status) {
		case SVGA_CB_STATUS_COMPLETED:
			__vmw_cmdbuf_header_free(entry);
			break;
		case SVGA_CB_STATUS_COMMAND_ERROR:
			WARN_ONCE(true, "Command buffer error.\n");
			entry->cb_header->status = SVGA_CB_STATUS_NONE;
			list_add_tail(&entry->list, &man->error);
			schedule_work(&man->work);
			break;
		case SVGA_CB_STATUS_PREEMPTED:
			entry->cb_header->status = SVGA_CB_STATUS_NONE;
			list_add_tail(&entry->list, &ctx->preempted);
			break;
		case SVGA_CB_STATUS_CB_HEADER_ERROR:
			WARN_ONCE(true, "Command buffer header error.\n");
			__vmw_cmdbuf_header_free(entry);
			break;
		default:
			WARN_ONCE(true, "Undefined command buffer status.\n");
			__vmw_cmdbuf_header_free(entry);
			break;
		}
	}

	vmw_cmdbuf_ctx_submit(man, ctx);
	if (!list_empty(&ctx->submitted))
		(*notempty)++;
}

 
static void vmw_cmdbuf_man_process(struct vmw_cmdbuf_man *man)
{
	int notempty;
	struct vmw_cmdbuf_context *ctx;
	int i;

retry:
	notempty = 0;
	for_each_cmdbuf_ctx(man, i, ctx)
		vmw_cmdbuf_ctx_process(man, ctx, &notempty);

	if (man->irq_on && !notempty) {
		vmw_generic_waiter_remove(man->dev_priv,
					  SVGA_IRQFLAG_COMMAND_BUFFER,
					  &man->dev_priv->cmdbuf_waiters);
		man->irq_on = false;
	} else if (!man->irq_on && notempty) {
		vmw_generic_waiter_add(man->dev_priv,
				       SVGA_IRQFLAG_COMMAND_BUFFER,
				       &man->dev_priv->cmdbuf_waiters);
		man->irq_on = true;

		 
		goto retry;
	}
}

 
static void vmw_cmdbuf_ctx_add(struct vmw_cmdbuf_man *man,
			       struct vmw_cmdbuf_header *header,
			       SVGACBContext cb_context)
{
	if (!(header->cb_header->flags & SVGA_CB_FLAG_DX_CONTEXT))
		header->cb_header->dxContext = 0;
	header->cb_context = cb_context;
	list_add_tail(&header->list, &man->ctx[cb_context].submitted);

	vmw_cmdbuf_man_process(man);
}

 
void vmw_cmdbuf_irqthread(struct vmw_cmdbuf_man *man)
{
	spin_lock(&man->lock);
	vmw_cmdbuf_man_process(man);
	spin_unlock(&man->lock);
}

 
static void vmw_cmdbuf_work_func(struct work_struct *work)
{
	struct vmw_cmdbuf_man *man =
		container_of(work, struct vmw_cmdbuf_man, work);
	struct vmw_cmdbuf_header *entry, *next;
	uint32_t dummy = 0;
	bool send_fence = false;
	struct list_head restart_head[SVGA_CB_CONTEXT_MAX];
	int i;
	struct vmw_cmdbuf_context *ctx;
	bool global_block = false;

	for_each_cmdbuf_ctx(man, i, ctx)
		INIT_LIST_HEAD(&restart_head[i]);

	mutex_lock(&man->error_mutex);
	spin_lock(&man->lock);
	list_for_each_entry_safe(entry, next, &man->error, list) {
		SVGACBHeader *cb_hdr = entry->cb_header;
		SVGA3dCmdHeader *header = (SVGA3dCmdHeader *)
			(entry->cmd + cb_hdr->errorOffset);
		u32 error_cmd_size, new_start_offset;
		const char *cmd_name;

		list_del_init(&entry->list);
		global_block = true;

		if (!vmw_cmd_describe(header, &error_cmd_size, &cmd_name)) {
			VMW_DEBUG_USER("Unknown command causing device error.\n");
			VMW_DEBUG_USER("Command buffer offset is %lu\n",
				       (unsigned long) cb_hdr->errorOffset);
			__vmw_cmdbuf_header_free(entry);
			send_fence = true;
			continue;
		}

		VMW_DEBUG_USER("Command \"%s\" causing device error.\n",
			       cmd_name);
		VMW_DEBUG_USER("Command buffer offset is %lu\n",
			       (unsigned long) cb_hdr->errorOffset);
		VMW_DEBUG_USER("Command size is %lu\n",
			       (unsigned long) error_cmd_size);

		new_start_offset = cb_hdr->errorOffset + error_cmd_size;

		if (new_start_offset >= cb_hdr->length) {
			__vmw_cmdbuf_header_free(entry);
			send_fence = true;
			continue;
		}

		if (man->using_mob)
			cb_hdr->ptr.mob.mobOffset += new_start_offset;
		else
			cb_hdr->ptr.pa += (u64) new_start_offset;

		entry->cmd += new_start_offset;
		cb_hdr->length -= new_start_offset;
		cb_hdr->errorOffset = 0;
		cb_hdr->offset = 0;

		list_add_tail(&entry->list, &restart_head[entry->cb_context]);
	}

	for_each_cmdbuf_ctx(man, i, ctx)
		man->ctx[i].block_submission = true;

	spin_unlock(&man->lock);

	 
	if (global_block && vmw_cmdbuf_preempt(man, 0))
		DRM_ERROR("Failed preempting command buffer contexts\n");

	spin_lock(&man->lock);
	for_each_cmdbuf_ctx(man, i, ctx) {
		 
		vmw_cmdbuf_ctx_process(man, ctx, &dummy);

		 
		list_splice_init(&ctx->preempted, restart_head[i].prev);

		 

		ctx->block_submission = false;
		list_splice_init(&restart_head[i], &ctx->submitted);
	}

	vmw_cmdbuf_man_process(man);
	spin_unlock(&man->lock);

	if (global_block && vmw_cmdbuf_startstop(man, 0, true))
		DRM_ERROR("Failed restarting command buffer contexts\n");

	 
	if (send_fence) {
		vmw_cmd_send_fence(man->dev_priv, &dummy);
		wake_up_all(&man->idle_queue);
	}

	mutex_unlock(&man->error_mutex);
}

 
static bool vmw_cmdbuf_man_idle(struct vmw_cmdbuf_man *man,
				bool check_preempted)
{
	struct vmw_cmdbuf_context *ctx;
	bool idle = false;
	int i;

	spin_lock(&man->lock);
	vmw_cmdbuf_man_process(man);
	for_each_cmdbuf_ctx(man, i, ctx) {
		if (!list_empty(&ctx->submitted) ||
		    !list_empty(&ctx->hw_submitted) ||
		    (check_preempted && !list_empty(&ctx->preempted)))
			goto out_unlock;
	}

	idle = list_empty(&man->error);

out_unlock:
	spin_unlock(&man->lock);

	return idle;
}

 
static void __vmw_cmdbuf_cur_flush(struct vmw_cmdbuf_man *man)
{
	struct vmw_cmdbuf_header *cur = man->cur;

	lockdep_assert_held_once(&man->cur_mutex);

	if (!cur)
		return;

	spin_lock(&man->lock);
	if (man->cur_pos == 0) {
		__vmw_cmdbuf_header_free(cur);
		goto out_unlock;
	}

	man->cur->cb_header->length = man->cur_pos;
	vmw_cmdbuf_ctx_add(man, man->cur, SVGA_CB_CONTEXT_0);
out_unlock:
	spin_unlock(&man->lock);
	man->cur = NULL;
	man->cur_pos = 0;
}

 
int vmw_cmdbuf_cur_flush(struct vmw_cmdbuf_man *man,
			 bool interruptible)
{
	int ret = vmw_cmdbuf_cur_lock(man, interruptible);

	if (ret)
		return ret;

	__vmw_cmdbuf_cur_flush(man);
	vmw_cmdbuf_cur_unlock(man);

	return 0;
}

 
int vmw_cmdbuf_idle(struct vmw_cmdbuf_man *man, bool interruptible,
		    unsigned long timeout)
{
	int ret;

	ret = vmw_cmdbuf_cur_flush(man, interruptible);
	vmw_generic_waiter_add(man->dev_priv,
			       SVGA_IRQFLAG_COMMAND_BUFFER,
			       &man->dev_priv->cmdbuf_waiters);

	if (interruptible) {
		ret = wait_event_interruptible_timeout
			(man->idle_queue, vmw_cmdbuf_man_idle(man, true),
			 timeout);
	} else {
		ret = wait_event_timeout
			(man->idle_queue, vmw_cmdbuf_man_idle(man, true),
			 timeout);
	}
	vmw_generic_waiter_remove(man->dev_priv,
				  SVGA_IRQFLAG_COMMAND_BUFFER,
				  &man->dev_priv->cmdbuf_waiters);
	if (ret == 0) {
		if (!vmw_cmdbuf_man_idle(man, true))
			ret = -EBUSY;
		else
			ret = 0;
	}
	if (ret > 0)
		ret = 0;

	return ret;
}

 
static bool vmw_cmdbuf_try_alloc(struct vmw_cmdbuf_man *man,
				 struct vmw_cmdbuf_alloc_info *info)
{
	int ret;

	if (info->done)
		return true;

	memset(info->node, 0, sizeof(*info->node));
	spin_lock(&man->lock);
	ret = drm_mm_insert_node(&man->mm, info->node, info->page_size);
	if (ret) {
		vmw_cmdbuf_man_process(man);
		ret = drm_mm_insert_node(&man->mm, info->node, info->page_size);
	}

	spin_unlock(&man->lock);
	info->done = !ret;

	return info->done;
}

 
static int vmw_cmdbuf_alloc_space(struct vmw_cmdbuf_man *man,
				  struct drm_mm_node *node,
				  size_t size,
				  bool interruptible)
{
	struct vmw_cmdbuf_alloc_info info;

	info.page_size = PFN_UP(size);
	info.node = node;
	info.done = false;

	 
	if (interruptible) {
		if (mutex_lock_interruptible(&man->space_mutex))
			return -ERESTARTSYS;
	} else {
		mutex_lock(&man->space_mutex);
	}

	 
	if (vmw_cmdbuf_try_alloc(man, &info))
		goto out_unlock;

	vmw_generic_waiter_add(man->dev_priv,
			       SVGA_IRQFLAG_COMMAND_BUFFER,
			       &man->dev_priv->cmdbuf_waiters);

	if (interruptible) {
		int ret;

		ret = wait_event_interruptible
			(man->alloc_queue, vmw_cmdbuf_try_alloc(man, &info));
		if (ret) {
			vmw_generic_waiter_remove
				(man->dev_priv, SVGA_IRQFLAG_COMMAND_BUFFER,
				 &man->dev_priv->cmdbuf_waiters);
			mutex_unlock(&man->space_mutex);
			return ret;
		}
	} else {
		wait_event(man->alloc_queue, vmw_cmdbuf_try_alloc(man, &info));
	}
	vmw_generic_waiter_remove(man->dev_priv,
				  SVGA_IRQFLAG_COMMAND_BUFFER,
				  &man->dev_priv->cmdbuf_waiters);

out_unlock:
	mutex_unlock(&man->space_mutex);

	return 0;
}

 
static int vmw_cmdbuf_space_pool(struct vmw_cmdbuf_man *man,
				 struct vmw_cmdbuf_header *header,
				 size_t size,
				 bool interruptible)
{
	SVGACBHeader *cb_hdr;
	size_t offset;
	int ret;

	if (!man->has_pool)
		return -ENOMEM;

	ret = vmw_cmdbuf_alloc_space(man, &header->node,  size, interruptible);

	if (ret)
		return ret;

	header->cb_header = dma_pool_zalloc(man->headers, GFP_KERNEL,
					    &header->handle);
	if (!header->cb_header) {
		ret = -ENOMEM;
		goto out_no_cb_header;
	}

	header->size = header->node.size << PAGE_SHIFT;
	cb_hdr = header->cb_header;
	offset = header->node.start << PAGE_SHIFT;
	header->cmd = man->map + offset;
	if (man->using_mob) {
		cb_hdr->flags = SVGA_CB_FLAG_MOB;
		cb_hdr->ptr.mob.mobid = man->cmd_space->tbo.resource->start;
		cb_hdr->ptr.mob.mobOffset = offset;
	} else {
		cb_hdr->ptr.pa = (u64)man->handle + (u64)offset;
	}

	return 0;

out_no_cb_header:
	spin_lock(&man->lock);
	drm_mm_remove_node(&header->node);
	spin_unlock(&man->lock);

	return ret;
}

 
static int vmw_cmdbuf_space_inline(struct vmw_cmdbuf_man *man,
				   struct vmw_cmdbuf_header *header,
				   int size)
{
	struct vmw_cmdbuf_dheader *dheader;
	SVGACBHeader *cb_hdr;

	if (WARN_ON_ONCE(size > VMW_CMDBUF_INLINE_SIZE))
		return -ENOMEM;

	dheader = dma_pool_zalloc(man->dheaders, GFP_KERNEL,
				  &header->handle);
	if (!dheader)
		return -ENOMEM;

	header->inline_space = true;
	header->size = VMW_CMDBUF_INLINE_SIZE;
	cb_hdr = &dheader->cb_header;
	header->cb_header = cb_hdr;
	header->cmd = dheader->cmd;
	cb_hdr->status = SVGA_CB_STATUS_NONE;
	cb_hdr->flags = SVGA_CB_FLAG_NONE;
	cb_hdr->ptr.pa = (u64)header->handle +
		(u64)offsetof(struct vmw_cmdbuf_dheader, cmd);

	return 0;
}

 
void *vmw_cmdbuf_alloc(struct vmw_cmdbuf_man *man,
		       size_t size, bool interruptible,
		       struct vmw_cmdbuf_header **p_header)
{
	struct vmw_cmdbuf_header *header;
	int ret = 0;

	*p_header = NULL;

	header = kzalloc(sizeof(*header), GFP_KERNEL);
	if (!header)
		return ERR_PTR(-ENOMEM);

	if (size <= VMW_CMDBUF_INLINE_SIZE)
		ret = vmw_cmdbuf_space_inline(man, header, size);
	else
		ret = vmw_cmdbuf_space_pool(man, header, size, interruptible);

	if (ret) {
		kfree(header);
		return ERR_PTR(ret);
	}

	header->man = man;
	INIT_LIST_HEAD(&header->list);
	header->cb_header->status = SVGA_CB_STATUS_NONE;
	*p_header = header;

	return header->cmd;
}

 
static void *vmw_cmdbuf_reserve_cur(struct vmw_cmdbuf_man *man,
				    size_t size,
				    int ctx_id,
				    bool interruptible)
{
	struct vmw_cmdbuf_header *cur;
	void *ret;

	if (vmw_cmdbuf_cur_lock(man, interruptible))
		return ERR_PTR(-ERESTARTSYS);

	cur = man->cur;
	if (cur && (size + man->cur_pos > cur->size ||
		    ((cur->cb_header->flags & SVGA_CB_FLAG_DX_CONTEXT) &&
		     ctx_id != cur->cb_header->dxContext)))
		__vmw_cmdbuf_cur_flush(man);

	if (!man->cur) {
		ret = vmw_cmdbuf_alloc(man,
				       max_t(size_t, size, man->default_size),
				       interruptible, &man->cur);
		if (IS_ERR(ret)) {
			vmw_cmdbuf_cur_unlock(man);
			return ret;
		}

		cur = man->cur;
	}

	if (ctx_id != SVGA3D_INVALID_ID) {
		cur->cb_header->flags |= SVGA_CB_FLAG_DX_CONTEXT;
		cur->cb_header->dxContext = ctx_id;
	}

	cur->reserved = size;

	return (void *) (man->cur->cmd + man->cur_pos);
}

 
static void vmw_cmdbuf_commit_cur(struct vmw_cmdbuf_man *man,
				  size_t size, bool flush)
{
	struct vmw_cmdbuf_header *cur = man->cur;

	lockdep_assert_held_once(&man->cur_mutex);

	WARN_ON(size > cur->reserved);
	man->cur_pos += size;
	if (!size)
		cur->cb_header->flags &= ~SVGA_CB_FLAG_DX_CONTEXT;
	if (flush)
		__vmw_cmdbuf_cur_flush(man);
	vmw_cmdbuf_cur_unlock(man);
}

 
void *vmw_cmdbuf_reserve(struct vmw_cmdbuf_man *man, size_t size,
			 int ctx_id, bool interruptible,
			 struct vmw_cmdbuf_header *header)
{
	if (!header)
		return vmw_cmdbuf_reserve_cur(man, size, ctx_id, interruptible);

	if (size > header->size)
		return ERR_PTR(-EINVAL);

	if (ctx_id != SVGA3D_INVALID_ID) {
		header->cb_header->flags |= SVGA_CB_FLAG_DX_CONTEXT;
		header->cb_header->dxContext = ctx_id;
	}

	header->reserved = size;
	return header->cmd;
}

 
void vmw_cmdbuf_commit(struct vmw_cmdbuf_man *man, size_t size,
		       struct vmw_cmdbuf_header *header, bool flush)
{
	if (!header) {
		vmw_cmdbuf_commit_cur(man, size, flush);
		return;
	}

	(void) vmw_cmdbuf_cur_lock(man, false);
	__vmw_cmdbuf_cur_flush(man);
	WARN_ON(size > header->reserved);
	man->cur = header;
	man->cur_pos = size;
	if (!size)
		header->cb_header->flags &= ~SVGA_CB_FLAG_DX_CONTEXT;
	if (flush)
		__vmw_cmdbuf_cur_flush(man);
	vmw_cmdbuf_cur_unlock(man);
}


 
static int vmw_cmdbuf_send_device_command(struct vmw_cmdbuf_man *man,
					  const void *command,
					  size_t size)
{
	struct vmw_cmdbuf_header *header;
	int status;
	void *cmd = vmw_cmdbuf_alloc(man, size, false, &header);

	if (IS_ERR(cmd))
		return PTR_ERR(cmd);

	memcpy(cmd, command, size);
	header->cb_header->length = size;
	header->cb_context = SVGA_CB_CONTEXT_DEVICE;
	spin_lock(&man->lock);
	status = vmw_cmdbuf_header_submit(header);
	spin_unlock(&man->lock);
	vmw_cmdbuf_header_free(header);

	if (status != SVGA_CB_STATUS_COMPLETED) {
		DRM_ERROR("Device context command failed with status %d\n",
			  status);
		return -EINVAL;
	}

	return 0;
}

 
static int vmw_cmdbuf_preempt(struct vmw_cmdbuf_man *man, u32 context)
{
	struct {
		uint32 id;
		SVGADCCmdPreempt body;
	} __packed cmd;

	cmd.id = SVGA_DC_CMD_PREEMPT;
	cmd.body.context = SVGA_CB_CONTEXT_0 + context;
	cmd.body.ignoreIDZero = 0;

	return vmw_cmdbuf_send_device_command(man, &cmd, sizeof(cmd));
}


 
static int vmw_cmdbuf_startstop(struct vmw_cmdbuf_man *man, u32 context,
				bool enable)
{
	struct {
		uint32 id;
		SVGADCCmdStartStop body;
	} __packed cmd;

	cmd.id = SVGA_DC_CMD_START_STOP_CONTEXT;
	cmd.body.enable = (enable) ? 1 : 0;
	cmd.body.context = SVGA_CB_CONTEXT_0 + context;

	return vmw_cmdbuf_send_device_command(man, &cmd, sizeof(cmd));
}

 
int vmw_cmdbuf_set_pool_size(struct vmw_cmdbuf_man *man, size_t size)
{
	struct vmw_private *dev_priv = man->dev_priv;
	int ret;

	if (man->has_pool)
		return -EINVAL;

	 
	size = PAGE_ALIGN(size);
	man->map = dma_alloc_coherent(dev_priv->drm.dev, size,
				      &man->handle, GFP_KERNEL);
	if (man->map) {
		man->using_mob = false;
	} else {
		struct vmw_bo_params bo_params = {
			.domain = VMW_BO_DOMAIN_MOB,
			.busy_domain = VMW_BO_DOMAIN_MOB,
			.bo_type = ttm_bo_type_kernel,
			.size = size,
			.pin = true
		};
		 
		if (!(dev_priv->capabilities & SVGA_CAP_DX) ||
		    !dev_priv->has_mob)
			return -ENOMEM;

		ret = vmw_bo_create(dev_priv, &bo_params, &man->cmd_space);
		if (ret)
			return ret;

		man->map = vmw_bo_map_and_cache(man->cmd_space);
		man->using_mob = man->map;
	}

	man->size = size;
	drm_mm_init(&man->mm, 0, size >> PAGE_SHIFT);

	man->has_pool = true;

	 
	man->default_size = VMW_CMDBUF_INLINE_SIZE;
	drm_info(&dev_priv->drm,
		 "Using command buffers with %s pool.\n",
		 (man->using_mob) ? "MOB" : "DMA");

	return 0;
}

 
struct vmw_cmdbuf_man *vmw_cmdbuf_man_create(struct vmw_private *dev_priv)
{
	struct vmw_cmdbuf_man *man;
	struct vmw_cmdbuf_context *ctx;
	unsigned int i;
	int ret;

	if (!(dev_priv->capabilities & SVGA_CAP_COMMAND_BUFFERS))
		return ERR_PTR(-ENOSYS);

	man = kzalloc(sizeof(*man), GFP_KERNEL);
	if (!man)
		return ERR_PTR(-ENOMEM);

	man->num_contexts = (dev_priv->capabilities & SVGA_CAP_HP_CMD_QUEUE) ?
		2 : 1;
	man->headers = dma_pool_create("vmwgfx cmdbuf",
				       dev_priv->drm.dev,
				       sizeof(SVGACBHeader),
				       64, PAGE_SIZE);
	if (!man->headers) {
		ret = -ENOMEM;
		goto out_no_pool;
	}

	man->dheaders = dma_pool_create("vmwgfx inline cmdbuf",
					dev_priv->drm.dev,
					sizeof(struct vmw_cmdbuf_dheader),
					64, PAGE_SIZE);
	if (!man->dheaders) {
		ret = -ENOMEM;
		goto out_no_dpool;
	}

	for_each_cmdbuf_ctx(man, i, ctx)
		vmw_cmdbuf_ctx_init(ctx);

	INIT_LIST_HEAD(&man->error);
	spin_lock_init(&man->lock);
	mutex_init(&man->cur_mutex);
	mutex_init(&man->space_mutex);
	mutex_init(&man->error_mutex);
	man->default_size = VMW_CMDBUF_INLINE_SIZE;
	init_waitqueue_head(&man->alloc_queue);
	init_waitqueue_head(&man->idle_queue);
	man->dev_priv = dev_priv;
	man->max_hw_submitted = SVGA_CB_MAX_QUEUED_PER_CONTEXT - 1;
	INIT_WORK(&man->work, &vmw_cmdbuf_work_func);
	vmw_generic_waiter_add(dev_priv, SVGA_IRQFLAG_ERROR,
			       &dev_priv->error_waiters);
	ret = vmw_cmdbuf_startstop(man, 0, true);
	if (ret) {
		DRM_ERROR("Failed starting command buffer contexts\n");
		vmw_cmdbuf_man_destroy(man);
		return ERR_PTR(ret);
	}

	return man;

out_no_dpool:
	dma_pool_destroy(man->headers);
out_no_pool:
	kfree(man);

	return ERR_PTR(ret);
}

 
void vmw_cmdbuf_remove_pool(struct vmw_cmdbuf_man *man)
{
	if (!man->has_pool)
		return;

	man->has_pool = false;
	man->default_size = VMW_CMDBUF_INLINE_SIZE;
	(void) vmw_cmdbuf_idle(man, false, 10*HZ);
	if (man->using_mob)
		vmw_bo_unreference(&man->cmd_space);
	else
		dma_free_coherent(man->dev_priv->drm.dev,
				  man->size, man->map, man->handle);
}

 
void vmw_cmdbuf_man_destroy(struct vmw_cmdbuf_man *man)
{
	WARN_ON_ONCE(man->has_pool);
	(void) vmw_cmdbuf_idle(man, false, 10*HZ);

	if (vmw_cmdbuf_startstop(man, 0, false))
		DRM_ERROR("Failed stopping command buffer contexts.\n");

	vmw_generic_waiter_remove(man->dev_priv, SVGA_IRQFLAG_ERROR,
				  &man->dev_priv->error_waiters);
	(void) cancel_work_sync(&man->work);
	dma_pool_destroy(man->dheaders);
	dma_pool_destroy(man->headers);
	mutex_destroy(&man->cur_mutex);
	mutex_destroy(&man->space_mutex);
	mutex_destroy(&man->error_mutex);
	kfree(man);
}
