
 

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_fbdev_generic.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_managed.h>
#include <drm/drm_probe_helper.h>

#include "rcar_du_drv.h"
#include "rcar_du_kms.h"

 

static const struct rcar_du_device_info rzg1_du_r8a7743_info = {
	.gen = 2,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(1) | BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 1,
		},
	},
	.num_lvds = 1,
	.num_rpf = 4,
};

static const struct rcar_du_device_info rzg1_du_r8a7745_info = {
	.gen = 2,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_DPAD1] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
	},
	.num_rpf = 4,
};

static const struct rcar_du_device_info rzg1_du_r8a77470_info = {
	.gen = 2,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_DPAD1] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0) | BIT(1),
			.port = 2,
		},
	},
	.num_rpf = 4,
};

static const struct rcar_du_device_info rcar_du_r8a774a1_info = {
	.gen = 3,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_VSP1_SOURCE
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(2) | BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(2),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_HDMI0] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 2,
		},
	},
	.num_lvds = 1,
	.num_rpf = 5,
	.dpll_mask =  BIT(1),
};

static const struct rcar_du_device_info rcar_du_r8a774b1_info = {
	.gen = 3,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_VSP1_SOURCE
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(3) | BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(2),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_HDMI0] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 2,
		},
	},
	.num_lvds = 1,
	.num_rpf = 5,
	.dpll_mask =  BIT(1),
};

static const struct rcar_du_device_info rcar_du_r8a774c0_info = {
	.gen = 3,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_VSP1_SOURCE,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(0) | BIT(1),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 1,
		},
		[RCAR_DU_OUTPUT_LVDS1] = {
			.possible_crtcs = BIT(1),
			.port = 2,
		},
	},
	.num_lvds = 2,
	.num_rpf = 4,
	.lvds_clk_mask =  BIT(1) | BIT(0),
};

static const struct rcar_du_device_info rcar_du_r8a774e1_info = {
	.gen = 3,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_VSP1_SOURCE
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(3) | BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(2),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_HDMI0] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 2,
		},
	},
	.num_lvds = 1,
	.num_rpf = 5,
	.dpll_mask =  BIT(1),
};

static const struct rcar_du_device_info rcar_du_r8a7779_info = {
	.gen = 1,
	.features = RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_DPAD1] = {
			.possible_crtcs = BIT(1) | BIT(0),
			.port = 1,
		},
	},
};

static const struct rcar_du_device_info rcar_du_r8a7790_info = {
	.gen = 2,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.quirks = RCAR_DU_QUIRK_ALIGN_128B,
	.channels_mask = BIT(2) | BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(2) | BIT(1) | BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 1,
		},
		[RCAR_DU_OUTPUT_LVDS1] = {
			.possible_crtcs = BIT(2) | BIT(1),
			.port = 2,
		},
	},
	.num_lvds = 2,
	.num_rpf = 4,
};

 
static const struct rcar_du_device_info rcar_du_r8a7791_info = {
	.gen = 2,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(1) | BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 1,
		},
	},
	.num_lvds = 1,
	.num_rpf = 4,
};

static const struct rcar_du_device_info rcar_du_r8a7792_info = {
	.gen = 2,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_DPAD1] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
	},
	.num_rpf = 4,
};

static const struct rcar_du_device_info rcar_du_r8a7794_info = {
	.gen = 2,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_DPAD1] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
	},
	.num_rpf = 4,
};

static const struct rcar_du_device_info rcar_du_r8a7795_info = {
	.gen = 3,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_VSP1_SOURCE
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(3) | BIT(2) | BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(3),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_HDMI0] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
		[RCAR_DU_OUTPUT_HDMI1] = {
			.possible_crtcs = BIT(2),
			.port = 2,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 3,
		},
	},
	.num_lvds = 1,
	.num_rpf = 5,
	.dpll_mask =  BIT(2) | BIT(1),
};

static const struct rcar_du_device_info rcar_du_r8a7796_info = {
	.gen = 3,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_VSP1_SOURCE
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(2) | BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(2),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_HDMI0] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 2,
		},
	},
	.num_lvds = 1,
	.num_rpf = 5,
	.dpll_mask =  BIT(1),
};

static const struct rcar_du_device_info rcar_du_r8a77965_info = {
	.gen = 3,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_VSP1_SOURCE
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(3) | BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(2),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_HDMI0] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 2,
		},
	},
	.num_lvds = 1,
	.num_rpf = 5,
	.dpll_mask =  BIT(1),
};

