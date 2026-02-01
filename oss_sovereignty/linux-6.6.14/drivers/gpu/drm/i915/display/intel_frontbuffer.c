 

 

#include "i915_drv.h"
#include "intel_display_trace.h"
#include "intel_display_types.h"
#include "intel_dp.h"
#include "intel_drrs.h"
#include "intel_fbc.h"
#include "intel_frontbuffer.h"
#include "intel_psr.h"

 
static void frontbuffer_flush(struct drm_i915_private *i915,
			      unsigned int frontbuffer_bits,
			      enum fb_op_origin origin)
{
	 
	spin_lock(&i915->display.fb_tracking.lock);
	frontbuffer_bits &= ~i915->display.fb_tracking.busy_bits;
	spin_unlock(&i915->display.fb_tracking.lock);

	if (!frontbuffer_bits)
		return;

	trace_intel_frontbuffer_flush(i915, frontbuffer_bits, origin);

	might_sleep();
	intel_drrs_flush(i915, frontbuffer_bits);
	intel_psr_flush(i915, frontbuffer_bits, origin);
	intel_fbc_flush(i915, frontbuffer_bits, origin);
}

 
void intel_frontbuffer_flip_prepare(struct drm_i915_private *i915,
				    unsigned frontbuffer_bits)
{
	spin_lock(&i915->display.fb_tracking.lock);
	i915->display.fb_tracking.flip_bits |= frontbuffer_bits;
	 
	i915->display.fb_tracking.busy_bits &= ~frontbuffer_bits;
	spin_unlock(&i915->display.fb_tracking.lock);
}

 
void intel_frontbuffer_flip_complete(struct drm_i915_private *i915,
				     unsigned frontbuffer_bits)
{
	spin_lock(&i915->display.fb_tracking.lock);
	 
	frontbuffer_bits &= i915->display.fb_tracking.flip_bits;
	i915->display.fb_tracking.flip_bits &= ~frontbuffer_bits;
	spin_unlock(&i915->display.fb_tracking.lock);

	if (frontbuffer_bits)
		frontbuffer_flush(i915, frontbuffer_bits, ORIGIN_FLIP);
}

 
void intel_frontbuffer_flip(struct drm_i915_private *i915,
			    unsigned frontbuffer_bits)
{
	spin_lock(&i915->display.fb_tracking.lock);
	 
	i915->display.fb_tracking.busy_bits &= ~frontbuffer_bits;
	spin_unlock(&i915->display.fb_tracking.lock);

	frontbuffer_flush(i915, frontbuffer_bits, ORIGIN_FLIP);
}

void __intel_fb_invalidate(struct intel_frontbuffer *front,
			   enum fb_op_origin origin,
			   unsigned int frontbuffer_bits)
{
	struct drm_i915_private *i915 = intel_bo_to_i915(front->obj);

	if (origin == ORIGIN_CS) {
		spin_lock(&i915->display.fb_tracking.lock);
		i915->display.fb_tracking.busy_bits |= frontbuffer_bits;
		i915->display.fb_tracking.flip_bits &= ~frontbuffer_bits;
		spin_unlock(&i915->display.fb_tracking.lock);
	}

	trace_intel_frontbuffer_invalidate(i915, frontbuffer_bits, origin);

	might_sleep();
	intel_psr_invalidate(i915, frontbuffer_bits, origin);
	intel_drrs_invalidate(i915, frontbuffer_bits);
	intel_fbc_invalidate(i915, frontbuffer_bits, origin);
}

void __intel_fb_flush(struct intel_frontbuffer *front,
		      enum fb_op_origin origin,
		      unsigned int frontbuffer_bits)
{
	struct drm_i915_private *i915 = intel_bo_to_i915(front->obj);

	if (origin == ORIGIN_CS) {
		spin_lock(&i915->display.fb_tracking.lock);
		 
		frontbuffer_bits &= i915->display.fb_tracking.busy_bits;
		i915->display.fb_tracking.busy_bits &= ~frontbuffer_bits;
		spin_unlock(&i915->display.fb_tracking.lock);
	}

	if (frontbuffer_bits)
		frontbuffer_flush(i915, frontbuffer_bits, origin);
}

static int frontbuffer_active(struct i915_active *ref)
{
	struct intel_frontbuffer *front =
		container_of(ref, typeof(*front), write);

	kref_get(&front->ref);
	return 0;
}

static void frontbuffer_retire(struct i915_active *ref)
{
	struct intel_frontbuffer *front =
		container_of(ref, typeof(*front), write);

	intel_frontbuffer_flush(front, ORIGIN_CS);
	intel_frontbuffer_put(front);
}

static void frontbuffer_release(struct kref *ref)
	__releases(&intel_bo_to_i915(front->obj)->display.fb_tracking.lock)
{
	struct intel_frontbuffer *front =
		container_of(ref, typeof(*front), ref);
	struct drm_i915_gem_object *obj = front->obj;

	drm_WARN_ON(&intel_bo_to_i915(obj)->drm, atomic_read(&front->bits));

	i915_ggtt_clear_scanout(obj);

	i915_gem_object_set_frontbuffer(obj, NULL);
	spin_unlock(&intel_bo_to_i915(obj)->display.fb_tracking.lock);

	i915_active_fini(&front->write);

	i915_gem_object_put(obj);
	kfree_rcu(front, rcu);
}

struct intel_frontbuffer *
intel_frontbuffer_get(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *i915 = intel_bo_to_i915(obj);
	struct intel_frontbuffer *front, *cur;

	front = i915_gem_object_get_frontbuffer(obj);
	if (front)
		return front;

	front = kmalloc(sizeof(*front), GFP_KERNEL);
	if (!front)
		return NULL;

	front->obj = obj;
	kref_init(&front->ref);
	atomic_set(&front->bits, 0);
	i915_active_init(&front->write,
			 frontbuffer_active,
			 frontbuffer_retire,
			 I915_ACTIVE_RETIRE_SLEEPS);

	spin_lock(&i915->display.fb_tracking.lock);
	cur = i915_gem_object_set_frontbuffer(obj, front);
	spin_unlock(&i915->display.fb_tracking.lock);
	if (cur != front)
		kfree(front);
	return cur;
}

void intel_frontbuffer_put(struct intel_frontbuffer *front)
{
	kref_put_lock(&front->ref,
		      frontbuffer_release,
		      &intel_bo_to_i915(front->obj)->display.fb_tracking.lock);
}

 
void intel_frontbuffer_track(struct intel_frontbuffer *old,
			     struct intel_frontbuffer *new,
			     unsigned int frontbuffer_bits)
{
	 
	BUILD_BUG_ON(INTEL_FRONTBUFFER_BITS_PER_PIPE * I915_MAX_PIPES >
		     BITS_PER_TYPE(atomic_t));
	BUILD_BUG_ON(INTEL_FRONTBUFFER_BITS_PER_PIPE * I915_MAX_PIPES > 32);
	BUILD_BUG_ON(I915_MAX_PLANES > INTEL_FRONTBUFFER_BITS_PER_PIPE);

	if (old) {
		drm_WARN_ON(&intel_bo_to_i915(old->obj)->drm,
			    !(atomic_read(&old->bits) & frontbuffer_bits));
		atomic_andnot(frontbuffer_bits, &old->bits);
	}

	if (new) {
		drm_WARN_ON(&intel_bo_to_i915(new->obj)->drm,
			    atomic_read(&new->bits) & frontbuffer_bits);
		atomic_or(frontbuffer_bits, &new->bits);
	}
}
