


#ifndef __LINUX_SOC_EXYNOS5422_ASV_H
#define __LINUX_SOC_EXYNOS5422_ASV_H

#include <linux/errno.h>

enum {
	EXYNOS_ASV_SUBSYS_ID_ARM,
	EXYNOS_ASV_SUBSYS_ID_KFC,
	EXYNOS_ASV_SUBSYS_ID_MAX
};

struct exynos_asv;

#ifdef CONFIG_EXYNOS_ASV_ARM
int exynos5422_asv_init(struct exynos_asv *asv);
#else
static inline int exynos5422_asv_init(struct exynos_asv *asv)
{
	return -ENOTSUPP;
}
#endif

#endif 
