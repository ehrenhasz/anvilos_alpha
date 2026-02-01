 
#include <linux/compat.h>

#include <drm/drm_ioctl.h>

#include "i915_drv.h"
#include "i915_getparam.h"
#include "i915_ioc32.h"

struct drm_i915_getparam32 {
	s32 param;
	 
	u32 value;
};

static int compat_i915_getparam(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct drm_i915_getparam32 req32;
	struct drm_i915_getparam req;

	if (copy_from_user(&req32, (void __user *)arg, sizeof(req32)))
		return -EFAULT;

	req.param = req32.param;
	req.value = compat_ptr(req32.value);

	return drm_ioctl_kernel(file, i915_getparam_ioctl, &req,
				DRM_RENDER_ALLOW);
}

static drm_ioctl_compat_t *i915_compat_ioctls[] = {
	[DRM_I915_GETPARAM] = compat_i915_getparam,
};

 
long i915_ioc32_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned int nr = DRM_IOCTL_NR(cmd);
	drm_ioctl_compat_t *fn = NULL;
	int ret;

	if (nr < DRM_COMMAND_BASE || nr >= DRM_COMMAND_END)
		return drm_compat_ioctl(filp, cmd, arg);

	if (nr < DRM_COMMAND_BASE + ARRAY_SIZE(i915_compat_ioctls))
		fn = i915_compat_ioctls[nr - DRM_COMMAND_BASE];

	if (fn != NULL)
		ret = (*fn) (filp, cmd, arg);
	else
		ret = drm_ioctl(filp, cmd, arg);

	return ret;
}
