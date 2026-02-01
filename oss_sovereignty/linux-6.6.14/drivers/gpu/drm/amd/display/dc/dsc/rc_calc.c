
 
#include "rc_calc.h"

 
void calc_rc_params(struct rc_params *rc, const struct drm_dsc_config *pps)
{
#if defined(CONFIG_DRM_AMD_DC_FP)
	enum colour_mode mode;
	enum bits_per_comp bpc;
	bool is_navite_422_or_420;
	u16 drm_bpp = pps->bits_per_pixel;
	int slice_width  = pps->slice_width;
	int slice_height = pps->slice_height;

	mode = pps->convert_rgb ? CM_RGB : (pps->simple_422  ? CM_444 :
					   (pps->native_422  ? CM_422 :
					    pps->native_420  ? CM_420 : CM_444));
	bpc = (pps->bits_per_component == 8) ? BPC_8 : (pps->bits_per_component == 10)
					     ? BPC_10 : BPC_12;

	is_navite_422_or_420 = pps->native_422 || pps->native_420;

	DC_FP_START();
	_do_calc_rc_params(rc, mode, bpc, drm_bpp, is_navite_422_or_420,
			   slice_width, slice_height,
			   pps->dsc_version_minor);
	DC_FP_END();
#endif
}
