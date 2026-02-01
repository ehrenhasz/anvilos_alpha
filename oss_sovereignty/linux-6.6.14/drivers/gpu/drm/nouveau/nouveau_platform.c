 
#include "nouveau_platform.h"

static int nouveau_platform_probe(struct platform_device *pdev)
{
	const struct nvkm_device_tegra_func *func;
	struct nvkm_device *device = NULL;
	struct drm_device *drm;
	int ret;

	func = of_device_get_match_data(&pdev->dev);

	drm = nouveau_platform_device_create(func, pdev, &device);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	ret = drm_dev_register(drm, 0);
	if (ret < 0) {
		drm_dev_put(drm);
		return ret;
	}

	return 0;
}

static int nouveau_platform_remove(struct platform_device *pdev)
{
	struct drm_device *dev = platform_get_drvdata(pdev);
	nouveau_drm_device_remove(dev);
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct nvkm_device_tegra_func gk20a_platform_data = {
	.iommu_bit = 34,
	.require_vdd = true,
};

static const struct nvkm_device_tegra_func gm20b_platform_data = {
	.iommu_bit = 34,
	.require_vdd = true,
	.require_ref_clk = true,
};

static const struct nvkm_device_tegra_func gp10b_platform_data = {
	.iommu_bit = 36,
	 
	.require_vdd = false,
};

static const struct of_device_id nouveau_platform_match[] = {
	{
		.compatible = "nvidia,gk20a",
		.data = &gk20a_platform_data,
	},
	{
		.compatible = "nvidia,gm20b",
		.data = &gm20b_platform_data,
	},
	{
		.compatible = "nvidia,gp10b",
		.data = &gp10b_platform_data,
	},
	{ }
};

MODULE_DEVICE_TABLE(of, nouveau_platform_match);
#endif

struct platform_driver nouveau_platform_driver = {
	.driver = {
		.name = "nouveau",
		.of_match_table = of_match_ptr(nouveau_platform_match),
	},
	.probe = nouveau_platform_probe,
	.remove = nouveau_platform_remove,
};
