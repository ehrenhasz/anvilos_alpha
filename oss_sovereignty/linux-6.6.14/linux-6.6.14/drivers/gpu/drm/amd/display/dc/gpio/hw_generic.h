#ifndef __DAL_HW_generic_H__
#define __DAL_HW_generic_H__
#include "generic_regs.h"
#include "hw_gpio.h"
struct hw_generic {
	struct hw_gpio base;
	const struct generic_registers *regs;
	const struct generic_sh_mask *shifts;
	const struct generic_sh_mask *masks;
};
#define HW_GENERIC_FROM_BASE(hw_gpio) \
	container_of((HW_GPIO_FROM_BASE(hw_gpio)), struct hw_generic, base)
void dal_hw_generic_init(
	struct hw_generic **hw_generic,
	struct dc_context *ctx,
	enum gpio_id id,
	uint32_t en);
struct hw_gpio_pin *dal_hw_generic_get_pin(struct gpio *gpio);
#endif
