 
 

#ifndef _DPU_FORMATS_H
#define _DPU_FORMATS_H

#include <drm/drm_fourcc.h>
#include "msm_gem.h"
#include "dpu_hw_mdss.h"

 
const struct dpu_format *dpu_get_dpu_format_ext(
		const uint32_t format,
		const uint64_t modifier);

#define dpu_get_dpu_format(f) dpu_get_dpu_format_ext(f, 0)

 
static inline bool dpu_find_format(u32 format, const u32 *supported_formats,
					size_t num_formats)
{
	int i;

	for (i = 0; i < num_formats; i++) {
		 
		if (format == supported_formats[i])
			return true;
	}

	return false;
}

 
const struct msm_format *dpu_get_msm_format(
		struct msm_kms *kms,
		const uint32_t format,
		const uint64_t modifiers);

 
int dpu_format_check_modified_format(
		const struct msm_kms *kms,
		const struct msm_format *msm_fmt,
		const struct drm_mode_fb_cmd2 *cmd,
		struct drm_gem_object **bos);

 
int dpu_format_populate_layout(
		struct msm_gem_address_space *aspace,
		struct drm_framebuffer *fb,
		struct dpu_hw_fmt_layout *fmtl);

#endif  
