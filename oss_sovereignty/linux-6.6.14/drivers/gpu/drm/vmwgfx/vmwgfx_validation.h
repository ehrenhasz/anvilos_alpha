 
 
#ifndef _VMWGFX_VALIDATION_H_
#define _VMWGFX_VALIDATION_H_

#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/ww_mutex.h>

#include <drm/ttm/ttm_execbuf_util.h>

#define VMW_RES_DIRTY_NONE 0
#define VMW_RES_DIRTY_SET BIT(0)
#define VMW_RES_DIRTY_CLEAR BIT(1)

 
struct vmw_validation_context {
	struct vmw_sw_context *sw_context;
	struct list_head resource_list;
	struct list_head resource_ctx_list;
	struct list_head bo_list;
	struct list_head page_list;
	struct ww_acquire_ctx ticket;
	struct mutex *res_mutex;
	unsigned int merge_dups;
	unsigned int mem_size_left;
	u8 *page_address;
	struct vmw_validation_mem *vm;
	size_t vm_size_left;
	size_t total_mem;
};

struct vmw_bo;
struct vmw_resource;
struct vmw_fence_obj;

#if 0
 
#endif
#define DECLARE_VAL_CONTEXT(_name, _sw_context, _merge_dups)		\
	struct vmw_validation_context _name =				\
	{ .sw_context = _sw_context,					\
	  .resource_list = LIST_HEAD_INIT((_name).resource_list),	\
	  .resource_ctx_list = LIST_HEAD_INIT((_name).resource_ctx_list), \
	  .bo_list = LIST_HEAD_INIT((_name).bo_list),			\
	  .page_list = LIST_HEAD_INIT((_name).page_list),		\
	  .res_mutex = NULL,						\
	  .merge_dups = _merge_dups,					\
	  .mem_size_left = 0,						\
	}

 
static inline bool
vmw_validation_has_bos(struct vmw_validation_context *ctx)
{
	return !list_empty(&ctx->bo_list);
}

 
static inline int
vmw_validation_bo_reserve(struct vmw_validation_context *ctx,
			  bool intr)
{
	return ttm_eu_reserve_buffers(&ctx->ticket, &ctx->bo_list, intr,
				      NULL);
}

 
static inline void
vmw_validation_bo_fence(struct vmw_validation_context *ctx,
			struct vmw_fence_obj *fence)
{
	ttm_eu_fence_buffer_objects(&ctx->ticket, &ctx->bo_list,
				    (void *) fence);
}

 
static inline unsigned int vmw_validation_align(unsigned int val)
{
	return ALIGN(val, sizeof(long));
}

int vmw_validation_add_bo(struct vmw_validation_context *ctx,
			  struct vmw_bo *vbo);
int vmw_validation_bo_validate(struct vmw_validation_context *ctx, bool intr);
void vmw_validation_unref_lists(struct vmw_validation_context *ctx);
int vmw_validation_add_resource(struct vmw_validation_context *ctx,
				struct vmw_resource *res,
				size_t priv_size,
				u32 dirty,
				void **p_node,
				bool *first_usage);
void vmw_validation_drop_ht(struct vmw_validation_context *ctx);
int vmw_validation_res_reserve(struct vmw_validation_context *ctx,
			       bool intr);
void vmw_validation_res_unreserve(struct vmw_validation_context *ctx,
				  bool backoff);
void vmw_validation_res_switch_backup(struct vmw_validation_context *ctx,
				      void *val_private,
				      struct vmw_bo *vbo,
				      unsigned long backup_offset);
int vmw_validation_res_validate(struct vmw_validation_context *ctx, bool intr);

int vmw_validation_prepare(struct vmw_validation_context *ctx,
			   struct mutex *mutex, bool intr);
void vmw_validation_revert(struct vmw_validation_context *ctx);
void vmw_validation_done(struct vmw_validation_context *ctx,
			 struct vmw_fence_obj *fence);

void *vmw_validation_mem_alloc(struct vmw_validation_context *ctx,
			       unsigned int size);
int vmw_validation_preload_bo(struct vmw_validation_context *ctx);
int vmw_validation_preload_res(struct vmw_validation_context *ctx,
			       unsigned int size);
void vmw_validation_res_set_dirty(struct vmw_validation_context *ctx,
				  void *val_private, u32 dirty);
void vmw_validation_bo_backoff(struct vmw_validation_context *ctx);

#endif
