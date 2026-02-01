 

#ifndef __MOCK_DRM_H
#define __MOCK_DRM_H

#include <drm/drm_file.h>

#include "i915_drv.h"

struct drm_file;
struct file;

static inline struct file *mock_file(struct drm_i915_private *i915)
{
	return mock_drm_getfile(i915->drm.primary, O_RDWR);
}

static inline struct drm_file *to_drm_file(struct file *f)
{
	return f->private_data;
}

#endif  
