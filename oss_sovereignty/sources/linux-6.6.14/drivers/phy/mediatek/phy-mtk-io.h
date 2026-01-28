


#ifndef __PHY_MTK_H__
#define __PHY_MTK_H__

#include <linux/bitfield.h>
#include <linux/io.h>

static inline void mtk_phy_clear_bits(void __iomem *reg, u32 bits)
{
	u32 tmp = readl(reg);

	tmp &= ~bits;
	writel(tmp, reg);
}

static inline void mtk_phy_set_bits(void __iomem *reg, u32 bits)
{
	u32 tmp = readl(reg);

	tmp |= bits;
	writel(tmp, reg);
}

static inline void mtk_phy_update_bits(void __iomem *reg, u32 mask, u32 val)
{
	u32 tmp = readl(reg);

	tmp &= ~mask;
	tmp |= val & mask;
	writel(tmp, reg);
}


#define mtk_phy_update_field(reg, mask, val) \
({ \
	BUILD_BUG_ON_MSG(!__builtin_constant_p(mask), "mask is not constant"); \
	mtk_phy_update_bits(reg, mask, FIELD_PREP(mask, val)); \
})

#endif