static const struct rcar_du_device_info rcar_du_r8a77970_info = {
	.gen = 3,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_VSP1_SOURCE
		  | RCAR_DU_FEATURE_INTERLACED
		  | RCAR_DU_FEATURE_TVM_SYNC,
	.channels_mask = BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 1,
		},
	},
	.num_lvds = 1,
	.num_rpf = 5,
};

static const struct rcar_du_device_info rcar_du_r8a7799x_info = {
	.gen = 3,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_CRTC_CLOCK
		  | RCAR_DU_FEATURE_VSP1_SOURCE,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DPAD0] = {
			.possible_crtcs = BIT(0) | BIT(1),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_LVDS0] = {
			.possible_crtcs = BIT(0),
			.port = 1,
		},
		[RCAR_DU_OUTPUT_LVDS1] = {
			.possible_crtcs = BIT(1),
			.port = 2,
		},
	},
	.num_lvds = 2,
	.num_rpf = 5,
	.lvds_clk_mask =  BIT(1) | BIT(0),
};

static const struct rcar_du_device_info rcar_du_r8a779a0_info = {
	.gen = 4,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_VSP1_SOURCE
		  | RCAR_DU_FEATURE_NO_BLENDING,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DSI0] = {
			.possible_crtcs = BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_DSI1] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
	},
	.num_rpf = 5,
	.dsi_clk_mask =  BIT(1) | BIT(0),
};

static const struct rcar_du_device_info rcar_du_r8a779g0_info = {
	.gen = 4,
	.features = RCAR_DU_FEATURE_CRTC_IRQ
		  | RCAR_DU_FEATURE_VSP1_SOURCE
		  | RCAR_DU_FEATURE_NO_BLENDING,
	.channels_mask = BIT(1) | BIT(0),
	.routes = {
		 
		[RCAR_DU_OUTPUT_DSI0] = {
			.possible_crtcs = BIT(0),
			.port = 0,
		},
		[RCAR_DU_OUTPUT_DSI1] = {
			.possible_crtcs = BIT(1),
			.port = 1,
		},
	},
	.num_rpf = 5,
	.dsi_clk_mask =  BIT(1) | BIT(0),
};

static const struct of_device_id rcar_du_of_table[] = {
	{ .compatible = "renesas,du-r8a7742", .data = &rcar_du_r8a7790_info },
	{ .compatible = "renesas,du-r8a7743", .data = &rzg1_du_r8a7743_info },
	{ .compatible = "renesas,du-r8a7744", .data = &rzg1_du_r8a7743_info },
	{ .compatible = "renesas,du-r8a7745", .data = &rzg1_du_r8a7745_info },
	{ .compatible = "renesas,du-r8a77470", .data = &rzg1_du_r8a77470_info },
	{ .compatible = "renesas,du-r8a774a1", .data = &rcar_du_r8a774a1_info },
	{ .compatible = "renesas,du-r8a774b1", .data = &rcar_du_r8a774b1_info },
	{ .compatible = "renesas,du-r8a774c0", .data = &rcar_du_r8a774c0_info },
	{ .compatible = "renesas,du-r8a774e1", .data = &rcar_du_r8a774e1_info },
	{ .compatible = "renesas,du-r8a7779", .data = &rcar_du_r8a7779_info },
	{ .compatible = "renesas,du-r8a7790", .data = &rcar_du_r8a7790_info },
	{ .compatible = "renesas,du-r8a7791", .data = &rcar_du_r8a7791_info },
	{ .compatible = "renesas,du-r8a7792", .data = &rcar_du_r8a7792_info },
	{ .compatible = "renesas,du-r8a7793", .data = &rcar_du_r8a7791_info },
	{ .compatible = "renesas,du-r8a7794", .data = &rcar_du_r8a7794_info },
	{ .compatible = "renesas,du-r8a7795", .data = &rcar_du_r8a7795_info },
	{ .compatible = "renesas,du-r8a7796", .data = &rcar_du_r8a7796_info },
	{ .compatible = "renesas,du-r8a77961", .data = &rcar_du_r8a7796_info },
	{ .compatible = "renesas,du-r8a77965", .data = &rcar_du_r8a77965_info },
	{ .compatible = "renesas,du-r8a77970", .data = &rcar_du_r8a77970_info },
	{ .compatible = "renesas,du-r8a77980", .data = &rcar_du_r8a77970_info },
	{ .compatible = "renesas,du-r8a77990", .data = &rcar_du_r8a7799x_info },
	{ .compatible = "renesas,du-r8a77995", .data = &rcar_du_r8a7799x_info },
	{ .compatible = "renesas,du-r8a779a0", .data = &rcar_du_r8a779a0_info },
	{ .compatible = "renesas,du-r8a779g0", .data = &rcar_du_r8a779g0_info },
	{ }
};

MODULE_DEVICE_TABLE(of, rcar_du_of_table);

