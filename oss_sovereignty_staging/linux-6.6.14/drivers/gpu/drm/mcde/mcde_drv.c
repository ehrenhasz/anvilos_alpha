
 

 

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/dma-buf.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_fbdev_dma.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_vblank.h>

#include "mcde_drm.h"

#define DRIVER_DESC	"DRM module for MCDE"

#define MCDE_PID 0x000001FC
#define MCDE_PID_METALFIX_VERSION_SHIFT 0
#define MCDE_PID_METALFIX_VERSION_MASK 0x000000FF
#define MCDE_PID_DEVELOPMENT_VERSION_SHIFT 8
#define MCDE_PID_DEVELOPMENT_VERSION_MASK 0x0000FF00
#define MCDE_PID_MINOR_VERSION_SHIFT 16
#define MCDE_PID_MINOR_VERSION_MASK 0x00FF0000
#define MCDE_PID_MAJOR_VERSION_SHIFT 24
#define MCDE_PID_MAJOR_VERSION_MASK 0xFF000000

static const struct drm_mode_config_funcs mcde_mode_config_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static const struct drm_mode_config_helper_funcs mcde_mode_config_helpers = {
	 
	.atomic_commit_tail = drm_atomic_helper_commit_tail_rpm,
};

static irqreturn_t mcde_irq(int irq, void *data)
{
	struct mcde *mcde = data;
	u32 val;

	val = readl(mcde->regs + MCDE_MISERR);

	mcde_display_irq(mcde);

	if (val)
		dev_info(mcde->dev, "some error IRQ\n");
	writel(val, mcde->regs + MCDE_RISERR);

	return IRQ_HANDLED;
}

static int mcde_modeset_init(struct drm_device *drm)
{
	struct drm_mode_config *mode_config;
	struct mcde *mcde = to_mcde(drm);
	int ret;

	 
	if (!mcde->bridge) {
		struct drm_panel *panel;
		struct drm_bridge *bridge;

		ret = drm_of_find_panel_or_bridge(drm->dev->of_node,
						  0, 0, &panel, &bridge);
		if (ret) {
			dev_err(drm->dev,
				"Could not locate any output bridge or panel\n");
			return ret;
		}
		if (panel) {
			bridge = drm_panel_bridge_add_typed(panel,
					DRM_MODE_CONNECTOR_DPI);
			if (IS_ERR(bridge)) {
				dev_err(drm->dev,
					"Could not connect panel bridge\n");
				return PTR_ERR(bridge);
			}
		}
		mcde->dpi_output = true;
		mcde->bridge = bridge;
		mcde->flow_mode = MCDE_DPI_FORMATTER_FLOW;
	}

	mode_config = &drm->mode_config;
	mode_config->funcs = &mcde_mode_config_funcs;
	mode_config->helper_private = &mcde_mode_config_helpers;
	 
	mode_config->min_width = 1;
	mode_config->max_width = 1920;
	mode_config->min_height = 1;
	mode_config->max_height = 1080;

	ret = drm_vblank_init(drm, 1);
	if (ret) {
		dev_err(drm->dev, "failed to init vblank\n");
		return ret;
	}

	ret = mcde_display_init(drm);
	if (ret) {
		dev_err(drm->dev, "failed to init display\n");
		return ret;
	}

	 
	ret = drm_simple_display_pipe_attach_bridge(&mcde->pipe,
						    mcde->bridge);
	if (ret) {
		dev_err(drm->dev, "failed to attach display output bridge\n");
		return ret;
	}

	drm_mode_config_reset(drm);
	drm_kms_helper_poll_init(drm);

	return 0;
}

DEFINE_DRM_GEM_DMA_FOPS(drm_fops);

static const struct drm_driver mcde_drm_driver = {
	.driver_features =
		DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,
	.ioctls = NULL,
	.fops = &drm_fops,
	.name = "mcde",
	.desc = DRIVER_DESC,
	.date = "20180529",
	.major = 1,
	.minor = 0,
	.patchlevel = 0,
	DRM_GEM_DMA_DRIVER_OPS,
};

