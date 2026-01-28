#ifndef __I915_TIMELINE_TYPES_H__
#define __I915_TIMELINE_TYPES_H__
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include "i915_active_types.h"
struct i915_vma;
struct i915_syncmap;
struct intel_gt;
struct intel_timeline {
	u64 fence_context;
	u32 seqno;
	struct mutex mutex;  
	atomic_t pin_count;
	atomic_t active_count;
	void *hwsp_map;
	const u32 *hwsp_seqno;
	struct i915_vma *hwsp_ggtt;
	u32 hwsp_offset;
	bool has_initial_breadcrumb;
	struct list_head requests;
	struct i915_active_fence last_request;
	struct i915_active active;
	struct intel_timeline *retire;
	struct i915_syncmap *sync;
	struct list_head link;
	struct intel_gt *gt;
	struct list_head engine_link;
	struct kref kref;
	struct rcu_head rcu;
};
#endif  
