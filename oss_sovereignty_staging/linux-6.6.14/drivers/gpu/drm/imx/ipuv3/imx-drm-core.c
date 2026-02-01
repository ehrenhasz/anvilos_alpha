
 

#include <linux/component.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <video/imx-ipu-v3.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fbdev_dma.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "imx-drm.h"
#include "ipuv3-plane.h"

#define MAX_CRTC	4

static int legacyfb_depth = 16;
module_param(legacyfb_depth, int, 0444);

DEFINE_DRM_GEM_DMA_FOPS(imx_drm_driver_fops);

void imx_drm_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}
EXPORT_SYMBOL_GPL(imx_drm_connector_destroy);

static int imx_drm_atomic_check(struct drm_device *dev,
				struct drm_atomic_state *state)
{
	int ret;

	ret = drm_atomic_helper_check(dev, state);
	if (ret)
		return ret;

	 
	ret = drm_atomic_helper_check_modeset(dev, state);
	if (ret)
		return ret;

	 
	ret = ipu_planes_assign_pre(dev, state);
	if (ret)
		return ret;

	return ret;
}

static const struct drm_mode_config_funcs imx_drm_mode_config_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = imx_drm_atomic_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void imx_drm_atomic_commit_tail(struct drm_atomic_state *state)
{
	struct drm_device *dev = state->dev;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state, *new_plane_state;
	bool plane_disabling = false;
	int i;

	drm_atomic_helper_commit_modeset_disables(dev, state);

	drm_atomic_helper_commit_planes(dev, state,
				DRM_PLANE_COMMIT_ACTIVE_ONLY |
				DRM_PLANE_COMMIT_NO_DISABLE_AFTER_MODESET);

	drm_atomic_helper_commit_modeset_enables(dev, state);

	for_each_oldnew_plane_in_state(state, plane, old_plane_state, new_plane_state, i) {
		if (drm_atomic_plane_disabling(old_plane_state, new_plane_state))
			plane_disabling = true;
	}

	 
	drm_atomic_helper_wait_for_flip_done(dev, state);

	if (plane_disabling) {
		for_each_old_plane_in_state(state, plane, old_plane_state, i)
			ipu_plane_disable_deferred(plane);

	}

	drm_atomic_helper_commit_hw_done(state);
}

static const struct drm_mode_config_helper_funcs imx_drm_mode_config_helpers = {
	.atomic_commit_tail = imx_drm_atomic_commit_tail,
};


int imx_drm_encoder_parse_of(struct drm_device *drm,
	struct drm_encoder *encoder, struct device_node *np)
{
	uint32_t crtc_mask = drm_of_find_possible_crtcs(drm, np);

	 
	if (crtc_mask == 0)
		return -EPROBE_DEFER;

	encoder->possible_crtcs = crtc_mask;

	 
	encoder->possible_clones = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(imx_drm_encoder_parse_of);

static const struct drm_ioctl_desc imx_drm_ioctls[] = {
	 
};

static int imx_drm_dumb_create(struct drm_file *file_priv,
			       struct drm_device *drm,
			       struct drm_mode_create_dumb *args)
{
	u32 width = args->width;
	int ret;

	args->width = ALIGN(width, 8);

	ret = drm_gem_dma_dumb_create(file_priv, drm, args);
	if (ret)
		return ret;

