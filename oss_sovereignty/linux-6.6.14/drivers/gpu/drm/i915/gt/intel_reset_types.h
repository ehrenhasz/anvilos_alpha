 
 

#ifndef __INTEL_RESET_TYPES_H_
#define __INTEL_RESET_TYPES_H_

#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/srcu.h>

struct intel_reset {
	 
	unsigned long flags;
#define I915_RESET_BACKOFF	0
#define I915_RESET_MODESET	1
#define I915_RESET_ENGINE	2
#define I915_WEDGED_ON_INIT	(BITS_PER_LONG - 3)
#define I915_WEDGED_ON_FINI	(BITS_PER_LONG - 2)
#define I915_WEDGED		(BITS_PER_LONG - 1)

	struct mutex mutex;  

	 
	wait_queue_head_t queue;

	struct srcu_struct backoff_srcu;
};

#endif  
