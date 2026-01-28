#ifndef __I915_VMA_TYPES_H__
#define __I915_VMA_TYPES_H__
#include <linux/rbtree.h>
#include <drm/drm_mm.h>
#include "gem/i915_gem_object_types.h"
struct i915_vma_resource;
struct intel_remapped_plane_info {
	u32 offset:31;
	u32 linear:1;
	union {
		struct {
			u16 width;
			u16 height;
			u16 src_stride;
			u16 dst_stride;
		};
		u32 size;
	};
} __packed;
struct intel_remapped_info {
	struct intel_remapped_plane_info plane[4];
	u32 plane_alignment;
} __packed;
struct intel_rotation_info {
	struct intel_remapped_plane_info plane[2];
} __packed;
struct intel_partial_info {
	u64 offset;
	unsigned int size;
} __packed;
enum i915_gtt_view_type {
	I915_GTT_VIEW_NORMAL = 0,
	I915_GTT_VIEW_ROTATED = sizeof(struct intel_rotation_info),
	I915_GTT_VIEW_PARTIAL = sizeof(struct intel_partial_info),
	I915_GTT_VIEW_REMAPPED = sizeof(struct intel_remapped_info),
};
static inline void assert_i915_gem_gtt_types(void)
{
	BUILD_BUG_ON(sizeof(struct intel_rotation_info) != 2 * sizeof(u32) + 8 * sizeof(u16));
	BUILD_BUG_ON(sizeof(struct intel_partial_info) != sizeof(u64) + sizeof(unsigned int));
	BUILD_BUG_ON(sizeof(struct intel_remapped_info) != 5 * sizeof(u32) + 16 * sizeof(u16));
	BUILD_BUG_ON(offsetof(struct intel_remapped_info, plane[0]) !=
		     offsetof(struct intel_rotation_info, plane[0]));
	BUILD_BUG_ON(offsetofend(struct intel_remapped_info, plane[1]) !=
		     offsetofend(struct intel_rotation_info, plane[1]));
	switch ((enum i915_gtt_view_type)0) {
	case I915_GTT_VIEW_NORMAL:
	case I915_GTT_VIEW_PARTIAL:
	case I915_GTT_VIEW_ROTATED:
	case I915_GTT_VIEW_REMAPPED:
		break;
	}
}
struct i915_gtt_view {
	enum i915_gtt_view_type type;
	union {
		struct intel_partial_info partial;
		struct intel_rotation_info rotated;
		struct intel_remapped_info remapped;
	};
};
struct i915_vma {
	struct drm_mm_node node;
	struct i915_address_space *vm;
	const struct i915_vma_ops *ops;
	struct drm_i915_gem_object *obj;
	struct sg_table *pages;
	void __iomem *iomap;
	void *private;  
	struct i915_fence_reg *fence;
	u64 size;
	struct i915_page_sizes page_sizes;
	struct i915_mmap_offset	*mmo;
	u32 guard;  
	u32 fence_size;
	u32 fence_alignment;
	u32 display_alignment;
	atomic_t open_count;
	atomic_t flags;
#define I915_VMA_PIN_MASK 0x3ff
#define I915_VMA_OVERFLOW 0x200
#define I915_VMA_GLOBAL_BIND_BIT 10
#define I915_VMA_LOCAL_BIND_BIT  11
#define I915_VMA_GLOBAL_BIND	((int)BIT(I915_VMA_GLOBAL_BIND_BIT))
#define I915_VMA_LOCAL_BIND	((int)BIT(I915_VMA_LOCAL_BIND_BIT))
#define I915_VMA_BIND_MASK (I915_VMA_GLOBAL_BIND | I915_VMA_LOCAL_BIND)
#define I915_VMA_ERROR_BIT	12
#define I915_VMA_ERROR		((int)BIT(I915_VMA_ERROR_BIT))
#define I915_VMA_GGTT_BIT	13
#define I915_VMA_CAN_FENCE_BIT	14
#define I915_VMA_USERFAULT_BIT	15
#define I915_VMA_GGTT_WRITE_BIT	16
#define I915_VMA_GGTT		((int)BIT(I915_VMA_GGTT_BIT))
#define I915_VMA_CAN_FENCE	((int)BIT(I915_VMA_CAN_FENCE_BIT))
#define I915_VMA_USERFAULT	((int)BIT(I915_VMA_USERFAULT_BIT))
#define I915_VMA_GGTT_WRITE	((int)BIT(I915_VMA_GGTT_WRITE_BIT))
#define I915_VMA_SCANOUT_BIT	17
#define I915_VMA_SCANOUT	((int)BIT(I915_VMA_SCANOUT_BIT))
	struct i915_active active;
#define I915_VMA_PAGES_BIAS 24
#define I915_VMA_PAGES_ACTIVE (BIT(24) | 1)
	atomic_t pages_count;  
	bool vm_ddestroy;
	struct i915_gtt_view gtt_view;
	struct list_head vm_link;
	struct list_head obj_link;  
	struct rb_node obj_node;
	struct hlist_node obj_hash;
	struct list_head evict_link;
	struct list_head closed_link;
	struct i915_vma_resource *resource;
};
#endif
