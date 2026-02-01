 

#include "dm_services.h"
#include "include/gpio_types.h"
#include "../hw_factory.h"

#include "../hw_gpio.h"
#include "../hw_ddc.h"
#include "../hw_hpd.h"
#include "../hw_generic.h"

#include "hw_factory_dce120.h"

#include "dce/dce_12_0_offset.h"
#include "dce/dce_12_0_sh_mask.h"
#include "soc15_hw_ip.h"
#include "vega10_ip_offset.h"

#define block HPD
#define reg_num 0

 
#define SF_HPD(reg_name, field_name, post_fix)\
	.field_name = HPD0_ ## reg_name ## __ ## field_name ## post_fix

 
#define SF_HPD(reg_name, field_name, post_fix)\
	.field_name = HPD0_ ## reg_name ## __ ## field_name ## post_fix

#define BASE_INNER(seg) \
	DCE_BASE__INST0_SEG ## seg

 
#define BASE(seg) \
	BASE_INNER(seg)

#define REG(reg_name)\
		BASE(mm ## reg_name ## _BASE_IDX) + mm ## reg_name

#define REGI(reg_name, block, id)\
	BASE(mm ## block ## id ## _ ## reg_name ## _BASE_IDX) + \
				mm ## block ## id ## _ ## reg_name


#include "reg_helper.h"
#include "../hpd_regs.h"

#define hpd_regs(id) \
{\
	HPD_REG_LIST(id)\
}

static const struct hpd_registers hpd_regs[] = {
	hpd_regs(0),
	hpd_regs(1),
	hpd_regs(2),
	hpd_regs(3),
	hpd_regs(4),
	hpd_regs(5)
};

static const struct hpd_sh_mask hpd_shift = {
		HPD_MASK_SH_LIST(__SHIFT)
};

static const struct hpd_sh_mask hpd_mask = {
		HPD_MASK_SH_LIST(_MASK)
};

#include "../ddc_regs.h"

  
#define SF_DDC(reg_name, field_name, post_fix)\
	.field_name = reg_name ## __ ## field_name ## post_fix

static const struct ddc_registers ddc_data_regs[] = {
	ddc_data_regs(1),
	ddc_data_regs(2),
	ddc_data_regs(3),
	ddc_data_regs(4),
	ddc_data_regs(5),
	ddc_data_regs(6),
	ddc_vga_data_regs,
	ddc_i2c_data_regs
};

static const struct ddc_registers ddc_clk_regs[] = {
	ddc_clk_regs(1),
	ddc_clk_regs(2),
	ddc_clk_regs(3),
	ddc_clk_regs(4),
	ddc_clk_regs(5),
	ddc_clk_regs(6),
	ddc_vga_clk_regs,
	ddc_i2c_clk_regs
};

static const struct ddc_sh_mask ddc_shift = {
		DDC_MASK_SH_LIST(__SHIFT)
};

static const struct ddc_sh_mask ddc_mask = {
		DDC_MASK_SH_LIST(_MASK)
};

static void define_ddc_registers(
		struct hw_gpio_pin *pin,
		uint32_t en)
{
	struct hw_ddc *ddc = HW_DDC_FROM_BASE(pin);

	switch (pin->id) {
	case GPIO_ID_DDC_DATA:
		ddc->regs = &ddc_data_regs[en];
		ddc->base.regs = &ddc_data_regs[en].gpio;
		break;
	case GPIO_ID_DDC_CLOCK:
		ddc->regs = &ddc_clk_regs[en];
		ddc->base.regs = &ddc_clk_regs[en].gpio;
		break;
	default:
		ASSERT_CRITICAL(false);
		return;
	}

	ddc->shifts = &ddc_shift;
	ddc->masks = &ddc_mask;

}

static void define_hpd_registers(struct hw_gpio_pin *pin, uint32_t en)
{
	struct hw_hpd *hpd = HW_HPD_FROM_BASE(pin);

	hpd->regs = &hpd_regs[en];
	hpd->shifts = &hpd_shift;
	hpd->masks = &hpd_mask;
	hpd->base.regs = &hpd_regs[en].gpio;
}


 
static const struct hw_factory_funcs funcs = {
	.init_ddc_data = dal_hw_ddc_init,
	.init_generic = NULL,
	.init_hpd = dal_hw_hpd_init,
	.get_ddc_pin = dal_hw_ddc_get_pin,
	.get_hpd_pin = dal_hw_hpd_get_pin,
	.get_generic_pin = NULL,
	.define_hpd_registers = define_hpd_registers,
	.define_ddc_registers = define_ddc_registers
};
 
void dal_hw_factory_dce120_init(struct hw_factory *factory)
{
	 
	factory->number_of_pins[GPIO_ID_DDC_DATA] = 8;
	factory->number_of_pins[GPIO_ID_DDC_CLOCK] = 8;
	factory->number_of_pins[GPIO_ID_GENERIC] = 7;
	factory->number_of_pins[GPIO_ID_HPD] = 6;
	factory->number_of_pins[GPIO_ID_GPIO_PAD] = 31;
	factory->number_of_pins[GPIO_ID_VIP_PAD] = 0;
	factory->number_of_pins[GPIO_ID_SYNC] = 2;
	factory->number_of_pins[GPIO_ID_GSL] = 4;

	factory->funcs = &funcs;
}
