 

#include <linux/ctype.h>
#include <linux/export.h>
#include <linux/fb.h>  
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/of.h>

#include <video/of_display_timing.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_edid.h>
#include <drm/drm_modes.h>
#include <drm/drm_print.h>

#include "drm_crtc_internal.h"

 
void drm_mode_debug_printmodeline(const struct drm_display_mode *mode)
{
	DRM_DEBUG_KMS("Modeline " DRM_MODE_FMT "\n", DRM_MODE_ARG(mode));
}
EXPORT_SYMBOL(drm_mode_debug_printmodeline);

 
struct drm_display_mode *drm_mode_create(struct drm_device *dev)
{
	struct drm_display_mode *nmode;

	nmode = kzalloc(sizeof(struct drm_display_mode), GFP_KERNEL);
	if (!nmode)
		return NULL;

	return nmode;
}
EXPORT_SYMBOL(drm_mode_create);

 
void drm_mode_destroy(struct drm_device *dev, struct drm_display_mode *mode)
{
	if (!mode)
		return;

	kfree(mode);
}
EXPORT_SYMBOL(drm_mode_destroy);

 
void drm_mode_probed_add(struct drm_connector *connector,
			 struct drm_display_mode *mode)
{
	WARN_ON(!mutex_is_locked(&connector->dev->mode_config.mutex));

	list_add_tail(&mode->head, &connector->probed_modes);
}
EXPORT_SYMBOL(drm_mode_probed_add);

enum drm_mode_analog {
	DRM_MODE_ANALOG_NTSC,  
	DRM_MODE_ANALOG_PAL,  
};

 
#define NTSC_LINE_DURATION_NS		63556U
#define NTSC_LINES_NUMBER		525

#define NTSC_HBLK_DURATION_TYP_NS	10900U
#define NTSC_HBLK_DURATION_MIN_NS	(NTSC_HBLK_DURATION_TYP_NS - 200)
#define NTSC_HBLK_DURATION_MAX_NS	(NTSC_HBLK_DURATION_TYP_NS + 200)

#define NTSC_HACT_DURATION_TYP_NS	(NTSC_LINE_DURATION_NS - NTSC_HBLK_DURATION_TYP_NS)
#define NTSC_HACT_DURATION_MIN_NS	(NTSC_LINE_DURATION_NS - NTSC_HBLK_DURATION_MAX_NS)
#define NTSC_HACT_DURATION_MAX_NS	(NTSC_LINE_DURATION_NS - NTSC_HBLK_DURATION_MIN_NS)

#define NTSC_HFP_DURATION_TYP_NS	1500
#define NTSC_HFP_DURATION_MIN_NS	1270
#define NTSC_HFP_DURATION_MAX_NS	2220

#define NTSC_HSLEN_DURATION_TYP_NS	4700
#define NTSC_HSLEN_DURATION_MIN_NS	(NTSC_HSLEN_DURATION_TYP_NS - 100)
#define NTSC_HSLEN_DURATION_MAX_NS	(NTSC_HSLEN_DURATION_TYP_NS + 100)

#define NTSC_HBP_DURATION_TYP_NS	4700

 
#define NTSC_HBP_DURATION_MIN_NS	(NTSC_HBP_DURATION_TYP_NS - 100)
#define NTSC_HBP_DURATION_MAX_NS	(NTSC_HBP_DURATION_TYP_NS + 100)

#define PAL_LINE_DURATION_NS		64000U
#define PAL_LINES_NUMBER		625

#define PAL_HACT_DURATION_TYP_NS	51950U
#define PAL_HACT_DURATION_MIN_NS	(PAL_HACT_DURATION_TYP_NS - 100)
#define PAL_HACT_DURATION_MAX_NS	(PAL_HACT_DURATION_TYP_NS + 400)

#define PAL_HBLK_DURATION_TYP_NS	(PAL_LINE_DURATION_NS - PAL_HACT_DURATION_TYP_NS)
#define PAL_HBLK_DURATION_MIN_NS	(PAL_LINE_DURATION_NS - PAL_HACT_DURATION_MAX_NS)
#define PAL_HBLK_DURATION_MAX_NS	(PAL_LINE_DURATION_NS - PAL_HACT_DURATION_MIN_NS)

#define PAL_HFP_DURATION_TYP_NS		1650
#define PAL_HFP_DURATION_MIN_NS		(PAL_HFP_DURATION_TYP_NS - 100)
#define PAL_HFP_DURATION_MAX_NS		(PAL_HFP_DURATION_TYP_NS + 400)

#define PAL_HSLEN_DURATION_TYP_NS	4700
#define PAL_HSLEN_DURATION_MIN_NS	(PAL_HSLEN_DURATION_TYP_NS - 200)
#define PAL_HSLEN_DURATION_MAX_NS	(PAL_HSLEN_DURATION_TYP_NS + 200)

#define PAL_HBP_DURATION_TYP_NS		5700
#define PAL_HBP_DURATION_MIN_NS		(PAL_HBP_DURATION_TYP_NS - 200)
#define PAL_HBP_DURATION_MAX_NS		(PAL_HBP_DURATION_TYP_NS + 200)

struct analog_param_field {
	unsigned int even, odd;
};

#define PARAM_FIELD(_odd, _even)		\
	{ .even = _even, .odd = _odd }

struct analog_param_range {
	unsigned int	min, typ, max;
};

#define PARAM_RANGE(_min, _typ, _max)		\
	{ .min = _min, .typ = _typ, .max = _max }

struct analog_parameters {
	unsigned int			num_lines;
	unsigned int			line_duration_ns;

	struct analog_param_range	hact_ns;
	struct analog_param_range	hfp_ns;
	struct analog_param_range	hslen_ns;
	struct analog_param_range	hbp_ns;
	struct analog_param_range	hblk_ns;

	unsigned int			bt601_hfp;

	struct analog_param_field	vfp_lines;
	struct analog_param_field	vslen_lines;
	struct analog_param_field	vbp_lines;
};

#define TV_MODE_PARAMETER(_mode, _lines, _line_dur, _hact, _hfp,	\
			  _hslen, _hbp, _hblk, _bt601_hfp, _vfp,	\
			  _vslen, _vbp)					\
	[_mode] = {							\
		.num_lines = _lines,					\
		.line_duration_ns = _line_dur,				\
		.hact_ns = _hact,					\
		.hfp_ns = _hfp,						\
		.hslen_ns = _hslen,					\
		.hbp_ns = _hbp,						\
		.hblk_ns = _hblk,					\
		.bt601_hfp = _bt601_hfp,				\
		.vfp_lines = _vfp,					\
		.vslen_lines = _vslen,					\
		.vbp_lines = _vbp,					\
	}

static const struct analog_parameters tv_modes_parameters[] = {
	TV_MODE_PARAMETER(DRM_MODE_ANALOG_NTSC,
			  NTSC_LINES_NUMBER,
			  NTSC_LINE_DURATION_NS,
			  PARAM_RANGE(NTSC_HACT_DURATION_MIN_NS,
				      NTSC_HACT_DURATION_TYP_NS,
				      NTSC_HACT_DURATION_MAX_NS),
			  PARAM_RANGE(NTSC_HFP_DURATION_MIN_NS,
				      NTSC_HFP_DURATION_TYP_NS,
				      NTSC_HFP_DURATION_MAX_NS),
			  PARAM_RANGE(NTSC_HSLEN_DURATION_MIN_NS,
				      NTSC_HSLEN_DURATION_TYP_NS,
				      NTSC_HSLEN_DURATION_MAX_NS),
			  PARAM_RANGE(NTSC_HBP_DURATION_MIN_NS,
				      NTSC_HBP_DURATION_TYP_NS,
				      NTSC_HBP_DURATION_MAX_NS),
			  PARAM_RANGE(NTSC_HBLK_DURATION_MIN_NS,
				      NTSC_HBLK_DURATION_TYP_NS,
				      NTSC_HBLK_DURATION_MAX_NS),
			  16,
			  PARAM_FIELD(3, 3),
			  PARAM_FIELD(3, 3),
			  PARAM_FIELD(16, 17)),
	TV_MODE_PARAMETER(DRM_MODE_ANALOG_PAL,
			  PAL_LINES_NUMBER,
			  PAL_LINE_DURATION_NS,
			  PARAM_RANGE(PAL_HACT_DURATION_MIN_NS,
				      PAL_HACT_DURATION_TYP_NS,
				      PAL_HACT_DURATION_MAX_NS),
			  PARAM_RANGE(PAL_HFP_DURATION_MIN_NS,
				      PAL_HFP_DURATION_TYP_NS,
				      PAL_HFP_DURATION_MAX_NS),
			  PARAM_RANGE(PAL_HSLEN_DURATION_MIN_NS,
				      PAL_HSLEN_DURATION_TYP_NS,
				      PAL_HSLEN_DURATION_MAX_NS),
			  PARAM_RANGE(PAL_HBP_DURATION_MIN_NS,
				      PAL_HBP_DURATION_TYP_NS,
				      PAL_HBP_DURATION_MAX_NS),
			  PARAM_RANGE(PAL_HBLK_DURATION_MIN_NS,
				      PAL_HBLK_DURATION_TYP_NS,
				      PAL_HBLK_DURATION_MAX_NS),
			  12,

			   
			  PARAM_FIELD(3, 2),

			   
			  PARAM_FIELD(3, 3),

			   
			  PARAM_FIELD(19, 20)),
};

