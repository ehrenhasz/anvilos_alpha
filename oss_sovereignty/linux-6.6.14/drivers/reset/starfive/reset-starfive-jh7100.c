
 

#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>

#include "reset-starfive-jh71x0.h"

#include <dt-bindings/reset/starfive-jh7100.h>

 
#define JH7100_RESET_ASSERT0	0x00
#define JH7100_RESET_ASSERT1	0x04
#define JH7100_RESET_ASSERT2	0x08
#define JH7100_RESET_ASSERT3	0x0c
#define JH7100_RESET_STATUS0	0x10
#define JH7100_RESET_STATUS1	0x14
#define JH7100_RESET_STATUS2	0x18
#define JH7100_RESET_STATUS3	0x1c

 
static const u32 jh7100_reset_asserted[4] = {
	 
	BIT(JH7100_RST_U74 % 32) |
	BIT(JH7100_RST_VP6_DRESET % 32) |
	BIT(JH7100_RST_VP6_BRESET % 32),
	 
	BIT(JH7100_RST_HIFI4_DRESET % 32) |
	BIT(JH7100_RST_HIFI4_BRESET % 32),
	 
	BIT(JH7100_RST_E24 % 32),
	 
	0,
};

static int __init jh7100_reset_probe(struct platform_device *pdev)
{
	void __iomem *base = devm_platform_ioremap_resource(pdev, 0);

	if (IS_ERR(base))
		return PTR_ERR(base);

	return reset_starfive_jh71x0_register(&pdev->dev, pdev->dev.of_node,
					      base + JH7100_RESET_ASSERT0,
					      base + JH7100_RESET_STATUS0,
					      jh7100_reset_asserted,
					      JH7100_RSTN_END,
					      THIS_MODULE);
}

static const struct of_device_id jh7100_reset_dt_ids[] = {
	{ .compatible = "starfive,jh7100-reset" },
	{   }
};

static struct platform_driver jh7100_reset_driver = {
	.driver = {
		.name = "jh7100-reset",
		.of_match_table = jh7100_reset_dt_ids,
		.suppress_bind_attrs = true,
	},
};
builtin_platform_driver_probe(jh7100_reset_driver, jh7100_reset_probe);
