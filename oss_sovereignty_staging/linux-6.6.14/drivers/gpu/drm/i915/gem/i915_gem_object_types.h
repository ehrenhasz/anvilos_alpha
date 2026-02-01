 

#ifndef __I915_GEM_OBJECT_TYPES_H__
#define __I915_GEM_OBJECT_TYPES_H__

#include <linux/mmu_notifier.h>

#include <drm/drm_gem.h>
#include <drm/ttm/ttm_bo.h>
#include <uapi/drm/i915_drm.h>

#include "i915_active.h"
#include "i915_selftest.h"
#include "i915_vma_resource.h"

#include "gt/intel_gt_defines.h"

struct drm_i915_gem_object;
struct intel_fronbuffer;
struct intel_memory_region;

 
struct i915_lut_handle {
	struct list_head obj_link;
	struct i915_gem_context *ctx;
	u32 handle;
};

struct drm_i915_gem_object_ops {
	unsigned int flags;
#define I915_GEM_OBJECT_IS_SHRINKABLE			BIT(1)
 
#define I915_GEM_OBJECT_SELF_MANAGED_SHRINK_LIST	BIT(2)
#define I915_GEM_OBJECT_IS_PROXY			BIT(3)
#define I915_GEM_OBJECT_NO_MMAP				BIT(4)

	 
	int (*get_pages)(struct drm_i915_gem_object *obj);
	void (*put_pages)(struct drm_i915_gem_object *obj,
			  struct sg_table *pages);
	int (*truncate)(struct drm_i915_gem_object *obj);
	 
#define I915_GEM_OBJECT_SHRINK_WRITEBACK   BIT(0)
#define I915_GEM_OBJECT_SHRINK_NO_GPU_WAIT BIT(1)
	int (*shrink)(struct drm_i915_gem_object *obj, unsigned int flags);

	int (*pread)(struct drm_i915_gem_object *obj,
		     const struct drm_i915_gem_pread *arg);
	int (*pwrite)(struct drm_i915_gem_object *obj,
		      const struct drm_i915_gem_pwrite *arg);
	u64 (*mmap_offset)(struct drm_i915_gem_object *obj);
	void (*unmap_virtual)(struct drm_i915_gem_object *obj);

	int (*dmabuf_export)(struct drm_i915_gem_object *obj);

	 
	void (*adjust_lru)(struct drm_i915_gem_object *obj);

	 
	void (*delayed_free)(struct drm_i915_gem_object *obj);

	 
	int (*migrate)(struct drm_i915_gem_object *obj,
		       struct intel_memory_region *mr,
		       unsigned int flags);

	void (*release)(struct drm_i915_gem_object *obj);

	const struct vm_operations_struct *mmap_ops;
	const char *name;  
};

 
enum i915_cache_level {
	 
	I915_CACHE_NONE = 0,
	 
	I915_CACHE_LLC,
	 
	I915_CACHE_L3_LLC,
	 
	I915_CACHE_WT,
	 
	I915_MAX_CACHE_LEVEL,
};

enum i915_map_type {
	I915_MAP_WB = 0,
	I915_MAP_WC,
#define I915_MAP_OVERRIDE BIT(31)
	I915_MAP_FORCE_WB = I915_MAP_WB | I915_MAP_OVERRIDE,
	I915_MAP_FORCE_WC = I915_MAP_WC | I915_MAP_OVERRIDE,
};

enum i915_mmap_type {
	I915_MMAP_TYPE_GTT = 0,
	I915_MMAP_TYPE_WC,
	I915_MMAP_TYPE_WB,
	I915_MMAP_TYPE_UC,
	I915_MMAP_TYPE_FIXED,
};

struct i915_mmap_offset {
	struct drm_vma_offset_node vma_node;
	struct drm_i915_gem_object *obj;
	enum i915_mmap_type mmap_type;

	struct rb_node offset;
};

struct i915_gem_object_page_iter {
	struct scatterlist *sg_pos;
	unsigned int sg_idx;  

	struct radix_tree_root radix;
	struct mutex lock;  
};

struct drm_i915_gem_object {
	 
	union {
		struct drm_gem_object base;
		struct ttm_buffer_object __do_not_access;
	};

	const struct drm_i915_gem_object_ops *ops;

	struct {
		 
		spinlock_t lock;

		 
		struct list_head list;

		 
		struct rb_root tree;
	} vma;

	 
	struct list_head lut_list;
	spinlock_t lut_lock;  

	 
	struct list_head obj_link;
	 
	struct i915_address_space *shares_resv_from;

	union {
		struct rcu_head rcu;
		struct llist_node freed;
	};

	 
	unsigned int userfault_count;
	struct list_head userfault_link;

