 

#include "qxl_drv.h"
#include "qxl_object.h"

 

int qxl_mode_dumb_create(struct drm_file *file_priv,
			    struct drm_device *dev,
			    struct drm_mode_create_dumb *args)
{
	struct qxl_device *qdev = to_qxl(dev);
	struct qxl_bo *qobj;
	struct drm_gem_object *gobj;
	uint32_t handle;
	int r;
	struct qxl_surface surf;
	uint32_t pitch, format;

	pitch = args->width * ((args->bpp + 1) / 8);
	args->size = pitch * args->height;
	args->size = ALIGN(args->size, PAGE_SIZE);

	switch (args->bpp) {
	case 16:
		format = SPICE_SURFACE_FMT_16_565;
		break;
	case 32:
		format = SPICE_SURFACE_FMT_32_xRGB;
		break;
	default:
		return -EINVAL;
	}

	surf.width = args->width;
	surf.height = args->height;
	surf.stride = pitch;
	surf.format = format;
	surf.data = 0;

	r = qxl_gem_object_create_with_handle(qdev, file_priv,
					      QXL_GEM_DOMAIN_CPU,
					      args->size, &surf, &gobj,
					      &handle);
	if (r)
		return r;
	qobj = gem_to_qxl_bo(gobj);
	qobj->is_dumb = true;
	drm_gem_object_put(gobj);
	args->pitch = pitch;
	args->handle = handle;
	return 0;
}
