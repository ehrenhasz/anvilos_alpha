#ifndef _I915_SCHEDULER_TYPES_H_
#define _I915_SCHEDULER_TYPES_H_
#include <linux/list.h>
#include "gt/intel_engine_types.h"
#include "i915_priolist_types.h"
struct drm_i915_private;
struct i915_request;
struct intel_engine_cs;
struct i915_sched_attr {
	int priority;
};
struct i915_sched_node {
	struct list_head signalers_list;  
	struct list_head waiters_list;  
	struct list_head link;
	struct i915_sched_attr attr;
	unsigned int flags;
#define I915_SCHED_HAS_EXTERNAL_CHAIN	BIT(0)
	intel_engine_mask_t semaphores;
};
struct i915_dependency {
	struct i915_sched_node *signaler;
	struct i915_sched_node *waiter;
	struct list_head signal_link;
	struct list_head wait_link;
	struct list_head dfs_link;
	unsigned long flags;
#define I915_DEPENDENCY_ALLOC		BIT(0)
#define I915_DEPENDENCY_EXTERNAL	BIT(1)
#define I915_DEPENDENCY_WEAK		BIT(2)
};
#define for_each_waiter(p__, rq__) \
	list_for_each_entry_lockless(p__, \
				     &(rq__)->sched.waiters_list, \
				     wait_link)
#define for_each_signaler(p__, rq__) \
	list_for_each_entry_rcu(p__, \
				&(rq__)->sched.signalers_list, \
				signal_link)
struct i915_sched_engine {
	struct kref ref;
	spinlock_t lock;
	struct list_head requests;
	struct list_head hold;
	struct tasklet_struct tasklet;
	struct i915_priolist default_priolist;
	int queue_priority_hint;
	struct rb_root_cached queue;
	bool no_priolist;
	void *private_data;
	void	(*destroy)(struct kref *kref);
	bool	(*disabled)(struct i915_sched_engine *sched_engine);
	void	(*kick_backend)(const struct i915_request *rq,
				int prio);
	void	(*bump_inflight_request_prio)(struct i915_request *rq,
					      int prio);
	void	(*retire_inflight_request_prio)(struct i915_request *rq);
	void	(*schedule)(struct i915_request *request,
			    const struct i915_sched_attr *attr);
};
#endif  