static int fill_analog_mode(struct drm_device *dev,
			    struct drm_display_mode *mode,
			    const struct analog_parameters *params,
			    unsigned long pixel_clock_hz,
			    unsigned int hactive,
			    unsigned int vactive,
			    bool interlace)
{
	unsigned long pixel_duration_ns = NSEC_PER_SEC / pixel_clock_hz;
	unsigned int htotal, vtotal;
	unsigned int max_hact, hact_duration_ns;
	unsigned int hblk, hblk_duration_ns;
	unsigned int hfp, hfp_duration_ns;
	unsigned int hslen, hslen_duration_ns;
	unsigned int hbp, hbp_duration_ns;
	unsigned int porches, porches_duration_ns;
	unsigned int vfp, vfp_min;
	unsigned int vbp, vbp_min;
	unsigned int vslen;
	bool bt601 = false;
	int porches_rem;
	u64 result;

	drm_dbg_kms(dev,
		    "Generating a %ux%u%c, %u-line mode with a %lu kHz clock\n",
		    hactive, vactive,
		    interlace ? 'i' : 'p',
		    params->num_lines,
		    pixel_clock_hz / 1000);

	max_hact = params->hact_ns.max / pixel_duration_ns;
	if (pixel_clock_hz == 13500000 && hactive > max_hact && hactive <= 720) {
		drm_dbg_kms(dev, "Trying to generate a BT.601 mode. Disabling checks.\n");
		bt601 = true;
	}

	 
	result = (u64)params->line_duration_ns * pixel_clock_hz;
	do_div(result, NSEC_PER_SEC);
	htotal = result;

	drm_dbg_kms(dev, "Total Horizontal Number of Pixels: %u\n", htotal);

	hact_duration_ns = hactive * pixel_duration_ns;
	if (!bt601 &&
	    (hact_duration_ns < params->hact_ns.min ||
	     hact_duration_ns > params->hact_ns.max)) {
		DRM_ERROR("Invalid horizontal active area duration: %uns (min: %u, max %u)\n",
			  hact_duration_ns, params->hact_ns.min, params->hact_ns.max);
		return -EINVAL;
	}

	hblk = htotal - hactive;
	drm_dbg_kms(dev, "Horizontal Blanking Period: %u\n", hblk);

	hblk_duration_ns = hblk * pixel_duration_ns;
	if (!bt601 &&
	    (hblk_duration_ns < params->hblk_ns.min ||
	     hblk_duration_ns > params->hblk_ns.max)) {
		DRM_ERROR("Invalid horizontal blanking duration: %uns (min: %u, max %u)\n",
			  hblk_duration_ns, params->hblk_ns.min, params->hblk_ns.max);
		return -EINVAL;
	}

	hslen = DIV_ROUND_UP(params->hslen_ns.typ, pixel_duration_ns);
	drm_dbg_kms(dev, "Horizontal Sync Period: %u\n", hslen);

	hslen_duration_ns = hslen * pixel_duration_ns;
	if (!bt601 &&
	    (hslen_duration_ns < params->hslen_ns.min ||
	     hslen_duration_ns > params->hslen_ns.max)) {
		DRM_ERROR("Invalid horizontal sync duration: %uns (min: %u, max %u)\n",
			  hslen_duration_ns, params->hslen_ns.min, params->hslen_ns.max);
		return -EINVAL;
	}

	porches = hblk - hslen;
	drm_dbg_kms(dev, "Remaining horizontal pixels for both porches: %u\n", porches);

	porches_duration_ns = porches * pixel_duration_ns;
	if (!bt601 &&
	    (porches_duration_ns > (params->hfp_ns.max + params->hbp_ns.max) ||
	     porches_duration_ns < (params->hfp_ns.min + params->hbp_ns.min))) {
		DRM_ERROR("Invalid horizontal porches duration: %uns\n", porches_duration_ns);
		return -EINVAL;
	}

	if (bt601) {
		hfp = params->bt601_hfp;
	} else {
		unsigned int hfp_min = DIV_ROUND_UP(params->hfp_ns.min,
						    pixel_duration_ns);
		unsigned int hbp_min = DIV_ROUND_UP(params->hbp_ns.min,
						    pixel_duration_ns);
		int porches_rem = porches - hfp_min - hbp_min;

		hfp = hfp_min + DIV_ROUND_UP(porches_rem, 2);
	}

	drm_dbg_kms(dev, "Horizontal Front Porch: %u\n", hfp);

	hfp_duration_ns = hfp * pixel_duration_ns;
	if (!bt601 &&
	    (hfp_duration_ns < params->hfp_ns.min ||
	     hfp_duration_ns > params->hfp_ns.max)) {
		DRM_ERROR("Invalid horizontal front porch duration: %uns (min: %u, max %u)\n",
			  hfp_duration_ns, params->hfp_ns.min, params->hfp_ns.max);
		return -EINVAL;
	}

	hbp = porches - hfp;
	drm_dbg_kms(dev, "Horizontal Back Porch: %u\n", hbp);

	hbp_duration_ns = hbp * pixel_duration_ns;
	if (!bt601 &&
	    (hbp_duration_ns < params->hbp_ns.min ||
	     hbp_duration_ns > params->hbp_ns.max)) {
		DRM_ERROR("Invalid horizontal back porch duration: %uns (min: %u, max %u)\n",
			  hbp_duration_ns, params->hbp_ns.min, params->hbp_ns.max);
		return -EINVAL;
	}

	if (htotal != (hactive + hfp + hslen + hbp))
		return -EINVAL;

	mode->clock = pixel_clock_hz / 1000;
	mode->hdisplay = hactive;
	mode->hsync_start = mode->hdisplay + hfp;
	mode->hsync_end = mode->hsync_start + hslen;
	mode->htotal = mode->hsync_end + hbp;

	if (interlace) {
		vfp_min = params->vfp_lines.even + params->vfp_lines.odd;
		vbp_min = params->vbp_lines.even + params->vbp_lines.odd;
		vslen = params->vslen_lines.even + params->vslen_lines.odd;
	} else {
		 
		vfp_min = params->vfp_lines.odd;
		vbp_min = params->vbp_lines.odd;
		vslen = params->vslen_lines.odd;
	}

	drm_dbg_kms(dev, "Vertical Sync Period: %u\n", vslen);

	porches = params->num_lines - vactive - vslen;
	drm_dbg_kms(dev, "Remaining vertical pixels for both porches: %u\n", porches);

	porches_rem = porches - vfp_min - vbp_min;
	vfp = vfp_min + (porches_rem / 2);
	drm_dbg_kms(dev, "Vertical Front Porch: %u\n", vfp);

	vbp = porches - vfp;
	drm_dbg_kms(dev, "Vertical Back Porch: %u\n", vbp);

	vtotal = vactive + vfp + vslen + vbp;
	if (params->num_lines != vtotal) {
		DRM_ERROR("Invalid vertical total: %upx (expected %upx)\n",
			  vtotal, params->num_lines);
		return -EINVAL;
	}

	mode->vdisplay = vactive;
	mode->vsync_start = mode->vdisplay + vfp;
	mode->vsync_end = mode->vsync_start + vslen;
	mode->vtotal = mode->vsync_end + vbp;

	if (mode->vtotal != params->num_lines)
		return -EINVAL;

	mode->type = DRM_MODE_TYPE_DRIVER;
	mode->flags = DRM_MODE_FLAG_NVSYNC | DRM_MODE_FLAG_NHSYNC;
	if (interlace)
		mode->flags |= DRM_MODE_FLAG_INTERLACE;

	drm_mode_set_name(mode);

	drm_dbg_kms(dev, "Generated mode " DRM_MODE_FMT "\n", DRM_MODE_ARG(mode));

	return 0;
}

 
struct drm_display_mode *drm_analog_tv_mode(struct drm_device *dev,
					    enum drm_connector_tv_mode tv_mode,
					    unsigned long pixel_clock_hz,
					    unsigned int hdisplay,
					    unsigned int vdisplay,
					    bool interlace)
{
	struct drm_display_mode *mode;
	enum drm_mode_analog analog;
	int ret;

	switch (tv_mode) {
	case DRM_MODE_TV_MODE_NTSC:
		fallthrough;
	case DRM_MODE_TV_MODE_NTSC_443:
		fallthrough;
	case DRM_MODE_TV_MODE_NTSC_J:
		fallthrough;
	case DRM_MODE_TV_MODE_PAL_M:
		analog = DRM_MODE_ANALOG_NTSC;
		break;

	case DRM_MODE_TV_MODE_PAL:
		fallthrough;
	case DRM_MODE_TV_MODE_PAL_N:
		fallthrough;
	case DRM_MODE_TV_MODE_SECAM:
		analog = DRM_MODE_ANALOG_PAL;
		break;

	default:
		return NULL;
	}

	mode = drm_mode_create(dev);
	if (!mode)
		return NULL;

	ret = fill_analog_mode(dev, mode,
			       &tv_modes_parameters[analog],
			       pixel_clock_hz, hdisplay, vdisplay, interlace);
	if (ret)
		goto err_free_mode;

	return mode;

err_free_mode:
	drm_mode_destroy(dev, mode);
	return NULL;
}
EXPORT_SYMBOL(drm_analog_tv_mode);

 
struct drm_display_mode *drm_cvt_mode(struct drm_device *dev, int hdisplay,
				      int vdisplay, int vrefresh,
				      bool reduced, bool interlaced, bool margins)
{
#define HV_FACTOR			1000
	 
#define	CVT_MARGIN_PERCENTAGE		18
	 
#define	CVT_H_GRANULARITY		8
	 
#define	CVT_MIN_V_PORCH			3
	 
#define	CVT_MIN_V_BPORCH		6
	 
#define CVT_CLOCK_STEP			250
	struct drm_display_mode *drm_mode;
	unsigned int vfieldrate, hperiod;
	int hdisplay_rnd, hmargin, vdisplay_rnd, vmargin, vsync;
	int interlace;
	u64 tmp;

	if (!hdisplay || !vdisplay)
		return NULL;

	 
	drm_mode = drm_mode_create(dev);
	if (!drm_mode)
		return NULL;

	 
	if (!vrefresh)
		vrefresh = 60;

	 
	if (interlaced)
		vfieldrate = vrefresh * 2;
	else
		vfieldrate = vrefresh;

	 
	hdisplay_rnd = hdisplay - (hdisplay % CVT_H_GRANULARITY);

	 
	hmargin = 0;
	if (margins) {
		hmargin = hdisplay_rnd * CVT_MARGIN_PERCENTAGE / 1000;
		hmargin -= hmargin % CVT_H_GRANULARITY;
	}
	 
	drm_mode->hdisplay = hdisplay_rnd + 2 * hmargin;

	 
	if (interlaced)
		vdisplay_rnd = vdisplay / 2;
	else
		vdisplay_rnd = vdisplay;

	 
	vmargin = 0;
	if (margins)
		vmargin = vdisplay_rnd * CVT_MARGIN_PERCENTAGE / 1000;

	drm_mode->vdisplay = vdisplay + 2 * vmargin;

	 
	if (interlaced)
		interlace = 1;
	else
		interlace = 0;

	 
	if (!(vdisplay % 3) && ((vdisplay * 4 / 3) == hdisplay))
		vsync = 4;
	else if (!(vdisplay % 9) && ((vdisplay * 16 / 9) == hdisplay))
		vsync = 5;
	else if (!(vdisplay % 10) && ((vdisplay * 16 / 10) == hdisplay))
		vsync = 6;
	else if (!(vdisplay % 4) && ((vdisplay * 5 / 4) == hdisplay))
		vsync = 7;
	else if (!(vdisplay % 9) && ((vdisplay * 15 / 9) == hdisplay))
		vsync = 7;
	else  
		vsync = 10;

	if (!reduced) {
		 
		 
		int tmp1, tmp2;
#define CVT_MIN_VSYNC_BP	550
		 
#define CVT_HSYNC_PERCENTAGE	8
		unsigned int hblank_percentage;
		int vsyncandback_porch, __maybe_unused vback_porch, hblank;

		 
		tmp1 = HV_FACTOR * 1000000  -
				CVT_MIN_VSYNC_BP * HV_FACTOR * vfieldrate;
		tmp2 = (vdisplay_rnd + 2 * vmargin + CVT_MIN_V_PORCH) * 2 +
				interlace;
		hperiod = tmp1 * 2 / (tmp2 * vfieldrate);

		tmp1 = CVT_MIN_VSYNC_BP * HV_FACTOR / hperiod + 1;
		 
		if (tmp1 < (vsync + CVT_MIN_V_PORCH))
			vsyncandback_porch = vsync + CVT_MIN_V_PORCH;
		else
			vsyncandback_porch = tmp1;
		 
		vback_porch = vsyncandback_porch - vsync;
		drm_mode->vtotal = vdisplay_rnd + 2 * vmargin +
				vsyncandback_porch + CVT_MIN_V_PORCH;
		 
		 
#define CVT_M_FACTOR	600
		 
#define CVT_C_FACTOR	40
		 
#define CVT_K_FACTOR	128
		 
#define CVT_J_FACTOR	20
#define CVT_M_PRIME	(CVT_M_FACTOR * CVT_K_FACTOR / 256)
#define CVT_C_PRIME	((CVT_C_FACTOR - CVT_J_FACTOR) * CVT_K_FACTOR / 256 + \
			 CVT_J_FACTOR)
		 
		hblank_percentage = CVT_C_PRIME * HV_FACTOR - CVT_M_PRIME *
					hperiod / 1000;
		 
		if (hblank_percentage < 20 * HV_FACTOR)
			hblank_percentage = 20 * HV_FACTOR;
		hblank = drm_mode->hdisplay * hblank_percentage /
			 (100 * HV_FACTOR - hblank_percentage);
		hblank -= hblank % (2 * CVT_H_GRANULARITY);
		 
		drm_mode->htotal = drm_mode->hdisplay + hblank;
		drm_mode->hsync_end = drm_mode->hdisplay + hblank / 2;
		drm_mode->hsync_start = drm_mode->hsync_end -
			(drm_mode->htotal * CVT_HSYNC_PERCENTAGE) / 100;
		drm_mode->hsync_start += CVT_H_GRANULARITY -
			drm_mode->hsync_start % CVT_H_GRANULARITY;
		 
		drm_mode->vsync_start = drm_mode->vdisplay + CVT_MIN_V_PORCH;
		drm_mode->vsync_end = drm_mode->vsync_start + vsync;
	} else {
		 
		 
#define CVT_RB_MIN_VBLANK	460
		 
#define CVT_RB_H_SYNC		32
		 
#define CVT_RB_H_BLANK		160
		 
#define CVT_RB_VFPORCH		3
		int vbilines;
		int tmp1, tmp2;
		 
		tmp1 = HV_FACTOR * 1000000 -
			CVT_RB_MIN_VBLANK * HV_FACTOR * vfieldrate;
		tmp2 = vdisplay_rnd + 2 * vmargin;
		hperiod = tmp1 / (tmp2 * vfieldrate);
		 
		vbilines = CVT_RB_MIN_VBLANK * HV_FACTOR / hperiod + 1;
		 
		if (vbilines < (CVT_RB_VFPORCH + vsync + CVT_MIN_V_BPORCH))
			vbilines = CVT_RB_VFPORCH + vsync + CVT_MIN_V_BPORCH;
		 
		drm_mode->vtotal = vdisplay_rnd + 2 * vmargin + vbilines;
		 
		drm_mode->htotal = drm_mode->hdisplay + CVT_RB_H_BLANK;
		 
		drm_mode->hsync_end = drm_mode->hdisplay + CVT_RB_H_BLANK / 2;
		drm_mode->hsync_start = drm_mode->hsync_end - CVT_RB_H_SYNC;
		 
		drm_mode->vsync_start = drm_mode->vdisplay + CVT_RB_VFPORCH;
		drm_mode->vsync_end = drm_mode->vsync_start + vsync;
	}
	 
	tmp = drm_mode->htotal;  
	tmp *= HV_FACTOR * 1000;
	do_div(tmp, hperiod);
	tmp -= drm_mode->clock % CVT_CLOCK_STEP;
	drm_mode->clock = tmp;
	 
	 
	if (interlaced) {
		drm_mode->vtotal *= 2;
		drm_mode->flags |= DRM_MODE_FLAG_INTERLACE;
	}
	 
	drm_mode_set_name(drm_mode);
	if (reduced)
		drm_mode->flags |= (DRM_MODE_FLAG_PHSYNC |
					DRM_MODE_FLAG_NVSYNC);
	else
		drm_mode->flags |= (DRM_MODE_FLAG_PVSYNC |
					DRM_MODE_FLAG_NHSYNC);

	return drm_mode;
}
EXPORT_SYMBOL(drm_cvt_mode);

 
struct drm_display_mode *
drm_gtf_mode_complex(struct drm_device *dev, int hdisplay, int vdisplay,
		     int vrefresh, bool interlaced, int margins,
		     int GTF_M, int GTF_2C, int GTF_K, int GTF_2J)
{	 
#define	GTF_MARGIN_PERCENTAGE		18
	 
#define	GTF_CELL_GRAN			8
	 
#define	GTF_MIN_V_PORCH			1
	 
#define V_SYNC_RQD			3
	 
#define H_SYNC_PERCENT			8
	 
#define MIN_VSYNC_PLUS_BP		550
	 
#define GTF_C_PRIME	((((GTF_2C - GTF_2J) * GTF_K / 256) + GTF_2J) / 2)
#define GTF_M_PRIME	(GTF_K * GTF_M / 256)
	struct drm_display_mode *drm_mode;
	unsigned int hdisplay_rnd, vdisplay_rnd, vfieldrate_rqd;
	int top_margin, bottom_margin;
	int interlace;
	unsigned int hfreq_est;
	int vsync_plus_bp, __maybe_unused vback_porch;
	unsigned int vtotal_lines, __maybe_unused vfieldrate_est;
	unsigned int __maybe_unused hperiod;
	unsigned int vfield_rate, __maybe_unused vframe_rate;
	int left_margin, right_margin;
	unsigned int total_active_pixels, ideal_duty_cycle;
	unsigned int hblank, total_pixels, pixel_freq;
	int hsync, hfront_porch, vodd_front_porch_lines;
	unsigned int tmp1, tmp2;

	if (!hdisplay || !vdisplay)
		return NULL;

	drm_mode = drm_mode_create(dev);
	if (!drm_mode)
		return NULL;

	 
	hdisplay_rnd = (hdisplay + GTF_CELL_GRAN / 2) / GTF_CELL_GRAN;
	hdisplay_rnd = hdisplay_rnd * GTF_CELL_GRAN;

	 
	if (interlaced)
		vdisplay_rnd = vdisplay / 2;
	else
		vdisplay_rnd = vdisplay;

	 
	if (interlaced)
		vfieldrate_rqd = vrefresh * 2;
	else
		vfieldrate_rqd = vrefresh;

	 
	top_margin = 0;
	if (margins)
		top_margin = (vdisplay_rnd * GTF_MARGIN_PERCENTAGE + 500) /
				1000;
	 
	bottom_margin = top_margin;

	 
	if (interlaced)
		interlace = 1;
	else
		interlace = 0;

	 
	{
		tmp1 = (1000000  - MIN_VSYNC_PLUS_BP * vfieldrate_rqd) / 500;
		tmp2 = (vdisplay_rnd + 2 * top_margin + GTF_MIN_V_PORCH) *
				2 + interlace;
		hfreq_est = (tmp2 * 1000 * vfieldrate_rqd) / tmp1;
	}

	 
	 
	vsync_plus_bp = MIN_VSYNC_PLUS_BP * hfreq_est / 1000;
	vsync_plus_bp = (vsync_plus_bp + 500) / 1000;
	 
	vback_porch = vsync_plus_bp - V_SYNC_RQD;
	 
	vtotal_lines = vdisplay_rnd + top_margin + bottom_margin +
			vsync_plus_bp + GTF_MIN_V_PORCH;
	 
	vfieldrate_est = hfreq_est / vtotal_lines;
	 
	hperiod = 1000000 / (vfieldrate_rqd * vtotal_lines);

	 
	vfield_rate = hfreq_est / vtotal_lines;
	 
	if (interlaced)
		vframe_rate = vfield_rate / 2;
	else
		vframe_rate = vfield_rate;
	 
	if (margins)
		left_margin = (hdisplay_rnd * GTF_MARGIN_PERCENTAGE + 500) /
				1000;
	else
		left_margin = 0;

	 
	right_margin = left_margin;
	 
	total_active_pixels = hdisplay_rnd + left_margin + right_margin;
	 
	ideal_duty_cycle = GTF_C_PRIME * 1000 -
				(GTF_M_PRIME * 1000000 / hfreq_est);
	 
	hblank = total_active_pixels * ideal_duty_cycle /
			(100000 - ideal_duty_cycle);
	hblank = (hblank + GTF_CELL_GRAN) / (2 * GTF_CELL_GRAN);
	hblank = hblank * 2 * GTF_CELL_GRAN;
	 
	total_pixels = total_active_pixels + hblank;
	 
	pixel_freq = total_pixels * hfreq_est / 1000;
	 
	 
	hsync = H_SYNC_PERCENT * total_pixels / 100;
	hsync = (hsync + GTF_CELL_GRAN / 2) / GTF_CELL_GRAN;
	hsync = hsync * GTF_CELL_GRAN;
	 
	hfront_porch = hblank / 2 - hsync;
	 
	vodd_front_porch_lines = GTF_MIN_V_PORCH ;

	 
	drm_mode->hdisplay = hdisplay_rnd;
	drm_mode->hsync_start = hdisplay_rnd + hfront_porch;
	drm_mode->hsync_end = drm_mode->hsync_start + hsync;
	drm_mode->htotal = total_pixels;
	drm_mode->vdisplay = vdisplay_rnd;
	drm_mode->vsync_start = vdisplay_rnd + vodd_front_porch_lines;
	drm_mode->vsync_end = drm_mode->vsync_start + V_SYNC_RQD;
	drm_mode->vtotal = vtotal_lines;

	drm_mode->clock = pixel_freq;

	if (interlaced) {
		drm_mode->vtotal *= 2;
		drm_mode->flags |= DRM_MODE_FLAG_INTERLACE;
	}

	drm_mode_set_name(drm_mode);
	if (GTF_M == 600 && GTF_2C == 80 && GTF_K == 128 && GTF_2J == 40)
		drm_mode->flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC;
	else
		drm_mode->flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC;

	return drm_mode;
}
EXPORT_SYMBOL(drm_gtf_mode_complex);

 
struct drm_display_mode *
drm_gtf_mode(struct drm_device *dev, int hdisplay, int vdisplay, int vrefresh,
	     bool interlaced, int margins)
{
	return drm_gtf_mode_complex(dev, hdisplay, vdisplay, vrefresh,
				    interlaced, margins,
				    600, 40 * 2, 128, 20 * 2);
}
EXPORT_SYMBOL(drm_gtf_mode);

