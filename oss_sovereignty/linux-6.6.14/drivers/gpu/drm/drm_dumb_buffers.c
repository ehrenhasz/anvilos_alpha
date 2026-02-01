 

#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_gem.h>
#include <drm/drm_mode.h>

#include "drm_crtc_internal.h"
#include "drm_internal.h"

 

int drm_mode_create_dumb(struct drm_device *dev,
			 struct drm_mode_create_dumb *args,
			 struct drm_file *file_priv)
{
	u32 cpp, stride, size;

	if (!dev->driver->dumb_create)
		return -ENOSYS;
	if (!args->width || !args->height || !args->bpp)
		return -EINVAL;

	 
	if (args->bpp > U32_MAX - 8)
		return -EINVAL;
	cpp = DIV_ROUND_UP(args->bpp, 8);
	if (cpp > U32_MAX / args->width)
		return -EINVAL;
	stride = cpp * args->width;
	if (args->height > U32_MAX / stride)
		return -EINVAL;

	 
	size = args->height * stride;
	if (PAGE_ALIGN(size) == 0)
		return -EINVAL;

	 
	args->handle = 0;
	args->pitch = 0;
	args->size = 0;

	return dev->driver->dumb_create(file_priv, dev, args);
}

int drm_mode_create_dumb_ioctl(struct drm_device *dev,
			       void *data, struct drm_file *file_priv)
{
	return drm_mode_create_dumb(dev, data, file_priv);
}

 
int drm_mode_mmap_dumb_ioctl(struct drm_device *dev,
			     void *data, struct drm_file *file_priv)
{
	struct drm_mode_map_dumb *args = data;

	if (!dev->driver->dumb_create)
		return -ENOSYS;

	if (dev->driver->dumb_map_offset)
		return dev->driver->dumb_map_offset(file_priv, dev,
						    args->handle,
						    &args->offset);
	else
		return drm_gem_dumb_map_offset(file_priv, dev, args->handle,
					       &args->offset);
}

int drm_mode_destroy_dumb(struct drm_device *dev, u32 handle,
			  struct drm_file *file_priv)
{
	if (!dev->driver->dumb_create)
		return -ENOSYS;

	return drm_gem_handle_delete(file_priv, handle);
}

int drm_mode_destroy_dumb_ioctl(struct drm_device *dev,
				void *data, struct drm_file *file_priv)
{
	struct drm_mode_destroy_dumb *args = data;

	return drm_mode_destroy_dumb(dev, args->handle, file_priv);
}
