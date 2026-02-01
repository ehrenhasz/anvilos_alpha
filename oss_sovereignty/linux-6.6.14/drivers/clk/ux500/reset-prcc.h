 

#ifndef __RESET_PRCC_H
#define __RESET_PRCC_H

#include <linux/reset-controller.h>
#include <linux/io.h>

 
struct u8500_prcc_reset {
	struct reset_controller_dev rcdev;
	u32 phy_base[CLKRST_MAX];
	void __iomem *base[CLKRST_MAX];
};

void u8500_prcc_reset_init(struct device_node *np, struct u8500_prcc_reset *ur);

#endif
