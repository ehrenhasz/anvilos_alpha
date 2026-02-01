 

#ifndef __DAL_HW_HPD_H__
#define __DAL_HW_HPD_H__

#include "hpd_regs.h"

struct hw_hpd {
	struct hw_gpio base;
	const struct hpd_registers *regs;
	const struct hpd_sh_mask *shifts;
	const struct hpd_sh_mask *masks;
};

#define HW_HPD_FROM_BASE(hw_gpio) \
	container_of((HW_GPIO_FROM_BASE(hw_gpio)), struct hw_hpd, base)

void dal_hw_hpd_init(
	struct hw_hpd **hw_hpd,
	struct dc_context *ctx,
	enum gpio_id id,
	uint32_t en);

struct hw_gpio_pin *dal_hw_hpd_get_pin(struct gpio *gpio);

#endif
