 

#ifndef DRIVERS_GPU_DRM_AMD_DC_DEV_DC_GPIO_GPIO_REGS_H_
#define DRIVERS_GPU_DRM_AMD_DC_DEV_DC_GPIO_GPIO_REGS_H_

struct gpio_registers {
	uint32_t MASK_reg;
	uint32_t MASK_mask;
	uint32_t MASK_shift;
	uint32_t A_reg;
	uint32_t A_mask;
	uint32_t A_shift;
	uint32_t EN_reg;
	uint32_t EN_mask;
	uint32_t EN_shift;
	uint32_t Y_reg;
	uint32_t Y_mask;
	uint32_t Y_shift;
};


#endif  