#ifdef CONFIG_VIDEOMODE_HELPERS
 
void drm_display_mode_from_videomode(const struct videomode *vm,
				     struct drm_display_mode *dmode)
{
	dmode->hdisplay = vm->hactive;
	dmode->hsync_start = dmode->hdisplay + vm->hfront_porch;
	dmode->hsync_end = dmode->hsync_start + vm->hsync_len;
	dmode->htotal = dmode->hsync_end + vm->hback_porch;

	dmode->vdisplay = vm->vactive;
	dmode->vsync_start = dmode->vdisplay + vm->vfront_porch;
	dmode->vsync_end = dmode->vsync_start + vm->vsync_len;
	dmode->vtotal = dmode->vsync_end + vm->vback_porch;

	dmode->clock = vm->pixelclock / 1000;

	dmode->flags = 0;
	if (vm->flags & DISPLAY_FLAGS_HSYNC_HIGH)
		dmode->flags |= DRM_MODE_FLAG_PHSYNC;
	else if (vm->flags & DISPLAY_FLAGS_HSYNC_LOW)
		dmode->flags |= DRM_MODE_FLAG_NHSYNC;
	if (vm->flags & DISPLAY_FLAGS_VSYNC_HIGH)
		dmode->flags |= DRM_MODE_FLAG_PVSYNC;
	else if (vm->flags & DISPLAY_FLAGS_VSYNC_LOW)
		dmode->flags |= DRM_MODE_FLAG_NVSYNC;
	if (vm->flags & DISPLAY_FLAGS_INTERLACED)
		dmode->flags |= DRM_MODE_FLAG_INTERLACE;
	if (vm->flags & DISPLAY_FLAGS_DOUBLESCAN)
		dmode->flags |= DRM_MODE_FLAG_DBLSCAN;
	if (vm->flags & DISPLAY_FLAGS_DOUBLECLK)
		dmode->flags |= DRM_MODE_FLAG_DBLCLK;
	drm_mode_set_name(dmode);
}
EXPORT_SYMBOL_GPL(drm_display_mode_from_videomode);

 
void drm_display_mode_to_videomode(const struct drm_display_mode *dmode,
				   struct videomode *vm)
{
	vm->hactive = dmode->hdisplay;
	vm->hfront_porch = dmode->hsync_start - dmode->hdisplay;
	vm->hsync_len = dmode->hsync_end - dmode->hsync_start;
	vm->hback_porch = dmode->htotal - dmode->hsync_end;

