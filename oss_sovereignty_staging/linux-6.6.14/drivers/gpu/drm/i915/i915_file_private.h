 
 

#ifndef __I915_FILE_PRIVATE_H__
#define __I915_FILE_PRIVATE_H__

#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/xarray.h>

struct drm_i915_private;
struct drm_file;
struct i915_drm_client;

struct drm_i915_file_private {
	struct drm_i915_private *i915;

	union {
		struct drm_file *file;
		struct rcu_head rcu;
	};

	 
	struct mutex proto_context_lock;

	 
	struct xarray proto_context_xa;

	 
	struct xarray context_xa;
	struct xarray vm_xa;

	unsigned int bsd_engine;

 
#define I915_CLIENT_SCORE_HANG_FAST	1
#define   I915_CLIENT_FAST_HANG_JIFFIES (60 * HZ)
#define I915_CLIENT_SCORE_CONTEXT_BAN   3
#define I915_CLIENT_SCORE_BANNED	9
	 
	atomic_t ban_score;
	unsigned long hang_timestamp;

	struct i915_drm_client *client;
};

#endif  