const char *rcar_du_output_name(enum rcar_du_output output)
{
	static const char * const names[] = {
		[RCAR_DU_OUTPUT_DPAD0] = "DPAD0",
		[RCAR_DU_OUTPUT_DPAD1] = "DPAD1",
		[RCAR_DU_OUTPUT_DSI0] = "DSI0",
		[RCAR_DU_OUTPUT_DSI1] = "DSI1",
		[RCAR_DU_OUTPUT_HDMI0] = "HDMI0",
		[RCAR_DU_OUTPUT_HDMI1] = "HDMI1",
		[RCAR_DU_OUTPUT_LVDS0] = "LVDS0",
		[RCAR_DU_OUTPUT_LVDS1] = "LVDS1",
		[RCAR_DU_OUTPUT_TCON] = "TCON",
	};

	if (output >= ARRAY_SIZE(names) || !names[output])
		return "UNKNOWN";

	return names[output];
}

 

DEFINE_DRM_GEM_DMA_FOPS(rcar_du_fops);

static const struct drm_driver rcar_du_driver = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.dumb_create		= rcar_du_dumb_create,
	.gem_prime_import_sg_table = rcar_du_gem_prime_import_sg_table,
	.fops			= &rcar_du_fops,
	.name			= "rcar-du",
	.desc			= "Renesas R-Car Display Unit",
	.date			= "20130110",
	.major			= 1,
	.minor			= 0,
};

 

static int rcar_du_pm_suspend(struct device *dev)
{
	struct rcar_du_device *rcdu = dev_get_drvdata(dev);

	return drm_mode_config_helper_suspend(&rcdu->ddev);
}

static int rcar_du_pm_resume(struct device *dev)
{
	struct rcar_du_device *rcdu = dev_get_drvdata(dev);

	return drm_mode_config_helper_resume(&rcdu->ddev);
}

static DEFINE_SIMPLE_DEV_PM_OPS(rcar_du_pm_ops,
				rcar_du_pm_suspend, rcar_du_pm_resume);

 

static void rcar_du_remove(struct platform_device *pdev)
{
	struct rcar_du_device *rcdu = platform_get_drvdata(pdev);
	struct drm_device *ddev = &rcdu->ddev;

	drm_dev_unregister(ddev);
	drm_atomic_helper_shutdown(ddev);

	drm_kms_helper_poll_fini(ddev);
}

static void rcar_du_shutdown(struct platform_device *pdev)
{
	struct rcar_du_device *rcdu = platform_get_drvdata(pdev);

	drm_atomic_helper_shutdown(&rcdu->ddev);
}

static int rcar_du_probe(struct platform_device *pdev)
{
	struct rcar_du_device *rcdu;
	unsigned int mask;
	int ret;

	if (drm_firmware_drivers_only())
		return -ENODEV;

	 
	rcdu = devm_drm_dev_alloc(&pdev->dev, &rcar_du_driver,
				  struct rcar_du_device, ddev);
	if (IS_ERR(rcdu))
		return PTR_ERR(rcdu);

	rcdu->dev = &pdev->dev;

	rcdu->info = of_device_get_match_data(rcdu->dev);

	platform_set_drvdata(pdev, rcdu);

	 
	rcdu->mmio = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(rcdu->mmio))
		return PTR_ERR(rcdu->mmio);

	 
	mask = rcar_du_has(rcdu, RCAR_DU_FEATURE_VSP1_SOURCE) ? 40 : 32;
	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(mask));
	if (ret)
		return ret;

	 
	ret = rcar_du_modeset_init(rcdu);
	if (ret < 0) {
		 
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev,
				"failed to initialize DRM/KMS (%d)\n", ret);
		goto error;
	}

	 
	ret = drm_dev_register(&rcdu->ddev, 0);
	if (ret)
		goto error;

	drm_info(&rcdu->ddev, "Device %s probed\n", dev_name(&pdev->dev));

	drm_fbdev_generic_setup(&rcdu->ddev, 32);

	return 0;

error:
	drm_kms_helper_poll_fini(&rcdu->ddev);
	return ret;
}

static struct platform_driver rcar_du_platform_driver = {
	.probe		= rcar_du_probe,
	.remove_new	= rcar_du_remove,
	.shutdown	= rcar_du_shutdown,
	.driver		= {
		.name	= "rcar-du",
		.pm	= pm_sleep_ptr(&rcar_du_pm_ops),
		.of_match_table = rcar_du_of_table,
	},
};

module_platform_driver(rcar_du_platform_driver);

MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_DESCRIPTION("Renesas R-Car Display Unit DRM Driver");
MODULE_LICENSE("GPL");