	vm->vactive = dmode->vdisplay;
	vm->vfront_porch = dmode->vsync_start - dmode->vdisplay;
	vm->vsync_len = dmode->vsync_end - dmode->vsync_start;
	vm->vback_porch = dmode->vtotal - dmode->vsync_end;

	vm->pixelclock = dmode->clock * 1000;

	vm->flags = 0;
	if (dmode->flags & DRM_MODE_FLAG_PHSYNC)
		vm->flags |= DISPLAY_FLAGS_HSYNC_HIGH;
	else if (dmode->flags & DRM_MODE_FLAG_NHSYNC)
		vm->flags |= DISPLAY_FLAGS_HSYNC_LOW;
	if (dmode->flags & DRM_MODE_FLAG_PVSYNC)
		vm->flags |= DISPLAY_FLAGS_VSYNC_HIGH;
	else if (dmode->flags & DRM_MODE_FLAG_NVSYNC)
		vm->flags |= DISPLAY_FLAGS_VSYNC_LOW;
	if (dmode->flags & DRM_MODE_FLAG_INTERLACE)
		vm->flags |= DISPLAY_FLAGS_INTERLACED;
	if (dmode->flags & DRM_MODE_FLAG_DBLSCAN)
		vm->flags |= DISPLAY_FLAGS_DOUBLESCAN;
	if (dmode->flags & DRM_MODE_FLAG_DBLCLK)
		vm->flags |= DISPLAY_FLAGS_DOUBLECLK;
}
EXPORT_SYMBOL_GPL(drm_display_mode_to_videomode);

 
void drm_bus_flags_from_videomode(const struct videomode *vm, u32 *bus_flags)
{
	*bus_flags = 0;
	if (vm->flags & DISPLAY_FLAGS_PIXDATA_POSEDGE)
		*bus_flags |= DRM_BUS_FLAG_PIXDATA_DRIVE_POSEDGE;
	if (vm->flags & DISPLAY_FLAGS_PIXDATA_NEGEDGE)
		*bus_flags |= DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE;

	if (vm->flags & DISPLAY_FLAGS_SYNC_POSEDGE)
		*bus_flags |= DRM_BUS_FLAG_SYNC_DRIVE_POSEDGE;
	if (vm->flags & DISPLAY_FLAGS_SYNC_NEGEDGE)
		*bus_flags |= DRM_BUS_FLAG_SYNC_DRIVE_NEGEDGE;

	if (vm->flags & DISPLAY_FLAGS_DE_LOW)
		*bus_flags |= DRM_BUS_FLAG_DE_LOW;
	if (vm->flags & DISPLAY_FLAGS_DE_HIGH)
		*bus_flags |= DRM_BUS_FLAG_DE_HIGH;
}
EXPORT_SYMBOL_GPL(drm_bus_flags_from_videomode);

#ifdef CONFIG_OF
 
int of_get_drm_display_mode(struct device_node *np,
			    struct drm_display_mode *dmode, u32 *bus_flags,
			    int index)
{
	struct videomode vm;
	int ret;

	ret = of_get_videomode(np, &vm, index);
	if (ret)
		return ret;

	drm_display_mode_from_videomode(&vm, dmode);
	if (bus_flags)
		drm_bus_flags_from_videomode(&vm, bus_flags);

	pr_debug("%pOF: got %dx%d display mode\n",
		np, vm.hactive, vm.vactive);
	drm_mode_debug_printmodeline(dmode);

	return 0;
}
EXPORT_SYMBOL_GPL(of_get_drm_display_mode);

 
int of_get_drm_panel_display_mode(struct device_node *np,
				  struct drm_display_mode *dmode, u32 *bus_flags)
{
	u32 width_mm = 0, height_mm = 0;
	struct display_timing timing;
	struct videomode vm;
	int ret;

