#ifndef DRIVERS_GPU_DRM_AMD_DC_DEV_DC_GPIO_GENERIC_REGS_H_
#define DRIVERS_GPU_DRM_AMD_DC_DEV_DC_GPIO_GENERIC_REGS_H_
#include "gpio_regs.h"
#define GENERIC_GPIO_REG_LIST_ENTRY(type, cd, id) \
	.type ## _reg =  REG(DC_GPIO_GENERIC_## type),\
	.type ## _mask =  DC_GPIO_GENERIC_ ## type ## __DC_GPIO_GENERIC ## id ## _ ## type ## _MASK,\
	.type ## _shift = DC_GPIO_GENERIC_ ## type ## __DC_GPIO_GENERIC ## id ## _ ## type ## __SHIFT
#define GENERIC_GPIO_REG_LIST(id) \
	{\
	GENERIC_GPIO_REG_LIST_ENTRY(MASK, cd, id),\
	GENERIC_GPIO_REG_LIST_ENTRY(A, cd, id),\
	GENERIC_GPIO_REG_LIST_ENTRY(EN, cd, id),\
	GENERIC_GPIO_REG_LIST_ENTRY(Y, cd, id)\
	}
#define GENERIC_REG_LIST(id) \
	GENERIC_GPIO_REG_LIST(id), \
	.mux = REG(DC_GENERIC ## id),\
#define GENERIC_MASK_SH_LIST(mask_sh, cd) \
	{(DC_GENERIC ## cd ##__GENERIC ## cd ##_EN## mask_sh),\
	(DC_GENERIC ## cd ##__GENERIC ## cd ##_SEL## mask_sh)}
struct generic_registers {
	struct gpio_registers gpio;
	uint32_t mux;
};
struct generic_sh_mask {
	uint32_t GENERIC_EN;
	uint32_t GENERIC_SEL;
};
#endif  
