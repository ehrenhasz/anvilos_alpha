#ifndef DRIVERS_GPU_DRM_AMD_DC_DEV_DC_GPIO_DDC_REGS_H_
#define DRIVERS_GPU_DRM_AMD_DC_DEV_DC_GPIO_DDC_REGS_H_
#include "gpio_regs.h"
#define DDC_GPIO_REG_LIST_ENTRY(type, cd, id) \
	.type ## _reg =   REG(DC_GPIO_DDC ## id ## _ ## type),\
	.type ## _mask =  DC_GPIO_DDC ## id ## _ ## type ## __DC_GPIO_DDC ## id ## cd ## _ ## type ## _MASK,\
	.type ## _shift = DC_GPIO_DDC ## id ## _ ## type ## __DC_GPIO_DDC ## id ## cd ## _ ## type ## __SHIFT
#define DDC_GPIO_REG_LIST(cd, id) \
	{\
	DDC_GPIO_REG_LIST_ENTRY(MASK, cd, id),\
	DDC_GPIO_REG_LIST_ENTRY(A, cd, id),\
	DDC_GPIO_REG_LIST_ENTRY(EN, cd, id),\
	DDC_GPIO_REG_LIST_ENTRY(Y, cd, id)\
	}
#define DDC_REG_LIST(cd, id) \
	DDC_GPIO_REG_LIST(cd, id),\
	.ddc_setup = REG(DC_I2C_DDC ## id ## _SETUP)
	#define DDC_REG_LIST_DCN2(cd, id) \
	DDC_GPIO_REG_LIST(cd, id),\
	.ddc_setup = REG(DC_I2C_DDC ## id ## _SETUP),\
	.phy_aux_cntl = REG(PHY_AUX_CNTL), \
	.dc_gpio_aux_ctrl_5 = REG(DC_GPIO_AUX_CTRL_5)
#define DDC_GPIO_VGA_REG_LIST_ENTRY(type, cd)\
	.type ## _reg =   REG(DC_GPIO_DDCVGA_ ## type),\
	.type ## _mask =  DC_GPIO_DDCVGA_ ## type ## __DC_GPIO_DDCVGA ## cd ## _ ## type ## _MASK,\
	.type ## _shift = DC_GPIO_DDCVGA_ ## type ## __DC_GPIO_DDCVGA ## cd ## _ ## type ## __SHIFT
#define DDC_GPIO_VGA_REG_LIST(cd) \
	{\
	DDC_GPIO_VGA_REG_LIST_ENTRY(MASK, cd),\
	DDC_GPIO_VGA_REG_LIST_ENTRY(A, cd),\
	DDC_GPIO_VGA_REG_LIST_ENTRY(EN, cd),\
	DDC_GPIO_VGA_REG_LIST_ENTRY(Y, cd)\
	}
#define DDC_VGA_REG_LIST(cd) \
	DDC_GPIO_VGA_REG_LIST(cd),\
	.ddc_setup = mmDC_I2C_DDCVGA_SETUP
#define DDC_GPIO_I2C_REG_LIST_ENTRY(type, cd) \
	.type ## _reg =   REG(DC_GPIO_I2CPAD_ ## type),\
	.type ## _mask =  DC_GPIO_I2CPAD_ ## type ## __DC_GPIO_ ## cd ## _ ## type ## _MASK,\
	.type ## _shift = DC_GPIO_I2CPAD_ ## type ## __DC_GPIO_ ## cd ## _ ## type ## __SHIFT
#define DDC_GPIO_I2C_REG_LIST(cd) \
	{\
	DDC_GPIO_I2C_REG_LIST_ENTRY(MASK, cd),\
	DDC_GPIO_I2C_REG_LIST_ENTRY(A, cd),\
	DDC_GPIO_I2C_REG_LIST_ENTRY(EN, cd),\
	DDC_GPIO_I2C_REG_LIST_ENTRY(Y, cd)\
	}
#define DDC_I2C_REG_LIST(cd) \
	DDC_GPIO_I2C_REG_LIST(cd),\
	.ddc_setup = 0
#define DDC_I2C_REG_LIST_DCN2(cd) \
	DDC_GPIO_I2C_REG_LIST(cd),\
	.ddc_setup = 0,\
	.phy_aux_cntl = REG(PHY_AUX_CNTL), \
	.dc_gpio_aux_ctrl_5 = REG(DC_GPIO_AUX_CTRL_5)
#define DDC_MASK_SH_LIST_COMMON(mask_sh) \
		SF_DDC(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_ENABLE, mask_sh),\
		SF_DDC(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_EDID_DETECT_ENABLE, mask_sh),\
		SF_DDC(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_EDID_DETECT_MODE, mask_sh),\
		SF_DDC(DC_GPIO_DDC1_MASK, DC_GPIO_DDC1DATA_PD_EN, mask_sh),\
		SF_DDC(DC_GPIO_DDC1_MASK, DC_GPIO_DDC1CLK_PD_EN, mask_sh),\
		SF_DDC(DC_GPIO_DDC1_MASK, AUX_PAD1_MODE, mask_sh)
#define DDC_MASK_SH_LIST(mask_sh) \
		DDC_MASK_SH_LIST_COMMON(mask_sh),\
		SF_DDC(DC_GPIO_I2CPAD_MASK, DC_GPIO_SDA_PD_DIS, mask_sh),\
		SF_DDC(DC_GPIO_I2CPAD_MASK, DC_GPIO_SCL_PD_DIS, mask_sh)
#define DDC_MASK_SH_LIST_DCN2(mask_sh, cd) \
	{DDC_MASK_SH_LIST_COMMON(mask_sh),\
	0,\
	0,\
	(PHY_AUX_CNTL__AUX## cd ##_PAD_RXSEL## mask_sh),\
	(DC_GPIO_AUX_CTRL_5__DDC_PAD## cd ##_I2CMODE## mask_sh)}
#define DDC_MASK_SH_LIST_DCN2_VGA(mask_sh) \
	{DDC_MASK_SH_LIST_COMMON(mask_sh),\
	0,\
	0,\
	0,\
	0}
struct ddc_registers {
	struct gpio_registers gpio;
	uint32_t ddc_setup;
	uint32_t phy_aux_cntl;
	uint32_t dc_gpio_aux_ctrl_5;
};
struct ddc_sh_mask {
	uint32_t DC_I2C_DDC1_ENABLE;
	uint32_t DC_I2C_DDC1_EDID_DETECT_ENABLE;
	uint32_t DC_I2C_DDC1_EDID_DETECT_MODE;
	uint32_t DC_GPIO_DDC1DATA_PD_EN;
	uint32_t DC_GPIO_DDC1CLK_PD_EN;
	uint32_t AUX_PAD1_MODE;
	uint32_t DC_GPIO_SDA_PD_DIS;
	uint32_t DC_GPIO_SCL_PD_DIS;
	uint32_t AUX_PAD_RXSEL;
	uint32_t DDC_PAD_I2CMODE;
};
#define ddc_data_regs(id) \
{\
	DDC_REG_LIST(DATA, id)\
}
#define ddc_clk_regs(id) \
{\
	DDC_REG_LIST(CLK, id)\
}
#define ddc_vga_data_regs \
{\
	DDC_VGA_REG_LIST(DATA)\
}
#define ddc_vga_clk_regs \
{\
	DDC_VGA_REG_LIST(CLK)\
}
#define ddc_i2c_data_regs \
{\
	DDC_I2C_REG_LIST(SDA)\
}
#define ddc_i2c_clk_regs \
{\
	DDC_I2C_REG_LIST(SCL)\
}
#define ddc_data_regs_dcn2(id) \
{\
	DDC_REG_LIST_DCN2(DATA, id)\
}
#define ddc_clk_regs_dcn2(id) \
{\
	DDC_REG_LIST_DCN2(CLK, id)\
}
#define ddc_i2c_data_regs_dcn2 \
{\
	DDC_I2C_REG_LIST_DCN2(SDA)\
}
#define ddc_i2c_clk_regs_dcn2 \
{\
	DDC_REG_LIST_DCN2(SCL)\
}
#endif  