	ret = of_get_display_timing(np, "panel-timing", &timing);
	if (ret)
		return ret;

	videomode_from_timing(&timing, &vm);

	memset(dmode, 0, sizeof(*dmode));
	drm_display_mode_from_videomode(&vm, dmode);
	if (bus_flags)
		drm_bus_flags_from_videomode(&vm, bus_flags);

	ret = of_property_read_u32(np, "width-mm", &width_mm);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "height-mm", &height_mm);
	if (ret)
		return ret;

	dmode->width_mm = width_mm;
	dmode->height_mm = height_mm;

	drm_mode_debug_printmodeline(dmode);

	return 0;
}
EXPORT_SYMBOL_GPL(of_get_drm_panel_display_mode);
#endif  
#endif  

 
void drm_mode_set_name(struct drm_display_mode *mode)
{
	bool interlaced = !!(mode->flags & DRM_MODE_FLAG_INTERLACE);

	snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "%dx%d%s",
		 mode->hdisplay, mode->vdisplay,
		 interlaced ? "i" : "");
}
EXPORT_SYMBOL(drm_mode_set_name);

 
int drm_mode_vrefresh(const struct drm_display_mode *mode)
{
	unsigned int num, den;

	if (mode->htotal == 0 || mode->vtotal == 0)
		return 0;

	num = mode->clock;
	den = mode->htotal * mode->vtotal;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		num *= 2;
	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		den *= 2;
	if (mode->vscan > 1)
		den *= mode->vscan;

	return DIV_ROUND_CLOSEST_ULL(mul_u32_u32(num, 1000), den);
}
EXPORT_SYMBOL(drm_mode_vrefresh);

 
void drm_mode_get_hv_timing(const struct drm_display_mode *mode,
			    int *hdisplay, int *vdisplay)
{
	struct drm_display_mode adjusted;

	drm_mode_init(&adjusted, mode);

	drm_mode_set_crtcinfo(&adjusted, CRTC_STEREO_DOUBLE_ONLY);
	*hdisplay = adjusted.crtc_hdisplay;
	*vdisplay = adjusted.crtc_vdisplay;
}
EXPORT_SYMBOL(drm_mode_get_hv_timing);

 
void drm_mode_set_crtcinfo(struct drm_display_mode *p, int adjust_flags)
{
	if (!p)
		return;

	p->crtc_clock = p->clock;
	p->crtc_hdisplay = p->hdisplay;
	p->crtc_hsync_start = p->hsync_start;
	p->crtc_hsync_end = p->hsync_end;
	p->crtc_htotal = p->htotal;
	p->crtc_hskew = p->hskew;
	p->crtc_vdisplay = p->vdisplay;
	p->crtc_vsync_start = p->vsync_start;
	p->crtc_vsync_end = p->vsync_end;
	p->crtc_vtotal = p->vtotal;

	if (p->flags & DRM_MODE_FLAG_INTERLACE) {
		if (adjust_flags & CRTC_INTERLACE_HALVE_V) {
			p->crtc_vdisplay /= 2;
			p->crtc_vsync_start /= 2;
			p->crtc_vsync_end /= 2;
			p->crtc_vtotal /= 2;
		}
	}

	if (!(adjust_flags & CRTC_NO_DBLSCAN)) {
		if (p->flags & DRM_MODE_FLAG_DBLSCAN) {
			p->crtc_vdisplay *= 2;
			p->crtc_vsync_start *= 2;
			p->crtc_vsync_end *= 2;
			p->crtc_vtotal *= 2;
		}
	}

	if (!(adjust_flags & CRTC_NO_VSCAN)) {
		if (p->vscan > 1) {
			p->crtc_vdisplay *= p->vscan;
			p->crtc_vsync_start *= p->vscan;
			p->crtc_vsync_end *= p->vscan;
			p->crtc_vtotal *= p->vscan;
		}
	}

	if (adjust_flags & CRTC_STEREO_DOUBLE) {
		unsigned int layout = p->flags & DRM_MODE_FLAG_3D_MASK;

		switch (layout) {
		case DRM_MODE_FLAG_3D_FRAME_PACKING:
			p->crtc_clock *= 2;
			p->crtc_vdisplay += p->crtc_vtotal;
			p->crtc_vsync_start += p->crtc_vtotal;
			p->crtc_vsync_end += p->crtc_vtotal;
			p->crtc_vtotal += p->crtc_vtotal;
			break;
		}
	}

	p->crtc_vblank_start = min(p->crtc_vsync_start, p->crtc_vdisplay);
	p->crtc_vblank_end = max(p->crtc_vsync_end, p->crtc_vtotal);
	p->crtc_hblank_start = min(p->crtc_hsync_start, p->crtc_hdisplay);
	p->crtc_hblank_end = max(p->crtc_hsync_end, p->crtc_htotal);
}
EXPORT_SYMBOL(drm_mode_set_crtcinfo);

 
void drm_mode_copy(struct drm_display_mode *dst, const struct drm_display_mode *src)
{
	struct list_head head = dst->head;

	*dst = *src;
	dst->head = head;
}
EXPORT_SYMBOL(drm_mode_copy);

 
void drm_mode_init(struct drm_display_mode *dst, const struct drm_display_mode *src)
{
	memset(dst, 0, sizeof(*dst));
	drm_mode_copy(dst, src);
}
EXPORT_SYMBOL(drm_mode_init);

 
struct drm_display_mode *drm_mode_duplicate(struct drm_device *dev,
					    const struct drm_display_mode *mode)
{
	struct drm_display_mode *nmode;

	nmode = drm_mode_create(dev);
	if (!nmode)
		return NULL;

	drm_mode_copy(nmode, mode);

	return nmode;
}
EXPORT_SYMBOL(drm_mode_duplicate);

static bool drm_mode_match_timings(const struct drm_display_mode *mode1,
				   const struct drm_display_mode *mode2)
{
	return mode1->hdisplay == mode2->hdisplay &&
		mode1->hsync_start == mode2->hsync_start &&
		mode1->hsync_end == mode2->hsync_end &&
		mode1->htotal == mode2->htotal &&
		mode1->hskew == mode2->hskew &&
		mode1->vdisplay == mode2->vdisplay &&
		mode1->vsync_start == mode2->vsync_start &&
		mode1->vsync_end == mode2->vsync_end &&
		mode1->vtotal == mode2->vtotal &&
		mode1->vscan == mode2->vscan;
}

static bool drm_mode_match_clock(const struct drm_display_mode *mode1,
				  const struct drm_display_mode *mode2)
{
	 
	if (mode1->clock && mode2->clock)
		return KHZ2PICOS(mode1->clock) == KHZ2PICOS(mode2->clock);
	else
		return mode1->clock == mode2->clock;
}

static bool drm_mode_match_flags(const struct drm_display_mode *mode1,
				 const struct drm_display_mode *mode2)
{
	return (mode1->flags & ~DRM_MODE_FLAG_3D_MASK) ==
		(mode2->flags & ~DRM_MODE_FLAG_3D_MASK);
}

static bool drm_mode_match_3d_flags(const struct drm_display_mode *mode1,
				    const struct drm_display_mode *mode2)
{
	return (mode1->flags & DRM_MODE_FLAG_3D_MASK) ==
		(mode2->flags & DRM_MODE_FLAG_3D_MASK);
}

static bool drm_mode_match_aspect_ratio(const struct drm_display_mode *mode1,
					const struct drm_display_mode *mode2)
{
	return mode1->picture_aspect_ratio == mode2->picture_aspect_ratio;
}

 
bool drm_mode_match(const struct drm_display_mode *mode1,
		    const struct drm_display_mode *mode2,
		    unsigned int match_flags)
{
	if (!mode1 && !mode2)
		return true;

	if (!mode1 || !mode2)
		return false;

	if (match_flags & DRM_MODE_MATCH_TIMINGS &&
	    !drm_mode_match_timings(mode1, mode2))
		return false;

	if (match_flags & DRM_MODE_MATCH_CLOCK &&
	    !drm_mode_match_clock(mode1, mode2))
		return false;

	if (match_flags & DRM_MODE_MATCH_FLAGS &&
	    !drm_mode_match_flags(mode1, mode2))
		return false;

	if (match_flags & DRM_MODE_MATCH_3D_FLAGS &&
	    !drm_mode_match_3d_flags(mode1, mode2))
		return false;

	if (match_flags & DRM_MODE_MATCH_ASPECT_RATIO &&
	    !drm_mode_match_aspect_ratio(mode1, mode2))
		return false;

	return true;
}
EXPORT_SYMBOL(drm_mode_match);

 
bool drm_mode_equal(const struct drm_display_mode *mode1,
		    const struct drm_display_mode *mode2)
{
	return drm_mode_match(mode1, mode2,
			      DRM_MODE_MATCH_TIMINGS |
			      DRM_MODE_MATCH_CLOCK |
			      DRM_MODE_MATCH_FLAGS |
			      DRM_MODE_MATCH_3D_FLAGS|
			      DRM_MODE_MATCH_ASPECT_RATIO);
}
EXPORT_SYMBOL(drm_mode_equal);

 
bool drm_mode_equal_no_clocks(const struct drm_display_mode *mode1,
			      const struct drm_display_mode *mode2)
{
	return drm_mode_match(mode1, mode2,
			      DRM_MODE_MATCH_TIMINGS |
			      DRM_MODE_MATCH_FLAGS |
			      DRM_MODE_MATCH_3D_FLAGS);
}
EXPORT_SYMBOL(drm_mode_equal_no_clocks);

 
bool drm_mode_equal_no_clocks_no_stereo(const struct drm_display_mode *mode1,
					const struct drm_display_mode *mode2)
{
	return drm_mode_match(mode1, mode2,
			      DRM_MODE_MATCH_TIMINGS |
			      DRM_MODE_MATCH_FLAGS);
}
EXPORT_SYMBOL(drm_mode_equal_no_clocks_no_stereo);

