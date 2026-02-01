 
 

#ifndef VMWGFX_BO_H
#define VMWGFX_BO_H

#include "device_include/svga_reg.h"

#include <drm/ttm/ttm_bo.h>
#include <drm/ttm/ttm_placement.h>

#include <linux/rbtree_types.h>
#include <linux/types.h>

struct vmw_bo_dirty;
struct vmw_fence_obj;
struct vmw_private;
struct vmw_resource;

enum vmw_bo_domain {
	VMW_BO_DOMAIN_SYS           = BIT(0),
	VMW_BO_DOMAIN_WAITABLE_SYS  = BIT(1),
	VMW_BO_DOMAIN_VRAM          = BIT(2),
	VMW_BO_DOMAIN_GMR           = BIT(3),
	VMW_BO_DOMAIN_MOB           = BIT(4),
};

struct vmw_bo_params {
	u32 domain;
	u32 busy_domain;
	enum ttm_bo_type bo_type;
	size_t size;
	bool pin;
};

 
struct vmw_bo {
	struct ttm_buffer_object tbo;

	struct ttm_placement placement;
	struct ttm_place places[5];
	struct ttm_place busy_places[5];

	 
	struct ttm_bo_kmap_obj map;

	struct rb_root res_tree;
	u32 res_prios[TTM_MAX_BO_PRIORITY];

	atomic_t cpu_writers;
	 
	struct vmw_resource *dx_query_ctx;
	struct vmw_bo_dirty *dirty;
};

void vmw_bo_placement_set(struct vmw_bo *bo, u32 domain, u32 busy_domain);
void vmw_bo_placement_set_default_accelerated(struct vmw_bo *bo);

int vmw_bo_create(struct vmw_private *dev_priv,
		  struct vmw_bo_params *params,
		  struct vmw_bo **p_bo);

int vmw_bo_unref_ioctl(struct drm_device *dev, void *data,
		       struct drm_file *file_priv);

int vmw_bo_pin_in_vram(struct vmw_private *dev_priv,
		       struct vmw_bo *buf,
		       bool interruptible);
int vmw_bo_pin_in_vram_or_gmr(struct vmw_private *dev_priv,
			      struct vmw_bo *buf,
			      bool interruptible);
int vmw_bo_pin_in_start_of_vram(struct vmw_private *vmw_priv,
				struct vmw_bo *bo,
				bool interruptible);
void vmw_bo_pin_reserved(struct vmw_bo *bo, bool pin);
int vmw_bo_unpin(struct vmw_private *vmw_priv,
		 struct vmw_bo *bo,
		 bool interruptible);

void vmw_bo_get_guest_ptr(const struct ttm_buffer_object *buf,
			  SVGAGuestPtr *ptr);
int vmw_user_bo_synccpu_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
void vmw_bo_fence_single(struct ttm_buffer_object *bo,
			 struct vmw_fence_obj *fence);

void *vmw_bo_map_and_cache(struct vmw_bo *vbo);
void vmw_bo_unmap(struct vmw_bo *vbo);

void vmw_bo_move_notify(struct ttm_buffer_object *bo,
			struct ttm_resource *mem);
void vmw_bo_swap_notify(struct ttm_buffer_object *bo);

int vmw_user_bo_lookup(struct drm_file *filp,
		       u32 handle,
		       struct vmw_bo **out);
 
static inline void vmw_bo_prio_adjust(struct vmw_bo *vbo)
{
	int i = ARRAY_SIZE(vbo->res_prios);

	while (i--) {
		if (vbo->res_prios[i]) {
			vbo->tbo.priority = i;
			return;
		}
	}

	vbo->tbo.priority = 3;
}

 
static inline void vmw_bo_prio_add(struct vmw_bo *vbo, int prio)
{
	if (vbo->res_prios[prio]++ == 0)
		vmw_bo_prio_adjust(vbo);
}

 
static inline void vmw_bo_prio_del(struct vmw_bo *vbo, int prio)
{
	if (--vbo->res_prios[prio] == 0)
		vmw_bo_prio_adjust(vbo);
}

static inline void vmw_bo_unreference(struct vmw_bo **buf)
{
	struct vmw_bo *tmp_buf = *buf;

	*buf = NULL;
	if (tmp_buf)
		ttm_bo_put(&tmp_buf->tbo);
}

static inline struct vmw_bo *vmw_bo_reference(struct vmw_bo *buf)
{
	ttm_bo_get(&buf->tbo);
	return buf;
}

static inline struct vmw_bo *vmw_user_bo_ref(struct vmw_bo *vbo)
{
	drm_gem_object_get(&vbo->tbo.base);
	return vbo;
}

static inline void vmw_user_bo_unref(struct vmw_bo **buf)
{
	struct vmw_bo *tmp_buf = *buf;

	*buf = NULL;
	if (tmp_buf)
		drm_gem_object_put(&tmp_buf->tbo.base);
}

static inline struct vmw_bo *to_vmw_bo(struct drm_gem_object *gobj)
{
	return container_of((gobj), struct vmw_bo, tbo.base);
}

#endif 
