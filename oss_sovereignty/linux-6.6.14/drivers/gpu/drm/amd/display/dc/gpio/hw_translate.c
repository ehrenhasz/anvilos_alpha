 

#include "dm_services.h"

 
#include "include/gpio_types.h"

 

#include "hw_translate.h"

 

#if defined(CONFIG_DRM_AMD_DC_SI)
#include "dce60/hw_translate_dce60.h"
#endif
#include "dce80/hw_translate_dce80.h"
#include "dce110/hw_translate_dce110.h"
#include "dce120/hw_translate_dce120.h"
#include "dcn10/hw_translate_dcn10.h"
#include "dcn20/hw_translate_dcn20.h"
#include "dcn21/hw_translate_dcn21.h"
#include "dcn30/hw_translate_dcn30.h"
#include "dcn315/hw_translate_dcn315.h"
#include "dcn32/hw_translate_dcn32.h"

 

bool dal_hw_translate_init(
	struct hw_translate *translate,
	enum dce_version dce_version,
	enum dce_environment dce_environment)
{
	switch (dce_version) {
#if defined(CONFIG_DRM_AMD_DC_SI)
	case DCE_VERSION_6_0:
	case DCE_VERSION_6_1:
	case DCE_VERSION_6_4:
		dal_hw_translate_dce60_init(translate);
		return true;
#endif
	case DCE_VERSION_8_0:
	case DCE_VERSION_8_1:
	case DCE_VERSION_8_3:
		dal_hw_translate_dce80_init(translate);
		return true;
	case DCE_VERSION_10_0:
	case DCE_VERSION_11_0:
	case DCE_VERSION_11_2:
	case DCE_VERSION_11_22:
		dal_hw_translate_dce110_init(translate);
		return true;
	case DCE_VERSION_12_0:
	case DCE_VERSION_12_1:
		dal_hw_translate_dce120_init(translate);
		return true;
	case DCN_VERSION_1_0:
	case DCN_VERSION_1_01:
		dal_hw_translate_dcn10_init(translate);
		return true;
	case DCN_VERSION_2_0:
		dal_hw_translate_dcn20_init(translate);
		return true;
	case DCN_VERSION_2_01:
	case DCN_VERSION_2_1:
		dal_hw_translate_dcn21_init(translate);
		return true;
	case DCN_VERSION_3_0:
	case DCN_VERSION_3_01:
	case DCN_VERSION_3_02:
	case DCN_VERSION_3_03:
	case DCN_VERSION_3_1:
	case DCN_VERSION_3_14:
	case DCN_VERSION_3_16:
		dal_hw_translate_dcn30_init(translate);
		return true;
	case DCN_VERSION_3_15:
		dal_hw_translate_dcn315_init(translate);
		return true;
	case DCN_VERSION_3_2:
	case DCN_VERSION_3_21:
		dal_hw_translate_dcn32_init(translate);
		return true;
	default:
		BREAK_TO_DEBUGGER();
		return false;
	}
}