static int mcde_drm_bind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);
	int ret;

	ret = drmm_mode_config_init(drm);
	if (ret)
		return ret;

	ret = component_bind_all(drm->dev, drm);
	if (ret) {
		dev_err(dev, "can't bind component devices\n");
		return ret;
	}

	ret = mcde_modeset_init(drm);
	if (ret)
		goto unbind;

	ret = drm_dev_register(drm, 0);
	if (ret < 0)
		goto unbind;

	drm_fbdev_dma_setup(drm, 32);

	return 0;

unbind:
	component_unbind_all(drm->dev, drm);
	return ret;
}

static void mcde_drm_unbind(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_dev_unregister(drm);
	drm_atomic_helper_shutdown(drm);
	component_unbind_all(drm->dev, drm);
}

static const struct component_master_ops mcde_drm_comp_ops = {
	.bind = mcde_drm_bind,
	.unbind = mcde_drm_unbind,
};

static struct platform_driver *const mcde_component_drivers[] = {
	&mcde_dsi_driver,
};

static int mcde_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct drm_device *drm;
	struct mcde *mcde;
	struct component_match *match = NULL;
	u32 pid;
	int irq;
	int ret;
	int i;

	mcde = devm_drm_dev_alloc(dev, &mcde_drm_driver, struct mcde, drm);
	if (IS_ERR(mcde))
		return PTR_ERR(mcde);
	drm = &mcde->drm;
	mcde->dev = dev;
	platform_set_drvdata(pdev, drm);

	 
	mcde->epod = devm_regulator_get(dev, "epod");
	if (IS_ERR(mcde->epod)) {
		ret = PTR_ERR(mcde->epod);
		dev_err(dev, "can't get EPOD regulator\n");
		return ret;
	}
	ret = regulator_enable(mcde->epod);
	if (ret) {
		dev_err(dev, "can't enable EPOD regulator\n");
		return ret;
	}
	mcde->vana = devm_regulator_get(dev, "vana");
	if (IS_ERR(mcde->vana)) {
		ret = PTR_ERR(mcde->vana);
		dev_err(dev, "can't get VANA regulator\n");
		goto regulator_epod_off;
	}
	ret = regulator_enable(mcde->vana);
	if (ret) {
		dev_err(dev, "can't enable VANA regulator\n");
		goto regulator_epod_off;
	}
	 

	 
	mcde->mcde_clk = devm_clk_get(dev, "mcde");
	if (IS_ERR(mcde->mcde_clk)) {
		dev_err(dev, "unable to get MCDE main clock\n");
		ret = PTR_ERR(mcde->mcde_clk);
		goto regulator_off;
	}
	ret = clk_prepare_enable(mcde->mcde_clk);
	if (ret) {
		dev_err(dev, "failed to enable MCDE main clock\n");
		goto regulator_off;
	}
	dev_info(dev, "MCDE clk rate %lu Hz\n", clk_get_rate(mcde->mcde_clk));

	mcde->lcd_clk = devm_clk_get(dev, "lcd");
	if (IS_ERR(mcde->lcd_clk)) {
		dev_err(dev, "unable to get LCD clock\n");
		ret = PTR_ERR(mcde->lcd_clk);
		goto clk_disable;
	}
	mcde->hdmi_clk = devm_clk_get(dev, "hdmi");
	if (IS_ERR(mcde->hdmi_clk)) {
		dev_err(dev, "unable to get HDMI clock\n");
		ret = PTR_ERR(mcde->hdmi_clk);
		goto clk_disable;
	}

	mcde->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mcde->regs)) {
		dev_err(dev, "no MCDE regs\n");
		ret = -EINVAL;
		goto clk_disable;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		ret = irq;
		goto clk_disable;
	}

	ret = devm_request_irq(dev, irq, mcde_irq, 0, "mcde", mcde);
	if (ret) {
		dev_err(dev, "failed to request irq %d\n", ret);
		goto clk_disable;
	}

	 
	pid = readl(mcde->regs + MCDE_PID);
	dev_info(dev, "found MCDE HW revision %d.%d (dev %d, metal fix %d)\n",
		 (pid & MCDE_PID_MAJOR_VERSION_MASK)
		 >> MCDE_PID_MAJOR_VERSION_SHIFT,
		 (pid & MCDE_PID_MINOR_VERSION_MASK)
		 >> MCDE_PID_MINOR_VERSION_SHIFT,
		 (pid & MCDE_PID_DEVELOPMENT_VERSION_MASK)
		 >> MCDE_PID_DEVELOPMENT_VERSION_SHIFT,
		 (pid & MCDE_PID_METALFIX_VERSION_MASK)
		 >> MCDE_PID_METALFIX_VERSION_SHIFT);
	if (pid != 0x03000800) {
		dev_err(dev, "unsupported hardware revision\n");
		ret = -ENODEV;
		goto clk_disable;
	}

	 
	mcde_display_disable_irqs(mcde);
	writel(0, mcde->regs + MCDE_IMSCERR);
	writel(0xFFFFFFFF, mcde->regs + MCDE_RISERR);

	 
	devm_of_platform_populate(dev);

	 
	for (i = 0; i < ARRAY_SIZE(mcde_component_drivers); i++) {
		struct device_driver *drv = &mcde_component_drivers[i]->driver;
		struct device *p = NULL, *d;

		while ((d = platform_find_device_by_driver(p, drv))) {
			put_device(p);
			component_match_add(dev, &match, component_compare_dev, d);
			p = d;
		}
		put_device(p);
	}
	if (!match) {
		dev_err(dev, "no matching components\n");
		ret = -ENODEV;
		goto clk_disable;
	}
	if (IS_ERR(match)) {
		dev_err(dev, "could not create component match\n");
		ret = PTR_ERR(match);
		goto clk_disable;
	}

	 
	ret = regulator_disable(mcde->epod);
	if (ret) {
		dev_err(dev, "can't disable EPOD regulator\n");
		return ret;
	}
	 
	usleep_range(50000, 70000);

	ret = component_master_add_with_match(&pdev->dev, &mcde_drm_comp_ops,
					      match);
	if (ret) {
		dev_err(dev, "failed to add component master\n");
		 
		clk_disable_unprepare(mcde->mcde_clk);
		regulator_disable(mcde->vana);
		return ret;
	}

	return 0;

