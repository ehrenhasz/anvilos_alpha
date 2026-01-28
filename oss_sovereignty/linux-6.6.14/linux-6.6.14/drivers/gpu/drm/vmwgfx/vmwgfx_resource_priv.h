#ifndef _VMWGFX_RESOURCE_PRIV_H_
#define _VMWGFX_RESOURCE_PRIV_H_
#include "vmwgfx_drv.h"
#define VMW_IDA_ACC_SIZE 128
enum vmw_cmdbuf_res_state {
	VMW_CMDBUF_RES_COMMITTED,
	VMW_CMDBUF_RES_ADD,
	VMW_CMDBUF_RES_DEL
};
struct vmw_user_resource_conv {
	enum ttm_object_type object_type;
	struct vmw_resource *(*base_obj_to_res)(struct ttm_base_object *base);
	void (*res_free) (struct vmw_resource *res);
};
struct vmw_res_func {
	enum vmw_res_type res_type;
	bool needs_guest_memory;
	const char *type_name;
	u32 domain;
	u32 busy_domain;
	bool may_evict;
	u32 prio;
	u32 dirty_prio;
	int (*create) (struct vmw_resource *res);
	int (*destroy) (struct vmw_resource *res);
	int (*bind) (struct vmw_resource *res,
		     struct ttm_validate_buffer *val_buf);
	int (*unbind) (struct vmw_resource *res,
		       bool readback,
		       struct ttm_validate_buffer *val_buf);
	void (*commit_notify)(struct vmw_resource *res,
			      enum vmw_cmdbuf_res_state state);
	int (*dirty_alloc)(struct vmw_resource *res);
	void (*dirty_free)(struct vmw_resource *res);
	int (*dirty_sync)(struct vmw_resource *res);
	void (*dirty_range_add)(struct vmw_resource *res, size_t start,
				 size_t end);
	int (*clean)(struct vmw_resource *res);
};
struct vmw_simple_resource_func {
	const struct vmw_res_func res_func;
	int ttm_res_type;
	size_t size;
	int (*init)(struct vmw_resource *res, void *data);
	void (*hw_destroy)(struct vmw_resource *res);
	void (*set_arg_handle)(void *data, u32 handle);
};
struct vmw_simple_resource {
	struct vmw_resource res;
	const struct vmw_simple_resource_func *func;
};
int vmw_resource_alloc_id(struct vmw_resource *res);
void vmw_resource_release_id(struct vmw_resource *res);
int vmw_resource_init(struct vmw_private *dev_priv, struct vmw_resource *res,
		      bool delay_id,
		      void (*res_free) (struct vmw_resource *res),
		      const struct vmw_res_func *func);
int
vmw_simple_resource_create_ioctl(struct drm_device *dev,
				 void *data,
				 struct drm_file *file_priv,
				 const struct vmw_simple_resource_func *func);
struct vmw_resource *
vmw_simple_resource_lookup(struct ttm_object_file *tfile,
			   uint32_t handle,
			   const struct vmw_simple_resource_func *func);
#endif
