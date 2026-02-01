 

#ifndef DRIVERS_GPU_DRM_AMD_DC_DEV_DC_GPIO_HPD_REGS_H_
#define DRIVERS_GPU_DRM_AMD_DC_DEV_DC_GPIO_HPD_REGS_H_

#include "gpio_regs.h"

#define ONE_MORE_0 1
#define ONE_MORE_1 2
#define ONE_MORE_2 3
#define ONE_MORE_3 4
#define ONE_MORE_4 5
#define ONE_MORE_5 6


#define HPD_GPIO_REG_LIST_ENTRY(type, cd, id) \
	.type ## _reg =  REG(DC_GPIO_HPD_## type),\
	.type ## _mask =  DC_GPIO_HPD_ ## type ## __DC_GPIO_HPD ## id ## _ ## type ## _MASK,\
	.type ## _shift = DC_GPIO_HPD_ ## type ## __DC_GPIO_HPD ## id ## _ ## type ## __SHIFT

#define HPD_GPIO_REG_LIST(id) \
	{\
	HPD_GPIO_REG_LIST_ENTRY(MASK, cd, id),\
	HPD_GPIO_REG_LIST_ENTRY(A, cd, id),\
	HPD_GPIO_REG_LIST_ENTRY(EN, cd, id),\
	HPD_GPIO_REG_LIST_ENTRY(Y, cd, id)\
	}

#define HPD_REG_LIST(id) \
	HPD_GPIO_REG_LIST(ONE_MORE_ ## id), \
	.int_status = REGI(DC_HPD_INT_STATUS, HPD, id),\
	.toggle_filt_cntl = REGI(DC_HPD_TOGGLE_FILT_CNTL, HPD, id)

 #define HPD_MASK_SH_LIST(mask_sh) \
		SF_HPD(DC_HPD_INT_STATUS, DC_HPD_SENSE_DELAYED, mask_sh),\
		SF_HPD(DC_HPD_INT_STATUS, DC_HPD_SENSE, mask_sh),\
		SF_HPD(DC_HPD_TOGGLE_FILT_CNTL, DC_HPD_CONNECT_INT_DELAY, mask_sh),\
		SF_HPD(DC_HPD_TOGGLE_FILT_CNTL, DC_HPD_DISCONNECT_INT_DELAY, mask_sh)

struct hpd_registers {
	struct gpio_registers gpio;
	uint32_t int_status;
	uint32_t toggle_filt_cntl;
};

struct hpd_sh_mask {
	 
	uint32_t DC_HPD_SENSE_DELAYED;
	uint32_t DC_HPD_SENSE;
	 
	uint32_t DC_HPD_CONNECT_INT_DELAY;
	uint32_t DC_HPD_DISCONNECT_INT_DELAY;
};


#endif  
