 
 

#ifndef __I915_MEMCPY_H__
#define __I915_MEMCPY_H__

#include <linux/types.h>

struct drm_i915_private;

void i915_memcpy_init_early(struct drm_i915_private *i915);

bool i915_memcpy_from_wc(void *dst, const void *src, unsigned long len);
void i915_unaligned_memcpy_from_wc(void *dst, const void *src, unsigned long len);

 
#define i915_can_memcpy_from_wc(dst, src, len) \
	i915_memcpy_from_wc((void *)((unsigned long)(dst) | (unsigned long)(src) | (len)), NULL, 0)

#define i915_has_memcpy_from_wc() \
	i915_memcpy_from_wc(NULL, NULL, 0)

#endif  
