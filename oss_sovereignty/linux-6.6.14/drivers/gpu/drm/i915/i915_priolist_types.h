 

#ifndef _I915_PRIOLIST_TYPES_H_
#define _I915_PRIOLIST_TYPES_H_

#include <linux/list.h>
#include <linux/rbtree.h>

#include <uapi/drm/i915_drm.h>

enum {
	I915_PRIORITY_MIN = I915_CONTEXT_MIN_USER_PRIORITY - 1,
	I915_PRIORITY_NORMAL = I915_CONTEXT_DEFAULT_PRIORITY,
	I915_PRIORITY_MAX = I915_CONTEXT_MAX_USER_PRIORITY + 1,

	 
	I915_PRIORITY_HEARTBEAT,

	 
	I915_PRIORITY_DISPLAY,
};

 
#define I915_PRIORITY_INVALID (INT_MIN)

 
#define I915_PRIORITY_UNPREEMPTABLE INT_MAX
#define I915_PRIORITY_BARRIER (I915_PRIORITY_UNPREEMPTABLE - 1)

struct i915_priolist {
	struct list_head requests;
	struct rb_node node;
	int priority;
};

#endif  
