

#ifndef __CLK_BT1_CCU_RST_H__
#define __CLK_BT1_CCU_RST_H__

#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

struct ccu_rst_info;


enum ccu_rst_type {
	CCU_RST_TRIG,
	CCU_RST_DIR,
};


struct ccu_rst_init_data {
	struct regmap *sys_regs;
	struct device_node *np;
};


struct ccu_rst {
	struct reset_controller_dev rcdev;
	struct regmap *sys_regs;
	const struct ccu_rst_info *rsts_info;
};
#define to_ccu_rst(_rcdev) container_of(_rcdev, struct ccu_rst, rcdev)

#ifdef CONFIG_CLK_BT1_CCU_RST

struct ccu_rst *ccu_rst_hw_register(const struct ccu_rst_init_data *init);

void ccu_rst_hw_unregister(struct ccu_rst *rst);

#else

static inline
struct ccu_rst *ccu_rst_hw_register(const struct ccu_rst_init_data *init)
{
	return NULL;
}

static inline void ccu_rst_hw_unregister(struct ccu_rst *rst) {}

#endif

#endif 
