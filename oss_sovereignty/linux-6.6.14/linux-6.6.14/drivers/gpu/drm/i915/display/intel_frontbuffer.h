#ifndef __INTEL_FRONTBUFFER_H__
#define __INTEL_FRONTBUFFER_H__
#include <linux/atomic.h>
#include <linux/bits.h>
#include <linux/kref.h>
#include "i915_active_types.h"
struct drm_i915_private;
enum fb_op_origin {
	ORIGIN_CPU = 0,
	ORIGIN_CS,
	ORIGIN_FLIP,
	ORIGIN_DIRTYFB,
	ORIGIN_CURSOR_UPDATE,
};
struct intel_frontbuffer {
	struct kref ref;
	atomic_t bits;
	struct i915_active write;
	struct drm_i915_gem_object *obj;
	struct rcu_head rcu;
};
#define INTEL_FRONTBUFFER_BITS_PER_PIPE 8
#define INTEL_FRONTBUFFER(pipe, plane_id) \
	BIT((plane_id) + INTEL_FRONTBUFFER_BITS_PER_PIPE * (pipe));
#define INTEL_FRONTBUFFER_OVERLAY(pipe) \
	BIT(INTEL_FRONTBUFFER_BITS_PER_PIPE - 1 + INTEL_FRONTBUFFER_BITS_PER_PIPE * (pipe))
#define INTEL_FRONTBUFFER_ALL_MASK(pipe) \
	GENMASK(INTEL_FRONTBUFFER_BITS_PER_PIPE * ((pipe) + 1) - 1,	\
		INTEL_FRONTBUFFER_BITS_PER_PIPE * (pipe))
void intel_frontbuffer_flip_prepare(struct drm_i915_private *i915,
				    unsigned frontbuffer_bits);
void intel_frontbuffer_flip_complete(struct drm_i915_private *i915,
				     unsigned frontbuffer_bits);
void intel_frontbuffer_flip(struct drm_i915_private *i915,
			    unsigned frontbuffer_bits);
void intel_frontbuffer_put(struct intel_frontbuffer *front);
struct intel_frontbuffer *
intel_frontbuffer_get(struct drm_i915_gem_object *obj);
void __intel_fb_invalidate(struct intel_frontbuffer *front,
			   enum fb_op_origin origin,
			   unsigned int frontbuffer_bits);
static inline bool intel_frontbuffer_invalidate(struct intel_frontbuffer *front,
						enum fb_op_origin origin)
{
	unsigned int frontbuffer_bits;
	if (!front)
		return false;
	frontbuffer_bits = atomic_read(&front->bits);
	if (!frontbuffer_bits)
		return false;
	__intel_fb_invalidate(front, origin, frontbuffer_bits);
	return true;
}
void __intel_fb_flush(struct intel_frontbuffer *front,
		      enum fb_op_origin origin,
		      unsigned int frontbuffer_bits);
static inline void intel_frontbuffer_flush(struct intel_frontbuffer *front,
					   enum fb_op_origin origin)
{
	unsigned int frontbuffer_bits;
	if (!front)
		return;
	frontbuffer_bits = atomic_read(&front->bits);
	if (!frontbuffer_bits)
		return;
	__intel_fb_flush(front, origin, frontbuffer_bits);
}
void intel_frontbuffer_track(struct intel_frontbuffer *old,
			     struct intel_frontbuffer *new,
			     unsigned int frontbuffer_bits);
#endif  