	args->width = width;
	return ret;
}

static const struct drm_driver imx_drm_driver = {
	.driver_features	= DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,
	DRM_GEM_DMA_DRIVER_OPS_WITH_DUMB_CREATE(imx_drm_dumb_create),
	.ioctls			= imx_drm_ioctls,
	.num_ioctls		= ARRAY_SIZE(imx_drm_ioctls),
	.fops			= &imx_drm_driver_fops,
	.name			= "imx-drm",
	.desc			= "i.MX DRM graphics",
	.date			= "20120507",
	.major			= 1,
	.minor			= 0,
	.patchlevel		= 0,
};

static int compare_of(struct device *dev, void *data)
{
	struct device_node *np = data;

	 
	if (strcmp(dev->driver->name, "imx-ipuv3-crtc") == 0) {
		struct ipu_client_platformdata *pdata = dev->platform_data;

		return pdata->of_node == np;
	}

	 
	if (of_node_name_eq(np, "lvds-channel")) {
		np = of_get_parent(np);
		of_node_put(np);
	}

	return dev->of_node == np;
}

static int imx_drm_bind(struct device *dev)
{
	struct drm_device *drm;
	int ret;

	drm = drm_dev_alloc(&imx_drm_driver, dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	 
	drm->mode_config.min_width = 1;
	drm->mode_config.min_height = 1;
	drm->mode_config.max_width = 4096;
	drm->mode_config.max_height = 4096;
	drm->mode_config.funcs = &imx_drm_mode_config_funcs;
	drm->mode_config.helper_private = &imx_drm_mode_config_helpers;
	drm->mode_config.normalize_zpos = true;

	ret = drmm_mode_config_init(drm);
	if (ret)
		goto err_kms;

	ret = drm_vblank_init(drm, MAX_CRTC);
	if (ret)
		goto err_kms;

	dev_set_drvdata(dev, drm);

	 
	ret = component_bind_all(dev, drm);
	if (ret)
		goto err_kms;

	drm_mode_config_reset(drm);

	 
	if (legacyfb_depth != 16 && legacyfb_depth != 32) {
		dev_warn(dev, "Invalid legacyfb_depth.  Defaulting to 16bpp\n");
		legacyfb_depth = 16;
	}

	drm_kms_helper_poll_init(drm);

	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err_poll_fini;

	drm_fbdev_dma_setup(drm, legacyfb_depth);

	return 0;

err_poll_fini:
	drm_kms_helper_poll_fini(drm);
	component_unbind_all(drm->dev, drm);
err_kms:
	drm_dev_put(drm);

	return ret;
}

static void imx_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_dev_unregister(drm);

	drm_kms_helper_poll_fini(drm);

	component_unbind_all(drm->dev, drm);

	drm_dev_put(drm);

	dev_set_drvdata(dev, NULL);
}

static const struct component_master_ops imx_drm_ops = {
	.bind = imx_drm_bind,
	.unbind = imx_drm_unbind,
};

static int imx_drm_platform_probe(struct platform_device *pdev)
{
	int ret = drm_of_component_probe(&pdev->dev, compare_of, &imx_drm_ops);

	if (!ret)
		ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));

	return ret;
}

static int imx_drm_platform_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &imx_drm_ops);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int imx_drm_suspend(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	return drm_mode_config_helper_suspend(drm_dev);
}

static int imx_drm_resume(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);

	return drm_mode_config_helper_resume(drm_dev);
}
#endif

static SIMPLE_DEV_PM_OPS(imx_drm_pm_ops, imx_drm_suspend, imx_drm_resume);

static const struct of_device_id imx_drm_dt_ids[] = {
	{ .compatible = "fsl,imx-display-subsystem", },
	{   },
};
MODULE_DEVICE_TABLE(of, imx_drm_dt_ids);

static struct platform_driver imx_drm_pdrv = {
	.probe		= imx_drm_platform_probe,
	.remove		= imx_drm_platform_remove,
	.driver		= {
		.name	= "imx-drm",
		.pm	= &imx_drm_pm_ops,
		.of_match_table = imx_drm_dt_ids,
	},
};

static struct platform_driver * const drivers[] = {
	&imx_drm_pdrv,
	&ipu_drm_driver,
};

static int __init imx_drm_init(void)
{
	if (drm_firmware_drivers_only())
		return -ENODEV;

	return platform_register_drivers(drivers, ARRAY_SIZE(drivers));
}
module_init(imx_drm_init);

static void __exit imx_drm_exit(void)
{
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
}
module_exit(imx_drm_exit);

MODULE_AUTHOR("Sascha Hauer <s.hauer@pengutronix.de>");
MODULE_DESCRIPTION("i.MX drm driver core");
MODULE_LICENSE("GPL");
