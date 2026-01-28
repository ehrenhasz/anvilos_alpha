


#ifndef __CLK_EXYNOS_ARM64_H
#define __CLK_EXYNOS_ARM64_H

#include "clk.h"

void exynos_arm64_register_cmu(struct device *dev,
		struct device_node *np, const struct samsung_cmu_info *cmu);
int exynos_arm64_register_cmu_pm(struct platform_device *pdev, bool set_manual);
int exynos_arm64_cmu_suspend(struct device *dev);
int exynos_arm64_cmu_resume(struct device *dev);

#endif 