clk_disable:
	clk_disable_unprepare(mcde->mcde_clk);
regulator_off:
	regulator_disable(mcde->vana);
regulator_epod_off:
	regulator_disable(mcde->epod);
	return ret;

}

static void mcde_remove(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);
	struct mcde *mcde = to_mcde(drm);

	component_master_del(&pdev->dev, &mcde_drm_comp_ops);
	clk_disable_unprepare(mcde->mcde_clk);
	regulator_disable(mcde->vana);
	regulator_disable(mcde->epod);
}

static const struct of_device_id mcde_of_match[] = {
	{
		.compatible = "ste,mcde",
	},
	{},
};

static struct platform_driver mcde_driver = {
	.driver = {
		.name           = "mcde",
		.of_match_table = mcde_of_match,
	},
	.probe = mcde_probe,
	.remove_new = mcde_remove,
};

static struct platform_driver *const component_drivers[] = {
	&mcde_dsi_driver,
};

static int __init mcde_drm_register(void)
{
	int ret;

	if (drm_firmware_drivers_only())
		return -ENODEV;

	ret = platform_register_drivers(component_drivers,
					ARRAY_SIZE(component_drivers));
	if (ret)
		return ret;

	return platform_driver_register(&mcde_driver);
}

static void __exit mcde_drm_unregister(void)
{
	platform_unregister_drivers(component_drivers,
				    ARRAY_SIZE(component_drivers));
	platform_driver_unregister(&mcde_driver);
}

module_init(mcde_drm_register);
module_exit(mcde_drm_unregister);

MODULE_ALIAS("platform:mcde-drm");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("Linus Walleij <linus.walleij@linaro.org>");
MODULE_LICENSE("GPL");
