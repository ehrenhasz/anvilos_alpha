
 

#include "ia_css_types.h"
#include "sh_css_defs.h"
#include "ia_css_debug.h"
#include "sh_css_frac.h"

#include "bnr/bnr_1.0/ia_css_bnr.host.h"
#include "ia_css_ynr.host.h"

const struct ia_css_nr_config default_nr_config = {
	16384,
	8192,
	1280,
	0,
	0
};

const struct ia_css_ee_config default_ee_config = {
	8192,
	128,
	2048
};

void
ia_css_nr_encode(
    struct sh_css_isp_ynr_params *to,
    const struct ia_css_nr_config *from,
    unsigned int size)
{
	(void)size;
	 
	to->threshold =
	    uDIGIT_FITTING(8192U, 16, SH_CSS_BAYER_BITS);
	to->gain_all =
	    uDIGIT_FITTING(from->ynr_gain, 16, SH_CSS_YNR_GAIN_SHIFT);
	to->gain_dir =
	    uDIGIT_FITTING(from->ynr_gain, 16, SH_CSS_YNR_GAIN_SHIFT);
	to->threshold_cb =
	    uDIGIT_FITTING(from->threshold_cb, 16, SH_CSS_BAYER_BITS);
	to->threshold_cr =
	    uDIGIT_FITTING(from->threshold_cr, 16, SH_CSS_BAYER_BITS);
}

void
ia_css_yee_encode(
    struct sh_css_isp_yee_params *to,
    const struct ia_css_yee_config *from,
    unsigned int size)
{
	int asiWk1 = (int)from->ee.gain;
	int asiWk2 = asiWk1 / 8;
	int asiWk3 = asiWk1 / 4;

	(void)size;
	 
	to->dirthreshold_s =
	    min((uDIGIT_FITTING(from->nr.direction, 16, SH_CSS_BAYER_BITS)
		 << 1),
		SH_CSS_BAYER_MAXVAL);
	to->dirthreshold_g =
	    min((uDIGIT_FITTING(from->nr.direction, 16, SH_CSS_BAYER_BITS)
		 << 4),
		SH_CSS_BAYER_MAXVAL);
	to->dirthreshold_width_log2 =
	    uFRACTION_BITS_FITTING(8);
	to->dirthreshold_width =
	    1 << to->dirthreshold_width_log2;
	to->detailgain =
	    uDIGIT_FITTING(from->ee.detail_gain, 11,
			   SH_CSS_YEE_DETAIL_GAIN_SHIFT);
	to->coring_s =
	    (uDIGIT_FITTING(56U, 16, SH_CSS_BAYER_BITS) *
	     from->ee.threshold) >> 8;
	to->coring_g =
	    (uDIGIT_FITTING(224U, 16, SH_CSS_BAYER_BITS) *
	     from->ee.threshold) >> 8;
	  * state,
    size_t size)
{
	memset(state, 0, size);
}
