 
 

#ifndef MSM_DSC_HELPER_H_
#define MSM_DSC_HELPER_H_

#include <linux/math.h>
#include <drm/display/drm_dsc_helper.h>

 
static inline u32 msm_dsc_get_slices_per_intf(const struct drm_dsc_config *dsc, u32 intf_width)
{
	return DIV_ROUND_UP(intf_width, dsc->slice_width);
}

 
static inline u32 msm_dsc_get_bytes_per_line(const struct drm_dsc_config *dsc)
{
	return dsc->slice_count * dsc->slice_chunk_size;
}

#endif  
