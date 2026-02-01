 

#include "hw_translate_dcn315.h"

#include "dm_services.h"
#include "include/gpio_types.h"
#include "../hw_translate.h"

#include "dcn/dcn_3_1_5_offset.h"
#include "dcn/dcn_3_1_5_sh_mask.h"

 

#define DCN_BASE__INST0_SEG0                       0x00000012
#define DCN_BASE__INST0_SEG1                       0x000000C0
#define DCN_BASE__INST0_SEG2                       0x000034C0
#define DCN_BASE__INST0_SEG3                       0x00009000
#define DCN_BASE__INST0_SEG4                       0x02403C00
#define DCN_BASE__INST0_SEG5                       0

 
#define block HPD
#define reg_num 0

#undef BASE_INNER
#define BASE_INNER(seg) DCN_BASE__INST0_SEG ## seg

#define BASE(seg) BASE_INNER(seg)

#undef REG
#define REG(reg_name)\
		BASE(reg ## reg_name ## _BASE_IDX) + reg ## reg_name
#define SF_HPD(reg_name, field_name, post_fix)\
	.field_name = reg_name ## __ ## field_name ## post_fix


 


static bool offset_to_id(
	uint32_t offset,
	uint32_t mask,
	enum gpio_id *id,
	uint32_t *en)
{
	switch (offset) {
	 
	case REG(DC_GPIO_GENERIC_A):
		*id = GPIO_ID_GENERIC;
		switch (mask) {
		case DC_GPIO_GENERIC_A__DC_GPIO_GENERICA_A_MASK:
			*en = GPIO_GENERIC_A;
			return true;
		case DC_GPIO_GENERIC_A__DC_GPIO_GENERICB_A_MASK:
			*en = GPIO_GENERIC_B;
			return true;
		case DC_GPIO_GENERIC_A__DC_GPIO_GENERICC_A_MASK:
			*en = GPIO_GENERIC_C;
			return true;
		case DC_GPIO_GENERIC_A__DC_GPIO_GENERICD_A_MASK:
			*en = GPIO_GENERIC_D;
			return true;
		case DC_GPIO_GENERIC_A__DC_GPIO_GENERICE_A_MASK:
			*en = GPIO_GENERIC_E;
			return true;
		case DC_GPIO_GENERIC_A__DC_GPIO_GENERICF_A_MASK:
			*en = GPIO_GENERIC_F;
			return true;
		case DC_GPIO_GENERIC_A__DC_GPIO_GENERICG_A_MASK:
			*en = GPIO_GENERIC_G;
			return true;
		default:
			ASSERT_CRITICAL(false);
			return false;
		}
	break;
	 
	case REG(DC_GPIO_HPD_A):
		*id = GPIO_ID_HPD;
		switch (mask) {
		case DC_GPIO_HPD_A__DC_GPIO_HPD1_A_MASK:
			*en = GPIO_HPD_1;
			return true;
		case DC_GPIO_HPD_A__DC_GPIO_HPD2_A_MASK:
			*en = GPIO_HPD_2;
			return true;
		case DC_GPIO_HPD_A__DC_GPIO_HPD3_A_MASK:
			*en = GPIO_HPD_3;
			return true;
		case DC_GPIO_HPD_A__DC_GPIO_HPD4_A_MASK:
			*en = GPIO_HPD_4;
			return true;
		case DC_GPIO_HPD_A__DC_GPIO_HPD5_A_MASK:
			*en = GPIO_HPD_5;
			return true;
		case DC_GPIO_HPD_A__DC_GPIO_HPD6_A_MASK:
			*en = GPIO_HPD_6;
			return true;
		default:
			ASSERT_CRITICAL(false);
			return false;
		}
	break;
	 
	case REG(DC_GPIO_GENLK_A):
		*id = GPIO_ID_GSL;
		switch (mask) {
		case DC_GPIO_GENLK_A__DC_GPIO_GENLK_CLK_A_MASK:
			*en = GPIO_GSL_GENLOCK_CLOCK;
			return true;
		case DC_GPIO_GENLK_A__DC_GPIO_GENLK_VSYNC_A_MASK:
			*en = GPIO_GSL_GENLOCK_VSYNC;
			return true;
		case DC_GPIO_GENLK_A__DC_GPIO_SWAPLOCK_A_A_MASK:
			*en = GPIO_GSL_SWAPLOCK_A;
			return true;
		case DC_GPIO_GENLK_A__DC_GPIO_SWAPLOCK_B_A_MASK:
			*en = GPIO_GSL_SWAPLOCK_B;
			return true;
		default:
			ASSERT_CRITICAL(false);
			return false;
		}
	break;
	 
	 
	case REG(DC_GPIO_DDC1_A):
		*en = GPIO_DDC_LINE_DDC1;
		return true;
	case REG(DC_GPIO_DDC2_A):
		*en = GPIO_DDC_LINE_DDC2;
		return true;
	case REG(DC_GPIO_DDC3_A):
		*en = GPIO_DDC_LINE_DDC3;
		return true;
	case REG(DC_GPIO_DDC4_A):
		*en = GPIO_DDC_LINE_DDC4;
		return true;
	case REG(DC_GPIO_DDC5_A):
		*en = GPIO_DDC_LINE_DDC5;
		return true;
	case REG(DC_GPIO_DDCVGA_A):
		*en = GPIO_DDC_LINE_DDC_VGA;
		return true;

 
	 
	default:
 
		ASSERT_CRITICAL(false);
		return false;
	}
}

static bool id_to_offset(
	enum gpio_id id,
	uint32_t en,
	struct gpio_pin_info *info)
{
	bool result = true;

	switch (id) {
	case GPIO_ID_DDC_DATA:
		info->mask = DC_GPIO_DDC1_A__DC_GPIO_DDC1DATA_A_MASK;
		switch (en) {
		case GPIO_DDC_LINE_DDC1:
			info->offset = REG(DC_GPIO_DDC1_A);
		break;
		case GPIO_DDC_LINE_DDC2:
			info->offset = REG(DC_GPIO_DDC2_A);
		break;
		case GPIO_DDC_LINE_DDC3:
			info->offset = REG(DC_GPIO_DDC3_A);
		break;
		case GPIO_DDC_LINE_DDC4:
			info->offset = REG(DC_GPIO_DDC4_A);
		break;
		case GPIO_DDC_LINE_DDC5:
			info->offset = REG(DC_GPIO_DDC5_A);
		break;
		case GPIO_DDC_LINE_DDC_VGA:
			info->offset = REG(DC_GPIO_DDCVGA_A);
		break;
		case GPIO_DDC_LINE_I2C_PAD:
		default:
			ASSERT_CRITICAL(false);
			result = false;
		}
	break;
	case GPIO_ID_DDC_CLOCK:
		info->mask = DC_GPIO_DDC1_A__DC_GPIO_DDC1CLK_A_MASK;
		switch (en) {
		case GPIO_DDC_LINE_DDC1:
			info->offset = REG(DC_GPIO_DDC1_A);
		break;
		case GPIO_DDC_LINE_DDC2:
			info->offset = REG(DC_GPIO_DDC2_A);
		break;
		case GPIO_DDC_LINE_DDC3:
			info->offset = REG(DC_GPIO_DDC3_A);
		break;
		case GPIO_DDC_LINE_DDC4:
			info->offset = REG(DC_GPIO_DDC4_A);
		break;
		case GPIO_DDC_LINE_DDC5:
			info->offset = REG(DC_GPIO_DDC5_A);
		break;
		case GPIO_DDC_LINE_DDC_VGA:
			info->offset = REG(DC_GPIO_DDCVGA_A);
		break;
		case GPIO_DDC_LINE_I2C_PAD:
		default:
			ASSERT_CRITICAL(false);
			result = false;
		}
	break;
	case GPIO_ID_GENERIC:
		info->offset = REG(DC_GPIO_GENERIC_A);
		switch (en) {
		case GPIO_GENERIC_A:
			info->mask = DC_GPIO_GENERIC_A__DC_GPIO_GENERICA_A_MASK;
		break;
		case GPIO_GENERIC_B:
			info->mask = DC_GPIO_GENERIC_A__DC_GPIO_GENERICB_A_MASK;
		break;
		case GPIO_GENERIC_C:
			info->mask = DC_GPIO_GENERIC_A__DC_GPIO_GENERICC_A_MASK;
		break;
		case GPIO_GENERIC_D:
			info->mask = DC_GPIO_GENERIC_A__DC_GPIO_GENERICD_A_MASK;
		break;
		case GPIO_GENERIC_E:
			info->mask = DC_GPIO_GENERIC_A__DC_GPIO_GENERICE_A_MASK;
		break;
		case GPIO_GENERIC_F:
			info->mask = DC_GPIO_GENERIC_A__DC_GPIO_GENERICF_A_MASK;
		break;
		case GPIO_GENERIC_G:
			info->mask = DC_GPIO_GENERIC_A__DC_GPIO_GENERICG_A_MASK;
		break;
		default:
			ASSERT_CRITICAL(false);
			result = false;
		}
	break;
	case GPIO_ID_HPD:
		info->offset = REG(DC_GPIO_HPD_A);
		switch (en) {
		case GPIO_HPD_1:
			info->mask = DC_GPIO_HPD_A__DC_GPIO_HPD1_A_MASK;
		break;
		case GPIO_HPD_2:
			info->mask = DC_GPIO_HPD_A__DC_GPIO_HPD2_A_MASK;
		break;
		case GPIO_HPD_3:
			info->mask = DC_GPIO_HPD_A__DC_GPIO_HPD3_A_MASK;
		break;
		case GPIO_HPD_4:
			info->mask = DC_GPIO_HPD_A__DC_GPIO_HPD4_A_MASK;
		break;
		case GPIO_HPD_5:
			info->mask = DC_GPIO_HPD_A__DC_GPIO_HPD5_A_MASK;
		break;
		case GPIO_HPD_6:
			info->mask = DC_GPIO_HPD_A__DC_GPIO_HPD6_A_MASK;
		break;
		default:
			ASSERT_CRITICAL(false);
			result = false;
		}
	break;
	case GPIO_ID_GSL:
		switch (en) {
		case GPIO_GSL_GENLOCK_CLOCK:
				 
			ASSERT_CRITICAL(false);
			result = false;
		break;
		case GPIO_GSL_GENLOCK_VSYNC:
			 
			ASSERT_CRITICAL(false);
			result = false;
		break;
		case GPIO_GSL_SWAPLOCK_A:
			 
			ASSERT_CRITICAL(false);
			result = false;
		break;
		case GPIO_GSL_SWAPLOCK_B:
			 
			ASSERT_CRITICAL(false);
			result = false;

		break;
		default:
			ASSERT_CRITICAL(false);
			result = false;
		}
	break;
	case GPIO_ID_SYNC:
	case GPIO_ID_VIP_PAD:
	default:
		ASSERT_CRITICAL(false);
		result = false;
	}

	if (result) {
		info->offset_y = info->offset + 2;
		info->offset_en = info->offset + 1;
		info->offset_mask = info->offset - 1;

		info->mask_y = info->mask;
		info->mask_en = info->mask;
		info->mask_mask = info->mask;
	}

	return result;
}

 
static const struct hw_translate_funcs funcs = {
	.offset_to_id = offset_to_id,
	.id_to_offset = id_to_offset,
};

 
void dal_hw_translate_dcn315_init(struct hw_translate *tr)
{
	tr->funcs = &funcs;
}