	struct {
		spinlock_t lock;  
		struct rb_root offsets;
	} mmo;

	I915_SELFTEST_DECLARE(struct list_head st_link);

	unsigned long flags;
#define I915_BO_ALLOC_CONTIGUOUS  BIT(0)
#define I915_BO_ALLOC_VOLATILE    BIT(1)
#define I915_BO_ALLOC_CPU_CLEAR   BIT(2)
#define I915_BO_ALLOC_USER        BIT(3)
 
#define I915_BO_ALLOC_PM_VOLATILE BIT(4)
 
#define I915_BO_ALLOC_PM_EARLY    BIT(5)
 
#define I915_BO_ALLOC_GPU_ONLY	  BIT(6)
#define I915_BO_ALLOC_CCS_AUX	  BIT(7)
 
#define I915_BO_PREALLOC	  BIT(8)
#define I915_BO_ALLOC_FLAGS (I915_BO_ALLOC_CONTIGUOUS | \
			     I915_BO_ALLOC_VOLATILE | \
			     I915_BO_ALLOC_CPU_CLEAR | \
			     I915_BO_ALLOC_USER | \
			     I915_BO_ALLOC_PM_VOLATILE | \
			     I915_BO_ALLOC_PM_EARLY | \
			     I915_BO_ALLOC_GPU_ONLY | \
			     I915_BO_ALLOC_CCS_AUX | \
			     I915_BO_PREALLOC)
#define I915_BO_READONLY          BIT(9)
#define I915_TILING_QUIRK_BIT     10  
#define I915_BO_PROTECTED         BIT(11)
	 
	unsigned int mem_flags;
#define I915_BO_FLAG_STRUCT_PAGE BIT(0)  
#define I915_BO_FLAG_IOMEM       BIT(1)  
	 
	unsigned int pat_index:6;
	 
	unsigned int pat_set_by_user:1;
	 
#define I915_BO_CACHE_COHERENT_FOR_READ BIT(0)
#define I915_BO_CACHE_COHERENT_FOR_WRITE BIT(1)
	unsigned int cache_coherent:2;

	 
	unsigned int cache_dirty:1;

	 
	unsigned int is_dpt:1;

	 
	u16 read_domains;

	 
	u16 write_domain;

	struct intel_frontbuffer __rcu *frontbuffer;

	 
	unsigned int tiling_and_stride;
#define FENCE_MINIMUM_STRIDE 128  
#define TILING_MASK (FENCE_MINIMUM_STRIDE - 1)
#define STRIDE_MASK (~TILING_MASK)

	struct {
		 
		atomic_t pages_pin_count;

		 
		atomic_t shrink_pin;

		 
		bool ttm_shrinkable;

		 
		bool unknown_state;

		 
		struct intel_memory_region **placements;
		int n_placements;

		 
		struct intel_memory_region *region;

		 
		struct ttm_resource *res;

		 
		struct list_head region_link;

		struct i915_refct_sgt *rsgt;
		struct sg_table *pages;
		void *mapping;

		struct i915_page_sizes page_sizes;

		I915_SELFTEST_DECLARE(unsigned int page_mask);

		struct i915_gem_object_page_iter get_page;
		struct i915_gem_object_page_iter get_dma_page;

		 
		struct list_head link;

		 
		unsigned int madv:2;

		 
		bool dirty:1;

		u32 tlb[I915_MAX_GT];
	} mm;

	struct {
		struct i915_refct_sgt *cached_io_rsgt;
		struct i915_gem_object_page_iter get_io_page;
		struct drm_i915_gem_object *backup;
		bool created:1;
	} ttm;

	 
	u32 pxp_key_instance;

	 
	unsigned long *bit_17;

	union {
#ifdef CONFIG_MMU_NOTIFIER
		struct i915_gem_userptr {
			uintptr_t ptr;
			unsigned long notifier_seq;

			struct mmu_interval_notifier notifier;
			struct page **pvec;
			int page_ref;
		} userptr;
#endif

		struct drm_mm_node *stolen;

		resource_size_t bo_offset;

		unsigned long scratch;
		u64 encode;

		void *gvt_info;
	};
};

#define intel_bo_to_drm_bo(bo) (&(bo)->base)
#define intel_bo_to_i915(bo) to_i915(intel_bo_to_drm_bo(bo)->dev)

static inline struct drm_i915_gem_object *
to_intel_bo(struct drm_gem_object *gem)
{
	 
	BUILD_BUG_ON(offsetof(struct drm_i915_gem_object, base));

	return container_of(gem, struct drm_i915_gem_object, base);
}

#endif