static enum drm_mode_status
drm_mode_validate_basic(const struct drm_display_mode *mode)
{
	if (mode->type & ~DRM_MODE_TYPE_ALL)
		return MODE_BAD;

	if (mode->flags & ~DRM_MODE_FLAG_ALL)
		return MODE_BAD;

	if ((mode->flags & DRM_MODE_FLAG_3D_MASK) > DRM_MODE_FLAG_3D_MAX)
		return MODE_BAD;

	if (mode->clock == 0)
		return MODE_CLOCK_LOW;

	if (mode->hdisplay == 0 ||
	    mode->hsync_start < mode->hdisplay ||
	    mode->hsync_end < mode->hsync_start ||
	    mode->htotal < mode->hsync_end)
		return MODE_H_ILLEGAL;

	if (mode->vdisplay == 0 ||
	    mode->vsync_start < mode->vdisplay ||
	    mode->vsync_end < mode->vsync_start ||
	    mode->vtotal < mode->vsync_end)
		return MODE_V_ILLEGAL;

	return MODE_OK;
}

 
enum drm_mode_status
drm_mode_validate_driver(struct drm_device *dev,
			const struct drm_display_mode *mode)
{
	enum drm_mode_status status;

	status = drm_mode_validate_basic(mode);
	if (status != MODE_OK)
		return status;

	if (dev->mode_config.funcs->mode_valid)
		return dev->mode_config.funcs->mode_valid(dev, mode);
	else
		return MODE_OK;
}
EXPORT_SYMBOL(drm_mode_validate_driver);

 
enum drm_mode_status
drm_mode_validate_size(const struct drm_display_mode *mode,
		       int maxX, int maxY)
{
	if (maxX > 0 && mode->hdisplay > maxX)
		return MODE_VIRTUAL_X;

	if (maxY > 0 && mode->vdisplay > maxY)
		return MODE_VIRTUAL_Y;

	return MODE_OK;
}
EXPORT_SYMBOL(drm_mode_validate_size);

 
enum drm_mode_status
drm_mode_validate_ycbcr420(const struct drm_display_mode *mode,
			   struct drm_connector *connector)
{
	if (!connector->ycbcr_420_allowed &&
	    drm_mode_is_420_only(&connector->display_info, mode))
		return MODE_NO_420;

	return MODE_OK;
}
EXPORT_SYMBOL(drm_mode_validate_ycbcr420);

#define MODE_STATUS(status) [MODE_ ## status + 3] = #status

static const char * const drm_mode_status_names[] = {
	MODE_STATUS(OK),
	MODE_STATUS(HSYNC),
	MODE_STATUS(VSYNC),
	MODE_STATUS(H_ILLEGAL),
	MODE_STATUS(V_ILLEGAL),
	MODE_STATUS(BAD_WIDTH),
	MODE_STATUS(NOMODE),
	MODE_STATUS(NO_INTERLACE),
	MODE_STATUS(NO_DBLESCAN),
	MODE_STATUS(NO_VSCAN),
	MODE_STATUS(MEM),
	MODE_STATUS(VIRTUAL_X),
	MODE_STATUS(VIRTUAL_Y),
	MODE_STATUS(MEM_VIRT),
	MODE_STATUS(NOCLOCK),
	MODE_STATUS(CLOCK_HIGH),
	MODE_STATUS(CLOCK_LOW),
	MODE_STATUS(CLOCK_RANGE),
	MODE_STATUS(BAD_HVALUE),
	MODE_STATUS(BAD_VVALUE),
	MODE_STATUS(BAD_VSCAN),
	MODE_STATUS(HSYNC_NARROW),
	MODE_STATUS(HSYNC_WIDE),
	MODE_STATUS(HBLANK_NARROW),
	MODE_STATUS(HBLANK_WIDE),
	MODE_STATUS(VSYNC_NARROW),
	MODE_STATUS(VSYNC_WIDE),
	MODE_STATUS(VBLANK_NARROW),
	MODE_STATUS(VBLANK_WIDE),
	MODE_STATUS(PANEL),
	MODE_STATUS(INTERLACE_WIDTH),
	MODE_STATUS(ONE_WIDTH),
	MODE_STATUS(ONE_HEIGHT),
	MODE_STATUS(ONE_SIZE),
	MODE_STATUS(NO_REDUCED),
	MODE_STATUS(NO_STEREO),
	MODE_STATUS(NO_420),
	MODE_STATUS(STALE),
	MODE_STATUS(BAD),
	MODE_STATUS(ERROR),
};

#undef MODE_STATUS

const char *drm_get_mode_status_name(enum drm_mode_status status)
{
	int index = status + 3;

	if (WARN_ON(index < 0 || index >= ARRAY_SIZE(drm_mode_status_names)))
		return "";

	return drm_mode_status_names[index];
}

 
void drm_mode_prune_invalid(struct drm_device *dev,
			    struct list_head *mode_list, bool verbose)
{
	struct drm_display_mode *mode, *t;

	list_for_each_entry_safe(mode, t, mode_list, head) {
		if (mode->status != MODE_OK) {
			list_del(&mode->head);
			if (mode->type & DRM_MODE_TYPE_USERDEF) {
				drm_warn(dev, "User-defined mode not supported: "
					 DRM_MODE_FMT "\n", DRM_MODE_ARG(mode));
			}
			if (verbose) {
				drm_mode_debug_printmodeline(mode);
				DRM_DEBUG_KMS("Not using %s mode: %s\n",
					      mode->name,
					      drm_get_mode_status_name(mode->status));
			}
			drm_mode_destroy(dev, mode);
		}
	}
}
EXPORT_SYMBOL(drm_mode_prune_invalid);

 
static int drm_mode_compare(void *priv, const struct list_head *lh_a,
			    const struct list_head *lh_b)
{
	struct drm_display_mode *a = list_entry(lh_a, struct drm_display_mode, head);
	struct drm_display_mode *b = list_entry(lh_b, struct drm_display_mode, head);
	int diff;

	diff = ((b->type & DRM_MODE_TYPE_PREFERRED) != 0) -
		((a->type & DRM_MODE_TYPE_PREFERRED) != 0);
	if (diff)
		return diff;
	diff = b->hdisplay * b->vdisplay - a->hdisplay * a->vdisplay;
	if (diff)
		return diff;

	diff = drm_mode_vrefresh(b) - drm_mode_vrefresh(a);
	if (diff)
		return diff;

	diff = b->clock - a->clock;
	return diff;
}

 
void drm_mode_sort(struct list_head *mode_list)
{
	list_sort(NULL, mode_list, drm_mode_compare);
}
EXPORT_SYMBOL(drm_mode_sort);

 
void drm_connector_list_update(struct drm_connector *connector)
{
	struct drm_display_mode *pmode, *pt;

	WARN_ON(!mutex_is_locked(&connector->dev->mode_config.mutex));

	list_for_each_entry_safe(pmode, pt, &connector->probed_modes, head) {
		struct drm_display_mode *mode;
		bool found_it = false;

		 
		list_for_each_entry(mode, &connector->modes, head) {
			if (!drm_mode_equal(pmode, mode))
				continue;

			found_it = true;

			 
			if (mode->status == MODE_STALE) {
				drm_mode_copy(mode, pmode);
			} else if ((mode->type & DRM_MODE_TYPE_PREFERRED) == 0 &&
				   (pmode->type & DRM_MODE_TYPE_PREFERRED) != 0) {
				pmode->type |= mode->type;
				drm_mode_copy(mode, pmode);
			} else {
				mode->type |= pmode->type;
			}

			list_del(&pmode->head);
			drm_mode_destroy(connector->dev, pmode);
			break;
		}

		if (!found_it) {
			list_move_tail(&pmode->head, &connector->modes);
		}
	}
}
EXPORT_SYMBOL(drm_connector_list_update);

static int drm_mode_parse_cmdline_bpp(const char *str, char **end_ptr,
				      struct drm_cmdline_mode *mode)
{
	unsigned int bpp;

	if (str[0] != '-')
		return -EINVAL;

	str++;
	bpp = simple_strtol(str, end_ptr, 10);
	if (*end_ptr == str)
		return -EINVAL;

	mode->bpp = bpp;
	mode->bpp_specified = true;

	return 0;
}

static int drm_mode_parse_cmdline_refresh(const char *str, char **end_ptr,
					  struct drm_cmdline_mode *mode)
{
	unsigned int refresh;

	if (str[0] != '@')
		return -EINVAL;

	str++;
	refresh = simple_strtol(str, end_ptr, 10);
	if (*end_ptr == str)
		return -EINVAL;

	mode->refresh = refresh;
	mode->refresh_specified = true;

	return 0;
}

static int drm_mode_parse_cmdline_extra(const char *str, int length,
					bool freestanding,
					const struct drm_connector *connector,
					struct drm_cmdline_mode *mode)
{
	int i;

