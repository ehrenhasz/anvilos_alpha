#ifndef __DSCC_TYPES_H__
#define __DSCC_TYPES_H__
#include <drm/display/drm_dsc.h>
#ifndef NUM_BUF_RANGES
#define NUM_BUF_RANGES 15
#endif
struct dsc_pps_rc_range {
	int range_min_qp;
	int range_max_qp;
	int range_bpg_offset;
};
struct dsc_parameters {
	struct drm_dsc_config pps;
	uint32_t bytes_per_pixel;  
	uint32_t rc_buffer_model_size;
};
struct rc_params;
int dscc_compute_dsc_parameters(const struct drm_dsc_config *pps,
		const struct rc_params *rc,
		struct dsc_parameters *dsc_params);
#endif
