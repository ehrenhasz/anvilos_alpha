#ifndef __DAL_HW_DDC_H__
#define __DAL_HW_DDC_H__
#include "ddc_regs.h"
struct hw_ddc {
	struct hw_gpio base;
	const struct ddc_registers *regs;
	const struct ddc_sh_mask *shifts;
	const struct ddc_sh_mask *masks;
};
#define HW_DDC_FROM_BASE(hw_gpio) \
	container_of((HW_GPIO_FROM_BASE(hw_gpio)), struct hw_ddc, base)
void dal_hw_ddc_init(
	struct hw_ddc **hw_ddc,
	struct dc_context *ctx,
	enum gpio_id id,
	uint32_t en);
struct hw_gpio_pin *dal_hw_ddc_get_pin(struct gpio *gpio);
#endif