	for (i = 0; i < length; i++) {
		switch (str[i]) {
		case 'i':
			if (freestanding)
				return -EINVAL;

			mode->interlace = true;
			break;
		case 'm':
			if (freestanding)
				return -EINVAL;

			mode->margins = true;
			break;
		case 'D':
			if (mode->force != DRM_FORCE_UNSPECIFIED)
				return -EINVAL;

			if ((connector->connector_type != DRM_MODE_CONNECTOR_DVII) &&
			    (connector->connector_type != DRM_MODE_CONNECTOR_HDMIB))
				mode->force = DRM_FORCE_ON;
			else
				mode->force = DRM_FORCE_ON_DIGITAL;
			break;
		case 'd':
			if (mode->force != DRM_FORCE_UNSPECIFIED)
				return -EINVAL;

			mode->force = DRM_FORCE_OFF;
			break;
		case 'e':
			if (mode->force != DRM_FORCE_UNSPECIFIED)
				return -EINVAL;

			mode->force = DRM_FORCE_ON;
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

static int drm_mode_parse_cmdline_res_mode(const char *str, unsigned int length,
					   bool extras,
					   const struct drm_connector *connector,
					   struct drm_cmdline_mode *mode)
{
	const char *str_start = str;
	bool rb = false, cvt = false;
	int xres = 0, yres = 0;
	int remaining, i;
	char *end_ptr;

	xres = simple_strtol(str, &end_ptr, 10);
	if (end_ptr == str)
		return -EINVAL;

	if (end_ptr[0] != 'x')
		return -EINVAL;
	end_ptr++;

	str = end_ptr;
	yres = simple_strtol(str, &end_ptr, 10);
	if (end_ptr == str)
		return -EINVAL;

	remaining = length - (end_ptr - str_start);
	if (remaining < 0)
		return -EINVAL;

	for (i = 0; i < remaining; i++) {
		switch (end_ptr[i]) {
		case 'M':
			cvt = true;
			break;
		case 'R':
			rb = true;
			break;
		default:
			 
			if (extras) {
				int ret = drm_mode_parse_cmdline_extra(end_ptr + i,
								       1,
								       false,
								       connector,
								       mode);
				if (ret)
					return ret;
			} else {
				return -EINVAL;
			}
		}
	}

	mode->xres = xres;
	mode->yres = yres;
	mode->cvt = cvt;
	mode->rb = rb;

	return 0;
}

static int drm_mode_parse_cmdline_int(const char *delim, unsigned int *int_ret)
{
	const char *value;
	char *endp;

	 
	if (*delim != '=')
		return -EINVAL;

	value = delim + 1;
	*int_ret = simple_strtol(value, &endp, 10);

	 
	if (endp == value)
		return -EINVAL;

	return 0;
}

static int drm_mode_parse_panel_orientation(const char *delim,
					    struct drm_cmdline_mode *mode)
{
	const char *value;

	if (*delim != '=')
		return -EINVAL;

	value = delim + 1;
	delim = strchr(value, ',');
	if (!delim)
		delim = value + strlen(value);

	if (!strncmp(value, "normal", delim - value))
		mode->panel_orientation = DRM_MODE_PANEL_ORIENTATION_NORMAL;
	else if (!strncmp(value, "upside_down", delim - value))
		mode->panel_orientation = DRM_MODE_PANEL_ORIENTATION_BOTTOM_UP;
	else if (!strncmp(value, "left_side_up", delim - value))
		mode->panel_orientation = DRM_MODE_PANEL_ORIENTATION_LEFT_UP;
	else if (!strncmp(value, "right_side_up", delim - value))
		mode->panel_orientation = DRM_MODE_PANEL_ORIENTATION_RIGHT_UP;
	else
		return -EINVAL;

	return 0;
}

static int drm_mode_parse_tv_mode(const char *delim,
				  struct drm_cmdline_mode *mode)
{
	const char *value;
	int ret;

	if (*delim != '=')
		return -EINVAL;

	value = delim + 1;
	delim = strchr(value, ',');
	if (!delim)
		delim = value + strlen(value);

	ret = drm_get_tv_mode_from_name(value, delim - value);
	if (ret < 0)
		return ret;

	mode->tv_mode_specified = true;
	mode->tv_mode = ret;

	return 0;
}

static int drm_mode_parse_cmdline_options(const char *str,
					  bool freestanding,
					  const struct drm_connector *connector,
					  struct drm_cmdline_mode *mode)
{
	unsigned int deg, margin, rotation = 0;
	const char *delim, *option, *sep;

	option = str;
	do {
		delim = strchr(option, '=');
		if (!delim) {
			delim = strchr(option, ',');

			if (!delim)
				delim = option + strlen(option);
		}

		if (!strncmp(option, "rotate", delim - option)) {
			if (drm_mode_parse_cmdline_int(delim, &deg))
				return -EINVAL;

			switch (deg) {
			case 0:
				rotation |= DRM_MODE_ROTATE_0;
				break;

			case 90:
				rotation |= DRM_MODE_ROTATE_90;
				break;

			case 180:
				rotation |= DRM_MODE_ROTATE_180;
				break;

			case 270:
				rotation |= DRM_MODE_ROTATE_270;
				break;

			default:
				return -EINVAL;
			}
		} else if (!strncmp(option, "reflect_x", delim - option)) {
			rotation |= DRM_MODE_REFLECT_X;
		} else if (!strncmp(option, "reflect_y", delim - option)) {
			rotation |= DRM_MODE_REFLECT_Y;
		} else if (!strncmp(option, "margin_right", delim - option)) {
			if (drm_mode_parse_cmdline_int(delim, &margin))
				return -EINVAL;

			mode->tv_margins.right = margin;
		} else if (!strncmp(option, "margin_left", delim - option)) {
			if (drm_mode_parse_cmdline_int(delim, &margin))
				return -EINVAL;

			mode->tv_margins.left = margin;
		} else if (!strncmp(option, "margin_top", delim - option)) {
			if (drm_mode_parse_cmdline_int(delim, &margin))
				return -EINVAL;

			mode->tv_margins.top = margin;
		} else if (!strncmp(option, "margin_bottom", delim - option)) {
			if (drm_mode_parse_cmdline_int(delim, &margin))
				return -EINVAL;

			mode->tv_margins.bottom = margin;
		} else if (!strncmp(option, "panel_orientation", delim - option)) {
			if (drm_mode_parse_panel_orientation(delim, mode))
				return -EINVAL;
		} else if (!strncmp(option, "tv_mode", delim - option)) {
			if (drm_mode_parse_tv_mode(delim, mode))
				return -EINVAL;
		} else {
			return -EINVAL;
		}
		sep = strchr(delim, ',');
		option = sep + 1;
	} while (sep);

	if (rotation && freestanding)
		return -EINVAL;

	if (!(rotation & DRM_MODE_ROTATE_MASK))
		rotation |= DRM_MODE_ROTATE_0;

	 
	if (!is_power_of_2(rotation & DRM_MODE_ROTATE_MASK))
		return -EINVAL;

	mode->rotation_reflection = rotation;

	return 0;
}

struct drm_named_mode {
	const char *name;
	unsigned int pixel_clock_khz;
	unsigned int xres;
	unsigned int yres;
	unsigned int flags;
	unsigned int tv_mode;
};

#define NAMED_MODE(_name, _pclk, _x, _y, _flags, _mode)	\
	{						\
		.name = _name,				\
		.pixel_clock_khz = _pclk,		\
		.xres = _x,				\
		.yres = _y,				\
		.flags = _flags,			\
		.tv_mode = _mode,			\
	}

static const struct drm_named_mode drm_named_modes[] = {
	NAMED_MODE("NTSC", 13500, 720, 480, DRM_MODE_FLAG_INTERLACE, DRM_MODE_TV_MODE_NTSC),
	NAMED_MODE("NTSC-J", 13500, 720, 480, DRM_MODE_FLAG_INTERLACE, DRM_MODE_TV_MODE_NTSC_J),
	NAMED_MODE("PAL", 13500, 720, 576, DRM_MODE_FLAG_INTERLACE, DRM_MODE_TV_MODE_PAL),
	NAMED_MODE("PAL-M", 13500, 720, 480, DRM_MODE_FLAG_INTERLACE, DRM_MODE_TV_MODE_PAL_M),
};

static int drm_mode_parse_cmdline_named_mode(const char *name,
					     unsigned int name_end,
					     struct drm_cmdline_mode *cmdline_mode)
{
	unsigned int i;

	if (!name_end)
		return 0;

	 
	if (isdigit(name[0]))
		return 0;

	 
	if (strnchr(name, name_end, '='))
		return 0;

	 
	if (name_end == 1 &&
	    (name[0] == 'd' || name[0] == 'D' || name[0] == 'e'))
		return 0;

	 
	for (i = 0; i < ARRAY_SIZE(drm_named_modes); i++) {
		const struct drm_named_mode *mode = &drm_named_modes[i];
		int ret;

		ret = str_has_prefix(name, mode->name);
		if (ret != name_end)
			continue;

		strscpy(cmdline_mode->name, mode->name, sizeof(cmdline_mode->name));
		cmdline_mode->pixel_clock = mode->pixel_clock_khz;
		cmdline_mode->xres = mode->xres;
		cmdline_mode->yres = mode->yres;
		cmdline_mode->interlace = !!(mode->flags & DRM_MODE_FLAG_INTERLACE);
		cmdline_mode->tv_mode = mode->tv_mode;
		cmdline_mode->tv_mode_specified = true;
		cmdline_mode->specified = true;

		return 1;
	}

	return -EINVAL;
}

 
bool drm_mode_parse_command_line_for_connector(const char *mode_option,
					       const struct drm_connector *connector,
					       struct drm_cmdline_mode *mode)
{
	const char *name;
	bool freestanding = false, parse_extras = false;
	unsigned int bpp_off = 0, refresh_off = 0, options_off = 0;
	unsigned int mode_end = 0;
	const char *bpp_ptr = NULL, *refresh_ptr = NULL, *extra_ptr = NULL;
	const char *options_ptr = NULL;
	char *bpp_end_ptr = NULL, *refresh_end_ptr = NULL;
	int len, ret;

	memset(mode, 0, sizeof(*mode));
	mode->panel_orientation = DRM_MODE_PANEL_ORIENTATION_UNKNOWN;

	if (!mode_option)
		return false;

	name = mode_option;

	 
	options_ptr = strchr(name, ',');
	if (options_ptr)
		options_off = options_ptr - name;
	else
		options_off = strlen(name);

	 
	bpp_ptr = strnchr(name, options_off, '-');
	while (bpp_ptr && !isdigit(bpp_ptr[1]))
		bpp_ptr = strnchr(bpp_ptr + 1, options_off, '-');
	if (bpp_ptr)
		bpp_off = bpp_ptr - name;

	refresh_ptr = strnchr(name, options_off, '@');
	if (refresh_ptr)
		refresh_off = refresh_ptr - name;

	 
	if (bpp_ptr) {
		mode_end = bpp_off;
	} else if (refresh_ptr) {
		mode_end = refresh_off;
	} else if (options_ptr) {
		mode_end = options_off;
		parse_extras = true;
	} else {
		mode_end = strlen(name);
		parse_extras = true;
	}

	if (!mode_end)
		return false;

	ret = drm_mode_parse_cmdline_named_mode(name, mode_end, mode);
	if (ret < 0)
		return false;

	 
	if (ret && refresh_ptr)
		return false;

	 
	if (!mode->specified && isdigit(name[0])) {
		ret = drm_mode_parse_cmdline_res_mode(name, mode_end,
						      parse_extras,
						      connector,
						      mode);
		if (ret)
			return false;

		mode->specified = true;
	}

	 
	if (!mode->specified) {
		unsigned int len = strlen(mode_option);

		if (bpp_ptr || refresh_ptr)
			return false;  

		if (len == 1 || (len >= 2 && mode_option[1] == ','))
			extra_ptr = mode_option;
		else
			options_ptr = mode_option - 1;

		freestanding = true;
	}

	if (bpp_ptr) {
		ret = drm_mode_parse_cmdline_bpp(bpp_ptr, &bpp_end_ptr, mode);
		if (ret)
			return false;

		mode->bpp_specified = true;
	}

	if (refresh_ptr) {
		ret = drm_mode_parse_cmdline_refresh(refresh_ptr,
						     &refresh_end_ptr, mode);
		if (ret)
			return false;

		mode->refresh_specified = true;
	}

	 
	if (bpp_ptr && refresh_ptr)
		extra_ptr = max(bpp_end_ptr, refresh_end_ptr);
	else if (bpp_ptr)
		extra_ptr = bpp_end_ptr;
	else if (refresh_ptr)
		extra_ptr = refresh_end_ptr;

	if (extra_ptr) {
		if (options_ptr)
			len = options_ptr - extra_ptr;
		else
			len = strlen(extra_ptr);

		ret = drm_mode_parse_cmdline_extra(extra_ptr, len, freestanding,
						   connector, mode);
		if (ret)
			return false;
	}

	if (options_ptr) {
		ret = drm_mode_parse_cmdline_options(options_ptr + 1,
						     freestanding,
						     connector, mode);
		if (ret)
			return false;
	}

	return true;
}
EXPORT_SYMBOL(drm_mode_parse_command_line_for_connector);

static struct drm_display_mode *drm_named_mode(struct drm_device *dev,
					       struct drm_cmdline_mode *cmd)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(drm_named_modes); i++) {
		const struct drm_named_mode *named_mode = &drm_named_modes[i];

		if (strcmp(cmd->name, named_mode->name))
			continue;

		if (!cmd->tv_mode_specified)
			continue;

		return drm_analog_tv_mode(dev,
					  named_mode->tv_mode,
					  named_mode->pixel_clock_khz * 1000,
					  named_mode->xres,
					  named_mode->yres,
					  named_mode->flags & DRM_MODE_FLAG_INTERLACE);
	}

	return NULL;
}

 
struct drm_display_mode *
drm_mode_create_from_cmdline_mode(struct drm_device *dev,
				  struct drm_cmdline_mode *cmd)
{
	struct drm_display_mode *mode;

