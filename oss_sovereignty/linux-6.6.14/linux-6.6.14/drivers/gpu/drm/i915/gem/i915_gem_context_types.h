#ifndef __I915_GEM_CONTEXT_TYPES_H__
#define __I915_GEM_CONTEXT_TYPES_H__
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/llist.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/radix-tree.h>
#include <linux/rbtree.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include "gt/intel_context_types.h"
#include "i915_scheduler.h"
#include "i915_sw_fence.h"
struct pid;
struct drm_i915_private;
struct drm_i915_file_private;
struct i915_address_space;
struct intel_timeline;
struct intel_ring;
struct i915_gem_engines {
	union {
		struct list_head link;
		struct rcu_head rcu;
	};
	struct i915_sw_fence fence;
	struct i915_gem_context *ctx;
	unsigned int num_engines;
	struct intel_context *engines[];
};
struct i915_gem_engines_iter {
	unsigned int idx;
	const struct i915_gem_engines *engines;
};
enum i915_gem_engine_type {
	I915_GEM_ENGINE_TYPE_INVALID = 0,
	I915_GEM_ENGINE_TYPE_PHYSICAL,
	I915_GEM_ENGINE_TYPE_BALANCED,
	I915_GEM_ENGINE_TYPE_PARALLEL,
};
struct i915_gem_proto_engine {
	enum i915_gem_engine_type type;
	struct intel_engine_cs *engine;
	unsigned int num_siblings;
	unsigned int width;
	struct intel_engine_cs **siblings;
	struct intel_sseu sseu;
};
struct i915_gem_proto_context {
	struct i915_address_space *vm;
	unsigned long user_flags;
	struct i915_sched_attr sched;
	int num_user_engines;
	struct i915_gem_proto_engine *user_engines;
	struct intel_sseu legacy_rcs_sseu;
	bool single_timeline;
	bool uses_protected_content;
	intel_wakeref_t pxp_wakeref;
};
struct i915_gem_context {
	struct drm_i915_private *i915;
	struct drm_i915_file_private *file_priv;
	struct i915_gem_engines __rcu *engines;
	struct mutex engines_mutex;
	struct drm_syncobj *syncobj;
	struct i915_address_space *vm;
	struct pid *pid;
	struct list_head link;
	struct i915_drm_client *client;
	struct list_head client_link;
	struct kref ref;
	struct work_struct release_work;
	struct rcu_head rcu;
	unsigned long user_flags;
#define UCONTEXT_NO_ERROR_CAPTURE	1
#define UCONTEXT_BANNABLE		2
#define UCONTEXT_RECOVERABLE		3
#define UCONTEXT_PERSISTENCE		4
	unsigned long flags;
#define CONTEXT_CLOSED			0
#define CONTEXT_USER_ENGINES		1
	bool uses_protected_content;
	intel_wakeref_t pxp_wakeref;
	struct mutex mutex;
	struct i915_sched_attr sched;
	atomic_t guilty_count;
	atomic_t active_count;
	unsigned long hang_timestamp[2];
#define CONTEXT_FAST_HANG_JIFFIES (120 * HZ)  
	u8 remap_slice;
	struct radix_tree_root handles_vma;
	struct mutex lut_mutex;
	char name[TASK_COMM_LEN + 8];
	struct {
		spinlock_t lock;
		struct list_head engines;
	} stale;
};
#endif  
