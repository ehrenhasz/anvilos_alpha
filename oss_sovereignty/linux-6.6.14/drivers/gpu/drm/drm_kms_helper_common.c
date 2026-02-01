 

#include <linux/module.h>

#include <drm/drm_edid.h>
#include <drm/drm_print.h>

#include "drm_crtc_helper_internal.h"

MODULE_AUTHOR("David Airlie, Jesse Barnes");
MODULE_DESCRIPTION("DRM KMS helper");
MODULE_LICENSE("GPL and additional rights");

#if IS_ENABLED(CONFIG_DRM_LOAD_EDID_FIRMWARE)

 
static int edid_firmware_set(const char *val, const struct kernel_param *kp)
{
	DRM_NOTE("drm_kms_helper.edid_firmware is deprecated, please use drm.edid_firmware instead.\n");

	return __drm_set_edid_firmware_path(val);
}

static int edid_firmware_get(char *buffer, const struct kernel_param *kp)
{
	return __drm_get_edid_firmware_path(buffer, PAGE_SIZE);
}

static const struct kernel_param_ops edid_firmware_ops = {
	.set = edid_firmware_set,
	.get = edid_firmware_get,
};

module_param_cb(edid_firmware, &edid_firmware_ops, NULL, 0644);
__MODULE_PARM_TYPE(edid_firmware, "charp");
MODULE_PARM_DESC(edid_firmware,
		 "DEPRECATED. Use drm.edid_firmware module parameter instead.");

#endif