	if (cmd->xres == 0 || cmd->yres == 0)
		return NULL;

	if (strlen(cmd->name))
		mode = drm_named_mode(dev, cmd);
	else if (cmd->cvt)
		mode = drm_cvt_mode(dev,
				    cmd->xres, cmd->yres,
				    cmd->refresh_specified ? cmd->refresh : 60,
				    cmd->rb, cmd->interlace,
				    cmd->margins);
	else
		mode = drm_gtf_mode(dev,
				    cmd->xres, cmd->yres,
				    cmd->refresh_specified ? cmd->refresh : 60,
				    cmd->interlace,
				    cmd->margins);
	if (!mode)
		return NULL;

	mode->type |= DRM_MODE_TYPE_USERDEF;
	 
	if (cmd->xres == 1366)
		drm_mode_fixup_1366x768(mode);
	drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
	return mode;
}
EXPORT_SYMBOL(drm_mode_create_from_cmdline_mode);

 
void drm_mode_convert_to_umode(struct drm_mode_modeinfo *out,
			       const struct drm_display_mode *in)
{
	out->clock = in->clock;
	out->hdisplay = in->hdisplay;
	out->hsync_start = in->hsync_start;
	out->hsync_end = in->hsync_end;
	out->htotal = in->htotal;
	out->hskew = in->hskew;
	out->vdisplay = in->vdisplay;
	out->vsync_start = in->vsync_start;
	out->vsync_end = in->vsync_end;
	out->vtotal = in->vtotal;
	out->vscan = in->vscan;
	out->vrefresh = drm_mode_vrefresh(in);
	out->flags = in->flags;
	out->type = in->type;

	switch (in->picture_aspect_ratio) {
	case HDMI_PICTURE_ASPECT_4_3:
		out->flags |= DRM_MODE_FLAG_PIC_AR_4_3;
		break;
	case HDMI_PICTURE_ASPECT_16_9:
		out->flags |= DRM_MODE_FLAG_PIC_AR_16_9;
		break;
	case HDMI_PICTURE_ASPECT_64_27:
		out->flags |= DRM_MODE_FLAG_PIC_AR_64_27;
		break;
	case HDMI_PICTURE_ASPECT_256_135:
		out->flags |= DRM_MODE_FLAG_PIC_AR_256_135;
		break;
	default:
		WARN(1, "Invalid aspect ratio (0%x) on mode\n",
		     in->picture_aspect_ratio);
		fallthrough;
	case HDMI_PICTURE_ASPECT_NONE:
		out->flags |= DRM_MODE_FLAG_PIC_AR_NONE;
		break;
	}

	strncpy(out->name, in->name, DRM_DISPLAY_MODE_LEN);
	out->name[DRM_DISPLAY_MODE_LEN-1] = 0;
}

 
int drm_mode_convert_umode(struct drm_device *dev,
			   struct drm_display_mode *out,
			   const struct drm_mode_modeinfo *in)
{
	if (in->clock > INT_MAX || in->vrefresh > INT_MAX)
		return -ERANGE;

	out->clock = in->clock;
	out->hdisplay = in->hdisplay;
	out->hsync_start = in->hsync_start;
	out->hsync_end = in->hsync_end;
	out->htotal = in->htotal;
	out->hskew = in->hskew;
	out->vdisplay = in->vdisplay;
	out->vsync_start = in->vsync_start;
	out->vsync_end = in->vsync_end;
	out->vtotal = in->vtotal;
	out->vscan = in->vscan;
	out->flags = in->flags;
	 
	out->type = in->type & DRM_MODE_TYPE_ALL;
	strncpy(out->name, in->name, DRM_DISPLAY_MODE_LEN);
	out->name[DRM_DISPLAY_MODE_LEN-1] = 0;

	 
	out->flags &= ~DRM_MODE_FLAG_PIC_AR_MASK;

	switch (in->flags & DRM_MODE_FLAG_PIC_AR_MASK) {
	case DRM_MODE_FLAG_PIC_AR_4_3:
		out->picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3;
		break;
	case DRM_MODE_FLAG_PIC_AR_16_9:
		out->picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9;
		break;
	case DRM_MODE_FLAG_PIC_AR_64_27:
		out->picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27;
		break;
	case DRM_MODE_FLAG_PIC_AR_256_135:
		out->picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135;
		break;
	case DRM_MODE_FLAG_PIC_AR_NONE:
		out->picture_aspect_ratio = HDMI_PICTURE_ASPECT_NONE;
		break;
	default:
		return -EINVAL;
	}

	out->status = drm_mode_validate_driver(dev, out);
	if (out->status != MODE_OK)
		return -EINVAL;

	drm_mode_set_crtcinfo(out, CRTC_INTERLACE_HALVE_V);

	return 0;
}

 
bool drm_mode_is_420_only(const struct drm_display_info *display,
			  const struct drm_display_mode *mode)
{
	u8 vic = drm_match_cea_mode(mode);

	return test_bit(vic, display->hdmi.y420_vdb_modes);
}
EXPORT_SYMBOL(drm_mode_is_420_only);

 
bool drm_mode_is_420_also(const struct drm_display_info *display,
			  const struct drm_display_mode *mode)
{
	u8 vic = drm_match_cea_mode(mode);

	return test_bit(vic, display->hdmi.y420_cmdb_modes);
}
EXPORT_SYMBOL(drm_mode_is_420_also);
 
bool drm_mode_is_420(const struct drm_display_info *display,
		     const struct drm_display_mode *mode)
{
	return drm_mode_is_420_only(display, mode) ||
		drm_mode_is_420_also(display, mode);
}
EXPORT_SYMBOL(drm_mode_is_420);
